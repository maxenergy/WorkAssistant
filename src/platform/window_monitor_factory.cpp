#include "window_monitor_v2.h"

#ifdef _WIN32
#include "windows/win32_window_monitor.h"
#elif defined(__linux__)
#include "linux/x11_window_monitor.h"
#endif

namespace work_assistant {

std::unique_ptr<IWindowMonitor> WindowMonitorFactory::Create() {
#ifdef _WIN32
    return std::make_unique<Win32WindowMonitor>();
#elif defined(__linux__)
    return std::make_unique<X11WindowMonitor>();
#else
    return nullptr;
#endif
}

} // namespace work_assistant