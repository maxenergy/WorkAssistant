#include "win32_daemon.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace work_assistant {

// Static members
volatile bool Win32Daemon::s_shutdownRequested = false;
Win32Daemon* Win32Daemon::s_instance = nullptr;

Win32Daemon::Win32Daemon(const std::string& serviceName)
    : m_serviceName(serviceName)
    , m_statusHandle(nullptr)
{
    // Convert service name to wide string
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, serviceName.c_str(), -1, nullptr, 0);
    m_serviceNameW.resize(wideSize);
    MultiByteToWideChar(CP_UTF8, 0, serviceName.c_str(), -1, &m_serviceNameW[0], wideSize);

    // Initialize service status
    ZeroMemory(&m_serviceStatus, sizeof(m_serviceStatus));
    m_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    m_serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    m_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    s_instance = this;
}

Win32Daemon::~Win32Daemon() {
    s_instance = nullptr;
}

bool Win32Daemon::InstallService(const std::string& executablePath) {
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scManager) {
        std::cerr << "Failed to open service control manager: " << GetLastError() << std::endl;
        return false;
    }

    // Convert executable path to wide string
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, executablePath.c_str(), -1, nullptr, 0);
    std::wstring executablePathW(wideSize, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, executablePath.c_str(), -1, &executablePathW[0], wideSize);

    SC_HANDLE service = CreateService(
        scManager,
        m_serviceNameW.c_str(),
        m_serviceNameW.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        executablePathW.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    if (!service) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_EXISTS) {
            std::cout << "Service already exists" << std::endl;
        } else {
            std::cerr << "Failed to create service: " << error << std::endl;
        }
        CloseServiceHandle(scManager);
        return false;
    }

    std::cout << "Service installed successfully" << std::endl;
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return true;
}

bool Win32Daemon::UninstallService() {
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scManager) {
        std::cerr << "Failed to open service control manager: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE service = OpenService(scManager, m_serviceNameW.c_str(), DELETE);
    if (!service) {
        std::cerr << "Failed to open service: " << GetLastError() << std::endl;
        CloseServiceHandle(scManager);
        return false;
    }

    // Stop service if running
    SERVICE_STATUS status;
    if (QueryServiceStatus(service, &status) && status.dwCurrentState != SERVICE_STOPPED) {
        ControlService(service, SERVICE_CONTROL_STOP, &status);
        
        // Wait for service to stop
        for (int i = 0; i < 30; ++i) {
            QueryServiceStatus(service, &status);
            if (status.dwCurrentState == SERVICE_STOPPED) {
                break;
            }
            Sleep(1000);
        }
    }

    if (!DeleteService(service)) {
        std::cerr << "Failed to delete service: " << GetLastError() << std::endl;
        CloseServiceHandle(service);
        CloseServiceHandle(scManager);
        return false;
    }

    std::cout << "Service uninstalled successfully" << std::endl;
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return true;
}

bool Win32Daemon::StartService() {
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) {
        std::cerr << "Failed to open service control manager: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE service = OpenService(scManager, m_serviceNameW.c_str(), SERVICE_START);
    if (!service) {
        std::cerr << "Failed to open service: " << GetLastError() << std::endl;
        CloseServiceHandle(scManager);
        return false;
    }

    if (!::StartService(service, 0, nullptr)) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_ALREADY_RUNNING) {
            std::cout << "Service is already running" << std::endl;
        } else {
            std::cerr << "Failed to start service: " << error << std::endl;
            CloseServiceHandle(service);
            CloseServiceHandle(scManager);
            return false;
        }
    }

    std::cout << "Service started successfully" << std::endl;
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return true;
}

bool Win32Daemon::StopService() {
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) {
        std::cerr << "Failed to open service control manager: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE service = OpenService(scManager, m_serviceNameW.c_str(), SERVICE_STOP);
    if (!service) {
        std::cerr << "Failed to open service: " << GetLastError() << std::endl;
        CloseServiceHandle(scManager);
        return false;
    }

    SERVICE_STATUS status;
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_NOT_ACTIVE) {
            std::cout << "Service is not running" << std::endl;
        } else {
            std::cerr << "Failed to stop service: " << error << std::endl;
            CloseServiceHandle(service);
            CloseServiceHandle(scManager);
            return false;
        }
    }

    std::cout << "Service stopped successfully" << std::endl;
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return true;
}

