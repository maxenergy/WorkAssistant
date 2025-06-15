#include "application.h"
#include "event_manager.h"
#include "directory_manager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <unordered_map>

namespace work_assistant {

Application::Application() 
    : m_initialized(false)
    , m_framesProcessed(0)
    , m_ocrExtractions(0)
    , m_aiAnalyses(0)
    , m_lastSummaryTime(std::chrono::steady_clock::now()) {
}

Application::~Application() {
    Shutdown();
}

bool Application::Initialize() {
    if (m_initialized) {
        return true;
    }

    std::cout << "Initializing Work Study Assistant..." << std::endl;
    
    // Initialize directory structure first
    if (!DirectoryManager::InitializeDirectories()) {
        std::cerr << "Failed to initialize directory structure" << std::endl;
        // Don't fail completely, but warn user
    } else {
        std::cout << "Directory structure initialized successfully" << std::endl;
    }

    // Initialize window monitor
    m_windowMonitor = WindowMonitorFactory::Create();
    if (!m_windowMonitor) {
        std::cerr << "Failed to create window monitor" << std::endl;
        // Don't fail completely - continue without window monitoring
    } else if (!m_windowMonitor->Initialize()) {
        std::cerr << "Failed to initialize window monitor" << std::endl;
        // Don't fail completely - continue without window monitoring
        m_windowMonitor.reset();
    }

    // Initialize screen capture
    m_screenCapture = std::make_unique<ScreenCaptureManager>();
    if (!m_screenCapture->Initialize()) {
        std::cerr << "Failed to initialize screen capture" << std::endl;
        // Don't fail completely if screen capture fails
        m_screenCapture.reset();
    }

    // Initialize OCR manager
    m_ocrManager = std::make_unique<OCRManager>();
    if (!m_ocrManager->Initialize(OCREngineFactory::EngineType::AUTO_SELECT)) {
        std::cerr << "Failed to initialize OCR manager" << std::endl;
        // Don't fail completely if OCR fails
        m_ocrManager.reset();
    }

    // Initialize AI content analyzer
    m_aiAnalyzer = std::make_unique<AIContentAnalyzer>();
    std::string model_path = DirectoryManager::JoinPath(DirectoryManager::GetModelsDirectory(), "qwen2.5-0.5b-instruct-q4_k_m.gguf");
    if (!m_aiAnalyzer->Initialize(model_path, AIEngineFactory::EngineType::LLAMA_CPP)) {
        std::cerr << "Failed to initialize AI analyzer" << std::endl;
        // Don't fail completely if AI fails
        m_aiAnalyzer.reset();
    } else {
        std::cout << "AI Content Analyzer ready for intelligent classification" << std::endl;
    }

    // Initialize encrypted storage manager
    m_storageManager = std::make_shared<EncryptedStorageManager>();
    StorageConfig storage_config;
    storage_config.storage_path = DirectoryManager::GetDataDirectory();
    storage_config.database_name = "work_assistant.db";
    storage_config.master_password = "default_password_2024"; // In production, get from user
    storage_config.security_level = SecurityLevel::STANDARD;
    
    if (!m_storageManager->Initialize(storage_config)) {
        std::cerr << "Failed to initialize storage manager" << std::endl;
        // Don't fail completely if storage fails
        m_storageManager.reset();
    } else {
        std::cout << "Encrypted Storage Manager ready for secure data persistence" << std::endl;
        m_storageManager->StartSession("main_session");
    }

    // Initialize web server
    m_webServer = std::make_unique<WebServer>();
    WebServerConfig web_config;
    web_config.host = "127.0.0.1";
    web_config.port = 8080;
    web_config.static_files_path = DirectoryManager::JoinPath(DirectoryManager::GetDataDirectory(), "web/static");
    web_config.enable_cors = true;
    web_config.enable_websocket = true;
    
    if (!m_webServer->Initialize(web_config, m_storageManager)) {
        std::cerr << "Failed to initialize web server" << std::endl;
        // Don't fail completely if web server fails
        m_webServer.reset();
    } else {
        std::cout << "Web Server ready at http://" << web_config.host << ":" << web_config.port << std::endl;
    }

    // Set up event handling
    EventManager::GetInstance().Subscribe<WindowEvent>(
        [this](const WindowEvent& event) {
            OnWindowEvent(event);
        }
    );

    m_initialized = true;
    std::cout << "Application initialized successfully" << std::endl;
    return true;
}

void Application::Run() {
    if (!m_initialized) {
        std::cerr << "Application not initialized" << std::endl;
        return;
    }

    std::cout << "Starting application..." << std::endl;

    // Start window monitoring
    if (!m_windowMonitor->StartMonitoring()) {
        std::cerr << "Failed to start window monitoring" << std::endl;
        return;
    }

    // Start screen capture if available
    if (m_screenCapture) {
        auto callback = [this](const CaptureFrame& frame) {
            OnScreenCaptureFrame(frame);
        };
        
        if (!m_screenCapture->StartMonitoring(callback)) {
            std::cerr << "Failed to start screen capture monitoring" << std::endl;
        } else {
            std::cout << "Screen capture monitoring started" << std::endl;
        }
    }

    // Start web server if available
    if (m_webServer) {
        if (!m_webServer->Start()) {
            std::cerr << "Failed to start web server" << std::endl;
        } else {
            std::cout << "Web server started successfully" << std::endl;
        }
    }

    std::cout << "Application running. Press Ctrl+C to exit..." << std::endl;

    // Main application loop
    while (true) {
        // Process events
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check for shutdown conditions
        // For now, run indefinitely until signal
    }
}

void Application::Shutdown() {
    if (!m_initialized) {
        return;
    }

    std::cout << "Shutting down application..." << std::endl;

    // Stop monitoring
    if (m_windowMonitor) {
        m_windowMonitor->StopMonitoring();
        m_windowMonitor->Shutdown();
        m_windowMonitor.reset();
    }

    if (m_screenCapture) {
        m_screenCapture->StopMonitoring();
        m_screenCapture->Shutdown();
        m_screenCapture.reset();
    }

    if (m_ocrManager) {
        m_ocrManager->Shutdown();
        m_ocrManager.reset();
    }

    if (m_aiAnalyzer) {
        m_aiAnalyzer->Shutdown();
        m_aiAnalyzer.reset();
    }

    if (m_webServer) {
        m_webServer->Stop();
        m_webServer->Shutdown();
        m_webServer.reset();
    }

    if (m_storageManager) {
        m_storageManager->EndSession();
        m_storageManager->Shutdown();
        m_storageManager.reset();
    }

    m_initialized = false;
    std::cout << "Application shut down" << std::endl;
}

void Application::OnWindowEvent(const WindowEvent& event) {
    std::cout << "Window Event: ";
    switch (event.type) {
        case WindowEventType::WINDOW_CREATED:
            std::cout << "CREATED";
            break;
        case WindowEventType::WINDOW_DESTROYED:
            std::cout << "DESTROYED";
            break;
        case WindowEventType::WINDOW_FOCUSED:
            std::cout << "FOCUSED";
            break;
        case WindowEventType::WINDOW_MINIMIZED:
            std::cout << "MINIMIZED";
            break;
        case WindowEventType::WINDOW_RESTORED:
            std::cout << "RESTORED";
            break;
    }
    
    std::cout << " - " << event.window_info.title 
              << " (" << event.window_info.process_name << ")" << std::endl;

    // Store window event in encrypted storage
    if (m_storageManager) {
        m_storageManager->StoreWindowEvent(event, event.window_info);
    }

    // Send real-time update to web clients
    if (m_webServer) {
        m_webServer->OnWindowEvent(event, event.window_info);
    }

    // Trigger screen capture on focus change
    if (event.type == WindowEventType::WINDOW_FOCUSED && m_screenCapture) {
        CaptureFrame frame;
        if (m_screenCapture->CaptureWindow(event.window_info.window_handle, frame)) {
            std::cout << "Captured window: " << frame.width << "x" << frame.height << std::endl;
            
            // Store screen capture
            if (m_storageManager) {
                m_storageManager->StoreScreenCapture(frame, event.window_info.title);
            }
        }
    }
}

void Application::OnScreenCaptureFrame(const CaptureFrame& frame) {
    m_framesProcessed++;
    
    // Log frame info periodically
    if (m_framesProcessed % 30 == 0) { // Log every 30 frames
        std::cout << "Screen capture frame: " << frame.width << "x" << frame.height 
                  << " (" << frame.data.size() << " bytes)" << std::endl;
        
        if (m_ocrManager) {
            auto stats = m_ocrManager->GetStatistics();
            std::cout << "OCR Stats: " << stats.successful_extractions 
                      << "/" << stats.total_processed << " successful, "
                      << "avg time: " << stats.average_processing_time_ms << "ms, "
                      << "avg confidence: " << stats.average_confidence << std::endl;
        }
        
        if (m_aiAnalyzer) {
            auto aiStats = m_aiAnalyzer->GetStatistics();
            std::cout << "AI Stats: " << aiStats.successful_classifications
                      << "/" << aiStats.total_analyzed << " classified, "
                      << "avg time: " << aiStats.average_processing_time_ms << "ms, "
                      << "avg confidence: " << aiStats.average_confidence << std::endl;
        }
        
        // Print productivity summary every 5 minutes
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::minutes>(now - m_lastSummaryTime).count() >= 5) {
            PrintProductivitySummary();
            PrintWorkPatterns();
            m_lastSummaryTime = now;
        }
    }

