#pragma once

#include "storage_engine.h"
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>

#ifdef WEB_ENABLED
#include <drogon/drogon.h>
using namespace drogon;
#endif

namespace work_assistant {

class EncryptedStorageManager;

// Web server configuration
struct WebServerConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string static_files_path = "web/static";
    bool enable_cors = true;
    bool enable_ssl = false;
    std::string ssl_cert_path;
    std::string ssl_key_path;
    std::string api_prefix = "/api/v1";
    bool enable_websocket = true;
    
    bool IsValid() const {
        return port > 0 && port < 65536 && !host.empty();
    }
};

// API Response structures
struct ApiResponse {
    bool success = true;
    std::string message;
    std::string data; // JSON string
    int status_code = 200;
    
    std::string ToJson() const;
};

struct ProductivitySummary {
    float productivity_score = 0.0f;
    float focused_time_ratio = 0.0f;
    int total_activities = 0;
    std::unordered_map<std::string, float> app_usage;
    std::unordered_map<std::string, int> content_types;
    std::string most_productive_hour;
    std::vector<std::string> top_keywords;
    
    std::string ToJson() const;
};

struct ActivityTimeline {
    std::vector<ContentAnalysisRecord> activities;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    std::string ToJson() const;
};

// WebSocket message types
enum class WSMessageType {
    WINDOW_EVENT,
    OCR_RESULT,
    AI_ANALYSIS,
    PRODUCTIVITY_UPDATE,
    SYSTEM_STATUS
};

struct WSMessage {
    WSMessageType type;
    std::string data; // JSON string
    std::chrono::system_clock::time_point timestamp;
    
    std::string ToJson() const;
};

// WebSocket client management
class WebSocketManager {
public:
    WebSocketManager();
    ~WebSocketManager();
    
    void AddClient(const std::string& client_id);
    void RemoveClient(const std::string& client_id);
    void BroadcastMessage(const WSMessage& message);
    void SendToClient(const std::string& client_id, const WSMessage& message);
    
    size_t GetClientCount() const;
    std::vector<std::string> GetConnectedClients() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// Main web server class
class WebServer {
public:
    WebServer();
    ~WebServer();
    
    bool Initialize(const WebServerConfig& config, 
                   std::shared_ptr<EncryptedStorageManager> storage);
    bool Start();
    void Stop();
    bool IsRunning() const;
    
    void Shutdown();
    
    // Event callbacks for real-time updates
    void OnWindowEvent(const WindowEvent& event, const WindowInfo& info);
    void OnOCRResult(const OCRDocument& document);
    void OnAIAnalysis(const ContentAnalysis& analysis);
    
    WebServerConfig GetConfig() const;
    void UpdateConfig(const WebServerConfig& config);
    
    // Statistics
    struct WebServerStats {
        size_t total_requests = 0;
        size_t active_websocket_connections = 0;
        std::chrono::system_clock::time_point start_time;
        std::string version = "1.0.0";
    };
    
    WebServerStats GetStatistics() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// API Handler functions (used internally)
namespace api_handlers {
    
    // Productivity endpoints
    ApiResponse GetProductivitySummary(const std::chrono::system_clock::time_point& start,
                                     const std::chrono::system_clock::time_point& end,
                                     EncryptedStorageManager* storage);
    
    ApiResponse GetActivityTimeline(const std::chrono::system_clock::time_point& start,
                                  const std::chrono::system_clock::time_point& end,
                                  EncryptedStorageManager* storage);
    
    ApiResponse GetApplicationUsage(const std::chrono::system_clock::time_point& start,
                                  const std::chrono::system_clock::time_point& end,
                                  EncryptedStorageManager* storage);
    
    // Search endpoints
    ApiResponse SearchContent(const std::string& query, int max_results,
                            EncryptedStorageManager* storage);
    
    // Export endpoints
    ApiResponse ExportData(const std::chrono::system_clock::time_point& start,
                         const std::chrono::system_clock::time_point& end,
                         const std::string& format,
                         EncryptedStorageManager* storage);
    
    // System endpoints
    ApiResponse GetSystemStatus();
    ApiResponse GetConfiguration();
    
} // namespace api_handlers

// Utility functions
namespace web_utils {
    
    std::string GetMimeType(const std::string& file_extension);
    std::string FormatFileSize(size_t bytes);
    std::string EscapeJsonString(const std::string& input);
    std::chrono::system_clock::time_point ParseTimestamp(const std::string& timestamp);
    bool ValidateTimeRange(const std::chrono::system_clock::time_point& start,
                          const std::chrono::system_clock::time_point& end);
    
} // namespace web_utils

} // namespace work_assistant