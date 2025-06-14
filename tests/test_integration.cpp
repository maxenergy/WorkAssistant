#include "application.h"
#include "storage_engine.h"
#include "ai_engine.h"
#include "web_server.h"
#include <iostream>
#include <thread>
#include <chrono>

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

bool test_storage_ai_integration() {
    // Test integration between storage and AI systems
    auto storage = std::make_shared<EncryptedStorageManager>();
    
    StorageConfig config;
    config.storage_path = "integration_test_data";
    config.database_name = "integration_test.db";
    config.master_password = "integration_test_pass";
    config.security_level = SecurityLevel::STANDARD;
    
    if (!storage->Initialize(config)) {
        return false;
    }
    
    storage->StartSession("integration_test");
    
    AIContentAnalyzer analyzer;
    if (!analyzer.Initialize()) {
        storage->Shutdown();
        return false;
    }
    
    // Create test content analysis
    OCRDocument doc;
    doc.full_text = "Integration test: working on C++ programming";
    doc.overall_confidence = 0.9f;
    
    auto analysis = analyzer.AnalyzeWindow(doc, "Integration Test", "test.exe");
    
    // Store analysis result
    bool stored = storage->StoreAIAnalysis(analysis);
    
    // Retrieve and verify
    auto start = std::chrono::system_clock::now() - std::chrono::hours(1);
    auto end = std::chrono::system_clock::now() + std::chrono::hours(1);
    auto retrieved = storage->GetContentAnalyses(start, end);
    
    bool found = !retrieved.empty();
    
    // Cleanup
    analyzer.Shutdown();
    storage->Shutdown();
    
    try {
        std::filesystem::remove_all("integration_test_data");
    } catch (...) {
        // Ignore cleanup errors
    }
    
    return stored && found;
}

bool test_web_storage_integration() {
    // Test web server with storage backend
    auto storage = std::make_shared<EncryptedStorageManager>();
    
    StorageConfig storage_config;
    storage_config.storage_path = "web_test_data";
    storage_config.database_name = "web_test.db";
    storage_config.master_password = "web_test_pass";
    storage_config.security_level = SecurityLevel::STANDARD;
    
    if (!storage->Initialize(storage_config)) {
        return false;
    }
    
    WebServer web_server;
    WebServerConfig web_config;
    web_config.host = "127.0.0.1";
    web_config.port = 8081; // Different port to avoid conflicts
    web_config.static_files_path = "web/static";
    
    bool web_initialized = web_server.Initialize(web_config, storage);
    
    // Test basic web server functionality
    bool web_ready = web_server.IsRunning() == false; // Should not be running yet
    
    // Test configuration
    auto retrieved_config = web_server.GetConfig();
    bool config_match = (retrieved_config.host == web_config.host && 
                        retrieved_config.port == web_config.port);
    
    // Cleanup
    web_server.Shutdown();
    storage->Shutdown();
    
    try {
        std::filesystem::remove_all("web_test_data");
    } catch (...) {
        // Ignore cleanup errors
    }
    
    return web_initialized && web_ready && config_match;
}

bool test_full_pipeline() {
    // Test the complete pipeline: OCR -> AI -> Storage
    auto storage = std::make_shared<EncryptedStorageManager>();
    
    StorageConfig config;
    config.storage_path = "pipeline_test_data";
    config.database_name = "pipeline_test.db";
    config.master_password = "pipeline_test_pass";
    config.security_level = SecurityLevel::STANDARD;
    
    if (!storage->Initialize(config)) {
        return false;
    }
    
    storage->StartSession("pipeline_test");
    
    AIContentAnalyzer analyzer;
    if (!analyzer.Initialize()) {
        storage->Shutdown();
        return false;
    }
    
    // Simulate OCR result
    OCRDocument ocr_doc;
    ocr_doc.full_text = "def calculate_fibonacci(n): return n if n <= 1 else calculate_fibonacci(n-1) + calculate_fibonacci(n-2)";
    ocr_doc.overall_confidence = 0.95f;
    
    TextBlock block;
    block.text = ocr_doc.full_text;
    block.confidence = 0.95f;
    block.x = 10;
    block.y = 50;
    block.width = 500;
    block.height = 20;
    ocr_doc.text_blocks.push_back(block);
    
    // Store OCR result
    bool ocr_stored = storage->StoreOCRResult(ocr_doc, "Pipeline Test");
    
    // Process with AI
    auto analysis = analyzer.AnalyzeWindow(ocr_doc, "Code Editor", "python.exe");
    
    // Store AI analysis
    bool ai_stored = storage->StoreAIAnalysis(analysis);
    
    // Generate productivity report
    auto start = std::chrono::system_clock::now() - std::chrono::hours(1);
    auto end = std::chrono::system_clock::now() + std::chrono::hours(1);
    auto report = storage->GetProductivityReport(start, end);
    
    bool has_report = !report.empty();
    
    // Test search functionality
    auto search_results = storage->SearchContent("fibonacci", 10);
    bool found_content = !search_results.empty();
    
    // Cleanup
    analyzer.Shutdown();
    storage->Shutdown();
    
    try {
        std::filesystem::remove_all("pipeline_test_data");
    } catch (...) {
        // Ignore cleanup errors
    }
    
    return ocr_stored && ai_stored && has_report && found_content;
}

