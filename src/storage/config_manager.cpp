#include "config_manager.h"
#include "directory_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace work_assistant {

ConfigManager::ConfigManager() : m_initialized(false) {}

ConfigManager::~ConfigManager() {
    if (m_initialized) {
        SaveConfig();
    }
}

bool ConfigManager::Initialize(const std::string& config_dir) {
    if (config_dir.empty()) {
        m_config_dir = DirectoryManager::GetConfigDirectory();
    } else {
        m_config_dir = config_dir;
    }
    
    // Ensure config directory exists
    if (!DirectoryManager::CreateDirectoryIfNotExists(m_config_dir)) {
        std::cerr << "Failed to create config directory: " << m_config_dir << std::endl;
        return false;
    }
    
    m_config_file_path = DirectoryManager::JoinPath(m_config_dir, "work_assistant.conf");
    
    // Set default configuration
    SetDefaultConfiguration();
    
    // Try to load existing configuration
    LoadConfig();
    
    m_initialized = true;
    std::cout << "Configuration manager initialized with config file: " << m_config_file_path << std::endl;
    
    return true;
}

bool ConfigManager::LoadConfig(const std::string& config_file) {
    std::string file_path = config_file.empty() ? m_config_file_path : config_file;
    
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cout << "Config file not found, using defaults: " << file_path << std::endl;
        return true; // Not an error - will use defaults
    }
    
    std::string line;
    std::string current_section;
    int line_number = 0;
    
    while (std::getline(file, line)) {
        line_number++;
        
        // Remove leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Check for section header
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Parse key-value pair
        std::string section, key, value;
        if (ParseConfigLine(line, section, key, value)) {
            if (!section.empty()) {
                current_section = section;
            } else {
                section = current_section;
            }
            
            if (!section.empty() && !key.empty()) {
                m_config_data[section][key] = UnescapeValue(value);
            }
        } else {
            std::cerr << "Invalid config line " << line_number << ": " << line << std::endl;
        }
    }
    
    file.close();
    std::cout << "Configuration loaded from: " << file_path << std::endl;
    
    return ValidateConfig();
}

bool ConfigManager::SaveConfig(const std::string& config_file) {
    std::string file_path = config_file.empty() ? m_config_file_path : config_file;
    
    std::ofstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file for writing: " << file_path << std::endl;
        return false;
    }
    
    // Write header
    file << "# Work Assistant Configuration File\n";
    file << "# Generated automatically - modify with care\n";
    file << "\n";
    
    // Write sections
    for (const auto& [section_name, section_data] : m_config_data) {
        file << "[" << section_name << "]\n";
        
        for (const auto& [key, value] : section_data) {
            file << key << " = " << EscapeValue(value) << "\n";
        }
        
        file << "\n";
    }
    
    file.close();
    std::cout << "Configuration saved to: " << file_path << std::endl;
    
    return true;
}

std::string ConfigManager::GetString(const std::string& section, const std::string& key, const std::string& default_value) const {
    auto section_it = m_config_data.find(section);
    if (section_it == m_config_data.end()) {
        return default_value;
    }
    
    auto key_it = section_it->second.find(key);
    if (key_it == section_it->second.end()) {
        return default_value;
    }
    
    return key_it->second;
}

int ConfigManager::GetInt(const std::string& section, const std::string& key, int default_value) const {
    std::string value = GetString(section, key);
    if (value.empty()) {
        return default_value;
    }
    
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        return default_value;
    }
}

