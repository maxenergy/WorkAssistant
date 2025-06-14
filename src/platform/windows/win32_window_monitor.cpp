#include "win32_window_monitor.h"
#include "event_manager.h"
#include <iostream>
#include <psapi.h>
#include <vector>

namespace work_assistant {

// Static instance pointer
Win32WindowMonitor* Win32WindowMonitor::s_instance = nullptr;

Win32WindowMonitor::Win32WindowMonitor()
    : m_initialized(false)
    , m_monitoring(false)
    , m_foregroundHook(nullptr)
    , m_createHook(nullptr)
    , m_destroyHook(nullptr)
    , m_minimizeHook(nullptr)
    , m_restoreHook(nullptr)
{
    s_instance = this;
}

Win32WindowMonitor::~Win32WindowMonitor() {
    Shutdown();
    s_instance = nullptr;
}

bool Win32WindowMonitor::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Initialize COM for some Windows APIs
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "Failed to initialize COM" << std::endl;
        return false;
    }

    m_initialized = true;
    std::cout << "Win32 Window Monitor initialized" << std::endl;
    return true;
}

void Win32WindowMonitor::Shutdown() {
    if (!m_initialized) {
        return;
    }

    StopMonitoring();
    CoUninitialize();
    m_initialized = false;
    std::cout << "Win32 Window Monitor shut down" << std::endl;
}

bool Win32WindowMonitor::StartMonitoring() {
    if (!m_initialized) {
        std::cerr << "Monitor not initialized" << std::endl;
        return false;
    }

    if (m_monitoring) {
        return true;
    }

    // Set up window event hooks
    m_foregroundHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    m_createHook = SetWinEventHook(
        EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    m_destroyHook = SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    m_minimizeHook = SetWinEventHook(
        EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZESTART,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    m_restoreHook = SetWinEventHook(
        EVENT_SYSTEM_MINIMIZEEND, EVENT_SYSTEM_MINIMIZEEND,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    if (!m_foregroundHook || !m_createHook || !m_destroyHook || 
        !m_minimizeHook || !m_restoreHook) {
        std::cerr << "Failed to set window event hooks" << std::endl;
        StopMonitoring();
        return false;
    }

    m_monitoring = true;
    std::cout << "Started monitoring Windows events" << std::endl;
    return true;
}

void Win32WindowMonitor::StopMonitoring() {
    if (!m_monitoring) {
        return;
    }

    // Unhook all event hooks
    if (m_foregroundHook) {
        UnhookWinEvent(m_foregroundHook);
        m_foregroundHook = nullptr;
    }
    if (m_createHook) {
        UnhookWinEvent(m_createHook);
        m_createHook = nullptr;
    }
    if (m_destroyHook) {
        UnhookWinEvent(m_destroyHook);
        m_destroyHook = nullptr;
    }
    if (m_minimizeHook) {
        UnhookWinEvent(m_minimizeHook);
        m_minimizeHook = nullptr;
    }
    if (m_restoreHook) {
        UnhookWinEvent(m_restoreHook);
        m_restoreHook = nullptr;
    }

    m_monitoring = false;
    std::cout << "Stopped monitoring Windows events" << std::endl;
}

bool Win32WindowMonitor::IsMonitoring() const {
    return m_monitoring;
}

WindowInfo Win32WindowMonitor::GetActiveWindow() const {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        return WindowInfo();
    }
    return CreateWindowInfo(hwnd);
}

std::vector<WindowInfo> Win32WindowMonitor::GetAllWindows() const {
    std::vector<WindowInfo> windows;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

void CALLBACK Win32WindowMonitor::WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD dwEventThread,
    DWORD dwmsEventTime) {
    
    // Only handle window events
    if (idObject != OBJID_WINDOW || !hwnd) {
        return;
    }

    if (s_instance) {
        s_instance->HandleWinEvent(event, hwnd);
    }
}

void Win32WindowMonitor::HandleWinEvent(DWORD event, HWND hwnd) {
    WindowInfo info = CreateWindowInfo(hwnd);
    
    switch (event) {
        case EVENT_SYSTEM_FOREGROUND:
            EventManager::GetInstance().EmitEvent(
                WindowEvent{WindowEventType::WINDOW_FOCUSED, info}
            );
            break;
            
        case EVENT_OBJECT_CREATE:
            EventManager::GetInstance().EmitEvent(
                WindowEvent{WindowEventType::WINDOW_CREATED, info}
            );
            break;
            
        case EVENT_OBJECT_DESTROY:
            EventManager::GetInstance().EmitEvent(
                WindowEvent{WindowEventType::WINDOW_DESTROYED, info}
            );
            break;
            
        case EVENT_SYSTEM_MINIMIZESTART:
            EventManager::GetInstance().EmitEvent(
                WindowEvent{WindowEventType::WINDOW_MINIMIZED, info}
            );
            break;
            
        case EVENT_SYSTEM_MINIMIZEEND:
            EventManager::GetInstance().EmitEvent(
                WindowEvent{WindowEventType::WINDOW_RESTORED, info}
            );
            break;
    }
}

WindowInfo Win32WindowMonitor::CreateWindowInfo(HWND hwnd) const {
    WindowInfo info;
    info.window_handle = reinterpret_cast<uint64_t>(hwnd);
    info.title = GetWindowText(hwnd);
    info.process_name = GetProcessName(hwnd);
    info.process_id = GetProcessId(hwnd);
    info.timestamp = std::chrono::system_clock::now();

    // Get window rectangle
    RECT rect;
    if (GetWindowRect(hwnd, &rect)) {
        info.x = rect.left;
        info.y = rect.top;
        info.width = rect.right - rect.left;
        info.height = rect.bottom - rect.top;
    }

    // Check if window is visible
    info.is_visible = IsWindowVisible(hwnd) != FALSE;

    return info;
}

std::string Win32WindowMonitor::GetWindowText(HWND hwnd) const {
    int length = GetWindowTextLength(hwnd);
    if (length == 0) {
        return "";
    }

    std::vector<char> buffer(length + 1);
    GetWindowTextA(hwnd, buffer.data(), buffer.size());
    return std::string(buffer.data());
}

std::string Win32WindowMonitor::GetProcessName(HWND hwnd) const {
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess) {
        return "";
    }

    char processName[MAX_PATH];
    DWORD size = sizeof(processName);
    if (QueryFullProcessImageNameA(hProcess, 0, processName, &size)) {
        CloseHandle(hProcess);
        
        // Extract just the executable name
        std::string fullPath(processName);
        size_t lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return fullPath.substr(lastSlash + 1);
        }
        return fullPath;
    }

    CloseHandle(hProcess);
    return "";
}

DWORD Win32WindowMonitor::GetProcessId(HWND hwnd) const {
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    return processId;
}

BOOL CALLBACK Win32WindowMonitor::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    // Only include visible windows with titles
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    int length = GetWindowTextLength(hwnd);
    if (length == 0) {
        return TRUE;
    }

    auto* windows = reinterpret_cast<std::vector<WindowInfo>*>(lParam);
    if (s_instance) {
        windows->push_back(s_instance->CreateWindowInfo(hwnd));
    }

    return TRUE;
}

} // namespace work_assistant