#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace WorkAssistant {

struct WindowInfo {
    uintptr_t handle;
    std::string title;
    std::string className;
    std::string processName;
    int x, y, width, height;
    bool isVisible;
    bool isForeground;
};

enum class WindowEvent {
    WINDOW_CREATED,
    WINDOW_DESTROYED,
    WINDOW_FOCUS_CHANGED,
    WINDOW_MOVED,
    WINDOW_RESIZED
};

using WindowEventCallback = std::function<void(WindowEvent, const WindowInfo&)>;

class IWindowMonitor {
public:
    virtual ~IWindowMonitor() = default;
    
    // Initialize the window monitor
    virtual bool Initialize() = 0;
    
    // Start monitoring window events
    virtual void StartMonitoring() = 0;
    
    // Stop monitoring window events
    virtual void StopMonitoring() = 0;
    
    // Get all visible windows
    virtual std::vector<WindowInfo> GetWindows() = 0;
    
    // Get the currently focused window
    virtual WindowInfo GetForegroundWindow() = 0;
    
    // Set callback for window events
    virtual void SetEventCallback(WindowEventCallback callback) = 0;
    
    // Check if monitoring is active
    virtual bool IsMonitoring() const = 0;
};

class WindowMonitorFactory {
public:
    static std::unique_ptr<IWindowMonitor> Create();
};

} // namespace WorkAssistant