bool ConfigManager::GetBool(const std::string& section, const std::string& key, bool default_value) const {
    std::string value = GetString(section, key);
    if (value.empty()) {
        return default_value;
    }
    
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

double ConfigManager::GetDouble(const std::string& section, const std::string& key, double default_value) const {
    std::string value = GetString(section, key);
    if (value.empty()) {
        return default_value;
    }
    
    try {
        return std::stod(value);
    } catch (const std::exception&) {
        return default_value;
    }
}

void ConfigManager::SetString(const std::string& section, const std::string& key, const std::string& value) {
    m_config_data[section][key] = value;
}

void ConfigManager::SetInt(const std::string& section, const std::string& key, int value) {
    m_config_data[section][key] = std::to_string(value);
}

void ConfigManager::SetBool(const std::string& section, const std::string& key, bool value) {
    m_config_data[section][key] = value ? "true" : "false";
}

void ConfigManager::SetDouble(const std::string& section, const std::string& key, double value) {
    m_config_data[section][key] = std::to_string(value);
}

bool ConfigManager::ValidateConfig() const {
    // Basic validation - check required sections exist
    std::vector<std::string> required_sections = {
        DefaultConfig::APP_SECTION,
        DefaultConfig::OCR_SECTION,
        DefaultConfig::AI_SECTION,
        DefaultConfig::STORAGE_SECTION,
        DefaultConfig::WEB_SECTION,
        DefaultConfig::MONITOR_SECTION
    };
    
    for (const auto& section : required_sections) {
        if (m_config_data.find(section) == m_config_data.end()) {
            std::cerr << "Missing required config section: " << section << std::endl;
            return false;
        }
    }
    
    // Validate specific values
    int port = GetInt(DefaultConfig::WEB_SECTION, DefaultConfig::WEB_PORT, 8080);
    if (port < 1 || port > 65535) {
        std::cerr << "Invalid web port: " << port << std::endl;
        return false;
    }
    
    double confidence = GetDouble(DefaultConfig::OCR_SECTION, DefaultConfig::OCR_CONFIDENCE_THRESHOLD, 0.7);
    if (confidence < 0.0 || confidence > 1.0) {
        std::cerr << "Invalid OCR confidence threshold: " << confidence << std::endl;
        return false;
    }
    
    return true;
}

std::vector<std::string> ConfigManager::GetSectionKeys(const std::string& section) const {
    std::vector<std::string> keys;
    
    auto section_it = m_config_data.find(section);
    if (section_it != m_config_data.end()) {
        for (const auto& [key, value] : section_it->second) {
            keys.push_back(key);
        }
    }
    
    return keys;
}

bool ConfigManager::HasKey(const std::string& section, const std::string& key) const {
    auto section_it = m_config_data.find(section);
    if (section_it == m_config_data.end()) {
        return false;
    }
    
    return section_it->second.find(key) != section_it->second.end();
}

bool ConfigManager::RemoveKey(const std::string& section, const std::string& key) {
    auto section_it = m_config_data.find(section);
    if (section_it == m_config_data.end()) {
        return false;
    }
    
    auto key_it = section_it->second.find(key);
    if (key_it == section_it->second.end()) {
        return false;
    }
    
    section_it->second.erase(key_it);
    return true;
}

void ConfigManager::ResetToDefaults() {
    m_config_data.clear();
    SetDefaultConfiguration();
}

std::string ConfigManager::GetFullKey(const std::string& section, const std::string& key) const {
    return section + "." + key;
}

void ConfigManager::SetDefaultConfiguration() {
    // Application settings
    SetString(DefaultConfig::APP_SECTION, DefaultConfig::APP_LOG_LEVEL, "info");
    SetBool(DefaultConfig::APP_SECTION, DefaultConfig::APP_AUTO_START, false);
    SetBool(DefaultConfig::APP_SECTION, DefaultConfig::APP_MINIMIZE_TO_TRAY, true);
    SetBool(DefaultConfig::APP_SECTION, DefaultConfig::APP_CHECK_UPDATES, true);
    
    // OCR settings
    SetInt(DefaultConfig::OCR_SECTION, DefaultConfig::OCR_DEFAULT_MODE, 3); // AUTO
    SetString(DefaultConfig::OCR_SECTION, DefaultConfig::OCR_LANGUAGE, "eng");
    SetDouble(DefaultConfig::OCR_SECTION, DefaultConfig::OCR_CONFIDENCE_THRESHOLD, 0.7);
    SetBool(DefaultConfig::OCR_SECTION, DefaultConfig::OCR_USE_GPU, true);
    SetInt(DefaultConfig::OCR_SECTION, DefaultConfig::OCR_MAX_IMAGE_SIZE, 2048);
    
    // AI settings
    SetString(DefaultConfig::AI_SECTION, DefaultConfig::AI_MODEL_PATH, "models/qwen2.5-1.5b-instruct-q4_k_m.gguf");
    SetInt(DefaultConfig::AI_SECTION, DefaultConfig::AI_CONTEXT_LENGTH, 2048);
    SetInt(DefaultConfig::AI_SECTION, DefaultConfig::AI_GPU_LAYERS, 32);
    SetDouble(DefaultConfig::AI_SECTION, DefaultConfig::AI_TEMPERATURE, 0.7);
    
    // Storage settings
    SetBool(DefaultConfig::STORAGE_SECTION, DefaultConfig::STORAGE_AUTO_BACKUP, true);
    SetInt(DefaultConfig::STORAGE_SECTION, DefaultConfig::STORAGE_BACKUP_INTERVAL_HOURS, 24);
    SetInt(DefaultConfig::STORAGE_SECTION, DefaultConfig::STORAGE_MAX_STORAGE_SIZE_GB, 10);
    SetBool(DefaultConfig::STORAGE_SECTION, DefaultConfig::STORAGE_ENCRYPTION_ENABLED, true);
    
    // Web server settings
    SetBool(DefaultConfig::WEB_SECTION, DefaultConfig::WEB_ENABLED, true);
    SetString(DefaultConfig::WEB_SECTION, DefaultConfig::WEB_HOST, "127.0.0.1");
    SetInt(DefaultConfig::WEB_SECTION, DefaultConfig::WEB_PORT, 8080);
    SetBool(DefaultConfig::WEB_SECTION, DefaultConfig::WEB_ENABLE_CORS, true);
    SetBool(DefaultConfig::WEB_SECTION, DefaultConfig::WEB_ENABLE_WEBSOCKET, true);
    
    // Monitoring settings
    SetBool(DefaultConfig::MONITOR_SECTION, DefaultConfig::MONITOR_WINDOW_EVENTS, true);
    SetBool(DefaultConfig::MONITOR_SECTION, DefaultConfig::MONITOR_SCREEN_CAPTURE, true);
    SetInt(DefaultConfig::MONITOR_SECTION, DefaultConfig::MONITOR_CAPTURE_INTERVAL_MS, 1000);
    SetInt(DefaultConfig::MONITOR_SECTION, DefaultConfig::MONITOR_OCR_INTERVAL_FRAMES, 10);
}

bool ConfigManager::ParseConfigLine(const std::string& line, std::string& section, std::string& key, std::string& value) {
    // Find equals sign
    size_t equals_pos = line.find('=');
    if (equals_pos == std::string::npos) {
        return false;
    }
    
    // Extract key and value
    std::string key_part = line.substr(0, equals_pos);
    std::string value_part = line.substr(equals_pos + 1);
    
    // Trim whitespace
    key_part.erase(0, key_part.find_first_not_of(" \t"));
    key_part.erase(key_part.find_last_not_of(" \t") + 1);
    value_part.erase(0, value_part.find_first_not_of(" \t"));
    value_part.erase(value_part.find_last_not_of(" \t") + 1);
    
    // Check for section.key format
    size_t dot_pos = key_part.find('.');
    if (dot_pos != std::string::npos) {
        section = key_part.substr(0, dot_pos);
        key = key_part.substr(dot_pos + 1);
    } else {
        section = "";
        key = key_part;
    }
    
    value = value_part;
    return !key.empty();
}

std::string ConfigManager::EscapeValue(const std::string& value) const {
    std::string escaped = value;
    
    // Replace special characters
    size_t pos = 0;
    while ((pos = escaped.find('\\', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\\");
        pos += 2;
    }
    
    pos = 0;
    while ((pos = escaped.find('\n', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\n");
        pos += 2;
    }
    
    pos = 0;
    while ((pos = escaped.find('\t', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\t");
        pos += 2;
    }
    
    return escaped;
}

std::string ConfigManager::UnescapeValue(const std::string& value) const {
    std::string unescaped = value;
    
    // Replace escaped characters
    size_t pos = 0;
    while ((pos = unescaped.find("\\\\", pos)) != std::string::npos) {
        unescaped.replace(pos, 2, "\\");
        pos += 1;
    }
    
    pos = 0;
    while ((pos = unescaped.find("\\n", pos)) != std::string::npos) {
        unescaped.replace(pos, 2, "\n");
        pos += 1;
    }
    
    pos = 0;
    while ((pos = unescaped.find("\\t", pos)) != std::string::npos) {
        unescaped.replace(pos, 2, "\t");
        pos += 1;
    }
    
    return unescaped;
}

} // namespace work_assistant