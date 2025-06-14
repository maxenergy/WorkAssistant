#include "web_server.h"
#include "storage_engine.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <thread>
#include <mutex>
#include <set>

#ifdef WEB_ENABLED
#include <drogon/drogon.h>
using namespace drogon;
#endif

namespace work_assistant {

// WebSocketManager implementation
class WebSocketManager::Impl {
public:
    Impl() = default;
    
    void AddClient(const std::string& client_id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clients.insert(client_id);
        std::cout << "WebSocket client connected: " << client_id << std::endl;
    }
    
    void RemoveClient(const std::string& client_id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clients.erase(client_id);
        std::cout << "WebSocket client disconnected: " << client_id << std::endl;
    }
    
    void BroadcastMessage(const WSMessage& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string json_message = message.ToJson();
        
        for (const auto& client_id : m_clients) {
            // In a real implementation, send to actual WebSocket connections
            std::cout << "Broadcasting to " << client_id << ": " << json_message << std::endl;
        }
    }
    
    void SendToClient(const std::string& client_id, const WSMessage& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_clients.find(client_id) != m_clients.end()) {
            std::cout << "Sending to " << client_id << ": " << message.ToJson() << std::endl;
        }
    }
    
    size_t GetClientCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_clients.size();
    }
    
    std::vector<std::string> GetConnectedClients() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<std::string>(m_clients.begin(), m_clients.end());
    }
    
private:
    mutable std::mutex m_mutex;
    std::set<std::string> m_clients;
};

// WebServer implementation
class WebServer::Impl {
public:
    Impl() 
        : m_initialized(false)
        , m_running(false)
        , m_websocket_manager(std::make_unique<WebSocketManager>()) {
    }
    
    bool Initialize(const WebServerConfig& config, 
                   std::shared_ptr<EncryptedStorageManager> storage) {
        if (m_initialized) {
            return true;
        }
        
        if (!config.IsValid()) {
            std::cerr << "Invalid web server configuration" << std::endl;
            return false;
        }
        
        if (!storage || !storage->IsReady()) {
            std::cerr << "Storage manager not ready" << std::endl;
            return false;
        }
        
        m_config = config;
        m_storage = storage;
        
#ifdef WEB_DISABLED
        std::cout << "Web interface disabled (Drogon not found)" << std::endl;
        m_initialized = true;
        return true;
#elif defined(WEB_ENABLED)
        // Initialize Drogon framework
        try {
            app().setLogPath("./logs")
                .setLogLevel(trantor::Logger::kInfo)
                .addListener(m_config.host, m_config.port)
                .setThreadNum(4)
                .enableRunAsDaemon()
                .setDocumentRoot(m_config.static_files_path);
            
            if (m_config.enable_cors) {
                app().registerPreSendingAdvice([](const HttpRequestPtr&, const HttpResponsePtr& resp) {
                    resp->addHeader("Access-Control-Allow-Origin", "*");
                    resp->addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
                    resp->addHeader("Access-Control-Allow-Headers", "Content-Type,Authorization");
                });
            }
            
            SetupRoutes();
            
            m_initialized = true;
            std::cout << "Web server initialized on " << m_config.host << ":" << m_config.port << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize web server: " << e.what() << std::endl;
            return false;
        }
#else
        std::cout << "Web interface disabled (Drogon not available)" << std::endl;
        m_initialized = true;
        return true;
#endif
    }
    
    bool Start() {
        if (!m_initialized) {
            std::cerr << "Web server not initialized" << std::endl;
            return false;
        }
        
        if (m_running) {
            return true;
        }
        
#if defined(WEB_DISABLED) || !defined(WEB_ENABLED)
        std::cout << "Web server start skipped (disabled)" << std::endl;
        m_running = true;
        return true;
#else
        try {
            // Start server in a separate thread
            m_server_thread = std::make_unique<std::thread>([this]() {
                app().run();
            });
            
            m_running = true;
            m_start_time = std::chrono::system_clock::now();
            std::cout << "Web server started successfully" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to start web server: " << e.what() << std::endl;
            return false;
        }
#endif
    }
    
