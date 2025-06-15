#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <any>

namespace work_assistant {

// Configuration management for the Work Assistant application
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    // Initialize configuration system
    bool Initialize(const std::string& config_dir = "");
    
    // Load configuration from file
    bool LoadConfig(const std::string& config_file = "");
    
    // Save configuration to file
    bool SaveConfig(const std::string& config_file = "");
    
    // Get configuration values
    template<typename T>
    T GetValue(const std::string& key, const T& default_value = T{}) const;
    
    // Set configuration values
    template<typename T>
    void SetValue(const std::string& key, const T& value);
    
    // Configuration sections
    std::string GetString(const std::string& section, const std::string& key, const std::string& default_value = "") const;
    int GetInt(const std::string& section, const std::string& key, int default_value = 0) const;
    bool GetBool(const std::string& section, const std::string& key, bool default_value = false) const;
    double GetDouble(const std::string& section, const std::string& key, double default_value = 0.0) const;
    
    void SetString(const std::string& section, const std::string& key, const std::string& value);
    void SetInt(const std::string& section, const std::string& key, int value);
    void SetBool(const std::string& section, const std::string& key, bool value);
    void SetDouble(const std::string& section, const std::string& key, double value);
    
    // Configuration validation
    bool ValidateConfig() const;
    
    // Get all keys in a section
    std::vector<std::string> GetSectionKeys(const std::string& section) const;
    
    // Check if key exists
    bool HasKey(const std::string& section, const std::string& key) const;
    
    // Remove configuration entry
    bool RemoveKey(const std::string& section, const std::string& key);
    
    // Get current config file path
    std::string GetConfigFilePath() const { return m_config_file_path; }
    
    // Reset to default configuration
    void ResetToDefaults();

private:
    // Internal storage
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_config_data;
    
    std::string m_config_dir;
    std::string m_config_file_path;
    bool m_initialized = false;
    
    // Helper methods
    std::string GetFullKey(const std::string& section, const std::string& key) const;
    void SetDefaultConfiguration();
    bool ParseConfigLine(const std::string& line, std::string& section, std::string& key, std::string& value);
    std::string EscapeValue(const std::string& value) const;
    std::string UnescapeValue(const std::string& value) const;
};

// Default configuration values
struct DefaultConfig {
    // Application settings
    static constexpr const char* APP_SECTION = "application";
    static constexpr const char* APP_LOG_LEVEL = "log_level";
    static constexpr const char* APP_AUTO_START = "auto_start";
    static constexpr const char* APP_MINIMIZE_TO_TRAY = "minimize_to_tray";
    static constexpr const char* APP_CHECK_UPDATES = "check_updates";
    
    // OCR settings
    static constexpr const char* OCR_SECTION = "ocr";
    static constexpr const char* OCR_DEFAULT_MODE = "default_mode";
    static constexpr const char* OCR_LANGUAGE = "language";
    static constexpr const char* OCR_CONFIDENCE_THRESHOLD = "confidence_threshold";
    static constexpr const char* OCR_USE_GPU = "use_gpu";
    static constexpr const char* OCR_MAX_IMAGE_SIZE = "max_image_size";
    
    // AI settings
    static constexpr const char* AI_SECTION = "ai";
    static constexpr const char* AI_MODEL_PATH = "model_path";
    static constexpr const char* AI_CONTEXT_LENGTH = "context_length";
    static constexpr const char* AI_GPU_LAYERS = "gpu_layers";
    static constexpr const char* AI_TEMPERATURE = "temperature";
    
    // Storage settings
    static constexpr const char* STORAGE_SECTION = "storage";
    static constexpr const char* STORAGE_AUTO_BACKUP = "auto_backup";
    static constexpr const char* STORAGE_BACKUP_INTERVAL_HOURS = "backup_interval_hours";
    static constexpr const char* STORAGE_MAX_STORAGE_SIZE_GB = "max_storage_size_gb";
    static constexpr const char* STORAGE_ENCRYPTION_ENABLED = "encryption_enabled";
    
    // Web server settings
    static constexpr const char* WEB_SECTION = "web";
    static constexpr const char* WEB_ENABLED = "enabled";
    static constexpr const char* WEB_HOST = "host";
    static constexpr const char* WEB_PORT = "port";
    static constexpr const char* WEB_ENABLE_CORS = "enable_cors";
    static constexpr const char* WEB_ENABLE_WEBSOCKET = "enable_websocket";
    
    // Monitoring settings
    static constexpr const char* MONITOR_SECTION = "monitoring";
    static constexpr const char* MONITOR_WINDOW_EVENTS = "window_events";
    static constexpr const char* MONITOR_SCREEN_CAPTURE = "screen_capture";
    static constexpr const char* MONITOR_CAPTURE_INTERVAL_MS = "capture_interval_ms";
    static constexpr const char* MONITOR_OCR_INTERVAL_FRAMES = "ocr_interval_frames";
};

// Template implementations
template<typename T>
T ConfigManager::GetValue(const std::string& key, const T& default_value) const {
    // This is a simplified implementation
    // In practice, you'd need proper type conversion
    return default_value;
}

template<typename T>
void ConfigManager::SetValue(const std::string& key, const T& value) {
    // This is a simplified implementation
    // In practice, you'd need proper type conversion
}

} // namespace work_assistant