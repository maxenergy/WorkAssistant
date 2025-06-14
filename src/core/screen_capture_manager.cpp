#include "screen_capture.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef _WIN32
#include "../platform/windows/win32_screen_capture.h"
#elif defined(__linux__)
#include "../platform/linux/linux_screen_capture.h"
#endif

namespace work_assistant {

// Screen capture factory implementation
std::unique_ptr<IScreenCapture> ScreenCaptureFactory::Create() {
#ifdef _WIN32
    return std::make_unique<Win32ScreenCapture>();
#elif defined(__linux__)
    return std::make_unique<LinuxScreenCapture>();
#else
    return nullptr;
#endif
}

// Screen capture manager implementation
class ScreenCaptureManager::Impl {
public:
    Impl() 
        : m_initialized(false)
        , m_monitoring(false)
        , m_changeDetectionEnabled(true)
        , m_changeThreshold(0.05) // 5% change threshold
        , m_maxFPS(30)
        , m_captureX(0), m_captureY(0)
        , m_captureWidth(0), m_captureHeight(0)
        , m_useCustomRegion(false)
        , m_lastHash(0)
    {
    }

    ~Impl() {
        Shutdown();
    }

    bool Initialize() {
        if (m_initialized) {
            return true;
        }

        m_capture = ScreenCaptureFactory::Create();
        if (!m_capture) {
            std::cerr << "Failed to create screen capture backend" << std::endl;
            return false;
        }

        if (!m_capture->Initialize()) {
            std::cerr << "Failed to initialize screen capture" << std::endl;
            return false;
        }

        m_initialized = true;
        std::cout << "Screen capture manager initialized" << std::endl;
        return true;
    }

    void Shutdown() {
        if (!m_initialized) {
            return;
        }

        StopMonitoring();

        if (m_capture) {
            m_capture->Shutdown();
            m_capture.reset();
        }

        m_initialized = false;
        std::cout << "Screen capture manager shut down" << std::endl;
    }

    bool StartMonitoring(std::function<void(const CaptureFrame&)> callback) {
        if (!m_initialized || m_monitoring) {
            return false;
        }

        m_frameCallback = callback;
        m_monitoring = true;
        m_shutdownRequested = false;

        // Start monitoring thread
        m_monitorThread = std::thread(&Impl::MonitoringLoop, this);
        
        std::cout << "Started screen capture monitoring" << std::endl;
        return true;
    }

    void StopMonitoring() {
        if (!m_monitoring) {
            return;
        }

        m_shutdownRequested = true;
        m_monitoring = false;

        if (m_monitorThread.joinable()) {
            m_monitorThread.join();
        }

        std::cout << "Stopped screen capture monitoring" << std::endl;
    }

    bool IsMonitoring() const {
        return m_monitoring;
    }

    bool CaptureNow(CaptureFrame& frame) {
        if (!m_initialized) {
            return false;
        }

        if (m_useCustomRegion) {
            return m_capture->CaptureRegion(m_captureX, m_captureY, 
                                          m_captureWidth, m_captureHeight, frame);
        } else {
            return m_capture->CaptureDesktop(frame);
        }
    }

    bool CaptureWindow(WindowHandle windowHandle, CaptureFrame& frame) {
        if (!m_initialized) {
            return false;
        }

        return m_capture->CaptureWindow(windowHandle, frame);
    }

    void SetChangeDetectionThreshold(double threshold) {
        m_changeThreshold = std::max(0.0, std::min(1.0, threshold));
    }

    void EnableChangeDetection(bool enable) {
        m_changeDetectionEnabled = enable;
    }

    void SetMaxFPS(int fps) {
        m_maxFPS = std::max(1, std::min(120, fps));
    }

    void SetCaptureRegion(int x, int y, int width, int height) {
        m_captureX = x;
        m_captureY = y;
        m_captureWidth = width;
        m_captureHeight = height;
        m_useCustomRegion = true;
    }

