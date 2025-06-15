#include "ocr_engine.h"
#include "paddle_ocr_engine.h"
#include "minicpm_v_engine.h"
#include <iostream>
#include <algorithm>
#include <regex>
#include <thread>
#include <chrono>
#include <sstream>
#include <set>

namespace work_assistant {

// OCR Engine Factory implementation with dual-mode support
std::unique_ptr<IOCREngine> OCREngineFactory::Create(EngineType type) {
    switch (type) {
        case EngineType::PADDLE_OCR:
            return std::make_unique<PaddleOCREngine>();
        case EngineType::MINICPM_V:
            return std::make_unique<MiniCPMVEngine>();
        case EngineType::AUTO_SELECT: {
            // Try MiniCPM-V first (if GPU available), fallback to PaddleOCR
            auto minicpm = std::make_unique<MiniCPMVEngine>();
            OCROptions options;
            options.use_gpu = true;
            if (minicpm->Initialize(options)) {
                std::cout << "Auto-selected MiniCPM-V engine" << std::endl;
                return minicpm;
            }
            
            std::cout << "Falling back to PaddleOCR engine" << std::endl;
            return std::make_unique<PaddleOCREngine>();
        }
        case EngineType::TESSERACT:
        default:
            std::cerr << "Legacy Tesseract engine deprecated, using PaddleOCR" << std::endl;
            return std::make_unique<PaddleOCREngine>();
    }
}

std::vector<OCREngineFactory::EngineType> OCREngineFactory::GetAvailableEngines() {
    return {
        EngineType::PADDLE_OCR,
        EngineType::MINICPM_V, 
        EngineType::AUTO_SELECT
    };
}

// Dual-mode OCR Manager implementation
class OCRManager::Impl {
public:
    Impl() : m_initialized(false), m_current_mode(OCRMode::AUTO), m_statistics{} {}

    bool Initialize(OCREngineFactory::EngineType engineType) {
        if (m_initialized) {
            return true;
        }

        // Initialize primary engine
        m_primary_engine = OCREngineFactory::Create(engineType);
        if (!m_primary_engine) {
            std::cerr << "Failed to create primary OCR engine" << std::endl;
            return false;
        }

        OCROptions options;
        options.language = "eng";
        options.confidence_threshold = 0.6f;
        options.auto_preprocess = true;
        options.use_gpu = true;

        if (!m_primary_engine->Initialize(options)) {
            std::cerr << "Failed to initialize primary OCR engine" << std::endl;
            return false;
        }

        // Try to initialize secondary engine for fallback
        InitializeSecondaryEngine();

        m_initialized = true;
        m_current_options = options;
        
        std::cout << "Dual-mode OCR Manager initialized with " 
                  << m_primary_engine->GetEngineInfo() << std::endl;
        return true;
    }

    bool Initialize(OCRMode mode) {
        OCREngineFactory::EngineType engineType;
        switch (mode) {
            case OCRMode::FAST:
                engineType = OCREngineFactory::EngineType::PADDLE_OCR;
                break;
            case OCRMode::ACCURATE:
            case OCRMode::MULTIMODAL:
                engineType = OCREngineFactory::EngineType::MINICPM_V;
                break;
            case OCRMode::AUTO:
            default:
                engineType = OCREngineFactory::EngineType::AUTO_SELECT;
                break;
        }
        
        m_current_mode = mode;
        return Initialize(engineType);
    }

    bool SetOCRMode(OCRMode mode) {
        if (!m_initialized) {
            return false;
        }

        if (m_current_mode == mode) {
            return true; // Already in the requested mode
        }

        // Switch engines if needed
        switch (mode) {
            case OCRMode::FAST:
                if (!EnsurePaddleOCREngine()) {
                    return false;
                }
                break;
            case OCRMode::ACCURATE:
            case OCRMode::MULTIMODAL:
                if (!EnsureMiniCPMVEngine()) {
                    return false;
                }
                break;
            case OCRMode::AUTO:
                // Keep current engine, just change mode
                break;
        }

        m_current_mode = mode;
        std::cout << "Switched to OCR mode: " << static_cast<int>(mode) << std::endl;
        return true;
    }

    OCRMode GetCurrentMode() const {
        return m_current_mode;
    }

    void Shutdown() {
        if (!m_initialized) {
            return;
        }

        if (m_primary_engine) {
            m_primary_engine->Shutdown();
            m_primary_engine.reset();
        }

        if (m_secondary_engine) {
            m_secondary_engine->Shutdown();
            m_secondary_engine.reset();
        }

        m_initialized = false;
        std::cout << "Dual-mode OCR Manager shut down" << std::endl;
    }

