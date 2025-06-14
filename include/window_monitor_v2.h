#pragma once

#include "common_types.h"
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <cstdint>

namespace work_assistant {

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