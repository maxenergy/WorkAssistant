#include "directory_manager.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <ctime>

namespace work_assistant {

// Static member initialization
std::string DirectoryManager::m_base_path = ".";
bool DirectoryManager::m_initialized = false;

bool DirectoryManager::InitializeDirectories(const std::string& base_path) {
    if (!base_path.empty()) {
        m_base_path = base_path;
    }
    
    // Ensure base path exists
    if (!CreateDirectoryIfNotExists(m_base_path)) {
        std::cerr << "Failed to create base directory: " << m_base_path << std::endl;
        return false;
    }
    
    // Create all required subdirectories
    std::vector<std::string> required_dirs = {
        DATA_DIR,
        CONFIG_DIR,
        LOGS_DIR,
        CACHE_DIR,
        MODELS_DIR,
        TEMP_DIR,
        JoinPath(DATA_DIR, SCREENSHOTS_DIR),
        JoinPath(DATA_DIR, OCR_RESULTS_DIR),
        JoinPath(DATA_DIR, AI_ANALYSIS_DIR),
        JoinPath(DATA_DIR, BACKUP_DIR)
    };
    
    bool all_success = true;
    for (const auto& dir : required_dirs) {
        std::string full_path = JoinPath(m_base_path, dir);
        if (!CreateDirectoryIfNotExists(full_path)) {
            std::cerr << "Failed to create directory: " << full_path << std::endl;
            all_success = false;
        } else {
            std::cout << "Created directory: " << full_path << std::endl;
        }
    }
    
    // Verify directories are writable
    for (const auto& dir : required_dirs) {
        std::string full_path = JoinPath(m_base_path, dir);
        if (!EnsureDirectoryWritable(full_path)) {
            std::cerr << "Directory not writable: " << full_path << std::endl;
            all_success = false;
        }
    }
    
    m_initialized = all_success;
    
    if (all_success) {
        std::cout << "Directory structure initialized successfully in: " << m_base_path << std::endl;
    }
    
    return all_success;
}

std::string DirectoryManager::GetDataDirectory() {
    return JoinPath(m_base_path, DATA_DIR);
}

std::string DirectoryManager::GetConfigDirectory() {
    return JoinPath(m_base_path, CONFIG_DIR);
}

std::string DirectoryManager::GetLogsDirectory() {
    return JoinPath(m_base_path, LOGS_DIR);
}

std::string DirectoryManager::GetCacheDirectory() {
    return JoinPath(m_base_path, CACHE_DIR);
}

std::string DirectoryManager::GetModelsDirectory() {
    return JoinPath(m_base_path, MODELS_DIR);
}

std::string DirectoryManager::GetTempDirectory() {
    return JoinPath(m_base_path, TEMP_DIR);
}

std::string DirectoryManager::GetScreenshotsDirectory() {
    return JoinPath(GetDataDirectory(), SCREENSHOTS_DIR);
}

std::string DirectoryManager::GetOCRResultsDirectory() {
    return JoinPath(GetDataDirectory(), OCR_RESULTS_DIR);
}

std::string DirectoryManager::GetAIAnalysisDirectory() {
    return JoinPath(GetDataDirectory(), AI_ANALYSIS_DIR);
}

std::string DirectoryManager::GetBackupDirectory() {
    return JoinPath(GetDataDirectory(), BACKUP_DIR);
}

bool DirectoryManager::CreateDirectoryIfNotExists(const std::string& path) {
    try {
        std::filesystem::path fs_path(path);
        
        if (std::filesystem::exists(fs_path)) {
            return std::filesystem::is_directory(fs_path);
        }
        
        return std::filesystem::create_directories(fs_path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error creating directory " << path << ": " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error creating directory " << path << ": " << e.what() << std::endl;
        return false;
    }
}

bool DirectoryManager::EnsureDirectoryWritable(const std::string& path) {
    try {
        std::filesystem::path fs_path(path);
        
        if (!std::filesystem::exists(fs_path)) {
            return false;
        }
        
        // Try to create a temporary file to test writability
        std::string test_file = JoinPath(path, ".write_test_tmp");
        std::ofstream test_stream(test_file);
        
        if (!test_stream.is_open()) {
            return false;
        }
        
        test_stream << "test" << std::endl;
        test_stream.close();
        
        // Clean up test file
        std::filesystem::remove(test_file);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error testing directory writability " << path << ": " << e.what() << std::endl;
        return false;
    }
}

bool DirectoryManager::CleanupTempFiles(int max_age_hours) {
    try {
        std::string temp_dir = GetTempDirectory();
        if (!std::filesystem::exists(temp_dir)) {
            return true;
        }
        
        auto now = std::chrono::system_clock::now();
        auto max_age = std::chrono::hours(max_age_hours);
        size_t deleted_count = 0;
        
        for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
            if (entry.is_regular_file()) {
                auto file_time = std::filesystem::last_write_time(entry);
                auto file_time_sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    file_time - std::filesystem::file_time_type::clock::now() + now);
                
                if (now - file_time_sys > max_age) {
                    try {
                        std::filesystem::remove(entry);
                        deleted_count++;
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to delete temp file " << entry.path() << ": " << e.what() << std::endl;
                    }
                }
            }
        }
        
        if (deleted_count > 0) {
            std::cout << "Cleaned up " << deleted_count << " temporary files" << std::endl;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error during temp file cleanup: " << e.what() << std::endl;
        return false;
    }
}

bool DirectoryManager::CleanupCacheFiles(size_t max_size_mb) {
    try {
        std::string cache_dir = GetCacheDirectory();
        if (!std::filesystem::exists(cache_dir)) {
            return true;
        }
        
        // Get all files with their sizes and timestamps
        struct FileInfo {
            std::filesystem::path path;
            size_t size;
            std::filesystem::file_time_type last_write;
        };
        
        std::vector<FileInfo> files;
        size_t total_size = 0;
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(cache_dir)) {
            if (entry.is_regular_file()) {
                size_t file_size = entry.file_size();
                files.push_back({
                    entry.path(),
                    file_size,
                    entry.last_write_time()
                });
                total_size += file_size;
            }
        }
        
        size_t max_bytes = max_size_mb * 1024 * 1024;
        if (total_size <= max_bytes) {
            return true; // No cleanup needed
        }
        
        // Sort by last write time (oldest first)
        std::sort(files.begin(), files.end(), 
                  [](const FileInfo& a, const FileInfo& b) {
                      return a.last_write < b.last_write;
                  });
        
        // Delete oldest files until we're under the limit
        size_t deleted_count = 0;
        for (const auto& file : files) {
            if (total_size <= max_bytes) {
                break;
            }
            
            try {
                std::filesystem::remove(file.path);
                total_size -= file.size;
                deleted_count++;
            } catch (const std::exception& e) {
                std::cerr << "Failed to delete cache file " << file.path << ": " << e.what() << std::endl;
            }
        }
        
        if (deleted_count > 0) {
            std::cout << "Cleaned up " << deleted_count << " cache files" << std::endl;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error during cache cleanup: " << e.what() << std::endl;
        return false;
    }
}

std::string DirectoryManager::JoinPath(const std::string& base, const std::string& sub) {
    std::filesystem::path result(base);
    result /= sub;
    return result.string();
}

std::string DirectoryManager::GetUniqueFilename(const std::string& directory, 
                                               const std::string& prefix, 
                                               const std::string& extension) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << prefix << "_";
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    ss << extension;
    
