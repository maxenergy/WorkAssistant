#include "screen_capture.h"
#include <ScreenCapture.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>

namespace work_assistant {

// Implementation using screen_capture_lite library
class ScreenCaptureLiteImpl : public IScreenCapture {
public:
    ScreenCaptureLiteImpl() 
        : m_initialized(false)
        , m_useHardwareAcceleration(true)
        , m_maxFPS(30)
    {}

    ~ScreenCaptureLiteImpl() override {
        Shutdown();
    }

    bool Initialize() override {
        if (m_initialized) {
            return true;
        }

        try {
            // Get all monitors
            auto monitors = SL::Screen_Capture::GetMonitors();
            if (monitors.empty()) {
                std::cerr << "No monitors found" << std::endl;
                return false;
            }

            // Store monitor information
            m_monitors.clear();
            int id = 0;
            for (const auto& mon : monitors) {
                MonitorInfo info;
                info.id = id++;
                info.name = mon.Name;
                info.x = mon.OffsetX;
                info.y = mon.OffsetY;
                info.width = mon.Width;
                info.height = mon.Height;
                info.is_primary = (id == 1); // First monitor is primary
                m_monitors.push_back(info);
            }

            m_initialized = true;
            std::cout << "Screen capture initialized with " << m_monitors.size() << " monitors" << std::endl;
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to initialize screen capture: " << e.what() << std::endl;
            return false;
        }
    }

    void Shutdown() override {
        if (!m_initialized) {
            return;
        }

        m_monitors.clear();
        m_initialized = false;
        std::cout << "Screen capture shut down" << std::endl;
    }

    std::vector<MonitorInfo> GetMonitors() const override {
        return m_monitors;
    }