bool test_concurrent_operations() {
    // Test concurrent access to storage system
    auto storage = std::make_shared<EncryptedStorageManager>();
    
    StorageConfig config;
    config.storage_path = "concurrent_test_data";
    config.database_name = "concurrent_test.db";
    config.master_password = "concurrent_test_pass";
    config.security_level = SecurityLevel::STANDARD;
    
    if (!storage->Initialize(config)) {
        return false;
    }
    
    storage->StartSession("concurrent_test");
    
    std::atomic<int> successful_operations{0};
    const int num_threads = 4;
    const int operations_per_thread = 5;
    
    std::vector<std::thread> threads;
    
    // Launch concurrent operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&storage, &successful_operations, t, operations_per_thread]() {
            AIContentAnalyzer analyzer;
            analyzer.Initialize();
            
            for (int i = 0; i < operations_per_thread; ++i) {
                try {
                    // Create unique test data
                    OCRDocument doc;
                    doc.full_text = "Concurrent test " + std::to_string(t) + "_" + std::to_string(i);
                    doc.overall_confidence = 0.8f;
                    
                    auto analysis = analyzer.AnalyzeWindow(doc, "Test App", "test.exe");
                    
                    if (storage->StoreAIAnalysis(analysis)) {
                        successful_operations++;
                    }
                    
                    // Small delay to increase chance of concurrent access
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    
                } catch (...) {
                    // Count failures but don't crash
                }
            }
            
            analyzer.Shutdown();
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Check results
    int expected_operations = num_threads * operations_per_thread;
    bool success = (successful_operations >= expected_operations * 0.8); // Allow some failures
    
    storage->Shutdown();
    
    try {
        std::filesystem::remove_all("concurrent_test_data");
    } catch (...) {
        // Ignore cleanup errors
    }
    
    return success;
}

bool test_error_handling() {
    // Test system behavior under error conditions
    
    // Test invalid storage configuration
    EncryptedStorageManager storage;
    StorageConfig invalid_config;
    // Leave config mostly empty/invalid
    invalid_config.master_password = ""; // Invalid password
    
    bool invalid_init = !storage.Initialize(invalid_config); // Should fail
    
    // Test AI analyzer with invalid content
    AIContentAnalyzer analyzer;
    analyzer.Initialize();
    
    OCRDocument empty_doc; // Empty document
    auto analysis = analyzer.AnalyzeWindow(empty_doc, "", "");
    
    // Should handle gracefully and return something
    bool handled_empty = (analysis.content_type != ContentType::UNKNOWN || 
                         analysis.work_category != WorkCategory::UNKNOWN);
    
    analyzer.Shutdown();
    
    return invalid_init && handled_empty;
}

int main() {
    TestFramework framework;
    
    std::cout << "=== Integration Tests ===" << std::endl;
    
    // Run integration tests
    framework.run_test("Storage-AI Integration", test_storage_ai_integration);
    framework.run_test("Web-Storage Integration", test_web_storage_integration);
    framework.run_test("Full Pipeline Test", test_full_pipeline);
    framework.run_test("Concurrent Operations", test_concurrent_operations);
    framework.run_test("Error Handling", test_error_handling);
    
    return framework.summary();
}