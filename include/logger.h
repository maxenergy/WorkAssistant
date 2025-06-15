#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <iostream>

namespace work_assistant {

// Log levels
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

// Log message structure
struct LogMessage {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string category;
    std::string message;
    std::string file;
    int line;
    std::string function;
};

// Logger interface
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void Log(const LogMessage& message) = 0;
    virtual void SetLevel(LogLevel level) = 0;
    virtual LogLevel GetLevel() const = 0;
    virtual void Flush() = 0;
};

// File logger implementation
class FileLogger : public ILogger {
public:
    FileLogger(const std::string& filename, bool append = true);
    ~FileLogger();
    
    void Log(const LogMessage& message) override;
    void SetLevel(LogLevel level) override;
    LogLevel GetLevel() const override;
    void Flush() override;
    
    // File-specific methods
    void SetRotation(size_t max_size_mb, int max_files = 5);
    void SetAsync(bool async);
    
private:
    void RotateIfNeeded();
    void WriteToFile(const std::string& formatted_message);
    std::string FormatMessage(const LogMessage& message);
    
private:
    std::string m_filename;
    std::ofstream m_file;
    LogLevel m_level;
    mutable std::mutex m_mutex;
    
    // Rotation settings
    size_t m_max_size_bytes;
    int m_max_files;
    size_t m_current_size;
    
    bool m_async;
};

// Console logger implementation
class ConsoleLogger : public ILogger {
public:
    ConsoleLogger();
    ~ConsoleLogger() = default;
    
    void Log(const LogMessage& message) override;
    void SetLevel(LogLevel level) override;
    LogLevel GetLevel() const override;
    void Flush() override;
    
    void SetColorEnabled(bool enabled) { m_color_enabled = enabled; }
    
private:
    std::string FormatMessage(const LogMessage& message);
    std::string GetColorCode(LogLevel level);
    std::string GetLevelString(LogLevel level);
    
private:
    LogLevel m_level;
    bool m_color_enabled;
    mutable std::mutex m_mutex;
};

// Composite logger that can write to multiple destinations
class CompositeLogger : public ILogger {
public:
    CompositeLogger();
    ~CompositeLogger() = default;
    
    void AddLogger(std::shared_ptr<ILogger> logger);
    void RemoveAllLoggers();
    
    void Log(const LogMessage& message) override;
    void SetLevel(LogLevel level) override;
    LogLevel GetLevel() const override;
    void Flush() override;
    
private:
    std::vector<std::shared_ptr<ILogger>> m_loggers;
    LogLevel m_level;
    mutable std::mutex m_mutex;
};

// Global logger manager
class Logger {
public:
    static Logger& GetInstance();
    
    // Logger management
    void Initialize(const std::string& log_file = "", LogLevel level = LogLevel::INFO);
    void SetLogger(std::shared_ptr<ILogger> logger);
    void AddFileLogger(const std::string& filename);
    void AddConsoleLogger();
    void SetLevel(LogLevel level);
    
    // Logging methods
    void Log(LogLevel level, const std::string& category, const std::string& message,
             const std::string& file = "", int line = 0, const std::string& function = "");
    
    void Debug(const std::string& message, const std::string& category = "");
    void Info(const std::string& message, const std::string& category = "");
    void Warning(const std::string& message, const std::string& category = "");
    void Error(const std::string& message, const std::string& category = "");
    void Critical(const std::string& message, const std::string& category = "");
    
    // Convenience methods
    bool IsEnabled(LogLevel level) const;
    void Flush();
    void Shutdown();
    
private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
private:
    std::shared_ptr<ILogger> m_logger;
    mutable std::mutex m_mutex;
    bool m_initialized = false;
};

// Utility functions
std::string LogLevelToString(LogLevel level);
LogLevel StringToLogLevel(const std::string& level_str);
std::string FormatTimestamp(const std::chrono::system_clock::time_point& time);

// Scoped logger for RAII timing
class ScopedLogger {
public:
    ScopedLogger(const std::string& function_name, const std::string& category = "PERF");
    ~ScopedLogger();
    
    void SetMessage(const std::string& message) { m_message = message; }
    
private:
    std::string m_function_name;
    std::string m_category;
    std::string m_message;
    std::chrono::high_resolution_clock::time_point m_start_time;
};

} // namespace work_assistant

// Convenience macros for logging
#define LOG_DEBUG(msg) work_assistant::Logger::GetInstance().Debug(msg, __func__)
#define LOG_INFO(msg) work_assistant::Logger::GetInstance().Info(msg, __func__)
#define LOG_WARNING(msg) work_assistant::Logger::GetInstance().Warning(msg, __func__)
#define LOG_ERROR(msg) work_assistant::Logger::GetInstance().Error(msg, __func__)
#define LOG_CRITICAL(msg) work_assistant::Logger::GetInstance().Critical(msg, __func__)

#define LOG_DEBUG_CAT(msg, cat) work_assistant::Logger::GetInstance().Debug(msg, cat)
#define LOG_INFO_CAT(msg, cat) work_assistant::Logger::GetInstance().Info(msg, cat)
#define LOG_WARNING_CAT(msg, cat) work_assistant::Logger::GetInstance().Warning(msg, cat)
#define LOG_ERROR_CAT(msg, cat) work_assistant::Logger::GetInstance().Error(msg, cat)
#define LOG_CRITICAL_CAT(msg, cat) work_assistant::Logger::GetInstance().Critical(msg, cat)

#define LOG_SCOPE(func_name) work_assistant::ScopedLogger _scoped_logger(func_name)
#define LOG_SCOPE_CAT(func_name, cat) work_assistant::ScopedLogger _scoped_logger(func_name, cat)