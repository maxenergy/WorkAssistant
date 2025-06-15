#include "websocket_server.h"
#include <drogon/drogon.h>
#include <drogon/WebSocketController.h>
#include <drogon/HttpController.h>
#include <json/json.h>
#include <thread>
#include <mutex>
#include <set>
#include <fstream>

namespace work_assistant {

using namespace drogon;

// WebSocket handler
class WorkAssistantWS : public drogon::WebSocketController<WorkAssistantWS> {
public:
    static void initPathRouting() {
        registerWebSocketController("/ws");
    }

    virtual void handleNewMessage(const WebSocketConnectionPtr& wsConnPtr,
                                  std::string&& message,
                                  const WebSocketMessageType& type) override {
        if (type == WebSocketMessageType::Text) {
            // Parse JSON message
            Json::Value request;
            Json::Reader reader;
            
            if (reader.parse(message, request)) {
                std::string cmd = request.get("command", "").asString();
                
                Json::Value response;
                response["type"] = "response";
                response["command"] = cmd;
                
                if (cmd == "get_status") {
                    response["status"] = "running";
                    response["monitoring"] = true;
                    response["windows_tracked"] = 5;
                } else if (cmd == "get_stats") {
                    response["cpu_usage"] = 2.5;
                    response["memory_mb"] = 150;
                    response["uptime_seconds"] = 3600;
                } else {
                    response["error"] = "Unknown command";
                }
                
                Json::FastWriter writer;
                wsConnPtr->send(writer.write(response));
            }
        }
    }

    virtual void handleNewConnection(const HttpRequestPtr& req,
                                     const WebSocketConnectionPtr& wsConnPtr) override {
        LOG_INFO << "New WebSocket connection from " << wsConnPtr->peerAddr().toIpPort();
        
        // Send welcome message
        Json::Value welcome;
        welcome["type"] = "welcome";
        welcome["version"] = "1.0.0";
        welcome["message"] = "Connected to Work Assistant";
        
        Json::FastWriter writer;
        wsConnPtr->send(writer.write(welcome));
    }

    virtual void handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr) override {
        LOG_INFO << "WebSocket connection closed";
    }
};

// REST API controller
class WorkAssistantAPI : public drogon::HttpController<WorkAssistantAPI> {
public:
    static void initPathRouting() {
        METHOD_ADD(WorkAssistantAPI::getStatus, "/api/status", Get);
        METHOD_ADD(WorkAssistantAPI::getConfig, "/api/config", Get);
        METHOD_ADD(WorkAssistantAPI::updateConfig, "/api/config", Post);
        METHOD_ADD(WorkAssistantAPI::getWindows, "/api/windows", Get);
        METHOD_ADD(WorkAssistantAPI::getOCRResults, "/api/ocr/results", Get);
        METHOD_ADD(WorkAssistantAPI::captureScreen, "/api/capture", Post);
    }

    void getStatus(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback) {
        Json::Value result;
        result["status"] = "running";
        result["version"] = "1.0.0";
        result["uptime"] = 3600;
        result["monitoring"] = true;
        
        auto resp = HttpResponse::newHttpJsonResponse(result);
        callback(resp);
    }

    void getConfig(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback) {
        Json::Value config;
        config["ocr_mode"] = "fast";
        config["capture_interval"] = 5000;
        config["ai_enabled"] = true;
        config["web_port"] = 8080;
        
        auto resp = HttpResponse::newHttpJsonResponse(config);
        callback(resp);
    }

    void updateConfig(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback) {
        auto json = req->getJsonObject();
        if (!json) {
            Json::Value error;
            error["error"] = "Invalid JSON";
            auto resp = HttpResponse::newHttpJsonResponse(error);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }
        
        // Process config update
        Json::Value result;
        result["success"] = true;
        result["message"] = "Configuration updated";
        
        auto resp = HttpResponse::newHttpJsonResponse(result);
        callback(resp);
    }

