#pragma once

#include <functional>
#include <vector>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <memory>

namespace work_assistant {

// Forward declarations
struct WindowEvent;

class EventManager {
public:
    static EventManager& GetInstance();
    
    // Subscribe to events
    template<typename EventType>
    void Subscribe(std::function<void(const EventType&)> callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto typeIndex = std::type_index(typeid(EventType));
        
        auto wrapper = [callback](const void* event) {
            callback(*static_cast<const EventType*>(event));
        };
        
        m_callbacks[typeIndex].push_back(wrapper);
    }
    
    // Emit events
    template<typename EventType>
    void EmitEvent(const EventType& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto typeIndex = std::type_index(typeid(EventType));
        
        auto it = m_callbacks.find(typeIndex);
        if (it != m_callbacks.end()) {
            for (const auto& callback : it->second) {
                callback(&event);
            }
        }
    }

private:
    EventManager() = default;
    ~EventManager() = default;
    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;

    std::mutex m_mutex;
    std::unordered_map<std::type_index, std::vector<std::function<void(const void*)>>> m_callbacks;
};

} // namespace work_assistant