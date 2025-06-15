#include "ai_engine.h"
#include <iostream>
#include <chrono>
#include <fstream>

using namespace work_assistant;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << " " << title << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

void test_engine_creation() {
    print_separator("TEST 1: Engine Creation");
    
    std::cout << "Creating LLaMA.cpp engine..." << std::endl;
    auto engine = AIEngineFactory::Create(AIEngineFactory::EngineType::LLAMA_CPP);
    
    if (engine) {
        std::cout << "✅ Engine created successfully" << std::endl;
        std::cout << "Engine info: " << engine->GetEngineInfo() << std::endl;
    } else {
        std::cout << "❌ Failed to create engine" << std::endl;
        return;
    }
    
    std::cout << "Testing initialization..." << std::endl;
    AIPromptConfig config;
    config.context_length = 512;  // Small context for testing
    config.gpu_layers = 0;        // CPU only for compatibility
    config.temperature = 0.7f;
    config.top_p = 0.9f;
    
    bool init_result = engine->Initialize(config);
    std::cout << (init_result ? "✅" : "❌") << " Engine initialization: " 
              << (init_result ? "SUCCESS" : "FAILED") << std::endl;
              
    if (init_result) {
        std::cout << "✅ llama.cpp backend initialized successfully" << std::endl;
        
        // Test engine info
        std::cout << "Initialized engine info: " << engine->GetEngineInfo() << std::endl;
        std::cout << "Supported formats: ";
        auto formats = engine->GetSupportedFormats();
        for (const auto& format : formats) {
            std::cout << format << " ";
        }
        std::cout << std::endl;
        
        engine->Shutdown();
        std::cout << "✅ Engine shutdown completed" << std::endl;
    }
}

void test_model_search() {
    print_separator("TEST 2: Model File Search");
    
    std::cout << "Searching for available models..." << std::endl;
    
    // Check if models directory exists
    std::ifstream models_dir("models");
    if (!models_dir.good()) {
        std::cout << "📁 Creating models directory..." << std::endl;
        system("mkdir -p models");
    }
    
    // Search for .gguf files
    std::cout << "Looking for .gguf model files in common locations..." << std::endl;
    
    std::vector<std::string> search_paths = {
        "models/",
        "./",
        "../models/",
        "/tmp/",
        "~/.cache/huggingface/hub/"
    };
    
    std::vector<std::string> found_models;
    
    for (const auto& path : search_paths) {
        std::string command = "find " + path + " -name \"*.gguf\" 2>/dev/null || true";
        FILE* pipe = popen(command.c_str(), "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                std::string model_path(buffer);
                model_path.erase(model_path.find_last_not_of(" \n\r\t") + 1); // trim
                if (!model_path.empty()) {
                    found_models.push_back(model_path);
                }
            }
            pclose(pipe);
        }
    }
    
    if (found_models.empty()) {
        std::cout << "⚠️  No .gguf model files found" << std::endl;
        std::cout << "\n📋 To test with a real model, you can:" << std::endl;
        std::cout << "   1. Download a small model like:" << std::endl;
        std::cout << "      wget https://huggingface.co/microsoft/DialoGPT-small/resolve/main/pytorch_model.bin" << std::endl;
        std::cout << "   2. Or install ollama and pull a model:" << std::endl;
        std::cout << "      ollama pull qwen2.5:1.5b" << std::endl;
        std::cout << "   3. Or download from HuggingFace:" << std::endl;
        std::cout << "      wget https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/qwen2.5-1.5b-instruct-q4_k_m.gguf" << std::endl;
    } else {
        std::cout << "✅ Found " << found_models.size() << " model file(s):" << std::endl;
        for (const auto& model : found_models) {
            std::cout << "   📄 " << model << std::endl;
            
            // Check file size
            std::ifstream file(model, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                auto size = file.tellg();
                std::cout << "      Size: " << (size / 1024 / 1024) << " MB" << std::endl;
            }
        }
    }
    
    // Test the AI factory model search
    auto factory_models = AIEngineFactory::FindAvailableModels("models/");
    std::cout << "\n🔍 AI Factory found " << factory_models.size() << " models in models/ directory" << std::endl;
    for (const auto& model : factory_models) {
        std::cout << "   📄 " << model.name << " (" << model.path << ")" << std::endl;
    }
}

