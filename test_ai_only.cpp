#include "ai_engine.h"
#include "directory_manager.h"
#include <iostream>

using namespace work_assistant;

int main() {
    std::cout << "Testing AI Content Analyzer with real llama.cpp..." << std::endl;
    
    // Initialize directory structure
    if (!DirectoryManager::InitializeDirectories()) {
        std::cerr << "Failed to initialize directories" << std::endl;
        return 1;
    }
    
    // Create AI analyzer
    auto analyzer = std::make_unique<AIContentAnalyzer>();
    
    // Set model path
    std::string model_path = DirectoryManager::JoinPath(DirectoryManager::GetModelsDirectory(), "qwen2.5-0.5b-instruct-q4_k_m.gguf");
    std::cout << "Using model: " << model_path << std::endl;
    
    // Initialize with real llama.cpp
    if (!analyzer->Initialize(model_path, AIEngineFactory::EngineType::LLAMA_CPP)) {
        std::cerr << "Failed to initialize AI analyzer" << std::endl;
        return 1;
    }
    
    std::cout << "AI Content Analyzer initialized successfully!" << std::endl;
    
    // Test content analysis
    OCRDocument test_doc;
    test_doc.full_text = "Writing code for a machine learning project in Python. Working on neural network implementation.";
    test_doc.overall_confidence = 0.95f;
    
    std::cout << "Testing content analysis..." << std::endl;
    ContentAnalysis result = analyzer->AnalyzeWindow(test_doc, "VS Code - main.py", "code");
    
    std::cout << "Analysis Results:" << std::endl;
    std::cout << "  Content Type: " << static_cast<int>(result.content_type) << std::endl;
    std::cout << "  Work Category: " << static_cast<int>(result.work_category) << std::endl;
    std::cout << "  Priority: " << static_cast<int>(result.priority) << std::endl;
    std::cout << "  Application: " << result.application << std::endl;
    
    analyzer->Shutdown();
    std::cout << "Test completed successfully!" << std::endl;
    
    return 0;
}