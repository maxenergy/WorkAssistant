#pragma once

#include "common_types.h"
#include <memory>
#include <vector>
#include <chrono>
#include <cstdint>
#include <functional>

namespace work_assistant {

// Monitor information
struct MonitorInfo {
    int id;
    std::string name;
    int x, y;           // Position
    int width, height;  // Dimensions
    bool is_primary;
};

// Screen capture interface
class IScreenCapture {
public:
    virtual ~IScreenCapture() = default;

    // Initialize capture system
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Get available monitors
    virtual std::vector<MonitorInfo> GetMonitors() const = 0;

    // Capture entire desktop (all monitors)
    virtual bool CaptureDesktop(CaptureFrame& frame) = 0;
    
    // Capture specific monitor
    virtual bool CaptureMonitor(int monitorId, CaptureFrame& frame) = 0;
    
    // Capture window by handle
    virtual bool CaptureWindow(WindowHandle windowHandle, CaptureFrame& frame) = 0;
    
    // Capture region of screen
    virtual bool CaptureRegion(int x, int y, int width, int height, CaptureFrame& frame) = 0;

    // Check if capture system supports hardware acceleration
    virtual bool SupportsHardwareAcceleration() const = 0;

    // Set capture quality/performance settings
    virtual void SetCaptureSettings(bool useHardwareAcceleration, int maxFPS = 30) = 0;
};

// Screen capture factory
class ScreenCaptureFactory {
public:
    static std::unique_ptr<IScreenCapture> Create();
};

// Screen capture manager with change detection
class ScreenCaptureManager {
public:
    ScreenCaptureManager();
    ~ScreenCaptureManager();

    // Initialize with capture backend
    bool Initialize();
    void Shutdown();

    // Start/stop capture monitoring
    bool StartMonitoring(std::function<void(const CaptureFrame&)> onFrameCallback);
    void StopMonitoring();
    bool IsMonitoring() const;

    // Manual capture
    bool CaptureNow(CaptureFrame& frame);
    bool CaptureWindow(WindowHandle windowHandle, CaptureFrame& frame);

    // Change detection settings
    void SetChangeDetectionThreshold(double threshold); // 0.0 - 1.0
    void EnableChangeDetection(bool enable);

    // Performance settings
    void SetMaxFPS(int fps);
    void SetCaptureRegion(int x, int y, int width, int height);
    void ResetCaptureRegion(); // Capture full desktop

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// Utility functions for image processing
namespace capture_utils {

// Calculate perceptual hash (dHash) for change detection
uint64_t CalculateHash(const CaptureFrame& frame);

// Calculate difference between two hashes (Hamming distance)
int CompareHashes(uint64_t hash1, uint64_t hash2);

// Convert frame to different format
bool ConvertFrame(const CaptureFrame& source, CaptureFrame& target, 
                  int targetBytesPerPixel);

// Save frame to file (for debugging)
bool SaveFrameToFile(const CaptureFrame& frame, const std::string& filename);

// Crop frame to region
bool CropFrame(const CaptureFrame& source, CaptureFrame& target,
               int x, int y, int width, int height);

} // namespace capture_utils

} // namespace work_assistant