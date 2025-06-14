#pragma once

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <cstdint>

namespace work_assistant {

// Window information structure
struct WindowInfo {
    uint64_t window_handle;
    std::string title;
    std::string class_name;
    std::string process_name;
    uint32_t process_id;
    int x, y;
    int width, height;
    bool is_visible;
    std::chrono::system_clock::time_point timestamp;
    
    WindowInfo() 
        : window_handle(0), process_id(0)
        , x(0), y(0), width(0), height(0)
        , is_visible(false) {}
};

// Window event types
enum class WindowEventType {
    WINDOW_CREATED,
    WINDOW_DESTROYED,
    WINDOW_FOCUSED,
    WINDOW_MINIMIZED,
    WINDOW_RESTORED,
    WINDOW_MOVED,
    WINDOW_RESIZED
};

// Window event structure
struct WindowEvent {
    WindowEventType type;
    WindowInfo window_info;
    
    WindowEvent(WindowEventType t, const WindowInfo& info) 
        : type(t), window_info(info) {}
};

// Window monitor interface
class IWindowMonitor {
public:
    virtual ~IWindowMonitor() = default;

    // Initialize the window monitor
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Start/stop monitoring window events
    virtual bool StartMonitoring() = 0;
    virtual void StopMonitoring() = 0;
    virtual bool IsMonitoring() const = 0;

    // Get current active window
    virtual WindowInfo GetActiveWindow() const = 0;
    
    // Get all visible windows
    virtual std::vector<WindowInfo> GetAllWindows() const = 0;
};

// Window monitor factory
class WindowMonitorFactory {
public:
    static std::unique_ptr<IWindowMonitor> Create();
};

} // namespace work_assistant