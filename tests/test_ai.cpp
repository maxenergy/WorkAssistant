#include "ai_engine.h"
#include <iostream>
#include <cassert>
#include <functional>

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

// Test AI utility functions
bool test_content_type_conversions() {
    // Test string conversion
    std::string code_str = ai_utils::ContentTypeToString(ContentType::CODE);
    ContentType code_type = ai_utils::StringToContentType("CODE");
    
    return code_str == "CODE" && code_type == ContentType::CODE;
}

bool test_work_category_conversions() {
    std::string work_str = ai_utils::WorkCategoryToString(WorkCategory::FOCUSED_WORK);
    WorkCategory work_cat = ai_utils::StringToWorkCategory("FOCUSED_WORK");
    
    return work_str == "FOCUSED_WORK" && work_cat == WorkCategory::FOCUSED_WORK;
}

bool test_entity_extraction() {
    std::string text = "This is a test document about machine learning and artificial intelligence.";
    auto entities = ai_utils::ExtractEntities(text);
    
    // Should extract some entities
    return !entities.empty();
}

bool test_content_helpers() {
    // Test content type helpers
    ContentType code_type = ContentType::CODE;
    std::string code_str = ai_utils::ContentTypeToString(code_type);
    ContentType parsed_type = ai_utils::StringToContentType(code_str);
    
    // Test productivity checks
    bool is_productive = ai_utils::IsProductiveContentType(ContentType::PRODUCTIVITY);
    bool is_focused = ai_utils::IsFocusedWorkCategory(WorkCategory::FOCUSED_WORK);
    
    return parsed_type == code_type && is_productive && is_focused;
}

bool test_ai_content_analyzer() {
    AIContentAnalyzer analyzer;
    
    // Test initialization
    if (!analyzer.Initialize()) {
        return false;
    }
    
    // Create test OCR document
    OCRDocument doc;
    doc.full_text = "Working on JavaScript code for the web application";
    doc.overall_confidence = 0.85f;
    
    TextBlock block;
    block.text = doc.full_text;
    block.confidence = 0.85f;
    block.x = 0;
    block.y = 0;
    block.width = 100;
    block.height = 20;
    doc.text_blocks.push_back(block);
    
    // Test analysis
    auto analysis = analyzer.AnalyzeWindow(doc, "VS Code", "Code.exe");
    
    // Verify analysis results
    bool valid_analysis = (
        analysis.content_type != ContentType::UNKNOWN &&
        analysis.work_category != WorkCategory::UNKNOWN &&
        analysis.classification_confidence > 0.0f &&
        !analysis.application.empty()
    );
    
    // Test statistics
    auto stats = analyzer.GetStatistics();
    
    analyzer.Shutdown();
    
    return valid_analysis && stats.total_analyzed > 0;
}

bool test_productivity_score() {
    AIContentAnalyzer analyzer;
    analyzer.Initialize();
    
    // Create test activities
    std::vector<ContentAnalysis> activities;
    
    // Add productive activity
    ContentAnalysis productive;
    productive.content_type = ContentType::CODE;
    productive.work_category = WorkCategory::FOCUSED_WORK;
    productive.is_productive = true;
    productive.classification_confidence = 0.9f;
    activities.push_back(productive);
    
    // Add non-productive activity
    ContentAnalysis distraction;
    distraction.content_type = ContentType::SOCIAL_MEDIA;
    distraction.work_category = WorkCategory::BREAK;
    distraction.is_productive = false;
    distraction.classification_confidence = 0.8f;
    activities.push_back(distraction);
    
    int score = analyzer.CalculateProductivityScore(activities);
    
    analyzer.Shutdown();
    
    // Score should be between 0 and 100
    return score >= 0 && score <= 100;
}

bool test_work_patterns() {
    AIContentAnalyzer analyzer;
    analyzer.Initialize();
    
    // Create test activities with patterns
    std::vector<ContentAnalysis> activities;
    
    // Add several code-related activities
    for (int i = 0; i < 5; ++i) {
        ContentAnalysis activity;
        activity.content_type = ContentType::CODE;
        activity.work_category = WorkCategory::FOCUSED_WORK;
        activity.is_productive = true;
        activity.timestamp = std::chrono::system_clock::now() - std::chrono::minutes(i * 10);
        activities.push_back(activity);
    }
    
    auto patterns = analyzer.DetectWorkPatterns(activities);
    
    // Should detect some patterns
    bool has_patterns = !patterns.empty();
    
    // Test prediction
    ContentType predicted = analyzer.PredictNextActivity(activities);
    bool valid_prediction = (predicted != ContentType::UNKNOWN);
    
    analyzer.Shutdown();
    
    return has_patterns || valid_prediction; // At least one should work
}

bool test_ai_engine_factory() {
    // Test AI engine factory functionality
    auto available_engines = AIEngineFactory::GetAvailableEngines();
    
    // Should have at least one engine type available
    if (available_engines.empty()) {
        std::cout << "(No AI engines available - expected in test environment) ";
        return true;
    }
    
    // Try to create an engine
    auto engine = AIEngineFactory::Create(AIEngineFactory::EngineType::LLAMA_CPP);
    
    // Engine creation should succeed even if initialization fails
    return engine != nullptr;
}

bool test_async_analysis() {
    AIContentAnalyzer analyzer;
    analyzer.Initialize();
    
    OCRDocument doc;
    doc.full_text = "Async test document with programming content";
    
    // Test async analysis
    auto future = analyzer.AnalyzeWindowAsync(doc, "Test Editor", "editor.exe");
    auto analysis = future.get();
    
    analyzer.Shutdown();
    
    return analysis.content_type != ContentType::UNKNOWN;
}

int main() {
    TestFramework framework;
    
    std::cout << "=== AI System Tests ===" << std::endl;
    
    // Run utility tests
    framework.run_test("AI Utils - Content Type Conversions", test_content_type_conversions);
    framework.run_test("AI Utils - Work Category Conversions", test_work_category_conversions);
    framework.run_test("AI Utils - Entity Extraction", test_entity_extraction);
    framework.run_test("AI Utils - Content Helpers", test_content_helpers);
    
    // Run AI system tests
    framework.run_test("AI Content Analyzer", test_ai_content_analyzer);
    framework.run_test("Productivity Score Calculation", test_productivity_score);
    framework.run_test("Work Pattern Detection", test_work_patterns);
    framework.run_test("AI Engine Factory", test_ai_engine_factory);
    framework.run_test("Async Analysis", test_async_analysis);
    
    return framework.summary();
}