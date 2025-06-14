#include "storage_engine.h"
#include "ocr_engine.h"
#include "ai_engine.h"
#include <iostream>
#include <sstream>
#include <random>
#include <iomanip>
#include <algorithm>
#include <fstream>

namespace work_assistant {

// EncryptedStorageManager implementation
class EncryptedStorageManager::Impl {
public:
    Impl() 
        : m_initialized(false)
        , m_current_session_id(storage_utils::GenerateSessionId()) {
    }

    bool Initialize(const StorageConfig& config) {
        if (m_initialized) {
            return true;
        }

        if (!config.IsValid()) {
            std::cerr << "Invalid storage configuration" << std::endl;
            return false;
        }

        m_config = config;

        // Create storage engine
        m_storage = StorageEngineFactory::Create();
        if (!m_storage) {
            std::cerr << "Failed to create storage engine" << std::endl;
            return false;
        }

        // Initialize storage engine
        if (!m_storage->Initialize(config)) {
            std::cerr << "Failed to initialize storage engine" << std::endl;
            return false;
        }

        // Create or open database
        if (!m_storage->OpenDatabase(config.master_password)) {
            std::cout << "Database doesn't exist, creating new one..." << std::endl;
            if (!m_storage->CreateDatabase()) {
                std::cerr << "Failed to create database" << std::endl;
                return false;
            }
            if (!m_storage->OpenDatabase(config.master_password)) {
                std::cerr << "Failed to open newly created database" << std::endl;
                return false;
            }
        }

        m_initialized = true;
        std::cout << "Encrypted Storage Manager initialized" << std::endl;
        return true;
    }

    void Shutdown() {
        if (!m_initialized) {
            return;
        }

        EndSession();

        if (m_storage) {
            m_storage->Shutdown();
            m_storage.reset();
        }

        m_initialized = false;
        std::cout << "Encrypted Storage Manager shut down" << std::endl;
    }

    bool IsReady() const {
        return m_initialized && m_storage && m_storage->IsInitialized();
    }

    bool StartSession(const std::string& session_name) {
        if (!IsReady()) {
            return false;
        }

        if (!session_name.empty()) {
            m_current_session_id = session_name;
        } else {
            m_current_session_id = storage_utils::GenerateSessionId();
        }

        m_session_start_time = std::chrono::system_clock::now();
        std::cout << "Started storage session: " << m_current_session_id << std::endl;
        return true;
    }

    bool EndSession() {
        if (m_current_session_id.empty()) {
            return false;
        }

        auto session_duration = std::chrono::system_clock::now() - m_session_start_time;
        std::cout << "Ended storage session: " << m_current_session_id 
                  << " (duration: " << std::chrono::duration_cast<std::chrono::minutes>(session_duration).count() 
                  << " minutes)" << std::endl;

        m_current_session_id = storage_utils::GenerateSessionId();
        return true;
    }

    std::string GetCurrentSessionId() const {
        return m_current_session_id;
    }

    bool StoreWindowEvent(const WindowEvent& event, const WindowInfo& info) {
        if (!IsReady()) {
            return false;
        }

        WindowActivityRecord activity;
        activity.timestamp = std::chrono::system_clock::now();
        activity.window_title = info.title;
        activity.application_name = info.process_name;
        activity.process_id = info.process_id;
        activity.x = info.x;
        activity.y = info.y;
        activity.width = info.width;
        activity.height = info.height;
        
        // Convert event type to string
        switch (event.type) {
            case WindowEventType::WINDOW_CREATED:
                activity.event_type = "created";
                break;
            case WindowEventType::WINDOW_DESTROYED:
                activity.event_type = "destroyed";
                break;
            case WindowEventType::WINDOW_FOCUSED:
                activity.event_type = "focused";
                break;
            case WindowEventType::WINDOW_MINIMIZED:
                activity.event_type = "minimized";
                break;
            case WindowEventType::WINDOW_RESTORED:
                activity.event_type = "restored";
                break;
            default:
                activity.event_type = "unknown";
                break;
        }

        uint64_t id = m_storage->StoreWindowActivity(activity);
        return id > 0;
    }

    bool StoreScreenCapture(const CaptureFrame& frame, const std::string& context) {
        if (!IsReady()) {
            return false;
        }

        uint64_t id = m_storage->StoreScreenCapture(frame, context);
        return id > 0;
    }

