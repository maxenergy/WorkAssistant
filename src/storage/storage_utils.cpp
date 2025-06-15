#include "storage_engine.h"
#include "common_types.h"
#include "ocr_engine.h"
#include "ai_engine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <filesystem>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

namespace work_assistant {
namespace storage_utils {

// Mock encryption functions (in production, use libsodium or similar)
std::vector<uint8_t> EncryptData(const std::vector<uint8_t>& data,
                                const std::string& password,
                                const std::string& salt) {
    // Simple XOR encryption for demonstration
    // In production, use AES-256-GCM or similar
    std::vector<uint8_t> encrypted = data;

    // Generate key from password (simplified)
    std::hash<std::string> hasher;
    size_t key = hasher(password + salt);

    for (size_t i = 0; i < encrypted.size(); ++i) {
        encrypted[i] ^= static_cast<uint8_t>((key >> (i % 8)) & 0xFF);
    }

    return encrypted;
}

std::vector<uint8_t> DecryptData(const std::vector<uint8_t>& encrypted_data,
                                const std::string& password,
                                const std::string& salt) {
    // Simple XOR decryption (same as encryption for XOR)
    return EncryptData(encrypted_data, password, salt);
}

// Mock compression functions
std::vector<uint8_t> CompressData(const std::vector<uint8_t>& data) {
    // Simple run-length encoding for demonstration
    // In production, use zlib, lz4, or similar
    std::vector<uint8_t> compressed;
    compressed.reserve(data.size());

    if (data.empty()) {
        return compressed;
    }

    uint8_t current = data[0];
    int count = 1;

    for (size_t i = 1; i < data.size(); ++i) {
        if (data[i] == current && count < 255) {
            count++;
        } else {
            compressed.push_back(static_cast<uint8_t>(count));
            compressed.push_back(current);
            current = data[i];
            count = 1;
        }
    }

    // Add the last run
    compressed.push_back(static_cast<uint8_t>(count));
    compressed.push_back(current);

    return compressed;
}

std::vector<uint8_t> DecompressData(const std::vector<uint8_t>& compressed_data) {
    std::vector<uint8_t> decompressed;

    for (size_t i = 0; i < compressed_data.size(); i += 2) {
        if (i + 1 < compressed_data.size()) {
            uint8_t count = compressed_data[i];
            uint8_t value = compressed_data[i + 1];

            for (int j = 0; j < count; ++j) {
                decompressed.push_back(value);
            }
        }
    }

    return decompressed;
}

// Data integrity functions
std::string CalculateChecksum(const std::vector<uint8_t>& data) {
    // Simple SHA-256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.data(), data.size());
    SHA256_Final(hash, &sha256);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return oss.str();
}

bool VerifyChecksum(const std::vector<uint8_t>& data, const std::string& checksum) {
    std::string calculated = CalculateChecksum(data);
    return calculated == checksum;
}

// Serialization functions
std::string SerializeToJson(const ContentAnalysisRecord& record) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"id\": " << record.id << ",\n";
    json << "  \"timestamp\": \"" << FormatTimestamp(record.timestamp) << "\",\n";
    json << "  \"session_id\": \"" << record.session_id << "\",\n";
    json << "  \"window_title\": \"" << record.window_title << "\",\n";
    json << "  \"application_name\": \"" << record.application_name << "\",\n";
    json << "  \"extracted_text\": \"" << record.extracted_text << "\",\n";
    json << "  \"ocr_confidence\": " << record.ocr_confidence << ",\n";
    json << "  \"content_type\": " << static_cast<int>(record.content_type) << ",\n";
    json << "  \"work_category\": " << static_cast<int>(record.work_category) << ",\n";
    json << "  \"priority\": " << static_cast<int>(record.priority) << ",\n";
    json << "  \"is_productive\": " << (record.is_productive ? "true" : "false") << ",\n";
    json << "  \"is_focused_work\": " << (record.is_focused_work ? "true" : "false") << ",\n";
    json << "  \"ai_confidence\": " << record.ai_confidence << ",\n";
    json << "  \"distraction_level\": " << record.distraction_level << ",\n";
    json << "  \"processing_time_ms\": " << record.processing_time.count() << ",\n";
    json << "  \"keywords\": [";
    for (size_t i = 0; i < record.keywords.size(); ++i) {
        if (i > 0) json << ", ";
        json << "\"" << record.keywords[i] << "\"";
    }
    json << "]\n";
    json << "}";
    return json.str();
}

ContentAnalysisRecord DeserializeFromJson(const std::string& json) {
    ContentAnalysisRecord record;

    // Simple JSON parsing (in production, use a proper JSON library)
    // This is a mock implementation that assumes well-formed JSON

    // Extract basic fields using string operations
    auto find_value = [&json](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\": ";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";

        pos += search.length();
        if (json[pos] == '"') {
            // String value
            pos++;
            size_t end = json.find('"', pos);
            if (end != std::string::npos) {
                return json.substr(pos, end - pos);
            }
        } else {
            // Numeric value
            size_t end = json.find_first_of(",\n}", pos);
            if (end != std::string::npos) {
                return json.substr(pos, end - pos);
            }
        }
        return "";
    };

    // Parse fields
    try {
        record.id = std::stoull(find_value("id"));
        record.session_id = find_value("session_id");
        record.window_title = find_value("window_title");
        record.application_name = find_value("application_name");
        record.extracted_text = find_value("extracted_text");
        record.ocr_confidence = std::stof(find_value("ocr_confidence"));
        record.content_type = static_cast<ContentType>(std::stoi(find_value("content_type")));
        record.work_category = static_cast<WorkCategory>(std::stoi(find_value("work_category")));
        record.priority = static_cast<ActivityPriority>(std::stoi(find_value("priority")));
        record.is_productive = (find_value("is_productive") == "true");
        record.is_focused_work = (find_value("is_focused_work") == "true");
        record.ai_confidence = std::stof(find_value("ai_confidence"));
        record.distraction_level = std::stoi(find_value("distraction_level"));
        record.processing_time = std::chrono::milliseconds(std::stoll(find_value("processing_time_ms")));

        // Parse timestamp
        record.timestamp = ParseTimestamp(find_value("timestamp"));
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
    }

    return record;
}

std::string SerializeToJson(const WindowActivityRecord& record) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"id\": " << record.id << ",\n";
    json << "  \"timestamp\": \"" << FormatTimestamp(record.timestamp) << "\",\n";
    json << "  \"window_title\": \"" << record.window_title << "\",\n";
    json << "  \"application_name\": \"" << record.application_name << "\",\n";
    json << "  \"process_id\": " << record.process_id << ",\n";
    json << "  \"event_type\": \"" << record.event_type << "\",\n";
    json << "  \"x\": " << record.x << ",\n";
    json << "  \"y\": " << record.y << ",\n";
    json << "  \"width\": " << record.width << ",\n";
    json << "  \"height\": " << record.height << ",\n";
    json << "  \"duration_ms\": " << record.duration.count() << "\n";
    json << "}";
    return json.str();
}

