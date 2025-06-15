#include "websocket_server.h"
#include "logger.h"
#include <sstream>
#include <random>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <algorithm>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <iomanip>

namespace work_assistant {

// WebSocket protocol constants
const std::string WS_MAGIC_STRING = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// WSMessage implementation
std::string WSMessage::ToJSON() const {
    std::stringstream ss;
    ss << "{";
    ss << "\"type\":" << static_cast<int>(type) << ",";
    ss << "\"timestamp\":\"" << std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count() << "\",";
    ss << "\"data\":" << data;
    ss << "}";
    return ss.str();
}

WSMessage WSMessage::FromJSON(const std::string& json) {
    WSMessage message;
    message.timestamp = std::chrono::system_clock::now();
    
    // Simple JSON parsing for message type
    size_t type_pos = json.find("\"type\":");
    if (type_pos != std::string::npos) {
        size_t value_start = json.find(":", type_pos) + 1;
        size_t value_end = json.find(",", value_start);
        if (value_end == std::string::npos) {
            value_end = json.find("}", value_start);
        }
        
        if (value_end != std::string::npos) {
            std::string type_str = json.substr(value_start, value_end - value_start);
            // Remove whitespace
            type_str.erase(std::remove_if(type_str.begin(), type_str.end(), ::isspace), type_str.end());
            
            try {
                int type_int = std::stoi(type_str);
                message.type = static_cast<WSMessageType>(type_int);
            } catch (...) {
                message.type = WSMessageType::ERROR_MESSAGE;
            }
        }
    }
    
    // Extract data field
    size_t data_pos = json.find("\"data\":");
    if (data_pos != std::string::npos) {
        size_t value_start = json.find(":", data_pos) + 1;
        size_t brace_count = 0;
        size_t value_end = value_start;
        bool in_string = false;
        
        for (size_t i = value_start; i < json.length(); ++i) {
            if (json[i] == '"' && (i == 0 || json[i-1] != '\\')) {
                in_string = !in_string;
            } else if (!in_string) {
                if (json[i] == '{') brace_count++;
                else if (json[i] == '}') {
                    if (brace_count == 0) {
                        value_end = i;
                        break;
                    }
                    brace_count--;
                }
            }
        }
        
        if (value_end > value_start) {
            message.data = json.substr(value_start, value_end - value_start);
            // Trim whitespace
            size_t first = message.data.find_first_not_of(" \t\r\n");
            size_t last = message.data.find_last_not_of(" \t\r\n");
            if (first != std::string::npos && last != std::string::npos) {
                message.data = message.data.substr(first, last - first + 1);
            }
        }
    }
    
    return message;
}

// SimpleWebSocketConnection implementation
SimpleWebSocketConnection::SimpleWebSocketConnection(int socket_fd, const std::string& client_id)
    : m_socket_fd(socket_fd)
    , m_client_id(client_id)
    , m_is_open(true) {
    
    // Get remote address
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(socket_fd, (struct sockaddr*)&addr, &addr_len) == 0) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        m_remote_address = std::string(ip_str) + ":" + std::to_string(ntohs(addr.sin_port));
    }
}

SimpleWebSocketConnection::~SimpleWebSocketConnection() {
    Close();
}

void SimpleWebSocketConnection::Send(const WSMessage& message) {
    Send(message.ToJSON());
}

void SimpleWebSocketConnection::Send(const std::string& data) {
    if (!m_is_open) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_send_mutex);
    
    try {
        std::string frame = EncodeWebSocketFrame(data);
        ssize_t bytes_sent = ::send(m_socket_fd, frame.c_str(), frame.length(), MSG_NOSIGNAL);
        
        if (bytes_sent <= 0) {
            LOG_WARNING("Failed to send WebSocket message to client " + m_client_id);
            m_is_open = false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while sending WebSocket message: " + std::string(e.what()));
        m_is_open = false;
    }
}

void SimpleWebSocketConnection::Close() {
    if (m_is_open) {
        m_is_open = false;
        if (m_socket_fd > 0) {
            close(m_socket_fd);
            m_socket_fd = -1;
        }
    }
}

bool SimpleWebSocketConnection::IsOpen() const {
    return m_is_open;
}

std::string SimpleWebSocketConnection::GetClientId() const {
    return m_client_id;
}

std::string SimpleWebSocketConnection::GetRemoteAddress() const {
    return m_remote_address;
}

