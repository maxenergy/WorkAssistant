#include "web_server.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <unordered_set>
#include <queue>
#include <iomanip>

#ifdef WEB_ENABLED
#include <drogon/WebSocketController.h>
#include <drogon/PubSubService.h>
using namespace drogon;
#endif

namespace work_assistant {

#ifdef WEB_ENABLED

// WebSocket controller for real-time updates
class WSController : public drogon::WebSocketController<WSController> {
public:
    void handleNewMessage(const WebSocketConnectionPtr& wsConnPtr,
                         std::string&& message,
                         const WebSocketMessageType& type) override {
        
        // Handle incoming WebSocket messages
        if (type == WebSocketMessageType::Text) {
            try {
                ProcessClientMessage(wsConnPtr, message);
            } catch (const std::exception& e) {
                std::cerr << "WebSocket message processing error: " << e.what() << std::endl;
                SendErrorMessage(wsConnPtr, "Message processing failed");
            }
        }
    }
    
    void handleNewConnection(const HttpRequestPtr& req,
                           const WebSocketConnectionPtr& wsConnPtr) override {
        
        // Generate unique client ID
        std::string client_id = GenerateClientId(wsConnPtr);
        
        {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            m_clients[client_id] = wsConnPtr;
        }
        
        std::cout << "WebSocket client connected: " << client_id 
                  << " (Total: " << GetClientCount() << ")" << std::endl;
        
        // Send welcome message
        SendWelcomeMessage(wsConnPtr, client_id);
        
        // Subscribe to real-time updates
        SubscribeToUpdates(client_id);
    }
    
    void handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr) override {
        std::string client_id = FindClientId(wsConnPtr);
        
        if (!client_id.empty()) {
            {
                std::lock_guard<std::mutex> lock(m_clients_mutex);
                m_clients.erase(client_id);
            }
            
            UnsubscribeFromUpdates(client_id);
            
            std::cout << "WebSocket client disconnected: " << client_id 
                      << " (Total: " << GetClientCount() << ")" << std::endl;
        }
    }
    
    // Public methods for broadcasting messages
    static void BroadcastToAll(const WSMessage& message) {
        auto instance = GetInstance();
        if (instance) {
            instance->DoBroadcast(message);
        }
    }
    
    static void SendToClient(const std::string& client_id, const WSMessage& message) {
        auto instance = GetInstance();
        if (instance) {
            instance->DoSendToClient(client_id, message);
        }
    }
    
    static size_t GetConnectedCount() {
        auto instance = GetInstance();
        return instance ? instance->GetClientCount() : 0;
    }
    
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/api/v1/ws", Get);
    WS_PATH_LIST_END
    
private:
    static WSController* GetInstance() {
        static WSController instance;
        return &instance;
    }
    
    std::string GenerateClientId(const WebSocketConnectionPtr& wsConnPtr) {
        static std::atomic<int> counter{0};
        auto peer_addr = wsConnPtr->peerAddr().toIp();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        std::ostringstream oss;
        oss << "client_" << peer_addr << "_" << timestamp << "_" << counter++;
        return oss.str();
    }
    
    std::string FindClientId(const WebSocketConnectionPtr& wsConnPtr) {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        for (const auto& [id, conn] : m_clients) {
            if (conn.lock() == wsConnPtr) {
                return id;
            }
        }
        return "";
    }
    
    size_t GetClientCount() const {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        return m_clients.size();
    }
    
    void ProcessClientMessage(const WebSocketConnectionPtr& wsConnPtr, const std::string& message) {
        // Parse client message (simple JSON parsing)
        if (message.find("\"type\":\"ping\"") != std::string::npos) {
            // Respond to ping with pong
            WSMessage pong_msg;
            pong_msg.type = WSMessageType::SYSTEM_STATUS;
            pong_msg.timestamp = std::chrono::system_clock::now();
            pong_msg.data = "{\"type\":\"pong\",\"timestamp\":\"" + 
                           storage_utils::FormatTimestamp(pong_msg.timestamp) + "\"}";
            
            wsConnPtr->send(pong_msg.ToJson());
        }
        else if (message.find("\"type\":\"subscribe\"") != std::string::npos) {
            // Handle subscription requests
            std::string client_id = FindClientId(wsConnPtr);
            if (!client_id.empty()) {
                SendSubscriptionConfirmation(wsConnPtr, client_id);
            }
        }
        else {
            std::cout << "Received unknown message: " << message << std::endl;
        }
    }
    
