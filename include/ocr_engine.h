#pragma once

#include "screen_capture.h"
#include <string>
#include <vector>
#include <memory>
#include <future>

namespace work_assistant {

// OCR result structure
struct OCRResult {
    std::string text;
    float confidence;
    int x, y, width, height;  // Bounding box
    
    OCRResult() : confidence(0.0f), x(0), y(0), width(0), height(0) {}
    OCRResult(const std::string& txt, float conf, int bx, int by, int bw, int bh)
        : text(txt), confidence(conf), x(bx), y(by), width(bw), height(bh) {}
};

// OCR document structure - collection of text blocks
struct OCRDocument {
    std::vector<OCRResult> text_blocks;
    std::string full_text;
    float overall_confidence;
    std::chrono::system_clock::time_point timestamp;
    
    OCRDocument() : overall_confidence(0.0f) {}
    
    // Combine all text blocks into full text
    void CombineText();
    
    // Filter results by confidence threshold
    std::vector<OCRResult> GetHighConfidenceResults(float threshold = 0.7f) const;
    
    // Get text in reading order (top-to-bottom, left-to-right)
    std::string GetOrderedText() const;
};

// OCR preprocessing options
struct OCROptions {
    bool auto_preprocess = true;        // Apply automatic image preprocessing
    float scale_factor = 1.0f;          // Scale image before OCR (1.0 = no scaling)
    bool denoise = true;                // Apply denoising
    bool enhance_contrast = true;       // Enhance contrast
    bool binarize = true;               // Convert to black/white
    float confidence_threshold = 0.5f;  // Minimum confidence for results
    std::string language = "eng";       // Tesseract language (eng, chi_sim, etc.)
    bool preserve_whitespace = true;    // Preserve whitespace in output
    
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

// OCR engine factory
class OCREngineFactory {
public:
    enum class EngineType {
        TESSERACT,      // Tesseract OCR
        WINDOWS_OCR,    // Windows.Media.Ocr (Windows 10+)
        CLOUD_VISION    // Future: Google Vision, Azure, etc.
    };
    
    static std::unique_ptr<IOCREngine> Create(EngineType type = EngineType::TESSERACT);
    static std::vector<EngineType> GetAvailableEngines();
};

// OCR manager - handles multiple engines and content extraction
class OCRManager {
public:
    OCRManager();
    ~OCRManager();

    // Initialize with preferred engine
    bool Initialize(OCREngineFactory::EngineType engineType = OCREngineFactory::EngineType::TESSERACT);
    void Shutdown();

    // Process content from screen captures
    OCRDocument ExtractText(const CaptureFrame& frame);
    std::future<OCRDocument> ExtractTextAsync(const CaptureFrame& frame);

    // Process specific window content
    OCRDocument ExtractWindowText(uint64_t windowHandle);

    // Content analysis
    bool ContainsText(const CaptureFrame& frame, const std::string& searchText);
    std::vector<std::string> ExtractKeywords(const OCRDocument& document);
    
    // Configuration
    void SetLanguage(const std::string& language);
    void SetConfidenceThreshold(float threshold);
    void EnablePreprocessing(bool enable);

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