    bool StoreOCRResult(const OCRDocument& document, const std::string& source) {
        if (!IsReady()) {
            return false;
        }

        DataRecord record;
        record.type = RecordType::OCR_RESULT;
        record.session_id = m_current_session_id;
        record.metadata["source"] = source;
        record.metadata["confidence"] = std::to_string(document.overall_confidence);
        record.metadata["text_blocks_count"] = std::to_string(document.text_blocks.size());
        
        // Store full text as data
        record.SetStringData(document.GetOrderedText());
        record.checksum = storage_utils::CalculateChecksum(record.data);

        uint64_t id = m_storage->StoreRecord(record);
        return id > 0;
    }

    bool StoreAIAnalysis(const ContentAnalysis& analysis) {
        if (!IsReady()) {
            return false;
        }

        ContentAnalysisRecord record;
        record.timestamp = analysis.timestamp;
        record.session_id = m_current_session_id;
        record.window_title = analysis.title;
        record.application_name = analysis.application;
        record.extracted_text = analysis.extracted_text;
        record.keywords = analysis.keywords;
        record.content_type = analysis.content_type;
        record.work_category = analysis.work_category;
        record.priority = analysis.priority;
        record.is_productive = analysis.is_productive;
        record.is_focused_work = analysis.is_focused_work;
        record.ai_confidence = analysis.classification_confidence;
        record.distraction_level = analysis.distraction_level;
        record.processing_time = analysis.processing_time;

        uint64_t id = m_storage->StoreContentAnalysis(record);
        return id > 0;
    }

    bool StoreBatch(const std::vector<WindowActivityRecord>& activities,
                   const std::vector<ContentAnalysisRecord>& analyses) {
        if (!IsReady()) {
            return false;
        }

        std::vector<DataRecord> records;
        
        // Convert activities to data records
        for (const auto& activity : activities) {
            records.push_back(activity.ToDataRecord());
        }
        
        // Convert analyses to data records
        for (const auto& analysis : analyses) {
            records.push_back(analysis.ToDataRecord());
        }

        return m_storage->StoreRecords(records);
    }

    std::vector<WindowActivityRecord> GetWindowActivities(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) {
        
        std::vector<WindowActivityRecord> activities;
        if (!IsReady()) {
            return activities;
        }

        QueryParams params;
        params.start_time = start;
        params.end_time = end;
        params.record_types = {RecordType::WINDOW_EVENT};

        auto records = m_storage->QueryRecords(params);
        for (const auto& record : records) {
            activities.push_back(WindowActivityRecord::FromDataRecord(record));
        }

        return activities;
    }

    std::vector<ContentAnalysisRecord> GetContentAnalyses(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) {
        
        if (!IsReady()) {
            return {};
        }

        return m_storage->GetProductivityData(start, end);
    }

    std::unordered_map<std::string, float> GetProductivityReport(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) {
        
        std::unordered_map<std::string, float> report;
        if (!IsReady()) {
            return report;
        }

        auto analyses = GetContentAnalyses(start, end);
        if (analyses.empty()) {
            return report;
        }

        // Calculate productivity metrics
        int total_activities = analyses.size();
        int productive_activities = 0;
        int focused_activities = 0;
        float total_confidence = 0.0f;
        int total_distraction = 0;

        std::unordered_map<ContentType, int> type_counts;
        std::unordered_map<WorkCategory, int> category_counts;

        for (const auto& analysis : analyses) {
            if (analysis.is_productive) productive_activities++;
            if (analysis.is_focused_work) focused_activities++;
            
            total_confidence += analysis.ai_confidence;
            total_distraction += analysis.distraction_level;
            
            type_counts[analysis.content_type]++;
            category_counts[analysis.work_category]++;
        }

        // Generate report
        report["total_activities"] = static_cast<float>(total_activities);
        report["productive_ratio"] = static_cast<float>(productive_activities) / total_activities;
        report["focused_ratio"] = static_cast<float>(focused_activities) / total_activities;
        report["avg_confidence"] = total_confidence / total_activities;
        report["avg_distraction"] = static_cast<float>(total_distraction) / total_activities;
        
        // Most common activity type
        auto max_type = std::max_element(type_counts.begin(), type_counts.end(),
                                       [](const auto& a, const auto& b) {
                                           return a.second < b.second;
                                       });
        if (max_type != type_counts.end()) {
            report["dominant_content_type"] = static_cast<float>(static_cast<int>(max_type->first));
        }

        return report;
    }