    void Stop() {
        if (!m_running) {
            return;
        }
        
#ifdef WEB_ENABLED
        try {
            app().quit();
            
            if (m_server_thread && m_server_thread->joinable()) {
                m_server_thread->join();
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error stopping web server: " << e.what() << std::endl;
        }
#endif
        
        m_running = false;
        std::cout << "Web server stopped" << std::endl;
    }
    
    bool IsRunning() const {
        return m_running;
    }
    
    void Shutdown() {
        Stop();
        
        if (m_websocket_manager) {
            m_websocket_manager.reset();
        }
        
        m_storage.reset();
        m_initialized = false;
        std::cout << "Web server shut down" << std::endl;
    }
    
    void OnWindowEvent(const WindowEvent& event, const WindowInfo& info) {
        if (!m_running || !m_websocket_manager) {
            return;
        }
        
        WSMessage message;
        message.type = WSMessageType::WINDOW_EVENT;
        message.timestamp = std::chrono::system_clock::now();
        
        // Create JSON data
        std::ostringstream json;
        json << "{";
        json << "\"type\": \"" << static_cast<int>(event.type) << "\",";
        json << "\"window_title\": \"" << web_utils::EscapeJsonString(info.title) << "\",";
        json << "\"process_name\": \"" << web_utils::EscapeJsonString(info.process_name) << "\",";
        json << "\"timestamp\": \"" << storage_utils::FormatTimestamp(event.timestamp) << "\"";
        json << "}";
        
        message.data = json.str();
        m_websocket_manager->BroadcastMessage(message);
        m_stats.total_requests++;
    }
    
    void OnOCRResult(const OCRDocument& document) {
        if (!m_running || !m_websocket_manager) {
            return;
        }
        
        WSMessage message;
        message.type = WSMessageType::OCR_RESULT;
        message.timestamp = std::chrono::system_clock::now();
        
        std::ostringstream json;
        json << "{";
        json << "\"text\": \"" << web_utils::EscapeJsonString(document.GetOrderedText()) << "\",";
        json << "\"confidence\": " << document.overall_confidence << ",";
        json << "\"blocks_count\": " << document.text_blocks.size();
        json << "}";
        
        message.data = json.str();
        m_websocket_manager->BroadcastMessage(message);
    }
    
    void OnAIAnalysis(const ContentAnalysis& analysis) {
        if (!m_running || !m_websocket_manager) {
            return;
        }
        
        WSMessage message;
        message.type = WSMessageType::AI_ANALYSIS;
        message.timestamp = std::chrono::system_clock::now();
        
        std::ostringstream json;
        json << "{";
        json << "\"content_type\": " << static_cast<int>(analysis.content_type) << ",";
        json << "\"work_category\": " << static_cast<int>(analysis.work_category) << ",";
        json << "\"is_productive\": " << (analysis.is_productive ? "true" : "false") << ",";
        json << "\"confidence\": " << analysis.classification_confidence << ",";
        json << "\"application\": \"" << web_utils::EscapeJsonString(analysis.application) << "\"";
        json << "}";
        
        message.data = json.str();
        m_websocket_manager->BroadcastMessage(message);
    }
    
    WebServerConfig GetConfig() const {
        return m_config;
    }
    
    void UpdateConfig(const WebServerConfig& config) {
        m_config = config;
        // In a real implementation, update the running server
    }
    
    WebServer::WebServerStats GetStatistics() const {
        WebServer::WebServerStats stats;
        stats.total_requests = m_stats.total_requests;
        stats.active_websocket_connections = m_websocket_manager ? m_websocket_manager->GetClientCount() : 0;
        stats.start_time = m_start_time;
        return stats;
    }
    
private:
#ifdef WEB_ENABLED
    void SetupRoutes() {
        auto api_prefix = m_config.api_prefix;
        
        // Productivity endpoints
        app().registerHandler(api_prefix + "/productivity/summary",
            [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
                HandleProductivitySummary(req, std::move(callback));
            },
            {Get, Post});
        
        app().registerHandler(api_prefix + "/productivity/timeline",
            [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
                HandleActivityTimeline(req, std::move(callback));
            },
            {Get, Post});
        
        app().registerHandler(api_prefix + "/applications/usage",
            [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
                HandleApplicationUsage(req, std::move(callback));
            },
            {Get, Post});
        
        // Search endpoints
        app().registerHandler(api_prefix + "/search",
            [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
                HandleSearch(req, std::move(callback));
            },
            {Get, Post});
        
        // Export endpoints
        app().registerHandler(api_prefix + "/export",
            [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
                HandleExport(req, std::move(callback));
            },
            {Get, Post});
        
        // System endpoints
        app().registerHandler(api_prefix + "/system/status",
            [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
                HandleSystemStatus(req, std::move(callback));
            },
            {Get});
        
        // WebSocket endpoint
        if (m_config.enable_websocket) {
            app().registerHandler(api_prefix + "/ws",
                [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
                    HandleWebSocket(req, std::move(callback));
                },
                {Get});
        }
        
        // Static file serving
        app().registerHandler("/",
            [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
                auto resp = HttpResponse::newFileResponse("web/static/index.html");
                callback(resp);
            },
            {Get});
    }
    
    void HandleProductivitySummary(const HttpRequestPtr& req, 
                                 std::function<void(const HttpResponsePtr&)>&& callback) {
        m_stats.total_requests++;
        
        // Parse time range from request
        auto start = std::chrono::system_clock::now() - std::chrono::hours(24);
        auto end = std::chrono::system_clock::now();
        
        auto response = api_handlers::GetProductivitySummary(start, end, m_storage.get());
        
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(static_cast<HttpStatusCode>(response.status_code));
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(response.ToJson());
        
        callback(resp);
    }
    
    void HandleActivityTimeline(const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& callback) {
        m_stats.total_requests++;
        
        auto start = std::chrono::system_clock::now() - std::chrono::hours(24);
        auto end = std::chrono::system_clock::now();
        
        auto response = api_handlers::GetActivityTimeline(start, end, m_storage.get());
        
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(static_cast<HttpStatusCode>(response.status_code));
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(response.ToJson());
        
        callback(resp);
    }
    
    void HandleApplicationUsage(const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& callback) {
        m_stats.total_requests++;
        
        auto start = std::chrono::system_clock::now() - std::chrono::hours(24);
        auto end = std::chrono::system_clock::now();
        
        auto response = api_handlers::GetApplicationUsage(start, end, m_storage.get());
        
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(static_cast<HttpStatusCode>(response.status_code));
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(response.ToJson());
        
        callback(resp);
    }
    
    void HandleSearch(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback) {
        m_stats.total_requests++;
        
        std::string query = req->getParameter("q");
        int max_results = 50;
        
        try {
            std::string limit_str = req->getParameter("limit");
            if (!limit_str.empty()) {
                max_results = std::stoi(limit_str);
            }
        } catch (...) {
            max_results = 50;
        }
        
        auto response = api_handlers::SearchContent(query, max_results, m_storage.get());
        
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(static_cast<HttpStatusCode>(response.status_code));
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(response.ToJson());
        
        callback(resp);
    }
    
    void HandleExport(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback) {
        m_stats.total_requests++;
        
        auto start = std::chrono::system_clock::now() - std::chrono::hours(24);
        auto end = std::chrono::system_clock::now();
        std::string format = req->getParameter("format");
        if (format.empty()) format = "json";
        
        auto response = api_handlers::ExportData(start, end, format, m_storage.get());
        
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(static_cast<HttpStatusCode>(response.status_code));
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(response.ToJson());
        
        callback(resp);
    }
    
    void HandleSystemStatus(const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback) {
        m_stats.total_requests++;
        
        auto response = api_handlers::GetSystemStatus();
        
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(static_cast<HttpStatusCode>(response.status_code));
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(response.ToJson());
        
        callback(resp);
    }
    
    void HandleWebSocket(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback) {
        // WebSocket implementation would go here
        // For now, return a placeholder response
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k501NotImplemented);
        resp->setBody("WebSocket not implemented yet");
        callback(resp);
    }
#endif

private:
    bool m_initialized;
    bool m_running;
    WebServerConfig m_config;
    std::shared_ptr<EncryptedStorageManager> m_storage;
    std::unique_ptr<WebSocketManager> m_websocket_manager;
    std::chrono::system_clock::time_point m_start_time;
    
