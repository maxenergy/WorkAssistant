#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <memory>
#include <vector>
#include <string>

namespace work_assistant {

struct CaptureFrame {
    std::vector<uint8_t> data;
    int width;
    int height;
    int bytes_per_pixel;
    std::chrono::system_clock::time_point timestamp;
};

class Win32ScreenCapture {
public:
    Win32ScreenCapture();
    ~Win32ScreenCapture();

    bool Initialize();
    void Shutdown();

    // Capture entire desktop
    bool CaptureDesktop(CaptureFrame& frame);
    
    // Capture specific window
    bool CaptureWindow(HWND hwnd, CaptureFrame& frame);
    
    // Capture region of screen
    bool CaptureRegion(int x, int y, int width, int height, CaptureFrame& frame);

    // Get screen dimensions
    int GetScreenWidth() const { return m_screenWidth; }
    int GetScreenHeight() const { return m_screenHeight; }

    // Check if DXGI Desktop Duplication is available (Windows 8+)
    bool SupportsDXGI() const { return m_supportsDXGI; }

private:
    // DXGI Desktop Duplication methods (Windows 8+)
    bool InitializeDXGI();
    bool CaptureDXGI(CaptureFrame& frame);
    void ShutdownDXGI();

    // GDI methods (fallback for older Windows)
    bool CaptureGDI(int x, int y, int width, int height, CaptureFrame& frame);

    // Helper methods
    bool ConvertBGRAToRGBA(std::vector<uint8_t>& data);

private:
    bool m_initialized;
    bool m_supportsDXGI;
    int m_screenWidth;
    int m_screenHeight;

    // DXGI Desktop Duplication resources
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    IDXGIOutputDuplication* m_duplication;
    ID3D11Texture2D* m_stagingTexture;

    // GDI resources
    HDC m_screenDC;
    HDC m_memoryDC;
    HBITMAP m_bitmap;
    BITMAPINFO m_bitmapInfo;
};

} // namespace work_assistant