    std::vector<std::pair<std::string, std::chrono::minutes>> GetTimeSpentByApplication(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) {
        
        std::vector<std::pair<std::string, std::chrono::minutes>> time_spent;
        if (!IsReady()) {
            return time_spent;
        }

        auto usage = m_storage->GetApplicationUsage(start, end);
        
        for (const auto& [app, count] : usage) {
            // Rough estimation: assume each event represents 30 seconds
            auto minutes = std::chrono::minutes(count / 2);
            time_spent.emplace_back(app, minutes);
        }

        // Sort by time spent (descending)
        std::sort(time_spent.begin(), time_spent.end(),
                 [](const auto& a, const auto& b) {
                     return a.second > b.second;
                 });

        return time_spent;
    }

    std::vector<ContentAnalysisRecord> SearchContent(const std::string& query, int max_results) {
        std::vector<ContentAnalysisRecord> results;
        if (!IsReady()) {
            return results;
        }

        // Simple search implementation
        QueryParams params;
        params.record_types = {RecordType::AI_ANALYSIS};
        params.limit = max_results;
        params.search_text = query;

        auto records = m_storage->QueryRecords(params);
        for (const auto& record : records) {
            if (record.type == RecordType::AI_ANALYSIS) {
                auto analysis = ContentAnalysisRecord::FromDataRecord(record);
                
                // Check if query matches extracted text
                std::string text = analysis.extracted_text;
                std::transform(text.begin(), text.end(), text.begin(), ::tolower);
                std::string lower_query = query;
                std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
                
                if (text.find(lower_query) != std::string::npos) {
                    results.push_back(analysis);
                }
            }
        }

        return results;
    }

