#include "x11_window_monitor.h"
#include "event_manager.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <iostream>
#include <cstring>
#include <unistd.h>

namespace work_assistant {

X11WindowMonitor::X11WindowMonitor()
    : m_display(nullptr), m_root(0), m_monitoring(false), m_initialized(false) {
}

X11WindowMonitor::~X11WindowMonitor() {
    StopMonitoring();
    Shutdown();
}

bool X11WindowMonitor::Initialize() {
    if (m_initialized) {
        return true;
    }

    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        std::cerr << "Failed to open X11 display" << std::endl;
        return false;
    }

    m_root = XDefaultRootWindow(m_display);
    m_initialized = true;
    std::cout << "X11 Window Monitor initialized" << std::endl;
    return true;
}

void X11WindowMonitor::Shutdown() {
    if (!m_initialized) {
        return;
    }

    StopMonitoring();

    if (m_display) {
        XCloseDisplay(m_display);
        m_display = nullptr;
    }

    m_initialized = false;
    std::cout << "X11 Window Monitor shut down" << std::endl;
}

bool X11WindowMonitor::StartMonitoring() {
    if (m_monitoring || !m_display) {
        return false;
    }

    m_monitoring = true;
    m_monitorThread = std::make_unique<std::thread>(&X11WindowMonitor::MonitorLoop, this);
    return true;
}

void X11WindowMonitor::StopMonitoring() {
    if (!m_monitoring) {
        return;
    }

    m_monitoring = false;
    if (m_monitorThread && m_monitorThread->joinable()) {
        m_monitorThread->join();
    }
    m_monitorThread.reset();
}

std::vector<WindowInfo> X11WindowMonitor::GetAllWindows() const {
    std::vector<WindowInfo> windows;

    if (!m_display) {
        return windows;
    }

    Window root, parent;
    Window* children;
    unsigned int nchildren;

    if (XQueryTree(m_display, m_root, &root, &parent, &children, &nchildren) == 0) {
        return windows;
    }

    for (unsigned int i = 0; i < nchildren; ++i) {
        if (IsWindowVisible(children[i])) {
            WindowInfo info = GetWindowInfo(children[i]);
            if (!info.title.empty()) {
                windows.push_back(info);
            }
        }
    }

    if (children) {
        XFree(children);
    }

    return windows;
}

WindowInfo X11WindowMonitor::GetActiveWindow() const {
    WindowInfo info = {};

    if (!m_display) {
        return info;
    }

    Window focused_window;
    int revert_to;
    XGetInputFocus(m_display, &focused_window, &revert_to);

    if (focused_window != None && focused_window != m_root) {
        info = GetWindowInfo(focused_window);
    }

    return info;
}



bool X11WindowMonitor::IsMonitoring() const {
    return m_monitoring;
}

void X11WindowMonitor::MonitorLoop() {
    XSelectInput(m_display, m_root,
                 SubstructureNotifyMask | PropertyChangeMask);

    Window last_focused = None;

    while (m_monitoring) {
        // Check for focus changes
        Window current_focused;
        int revert_to;
        XGetInputFocus(m_display, &current_focused, &revert_to);

        if (current_focused != last_focused) {
            // Focus changed - could emit event here if needed
            last_focused = current_focused;
        }

        // Process X11 events
        while (XPending(m_display) > 0) {
            XEvent event;
            XNextEvent(m_display, &event);

            // Process X11 events (simplified for now)
            switch (event.type) {
                case CreateNotify:
                case DestroyNotify:
                case ConfigureNotify:
                    // Window events detected - could emit events here if needed
                    break;
            }
        }

        usleep(50000); // 50ms sleep
    }
}

WindowInfo X11WindowMonitor::GetWindowInfo(Window window) const {
    WindowInfo info = {};
    info.window_handle = reinterpret_cast<WindowHandle>(window);
    info.title = GetWindowTitle(window);
    info.class_name = GetWindowClassName(window);
    info.process_name = GetProcessName(window);
    info.process_id = GetProcessId(window);
    info.is_visible = IsWindowVisible(window);
    info.timestamp = std::chrono::system_clock::now();

    // Get window geometry
    XWindowAttributes attrs;
    if (XGetWindowAttributes(m_display, window, &attrs) == 0) {
        info.x = attrs.x;
        info.y = attrs.y;
        info.width = attrs.width;
        info.height = attrs.height;
    }

    return info;
}

std::string X11WindowMonitor::GetWindowTitle(Window window) const {
    char* title = nullptr;
    if (XFetchName(m_display, window, &title) && title) {
        std::string result(title);
        XFree(title);
        return result;
    }
    return "";
}

std::string X11WindowMonitor::GetWindowClassName(Window window) const {
    XClassHint class_hint;
    if (XGetClassHint(m_display, window, &class_hint)) {
        std::string result;
        if (class_hint.res_class) {
            result = class_hint.res_class;
            XFree(class_hint.res_class);
        }
        if (class_hint.res_name) {
            XFree(class_hint.res_name);
        }
        return result;
    }
    return "";
}

std::string X11WindowMonitor::GetProcessName(Window window) const {
    Atom pid_atom = XInternAtom(m_display, "_NET_WM_PID", True);
    if (pid_atom == None) {
        return "";
    }

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = nullptr;

    if (XGetWindowProperty(m_display, window, pid_atom, 0, 1, False,
                          XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        pid_t pid = *reinterpret_cast<pid_t*>(prop);
        XFree(prop);

        // Read process name from /proc/pid/comm
        char comm_path[64];
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);

        FILE* comm_file = fopen(comm_path, "r");
        if (comm_file) {
            char comm[256];
            if (fgets(comm, sizeof(comm), comm_file)) {
                // Remove newline
                size_t len = strlen(comm);
                if (len > 0 && comm[len-1] == '\n') {
                    comm[len-1] = '\0';
                }
                fclose(comm_file);
                return std::string(comm);
            }
            fclose(comm_file);
        }
    }

    return "";
}

uint32_t X11WindowMonitor::GetProcessId(Window window) const {
    Atom pid_atom = XInternAtom(m_display, "_NET_WM_PID", True);
    if (pid_atom == None) {
        return 0;
    }

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = nullptr;

    if (XGetWindowProperty(m_display, window, pid_atom, 0, 1, False,
                          XA_CARDINAL, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) == Success && prop) {
        uint32_t pid = *reinterpret_cast<uint32_t*>(prop);
        XFree(prop);
        return pid;
    }

    return 0;
}

bool X11WindowMonitor::IsWindowVisible(Window window) const {
    XWindowAttributes attrs;
    if (XGetWindowAttributes(m_display, window, &attrs) == 0) {
        return false;
    }

    return attrs.map_state == IsViewable &&
           attrs.c_class == InputOutput &&
           attrs.width > 1 && attrs.height > 1;
}

} // namespace work_assistant