    void SendWelcomeMessage(const WebSocketConnectionPtr& wsConnPtr, const std::string& client_id) {
        WSMessage welcome;
        welcome.type = WSMessageType::SYSTEM_STATUS;
        welcome.timestamp = std::chrono::system_clock::now();
        
        std::ostringstream data;
        data << "{";
        data << "\"type\":\"welcome\",";
        data << "\"client_id\":\"" << client_id << "\",";
        data << "\"server_version\":\"1.0.0\",";
        data << "\"features\":[\"real_time_updates\",\"productivity_tracking\",\"ai_analysis\"],";
        data << "\"timestamp\":\"" << storage_utils::FormatTimestamp(welcome.timestamp) << "\"";
        data << "}";
        
        welcome.data = data.str();
        wsConnPtr->send(welcome.ToJson());
    }
    
    void SendSubscriptionConfirmation(const WebSocketConnectionPtr& wsConnPtr, const std::string& client_id) {
        WSMessage confirm;
        confirm.type = WSMessageType::SYSTEM_STATUS;
        confirm.timestamp = std::chrono::system_clock::now();
        
        std::ostringstream data;
        data << "{";
        data << "\"type\":\"subscription_confirmed\",";
        data << "\"client_id\":\"" << client_id << "\",";
        data << "\"subscriptions\":[\"window_events\",\"ocr_results\",\"ai_analysis\",\"productivity_updates\"],";
        data << "\"timestamp\":\"" << storage_utils::FormatTimestamp(confirm.timestamp) << "\"";
        data << "}";
        
        confirm.data = data.str();
        wsConnPtr->send(confirm.ToJson());
    }
    
    void SendErrorMessage(const WebSocketConnectionPtr& wsConnPtr, const std::string& error) {
        WSMessage error_msg;
        error_msg.type = WSMessageType::SYSTEM_STATUS;
        error_msg.timestamp = std::chrono::system_clock::now();
        error_msg.data = "{\"type\":\"error\",\"message\":\"" + 
                        web_utils::EscapeJsonString(error) + "\"}";
        
        wsConnPtr->send(error_msg.ToJson());
    }
    
    void SubscribeToUpdates(const std::string& client_id) {
        // In a real implementation, set up subscriptions to application events
        std::cout << "Client " << client_id << " subscribed to real-time updates" << std::endl;
    }
    
    void UnsubscribeFromUpdates(const std::string& client_id) {
        std::cout << "Client " << client_id << " unsubscribed from updates" << std::endl;
    }
    
    void DoBroadcast(const WSMessage& message) {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        std::string json_message = message.ToJson();
        
        // Clean up expired connections
        auto it = m_clients.begin();
        while (it != m_clients.end()) {
            auto connection = it->second.lock();
            if (!connection || connection->disconnected()) {
                it = m_clients.erase(it);
            } else {
                try {
                    connection->send(json_message);
                } catch (const std::exception& e) {
                    std::cerr << "Failed to send to client " << it->first << ": " << e.what() << std::endl;
                }
                ++it;
            }
        }
    }
    
    void DoSendToClient(const std::string& client_id, const WSMessage& message) {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        auto it = m_clients.find(client_id);
        
        if (it != m_clients.end()) {
            auto connection = it->second.lock();
            if (connection && !connection->disconnected()) {
                try {
                    connection->send(message.ToJson());
                } catch (const std::exception& e) {
                    std::cerr << "Failed to send to client " << client_id << ": " << e.what() << std::endl;
                }
            } else {
                // Remove expired connection
                m_clients.erase(it);
            }
        }
    }
    
private:
    mutable std::mutex m_clients_mutex;
    std::unordered_map<std::string, std::weak_ptr<WebSocketConnection>> m_clients;
};

#endif // WEB_ENABLED

// WebSocket message queue for buffering messages when no clients are connected
class MessageQueue {
public:
    static MessageQueue& GetInstance() {
        static MessageQueue instance;
        return instance;
    }
    
    void QueueMessage(const WSMessage& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_message_queue.push(message);
        
        // Limit queue size to prevent memory growth
        while (m_message_queue.size() > MAX_QUEUE_SIZE) {
            m_message_queue.pop();
        }
    }
    
    std::vector<WSMessage> GetQueuedMessages() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<WSMessage> messages;
        while (!m_message_queue.empty()) {
            messages.push_back(m_message_queue.front());
            m_message_queue.pop();
        }
        
        return messages;
    }
    
    size_t GetQueueSize() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_message_queue.size();
    }
    
private:
    static constexpr size_t MAX_QUEUE_SIZE = 1000;
    
    mutable std::mutex m_mutex;
    std::queue<WSMessage> m_message_queue;
};

