#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace work_assistant {

// Directory management for the Work Assistant application
class DirectoryManager {
public:
    // Initialize directory structure
    static bool InitializeDirectories(const std::string& base_path = "");
    
    // Get standard directory paths
    static std::string GetDataDirectory();
    static std::string GetConfigDirectory();
    static std::string GetLogsDirectory();
    static std::string GetCacheDirectory();
    static std::string GetModelsDirectory();
    static std::string GetTempDirectory();
    
    // Specialized directories
    static std::string GetScreenshotsDirectory();
    static std::string GetOCRResultsDirectory();
    static std::string GetAIAnalysisDirectory();
    static std::string GetBackupDirectory();
    
    // Directory operations
    static bool CreateDirectoryIfNotExists(const std::string& path);
    static bool EnsureDirectoryWritable(const std::string& path);
    static bool CleanupTempFiles(int max_age_hours = 24);
    static bool CleanupCacheFiles(size_t max_size_mb = 1024);
    
    // Path utilities
    static std::string JoinPath(const std::string& base, const std::string& sub);
    static std::string GetUniqueFilename(const std::string& directory, const std::string& prefix, const std::string& extension);
    static bool IsValidPath(const std::string& path);
    
    // Storage statistics
    struct DirectoryStats {
        size_t total_files = 0;
        size_t total_size_bytes = 0;
        std::string oldest_file;
        std::string newest_file;
    };
    
    static DirectoryStats GetDirectoryStats(const std::string& path);
    static std::vector<std::string> ListFiles(const std::string& directory, const std::string& extension = "");
    
private:
    static std::string m_base_path;
    static bool m_initialized;
    
    // Directory structure constants
    static constexpr const char* DATA_DIR = "data";
    static constexpr const char* CONFIG_DIR = "config";
    static constexpr const char* LOGS_DIR = "logs";
    static constexpr const char* CACHE_DIR = "cache";
    static constexpr const char* MODELS_DIR = "models";
    static constexpr const char* TEMP_DIR = "temp";
    static constexpr const char* SCREENSHOTS_DIR = "screenshots";
    static constexpr const char* OCR_RESULTS_DIR = "ocr_results";
    static constexpr const char* AI_ANALYSIS_DIR = "ai_analysis";
    static constexpr const char* BACKUP_DIR = "backup";
};

} // namespace work_assistant