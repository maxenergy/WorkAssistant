#include "ai_engine.h"
#include "directory_manager.h"
#include <iostream>
#include <fstream>

using namespace work_assistant;

int main() {
    std::cout << "Testing AI model loading specifically..." << std::endl;
    
    // Initialize directory structure
    if (!DirectoryManager::InitializeDirectories()) {
        std::cerr << "Failed to initialize directories" << std::endl;
        return 1;
    }
    
    // Create AI analyzer
    auto analyzer = std::make_unique<AIContentAnalyzer>();
    
    // Set model path - use absolute path
    std::string model_path = "./models/qwen2.5-0.5b-instruct-q4_k_m.gguf";
    std::cout << "Using model: " << model_path << std::endl;
    
    // Check if model file exists
    std::ifstream file(model_path);
    if (!file.good()) {
        std::cerr << "Model file does not exist or is not accessible: " << model_path << std::endl;
        return 1;
    }
    file.close();
    std::cout << "Model file exists and is accessible" << std::endl;
    
    // Initialize with real llama.cpp
    std::cout << "Initializing AI analyzer..." << std::endl;
    if (!analyzer->Initialize(model_path, AIEngineFactory::EngineType::LLAMA_CPP)) {
        std::cerr << "Failed to initialize AI analyzer" << std::endl;
        return 1;
    }
    
    std::cout << "AI Content Analyzer initialized successfully!" << std::endl;
    
    // Test content analysis
    OCRDocument test_doc;
    test_doc.full_text = "Writing code for a machine learning project in Python.";
    test_doc.overall_confidence = 0.95f;
    
    std::cout << "Testing content analysis with loaded model..." << std::endl;
    ContentAnalysis result = analyzer->AnalyzeWindow(test_doc, "VS Code - main.py", "code");
    
    std::cout << "Analysis Results:" << std::endl;
    std::cout << "  Content Type: " << static_cast<int>(result.content_type) << std::endl;
    std::cout << "  Work Category: " << static_cast<int>(result.work_category) << std::endl;
    std::cout << "  Priority: " << static_cast<int>(result.priority) << std::endl;
    
    analyzer->Shutdown();
    std::cout << "Test completed successfully!" << std::endl;
    
    return 0;
}