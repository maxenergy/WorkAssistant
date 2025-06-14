#pragma once

#include "ocr_engine.h"
#include "common_types.h"
#include <memory>
#include <string>
#include <vector>

// llama.cpp availability check  
#ifdef LLAMA_CPP_DIR
    // Include llama.cpp headers when available
    #include "llama.h"
    #define LLAMA_CPP_AVAILABLE 1
#else
    #define LLAMA_CPP_AVAILABLE 0
#endif

namespace work_assistant {

// MiniCPM-V model configuration
struct MiniCPMVConfig {
    // Model paths
    std::string model_path = "models/minicpm-v/minicpm-v-2.0-q4_k_m.gguf";
    std::string tokenizer_path = "models/minicpm-v/tokenizer.json";
    
    // Model parameters
    int context_length = 2048;           // Context window size
    float temperature = 0.3f;            // Sampling temperature
    float top_p = 0.9f;                  // Top-p sampling
    int max_tokens = 512;                // Maximum output tokens
    
    // Performance parameters
    bool use_gpu = true;                 // Use GPU acceleration
    int gpu_layers = 32;                 // Number of GPU layers (-1 for all)
    int batch_size = 1;                  // Batch processing size
    int threads = 4;                     // CPU thread count
    
    // Image processing
    int max_image_size = 768;            // Maximum image dimension
    bool auto_resize = true;             // Auto resize large images
    
    // OCR specific
    std::string ocr_prompt_template = "Extract all text from this image. Output only the text content, preserve formatting and layout.";
    std::string qa_prompt_template = "Based on the image content, answer the following question: {question}";
    
    MiniCPMVConfig() = default;
};

// MiniCPM-V multimodal response
struct MultimodalResponse {
    std::string text_content;            // Extracted text or response
    float confidence = 0.0f;             // Response confidence
    std::chrono::milliseconds processing_time{0};
    std::vector<std::string> detected_elements;  // Additional detected elements
    
    // Structured data extraction results
    std::unordered_map<std::string, std::string> structured_data;
};

// MiniCPM-V Engine implementation  
class MiniCPMVEngine : public IOCREngine {
public:
    MiniCPMVEngine();
    ~MiniCPMVEngine() override;

    // IOCREngine interface
    bool Initialize(const OCROptions& options = OCROptions()) override;
    void Shutdown() override;
    
    OCRDocument ProcessImage(const CaptureFrame& frame) override;
    OCRDocument ProcessImageRegion(const CaptureFrame& frame,
                                  int x, int y, int width, int height) override;
    std::future<OCRDocument> ProcessImageAsync(const CaptureFrame& frame) override;
    
    void SetOptions(const OCROptions& options) override;
    OCROptions GetOptions() const override;
    std::vector<std::string> GetSupportedLanguages() const override;
    
    bool IsInitialized() const override;
    std::string GetEngineInfo() const override;

    // MiniCPM-V specific multimodal methods
    bool InitializeMiniCPM(const MiniCPMVConfig& config);
    void SetMiniCPMConfig(const MiniCPMVConfig& config);
    MiniCPMVConfig GetMiniCPMConfig() const;
    
    // Advanced multimodal capabilities
    MultimodalResponse AnswerQuestion(const CaptureFrame& frame, const std::string& question);
    MultimodalResponse DescribeImage(const CaptureFrame& frame);
    MultimodalResponse ExtractStructuredData(const CaptureFrame& frame, const std::string& data_type);
    
    // Batch processing
    std::vector<OCRDocument> ProcessImageBatch(const std::vector<CaptureFrame>& frames);
    std::vector<MultimodalResponse> AnswerQuestionBatch(
        const std::vector<CaptureFrame>& frames, 
        const std::vector<std::string>& questions);
    
    // Model management
    bool LoadModel(const std::string& model_path);
    void UnloadModel();
    bool IsModelLoaded() const;
    
    // Performance monitoring
    struct Statistics {
        size_t total_inferences = 0;
        size_t ocr_requests = 0;
        size_t qa_requests = 0;
        size_t description_requests = 0;
        double avg_inference_time_ms = 0.0;
        double avg_image_processing_ms = 0.0;
        size_t total_tokens_generated = 0;
        double tokens_per_second = 0.0;
        size_t gpu_memory_used_mb = 0;
    };
    
    Statistics GetStatistics() const;
    void ResetStatistics();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// MiniCPM-V utility functions
namespace minicpm_utils {

// Image preprocessing for vision model
bool PrepareImageForModel(const CaptureFrame& input, CaptureFrame& output, int target_size);
bool ValidateImageSize(const CaptureFrame& frame, int max_size);
std::vector<uint8_t> EncodeImageForModel(const CaptureFrame& frame);

// Prompt engineering
std::string BuildOCRPrompt(const std::string& language = "auto");
std::string BuildQAPrompt(const std::string& question);
std::string BuildStructuredExtractionPrompt(const std::string& data_type);

// Response processing
OCRDocument ParseOCRResponse(const std::string& response);
std::vector<TextBlock> ExtractTextBlocks(const std::string& response);
std::unordered_map<std::string, std::string> ParseStructuredData(
    const std::string& response, const std::string& data_type);

// Model utilities
bool DownloadMiniCPMModel(const std::string& model_name, const std::string& target_dir);
bool ValidateModelFile(const std::string& model_path);
size_t EstimateModelMemoryUsage(const std::string& model_path);
std::vector<std::string> GetAvailableModels();

// Performance optimization
std::vector<CaptureFrame> OptimizeBatchSize(const std::vector<CaptureFrame>& frames, int max_batch_size);
bool ShouldUseGPU(const MiniCPMVConfig& config);
int CalculateOptimalGPULayers(const MiniCPMVConfig& config, size_t available_vram_mb);

} // namespace minicpm_utils

} // namespace work_assistant