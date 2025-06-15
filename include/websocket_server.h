#pragma once

#include "common_types.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>

namespace work_assistant {

// WebSocket message types
enum class WSMessageType {
    WINDOW_EVENT = 0,
    OCR_RESULT = 1,
    AI_ANALYSIS = 2,
    PRODUCTIVITY_UPDATE = 3,
    SYSTEM_STATUS = 4,
    SUBSCRIPTION = 5,
    ERROR_MESSAGE = 6
};

// WebSocket message structure
struct WSMessage {
    WSMessageType type;
    std::string data;
    std::chrono::system_clock::time_point timestamp;
    
    std::string ToJSON() const;
    static WSMessage FromJSON(const std::string& json);
};

// WebSocket connection interface
class IWebSocketConnection {
public:
    virtual ~IWebSocketConnection() = default;
    virtual void Send(const WSMessage& message) = 0;
    virtual void Send(const std::string& data) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;
    virtual std::string GetClientId() const = 0;
    virtual std::string GetRemoteAddress() const = 0;
};

// Simple WebSocket connection implementation
class SimpleWebSocketConnection : public IWebSocketConnection {
public:
    SimpleWebSocketConnection(int socket_fd, const std::string& client_id);
    ~SimpleWebSocketConnection();
    
    void Send(const WSMessage& message) override;
    void Send(const std::string& data) override;
    void Close() override;
    bool IsOpen() const override;
    std::string GetClientId() const override;
    std::string GetRemoteAddress() const override;
    
    // Internal methods
    bool PerformHandshake(const std::string& request);
    void ProcessIncomingData();
    
private:
    std::string EncodeWebSocketFrame(const std::string& payload);
    std::string DecodeWebSocketFrame(const std::vector<uint8_t>& frame);
    std::string GenerateWebSocketAccept(const std::string& key);
    
private:
    int m_socket_fd;
    std::string m_client_id;
    std::string m_remote_address;
    std::atomic<bool> m_is_open;
    mutable std::mutex m_send_mutex;
};

// WebSocket server interface
class IWebSocketServer {
public:
    virtual ~IWebSocketServer() = default;
    
    // Server lifecycle
    virtual bool Start(int port = 8081) = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;
    
    // Connection management
    virtual void BroadcastMessage(const WSMessage& message) = 0;
    virtual void SendToClient(const std::string& client_id, const WSMessage& message) = 0;
    virtual std::vector<std::string> GetConnectedClients() const = 0;
    virtual size_t GetConnectionCount() const = 0;
    
    // Event handlers
    using ConnectionHandler = std::function<void(std::shared_ptr<IWebSocketConnection>)>;
    using MessageHandler = std::function<void(std::shared_ptr<IWebSocketConnection>, const WSMessage&)>;
    using DisconnectionHandler = std::function<void(const std::string& client_id)>;
    
    virtual void SetConnectionHandler(ConnectionHandler handler) = 0;
    virtual void SetMessageHandler(MessageHandler handler) = 0;
    virtual void SetDisconnectionHandler(DisconnectionHandler handler) = 0;
};

// Simple WebSocket server implementation
class SimpleWebSocketServer : public IWebSocketServer {
public:
    SimpleWebSocketServer();
    ~SimpleWebSocketServer();
    
    // IWebSocketServer implementation
    bool Start(int port = 8081) override;
    void Stop() override;
    bool IsRunning() const override;
    
    void BroadcastMessage(const WSMessage& message) override;
    void SendToClient(const std::string& client_id, const WSMessage& message) override;
    std::vector<std::string> GetConnectedClients() const override;
    size_t GetConnectionCount() const override;
    
    void SetConnectionHandler(ConnectionHandler handler) override;
    void SetMessageHandler(MessageHandler handler) override;
    void SetDisconnectionHandler(DisconnectionHandler handler) override;
    
private:
    void ServerLoop();
    void HandleNewConnection(int client_socket);
    void CleanupClosedConnections();
    std::string GenerateClientId();
    
private:
    int m_server_socket;
    int m_port;
    std::atomic<bool> m_running;
    std::thread m_server_thread;
    
    // Connection management
    std::unordered_map<std::string, std::shared_ptr<SimpleWebSocketConnection>> m_connections;
    mutable std::mutex m_connections_mutex;
    
    // Event handlers
    ConnectionHandler m_connection_handler;
    MessageHandler m_message_handler;
    DisconnectionHandler m_disconnection_handler;
    
    // Cleanup thread
    std::thread m_cleanup_thread;
    std::atomic<bool> m_cleanup_running;
};

// WebSocket message utilities
class WSMessageBuilder {
public:
    static WSMessage CreateWindowEvent(const WindowEvent& event, const WindowInfo& info);
    static WSMessage CreateOCRResult(const OCRDocument& document);
    static WSMessage CreateAIAnalysis(const ContentAnalysis& analysis);
    static WSMessage CreateProductivityUpdate(int score);
    static WSMessage CreateSystemStatus(const std::string& status);
    static WSMessage CreateWelcomeMessage();
    static WSMessage CreateErrorMessage(const std::string& error);
};

// WebSocket client manager for handling subscriptions
class WebSocketClientManager {
public:
    WebSocketClientManager(std::shared_ptr<IWebSocketServer> server);
    ~WebSocketClientManager();
    
    // Subscription management
    void HandleClientSubscription(const std::string& client_id, const std::vector<std::string>& subscriptions);
    void UnsubscribeClient(const std::string& client_id);
    
    // Event broadcasting
    void BroadcastWindowEvent(const WindowEvent& event, const WindowInfo& info);
    void BroadcastOCRResult(const OCRDocument& document);
    void BroadcastAIAnalysis(const ContentAnalysis& analysis);
    void BroadcastProductivityUpdate(int score);
    void BroadcastSystemStatus(const std::string& status);
    
    // Statistics
    size_t GetSubscribedClientCount() const;
    std::vector<std::string> GetClientSubscriptions(const std::string& client_id) const;
    
private:
    void OnClientConnected(std::shared_ptr<IWebSocketConnection> connection);
    void OnClientMessage(std::shared_ptr<IWebSocketConnection> connection, const WSMessage& message);
    void OnClientDisconnected(const std::string& client_id);
    
    bool IsClientSubscribed(const std::string& client_id, const std::string& event_type) const;
    
private:
    std::shared_ptr<IWebSocketServer> m_server;
    
    // Client subscriptions: client_id -> set of subscribed event types
    std::unordered_map<std::string, std::vector<std::string>> m_client_subscriptions;
    mutable std::mutex m_subscriptions_mutex;
};

} // namespace work_assistant