    struct {
        size_t total_requests = 0;
    } m_stats;
    
#ifdef WEB_ENABLED
    std::unique_ptr<std::thread> m_server_thread;
#endif
};

// WebSocketManager implementation
WebSocketManager::WebSocketManager() : m_impl(std::make_unique<Impl>()) {}
WebSocketManager::~WebSocketManager() = default;

void WebSocketManager::AddClient(const std::string& client_id) {
    m_impl->AddClient(client_id);
}

void WebSocketManager::RemoveClient(const std::string& client_id) {
    m_impl->RemoveClient(client_id);
}

void WebSocketManager::BroadcastMessage(const WSMessage& message) {
    m_impl->BroadcastMessage(message);
}

void WebSocketManager::SendToClient(const std::string& client_id, const WSMessage& message) {
    m_impl->SendToClient(client_id, message);
}

size_t WebSocketManager::GetClientCount() const {
    return m_impl->GetClientCount();
}

std::vector<std::string> WebSocketManager::GetConnectedClients() const {
    return m_impl->GetConnectedClients();
}

// WebServer implementation
WebServer::WebServer() : m_impl(std::make_unique<Impl>()) {}
WebServer::~WebServer() = default;

bool WebServer::Initialize(const WebServerConfig& config, 
                          std::shared_ptr<EncryptedStorageManager> storage) {
    return m_impl->Initialize(config, storage);
}