    std::string filename = ss.str();
    std::string full_path = JoinPath(directory, filename);
    
    // Ensure uniqueness by adding a counter if needed
    int counter = 1;
    while (std::filesystem::exists(full_path)) {
        std::stringstream unique_ss;
        unique_ss << prefix << "_";
        unique_ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        unique_ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
        unique_ss << "_" << counter;
        unique_ss << extension;
        
        filename = unique_ss.str();
        full_path = JoinPath(directory, filename);
        counter++;
    }
    
    return filename;
}

bool DirectoryManager::IsValidPath(const std::string& path) {
    try {
        std::filesystem::path fs_path(path);
        
        // Check for invalid characters (basic check)
        std::string path_str = fs_path.string();
        if (path_str.find('\0') != std::string::npos) {
            return false;
        }
        
        // Check if path is absolute or relative is valid
        return !path_str.empty();
    } catch (const std::exception&) {
        return false;
    }
}

DirectoryManager::DirectoryStats DirectoryManager::GetDirectoryStats(const std::string& path) {
    DirectoryStats stats;
    
    try {
        if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
            return stats;
        }
        
        std::filesystem::file_time_type oldest_time = std::filesystem::file_time_type::max();
        std::filesystem::file_time_type newest_time = std::filesystem::file_time_type::min();
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                stats.total_files++;
                stats.total_size_bytes += entry.file_size();
                
                auto write_time = entry.last_write_time();
                if (write_time < oldest_time) {
                    oldest_time = write_time;
                    stats.oldest_file = entry.path().string();
                }
                if (write_time > newest_time) {
                    newest_time = write_time;
                    stats.newest_file = entry.path().string();
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting directory stats for " << path << ": " << e.what() << std::endl;
    }
    
    return stats;
}

std::vector<std::string> DirectoryManager::ListFiles(const std::string& directory, 
                                                     const std::string& extension) {
    std::vector<std::string> files;
    
    try {
        if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
            return files;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                
                if (extension.empty() || 
                    (filename.length() >= extension.length() && 
                     filename.substr(filename.length() - extension.length()) == extension)) {
                    files.push_back(filename);
                }
            }
        }
        
        // Sort files alphabetically
        std::sort(files.begin(), files.end());
    } catch (const std::exception& e) {
        std::cerr << "Error listing files in " << directory << ": " << e.what() << std::endl;
    }
    
    return files;
}

} // namespace work_assistant