bool Win32Daemon::IsServiceInstalled() const {
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) {
        return false;
    }

    SC_HANDLE service = OpenService(scManager, m_serviceNameW.c_str(), SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scManager);
        return false;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return true;
}

bool Win32Daemon::IsServiceRunning() const {
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) {
        return false;
    }

    SC_HANDLE service = OpenService(scManager, m_serviceNameW.c_str(), SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scManager);
        return false;
    }

    SERVICE_STATUS status;
    bool isRunning = false;
    if (QueryServiceStatus(service, &status)) {
        isRunning = (status.dwCurrentState == SERVICE_RUNNING);
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return isRunning;
}

bool Win32Daemon::RunAsConsole(std::function<void()> mainFunction) {
    // Set up console control handler
    SetConsoleCtrlHandler([](DWORD ctrlType) -> BOOL {
        switch (ctrlType) {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
            case CTRL_LOGOFF_EVENT:
            case CTRL_SHUTDOWN_EVENT:
                std::cout << "Shutting down..." << std::endl;
                SignalShutdown();
                return TRUE;
            default:
                return FALSE;
        }
    }, TRUE);

    std::cout << "Running as console application..." << std::endl;
    
    // Run the main function
    if (mainFunction) {
        mainFunction();
    }

    return true;
}

bool Win32Daemon::RunAsService(std::function<void()> mainFunction) {
    m_serviceFunction = mainFunction;

    SERVICE_TABLE_ENTRY serviceTable[] = {
        { const_cast<LPWSTR>(m_serviceNameW.c_str()), ServiceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcher(serviceTable)) {
        DWORD error = GetLastError();
        if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            std::cerr << "This application must be run as a service or with --console flag" << std::endl;
        } else {
            std::cerr << "Failed to start service control dispatcher: " << error << std::endl;
        }
        return false;
    }

    return true;
}

void Win32Daemon::SignalShutdown() {
    s_shutdownRequested = true;
}

bool Win32Daemon::ShouldShutdown() {
    return s_shutdownRequested;
}

VOID WINAPI Win32Daemon::ServiceCtrlHandler(DWORD ctrl) {
    if (!s_instance) {
        return;
    }

    switch (ctrl) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            s_instance->ReportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
            SignalShutdown();
            s_instance->ReportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
            break;
        default:
            break;
    }
}

VOID WINAPI Win32Daemon::ServiceMain(DWORD argc, LPWSTR* argv) {
    if (!s_instance) {
        return;
    }

    // Register service control handler
    s_instance->m_statusHandle = RegisterServiceCtrlHandler(
        s_instance->m_serviceNameW.c_str(),
        ServiceCtrlHandler
    );

    if (!s_instance->m_statusHandle) {
        return;
    }

    // Report service is starting
    s_instance->ReportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Start service worker thread
    std::thread workerThread(&Win32Daemon::ServiceWorkerThread, s_instance);

    // Report service is running
    s_instance->ReportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

    // Wait for worker thread to complete
    workerThread.join();
}

void Win32Daemon::ReportServiceStatus(DWORD currentState, DWORD exitCode, DWORD waitHint) {
    static DWORD checkPoint = 1;

    m_serviceStatus.dwCurrentState = currentState;
    m_serviceStatus.dwWin32ExitCode = exitCode;
    m_serviceStatus.dwWaitHint = waitHint;

    if (currentState == SERVICE_START_PENDING) {
        m_serviceStatus.dwControlsAccepted = 0;
    } else {
        m_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }

    if (currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED) {
        m_serviceStatus.dwCheckPoint = 0;
    } else {
        m_serviceStatus.dwCheckPoint = checkPoint++;
    }

    SetServiceStatus(m_statusHandle, &m_serviceStatus);
}

void Win32Daemon::ServiceWorkerThread() {
    if (m_serviceFunction) {
        m_serviceFunction();
    }
}

} // namespace work_assistant