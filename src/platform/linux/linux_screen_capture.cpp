#include "linux_screen_capture.h"
#include <iostream>
#include <chrono>
#include <cstring>

namespace work_assistant {

LinuxScreenCapture::LinuxScreenCapture()
    : m_initialized(false)
    , m_display(nullptr)
    , m_rootWindow(0)
    , m_screenWidth(0)
    , m_screenHeight(0)
    , m_useHardwareAcceleration(false)
    , m_maxFPS(30)
{
}

LinuxScreenCapture::~LinuxScreenCapture() {
    Shutdown();
}

bool LinuxScreenCapture::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Open X11 display
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        std::cerr << "Failed to open X11 display" << std::endl;
        return false;
    }

    // Get root window and screen dimensions
    int screen = DefaultScreen(m_display);
    m_rootWindow = RootWindow(m_display, screen);
    m_screenWidth = DisplayWidth(m_display, screen);
    m_screenHeight = DisplayHeight(m_display, screen);

    if (m_screenWidth <= 0 || m_screenHeight <= 0) {
        std::cerr << "Invalid screen dimensions: " << m_screenWidth << "x" << m_screenHeight << std::endl;
        XCloseDisplay(m_display);
        m_display = nullptr;
        return false;
    }

    m_initialized = true;
    std::cout << "Linux screen capture initialized (" << m_screenWidth << "x" << m_screenHeight << ")" << std::endl;
    return true;
}

void LinuxScreenCapture::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_display) {
        XCloseDisplay(m_display);
        m_display = nullptr;
    }

    m_initialized = false;
    std::cout << "Linux screen capture shut down" << std::endl;
}

std::vector<MonitorInfo> LinuxScreenCapture::GetMonitors() const {
    std::vector<MonitorInfo> monitors;
    
    if (!m_initialized) {
        return monitors;
    }

    // For now, return single monitor (root window)
    // TODO: Add multi-monitor support using XRandR
    MonitorInfo monitor;
    monitor.id = 0;
    monitor.name = "Primary Display";
    monitor.x = 0;
    monitor.y = 0;
    monitor.width = m_screenWidth;
    monitor.height = m_screenHeight;
    monitor.is_primary = true;
    
    monitors.push_back(monitor);
    return monitors;
}

bool LinuxScreenCapture::CaptureDesktop(CaptureFrame& frame) {
    return CaptureRegion(0, 0, m_screenWidth, m_screenHeight, frame);
}

bool LinuxScreenCapture::CaptureMonitor(int monitorId, CaptureFrame& frame) {
    // For now, only support primary monitor
    if (monitorId != 0) {
        return false;
    }
    return CaptureDesktop(frame);
}

bool LinuxScreenCapture::CaptureWindow(uint64_t windowHandle, CaptureFrame& frame) {
    if (!m_initialized) {
        return false;
    }

    Window window = static_cast<Window>(windowHandle);
    
    // Get window geometry
    Window root;
    int x, y;
    unsigned int width, height, border, depth;
    
    if (!XGetGeometry(m_display, window, &root, &x, &y, &width, &height, &border, &depth)) {
        std::cerr << "Failed to get window geometry" << std::endl;
        return false;
    }

    // Get window position relative to root
    Window child;
    int rootX, rootY;
    if (!XTranslateCoordinates(m_display, window, m_rootWindow, 0, 0, &rootX, &rootY, &child)) {
        std::cerr << "Failed to translate window coordinates" << std::endl;
        return false;
    }

    return CaptureRegion(rootX, rootY, width, height, frame);
}

bool LinuxScreenCapture::CaptureRegion(int x, int y, int width, int height, CaptureFrame& frame) {
    if (!m_initialized) {
        return false;
    }

    return CaptureX11Region(x, y, width, height, frame);
}

bool LinuxScreenCapture::SupportsHardwareAcceleration() const {
    return false; // Not implemented yet
}

void LinuxScreenCapture::SetCaptureSettings(bool useHardwareAcceleration, int maxFPS) {
    m_useHardwareAcceleration = useHardwareAcceleration;
    m_maxFPS = maxFPS;
}

bool LinuxScreenCapture::CaptureX11Region(int x, int y, int width, int height, CaptureFrame& frame) {
    if (!m_display) {
        return false;
    }

    // Clamp region to screen bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > m_screenWidth) { width = m_screenWidth - x; }
    if (y + height > m_screenHeight) { height = m_screenHeight - y; }

    if (width <= 0 || height <= 0) {
        return false;
    }

    // Capture screen using XGetImage
    XImage* image = XGetImage(m_display, m_rootWindow, x, y, width, height, AllPlanes, ZPixmap);
    if (!image) {
        std::cerr << "Failed to capture screen region" << std::endl;
        return false;
    }

    bool success = ConvertXImageToFrame(image, frame);
    XDestroyImage(image);

    return success;
}

bool LinuxScreenCapture::ConvertXImageToFrame(XImage* image, CaptureFrame& frame) {
    if (!image) {
        return false;
    }

    frame.width = image->width;
    frame.height = image->height;
    frame.bytes_per_pixel = 4; // RGBA
    frame.timestamp = std::chrono::system_clock::now();
    frame.data.resize(frame.GetDataSize());

    // Convert XImage data to RGBA format
    for (int y = 0; y < image->height; ++y) {
        for (int x = 0; x < image->width; ++x) {
            unsigned long pixel = XGetPixel(image, x, y);
            
            // Extract RGB components (assuming 24-bit or 32-bit display)
            uint8_t r, g, b, a = 255;
            
            if (image->depth == 24 || image->depth == 32) {
                // Typical RGB layout
                r = (pixel >> 16) & 0xFF;
                g = (pixel >> 8) & 0xFF;
                b = pixel & 0xFF;
            } else {
                // Fallback for other depths
                r = g = b = static_cast<uint8_t>(pixel & 0xFF);
            }

            int frameIndex = (y * image->width + x) * 4;
            frame.data[frameIndex + 0] = r;
            frame.data[frameIndex + 1] = g;
            frame.data[frameIndex + 2] = b;
            frame.data[frameIndex + 3] = a;
        }
    }

    return true;
}

} // namespace work_assistant