    bool CaptureDesktop(CaptureFrame& frame) override {
        if (!m_initialized) {
            return false;
        }

        try {
            std::mutex captureMutex;
            std::condition_variable captureCV;
            bool captureComplete = false;
            bool captureSuccess = false;

            auto monitors = SL::Screen_Capture::GetMonitors();
            if (monitors.empty()) {
                return false;
            }

            // Create capture configuration for all monitors
            auto captureConfig = SL::Screen_Capture::CreateCaptureConfiguration([&]() {
                return monitors;
            });

            captureConfig->onNewFrame([&](const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Monitor& monitor) {
                std::lock_guard<std::mutex> lock(captureMutex);
                
                // Copy image data to frame
                frame.width = img.Width;
                frame.height = img.Height;
                frame.stride = img.Stride;
                frame.bytes_per_pixel = img.BytesToNextRow / img.Width;
                frame.format = (frame.bytes_per_pixel == 4) ? ImageFormat::RGBA : ImageFormat::RGB;
                
                size_t dataSize = img.Height * img.Stride;
                frame.data.resize(dataSize);
                std::memcpy(frame.data.data(), img.Data, dataSize);
                
                frame.timestamp = std::chrono::system_clock::now();
                captureSuccess = true;
                captureComplete = true;
                captureCV.notify_one();
            });

            // Start capture
            auto captureManager = captureConfig->start_capturing();

            // Wait for capture to complete
            std::unique_lock<std::mutex> lock(captureMutex);
            captureCV.wait_for(lock, std::chrono::milliseconds(100), [&] { return captureComplete; });

            return captureSuccess;
        }
        catch (const std::exception& e) {
            std::cerr << "Desktop capture failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool CaptureMonitor(int monitorId, CaptureFrame& frame) override {
        if (!m_initialized || monitorId < 0 || monitorId >= static_cast<int>(m_monitors.size())) {
            return false;
        }

        try {
            std::mutex captureMutex;
            std::condition_variable captureCV;
            bool captureComplete = false;
            bool captureSuccess = false;

            auto monitors = SL::Screen_Capture::GetMonitors();
            if (monitorId >= static_cast<int>(monitors.size())) {
                return false;
            }

            // Create vector with single monitor
            std::vector<SL::Screen_Capture::Monitor> targetMonitor = { monitors[monitorId] };

            auto captureConfig = SL::Screen_Capture::CreateCaptureConfiguration([&]() {
                return targetMonitor;
            });

            captureConfig->onNewFrame([&](const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Monitor& monitor) {
                std::lock_guard<std::mutex> lock(captureMutex);
                
                // Copy image data to frame
                frame.width = img.Width;
                frame.height = img.Height;
                frame.stride = img.Stride;
                frame.bytes_per_pixel = img.BytesToNextRow / img.Width;
                frame.format = (frame.bytes_per_pixel == 4) ? ImageFormat::RGBA : ImageFormat::RGB;
                
                size_t dataSize = img.Height * img.Stride;
                frame.data.resize(dataSize);
                std::memcpy(frame.data.data(), img.Data, dataSize);
                
                frame.timestamp = std::chrono::system_clock::now();
                captureSuccess = true;
                captureComplete = true;
                captureCV.notify_one();
            });

            // Start capture
            auto captureManager = captureConfig->start_capturing();

            // Wait for capture to complete
            std::unique_lock<std::mutex> lock(captureMutex);
            captureCV.wait_for(lock, std::chrono::milliseconds(100), [&] { return captureComplete; });

            return captureSuccess;
        }
        catch (const std::exception& e) {
            std::cerr << "Monitor capture failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool CaptureWindow(WindowHandle windowHandle, CaptureFrame& frame) override {
        if (!m_initialized) {
            return false;
        }

        try {
            std::mutex captureMutex;
            std::condition_variable captureCV;
            bool captureComplete = false;
            bool captureSuccess = false;

            // Get window list
            auto windows = SL::Screen_Capture::GetWindows();
            
            // Find window by handle
            auto it = std::find_if(windows.begin(), windows.end(), 
                [windowHandle](const SL::Screen_Capture::Window& w) {
                    return reinterpret_cast<WindowHandle>(w.Handle) == windowHandle;
                });

            if (it == windows.end()) {
                return false;
            }

            std::vector<SL::Screen_Capture::Window> targetWindow = { *it };

            auto captureConfig = SL::Screen_Capture::CreateCaptureConfiguration([&]() {
                return targetWindow;
            });

            captureConfig->onNewFrame([&](const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Window& window) {
                std::lock_guard<std::mutex> lock(captureMutex);
                
                // Copy image data to frame
                frame.width = img.Width;
                frame.height = img.Height;
                frame.stride = img.Stride;
                frame.bytes_per_pixel = img.BytesToNextRow / img.Width;
                frame.format = (frame.bytes_per_pixel == 4) ? ImageFormat::RGBA : ImageFormat::RGB;
                
                size_t dataSize = img.Height * img.Stride;
                frame.data.resize(dataSize);
                std::memcpy(frame.data.data(), img.Data, dataSize);
                
                frame.timestamp = std::chrono::system_clock::now();
                captureSuccess = true;
                captureComplete = true;
                captureCV.notify_one();
            });

            // Start capture
            auto captureManager = captureConfig->start_capturing();

            // Wait for capture to complete
            std::unique_lock<std::mutex> lock(captureMutex);
            captureCV.wait_for(lock, std::chrono::milliseconds(100), [&] { return captureComplete; });

            return captureSuccess;
        }
        catch (const std::exception& e) {
            std::cerr << "Window capture failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool CaptureRegion(int x, int y, int width, int height, CaptureFrame& frame) override {
        if (!m_initialized) {
            return false;
        }

        // Capture desktop first
        CaptureFrame desktopFrame;
        if (!CaptureDesktop(desktopFrame)) {
            return false;
        }

        // Crop to region
        return capture_utils::CropFrame(desktopFrame, frame, x, y, width, height);
    }

    bool SupportsHardwareAcceleration() const override {
        // screen_capture_lite uses hardware acceleration when available
        return true;
    }

    void SetCaptureSettings(bool useHardwareAcceleration, int maxFPS) override {
        m_useHardwareAcceleration = useHardwareAcceleration;
        m_maxFPS = maxFPS;
    }

private:
    bool m_initialized;
    std::vector<MonitorInfo> m_monitors;
    bool m_useHardwareAcceleration;
    int m_maxFPS;
};

// Update factory to use screen_capture_lite implementation
std::unique_ptr<IScreenCapture> ScreenCaptureFactory::Create() {
    return std::make_unique<ScreenCaptureLiteImpl>();
}

} // namespace work_assistant