// WebSocket utility functions
namespace websocket_utils {

void BroadcastMessage(const WSMessage& message) {
#ifdef WEB_ENABLED
    // Try to broadcast immediately
    WSController::BroadcastToAll(message);
#endif
    
    // Also queue the message for any clients that connect later
    MessageQueue::GetInstance().QueueMessage(message);
}

void SendToClient(const std::string& client_id, const WSMessage& message) {
#ifdef WEB_ENABLED
    WSController::SendToClient(client_id, message);
#endif
}

size_t GetConnectedClientCount() {
#ifdef WEB_ENABLED
    return WSController::GetConnectedCount();
#else
    return 0;
#endif
}

std::vector<WSMessage> GetQueuedMessages() {
    return MessageQueue::GetInstance().GetQueuedMessages();
}

WSMessage CreateWindowEventMessage(const WindowEvent& event, const WindowInfo& info) {
    WSMessage message;
    message.type = WSMessageType::WINDOW_EVENT;
    message.timestamp = event.timestamp;
    
    std::ostringstream data;
    data << "{";
    data << "\"event_type\":\"" << static_cast<int>(event.type) << "\",";
    data << "\"window_title\":\"" << web_utils::EscapeJsonString(info.title) << "\",";
    data << "\"process_name\":\"" << web_utils::EscapeJsonString(info.process_name) << "\",";
    data << "\"process_id\":" << info.process_id << ",";
    data << "\"window_handle\":\"" << std::hex << info.window_handle << std::dec << "\",";
    data << "\"position\":{\"x\":" << info.x << ",\"y\":" << info.y << "},";
    data << "\"size\":{\"width\":" << info.width << ",\"height\":" << info.height << "},";
    data << "\"timestamp\":\"" << storage_utils::FormatTimestamp(event.timestamp) << "\"";
    data << "}";
    
    message.data = data.str();
    return message;
}

WSMessage CreateOCRResultMessage(const OCRDocument& document) {
    WSMessage message;
    message.type = WSMessageType::OCR_RESULT;
    message.timestamp = std::chrono::system_clock::now();
    
    std::ostringstream data;
    data << "{";
    data << "\"text\":\"" << web_utils::EscapeJsonString(document.GetOrderedText()) << "\",";
    data << "\"confidence\":" << document.overall_confidence << ",";
    data << "\"text_blocks\":" << document.text_blocks.size() << ",";
    data << "\"processing_time_ms\":" << document.processing_time.count() << ",";
    data << "\"timestamp\":\"" << storage_utils::FormatTimestamp(message.timestamp) << "\"";
    data << "}";
    
    message.data = data.str();
    return message;
}

WSMessage CreateAIAnalysisMessage(const ContentAnalysis& analysis) {
    WSMessage message;
    message.type = WSMessageType::AI_ANALYSIS;
    message.timestamp = analysis.timestamp;
    
    std::ostringstream data;
    data << "{";
    data << "\"content_type\":" << static_cast<int>(analysis.content_type) << ",";
    data << "\"work_category\":" << static_cast<int>(analysis.work_category) << ",";
    data << "\"is_productive\":" << (analysis.is_productive ? "true" : "false") << ",";
    data << "\"is_focused_work\":" << (analysis.is_focused_work ? "true" : "false") << ",";
    data << "\"confidence\":" << analysis.classification_confidence << ",";
    data << "\"distraction_level\":" << analysis.distraction_level << ",";
    data << "\"priority\":" << static_cast<int>(analysis.priority) << ",";
    data << "\"application\":\"" << web_utils::EscapeJsonString(analysis.application) << "\",";
    data << "\"title\":\"" << web_utils::EscapeJsonString(analysis.title) << "\",";
    data << "\"processing_time_ms\":" << analysis.processing_time.count() << ",";
    data << "\"timestamp\":\"" << storage_utils::FormatTimestamp(analysis.timestamp) << "\"";
    data << "}";
    
    message.data = data.str();
    return message;
}

WSMessage CreateProductivityUpdateMessage(float score, const std::string& level) {
    WSMessage message;
    message.type = WSMessageType::PRODUCTIVITY_UPDATE;
    message.timestamp = std::chrono::system_clock::now();
    
    std::ostringstream data;
    data << "{";
    data << "\"productivity_score\":" << std::fixed << std::setprecision(1) << score << ",";
    data << "\"level\":\"" << level << "\",";
    data << "\"timestamp\":\"" << storage_utils::FormatTimestamp(message.timestamp) << "\"";
    data << "}";
    
    message.data = data.str();
    return message;
}

WSMessage CreateSystemStatusMessage(const std::string& status, const std::string& details) {
    WSMessage message;
    message.type = WSMessageType::SYSTEM_STATUS;
    message.timestamp = std::chrono::system_clock::now();
    
    std::ostringstream data;
    data << "{";
    data << "\"status\":\"" << status << "\",";
    data << "\"details\":\"" << web_utils::EscapeJsonString(details) << "\",";
    data << "\"connected_clients\":" << GetConnectedClientCount() << ",";
    data << "\"queued_messages\":" << MessageQueue::GetInstance().GetQueueSize() << ",";
    data << "\"timestamp\":\"" << storage_utils::FormatTimestamp(message.timestamp) << "\"";
    data << "}";
    
    message.data = data.str();
    return message;
}

} // namespace websocket_utils

} // namespace work_assistant