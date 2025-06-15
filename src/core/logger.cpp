#include "logger.h"
#include "directory_manager.h"
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace work_assistant {

// Utility functions
std::string LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

LogLevel StringToLogLevel(const std::string& level_str) {
    std::string upper_str = level_str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), ::toupper);
    
    if (upper_str == "DEBUG") return LogLevel::DEBUG;
    if (upper_str == "INFO") return LogLevel::INFO;
    if (upper_str == "WARNING" || upper_str == "WARN") return LogLevel::WARNING;
    if (upper_str == "ERROR") return LogLevel::ERROR;
    if (upper_str == "CRITICAL" || upper_str == "CRIT") return LogLevel::CRITICAL;
    
    return LogLevel::INFO; // Default
}

std::string FormatTimestamp(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

// FileLogger implementation
FileLogger::FileLogger(const std::string& filename, bool append)
    : m_filename(filename)
    , m_level(LogLevel::INFO)
    , m_max_size_bytes(10 * 1024 * 1024) // 10MB
    , m_max_files(5)
    , m_current_size(0)
    , m_async(false) {
    
    // Ensure log directory exists
    std::filesystem::path file_path(filename);
    if (file_path.has_parent_path()) {
        DirectoryManager::CreateDirectoryIfNotExists(file_path.parent_path().string());
    }
    
    auto mode = append ? std::ios::app : std::ios::trunc;
    m_file.open(filename, mode);
    
    if (m_file.is_open() && append) {
        // Get current file size
        m_file.seekp(0, std::ios::end);
        m_current_size = m_file.tellp();
    }
}

FileLogger::~FileLogger() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

void FileLogger::Log(const LogMessage& message) {
    if (message.level < m_level) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_file.is_open()) {
        return;
    }
    
    std::string formatted = FormatMessage(message);
    WriteToFile(formatted);
    
    if (!m_async) {
        m_file.flush();
    }
}

void FileLogger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
}

LogLevel FileLogger::GetLevel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_level;
}

void FileLogger::Flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        m_file.flush();
    }
}

void FileLogger::SetRotation(size_t max_size_mb, int max_files) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_max_size_bytes = max_size_mb * 1024 * 1024;
    m_max_files = max_files;
}

void FileLogger::SetAsync(bool async) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_async = async;
}

void FileLogger::RotateIfNeeded() {
    if (m_current_size < m_max_size_bytes) {
        return;
    }
    
    m_file.close();
    
    // Rotate existing files
    for (int i = m_max_files - 1; i > 0; --i) {
        std::string old_name = m_filename + "." + std::to_string(i);
        std::string new_name = m_filename + "." + std::to_string(i + 1);
        
        if (std::filesystem::exists(old_name)) {
            if (i == m_max_files - 1) {
                std::filesystem::remove(new_name); // Remove oldest
            }
            std::filesystem::rename(old_name, new_name);
        }
    }
    
    // Move current log to .1
    std::string backup_name = m_filename + ".1";
    if (std::filesystem::exists(m_filename)) {
        std::filesystem::rename(m_filename, backup_name);
    }
    
    // Open new log file
    m_file.open(m_filename, std::ios::trunc);
    m_current_size = 0;
}

void FileLogger::WriteToFile(const std::string& formatted_message) {
    RotateIfNeeded();
    
    m_file << formatted_message << std::endl;
    m_current_size += formatted_message.length() + 1;
}

std::string FileLogger::FormatMessage(const LogMessage& message) {
    std::stringstream ss;
    
    ss << "[" << FormatTimestamp(message.timestamp) << "] ";
    ss << "[" << LogLevelToString(message.level) << "] ";
    
    if (!message.category.empty()) {
        ss << "[" << message.category << "] ";
    }
    
    ss << message.message;
    
    if (!message.file.empty() && message.line > 0) {
        std::filesystem::path file_path(message.file);
        ss << " (" << file_path.filename().string() << ":" << message.line << ")";
    }
    
    return ss.str();
}

// ConsoleLogger implementation
ConsoleLogger::ConsoleLogger()
    : m_level(LogLevel::INFO)
    , m_color_enabled(true) {
}

void ConsoleLogger::Log(const LogMessage& message) {
    if (message.level < m_level) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string formatted = FormatMessage(message);
    
    if (message.level >= LogLevel::ERROR) {
        std::cerr << formatted << std::endl;
    } else {
        std::cout << formatted << std::endl;
    }
}

void ConsoleLogger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
}

LogLevel ConsoleLogger::GetLevel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_level;
}

void ConsoleLogger::Flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout.flush();
    std::cerr.flush();
}

std::string ConsoleLogger::FormatMessage(const LogMessage& message) {
    std::stringstream ss;
    
    if (m_color_enabled) {
        ss << GetColorCode(message.level);
    }
    
    ss << "[" << FormatTimestamp(message.timestamp) << "] ";
    ss << "[" << GetLevelString(message.level) << "] ";
    
    if (!message.category.empty()) {
        ss << "[" << message.category << "] ";
    }
    
    ss << message.message;
    
    if (m_color_enabled) {
        ss << "\033[0m"; // Reset color
    }
    
    return ss.str();
}