    // Process frame with OCR (every 10th frame to reduce load)
    if (m_framesProcessed % 10 == 0) {
        ProcessFrameWithOCR(frame);
    }
}

void Application::ProcessFrameWithOCR(const CaptureFrame& frame) {
    if (!m_ocrManager) {
        return;
    }

    // Process OCR asynchronously to avoid blocking
    auto future = m_ocrManager->ExtractTextAsync(frame);
    
    // In a real application, you would store this future and process results later
    // For demonstration, we'll just log when text is found
    std::thread([this, future = std::move(future)]() mutable {
        try {
            OCRDocument document = future.get();
            
            if (!document.text_blocks.empty()) {
                m_ocrExtractions++;
                
                std::string text = document.GetOrderedText();
                if (!text.empty() && ocr_utils::IsTextMeaningful(text)) {
                    std::cout << "OCR Extracted: \"" << text.substr(0, 50);
                    if (text.length() > 50) std::cout << "...";
                    std::cout << "\" (confidence: " << document.overall_confidence << ")" << std::endl;
                    
                    // Extract keywords for content analysis
                    auto keywords = m_ocrManager->ExtractKeywords(document);
                    if (!keywords.empty()) {
                        std::cout << "Keywords: ";
                        for (size_t i = 0; i < std::min(keywords.size(), size_t(5)); ++i) {
                            std::cout << keywords[i];
                            if (i < std::min(keywords.size(), size_t(5)) - 1) std::cout << ", ";
                        }
                        std::cout << std::endl;
                    }
                    
                    // Store OCR result
                    if (this->m_storageManager) {
                        this->m_storageManager->StoreOCRResult(document, "Screen Capture");
                    }

                    // Send real-time update to web clients
                    if (this->m_webServer) {
                        this->m_webServer->OnOCRResult(document);
                    }
                    
                    // Process with AI for content classification
                    if (this->m_aiAnalyzer) {
                        this->ProcessContentWithAI(document, "Screen Capture", "Unknown");
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "OCR processing error: " << e.what() << std::endl;
        }
    }).detach();
}

void Application::ProcessContentWithAI(const OCRDocument& ocr_result,
                                      const std::string& window_title,
                                      const std::string& app_name) {
    if (!m_aiAnalyzer) {
        return;
    }

    // Process AI analysis asynchronously
    auto future = m_aiAnalyzer->AnalyzeWindowAsync(ocr_result, window_title, app_name);
    
    std::thread([this, future = std::move(future)]() mutable {
        try {
            ContentAnalysis analysis = future.get();
            m_aiAnalyses++;
            
            // Store AI analysis in encrypted storage
            if (this->m_storageManager) {
                this->m_storageManager->StoreAIAnalysis(analysis);
            }

            // Send real-time update to web clients
            if (this->m_webServer) {
                this->m_webServer->OnAIAnalysis(analysis);
            }
            
            // Add to activity history
            m_recentActivities.push_back(analysis);
            if (m_recentActivities.size() > MAX_ACTIVITY_HISTORY) {
                m_recentActivities.pop_front();
            }
            
            // Log interesting classifications
            if (analysis.content_type != ContentType::UNKNOWN) {
                std::cout << "🤖 AI Analysis: " 
                          << ai_utils::ContentTypeToString(analysis.content_type)
                          << " → " << ai_utils::WorkCategoryToString(analysis.work_category);
                
                if (analysis.is_productive) {
                    std::cout << " ✅ Productive";
                } else {
                    std::cout << " ⏱️  Break";
                }
                
                std::cout << " (Priority: " << static_cast<int>(analysis.priority)
                          << ", Confidence: " << std::fixed << std::setprecision(2) 
                          << analysis.classification_confidence << ")" << std::endl;
                
                // Show productivity insights
                if (m_recentActivities.size() >= 5) {
                    int productivity_score = m_aiAnalyzer->CalculateProductivityScore(
                        std::vector<ContentAnalysis>(m_recentActivities.begin(), m_recentActivities.end())
                    );
                    
                    if (m_aiAnalyses % 10 == 0) { // Every 10 analyses
                        std::cout << "📊 Productivity Score: " << productivity_score << "/100" << std::endl;
                    }
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "AI processing error: " << e.what() << std::endl;
        }
    }).detach();
}

void Application::PrintProductivitySummary() {
    if (m_recentActivities.empty() || !m_aiAnalyzer) {
        return;
    }

    std::cout << "\n=== 📈 PRODUCTIVITY SUMMARY ===" << std::endl;
    
    // Calculate overall productivity
    std::vector<ContentAnalysis> activities(m_recentActivities.begin(), m_recentActivities.end());
    int productivity_score = m_aiAnalyzer->CalculateProductivityScore(activities);
    
    std::cout << "Overall Productivity Score: " << productivity_score << "/100 ";
    if (productivity_score >= 80) std::cout << "🔥 Excellent!";
    else if (productivity_score >= 60) std::cout << "👍 Good";
    else if (productivity_score >= 40) std::cout << "📈 Room for improvement";
    else std::cout << "⚠️ Low productivity detected";
    std::cout << std::endl;
    
    // Activity type breakdown
    std::unordered_map<ContentType, int> type_counts;
    std::unordered_map<WorkCategory, int> category_counts;
    int productive_count = 0;
    
    for (const auto& activity : activities) {
        type_counts[activity.content_type]++;
        category_counts[activity.work_category]++;
        if (m_aiAnalyzer->IsProductiveActivity(activity)) {
            productive_count++;
        }
    }
    
    std::cout << "Activity Breakdown:" << std::endl;
    for (const auto& [type, count] : type_counts) {
        if (count > 0) {
            float percentage = (static_cast<float>(count) / activities.size()) * 100.0f;
            std::cout << "  " << ai_utils::ContentTypeToString(type) 
                      << ": " << std::fixed << std::setprecision(1) << percentage << "%" << std::endl;
        }
    }
    
    float productive_ratio = static_cast<float>(productive_count) / activities.size();
    std::cout << "Productive Time: " << std::fixed << std::setprecision(1) 
              << (productive_ratio * 100.0f) << "%" << std::endl;
}

void Application::PrintWorkPatterns() {
    if (m_recentActivities.empty() || !m_aiAnalyzer) {
        return;
    }

    std::cout << "\n=== 🎯 WORK PATTERNS ===" << std::endl;
    
    std::vector<ContentAnalysis> activities(m_recentActivities.begin(), m_recentActivities.end());
    auto patterns = m_aiAnalyzer->DetectWorkPatterns(activities);
    
    if (patterns.empty()) {
        std::cout << "No significant patterns detected yet." << std::endl;
    } else {
        for (const auto& pattern : patterns) {
            std::cout << "• " << pattern << std::endl;
        }
    }
    
    // Predict next activity
    ContentType predicted = m_aiAnalyzer->PredictNextActivity(activities);
    if (predicted != ContentType::UNKNOWN) {
        std::cout << "Predicted next activity: " 
                  << ai_utils::ContentTypeToString(predicted) << std::endl;
    }
    
    std::cout << "================================\n" << std::endl;
}

} // namespace work_assistant