bool SimpleWebSocketConnection::PerformHandshake(const std::string& request) {
    // Extract WebSocket key
    size_t key_pos = request.find("Sec-WebSocket-Key:");
    if (key_pos == std::string::npos) {
        return false;
    }
    
    size_t key_start = request.find(":", key_pos) + 1;
    size_t key_end = request.find("\r\n", key_start);
    if (key_end == std::string::npos) {
        return false;
    }
    
    std::string websocket_key = request.substr(key_start, key_end - key_start);
    // Trim whitespace
    websocket_key.erase(0, websocket_key.find_first_not_of(" \t"));
    websocket_key.erase(websocket_key.find_last_not_of(" \t") + 1);
    
    // Generate response
    std::string accept_key = GenerateWebSocketAccept(websocket_key);
    
    std::stringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << accept_key << "\r\n";
    response << "\r\n";
    
    std::string response_str = response.str();
    ssize_t bytes_sent = ::send(m_socket_fd, response_str.c_str(), response_str.length(), 0);
    
    return bytes_sent == static_cast<ssize_t>(response_str.length());
}

std::string SimpleWebSocketConnection::EncodeWebSocketFrame(const std::string& payload) {
    std::vector<uint8_t> frame;
    
    // First byte: FIN bit set, opcode for text frame
    frame.push_back(0x81);
    
    // Payload length
    if (payload.length() < 126) {
        frame.push_back(static_cast<uint8_t>(payload.length()));
    } else if (payload.length() < 65536) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((payload.length() >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(payload.length() & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((payload.length() >> (i * 8)) & 0xFF));
        }
    }
    
    // Payload data
    for (char c : payload) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    
    return std::string(frame.begin(), frame.end());
}

std::string SimpleWebSocketConnection::GenerateWebSocketAccept(const std::string& key) {
    std::string combined = key + WS_MAGIC_STRING;
    
    // Calculate SHA-1 hash
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.length(), hash);
    
    // Base64 encode
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    
    BIO_write(bio, hash, SHA_DIGEST_LENGTH);
    BIO_flush(bio);
    
    BUF_MEM* buffer_ptr;
    BIO_get_mem_ptr(bio, &buffer_ptr);
    
    std::string result(buffer_ptr->data, buffer_ptr->length);
    BIO_free_all(bio);
    
    return result;
}

// SimpleWebSocketServer implementation
SimpleWebSocketServer::SimpleWebSocketServer()
    : m_server_socket(-1)
    , m_port(8081)
    , m_running(false)
    , m_cleanup_running(false) {
}

SimpleWebSocketServer::~SimpleWebSocketServer() {
    Stop();
}

bool SimpleWebSocketServer::Start(int port) {
    if (m_running) {
        return true;
    }
    
    m_port = port;
    
    // Create socket
    m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_socket < 0) {
        LOG_ERROR("Failed to create WebSocket server socket");
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR("Failed to set socket options");
        close(m_server_socket);
        return false;
    }
    
    // Bind socket
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(m_server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        LOG_ERROR("Failed to bind WebSocket server to port " + std::to_string(port));
        close(m_server_socket);
        return false;
    }
    
    // Listen for connections
    if (listen(m_server_socket, 10) < 0) {
        LOG_ERROR("Failed to listen on WebSocket server socket");
        close(m_server_socket);
        return false;
    }
    
    m_running = true;
    m_cleanup_running = true;
    
    // Start server thread
    m_server_thread = std::thread(&SimpleWebSocketServer::ServerLoop, this);
    m_cleanup_thread = std::thread([this]() {
        while (m_cleanup_running) {
            CleanupClosedConnections();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });
    
    LOG_INFO("WebSocket server started on port " + std::to_string(port));
    return true;
}

void SimpleWebSocketServer::Stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    m_cleanup_running = false;
    
    // Close server socket
    if (m_server_socket > 0) {
        close(m_server_socket);
        m_server_socket = -1;
    }
    
    // Wait for threads to finish
    if (m_server_thread.joinable()) {
        m_server_thread.join();
    }
    
    if (m_cleanup_thread.joinable()) {
        m_cleanup_thread.join();
    }
    
    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(m_connections_mutex);
        for (auto& [client_id, connection] : m_connections) {
            connection->Close();
        }
        m_connections.clear();
    }
    
    LOG_INFO("WebSocket server stopped");
}

bool SimpleWebSocketServer::IsRunning() const {
    return m_running;
}

void SimpleWebSocketServer::BroadcastMessage(const WSMessage& message) {
    std::lock_guard<std::mutex> lock(m_connections_mutex);
    
    for (auto& [client_id, connection] : m_connections) {
        if (connection->IsOpen()) {
            connection->Send(message);
        }
    }
}

void SimpleWebSocketServer::SendToClient(const std::string& client_id, const WSMessage& message) {
    std::lock_guard<std::mutex> lock(m_connections_mutex);
    
    auto it = m_connections.find(client_id);
    if (it != m_connections.end() && it->second->IsOpen()) {
        it->second->Send(message);
    }
}

