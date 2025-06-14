#include "storage_engine.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace work_assistant;

// Simple test framework
class TestFramework {
private:
    int tests_run = 0;
    int tests_passed = 0;
    
public:
    void run_test(const std::string& name, std::function<bool()> test_func) {
        tests_run++;
        std::cout << "Running test: " << name << "... ";
        
        try {
            if (test_func()) {
                tests_passed++;
                std::cout << "PASSED" << std::endl;
            } else {
                std::cout << "FAILED" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "FAILED (Exception: " << e.what() << ")" << std::endl;
        }
    }
    
    int summary() {
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Tests run: " << tests_run << std::endl;
        std::cout << "Tests passed: " << tests_passed << std::endl;
        std::cout << "Tests failed: " << (tests_run - tests_passed) << std::endl;
        
        if (tests_passed == tests_run) {
            std::cout << "All tests PASSED!" << std::endl;
            return 0;
        } else {
            std::cout << "Some tests FAILED!" << std::endl;
            return 1;
        }
    }
};

// Test utility functions
bool test_storage_utils() {
    // Test encryption/decryption
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    std::string password = "test_password";
    std::string salt = "test_salt";
    
    auto encrypted = storage_utils::EncryptData(data, password, salt);
    auto decrypted = storage_utils::DecryptData(encrypted, password, salt);
    
    return data == decrypted;
}

bool test_compression() {
    std::vector<uint8_t> data = {'A', 'A', 'A', 'B', 'B', 'C'};
    
    auto compressed = storage_utils::CompressData(data);
    auto decompressed = storage_utils::DecompressData(compressed);
    
    return data == decompressed;
}

bool test_checksum() {
    std::vector<uint8_t> data = {'t', 'e', 's', 't'};
    
    std::string checksum1 = storage_utils::CalculateChecksum(data);
    std::string checksum2 = storage_utils::CalculateChecksum(data);
    
    // Same data should produce same checksum
    if (checksum1 != checksum2) return false;
    
    // Verify checksum
    return storage_utils::VerifyChecksum(data, checksum1);
}

bool test_timestamp_formatting() {
    auto now = std::chrono::system_clock::now();
    std::string formatted = storage_utils::FormatTimestamp(now);
    
    // Should contain basic timestamp components
    return formatted.find("T") != std::string::npos && 
           formatted.find("Z") != std::string::npos;
}

bool test_session_id_generation() {
    std::string id1 = storage_utils::GenerateSessionId();
    std::string id2 = storage_utils::GenerateSessionId();
    
    // IDs should be different and contain expected prefix
    return id1 != id2 && 
           id1.find("session_") == 0 && 
           id2.find("session_") == 0;
}

bool test_directory_operations() {
    std::string test_dir = "test_storage_dir";
    
    // Test directory creation
    bool created = storage_utils::EnsureDirectoryExists(test_dir);
    
    // Test path validation
    std::string test_db_path = test_dir + "/test.db";
    bool valid = storage_utils::IsValidDatabasePath(test_db_path);
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (...) {
        // Ignore cleanup errors
    }
    
    return created && valid;
}

bool test_storage_config() {
    StorageConfig config;
    config.storage_path = "test_data";
    config.database_name = "test.db";
    config.master_password = "test_password";
    config.security_level = SecurityLevel::STANDARD;
    
    return config.IsValid();
}

bool test_encrypted_storage_manager() {
    // Create temporary storage config
    StorageConfig config;
    config.storage_path = "test_storage";
    config.database_name = "test_manager.db";
    config.master_password = "test_password_123";
    config.security_level = SecurityLevel::STANDARD;
    
    EncryptedStorageManager manager;
    
    // Test initialization
    if (!manager.Initialize(config)) {
        return false;
    }
    
    // Test session management
    if (!manager.StartSession("test_session")) {
        return false;
    }
    
    std::string session_id = manager.GetCurrentSessionId();
    if (session_id.empty()) {
        return false;
    }
    
    // Test basic operations
    bool ready = manager.IsReady();
    
    // Clean shutdown
    manager.EndSession();
    manager.Shutdown();
    
    // Cleanup test files
    try {
        std::filesystem::remove_all("test_storage");
    } catch (...) {
        // Ignore cleanup errors
    }
    
    return ready;
}

bool test_data_record_operations() {
    DataRecord record;
    record.type = RecordType::OCR_RESULT;
    record.session_id = "test_session";
    record.metadata["source"] = "test";
    record.SetStringData("test data");
    record.checksum = storage_utils::CalculateChecksum(record.data);
    
    // Test serialization
    std::string json = storage_utils::SerializeToJson(
        *reinterpret_cast<const ContentAnalysisRecord*>(&record)
    );
    
    return !json.empty() && json.find("test_session") != std::string::npos;
}

int main() {
    TestFramework framework;
    
    std::cout << "=== Storage System Tests ===" << std::endl;
    
    // Run utility tests
    framework.run_test("Storage Utils - Encryption/Decryption", test_storage_utils);
    framework.run_test("Storage Utils - Compression", test_compression);
    framework.run_test("Storage Utils - Checksum", test_checksum);
    framework.run_test("Storage Utils - Timestamp Formatting", test_timestamp_formatting);
    framework.run_test("Storage Utils - Session ID Generation", test_session_id_generation);
    framework.run_test("Storage Utils - Directory Operations", test_directory_operations);
    
    // Run storage tests
    framework.run_test("Storage Config Validation", test_storage_config);
    framework.run_test("Encrypted Storage Manager", test_encrypted_storage_manager);
    framework.run_test("Data Record Operations", test_data_record_operations);
    
    return framework.summary();
}