bool WebServer::Start() {
    return m_impl->Start();
}

void WebServer::Stop() {
    m_impl->Stop();
}

bool WebServer::IsRunning() const {
    return m_impl->IsRunning();
}

void WebServer::Shutdown() {
    m_impl->Shutdown();
}

void WebServer::OnWindowEvent(const WindowEvent& event, const WindowInfo& info) {
    m_impl->OnWindowEvent(event, info);
}

void WebServer::OnOCRResult(const OCRDocument& document) {
    m_impl->OnOCRResult(document);
}

void WebServer::OnAIAnalysis(const ContentAnalysis& analysis) {
    m_impl->OnAIAnalysis(analysis);
}

WebServerConfig WebServer::GetConfig() const {
    return m_impl->GetConfig();
}

void WebServer::UpdateConfig(const WebServerConfig& config) {
    m_impl->UpdateConfig(config);
}

WebServer::WebServerStats WebServer::GetStatistics() const {
    return m_impl->GetStatistics();
}

// Utility functions
std::string ApiResponse::ToJson() const {
    std::ostringstream json;
    json << "{";
    json << "\"success\": " << (success ? "true" : "false") << ",";
    json << "\"message\": \"" << web_utils::EscapeJsonString(message) << "\",";
    json << "\"data\": " << (data.empty() ? "null" : data) << ",";
    json << "\"status_code\": " << status_code;
    json << "}";
    return json.str();
}

std::string WSMessage::ToJson() const {
    std::ostringstream json;
    json << "{";
    json << "\"type\": " << static_cast<int>(type) << ",";
    json << "\"data\": " << data << ",";
    json << "\"timestamp\": \"" << storage_utils::FormatTimestamp(timestamp) << "\"";
    json << "}";
    return json.str();
}

} // namespace work_assistant