    void getWindows(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
        Json::Value result;
        Json::Value windows(Json::arrayValue);
        
        // Mock window data
        Json::Value window1;
        window1["id"] = 1;
        window1["title"] = "Visual Studio Code";
        window1["process"] = "code.exe";
        window1["focused"] = true;
        windows.append(window1);
        
        Json::Value window2;
        window2["id"] = 2;
        window2["title"] = "Chrome";
        window2["process"] = "chrome.exe";
        window2["focused"] = false;
        windows.append(window2);
        
        result["windows"] = windows;
        result["count"] = 2;
        
        auto resp = HttpResponse::newHttpJsonResponse(result);
        callback(resp);
    }

    void getOCRResults(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
        Json::Value result;
        Json::Value results(Json::arrayValue);
        
        // Mock OCR result
        Json::Value ocr1;
        ocr1["timestamp"] = "2025-06-15T10:30:00Z";
        ocr1["window_title"] = "Document.pdf";
        ocr1["text"] = "Sample extracted text from document";
        ocr1["confidence"] = 0.95;
        results.append(ocr1);
        
        result["results"] = results;
        result["count"] = 1;
        
        auto resp = HttpResponse::newHttpJsonResponse(result);
        callback(resp);
    }

    void captureScreen(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) {
        Json::Value result;
        result["success"] = true;
        result["message"] = "Screen capture initiated";
        result["capture_id"] = "cap_" + std::to_string(time(nullptr));
        
        auto resp = HttpResponse::newHttpJsonResponse(result);
        callback(resp);
    }
};

// Drogon-based WebSocket server implementation
class DrogonWebServer : public IWebSocketServer {
public:
    DrogonWebServer() : m_running(false), m_port(8080) {}
    
    ~DrogonWebServer() override {
        Stop();
    }

    bool Start(int port) override {
        if (m_running) {
            return true;
        }

        m_port = port;

        // Configure Drogon
        app().setLogPath("./logs")
              .setLogLevel(trantor::Logger::kInfo)
              .addListener("0.0.0.0", m_port)
              .setThreadNum(4)
              .setDocumentRoot("./web/static");

        // Set up custom 404 handler
        app().setCustom404Page(
            Json::Value{{"error", "Not Found"}, {"code", 404}},
            ContentType::CT_APPLICATION_JSON
        );

        // Register controllers
        app().registerController(std::make_shared<WorkAssistantWS>());
        app().registerController(std::make_shared<WorkAssistantAPI>());

        // Configure static file serving
        app().setStaticFilesCacheTime(0); // Disable cache for development

        // Start server in separate thread
        m_serverThread = std::thread([this]() {
            try {
                LOG_INFO << "Starting Drogon web server on port " << m_port;
                app().run();
            } catch (const std::exception& e) {
                LOG_ERROR << "Drogon server error: " << e.what();
            }
        });

        m_running = true;
        
        // Wait a bit for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        LOG_INFO << "Drogon web server started on http://127.0.0.1:" << m_port;
        return true;
    }

    void Stop() override {
        if (!m_running) {
            return;
        }

        LOG_INFO << "Stopping Drogon web server...";
        app().quit();
        
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }

        m_running = false;
        LOG_INFO << "Drogon web server stopped";
    }

    bool IsRunning() const override {
        return m_running;
    }

    void BroadcastMessage(const std::string& message) override {
        // Drogon handles WebSocket connections internally
        // This would require maintaining a list of connections
        LOG_DEBUG << "Broadcasting: " << message;
    }

    void SendToClient(const std::string& clientId, const std::string& message) override {
        // Drogon handles WebSocket connections internally
        LOG_DEBUG << "Sending to " << clientId << ": " << message;
    }

    void OnClientMessage(std::function<void(const std::string&, const std::string&)> handler) override {
        m_messageHandler = handler;
    }

    void OnClientConnect(std::function<void(const std::string&)> handler) override {
        m_connectHandler = handler;
    }

    void OnClientDisconnect(std::function<void(const std::string&)> handler) override {
        m_disconnectHandler = handler;
    }

private:
    std::atomic<bool> m_running;
    int m_port;
    std::thread m_serverThread;
    
    std::function<void(const std::string&, const std::string&)> m_messageHandler;
    std::function<void(const std::string&)> m_connectHandler;
    std::function<void(const std::string&)> m_disconnectHandler;
};

// Factory implementation for Drogon
std::unique_ptr<IWebSocketServer> CreateDrogonWebServer() {
    return std::make_unique<DrogonWebServer>();
}

} // namespace work_assistant