WindowActivityRecord DeserializeWindowFromJson(const std::string& json) {
    WindowActivityRecord record;

    // Simple JSON parsing (mock implementation)
    auto find_value = [&json](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\": ";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";

        pos += search.length();
        if (json[pos] == '"') {
            pos++;
            size_t end = json.find('"', pos);
            if (end != std::string::npos) {
                return json.substr(pos, end - pos);
            }
        } else {
            size_t end = json.find_first_of(",\n}", pos);
            if (end != std::string::npos) {
                return json.substr(pos, end - pos);
            }
        }
        return "";
    };

    try {
        record.id = std::stoull(find_value("id"));
        record.window_title = find_value("window_title");
        record.application_name = find_value("application_name");
        record.process_id = std::stoul(find_value("process_id"));
        record.event_type = find_value("event_type");
        record.x = std::stoi(find_value("x"));
        record.y = std::stoi(find_value("y"));
        record.width = std::stoi(find_value("width"));
        record.height = std::stoi(find_value("height"));
        record.duration = std::chrono::milliseconds(std::stoll(find_value("duration_ms")));
        record.timestamp = ParseTimestamp(find_value("timestamp"));
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
    }

    return record;
}

// File operation utilities
bool EnsureDirectoryExists(const std::string& path) {
    try {
        if (std::filesystem::exists(path)) {
            return std::filesystem::is_directory(path);
        }
        return std::filesystem::create_directories(path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create directory " << path << ": " << e.what() << std::endl;
        return false;
    }
}

bool IsValidDatabasePath(const std::string& path) {
    std::filesystem::path db_path(path);

    // Check if parent directory exists or can be created
    auto parent = db_path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        if (!EnsureDirectoryExists(parent.string())) {
            return false;
        }
    }

    // Check write permissions
    try {
        std::ofstream test_file(path, std::ios::app);
        if (!test_file.is_open()) {
            return false;
        }
        test_file.close();

        // If file was created just for testing, remove it
        if (std::filesystem::file_size(path) == 0) {
            std::filesystem::remove(path);
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

size_t GetDirectorySize(const std::string& path) {
    size_t size = 0;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                size += entry.file_size();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error calculating directory size: " << e.what() << std::endl;
    }
    return size;
}

bool SecureDeleteFile(const std::string& path) {
    try {
        // Overwrite file with random data before deletion
        std::ifstream file_check(path);
        if (!file_check.is_open()) {
            return false; // File doesn't exist
        }
        file_check.close();

        size_t file_size = std::filesystem::file_size(path);

        std::ofstream file(path, std::ios::binary | std::ios::out);
        if (!file.is_open()) {
            return false;
        }

        // Overwrite with random data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> dis(0, 255);

        for (size_t i = 0; i < file_size; ++i) {
            file.put(static_cast<char>(dis(gen)));
        }
        file.close();

        // Now delete the file
        return std::filesystem::remove(path);
    } catch (const std::exception& e) {
        std::cerr << "Secure delete failed: " << e.what() << std::endl;
        return false;
    }
}

// Time utilities
std::string FormatTimestamp(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';

    return oss.str();
}

std::chrono::system_clock::time_point ParseTimestamp(const std::string& timestamp) {
    // Simple ISO 8601 parsing (mock implementation)
    // In production, use a proper date/time library

    std::istringstream iss(timestamp);
    std::tm tm = {};

    if (timestamp.size() >= 19) {
        // Parse YYYY-MM-DDTHH:MM:SS
        iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

        if (!iss.fail()) {
            auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));

            // Parse milliseconds if present
            if (timestamp.size() > 20 && timestamp[19] == '.') {
                std::string ms_str = timestamp.substr(20, 3);
                try {
                    int ms = std::stoi(ms_str);
                    time_point += std::chrono::milliseconds(ms);
                } catch (...) {
                    // Ignore milliseconds parsing errors
                }
            }

            return time_point;
        }
    }

    // Return current time if parsing fails
    return std::chrono::system_clock::now();
}

std::string GenerateSessionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    uint32_t random_part = dis(gen);

    std::ostringstream oss;
    oss << "session_" << std::hex << timestamp << "_" << random_part;
    return oss.str();
}

} // namespace storage_utils
} // namespace work_assistant