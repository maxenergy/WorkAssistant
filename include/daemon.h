#pragma once

#include <functional>
#include <string>
#include <memory>

namespace work_assistant {

// Simple daemon interface for background running
class IDaemonService {
public:
    virtual ~IDaemonService() = default;
    
    // Basic daemon operations
    virtual bool StartDaemon() = 0;
    virtual void StopDaemon() = 0;
    virtual bool IsDaemonRunning() const = 0;
    
    // Set main function to run in daemon mode
    virtual void SetMainFunction(std::function<void()> mainFunc) = 0;
    virtual void SetShutdownFunction(std::function<void()> shutdownFunc) = 0;
};

// Simple daemon implementation
class SimpleDaemonService : public IDaemonService {
public:
    SimpleDaemonService();
    ~SimpleDaemonService();
    
    bool StartDaemon() override;
    void StopDaemon() override;
    bool IsDaemonRunning() const override;
    
    void SetMainFunction(std::function<void()> mainFunc) override;
    void SetShutdownFunction(std::function<void()> shutdownFunc) override;
    
private:
    bool m_running;
    std::function<void()> m_mainFunction;
    std::function<void()> m_shutdownFunction;
};

// Factory for daemon service
class DaemonServiceFactory {
public:
    static std::unique_ptr<IDaemonService> Create();
};

} // namespace work_assistant