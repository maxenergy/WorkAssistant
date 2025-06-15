#include "include/ocr_engine.h"
#include "include/common_types.h"
#include <iostream>
// #include <opencv2/opencv.hpp>  // We'll test without OpenCV for now

using namespace work_assistant;

int main() {
    std::cout << "Testing OCR functionality..." << std::endl;
    
    // Test enum compilation
    OCRMode mode = OCRMode::AUTO;
    std::cout << "OCR Mode: " << static_cast<int>(mode) << std::endl;
    
    // Test OCROptions
    OCROptions options;
    options.preferred_mode = OCRMode::FAST;
    std::cout << "OCR Options configured successfully" << std::endl;
    
    // Test OCREngineFactory
    auto engines = OCREngineFactory::GetAvailableEngines();
    std::cout << "Available engines: " << engines.size() << std::endl;
    
    // Test OCRDocument
    OCRDocument doc;
    doc.full_text = "Test text";
    std::cout << "Document text: " << doc.GetOrderedText() << std::endl;
    
    std::cout << "OCR test completed successfully!" << std::endl;
    return 0;
}