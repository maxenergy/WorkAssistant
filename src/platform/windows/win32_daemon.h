#pragma once

#include <windows.h>
#include <string>
#include <functional>

namespace work_assistant {

class Win32Daemon {
public:
    Win32Daemon(const std::string& serviceName);
    ~Win32Daemon();

    // Service control functions
    bool InstallService(const std::string& executablePath);
    bool UninstallService();
    bool StartService();
    bool StopService();
    bool IsServiceInstalled() const;
    bool IsServiceRunning() const;

    // Run as console application (for development/testing)
    bool RunAsConsole(std::function<void()> mainFunction);

    // Run as Windows service
    bool RunAsService(std::function<void()> mainFunction);

    // Signal handlers
    static void SignalShutdown();
    static bool ShouldShutdown();

private:
    // Service control handler
    static VOID WINAPI ServiceCtrlHandler(DWORD ctrl);
    
    // Service main function
    static VOID WINAPI ServiceMain(DWORD argc, LPWSTR* argv);

    // Report service status
    void ReportServiceStatus(DWORD currentState, DWORD exitCode, DWORD waitHint);

    // Service implementation
    void ServiceWorkerThread();

private:
    std::string m_serviceName;
    std::wstring m_serviceNameW;
    
    // Service status
    SERVICE_STATUS m_serviceStatus;
    SERVICE_STATUS_HANDLE m_statusHandle;
    
    // Service function
    std::function<void()> m_serviceFunction;
    
    // Shutdown flag
    static volatile bool s_shutdownRequested;
    
    // Static instance for service callbacks
    static Win32Daemon* s_instance;
};

} // namespace work_assistant