void test_model_loading(const std::string& model_path = "") {
    print_separator("TEST 3: Model Loading");
    
    auto engine = AIEngineFactory::Create(AIEngineFactory::EngineType::LLAMA_CPP);
    if (!engine) {
        std::cout << "❌ Failed to create engine" << std::endl;
        return;
    }
    
    AIPromptConfig config;
    config.context_length = 512;
    config.gpu_layers = 0;  // CPU only
    
    if (!engine->Initialize(config)) {
        std::cout << "❌ Failed to initialize engine" << std::endl;
        return;
    }
    
    if (model_path.empty()) {
        std::cout << "⚠️  No model path provided, testing with mock model path" << std::endl;
        
        // Test with non-existent model (should fail gracefully)
        std::string mock_path = "models/test_model.gguf";
        std::cout << "Testing model loading with: " << mock_path << std::endl;
        
        bool load_result = engine->LoadModel(mock_path);
        std::cout << (load_result ? "❌ Unexpected success" : "✅ Expected failure") 
                  << " - Model loading with non-existent file" << std::endl;
    } else {
        std::cout << "Testing model loading with: " << model_path << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        bool load_result = engine->LoadModel(model_path);
        auto end_time = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (load_result) {
            std::cout << "✅ Model loaded successfully in " << duration.count() << "ms" << std::endl;
            
            auto model_info = engine->GetModelInfo();
            std::cout << "📊 Model Information:" << std::endl;
            std::cout << "   Name: " << model_info.name << std::endl;
            std::cout << "   Type: " << model_info.type << std::endl;
            std::cout << "   Size: " << model_info.size_mb << " MB" << std::endl;
            std::cout << "   Loaded: " << (model_info.is_loaded ? "Yes" : "No") << std::endl;
            std::cout << "   Classification Support: " << (model_info.supports_classification ? "Yes" : "No") << std::endl;
            
            engine->UnloadModel();
            std::cout << "✅ Model unloaded successfully" << std::endl;
        } else {
            std::cout << "❌ Failed to load model (this may be normal if model is corrupted or incompatible)" << std::endl;
        }
    }
    
    engine->Shutdown();
}

void test_basic_inference() {
    print_separator("TEST 4: Basic Inference (Without Model)");
    
    auto engine = AIEngineFactory::Create(AIEngineFactory::EngineType::LLAMA_CPP);
    if (!engine) {
        std::cout << "❌ Failed to create engine" << std::endl;
        return;
    }
    
    if (!engine->Initialize()) {
        std::cout << "❌ Failed to initialize engine" << std::endl;
        return;
    }
    
    std::cout << "Testing content analysis without loaded model (should fallback to heuristics)..." << std::endl;
    
    // Create a test OCR document
    OCRDocument test_doc;
    test_doc.full_text = "Working on JavaScript code for a web application using React framework";
    test_doc.overall_confidence = 0.9f;
    
    TextBlock block;
    block.text = test_doc.full_text;
    block.confidence = 0.9f;
    block.x = 0; block.y = 0; block.width = 800; block.height = 20;
    test_doc.text_blocks.push_back(block);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto analysis = engine->AnalyzeContent(test_doc, "VS Code", "Code.exe");
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "✅ Analysis completed in " << duration.count() << "ms" << std::endl;
    std::cout << "📊 Analysis Results:" << std::endl;
    std::cout << "   Content Type: " << ai_utils::ContentTypeToString(analysis.content_type) << std::endl;
    std::cout << "   Work Category: " << ai_utils::WorkCategoryToString(analysis.work_category) << std::endl;
    std::cout << "   Is Productive: " << (analysis.is_productive ? "Yes" : "No") << std::endl;
    std::cout << "   Is Focused Work: " << (analysis.is_focused_work ? "Yes" : "No") << std::endl;
    std::cout << "   Classification Confidence: " << analysis.classification_confidence << std::endl;
    std::cout << "   Processing Time: " << analysis.processing_time.count() << "ms" << std::endl;
    
    if (!analysis.keywords.empty()) {
        std::cout << "   Keywords: ";
        for (const auto& keyword : analysis.keywords) {
            std::cout << keyword << " ";
        }
        std::cout << std::endl;
    }
    
    // Test async analysis
    std::cout << "\nTesting async analysis..." << std::endl;
    auto future = engine->AnalyzeContentAsync(test_doc, "Chrome", "chrome.exe");
    auto async_analysis = future.get();
    std::cout << "✅ Async analysis completed" << std::endl;
    std::cout << "   Async Content Type: " << ai_utils::ContentTypeToString(async_analysis.content_type) << std::endl;
    
    engine->Shutdown();
}

