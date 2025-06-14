#include "ocr_engine.h"
#include <iostream>
#include <algorithm>
#include <regex>
#include <thread>
#include <chrono>
#include <sstream>
#include <set>

namespace work_assistant {

// OCRDocument implementation
void OCRDocument::CombineText() {
    full_text.clear();
    if (text_blocks.empty()) {
        return;
    }

    // Sort blocks by reading order (top-to-bottom, left-to-right)
    std::vector<OCRResult> sorted_blocks = text_blocks;
    std::sort(sorted_blocks.begin(), sorted_blocks.end(), 
              [](const OCRResult& a, const OCRResult& b) {
                  // First sort by Y position (top to bottom)
                  if (std::abs(a.y - b.y) > 20) { // 20px tolerance for same line
                      return a.y < b.y;
                  }
                  // Then by X position (left to right)
                  return a.x < b.x;
              });

    // Combine text with proper spacing
    for (size_t i = 0; i < sorted_blocks.size(); ++i) {
        if (!sorted_blocks[i].text.empty()) {
            full_text += sorted_blocks[i].text;
            
            // Add space or newline based on position
            if (i < sorted_blocks.size() - 1) {
                const auto& current = sorted_blocks[i];
                const auto& next = sorted_blocks[i + 1];
                
                // If next block is significantly below, add newline
                if (next.y > current.y + current.height + 10) {
                    full_text += "\n";
                } else {
                    full_text += " ";
                }
            }
        }
    }

    // Calculate overall confidence
    if (!text_blocks.empty()) {
        float total_confidence = 0.0f;
        for (const auto& block : text_blocks) {
            total_confidence += block.confidence;
        }
        overall_confidence = total_confidence / text_blocks.size();
    }
}

std::vector<OCRResult> OCRDocument::GetHighConfidenceResults(float threshold) const {
    std::vector<OCRResult> results;
    for (const auto& block : text_blocks) {
        if (block.confidence >= threshold) {
            results.push_back(block);
        }
    }
    return results;
}

std::string OCRDocument::GetOrderedText() const {
    if (full_text.empty()) {
        const_cast<OCRDocument*>(this)->CombineText();
    }
    return full_text;
}

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
        options.confidence_threshold = 0.5f;
        options.auto_preprocess = true;

        if (!m_engine->Initialize(options)) {
            std::cerr << "Failed to initialize OCR engine" << std::endl;
            m_engine.reset();
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
        
        OCRDocument result = m_engine->ProcessImage(frame);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Update statistics
        UpdateStatistics(result, duration.count());

        return result;
    }

    std::future<OCRDocument> ExtractTextAsync(const CaptureFrame& frame) {
        if (!m_initialized || !m_engine) {
            std::promise<OCRDocument> promise;
            promise.set_value(OCRDocument());
            return promise.get_future();
        }

        return m_engine->ProcessImageAsync(frame);
    }

    OCRDocument ExtractWindowText(uint64_t windowHandle) {
        // This would require integration with screen capture
        // For now, return empty document
        return OCRDocument();
    }

    bool ContainsText(const CaptureFrame& frame, const std::string& searchText) {
        OCRDocument doc = ExtractText(frame);
        std::string fullText = doc.GetOrderedText();
        
        // Case-insensitive search
        std::string lowerFullText = fullText;
        std::string lowerSearchText = searchText;
        std::transform(lowerFullText.begin(), lowerFullText.end(), lowerFullText.begin(), ::tolower);
        std::transform(lowerSearchText.begin(), lowerSearchText.end(), lowerSearchText.begin(), ::tolower);
        
        return lowerFullText.find(lowerSearchText) != std::string::npos;
    }

    std::vector<std::string> ExtractKeywords(const OCRDocument& document) {
        std::vector<std::string> keywords;
        std::string text = document.GetOrderedText();
        
        if (text.empty()) {
            return keywords;
        }

        // Simple keyword extraction - split by whitespace and filter
        std::istringstream iss(text);
        std::string word;
        std::set<std::string> uniqueWords;
        
        while (iss >> word) {
            // Clean word
            word = ocr_utils::CleanExtractedText(word);
            
            // Filter out short words and common words
            if (word.length() >= 3 && IsKeywordCandidate(word)) {
                std::transform(word.begin(), word.end(), word.begin(), ::tolower);
                uniqueWords.insert(word);
            }
        }
        
        keywords.assign(uniqueWords.begin(), uniqueWords.end());
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
        m_statistics = Statistics{};
    }

private:
    void UpdateStatistics(const OCRDocument& result, double processing_time_ms) {
        m_statistics.total_processed++;
        
        if (!result.text_blocks.empty()) {
            m_statistics.successful_extractions++;
            
            // Update average confidence
            double total_confidence = m_statistics.average_confidence * (m_statistics.successful_extractions - 1);
            total_confidence += result.overall_confidence;
            m_statistics.average_confidence = total_confidence / m_statistics.successful_extractions;
        }
        
        // Update average processing time
        double total_time = m_statistics.average_processing_time_ms * (m_statistics.total_processed - 1);
        total_time += processing_time_ms;
        m_statistics.average_processing_time_ms = total_time / m_statistics.total_processed;
    }

