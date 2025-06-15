#include "include/ai_engine.h"
#include <iostream>
#include <chrono>

using namespace work_assistant;

int main() {
    std::cout << "🚀 Testing real LLaMA.cpp model integration" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    // Test model path
    std::string model_path = "models/qwen2.5-0.5b-instruct-q4_k_m.gguf";
    
    // Create and initialize engine
    auto engine = AIEngineFactory::Create(AIEngineFactory::EngineType::LLAMA_CPP);
    if (!engine) {
        std::cout << "❌ Failed to create LLaMA engine" << std::endl;
        return 1;
    }
    
    AIPromptConfig config;
    config.context_length = 1024;
    config.gpu_layers = 0;  // CPU only
    config.temperature = 0.7f;
    config.top_p = 0.9f;
    
    std::cout << "📥 Initializing engine..." << std::endl;
    if (!engine->Initialize(config)) {
        std::cout << "❌ Failed to initialize engine" << std::endl;
        return 1;
    }
    std::cout << "✅ Engine initialized: " << engine->GetEngineInfo() << std::endl;
    
    // Test model loading
    std::cout << "📥 Loading model: " << model_path << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    bool load_success = engine->LoadModel(model_path);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (load_success) {
        std::cout << "✅ Model loaded successfully in " << duration.count() << "ms" << std::endl;
        
        // Get model info
        auto model_info = engine->GetModelInfo();
        std::cout << "📊 Model Information:" << std::endl;
        std::cout << "   Name: " << model_info.name << std::endl;
        std::cout << "   Type: " << model_info.type << std::endl;
        std::cout << "   Size: " << model_info.size_mb << " MB" << std::endl;
        std::cout << "   Loaded: " << (model_info.is_loaded ? "Yes" : "No") << std::endl;
        std::cout << "   Classification Support: " << (model_info.supports_classification ? "Yes" : "No") << std::endl;
        
        // Test real AI content analysis
        std::cout << "🧠 Testing real AI analysis..." << std::endl;
        
        OCRDocument test_doc;
        test_doc.full_text = "import tensorflow as tf\\nfrom sklearn import datasets\\n\\n# Load dataset\\ndata = datasets.load_iris()\\nX, y = data.data, data.target\\n\\n# Build model\\nmodel = tf.keras.Sequential([\\n    tf.keras.layers.Dense(10, activation='relu'),\\n    tf.keras.layers.Dense(3, activation='softmax')\\n])";
        test_doc.overall_confidence = 0.95f;
        
        TextBlock block;
        block.text = test_doc.full_text;
        block.confidence = 0.95f;
        block.x = 0; block.y = 0; block.width = 1000; block.height = 200;
        test_doc.text_blocks.push_back(block);
        
        auto analysis_start = std::chrono::high_resolution_clock::now();
        auto analysis = engine->AnalyzeContent(test_doc, "PyCharm", "pycharm64.exe");
        auto analysis_end = std::chrono::high_resolution_clock::now();
        
        auto analysis_duration = std::chrono::duration_cast<std::chrono::milliseconds>(analysis_end - analysis_start);
        
        std::cout << "✅ Real AI analysis completed in " << analysis_duration.count() << "ms" << std::endl;
        std::cout << "📊 Analysis Results:" << std::endl;
        std::cout << "   Content Type: " << ai_utils::ContentTypeToString(analysis.content_type) << std::endl;
        std::cout << "   Work Category: " << ai_utils::WorkCategoryToString(analysis.work_category) << std::endl;
        std::cout << "   Is Productive: " << (analysis.is_productive ? "Yes" : "No") << std::endl;
        std::cout << "   Is Focused Work: " << (analysis.is_focused_work ? "Yes" : "No") << std::endl;
        std::cout << "   Classification Confidence: " << analysis.classification_confidence << std::endl;
        std::cout << "   Processing Time: " << analysis.processing_time.count() << "ms" << std::endl;
        
        if (!analysis.keywords.empty()) {
            std::cout << "   AI-Generated Keywords: ";
            for (const auto& keyword : analysis.keywords) {
                std::cout << keyword << " ";
            }
            std::cout << std::endl;
        }
        
        // Test with another example - web content
        std::cout << "\\n🌐 Testing with web browsing content..." << std::endl;
        
        OCRDocument web_doc;
        web_doc.full_text = "Stack Overflow - How to optimize React performance\\nAnswered questions about useMemo, useCallback, and React.memo";
        web_doc.overall_confidence = 0.88f;
        
        TextBlock web_block;
        web_block.text = web_doc.full_text;
        web_block.confidence = 0.88f;
        web_block.x = 0; web_block.y = 0; web_block.width = 1200; web_block.height = 50;
        web_doc.text_blocks.push_back(web_block);
        
        auto web_analysis = engine->AnalyzeContent(web_doc, "Chrome", "chrome.exe");
        std::cout << "✅ Web content analysis:" << std::endl;
        std::cout << "   Content Type: " << ai_utils::ContentTypeToString(web_analysis.content_type) << std::endl;
        std::cout << "   Work Category: " << ai_utils::WorkCategoryToString(web_analysis.work_category) << std::endl;
        std::cout << "   Is Productive: " << (web_analysis.is_productive ? "Yes" : "No") << std::endl;
        
        // Performance metrics
        std::cout << "\\n📊 Performance Metrics:" << std::endl;
        std::cout << "   Average Processing Time: " << engine->GetAverageProcessingTime() << "ms" << std::endl;
        std::cout << "   Total Processed Items: " << engine->GetTotalProcessedItems() << std::endl;
        
        engine->UnloadModel();
        std::cout << "✅ Model unloaded successfully" << std::endl;
        
    } else {
        std::cout << "❌ Failed to load model" << std::endl;
    }
    
    engine->Shutdown();
    std::cout << "🎉 Real LLaMA.cpp test completed!" << std::endl;
    
    return 0;
}