std::vector<std::string> SimpleWebSocketServer::GetConnectedClients() const {
    std::lock_guard<std::mutex> lock(m_connections_mutex);
    
    std::vector<std::string> clients;
    for (const auto& [client_id, connection] : m_connections) {
        if (connection->IsOpen()) {
            clients.push_back(client_id);
        }
    }
    
    return clients;
}

size_t SimpleWebSocketServer::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(m_connections_mutex);
    
    size_t count = 0;
    for (const auto& [client_id, connection] : m_connections) {
        if (connection->IsOpen()) {
            count++;
        }
    }
    
    return count;
}

void SimpleWebSocketServer::SetConnectionHandler(ConnectionHandler handler) {
    m_connection_handler = handler;
}

void SimpleWebSocketServer::SetMessageHandler(MessageHandler handler) {
    m_message_handler = handler;
}

void SimpleWebSocketServer::SetDisconnectionHandler(DisconnectionHandler handler) {
    m_disconnection_handler = handler;
}

void SimpleWebSocketServer::ServerLoop() {
    while (m_running) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        
        int client_socket = accept(m_server_socket, (struct sockaddr*)&client_address, &client_len);
        if (client_socket < 0) {
            if (m_running) {
                LOG_WARNING("Failed to accept WebSocket client connection");
            }
            continue;
        }
        
        HandleNewConnection(client_socket);
    }
}

void SimpleWebSocketServer::HandleNewConnection(int client_socket) {
    // Read HTTP request
    char buffer[4096];
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }
    
    buffer[bytes_read] = '\0';
    std::string request(buffer);
    
    // Generate client ID
    std::string client_id = GenerateClientId();
    
    // Create connection
    auto connection = std::make_shared<SimpleWebSocketConnection>(client_socket, client_id);
    
    // Perform WebSocket handshake
    if (!connection->PerformHandshake(request)) {
        LOG_WARNING("WebSocket handshake failed for client " + client_id);
        connection->Close();
        return;
    }
    
    // Add to connections
    {
        std::lock_guard<std::mutex> lock(m_connections_mutex);
        m_connections[client_id] = connection;
    }
    
    LOG_INFO("WebSocket client connected: " + client_id + " from " + connection->GetRemoteAddress());
    
    // Notify connection handler
    if (m_connection_handler) {
        m_connection_handler(connection);
    }
}

void SimpleWebSocketServer::CleanupClosedConnections() {
    std::lock_guard<std::mutex> lock(m_connections_mutex);
    
    auto it = m_connections.begin();
    while (it != m_connections.end()) {
        if (!it->second->IsOpen()) {
            LOG_INFO("Cleaning up closed WebSocket connection: " + it->first);
            
            if (m_disconnection_handler) {
                m_disconnection_handler(it->first);
            }
            
            it = m_connections.erase(it);
        } else {
            ++it;
        }
    }
}

std::string SimpleWebSocketServer::GenerateClientId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "client_";
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

// WSMessageBuilder implementation
WSMessage WSMessageBuilder::CreateWindowEvent(const WindowEvent& event, const WindowInfo& info) {
    WSMessage message;
    message.type = WSMessageType::WINDOW_EVENT;
    message.timestamp = std::chrono::system_clock::now();
    
    std::stringstream ss;
    ss << "{";
    ss << "\"event_type\":\"" << static_cast<int>(event.type) << "\",";
    ss << "\"window_title\":\"" << info.title << "\",";
    ss << "\"process_name\":\"" << info.process_name << "\",";
    ss << "\"timestamp\":\"" << std::chrono::duration_cast<std::chrono::milliseconds>(
        message.timestamp.time_since_epoch()).count() << "\"";
    ss << "}";
    
    message.data = ss.str();
    return message;
}

WSMessage WSMessageBuilder::CreateOCRResult(const OCRDocument& document) {
    WSMessage message;
    message.type = WSMessageType::OCR_RESULT;
    message.timestamp = std::chrono::system_clock::now();
    
    std::stringstream ss;
    ss << "{";
    ss << "\"text\":\"" << document.GetOrderedText() << "\",";
    ss << "\"confidence\":" << document.overall_confidence << ",";
    ss << "\"block_count\":" << document.text_blocks.size() << ",";
    ss << "\"timestamp\":\"" << std::chrono::duration_cast<std::chrono::milliseconds>(
        message.timestamp.time_since_epoch()).count() << "\"";
    ss << "}";
    
    message.data = ss.str();
    return message;
}