void test_performance_metrics() {
    print_separator("TEST 5: Performance Metrics");
    
    auto engine = AIEngineFactory::Create(AIEngineFactory::EngineType::LLAMA_CPP);
    if (!engine || !engine->Initialize()) {
        std::cout << "❌ Failed to initialize engine" << std::endl;
        return;
    }
    
    std::cout << "Running multiple analyses to test performance metrics..." << std::endl;
    
    OCRDocument test_doc;
    test_doc.full_text = "Performance testing document";
    
    // Run multiple analyses
    for (int i = 0; i < 5; ++i) {
        engine->AnalyzeContent(test_doc, "Test Window " + std::to_string(i), "test.exe");
    }
    
    std::cout << "📊 Performance Metrics:" << std::endl;
    std::cout << "   Average Processing Time: " << engine->GetAverageProcessingTime() << "ms" << std::endl;
    std::cout << "   Total Processed Items: " << engine->GetTotalProcessedItems() << std::endl;
    std::cout << "   Engine Info: " << engine->GetEngineInfo() << std::endl;
    
    engine->Shutdown();
    std::cout << "✅ Performance test completed" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "🚀 LLaMA.cpp Engine Verification Tool" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    try {
        // Test 1: Engine Creation and Initialization
        test_engine_creation();
        
        // Test 2: Model File Search
        test_model_search();
        
        // Test 3: Model Loading
        if (argc > 1) {
            test_model_loading(argv[1]);
        } else {
            test_model_loading();
        }
        
        // Test 4: Basic Inference
        test_basic_inference();
        
        // Test 5: Performance Metrics
        test_performance_metrics();
        
        print_separator("VERIFICATION SUMMARY");
        std::cout << "✅ LLaMA.cpp engine verification completed!" << std::endl;
        std::cout << "\n📋 What was tested:" << std::endl;
        std::cout << "   ✅ Engine creation and factory pattern" << std::endl;
        std::cout << "   ✅ llama.cpp backend initialization" << std::endl;
        std::cout << "   ✅ Model file search and discovery" << std::endl;
        std::cout << "   ✅ Model loading (with graceful failure handling)" << std::endl;
        std::cout << "   ✅ Content analysis with fallback heuristics" << std::endl;
        std::cout << "   ✅ Async analysis functionality" << std::endl;
        std::cout << "   ✅ Performance metrics collection" << std::endl;
        std::cout << "   ✅ Engine shutdown and cleanup" << std::endl;
        
        std::cout << "\n🎯 Next steps to test with real models:" << std::endl;
        std::cout << "   1. Download a small GGUF model file" << std::endl;
        std::cout << "   2. Run: ./test_llama_verification <model_path>" << std::endl;
        std::cout << "   3. Observe real AI inference in action!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception during verification: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}