#pragma once

#include "common_types.h"
#include "screen_capture.h"
#include <string>
#include <vector>
#include <memory>
#include <future>

namespace work_assistant {

// OCR preprocessing options
struct OCROptions {
    // General options
    bool auto_preprocess = true;        // Apply automatic image preprocessing
    float scale_factor = 1.0f;          // Scale image before OCR (1.0 = no scaling)
    bool denoise = true;                // Apply denoising
    bool enhance_contrast = true;       // Enhance contrast
    bool binarize = true;               // Convert to black/white
    float confidence_threshold = 0.5f;  // Minimum confidence for results
    std::string language = "eng";       // Language (eng, chi_sim, etc.)
    bool preserve_whitespace = true;    // Preserve whitespace in output
    
    // Mode-specific options
    OCRMode preferred_mode = OCRMode::AUTO;  // Preferred processing mode
    bool enable_multimodal = true;      // Enable multimodal capabilities when available
    bool use_gpu = true;                // Use GPU acceleration when available
    int max_image_size = 2048;          // Maximum image dimension for processing
    
    // Performance options
    bool enable_caching = true;         // Enable result caching
    int cache_ttl_seconds = 300;        // Cache time-to-live
    bool batch_processing = false;      // Enable batch processing optimization
    
    OCROptions() = default;
};

// OCR engine interface
class IOCREngine {
public:
    virtual ~IOCREngine() = default;

    // Initialize OCR engine
    virtual bool Initialize(const OCROptions& options = OCROptions()) = 0;
    virtual void Shutdown() = 0;

    // Synchronous OCR processing
    virtual OCRDocument ProcessImage(const CaptureFrame& frame) = 0;
    virtual OCRDocument ProcessImageRegion(const CaptureFrame& frame, 
                                          int x, int y, int width, int height) = 0;

    // Asynchronous OCR processing
    virtual std::future<OCRDocument> ProcessImageAsync(const CaptureFrame& frame) = 0;

    // Configuration
    virtual void SetOptions(const OCROptions& options) = 0;
    virtual OCROptions GetOptions() const = 0;

    // Supported languages
    virtual std::vector<std::string> GetSupportedLanguages() const = 0;
    
    // Performance info
    virtual bool IsInitialized() const = 0;
    virtual std::string GetEngineInfo() const = 0;
};

// OCR processing modes
enum class OCRMode {
    FAST,           // 快速模式：PaddleOCR v4
    ACCURATE,       // 高精度模式：MiniCPM-V 2.0
    MULTIMODAL,     // 多模态理解：MiniCPM-V 2.0 + 问答
    AUTO            // 智能选择
};

// OCR engine factory
class OCREngineFactory {
public:
    enum class EngineType {
        TESSERACT,      // Tesseract OCR (Legacy)
        PADDLE_OCR,     // PaddleOCR v4 (轻量级)
        MINICPM_V,      // MiniCPM-V 2.0 (多模态)
        AUTO_SELECT     // 自动选择最佳引擎
    };
    
    static std::unique_ptr<IOCREngine> Create(EngineType type = EngineType::TESSERACT);
    static std::vector<EngineType> GetAvailableEngines();
};

// OCR manager - handles multiple engines and content extraction
class OCRManager {
public:
    OCRManager();
    ~OCRManager();

    // Initialize with preferred engine or mode
    bool Initialize(OCREngineFactory::EngineType engineType = OCREngineFactory::EngineType::AUTO_SELECT);
    bool Initialize(OCRMode mode = OCRMode::AUTO);
    
    // Switch OCR mode dynamically
    bool SetOCRMode(OCRMode mode);
    OCRMode GetCurrentMode() const;
    void Shutdown();

    // Process content from screen captures
    OCRDocument ExtractText(const CaptureFrame& frame);
    std::future<OCRDocument> ExtractTextAsync(const CaptureFrame& frame);

    // Process specific window content
    OCRDocument ExtractWindowText(WindowHandle windowHandle);

    // Content analysis
    bool ContainsText(const CaptureFrame& frame, const std::string& searchText);
    std::vector<std::string> ExtractKeywords(const OCRDocument& document);
    
    // Multimodal capabilities (MiniCPM-V only)
    std::string AnswerQuestion(const CaptureFrame& frame, const std::string& question);
    std::string DescribeImage(const CaptureFrame& frame);
    std::vector<std::string> ExtractStructuredData(const CaptureFrame& frame, const std::string& dataType);
    
    // Configuration
    void SetLanguage(const std::string& language);
    void SetConfidenceThreshold(float threshold);
    void EnablePreprocessing(bool enable);
    void SetOptions(const OCROptions& options);
    OCROptions GetOptions() const;
    
    // Performance and resource management
    void EnableGPU(bool enable);
    void SetMaxImageSize(int max_size);
    void EnableCaching(bool enable, int ttl_seconds = 300);

    // Performance monitoring
    struct Statistics {
        size_t total_processed = 0;
        size_t successful_extractions = 0;
        double average_processing_time_ms = 0.0;
        double average_confidence = 0.0;
    };
    
    Statistics GetStatistics() const;
    void ResetStatistics();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// Utility functions for image preprocessing
namespace ocr_utils {

// Image preprocessing functions
bool PreprocessImage(const CaptureFrame& input, CaptureFrame& output, const OCROptions& options);
bool ScaleImage(const CaptureFrame& input, CaptureFrame& output, float scale);
bool ConvertToGrayscale(const CaptureFrame& input, CaptureFrame& output);
bool EnhanceContrast(CaptureFrame& frame, float factor = 1.5f);
bool DenoiseImage(CaptureFrame& frame);
bool BinarizeImage(CaptureFrame& frame, int threshold = 128);

// Text processing utilities
std::string CleanExtractedText(const std::string& text);
std::vector<std::string> SplitIntoLines(const std::string& text);
std::vector<std::string> ExtractWords(const std::string& text);
bool IsTextMeaningful(const std::string& text, float min_ratio = 0.6f);

// Language detection
std::string DetectLanguage(const std::string& text);
bool IsLikelyCode(const std::string& text);
bool IsLikelyEmail(const std::string& text);
bool IsLikelyURL(const std::string& text);

} // namespace ocr_utils

} // namespace work_assistant