#pragma once

#include "window_monitor_v2.h"
#include <X11/Xlib.h>
#include <memory>
#include <thread>
#include <atomic>

namespace work_assistant {

class X11WindowMonitor : public IWindowMonitor {
public:
    X11WindowMonitor();
    ~X11WindowMonitor() override;

    bool Initialize() override;
    void Shutdown() override;
    bool StartMonitoring() override;
    void StopMonitoring() override;
    bool IsMonitoring() const override;
    WindowInfo GetActiveWindow() const override;
    std::vector<WindowInfo> GetAllWindows() const override;

private:
    Display* m_display;
    Window m_root;
    std::unique_ptr<std::thread> m_monitorThread;
    std::atomic<bool> m_monitoring;
    std::atomic<bool> m_initialized;
    
    void MonitorLoop();
    WindowInfo GetWindowInfo(Window window) const;
    std::string GetWindowTitle(Window window) const;
    std::string GetWindowClassName(Window window) const;
    std::string GetProcessName(Window window) const;
    uint32_t GetProcessId(Window window) const;
    bool IsWindowVisible(Window window) const;
};

} // namespace work_assistant