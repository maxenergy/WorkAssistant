#pragma once

#include "common_types.h"
#include "ocr_engine.h"
#include <string>
#include <vector>
#include <memory>
#include <future>
#include <chrono>
#include <unordered_map>

namespace work_assistant {

// AI prompt templates and configuration
struct AIPromptConfig {
    std::string system_prompt;
    std::string classification_prompt;
    std::string priority_prompt;
    std::string category_prompt;
    
    // Model parameters
    int max_tokens = 512;
    float temperature = 0.3f;  // Lower for more consistent classification
    float top_p = 0.9f;
    int repeat_penalty = 1.1f;
    
    // Performance settings
    int context_length = 2048;
    bool use_gpu = true;
    int gpu_layers = 32;
    
    AIPromptConfig();
    void LoadDefaultPrompts();
};

// AI model information
struct AIModelInfo {
    std::string name;
    std::string path;
    std::string type;        // "gguf", "ggml", etc.
    size_t size_mb = 0;
    bool is_loaded = false;
    bool supports_classification = false;
    
    // Performance characteristics
    float avg_tokens_per_second = 0.0f;
    int recommended_context = 2048;
    int min_gpu_memory_mb = 0;
    
    AIModelInfo() = default;
    AIModelInfo(const std::string& model_name, const std::string& model_path)
        : name(model_name), path(model_path) {}
};

// AI engine interface
class IAIEngine {
public:
    virtual ~IAIEngine() = default;

    // Initialization and lifecycle
    virtual bool Initialize(const AIPromptConfig& config = AIPromptConfig()) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    // Model management
    virtual bool LoadModel(const std::string& model_path) = 0;
    virtual void UnloadModel() = 0;
    virtual AIModelInfo GetModelInfo() const = 0;
    virtual std::vector<std::string> GetSupportedFormats() const = 0;

    // Content analysis
    virtual ContentAnalysis AnalyzeContent(const OCRDocument& ocr_result, 
                                          const std::string& window_title = "",
                                          const std::string& app_name = "") = 0;
    
    virtual std::future<ContentAnalysis> AnalyzeContentAsync(const OCRDocument& ocr_result,
                                                            const std::string& window_title = "",
                                                            const std::string& app_name = "") = 0;

    // Batch processing
    virtual std::vector<ContentAnalysis> AnalyzeBatch(const std::vector<OCRDocument>& documents) = 0;

    // Direct text analysis (for testing or when OCR is not needed)
    virtual ContentAnalysis AnalyzeText(const std::string& text,
                                       const std::string& context = "") = 0;

    // Configuration
    virtual void UpdateConfig(const AIPromptConfig& config) = 0;
    virtual AIPromptConfig GetConfig() const = 0;

    // Performance monitoring
    virtual float GetAverageProcessingTime() const = 0;
    virtual size_t GetTotalProcessedItems() const = 0;
    virtual std::string GetEngineInfo() const = 0;
};

// AI engine factory
class AIEngineFactory {
public:
    enum class EngineType {
        LLAMA_CPP,      // llama.cpp integration
        OLLAMA,         // Ollama API
        OPENAI_API,     // OpenAI API (future)
        LOCAL_TRANSFORMER // Custom transformer (future)
    };
    
    static std::unique_ptr<IAIEngine> Create(EngineType type = EngineType::LLAMA_CPP);
    static std::vector<EngineType> GetAvailableEngines();
    static std::vector<AIModelInfo> FindAvailableModels(const std::string& models_dir = "models/");
};

// AI content analyzer - main interface for the application
class AIContentAnalyzer {
public:
    AIContentAnalyzer();
    ~AIContentAnalyzer();

    // Initialization
    bool Initialize(const std::string& model_path = "", 
                   AIEngineFactory::EngineType engine_type = AIEngineFactory::EngineType::LLAMA_CPP);
    void Shutdown();
    bool IsReady() const;

    // Main analysis functions
    ContentAnalysis AnalyzeWindow(const OCRDocument& ocr_result,
                                 const std::string& window_title,
                                 const std::string& app_name);

    std::future<ContentAnalysis> AnalyzeWindowAsync(const OCRDocument& ocr_result,
                                                    const std::string& window_title,
                                                    const std::string& app_name);

    // Productivity analysis
    bool IsProductiveActivity(const ContentAnalysis& analysis) const;
    int CalculateProductivityScore(const std::vector<ContentAnalysis>& recent_activities) const;
    
    // Pattern detection
    std::vector<std::string> DetectWorkPatterns(const std::vector<ContentAnalysis>& activities) const;
    ContentType PredictNextActivity(const std::vector<ContentAnalysis>& recent_activities) const;

    // Configuration and tuning
    void SetProductivityThresholds(float min_focused_ratio = 0.6f, 
                                  float max_distraction_level = 3.0f);
    void UpdatePrompts(const AIPromptConfig& config);
    void EnableLearning(bool enable = true);  // Future: learn from user feedback

    // Statistics and monitoring
    struct Statistics {
        size_t total_analyzed = 0;
        size_t successful_classifications = 0;
        double average_processing_time_ms = 0.0;
        double average_confidence = 0.0;
        std::unordered_map<ContentType, size_t> type_counts;
        std::unordered_map<WorkCategory, size_t> category_counts;
    };
    
    Statistics GetStatistics() const;
    void ResetStatistics();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// Utility functions for AI analysis
namespace ai_utils {

// Content type helpers
std::string ContentTypeToString(ContentType type);
ContentType StringToContentType(const std::string& str);
std::string WorkCategoryToString(WorkCategory category);
WorkCategory StringToWorkCategory(const std::string& str);
std::string ActivityPriorityToString(ActivityPriority priority);

// Analysis helpers
bool IsProductiveContentType(ContentType type);
bool IsFocusedWorkCategory(WorkCategory category);
float CalculateProductivityScore(const ContentAnalysis& analysis);
std::vector<std::string> ExtractEntities(const std::string& text);

// Model utilities
bool ValidateModelFile(const std::string& model_path);
size_t EstimateModelMemoryUsage(const std::string& model_path);
std::vector<std::string> GetRecommendedModels();

// Prompt engineering helpers
std::string BuildClassificationPrompt(const std::string& text, 
                                     const std::string& window_title,
                                     const std::string& app_name);
std::string ParseClassificationResponse(const std::string& response);

} // namespace ai_utils

} // namespace work_assistant