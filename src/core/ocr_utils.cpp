#include "ocr_engine.h"
#include <algorithm>
#include <regex>
#include <cctype>
#include <sstream>

namespace work_assistant {
namespace ocr_utils {

bool PreprocessImage(const CaptureFrame& input, CaptureFrame& output, const OCROptions& options) {
    // Simple implementation - just copy the input to output
    output = input;
    return true;
}

bool ScaleImage(const CaptureFrame& input, CaptureFrame& output, float scale) {
    // Simple implementation - just copy the input to output
    output = input;
    return true;
}

bool ConvertToGrayscale(const CaptureFrame& input, CaptureFrame& output) {
    // Simple implementation - just copy the input to output
    output = input;
    return true;
}

bool EnhanceContrast(CaptureFrame& frame, float factor) {
    // Simple implementation - no-op
    return true;
}

bool DenoiseImage(CaptureFrame& frame) {
    // Simple implementation - no-op
    return true;
}

bool BinarizeImage(CaptureFrame& frame, int threshold) {
    // Simple implementation - no-op
    return true;
}

std::string CleanExtractedText(const std::string& text) {
    std::string cleaned = text;
    
    // Remove excessive whitespace
    std::regex multi_space("\\s+");
    cleaned = std::regex_replace(cleaned, multi_space, " ");
    
    // Trim leading and trailing whitespace
    cleaned.erase(cleaned.begin(), std::find_if(cleaned.begin(), cleaned.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    cleaned.erase(std::find_if(cleaned.rbegin(), cleaned.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), cleaned.end());
    
    return cleaned;
}

std::vector<std::string> SplitIntoLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            lines.push_back(line);
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
    
    // Count alphanumeric characters vs total characters
    int alphanumeric_count = 0;
    int total_count = 0;
    
    for (char c : text) {
        if (c != ' ') {  // Don't count spaces
            total_count++;
            if (std::isalnum(c)) {
                alphanumeric_count++;
            }
        }
    }
    
    if (total_count == 0) {
        return false;
    }
    
    float ratio = static_cast<float>(alphanumeric_count) / total_count;
    return ratio >= min_ratio;
}

std::string DetectLanguage(const std::string& text) {
    // Simple implementation - always return English
    return "en";
}

bool IsLikelyCode(const std::string& text) {
    // Simple heuristic - check for common code patterns
    return text.find("{") != std::string::npos ||
           text.find("}") != std::string::npos ||
           text.find("function") != std::string::npos ||
           text.find("class") != std::string::npos ||
           text.find("import") != std::string::npos ||
           text.find("#include") != std::string::npos;
}

bool IsLikelyEmail(const std::string& text) {
    // Simple email detection
    std::regex email_regex(R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b)");
    return std::regex_search(text, email_regex);
}

bool IsLikelyURL(const std::string& text) {
    // Simple URL detection
    std::regex url_regex(R"(https?://[^\s]+)");
    return std::regex_search(text, url_regex);
}

} // namespace ocr_utils
} // namespace work_assistant