#pragma once

#include "window_monitor_v2.h"
#include "screen_capture.h"
#include "ocr_engine.h"
#include "ai_engine.h"
#include "storage_engine.h"
#include "web_server.h"
#include <memory>
#include <vector>
#include <deque>

namespace work_assistant {

class Application {
public:
    Application();
    ~Application();
    
    bool Initialize();
    void Run();
    void Shutdown();
    
private:
    void OnWindowEvent(const WindowEvent& event);
    void OnScreenCaptureFrame(const CaptureFrame& frame);
    void ProcessFrameWithOCR(const CaptureFrame& frame);
    void ProcessContentWithAI(const OCRDocument& ocr_result, 
                             const std::string& window_title,
                             const std::string& app_name);
    
    // Analysis and reporting
    void PrintProductivitySummary();
    void PrintWorkPatterns();
    
private:
    bool m_initialized;
    std::unique_ptr<IWindowMonitor> m_windowMonitor;
    std::unique_ptr<ScreenCaptureManager> m_screenCapture;
    std::unique_ptr<OCRManager> m_ocrManager;
    std::unique_ptr<AIContentAnalyzer> m_aiAnalyzer;
    std::shared_ptr<EncryptedStorageManager> m_storageManager;
    std::unique_ptr<WebServer> m_webServer;
    
    // Activity history for pattern analysis
    std::deque<ContentAnalysis> m_recentActivities;
    static const size_t MAX_ACTIVITY_HISTORY = 50;
    
    // Statistics
    size_t m_framesProcessed;
    size_t m_ocrExtractions;
    size_t m_aiAnalyses;
    std::chrono::steady_clock::time_point m_lastSummaryTime;
};

} // namespace work_assistant