    bool IsKeywordCandidate(const std::string& word) {
        // Simple heuristics for keyword filtering
        static const std::set<std::string> commonWords = {
            "the", "and", "for", "are", "but", "not", "you", "all", "can", "had", "her", "was", "one", "our", "out", "day", "get", "has", "him", "his", "how", "man", "new", "now", "old", "see", "two", "way", "who", "boy", "did", "its", "let", "put", "say", "she", "too", "use"
        };
        
        std::string lowerWord = word;
        std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), ::tolower);
        
        return commonWords.find(lowerWord) == commonWords.end();
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

// OCR utilities implementation
namespace ocr_utils {

bool PreprocessImage(const CaptureFrame& input, CaptureFrame& output, const OCROptions& options) {
    if (!input.IsValid()) {
        return false;
    }

    output = input; // Start with copy

    if (options.auto_preprocess) {
        // Apply preprocessing pipeline
        if (options.scale_factor != 1.0f) {
            CaptureFrame scaled;
            if (ScaleImage(output, scaled, options.scale_factor)) {
                output = scaled;
            }
        }

        if (options.denoise) {
            DenoiseImage(output);
        }

        if (options.enhance_contrast) {
            EnhanceContrast(output);
        }

        // Convert to grayscale before binarization
        CaptureFrame grayscale;
        if (ConvertToGrayscale(output, grayscale)) {
            output = grayscale;
        }

        if (options.binarize) {
            BinarizeImage(output);
        }
    }

    return true;
}

bool ScaleImage(const CaptureFrame& input, CaptureFrame& output, float scale) {
    if (!input.IsValid() || scale <= 0.0f) {
        return false;
    }

    int newWidth = static_cast<int>(input.width * scale);
    int newHeight = static_cast<int>(input.height * scale);

    output.width = newWidth;
    output.height = newHeight;
    output.bytes_per_pixel = input.bytes_per_pixel;
    output.timestamp = input.timestamp;
    output.data.resize(output.GetDataSize());

    // Simple nearest neighbor scaling
    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            int srcX = static_cast<int>(x / scale);
            int srcY = static_cast<int>(y / scale);
            
            // Clamp to source bounds
            srcX = std::min(srcX, input.width - 1);
            srcY = std::min(srcY, input.height - 1);
            
            int srcIndex = (srcY * input.width + srcX) * input.bytes_per_pixel;
            int dstIndex = (y * newWidth + x) * output.bytes_per_pixel;
            
            for (int c = 0; c < input.bytes_per_pixel; ++c) {
                output.data[dstIndex + c] = input.data[srcIndex + c];
            }
        }
    }

    return true;
}

bool ConvertToGrayscale(const CaptureFrame& input, CaptureFrame& output) {
    if (!input.IsValid() || input.bytes_per_pixel < 3) {
        return false;
    }

    output.width = input.width;
    output.height = input.height;
    output.bytes_per_pixel = 1;
    output.timestamp = input.timestamp;
    output.data.resize(output.GetDataSize());

    for (int i = 0; i < input.width * input.height; ++i) {
        int srcIndex = i * input.bytes_per_pixel;
        uint8_t r = input.data[srcIndex];
        uint8_t g = input.data[srcIndex + 1];
        uint8_t b = input.data[srcIndex + 2];
        
        // Use standard grayscale conversion
        uint8_t gray = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
        output.data[i] = gray;
    }

    return true;
}

bool EnhanceContrast(CaptureFrame& frame, float factor) {
    if (!frame.IsValid() || factor <= 0.0f) {
        return false;
    }

    for (size_t i = 0; i < frame.data.size(); ++i) {
        float pixel = static_cast<float>(frame.data[i]);
        pixel = (pixel - 128.0f) * factor + 128.0f;
        pixel = std::max(0.0f, std::min(255.0f, pixel));
        frame.data[i] = static_cast<uint8_t>(pixel);
    }

    return true;
}

