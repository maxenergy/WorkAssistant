#include "include/ocr_engine.h"
#include "include/common_types.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace work_assistant;

int main() {
    std::cout << "Testing OCR Manager functionality..." << std::endl;
    
    // Test OCR Manager initialization
    OCRManager manager;
    
    std::cout << "Initializing OCR Manager with AUTO mode..." << std::endl;
    if (!manager.Initialize(OCRMode::AUTO)) {
        std::cerr << "Failed to initialize OCR Manager" << std::endl;
        return 1;
    }
    
    std::cout << "OCR Manager initialized successfully!" << std::endl;
    std::cout << "Current mode: " << static_cast<int>(manager.GetCurrentMode()) << std::endl;
    
    // Test mode switching
    std::cout << "\nTesting mode switching..." << std::endl;
    if (manager.SetOCRMode(OCRMode::FAST)) {
        std::cout << "Switched to FAST mode successfully" << std::endl;
    }
    
    if (manager.SetOCRMode(OCRMode::ACCURATE)) {
        std::cout << "Switched to ACCURATE mode successfully" << std::endl;
    }
    
    // Test configuration
    std::cout << "\nTesting configuration..." << std::endl;
    manager.SetLanguage("eng");
    manager.SetConfidenceThreshold(0.7f);
    manager.EnablePreprocessing(true);
    manager.EnableGPU(true);
    
    OCROptions options = manager.GetOptions();
    std::cout << "Configuration applied - Language: " << options.language 
              << ", Confidence: " << options.confidence_threshold << std::endl;
    
    // Test statistics
    std::cout << "\nTesting statistics..." << std::endl;
    auto stats = manager.GetStatistics();
    std::cout << "Total processed: " << stats.total_processed << std::endl;
    std::cout << "Successful extractions: " << stats.successful_extractions << std::endl;
    
    // Create a mock capture frame for testing
    CaptureFrame frame;
    frame.width = 800;
    frame.height = 600;
    frame.bytes_per_pixel = 4; // RGBA
    frame.data.resize(800 * 600 * 4, 128); // Fill with gray color
    frame.timestamp = std::chrono::system_clock::now();
    
    std::cout << "\nTesting text extraction with mock frame..." << std::endl;
    OCRDocument document = manager.ExtractText(frame);
    std::cout << "Extraction result - Text blocks: " << document.text_blocks.size() 
              << ", Confidence: " << document.overall_confidence << std::endl;
    
    // Test keyword extraction
    std::cout << "\nTesting keyword extraction..." << std::endl;
    auto keywords = manager.ExtractKeywords(document);
    std::cout << "Extracted " << keywords.size() << " keywords" << std::endl;
    
    // Test multimodal capabilities
    std::cout << "\nTesting multimodal capabilities..." << std::endl;
    std::string description = manager.DescribeImage(frame);
    std::cout << "Image description: " << description << std::endl;
    
    std::string answer = manager.AnswerQuestion(frame, "What do you see in this image?");
    std::cout << "Q&A response: " << answer << std::endl;
    
    // Test async processing
    std::cout << "\nTesting async processing..." << std::endl;
    auto future = manager.ExtractTextAsync(frame);
    
    // Do other work while processing...
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Get the result
    OCRDocument async_document = future.get();
    std::cout << "Async extraction completed - Text blocks: " << async_document.text_blocks.size() << std::endl;
    
    // Test final statistics
    std::cout << "\nFinal statistics:" << std::endl;
    stats = manager.GetStatistics();
    std::cout << "Total processed: " << stats.total_processed << std::endl;
    std::cout << "Successful extractions: " << stats.successful_extractions << std::endl;
    std::cout << "Average processing time: " << stats.average_processing_time_ms << "ms" << std::endl;
    std::cout << "Average confidence: " << stats.average_confidence << std::endl;
    
    // Shutdown
    manager.Shutdown();
    std::cout << "\nOCR Manager test completed successfully!" << std::endl;
    
    return 0;
}