    OCRDocument ExtractText(const CaptureFrame& frame) {
        if (!m_initialized || !m_primary_engine) {
            return OCRDocument();
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        // Choose engine based on current mode
        IOCREngine* engine = SelectEngine(frame);
        
        OCRDocument document = engine->ProcessImage(frame);
        document.timestamp = std::chrono::system_clock::now();

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Update statistics
        UpdateStatistics(document, duration.count());

        return document;
    }

    std::future<OCRDocument> ExtractTextAsync(const CaptureFrame& frame) {
        if (!m_initialized || !m_primary_engine) {
            std::promise<OCRDocument> promise;
            promise.set_value(OCRDocument());
            return promise.get_future();
        }

        IOCREngine* engine = SelectEngine(frame);
        return engine->ProcessImageAsync(frame);
    }

    OCRDocument ExtractWindowText(WindowHandle windowHandle) {
        if (!m_initialized) {
            return OCRDocument();
        }

        // For now, return empty document
        // In a full implementation, we would capture the specific window
        OCRDocument document;
        document.timestamp = std::chrono::system_clock::now();
        return document;
    }

    // Multimodal capabilities (MiniCPM-V only)
    std::string AnswerQuestion(const CaptureFrame& frame, const std::string& question) {
        if (!EnsureMiniCPMVEngine()) {
            return "Multimodal capabilities not available";
        }

        auto* minicpm_engine = dynamic_cast<MiniCPMVEngine*>(GetMiniCPMVEngine());
        if (!minicpm_engine) {
            return "MiniCPM-V engine not available";
        }

        auto response = minicpm_engine->AnswerQuestion(frame, question);
        return response.text_content;
    }

    std::string DescribeImage(const CaptureFrame& frame) {
        if (!EnsureMiniCPMVEngine()) {
            return "Image description not available";
        }

        auto* minicpm_engine = dynamic_cast<MiniCPMVEngine*>(GetMiniCPMVEngine());
        if (!minicpm_engine) {
            return "MiniCPM-V engine not available";
        }

        auto response = minicpm_engine->DescribeImage(frame);
        return response.text_content;
    }

    std::vector<std::string> ExtractStructuredData(const CaptureFrame& frame, const std::string& dataType) {
        if (!EnsureMiniCPMVEngine()) {
            return {};
        }

        auto* minicpm_engine = dynamic_cast<MiniCPMVEngine*>(GetMiniCPMVEngine());
        if (!minicpm_engine) {
            return {};
        }

        auto response = minicpm_engine->ExtractStructuredData(frame, dataType);
        return response.detected_elements;
    }

    bool ContainsText(const CaptureFrame& frame, const std::string& searchText) {
        OCRDocument document = ExtractText(frame);
        std::string text = document.GetOrderedText();
        
        // Convert to lowercase for case-insensitive search
        std::transform(text.begin(), text.end(), text.begin(), ::tolower);
        std::string searchLower = searchText;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
        
        return text.find(searchLower) != std::string::npos;
    }

    std::vector<std::string> ExtractKeywords(const OCRDocument& document) {
        std::vector<std::string> keywords;
        std::string text = document.GetOrderedText();
        
        if (text.empty()) {
            return keywords;
        }

        // Simple keyword extraction using regex
        std::regex word_regex(R"(\b[a-zA-Z]{2,}\b)"); // English words only for now
        std::sregex_iterator iter(text.begin(), text.end(), word_regex);
        std::sregex_iterator end;

        std::set<std::string> unique_words;
        for (; iter != end; ++iter) {
            std::string word = iter->str();
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            
            // Skip common words
            if (IsCommonWord(word)) {
                continue;
            }
            
            unique_words.insert(word);
        }

        keywords.assign(unique_words.begin(), unique_words.end());
        return keywords;
    }

    void SetLanguage(const std::string& language) {
        m_current_options.language = language;
        if (m_primary_engine) {
            m_primary_engine->SetOptions(m_current_options);
        }
        if (m_secondary_engine) {
            m_secondary_engine->SetOptions(m_current_options);
        }
    }

    void SetConfidenceThreshold(float threshold) {
        m_current_options.confidence_threshold = threshold;
        if (m_primary_engine) {
            m_primary_engine->SetOptions(m_current_options);
        }
        if (m_secondary_engine) {
            m_secondary_engine->SetOptions(m_current_options);
        }
    }

    void EnablePreprocessing(bool enable) {
        m_current_options.auto_preprocess = enable;
        if (m_primary_engine) {
            m_primary_engine->SetOptions(m_current_options);
        }
        if (m_secondary_engine) {
            m_secondary_engine->SetOptions(m_current_options);
        }
    }

    void SetOptions(const OCROptions& options) {
        m_current_options = options;
        if (m_primary_engine) {
            m_primary_engine->SetOptions(options);
        }
        if (m_secondary_engine) {
            m_secondary_engine->SetOptions(options);
        }
    }

    OCROptions GetOptions() const {
        return m_current_options;
    }

    void EnableGPU(bool enable) {
        m_current_options.use_gpu = enable;
        SetOptions(m_current_options);
    }

    void SetMaxImageSize(int max_size) {
        m_current_options.max_image_size = max_size;
        SetOptions(m_current_options);
    }

    void EnableCaching(bool enable, int ttl_seconds) {
        m_current_options.enable_caching = enable;
        m_current_options.cache_ttl_seconds = ttl_seconds;
        SetOptions(m_current_options);
    }

    OCRManager::Statistics GetStatistics() const {
        return m_statistics;
    }

    void ResetStatistics() {
        m_statistics = OCRManager::Statistics{};
    }

private:
    void InitializeSecondaryEngine() {
        // Try to initialize the other engine type for fallback
        if (dynamic_cast<PaddleOCREngine*>(m_primary_engine.get())) {
            // Primary is PaddleOCR, try MiniCPM-V as secondary
            m_secondary_engine = std::make_unique<MiniCPMVEngine>();
        } else {
            // Primary is MiniCPM-V, try PaddleOCR as secondary
            m_secondary_engine = std::make_unique<PaddleOCREngine>();
        }

        if (m_secondary_engine) {
            OCROptions options = m_current_options;
            if (!m_secondary_engine->Initialize(options)) {
                m_secondary_engine.reset(); // Failed to initialize, remove it
            }
        }
    }

    IOCREngine* SelectEngine(const CaptureFrame& frame) {
        switch (m_current_mode) {
            case OCRMode::FAST:
                return GetPaddleOCREngine();
            case OCRMode::ACCURATE:
            case OCRMode::MULTIMODAL:
                return GetMiniCPMVEngine();
            case OCRMode::AUTO:
                // Intelligent selection based on image characteristics
                return SelectEngineIntelligently(frame);
        }
        return m_primary_engine.get();
    }

    IOCREngine* SelectEngineIntelligently(const CaptureFrame& frame) {
        // Simple heuristics for engine selection
        if (frame.GetDataSize() > 1920 * 1080 * 4) {
            // Large image, use fast engine
            return GetPaddleOCREngine();
        }
        
        // Default to primary engine
        return m_primary_engine.get();
    }

    IOCREngine* GetPaddleOCREngine() {
        if (dynamic_cast<PaddleOCREngine*>(m_primary_engine.get())) {
            return m_primary_engine.get();
        }
        if (dynamic_cast<PaddleOCREngine*>(m_secondary_engine.get())) {
            return m_secondary_engine.get();
        }
        return m_primary_engine.get(); // Fallback
    }

    IOCREngine* GetMiniCPMVEngine() {
        if (dynamic_cast<MiniCPMVEngine*>(m_primary_engine.get())) {
            return m_primary_engine.get();
        }
        if (dynamic_cast<MiniCPMVEngine*>(m_secondary_engine.get())) {
            return m_secondary_engine.get();
        }
        return nullptr;
    }

    bool EnsurePaddleOCREngine() {
        return GetPaddleOCREngine() != nullptr;
    }

    bool EnsureMiniCPMVEngine() {
        return GetMiniCPMVEngine() != nullptr;
    }

    void UpdateStatistics(const OCRDocument& document, double processing_time_ms) {
        m_statistics.total_processed++;
        
        if (!document.text_blocks.empty()) {
            m_statistics.successful_extractions++;
        }

        // Update average processing time
        double total_time = m_statistics.average_processing_time_ms * (m_statistics.total_processed - 1);
        total_time += processing_time_ms;
        m_statistics.average_processing_time_ms = total_time / m_statistics.total_processed;

        // Update average confidence
        if (document.overall_confidence > 0.0f) {
            double total_confidence = m_statistics.average_confidence * (m_statistics.successful_extractions - 1);
            total_confidence += document.overall_confidence;
            m_statistics.average_confidence = total_confidence / m_statistics.successful_extractions;
        }
    }

    bool IsCommonWord(const std::string& word) {
        static const std::set<std::string> common_words = {
            "the", "and", "or", "but", "in", "on", "at", "to", "for", "of", "with",
            "by", "from", "up", "about", "into", "through", "during", "before",
            "after", "above", "below", "between", "among", "since", "without",
            "under", "within", "along", "following", "across", "behind", "beyond",
            "plus", "except", "but", "unless", "until", "while", "where", "when",
            "why", "how", "all", "any", "both", "each", "few", "more", "most",
            "other", "some", "such", "only", "own", "same", "so", "than", "too",
            "very", "can", "will", "just", "should", "now", "may", "must", "shall",
            "would", "could", "might", "ought", "need", "dare", "used", "able",
            "\u7684", "\u4e00", "\u662f", "\u5728", "\u4e0d", "\u4e86", "\u6709", "\u548c", "\u4eba", "\u8fd9", 
            "\u4e2d", "\u5927", "\u4e3a", "\u4e0a", "\u4e2a", "\u56fd", "\u6211", "\u4ee5", "\u8981", "\u4ed6", 
            "\u65f6", "\u6765", "\u7528", "\u4eec", "\u751f", "\u5230", "\u4f5c", "\u5730", "\u4e8e", "\u51fa", 
            "\u5c31", "\u5206", "\u5bf9", "\u6210", "\u4f1a", "\u53ef", "\u4e3b", "\u53d1", "\u5e74", "\u52a8", 
            "\u540c", "\u5de5", "\u4e5f", "\u80fd", "\u4e0b", "\u8fc7", "\u5b50", "\u8bf4", "\u4ea7", "\u79cd", 
            "\u9762", "\u800c", "\u65b9", "\u540e", "\u591a", "\u5b9a", "\u884c", "\u5b66", "\u6cd5", "\u6240"
        };
        
        return common_words.find(word) != common_words.end();
    }

private:
    bool m_initialized;
    OCRMode m_current_mode;
    std::unique_ptr<IOCREngine> m_primary_engine;
    std::unique_ptr<IOCREngine> m_secondary_engine;
    OCROptions m_current_options;
    OCRManager::Statistics m_statistics;
};

// OCRManager public interface
OCRManager::OCRManager() : m_impl(std::make_unique<Impl>()) {}
OCRManager::~OCRManager() = default;

bool OCRManager::Initialize(OCREngineFactory::EngineType engineType) {
    return m_impl->Initialize(engineType);
}

bool OCRManager::Initialize(OCRMode mode) {
    return m_impl->Initialize(mode);
}

bool OCRManager::SetOCRMode(OCRMode mode) {
    return m_impl->SetOCRMode(mode);
}

OCRMode OCRManager::GetCurrentMode() const {
    return m_impl->GetCurrentMode();
}

void OCRManager::Shutdown() {
    m_impl->Shutdown();
}

OCRDocument OCRManager::ExtractText(const CaptureFrame& frame) {
    return m_impl->ExtractText(frame);
}

std::future<OCRDocument> OCRManager::ExtractTextAsync(const CaptureFrame& frame) {
    return m_impl->ExtractTextAsync(frame);
}

OCRDocument OCRManager::ExtractWindowText(WindowHandle windowHandle) {
    return m_impl->ExtractWindowText(windowHandle);
}

std::string OCRManager::AnswerQuestion(const CaptureFrame& frame, const std::string& question) {
    return m_impl->AnswerQuestion(frame, question);
}

std::string OCRManager::DescribeImage(const CaptureFrame& frame) {
    return m_impl->DescribeImage(frame);
}

std::vector<std::string> OCRManager::ExtractStructuredData(const CaptureFrame& frame, const std::string& dataType) {
    return m_impl->ExtractStructuredData(frame, dataType);
}

bool OCRManager::ContainsText(const CaptureFrame& frame, const std::string& searchText) {
    return m_impl->ContainsText(frame, searchText);
}

std::vector<std::string> OCRManager::ExtractKeywords(const OCRDocument& document) {
    return m_impl->ExtractKeywords(document);
}

void OCRManager::SetLanguage(const std::string& language) {
    m_impl->SetLanguage(language);
}

void OCRManager::SetConfidenceThreshold(float threshold) {
    m_impl->SetConfidenceThreshold(threshold);
}

void OCRManager::EnablePreprocessing(bool enable) {
    m_impl->EnablePreprocessing(enable);
}

void OCRManager::SetOptions(const OCROptions& options) {
    m_impl->SetOptions(options);
}

OCROptions OCRManager::GetOptions() const {
    return m_impl->GetOptions();
}

void OCRManager::EnableGPU(bool enable) {
    m_impl->EnableGPU(enable);
}

void OCRManager::SetMaxImageSize(int max_size) {
    m_impl->SetMaxImageSize(max_size);
}

void OCRManager::EnableCaching(bool enable, int ttl_seconds) {
    m_impl->EnableCaching(enable, ttl_seconds);
}

OCRManager::Statistics OCRManager::GetStatistics() const {
    return m_impl->GetStatistics();
}

void OCRManager::ResetStatistics() {
    m_impl->ResetStatistics();
}

} // namespace work_assistant