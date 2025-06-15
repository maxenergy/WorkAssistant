#include "include/ocr_engine.h"
#include "include/common_types.h"
#include <iostream>

using namespace work_assistant;

int main() {
    std::cout << "Testing header compilation..." << std::endl;
    
    // Test enum compilation
    OCRMode mode = OCRMode::AUTO;
    std::cout << "OCR Mode: " << static_cast<int>(mode) << std::endl;
    
    // Test OCROptions
    OCROptions options;
    options.preferred_mode = OCRMode::FAST;
    std::cout << "OCR Options configured successfully" << std::endl;
    
    // Test OCRDocument
    OCRDocument doc;
    doc.full_text = "Test text";
    std::cout << "Document text: " << doc.GetOrderedText() << std::endl;
    
    std::cout << "Header compilation test completed successfully!" << std::endl;
    return 0;
}