    bool ExportData(const std::string& export_path,
                   const std::chrono::system_clock::time_point& start,
                   const std::chrono::system_clock::time_point& end) {
        if (!IsReady()) {
            return false;
        }

        try {
            std::ofstream file(export_path);
            if (!file.is_open()) {
                std::cerr << "Failed to create export file: " << export_path << std::endl;
                return false;
            }

            // Export as JSON
            file << "{\n";
            file << "  \"export_info\": {\n";
            file << "    \"timestamp\": \"" << storage_utils::FormatTimestamp(std::chrono::system_clock::now()) << "\",\n";
            file << "    \"start_time\": \"" << storage_utils::FormatTimestamp(start) << "\",\n";
            file << "    \"end_time\": \"" << storage_utils::FormatTimestamp(end) << "\"\n";
            file << "  },\n";

            // Export window activities
            auto activities = GetWindowActivities(start, end);
            file << "  \"window_activities\": [\n";
            for (size_t i = 0; i < activities.size(); ++i) {
                if (i > 0) file << ",\n";
                file << "    " << storage_utils::SerializeToJson(activities[i]);
            }
            file << "\n  ],\n";

            // Export content analyses
            auto analyses = GetContentAnalyses(start, end);
            file << "  \"content_analyses\": [\n";
            for (size_t i = 0; i < analyses.size(); ++i) {
                if (i > 0) file << ",\n";
                file << "    " << storage_utils::SerializeToJson(analyses[i]);
            }
            file << "\n  ]\n";
            file << "}\n";

            file.close();
            std::cout << "Data exported to: " << export_path << std::endl;
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Export failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool ImportData(const std::string& import_path) {
        if (!IsReady()) {
            return false;
        }

        // Implementation would parse JSON and store records
        std::cout << "Import functionality not fully implemented yet" << std::endl;
        return false;
    }

    bool CleanupOldData(const std::chrono::hours& retention_period) {
        if (!IsReady()) {
            return false;
        }

        return m_storage->CleanupOldData();
    }

    bool ChangePassword(const std::string& old_password, const std::string& new_password) {
        if (!IsReady()) {
            return false;
        }

        // Verify old password
        if (old_password != m_config.master_password) {
            std::cerr << "Invalid old password" << std::endl;
            return false;
        }

        // Update configuration
        m_config.master_password = new_password;
        
        // In a real implementation, you would re-encrypt all data with the new password
        std::cout << "Password changed successfully (mock implementation)" << std::endl;
        return true;
    }

    bool VerifyIntegrity() {
        if (!IsReady()) {
            return false;
        }

        return m_storage->VerifyDataIntegrity();
    }

    bool CreateBackup(const std::string& backup_path) {
        if (!IsReady()) {
            return false;
        }

        return m_storage->BackupDatabase(backup_path);
    }

    bool RestoreFromBackup(const std::string& backup_path) {
        if (!IsReady()) {
            return false;
        }

        return m_storage->RestoreDatabase(backup_path);
    }

    StorageStatistics GetStatistics() const {
        if (!IsReady()) {
            return StorageStatistics();
        }

        return m_storage->GetStatistics();
    }

    StorageConfig GetConfig() const {
        return m_config;
    }

    void UpdateConfig(const StorageConfig& config) {
        m_config = config;
        // In a real implementation, update the storage engine configuration
    }

private:
    bool m_initialized;
    std::unique_ptr<IStorageEngine> m_storage;
    StorageConfig m_config;
    std::string m_current_session_id;
    std::chrono::system_clock::time_point m_session_start_time;
};

// EncryptedStorageManager public interface
EncryptedStorageManager::EncryptedStorageManager() : m_impl(std::make_unique<Impl>()) {}
EncryptedStorageManager::~EncryptedStorageManager() = default;

bool EncryptedStorageManager::Initialize(const StorageConfig& config) {
    return m_impl->Initialize(config);
}

void EncryptedStorageManager::Shutdown() {
    m_impl->Shutdown();
}

bool EncryptedStorageManager::IsReady() const {
    return m_impl->IsReady();
}

bool EncryptedStorageManager::StartSession(const std::string& session_name) {
    return m_impl->StartSession(session_name);
}

bool EncryptedStorageManager::EndSession() {
    return m_impl->EndSession();
}

std::string EncryptedStorageManager::GetCurrentSessionId() const {
    return m_impl->GetCurrentSessionId();
}

bool EncryptedStorageManager::StoreWindowEvent(const WindowEvent& event, const WindowInfo& info) {
    return m_impl->StoreWindowEvent(event, info);
}

bool EncryptedStorageManager::StoreScreenCapture(const CaptureFrame& frame, const std::string& context) {
    return m_impl->StoreScreenCapture(frame, context);
}

bool EncryptedStorageManager::StoreOCRResult(const OCRDocument& document, const std::string& source) {
    return m_impl->StoreOCRResult(document, source);
}

bool EncryptedStorageManager::StoreAIAnalysis(const ContentAnalysis& analysis) {
    return m_impl->StoreAIAnalysis(analysis);
}

bool EncryptedStorageManager::StoreBatch(const std::vector<WindowActivityRecord>& activities,
                                        const std::vector<ContentAnalysisRecord>& analyses) {
    return m_impl->StoreBatch(activities, analyses);
}

std::vector<WindowActivityRecord> EncryptedStorageManager::GetWindowActivities(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {
    return m_impl->GetWindowActivities(start, end);
}

std::vector<ContentAnalysisRecord> EncryptedStorageManager::GetContentAnalyses(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {
    return m_impl->GetContentAnalyses(start, end);
}

std::unordered_map<std::string, float> EncryptedStorageManager::GetProductivityReport(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {
    return m_impl->GetProductivityReport(start, end);
}

std::vector<std::pair<std::string, std::chrono::minutes>> EncryptedStorageManager::GetTimeSpentByApplication(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {
    return m_impl->GetTimeSpentByApplication(start, end);
}

std::vector<ContentAnalysisRecord> EncryptedStorageManager::SearchContent(const std::string& query,
                                                                         int max_results) {
    return m_impl->SearchContent(query, max_results);
}

bool EncryptedStorageManager::ExportData(const std::string& export_path,
                                        const std::chrono::system_clock::time_point& start,
                                        const std::chrono::system_clock::time_point& end) {
    return m_impl->ExportData(export_path, start, end);
}

bool EncryptedStorageManager::ImportData(const std::string& import_path) {
    return m_impl->ImportData(import_path);
}

bool EncryptedStorageManager::CleanupOldData(const std::chrono::hours& retention_period) {
    return m_impl->CleanupOldData(retention_period);
}

bool EncryptedStorageManager::ChangePassword(const std::string& old_password, const std::string& new_password) {
    return m_impl->ChangePassword(old_password, new_password);
}

bool EncryptedStorageManager::VerifyIntegrity() {
    return m_impl->VerifyIntegrity();
}

bool EncryptedStorageManager::CreateBackup(const std::string& backup_path) {
    return m_impl->CreateBackup(backup_path);
}

bool EncryptedStorageManager::RestoreFromBackup(const std::string& backup_path) {
    return m_impl->RestoreFromBackup(backup_path);
}

StorageStatistics EncryptedStorageManager::GetStatistics() const {
    return m_impl->GetStatistics();
}

StorageConfig EncryptedStorageManager::GetConfig() const {
    return m_impl->GetConfig();
}

void EncryptedStorageManager::UpdateConfig(const StorageConfig& config) {
    m_impl->UpdateConfig(config);
}

} // namespace work_assistant