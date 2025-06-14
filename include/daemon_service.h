#pragma once

#include <string>
#include <memory>
#include <functional>

namespace work_assistant {

class Application;

// Service/Daemon configuration
struct ServiceConfig {
    std::string service_name = "WorkStudyAssistant";
    std::string service_description = "Intelligent work and study activity monitoring service";
    std::string log_file_path = "logs/work_assistant_service.log";
    std::string pid_file_path = "work_assistant.pid";
    std::string config_file_path = "config/work_assistant.conf";
    bool auto_start = false;
    bool enable_logging = true;
    
    bool IsValid() const {
        return !service_name.empty();
    }
};

// Service status enumeration
enum class ServiceStatus {
    STOPPED,
    STARTING,
    RUNNING,
    STOPPING,
    ERROR
};

// Service control interface
class IServiceController {
public:
    virtual ~IServiceController() = default;
    
    virtual bool Install() = 0;
    virtual bool Uninstall() = 0;
    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    virtual bool Restart() = 0;
    
    virtual ServiceStatus GetStatus() const = 0;
    virtual std::string GetStatusString() const = 0;
    
    virtual bool IsInstalled() const = 0;
    virtual bool IsRunning() const = 0;
};

// Platform-specific service implementations
class LinuxDaemonController : public IServiceController {
public:
    LinuxDaemonController(const ServiceConfig& config);
    ~LinuxDaemonController() override;
    
    bool Install() override;
    bool Uninstall() override;
    bool Start() override;
    bool Stop() override;
    bool Restart() override;
    
    ServiceStatus GetStatus() const override;
    std::string GetStatusString() const override;
    
    bool IsInstalled() const override;
    bool IsRunning() const override;
    
    // Daemon-specific methods
    bool RunAsDaemon();
    bool CreatePidFile();
    bool RemovePidFile();
    pid_t GetDaemonPid() const;
    
private:
    ServiceConfig m_config;
    mutable ServiceStatus m_status;
};

class WindowsServiceController : public IServiceController {
public:
    WindowsServiceController(const ServiceConfig& config);
    ~WindowsServiceController() override;
    
    bool Install() override;
    bool Uninstall() override;
    bool Start() override;
    bool Stop() override;
    bool Restart() override;
    
    ServiceStatus GetStatus() const override;
    std::string GetStatusString() const override;
    
    bool IsInstalled() const override;
    bool IsRunning() const override;
    
    // Windows service-specific methods
    static void WINAPI ServiceMain(DWORD argc, LPWSTR* argv);
    static void WINAPI ServiceControlHandler(DWORD control);
    
private:
    ServiceConfig m_config;
    mutable ServiceStatus m_status;
    
#ifdef _WIN32
    SERVICE_STATUS m_serviceStatus;
    SERVICE_STATUS_HANDLE m_serviceStatusHandle;
#endif
};

// Service manager - unified interface for all platforms
class ServiceManager {
public:
    ServiceManager();
    ~ServiceManager();
    
    bool Initialize(const ServiceConfig& config);
    void Shutdown();
    
    // Service control operations
    bool InstallService();
    bool UninstallService();
    bool StartService();
    bool StopService();
    bool RestartService();
    
    // Service status
    ServiceStatus GetServiceStatus() const;
    std::string GetServiceStatusString() const;
    bool IsServiceInstalled() const;
    bool IsServiceRunning() const;
    
    // Run modes
    bool RunAsService();      // Run as Windows service or Linux daemon
    bool RunInteractive();    // Run in foreground/interactive mode
    bool RunOnce();          // Run once and exit
    
    // Application integration
    void SetApplication(std::shared_ptr<Application> app);
    
    // Logging and monitoring
    void EnableLogging(bool enable);
    void SetLogLevel(const std::string& level);
    
    // Configuration
    ServiceConfig GetConfig() const;
    void UpdateConfig(const ServiceConfig& config);
    
private:
    bool m_initialized;
    ServiceConfig m_config;
    std::unique_ptr<IServiceController> m_controller;
    std::shared_ptr<Application> m_application;
    
    void SetupSignalHandlers();
    void HandleSignal(int signal);
    
    static ServiceManager* s_instance;
    static void SignalHandler(int signal);
};

// Service factory
class ServiceFactory {
public:
    static std::unique_ptr<IServiceController> CreateController(const ServiceConfig& config);
    static std::string GetPlatformName();
    static bool IsServiceSupported();
};

// Service utilities
namespace service_utils {
    
    bool CreateServiceDirectories(const ServiceConfig& config);
    bool CheckServicePermissions();
    void LogServiceEvent(const std::string& message);
    std::string GetCurrentUserName();
    std::string GetExecutablePath();
    bool IsRunningAsAdmin();
    
    // Process management
    bool CreatePidFile(const std::string& pid_file_path);
    bool RemovePidFile(const std::string& pid_file_path);
    pid_t ReadPidFile(const std::string& pid_file_path);
    bool IsProcessRunning(pid_t pid);
    bool KillProcess(pid_t pid, int signal = 15); // SIGTERM by default
    
    // Configuration file management
    bool LoadConfigFromFile(const std::string& config_path, ServiceConfig& config);
    bool SaveConfigToFile(const std::string& config_path, const ServiceConfig& config);
    
} // namespace service_utils

} // namespace work_assistant