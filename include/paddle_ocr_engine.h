#pragma once

#include "ocr_engine.h"
#include "common_types.h"
#include <memory>
#include <string>
#include <vector>

// Conditional compilation based on PaddlePaddle availability
#ifndef PADDLE_NOT_AVAILABLE
    // Include PaddlePaddle headers when available
    // These will be included when PaddlePaddle is properly installed
    // #include "paddle_inference_api.h" 
    #define PADDLE_AVAILABLE 1
#else
    #define PADDLE_AVAILABLE 0
#endif

namespace work_assistant {

// PaddleOCR detection result
struct PaddleDetectionResult {
    std::vector<std::vector<int>> boxes;  // Detection boxes
    std::vector<float> scores;            // Detection confidence scores
};

// PaddleOCR recognition result  
struct PaddleRecognitionResult {
    std::vector<std::string> texts;       // Recognized texts
    std::vector<float> scores;            // Recognition confidence scores
};

// PaddleOCR configuration
struct PaddleOCRConfig {
    // Model paths
    std::string det_model_path = "models/paddle_ocr/det_model";
    std::string rec_model_path = "models/paddle_ocr/rec_model";
    std::string cls_model_path = "models/paddle_ocr/cls_model";
    
    // Detection parameters
    int max_side_len = 960;              // Maximum image side length
    float det_db_thresh = 0.3f;          // Detection threshold
    float det_db_box_thresh = 0.6f;      // Box threshold
    float det_db_unclip_ratio = 1.5f;    // Unclip ratio
    
    // Recognition parameters
    int rec_batch_num = 6;               // Recognition batch size
    std::string rec_char_dict_path = "models/paddle_ocr/ppocr_keys_v1.txt";
    
    // Classification parameters
    float cls_thresh = 0.9f;             // Classification threshold
    
    // System parameters
    bool use_gpu = false;                // Use GPU acceleration
    int gpu_id = 0;                      // GPU device ID
    int cpu_threads = 4;                 // CPU thread count
    bool enable_mkldnn = false;          // Enable MKL-DNN
    
    PaddleOCRConfig() = default;
};

// PaddleOCR Engine implementation
class PaddleOCREngine : public IOCREngine {
public:
    PaddleOCREngine();
    ~PaddleOCREngine() override;

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

    // PaddleOCR specific methods
    bool InitializePaddle(const PaddleOCRConfig& config);
    void SetPaddleConfig(const PaddleOCRConfig& config);
    PaddleOCRConfig GetPaddleConfig() const;
    
    // Performance monitoring
    struct Statistics {
        size_t total_processed = 0;
        double avg_detection_time_ms = 0.0;
        double avg_recognition_time_ms = 0.0;
        double avg_total_time_ms = 0.0;
        size_t cache_hits = 0;
        size_t cache_misses = 0;
    };
    
    Statistics GetStatistics() const;
    void ResetStatistics();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// PaddleOCR utility functions
namespace paddle_utils {

// Image preprocessing
bool PreprocessImage(const CaptureFrame& input, CaptureFrame& output);
bool ResizeImage(const CaptureFrame& input, CaptureFrame& output, int target_size);
bool NormalizeImage(CaptureFrame& frame);

// Result processing
OCRDocument ConvertToOCRDocument(const PaddleDetectionResult& det_result,
                                const PaddleRecognitionResult& rec_result);
std::vector<TextBlock> MergeTextBlocks(const std::vector<TextBlock>& blocks);
std::string OrderTextByPosition(const std::vector<TextBlock>& blocks);

// Model management
bool DownloadPaddleModels(const std::string& model_dir);
bool ValidatePaddleModel(const std::string& model_path);
std::vector<std::string> GetAvailableLanguages();

} // namespace paddle_utils

} // namespace work_assistant