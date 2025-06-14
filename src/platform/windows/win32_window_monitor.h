#pragma once

#include "window_monitor.h"
#include <windows.h>
#include <memory>
#include <unordered_map>

namespace work_assistant {

class Win32WindowMonitor : public IWindowMonitor {
public:
    Win32WindowMonitor();
    ~Win32WindowMonitor() override;

    bool Initialize() override;
    void Shutdown() override;
    bool StartMonitoring() override;
    void StopMonitoring() override;
    bool IsMonitoring() const override;

    // Get current active window
    WindowInfo GetActiveWindow() const override;
    
    // Get all visible windows
    std::vector<WindowInfo> GetAllWindows() const override;

private:
    // Static callback functions for Win32 API
    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hWinEventHook,
        DWORD event,
        HWND hwnd,
        LONG idObject,
        LONG idChild,
        DWORD dwEventThread,
        DWORD dwmsEventTime
    );

    // Instance method to handle events
    void HandleWinEvent(DWORD event, HWND hwnd);

    // Helper methods
    WindowInfo CreateWindowInfo(HWND hwnd) const;
    std::string GetWindowText(HWND hwnd) const;
    std::string GetProcessName(HWND hwnd) const;
    DWORD GetProcessId(HWND hwnd) const;

    // Static callback for EnumWindows
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

private:
    bool m_initialized;
    bool m_monitoring;
    
    // Win32 event hooks
    HWINEVENTHOOK m_foregroundHook;
    HWINEVENTHOOK m_createHook;
    HWINEVENTHOOK m_destroyHook;
    HWINEVENTHOOK m_minimizeHook;
    HWINEVENTHOOK m_restoreHook;

    // Static pointer to current instance for callbacks
    static Win32WindowMonitor* s_instance;
};

} // namespace work_assistant