#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace work_assistant {

// Common type definitions to avoid circular dependencies

// Window-related types
using WindowHandle = void*;

struct WindowInfo {
    WindowHandle window_handle = nullptr;
    std::string title;
    std::string class_name;
    std::string process_name;
    uint32_t process_id = 0;
    int x = 0, y = 0;
    int width = 0, height = 0;
    bool is_visible = false;
    std::chrono::system_clock::time_point timestamp;
};

enum class WindowEventType {
    WINDOW_CREATED,
    WINDOW_DESTROYED,
    WINDOW_FOCUSED,
    WINDOW_MINIMIZED,
    WINDOW_RESTORED
};

struct WindowEvent {
    WindowEventType type;
    std::chrono::system_clock::time_point timestamp;
    WindowInfo window_info;
};

// AI-related types
enum class ContentType {
    UNKNOWN = 0,
    DOCUMENT = 1,
    CODE = 2,
    EMAIL = 3,
    WEB_BROWSING = 4,
    SOCIAL_MEDIA = 5,
    CHAT = 6,
    VIDEO = 7,
    GAME = 8,
    PRODUCTIVITY = 9,
    ENTERTAINMENT = 10,
    COMMUNICATION = 11,
    DEVELOPMENT = 12,
    DESIGN = 13,
    EDUCATION = 14,
    FINANCE = 15,
    SETTINGS = 16
};

enum class WorkCategory {
    UNKNOWN = 0,
    FOCUSED_WORK = 1,
    COMMUNICATION = 2,
    RESEARCH = 3,
    BREAK = 4,
    MEETING = 5,
    LEARNING = 6,
    BREAK_TIME = 7,
    PLANNING = 8,
    ADMINISTRATIVE = 9,
    CREATIVE = 10,
    ANALYSIS = 11,
    COLLABORATION = 12
};

enum class ActivityPriority {
    VERY_LOW = 1,
    LOW = 2,
    MEDIUM = 3,
    HIGH = 4,
    VERY_HIGH = 5,
    URGENT = 6
};

// Image format enumeration
enum class ImageFormat {
    UNKNOWN = 0,
    RGB = 1,
    RGBA = 2,
    BGR = 3,
    BGRA = 4,
    GRAY = 5
};

// Screen capture types
struct CaptureFrame {
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    int bytes_per_pixel = 4; // RGBA by default
    int stride = 0; // Bytes per row
    ImageFormat format = ImageFormat::RGBA;
    std::chrono::system_clock::time_point timestamp;
    
    // Utility methods
    size_t GetDataSize() const {
        int actual_stride = (stride > 0) ? stride : width * bytes_per_pixel;
        return static_cast<size_t>(height * actual_stride);
    }
    
    bool IsValid() const {
        return width > 0 && height > 0 && !data.empty() && 
               data.size() >= GetDataSize();
    }
};

// OCR types
struct TextBlock {
    std::string text;
    float confidence = 0.0f;
    int x = 0, y = 0;
    int width = 0, height = 0;
};

// Alias for compatibility
using OCRResult = TextBlock;

struct OCRDocument {
    std::vector<TextBlock> text_blocks;
    std::string full_text;
    float overall_confidence = 0.0f;
    std::chrono::milliseconds processing_time{0};
    std::chrono::system_clock::time_point timestamp;
    
    std::string GetOrderedText() const {
        if (!full_text.empty()) {
            return full_text;
        }
        
        std::string result;
        for (const auto& block : text_blocks) {
            if (!result.empty()) {
                result += " ";
            }
            result += block.text;
        }
        return result;
    }
};

// AI analysis types
struct ContentAnalysis {
    std::chrono::system_clock::time_point timestamp;
    std::string title;
    std::string application;
    std::string extracted_text;
    std::vector<std::string> keywords;
    
    ContentType content_type = ContentType::UNKNOWN;
    WorkCategory work_category = WorkCategory::UNKNOWN;
    ActivityPriority priority = ActivityPriority::MEDIUM;
    
    bool is_productive = false;
    bool is_focused_work = false;
    bool requires_attention = false;
    float classification_confidence = 0.0f;
    float priority_confidence = 0.0f;
    float category_confidence = 0.0f;
    int distraction_level = 0;
    std::chrono::milliseconds processing_time{0};
};

} // namespace work_assistant