std::string ConsoleLogger::GetColorCode(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "\033[36m"; // Cyan
        case LogLevel::INFO: return "\033[32m";  // Green
        case LogLevel::WARNING: return "\033[33m"; // Yellow
        case LogLevel::ERROR: return "\033[31m"; // Red
        case LogLevel::CRITICAL: return "\033[35m"; // Magenta
        default: return "";
    }
}

std::string ConsoleLogger::GetLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DBG";
        case LogLevel::INFO: return "INF";
        case LogLevel::WARNING: return "WRN";
        case LogLevel::ERROR: return "ERR";
        case LogLevel::CRITICAL: return "CRT";
        default: return "UNK";
    }
}

// CompositeLogger implementation
CompositeLogger::CompositeLogger()
    : m_level(LogLevel::INFO) {
}

void CompositeLogger::AddLogger(std::shared_ptr<ILogger> logger) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_loggers.push_back(logger);
}

void CompositeLogger::RemoveAllLoggers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_loggers.clear();
}

void CompositeLogger::Log(const LogMessage& message) {
    if (message.level < m_level) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& logger : m_loggers) {
        if (logger) {
            logger->Log(message);
        }
    }
}

void CompositeLogger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
    
    for (auto& logger : m_loggers) {
        if (logger) {
            logger->SetLevel(level);
        }
    }
}

LogLevel CompositeLogger::GetLevel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_level;
}

void CompositeLogger::Flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& logger : m_loggers) {
        if (logger) {
            logger->Flush();
        }
    }
}

// Logger singleton implementation
Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

void Logger::Initialize(const std::string& log_file, LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto composite = std::make_shared<CompositeLogger>();
    
    // Always add console logger
    composite->AddConsoleLogger();
    
    // Add file logger if specified
    if (!log_file.empty()) {
        composite->AddFileLogger(log_file);
    } else {
        // Default log file in logs directory
        std::string default_log = DirectoryManager::JoinPath(
            DirectoryManager::GetLogsDirectory(), "work_assistant.log");
        composite->AddFileLogger(default_log);
    }
    
    composite->SetLevel(level);
    m_logger = composite;
    m_initialized = true;
    
    Info("Logger initialized successfully", "LOGGER");
}

void Logger::SetLogger(std::shared_ptr<ILogger> logger) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logger = logger;
    m_initialized = true;
}

void Logger::AddFileLogger(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_logger) {
        m_logger = std::make_shared<CompositeLogger>();
    }
    
    auto composite = std::dynamic_pointer_cast<CompositeLogger>(m_logger);
    if (composite) {
        auto file_logger = std::make_shared<FileLogger>(filename);
        composite->AddLogger(file_logger);
    }
}

void Logger::AddConsoleLogger() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_logger) {
        m_logger = std::make_shared<CompositeLogger>();
    }
    
    auto composite = std::dynamic_pointer_cast<CompositeLogger>(m_logger);
    if (composite) {
        auto console_logger = std::make_shared<ConsoleLogger>();
        composite->AddLogger(console_logger);
    }
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logger) {
        m_logger->SetLevel(level);
    }
}

void Logger::Log(LogLevel level, const std::string& category, const std::string& message,
                 const std::string& file, int line, const std::string& function) {
    if (!m_initialized || !m_logger) {
        // Fallback to console output
        std::cout << "[" << LogLevelToString(level) << "] " << message << std::endl;
        return;
    }
    
    LogMessage log_msg;
    log_msg.timestamp = std::chrono::system_clock::now();
    log_msg.level = level;
    log_msg.category = category;
    log_msg.message = message;
    log_msg.file = file;
    log_msg.line = line;
    log_msg.function = function;
    
    m_logger->Log(log_msg);
}

void Logger::Debug(const std::string& message, const std::string& category) {
    Log(LogLevel::DEBUG, category, message);
}

void Logger::Info(const std::string& message, const std::string& category) {
    Log(LogLevel::INFO, category, message);
}

void Logger::Warning(const std::string& message, const std::string& category) {
    Log(LogLevel::WARNING, category, message);
}

void Logger::Error(const std::string& message, const std::string& category) {
    Log(LogLevel::ERROR, category, message);
}

void Logger::Critical(const std::string& message, const std::string& category) {
    Log(LogLevel::CRITICAL, category, message);
}

bool Logger::IsEnabled(LogLevel level) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logger) {
        return level >= m_logger->GetLevel();
    }
    return level >= LogLevel::INFO;
}

void Logger::Flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logger) {
        m_logger->Flush();
    }
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logger) {
        m_logger->Flush();
        m_logger.reset();
    }
    m_initialized = false;
}

// ScopedLogger implementation
ScopedLogger::ScopedLogger(const std::string& function_name, const std::string& category)
    : m_function_name(function_name)
    , m_category(category)
    , m_start_time(std::chrono::high_resolution_clock::now()) {
    
    Logger::GetInstance().Debug("Entering " + m_function_name, m_category);
}

ScopedLogger::~ScopedLogger() {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - m_start_time);
    
    std::stringstream ss;
    ss << "Exiting " << m_function_name;
    if (!m_message.empty()) {
        ss << " - " << m_message;
    }
    ss << " (took " << duration.count() << "μs)";
    
    Logger::GetInstance().Debug(ss.str(), m_category);
}

} // namespace work_assistant