WSMessage WSMessageBuilder::CreateAIAnalysis(const ContentAnalysis& analysis) {
    WSMessage message;
    message.type = WSMessageType::AI_ANALYSIS;
    message.timestamp = std::chrono::system_clock::now();
    
    std::stringstream ss;
    ss << "{";
    ss << "\"content_type\":\"" << static_cast<int>(analysis.content_type) << "\",";
    ss << "\"work_category\":\"" << static_cast<int>(analysis.work_category) << "\",";
    ss << "\"is_productive\":" << (analysis.is_productive ? "true" : "false") << ",";
    ss << "\"confidence\":" << analysis.classification_confidence << ",";
    ss << "\"timestamp\":\"" << std::chrono::duration_cast<std::chrono::milliseconds>(
        message.timestamp.time_since_epoch()).count() << "\"";
    ss << "}";
    
    message.data = ss.str();
    return message;
}

WSMessage WSMessageBuilder::CreateProductivityUpdate(int score) {
    WSMessage message;
    message.type = WSMessageType::PRODUCTIVITY_UPDATE;
    message.timestamp = std::chrono::system_clock::now();
    
    std::stringstream ss;
    ss << "{";
    ss << "\"productivity_score\":" << score << ",";
    ss << "\"timestamp\":\"" << std::chrono::duration_cast<std::chrono::milliseconds>(
        message.timestamp.time_since_epoch()).count() << "\"";
    ss << "}";
    
    message.data = ss.str();
    return message;
}

WSMessage WSMessageBuilder::CreateWelcomeMessage() {
    WSMessage message;
    message.type = WSMessageType::SYSTEM_STATUS;
    message.timestamp = std::chrono::system_clock::now();
    
    message.data = R"({"type":"welcome","message":"Connected to Work Assistant"})";
    return message;
}

// WebSocketClientManager implementation
WebSocketClientManager::WebSocketClientManager(std::shared_ptr<IWebSocketServer> server)
    : m_server(server) {
    
    if (m_server) {
        m_server->SetConnectionHandler([this](auto conn) { OnClientConnected(conn); });
        m_server->SetMessageHandler([this](auto conn, auto msg) { OnClientMessage(conn, msg); });
        m_server->SetDisconnectionHandler([this](auto id) { OnClientDisconnected(id); });
    }
}

WebSocketClientManager::~WebSocketClientManager() = default;

void WebSocketClientManager::OnClientConnected(std::shared_ptr<IWebSocketConnection> connection) {
    LOG_INFO("WebSocket client connected: " + connection->GetClientId());
    
    // Send welcome message
    WSMessage welcome = WSMessageBuilder::CreateWelcomeMessage();
    connection->Send(welcome);
}

void WebSocketClientManager::BroadcastWindowEvent(const WindowEvent& event, const WindowInfo& info) {
    if (!m_server) return;
    
    WSMessage message = WSMessageBuilder::CreateWindowEvent(event, info);
    
    std::lock_guard<std::mutex> lock(m_subscriptions_mutex);
    for (const auto& [client_id, subscriptions] : m_client_subscriptions) {
        if (IsClientSubscribed(client_id, "window_events")) {
            m_server->SendToClient(client_id, message);
        }
    }
}

bool WebSocketClientManager::IsClientSubscribed(const std::string& client_id, const std::string& event_type) const {
    auto it = m_client_subscriptions.find(client_id);
    if (it == m_client_subscriptions.end()) {
        return false;
    }
    
    const auto& subscriptions = it->second;
    return std::find(subscriptions.begin(), subscriptions.end(), event_type) != subscriptions.end();
}

void WebSocketClientManager::OnClientMessage(std::shared_ptr<IWebSocketConnection> connection, const WSMessage& message) {
    // Handle subscription messages
    if (message.type == WSMessageType::SUBSCRIPTION) {
        // Parse subscription data (simplified)
        // In a real implementation, you would properly parse JSON
        std::vector<std::string> subscriptions = {"window_events", "ocr_results", "ai_analysis"};
        HandleClientSubscription(connection->GetClientId(), subscriptions);
    }
}

void WebSocketClientManager::OnClientDisconnected(const std::string& client_id) {
    UnsubscribeClient(client_id);
    LOG_INFO("WebSocket client disconnected: " + client_id);
}

void WebSocketClientManager::HandleClientSubscription(const std::string& client_id, const std::vector<std::string>& subscriptions) {
    std::lock_guard<std::mutex> lock(m_subscriptions_mutex);
    m_client_subscriptions[client_id] = subscriptions;
    
    LOG_INFO("Client " + client_id + " subscribed to " + std::to_string(subscriptions.size()) + " event types");
}

void WebSocketClientManager::UnsubscribeClient(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(m_subscriptions_mutex);
    m_client_subscriptions.erase(client_id);
}

} // namespace work_assistant