bool DenoiseImage(CaptureFrame& frame) {
    if (!frame.IsValid()) {
        return false;
    }

    // Simple median filter for denoising
    std::vector<uint8_t> temp = frame.data;
    
    for (int y = 1; y < frame.height - 1; ++y) {
        for (int x = 1; x < frame.width - 1; ++x) {
            for (int c = 0; c < frame.bytes_per_pixel; ++c) {
                std::vector<uint8_t> neighborhood;
                
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int idx = ((y + dy) * frame.width + (x + dx)) * frame.bytes_per_pixel + c;
                        neighborhood.push_back(temp[idx]);
                    }
                }
                
                std::sort(neighborhood.begin(), neighborhood.end());
                int idx = (y * frame.width + x) * frame.bytes_per_pixel + c;
                frame.data[idx] = neighborhood[4]; // Median of 9 values
            }
        }
    }

    return true;
}

bool BinarizeImage(CaptureFrame& frame, int threshold) {
    if (!frame.IsValid()) {
        return false;
    }

    for (size_t i = 0; i < frame.data.size(); ++i) {
        frame.data[i] = (frame.data[i] >= threshold) ? 255 : 0;
    }

    return true;
}

std::string CleanExtractedText(const std::string& text) {
    std::string cleaned = text;
    
    // Remove extra whitespace
    cleaned = std::regex_replace(cleaned, std::regex("\\s+"), " ");
    
    // Remove non-printable characters except newlines and tabs
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(),
                                [](char c) {
                                    return !std::isprint(c) && c != '\n' && c != '\t';
                                }), cleaned.end());
    
    // Trim leading/trailing whitespace
    size_t start = cleaned.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = cleaned.find_last_not_of(" \t\n\r");
    
    return cleaned.substr(start, end - start + 1);
}

std::vector<std::string> SplitIntoLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            lines.push_back(CleanExtractedText(line));
        }
    }
    
    return lines;
}

std::vector<std::string> ExtractWords(const std::string& text) {
    std::vector<std::string> words;
    std::regex word_regex(R"(\b\w+\b)");
    std::sregex_iterator iter(text.begin(), text.end(), word_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        words.push_back(iter->str());
    }
    
    return words;
}

bool IsTextMeaningful(const std::string& text, float min_ratio) {
    if (text.empty()) {
        return false;
    }
    
    int alphanumeric_count = 0;
    for (char c : text) {
        if (std::isalnum(c)) {
            alphanumeric_count++;
        }
    }
    
    float ratio = static_cast<float>(alphanumeric_count) / text.length();
    return ratio >= min_ratio;
}

std::string DetectLanguage(const std::string& text) {
    // Simple heuristic language detection
    if (text.empty()) {
        return "unknown";
    }
    
    // Count character types
    int ascii_count = 0;
    int chinese_count = 0;
    int cyrillic_count = 0;
    
    for (char c : text) {
        if (static_cast<unsigned char>(c) < 128) {
            ascii_count++;
        } else {
            // This is a simplified check - proper Unicode handling would be better
            unsigned char uc = static_cast<unsigned char>(c);
            if (uc >= 0xE4 && uc <= 0xE9) { // Rough CJK range
                chinese_count++;
            } else if (uc >= 0xD0 && uc <= 0xDF) { // Rough Cyrillic range
                cyrillic_count++;
            }
        }
    }
    
    if (chinese_count > ascii_count * 0.1) {
        return "chi_sim";
    } else if (cyrillic_count > ascii_count * 0.1) {
        return "rus";
    }
    
    return "eng";
}

bool IsLikelyCode(const std::string& text) {
    // Simple heuristics for code detection
    std::regex code_patterns(R"(\{|\}|;|->|=>|==|!=|\+\+|--|#include|function|class|if\s*\(|for\s*\(|while\s*\()");
    return std::regex_search(text, code_patterns);
}

bool IsLikelyEmail(const std::string& text) {
    std::regex email_pattern(R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b)");
    return std::regex_search(text, email_pattern);
}

bool IsLikelyURL(const std::string& text) {
    std::regex url_pattern(R"(https?://[^\s]+)");
    return std::regex_search(text, url_pattern);
}

} // namespace ocr_utils

} // namespace work_assistant