    void ResetCaptureRegion() {
        m_useCustomRegion = false;
    }

private:
    void MonitoringLoop() {
        const auto frameDuration = std::chrono::milliseconds(1000 / m_maxFPS);
        auto lastCaptureTime = std::chrono::steady_clock::now();

        while (!m_shutdownRequested) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = currentTime - lastCaptureTime;

            if (elapsed >= frameDuration) {
                CaptureFrame frame;
                if (CaptureNow(frame)) {
                    bool shouldProcess = true;

                    // Check for changes if enabled
                    if (m_changeDetectionEnabled) {
                        uint64_t currentHash = capture_utils::CalculateHash(frame);
                        
                        if (m_lastHash != 0) {
                            int hammingDistance = capture_utils::CompareHashes(currentHash, m_lastHash);
                            double changeRatio = static_cast<double>(hammingDistance) / 64.0;
                            
                            if (changeRatio < m_changeThreshold) {
                                shouldProcess = false;
                            }
                        }
                        
                        m_lastHash = currentHash;
                    }

                    if (shouldProcess && m_frameCallback) {
                        m_frameCallback(frame);
                    }
                }

                lastCaptureTime = currentTime;
            }

            // Sleep for a short time to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    bool m_initialized;
    std::atomic<bool> m_monitoring;
    std::atomic<bool> m_shutdownRequested;
    
    std::unique_ptr<IScreenCapture> m_capture;
    std::function<void(const CaptureFrame&)> m_frameCallback;
    std::thread m_monitorThread;

    // Settings
    bool m_changeDetectionEnabled;
    double m_changeThreshold;
    int m_maxFPS;
    
    // Capture region
    int m_captureX, m_captureY;
    int m_captureWidth, m_captureHeight;
    bool m_useCustomRegion;

    // Change detection
    uint64_t m_lastHash;
};

// ScreenCaptureManager implementation
ScreenCaptureManager::ScreenCaptureManager() 
    : m_impl(std::make_unique<Impl>()) {
}

ScreenCaptureManager::~ScreenCaptureManager() = default;

bool ScreenCaptureManager::Initialize() {
    return m_impl->Initialize();
}

void ScreenCaptureManager::Shutdown() {
    m_impl->Shutdown();
}

bool ScreenCaptureManager::StartMonitoring(std::function<void(const CaptureFrame&)> onFrameCallback) {
    return m_impl->StartMonitoring(onFrameCallback);
}

void ScreenCaptureManager::StopMonitoring() {
    m_impl->StopMonitoring();
}

bool ScreenCaptureManager::IsMonitoring() const {
    return m_impl->IsMonitoring();
}

bool ScreenCaptureManager::CaptureNow(CaptureFrame& frame) {
    return m_impl->CaptureNow(frame);
}

bool ScreenCaptureManager::CaptureWindow(WindowHandle windowHandle, CaptureFrame& frame) {
    return m_impl->CaptureWindow(windowHandle, frame);
}

void ScreenCaptureManager::SetChangeDetectionThreshold(double threshold) {
    m_impl->SetChangeDetectionThreshold(threshold);
}

void ScreenCaptureManager::EnableChangeDetection(bool enable) {
    m_impl->EnableChangeDetection(enable);
}

void ScreenCaptureManager::SetMaxFPS(int fps) {
    m_impl->SetMaxFPS(fps);
}

void ScreenCaptureManager::SetCaptureRegion(int x, int y, int width, int height) {
    m_impl->SetCaptureRegion(x, y, width, height);
}

void ScreenCaptureManager::ResetCaptureRegion() {
    m_impl->ResetCaptureRegion();
}

// Utility functions
namespace capture_utils {

uint64_t CalculateHash(const CaptureFrame& frame) {
    if (!frame.IsValid() || frame.bytes_per_pixel < 3) {
        return 0;
    }

    // Simple dHash implementation
    const int hashSize = 8;
    const int scaledSize = hashSize + 1;
    
    // Create a scaled down grayscale version
    std::vector<uint8_t> grayscale(scaledSize * scaledSize);
    
    for (int y = 0; y < scaledSize; ++y) {
        for (int x = 0; x < scaledSize; ++x) {
            // Calculate source pixel position
            int srcX = (x * frame.width) / scaledSize;
            int srcY = (y * frame.height) / scaledSize;
            
            if (srcX >= frame.width) srcX = frame.width - 1;
            if (srcY >= frame.height) srcY = frame.height - 1;
            
            int srcIndex = (srcY * frame.width + srcX) * frame.bytes_per_pixel;
            
            // Convert to grayscale (assuming RGB/RGBA format)
            uint8_t r = frame.data[srcIndex];
            uint8_t g = frame.data[srcIndex + 1];
            uint8_t b = frame.data[srcIndex + 2];
            
            uint8_t gray = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
            grayscale[y * scaledSize + x] = gray;
        }
    }
    
    // Calculate hash
    uint64_t hash = 0;
    for (int y = 0; y < hashSize; ++y) {
        for (int x = 0; x < hashSize; ++x) {
            int current = grayscale[y * scaledSize + x];
            int next = grayscale[y * scaledSize + x + 1];
            
            if (current > next) {
                hash |= (1ULL << (y * hashSize + x));
            }
        }
    }
    
    return hash;
}

int CompareHashes(uint64_t hash1, uint64_t hash2) {
    uint64_t diff = hash1 ^ hash2;
    
    // Count set bits (Hamming distance)
    int count = 0;
    while (diff) {
        count += diff & 1;
        diff >>= 1;
    }
    
    return count;
}

bool ConvertFrame(const CaptureFrame& source, CaptureFrame& target, int targetBytesPerPixel) {
    if (!source.IsValid() || targetBytesPerPixel < 1 || targetBytesPerPixel > 4) {
        return false;
    }

    target.width = source.width;
    target.height = source.height;
    target.bytes_per_pixel = targetBytesPerPixel;
    target.timestamp = source.timestamp;
    target.data.resize(target.GetDataSize());

    // Simple conversion (just copy/truncate channels)
    for (int i = 0; i < source.width * source.height; ++i) {
        int srcIndex = i * source.bytes_per_pixel;
        int dstIndex = i * target.bytes_per_pixel;
        
        int copyBytes = std::min(source.bytes_per_pixel, target.bytes_per_pixel);
        memcpy(target.data.data() + dstIndex, 
               source.data.data() + srcIndex, copyBytes);
    }

    return true;
}

bool SaveFrameToFile(const CaptureFrame& frame, const std::string& filename) {
    // Simple PPM format output for debugging
    if (!frame.IsValid()) {
        return false;
    }

    std::FILE* file = std::fopen(filename.c_str(), "wb");
    if (!file) {
        return false;
    }

    // Write PPM header
    std::fprintf(file, "P6\n%d %d\n255\n", frame.width, frame.height);

    // Write pixel data (convert to RGB if needed)
    for (int i = 0; i < frame.width * frame.height; ++i) {
        int srcIndex = i * frame.bytes_per_pixel;
        
        if (frame.bytes_per_pixel >= 3) {
            // Assume RGB or RGBA
            std::fwrite(frame.data.data() + srcIndex, 1, 3, file);
        } else {
            // Grayscale or other format
            uint8_t gray = frame.data[srcIndex];
            std::fwrite(&gray, 1, 1, file);
            std::fwrite(&gray, 1, 1, file);
            std::fwrite(&gray, 1, 1, file);
        }
    }

    std::fclose(file);
    return true;
}

bool CropFrame(const CaptureFrame& source, CaptureFrame& target,
               int x, int y, int width, int height) {
    if (!source.IsValid() || x < 0 || y < 0 || 
        x + width > source.width || y + height > source.height) {
        return false;
    }

    target.width = width;
    target.height = height;
    target.bytes_per_pixel = source.bytes_per_pixel;
    target.timestamp = source.timestamp;
    target.data.resize(target.GetDataSize());

    for (int row = 0; row < height; ++row) {
        int srcRow = y + row;
        int srcOffset = (srcRow * source.width + x) * source.bytes_per_pixel;
        int dstOffset = row * width * source.bytes_per_pixel;
        
        memcpy(target.data.data() + dstOffset,
               source.data.data() + srcOffset,
               width * source.bytes_per_pixel);
    }

    return true;
}

} // namespace capture_utils

} // namespace work_assistant