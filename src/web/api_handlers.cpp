#include "web_server.h"
#include "storage_engine.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>

namespace work_assistant {
namespace api_handlers {

// Helper function to create error response
ApiResponse CreateErrorResponse(const std::string& message, int status_code = 400) {
    ApiResponse response;
    response.success = false;
    response.message = message;
    response.status_code = status_code;
    return response;
}

// Helper function to create success response
ApiResponse CreateSuccessResponse(const std::string& data, const std::string& message = "Success") {
    ApiResponse response;
    response.success = true;
    response.message = message;
    response.data = data;
    response.status_code = 200;
    return response;
}

ApiResponse GetProductivitySummary(const std::chrono::system_clock::time_point& start,
                                 const std::chrono::system_clock::time_point& end,
                                 EncryptedStorageManager* storage) {
    if (!storage || !storage->IsReady()) {
        return CreateErrorResponse("Storage not available", 503);
    }
    
    if (!web_utils::ValidateTimeRange(start, end)) {
        return CreateErrorResponse("Invalid time range", 400);
    }
    
    try {
        // Get productivity report from storage
        auto report = storage->GetProductivityReport(start, end);
        
        if (report.empty()) {
            std::ostringstream json;
            json << "{";
            json << "\"total_activities\": 0,";
            json << "\"productive_ratio\": 0.0,";
            json << "\"focused_ratio\": 0.0,";
            json << "\"avg_confidence\": 0.0,";
            json << "\"message\": \"No data available for the specified time range\"";
            json << "}";
            
            return CreateSuccessResponse(json.str(), "No activities found");
        }
        
        // Build JSON response
        std::ostringstream json;
        json << "{";
        json << "\"period\": {";
        json << "  \"start\": \"" << storage_utils::FormatTimestamp(start) << "\",";
        json << "  \"end\": \"" << storage_utils::FormatTimestamp(end) << "\"";
        json << "},";
        json << "\"summary\": {";
        json << "  \"total_activities\": " << static_cast<int>(report.at("total_activities")) << ",";
        json << "  \"productive_ratio\": " << std::fixed << std::setprecision(2) << report.at("productive_ratio") << ",";
        json << "  \"focused_ratio\": " << std::fixed << std::setprecision(2) << report.at("focused_ratio") << ",";
        json << "  \"avg_confidence\": " << std::fixed << std::setprecision(2) << report.at("avg_confidence") << ",";
        json << "  \"avg_distraction\": " << std::fixed << std::setprecision(1) << report.at("avg_distraction");
        
        if (report.find("dominant_content_type") != report.end()) {
            int type_val = static_cast<int>(report.at("dominant_content_type"));
            ContentType dominant_type = static_cast<ContentType>(type_val);
            json << ",";
            json << "  \"dominant_content_type\": " << static_cast<int>(dominant_type) << "";
        }
        
        json << "},";
        
        // Calculate productivity score
        float productivity_score = report.at("productive_ratio") * 100.0f;
        std::string productivity_level;
        if (productivity_score >= 80) productivity_level = "Excellent";
        else if (productivity_score >= 60) productivity_level = "Good";
        else if (productivity_score >= 40) productivity_level = "Fair";
        else productivity_level = "Poor";
        
        json << "\"productivity\": {";
        json << "  \"score\": " << static_cast<int>(productivity_score) << ",";
        json << "  \"level\": \"" << productivity_level << "\",";
        json << "  \"recommendation\": \"";
        
        if (productivity_score < 60) {
            json << "Consider taking more focused work sessions and reducing distractions";
        } else if (productivity_score < 80) {
            json << "Good productivity! Try to maintain focused work sessions";
        } else {
            json << "Excellent productivity! Keep up the great work";
        }
        
        json << "\"";
        json << "},";
        
        // Get application usage
        auto app_usage = storage->GetTimeSpentByApplication(start, end);
        json << "\"applications\": [";
        for (size_t i = 0; i < std::min(app_usage.size(), size_t(10)); ++i) {
            if (i > 0) json << ",";
            json << "{";
            json << "\"name\": \"" << web_utils::EscapeJsonString(app_usage[i].first) << "\",";
            json << "\"time_minutes\": " << app_usage[i].second.count();
            json << "}";
        }
        json << "]";
        json << "}";
        
        return CreateSuccessResponse(json.str(), "Productivity summary generated");
        
    } catch (const std::exception& e) {
        return CreateErrorResponse("Failed to generate productivity summary: " + std::string(e.what()), 500);
    }
}

ApiResponse GetActivityTimeline(const std::chrono::system_clock::time_point& start,
                              const std::chrono::system_clock::time_point& end,
                              EncryptedStorageManager* storage) {
    if (!storage || !storage->IsReady()) {
        return CreateErrorResponse("Storage not available", 503);
    }
    
    if (!web_utils::ValidateTimeRange(start, end)) {
        return CreateErrorResponse("Invalid time range", 400);
    }
    
    try {
        auto activities = storage->GetContentAnalyses(start, end);
        
        std::ostringstream json;
        json << "{";
        json << "\"period\": {";
        json << "  \"start\": \"" << storage_utils::FormatTimestamp(start) << "\",";
        json << "  \"end\": \"" << storage_utils::FormatTimestamp(end) << "\"";
        json << "},";
        json << "\"total_activities\": " << activities.size() << ",";
        json << "\"activities\": [";
        
        for (size_t i = 0; i < activities.size(); ++i) {
            if (i > 0) json << ",";
            
            const auto& activity = activities[i];
            json << "{";
            json << "\"id\": " << activity.id << ",";
            json << "\"timestamp\": \"" << storage_utils::FormatTimestamp(activity.timestamp) << "\",";
            json << "\"application\": \"" << web_utils::EscapeJsonString(activity.application_name) << "\",";
            json << "\"window_title\": \"" << web_utils::EscapeJsonString(activity.window_title) << "\",";
            json << "\"content_type\": " << static_cast<int>(activity.content_type) << ",";
            json << "\"work_category\": " << static_cast<int>(activity.work_category) << ",";
            json << "\"is_productive\": " << (activity.is_productive ? "true" : "false") << ",";
            json << "\"is_focused\": " << (activity.is_focused_work ? "true" : "false") << ",";
            json << "\"confidence\": " << std::fixed << std::setprecision(2) << activity.ai_confidence << ",";
            json << "\"distraction_level\": " << activity.distraction_level << ",";
            json << "\"priority\": " << static_cast<int>(activity.priority);
            
            if (!activity.keywords.empty()) {
                json << ",\"keywords\": [";
                for (size_t j = 0; j < std::min(activity.keywords.size(), size_t(5)); ++j) {
                    if (j > 0) json << ",";
                    json << "\"" << web_utils::EscapeJsonString(activity.keywords[j]) << "\"";
                }
                json << "]";
            }
            
            json << "}";
        }
        
        json << "]";
        json << "}";
        
        return CreateSuccessResponse(json.str(), "Activity timeline generated");
        
    } catch (const std::exception& e) {
        return CreateErrorResponse("Failed to generate timeline: " + std::string(e.what()), 500);
    }
}

ApiResponse GetApplicationUsage(const std::chrono::system_clock::time_point& start,
                              const std::chrono::system_clock::time_point& end,
                              EncryptedStorageManager* storage) {
    if (!storage || !storage->IsReady()) {
        return CreateErrorResponse("Storage not available", 503);
    }
    
    if (!web_utils::ValidateTimeRange(start, end)) {
        return CreateErrorResponse("Invalid time range", 400);
    }
    
    try {
        auto app_usage = storage->GetTimeSpentByApplication(start, end);
        
        std::ostringstream json;
        json << "{";
        json << "\"period\": {";
        json << "  \"start\": \"" << storage_utils::FormatTimestamp(start) << "\",";
        json << "  \"end\": \"" << storage_utils::FormatTimestamp(end) << "\"";
        json << "},";
        json << "\"total_applications\": " << app_usage.size() << ",";
        json << "\"applications\": [";
        
        for (size_t i = 0; i < app_usage.size(); ++i) {
            if (i > 0) json << ",";
            
            json << "{";
            json << "\"name\": \"" << web_utils::EscapeJsonString(app_usage[i].first) << "\",";
            json << "\"time_minutes\": " << app_usage[i].second.count() << ",";
            json << "\"time_formatted\": \"";
            
            auto minutes = app_usage[i].second.count();
            if (minutes >= 60) {
                json << (minutes / 60) << "h " << (minutes % 60) << "m";
            } else {
                json << minutes << "m";
            }
            
            json << "\",";
            
            // Calculate percentage of total time
            auto total_minutes = std::chrono::duration_cast<std::chrono::minutes>(end - start).count();
            float percentage = total_minutes > 0 ? (static_cast<float>(minutes) / total_minutes) * 100.0f : 0.0f;
            json << "\"percentage\": " << std::fixed << std::setprecision(1) << percentage;
            json << "}";
        }
        
        json << "]";
        json << "}";
        
        return CreateSuccessResponse(json.str(), "Application usage report generated");
        
    } catch (const std::exception& e) {
        return CreateErrorResponse("Failed to generate usage report: " + std::string(e.what()), 500);
    }
}

ApiResponse SearchContent(const std::string& query, int max_results,
                        EncryptedStorageManager* storage) {
    if (!storage || !storage->IsReady()) {
        return CreateErrorResponse("Storage not available", 503);
    }
    
    if (query.empty()) {
        return CreateErrorResponse("Search query cannot be empty", 400);
    }
    
    if (max_results <= 0 || max_results > 1000) {
        max_results = 50; // Default limit
    }
    
    try {
        auto results = storage->SearchContent(query, max_results);
        
        std::ostringstream json;
        json << "{";
        json << "\"query\": \"" << web_utils::EscapeJsonString(query) << "\",";
        json << "\"total_results\": " << results.size() << ",";
        json << "\"max_results\": " << max_results << ",";
        json << "\"results\": [";
        
        for (size_t i = 0; i < results.size(); ++i) {
            if (i > 0) json << ",";
            
            const auto& result = results[i];
            json << "{";
            json << "\"id\": " << result.id << ",";
            json << "\"timestamp\": \"" << storage_utils::FormatTimestamp(result.timestamp) << "\",";
            json << "\"application\": \"" << web_utils::EscapeJsonString(result.application_name) << "\",";
            json << "\"window_title\": \"" << web_utils::EscapeJsonString(result.window_title) << "\",";
            json << "\"content_type\": " << static_cast<int>(result.content_type) << ",";
            
            // Truncate extracted text for search results
            std::string text = result.extracted_text;
            if (text.length() > 200) {
                text = text.substr(0, 200) + "...";
            }
            json << "\"text_preview\": \"" << web_utils::EscapeJsonString(text) << "\",";
            json << "\"confidence\": " << std::fixed << std::setprecision(2) << result.ai_confidence << ",";
            json << "\"is_productive\": " << (result.is_productive ? "true" : "false");
            json << "}";
        }
        
        json << "]";
        json << "}";
        
        return CreateSuccessResponse(json.str(), "Search completed");
        
    } catch (const std::exception& e) {
        return CreateErrorResponse("Search failed: " + std::string(e.what()), 500);
    }
}

ApiResponse ExportData(const std::chrono::system_clock::time_point& start,
                     const std::chrono::system_clock::time_point& end,
                     const std::string& format,
                     EncryptedStorageManager* storage) {
    if (!storage || !storage->IsReady()) {
        return CreateErrorResponse("Storage not available", 503);
    }
    
    if (!web_utils::ValidateTimeRange(start, end)) {
        return CreateErrorResponse("Invalid time range", 400);
    }
    
    if (format != "json" && format != "csv") {
        return CreateErrorResponse("Unsupported export format. Use 'json' or 'csv'", 400);
    }
    
    try {
        // Generate unique filename
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream filename;
        filename << "work_assistant_export_" << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S") << "." << format;
        
        std::string export_path = "exports/" + filename.str();
        
        // Export data
        bool success = storage->ExportData(export_path, start, end);
        
        if (!success) {
            return CreateErrorResponse("Export failed", 500);
        }
        
        std::ostringstream json;
        json << "{";
        json << "\"export_path\": \"" << export_path << "\",";
        json << "\"filename\": \"" << filename.str() << "\",";
        json << "\"format\": \"" << format << "\",";
        json << "\"period\": {";
        json << "  \"start\": \"" << storage_utils::FormatTimestamp(start) << "\",";
        json << "  \"end\": \"" << storage_utils::FormatTimestamp(end) << "\"";
        json << "},";
        json << "\"generated_at\": \"" << storage_utils::FormatTimestamp(now) << "\"";
        json << "}";
        
        return CreateSuccessResponse(json.str(), "Data exported successfully");
        
    } catch (const std::exception& e) {
        return CreateErrorResponse("Export failed: " + std::string(e.what()), 500);
    }
}

ApiResponse GetSystemStatus() {
    try {
        std::ostringstream json;
        json << "{";
        json << "\"status\": \"running\",";
        json << "\"version\": \"1.0.0\",";
        json << "\"uptime\": \"" << "calculated_uptime" << "\",";
        json << "\"components\": {";
        json << "  \"window_monitor\": \"active\",";
        json << "  \"screen_capture\": \"active\",";
        json << "  \"ocr_engine\": \"active\",";
        json << "  \"ai_analyzer\": \"active\",";
        json << "  \"storage\": \"active\",";
        json << "  \"web_server\": \"active\"";
        json << "},";
        json << "\"system_info\": {";
        json << "  \"platform\": \"";
        
#ifdef _WIN32
        json << "Windows";
#elif __linux__
        json << "Linux";
#elif __APPLE__
        json << "macOS";
#else
        json << "Unknown";
#endif
        
        json << "\",";
        json << "  \"build_type\": \"";
        
#ifdef NDEBUG
        json << "Release";
#else
        json << "Debug";
#endif
        
        json << "\",";
        json << "  \"features\": [";
        json << "    \"window_monitoring\",";
        json << "    \"screen_capture\",";
        json << "    \"ocr_processing\",";
        json << "    \"ai_classification\",";
        json << "    \"encrypted_storage\",";
        json << "    \"web_interface\"";
        
#ifndef WEB_DISABLED
        json << ",    \"drogon_framework\"";
#endif
        
        json << "  ]";
        json << "}";
        json << "}";
        
        return CreateSuccessResponse(json.str(), "System status retrieved");
        
    } catch (const std::exception& e) {
        return CreateErrorResponse("Failed to get system status: " + std::string(e.what()), 500);
    }
}

ApiResponse GetConfiguration() {
    try {
        std::ostringstream json;
        json << "{";
        json << "\"application\": {";
        json << "  \"name\": \"Work Study Assistant\",";
        json << "  \"version\": \"1.0.0\",";
        json << "  \"description\": \"Intelligent work and study activity monitoring system\"";
        json << "},";
        json << "\"features\": {";
        json << "  \"window_monitoring\": true,";
        json << "  \"screen_capture\": true,";
        json << "  \"ocr_processing\": true,";
        json << "  \"ai_classification\": true,";
        json << "  \"encrypted_storage\": true,";
        json << "  \"web_interface\": true,";
        json << "  \"real_time_updates\": true";
        json << "},";
        json << "\"limits\": {";
        json << "  \"max_search_results\": 1000,";
        json << "  \"max_export_days\": 365,";
        json << "  \"max_timeline_activities\": 10000";
        json << "}";
        json << "}";
        
        return CreateSuccessResponse(json.str(), "Configuration retrieved");
        
    } catch (const std::exception& e) {
        return CreateErrorResponse("Failed to get configuration: " + std::string(e.what()), 500);
    }
}

} // namespace api_handlers

// Web utility functions
namespace web_utils {

std::string GetMimeType(const std::string& file_extension) {
    static const std::unordered_map<std::string, std::string> mime_types = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".txt", "text/plain"},
        {".pdf", "application/pdf"},
        {".zip", "application/zip"}
    };
    
    auto it = mime_types.find(file_extension);
    return it != mime_types.end() ? it->second : "application/octet-stream";
}

std::string FormatFileSize(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    return oss.str();
}

std::string EscapeJsonString(const std::string& input) {
    std::ostringstream oss;
    for (char c : input) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            default:
                if (c < 0x20) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
                break;
        }
    }
    return oss.str();
}

std::chrono::system_clock::time_point ParseTimestamp(const std::string& timestamp) {
    return storage_utils::ParseTimestamp(timestamp);
}

bool ValidateTimeRange(const std::chrono::system_clock::time_point& start,
                      const std::chrono::system_clock::time_point& end) {
    if (start >= end) {
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    if (start > now) {
        return false;
    }
    
    // Check if range is not too large (max 1 year)
    auto max_range = std::chrono::hours(24 * 365);
    if ((end - start) > max_range) {
        return false;
    }
    
    return true;
}

} // namespace web_utils

} // namespace work_assistant