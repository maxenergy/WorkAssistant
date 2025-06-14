#pragma once

#include "screen_capture.h"
#include "common_types.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <memory>
#include <vector>

namespace work_assistant {

class LinuxScreenCapture : public IScreenCapture {
public:
    LinuxScreenCapture();
    ~LinuxScreenCapture() override;

    bool Initialize() override;
    void Shutdown() override;

    std::vector<MonitorInfo> GetMonitors() const override;
    bool CaptureDesktop(CaptureFrame& frame) override;
    bool CaptureMonitor(int monitorId, CaptureFrame& frame) override;
    bool CaptureWindow(WindowHandle windowHandle, CaptureFrame& frame) override;
    bool CaptureRegion(int x, int y, int width, int height, CaptureFrame& frame) override;

    bool SupportsHardwareAcceleration() const override;
    void SetCaptureSettings(bool useHardwareAcceleration, int maxFPS) override;

private:
    bool CaptureX11Region(int x, int y, int width, int height, CaptureFrame& frame);
    bool ConvertXImageToFrame(XImage* image, CaptureFrame& frame);
    
private:
    bool m_initialized;
    Display* m_display;
    Window m_rootWindow;
    int m_screenWidth;
    int m_screenHeight;
    bool m_useHardwareAcceleration;
    int m_maxFPS;
};

} // namespace work_assistant