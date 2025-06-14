#include "ocr_engine.h"
#include <iostream>
#include <algorithm>
#include <regex>
#include <thread>
#include <chrono>
#include <sstream>
#include <set>

namespace work_assistant {

// Helper structs for OCR processing
struct ConfidenceComparator {
    bool operator()(const OCRResult& a, const OCRResult& b) const {
        return a.confidence > b.confidence; // Sort by confidence descending
    }
};

struct ReadingOrderComparator {
    bool operator()(const OCRResult& a, const OCRResult& b) const {
        // Sort by reading order (top-to-bottom, left-to-right)
        if (std::abs(a.y - b.y) > 20) { // 20px tolerance for same line
            return a.y < b.y;
        }
        return a.x < b.x;
    }
};

// OCR Manager implementation
class OCRManager::Impl {
public:
    Impl() : m_initialized(false), m_statistics{} {}

    bool Initialize(OCREngineFactory::EngineType engineType) {
        if (m_initialized) {
            return true;
        }

        m_engine = OCREngineFactory::Create(engineType);
        if (!m_engine) {
            std::cerr << "Failed to create OCR engine" << std::endl;
            return false;
        }

        OCROptions options;
        options.language = "eng";
        options.confidence_threshold = 0.6f;
        options.auto_preprocess = true;

        if (!m_engine->Initialize(options)) {
            std::cerr << "Failed to initialize OCR engine" << std::endl;
            return false;
        }

        m_initialized = true;
        std::cout << "OCR Manager initialized with " << m_engine->GetEngineInfo() << std::endl;
        return true;
    }

    void Shutdown() {
        if (!m_initialized) {
            return;
        }

        if (m_engine) {
            m_engine->Shutdown();
            m_engine.reset();
        }

        m_initialized = false;
        std::cout << "OCR Manager shut down" << std::endl;
    }

    OCRDocument ExtractText(const CaptureFrame& frame) {
        if (!m_initialized || !m_engine) {
            return OCRDocument();
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        OCRDocument document = m_engine->ProcessImage(frame);
        document.timestamp = std::chrono::system_clock::now();

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Update statistics
        UpdateStatistics(document, duration.count());

        return document;
    }

    std::future<OCRDocument> ExtractTextAsync(const CaptureFrame& frame) {
        if (!m_initialized || !m_engine) {
            std::promise<OCRDocument> promise;
            promise.set_value(OCRDocument());
            return promise.get_future();
        }

        return m_engine->ProcessImageAsync(frame);
    }

    OCRDocument ExtractWindowText(WindowHandle windowHandle) {
        if (!m_initialized) {
            return OCRDocument();
        }

        // For now, return empty document
        // In a full implementation, we would:
        // 1. Capture the specific window
        // 2. Process the captured image
        OCRDocument document;
        document.timestamp = std::chrono::system_clock::now();
        return document;
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
        std::regex word_regex(R"(\b[a-zA-Z]{3,}\b)");
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
        if (m_engine) {
            OCROptions options = m_engine->GetOptions();
            options.language = language;
            m_engine->SetOptions(options);
        }
    }

    void SetConfidenceThreshold(float threshold) {
        if (m_engine) {
            OCROptions options = m_engine->GetOptions();
            options.confidence_threshold = threshold;
            m_engine->SetOptions(options);
        }
    }

    void EnablePreprocessing(bool enable) {
        if (m_engine) {
            OCROptions options = m_engine->GetOptions();
            options.auto_preprocess = enable;
            m_engine->SetOptions(options);
        }
    }

    OCRManager::Statistics GetStatistics() const {
        return m_statistics;
    }

    void ResetStatistics() {
        m_statistics = OCRManager::Statistics{};
    }

private:
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
            "would", "could", "might", "ought", "need", "dare", "used", "able"
        };
        
        return common_words.find(word) != common_words.end();
    }

private:
    bool m_initialized;
    std::unique_ptr<IOCREngine> m_engine;
    OCRManager::Statistics m_statistics;
};

// OCRManager public interface
OCRManager::OCRManager() : m_impl(std::make_unique<Impl>()) {}
OCRManager::~OCRManager() = default;

bool OCRManager::Initialize(OCREngineFactory::EngineType engineType) {
    return m_impl->Initialize(engineType);
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

OCRManager::Statistics OCRManager::GetStatistics() const {
    return m_impl->GetStatistics();
}

void OCRManager::ResetStatistics() {
    m_impl->ResetStatistics();
}

} // namespace work_assistant