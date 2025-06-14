#include "win32_screen_capture.h"
#include <iostream>
#include <chrono>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "gdi32.lib")

namespace work_assistant {

Win32ScreenCapture::Win32ScreenCapture()
    : m_initialized(false)
    , m_supportsDXGI(false)
    , m_screenWidth(0)
    , m_screenHeight(0)
    , m_device(nullptr)
    , m_context(nullptr)
    , m_duplication(nullptr)
    , m_stagingTexture(nullptr)
    , m_screenDC(nullptr)
    , m_memoryDC(nullptr)
    , m_bitmap(nullptr)
{
    ZeroMemory(&m_bitmapInfo, sizeof(m_bitmapInfo));
}

Win32ScreenCapture::~Win32ScreenCapture() {
    Shutdown();
}

bool Win32ScreenCapture::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Get screen dimensions
    m_screenWidth = GetSystemMetrics(SM_CXSCREEN);
    m_screenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (m_screenWidth <= 0 || m_screenHeight <= 0) {
        std::cerr << "Failed to get screen dimensions" << std::endl;
        return false;
    }

    // Try to initialize DXGI first (Windows 8+)
    if (InitializeDXGI()) {
        m_supportsDXGI = true;
        std::cout << "Using DXGI Desktop Duplication for screen capture" << std::endl;
    } else {
        // Fall back to GDI
        m_screenDC = GetDC(nullptr);
        if (!m_screenDC) {
            std::cerr << "Failed to get screen DC" << std::endl;
            return false;
        }

        m_memoryDC = CreateCompatibleDC(m_screenDC);
        if (!m_memoryDC) {
            std::cerr << "Failed to create memory DC" << std::endl;
            return false;
        }

        // Create bitmap info structure
        m_bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        m_bitmapInfo.bmiHeader.biWidth = m_screenWidth;
        m_bitmapInfo.bmiHeader.biHeight = -m_screenHeight; // Negative for top-down
        m_bitmapInfo.bmiHeader.biPlanes = 1;
        m_bitmapInfo.bmiHeader.biBitCount = 32;
        m_bitmapInfo.bmiHeader.biCompression = BI_RGB;

        std::cout << "Using GDI for screen capture" << std::endl;
    }

    m_initialized = true;
    return true;
}

void Win32ScreenCapture::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_supportsDXGI) {
        ShutdownDXGI();
    } else {
        if (m_bitmap) {
            DeleteObject(m_bitmap);
            m_bitmap = nullptr;
        }
        if (m_memoryDC) {
            DeleteDC(m_memoryDC);
            m_memoryDC = nullptr;
        }
        if (m_screenDC) {
            ReleaseDC(nullptr, m_screenDC);
            m_screenDC = nullptr;
        }
    }

    m_initialized = false;
}

bool Win32ScreenCapture::CaptureDesktop(CaptureFrame& frame) {
    if (!m_initialized) {
        return false;
    }

    if (m_supportsDXGI) {
        return CaptureDXGI(frame);
    } else {
        return CaptureGDI(0, 0, m_screenWidth, m_screenHeight, frame);
    }
}

bool Win32ScreenCapture::CaptureWindow(HWND hwnd, CaptureFrame& frame) {
    if (!m_initialized || !hwnd) {
        return false;
    }

    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        return false;
    }

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    if (width <= 0 || height <= 0) {
        return false;
    }

    return CaptureRegion(rect.left, rect.top, width, height, frame);
}

bool Win32ScreenCapture::CaptureRegion(int x, int y, int width, int height, CaptureFrame& frame) {
    if (!m_initialized) {
        return false;
    }

    // For DXGI, we need to capture the entire desktop and crop
    if (m_supportsDXGI) {
        CaptureFrame fullFrame;
        if (!CaptureDXGI(fullFrame)) {
            return false;
        }

        // Crop the frame
        frame.width = width;
        frame.height = height;
        frame.bytes_per_pixel = 4;
        frame.timestamp = fullFrame.timestamp;
        frame.data.resize(width * height * 4);

        for (int row = 0; row < height; ++row) {
            int srcRow = y + row;
            if (srcRow >= 0 && srcRow < m_screenHeight) {
                int srcOffset = (srcRow * m_screenWidth + x) * 4;
                int dstOffset = row * width * 4;
                int copyWidth = std::min(width, m_screenWidth - x);
                if (copyWidth > 0) {
                    std::memcpy(frame.data.data() + dstOffset,
                               fullFrame.data.data() + srcOffset,
                               copyWidth * 4);
                }
            }
        }
        return true;
    } else {
        return CaptureGDI(x, y, width, height, frame);
    }
}

bool Win32ScreenCapture::InitializeDXGI() {
    HRESULT hr;

    // Create D3D11 device
    D3D_FEATURE_LEVEL featureLevel;
    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &m_device,
        &featureLevel,
        &m_context
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 device" << std::endl;
        return false;
    }

    // Get DXGI device
    IDXGIDevice* dxgiDevice = nullptr;
    hr = m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI device" << std::endl;
        return false;
    }

    // Get DXGI adapter
    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    dxgiDevice->Release();
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI adapter" << std::endl;
        return false;
    }

    // Get output (monitor)
    IDXGIOutput* dxgiOutput = nullptr;
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    dxgiAdapter->Release();
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI output" << std::endl;
        return false;
    }

    // Get output1 for desktop duplication
    IDXGIOutput1* dxgiOutput1 = nullptr;
    hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&dxgiOutput1);
    dxgiOutput->Release();
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI output1" << std::endl;
        return false;
    }

    // Create desktop duplication
    hr = dxgiOutput1->DuplicateOutput(m_device, &m_duplication);
    dxgiOutput1->Release();
    if (FAILED(hr)) {
        std::cerr << "Failed to create desktop duplication" << std::endl;
        return false;
    }

    // Create staging texture
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = m_screenWidth;
    desc.Height = m_screenHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = m_device->CreateTexture2D(&desc, nullptr, &m_stagingTexture);
    if (FAILED(hr)) {
        std::cerr << "Failed to create staging texture" << std::endl;
        return false;
    }

    return true;
}

bool Win32ScreenCapture::CaptureDXGI(CaptureFrame& frame) {
    if (!m_duplication || !m_stagingTexture) {
        return false;
    }

    HRESULT hr;
    IDXGIResource* dxgiResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    // Acquire next frame
    hr = m_duplication->AcquireNextFrame(500, &frameInfo, &dxgiResource);
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // No new frame available
            return false;
        }
        std::cerr << "Failed to acquire frame: " << std::hex << hr << std::endl;
        return false;
    }

    // Get texture from resource
    ID3D11Texture2D* texture = nullptr;
    hr = dxgiResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture);
    dxgiResource->Release();
    if (FAILED(hr)) {
        std::cerr << "Failed to get texture from resource" << std::endl;
        m_duplication->ReleaseFrame();
        return false;
    }

    // Copy to staging texture
    m_context->CopyResource(m_stagingTexture, texture);
    texture->Release();

    // Map staging texture
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = m_context->Map(m_stagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        std::cerr << "Failed to map staging texture" << std::endl;
        m_duplication->ReleaseFrame();
        return false;
    }

    // Copy data
    frame.width = m_screenWidth;
    frame.height = m_screenHeight;
    frame.bytes_per_pixel = 4;
    frame.timestamp = std::chrono::system_clock::now();
    frame.data.resize(m_screenWidth * m_screenHeight * 4);

    uint8_t* src = static_cast<uint8_t*>(mappedResource.pData);
    uint8_t* dst = frame.data.data();

    for (int y = 0; y < m_screenHeight; ++y) {
        std::memcpy(dst + y * m_screenWidth * 4,
                   src + y * mappedResource.RowPitch,
                   m_screenWidth * 4);
    }

    // Convert BGRA to RGBA
    ConvertBGRAToRGBA(frame.data);

    // Unmap and release
    m_context->Unmap(m_stagingTexture, 0);
    m_duplication->ReleaseFrame();

    return true;
}

void Win32ScreenCapture::ShutdownDXGI() {
    if (m_stagingTexture) {
        m_stagingTexture->Release();
        m_stagingTexture = nullptr;
    }
    if (m_duplication) {
        m_duplication->Release();
        m_duplication = nullptr;
    }
    if (m_context) {
        m_context->Release();
        m_context = nullptr;
    }
    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }
}

bool Win32ScreenCapture::CaptureGDI(int x, int y, int width, int height, CaptureFrame& frame) {
    // Create bitmap if needed or if size changed
    if (!m_bitmap || frame.width != width || frame.height != height) {
        if (m_bitmap) {
            DeleteObject(m_bitmap);
        }

        m_bitmap = CreateCompatibleBitmap(m_screenDC, width, height);
        if (!m_bitmap) {
            std::cerr << "Failed to create bitmap" << std::endl;
            return false;
        }
    }

    // Select bitmap into memory DC
    HGDIOBJ oldBitmap = SelectObject(m_memoryDC, m_bitmap);

    // Capture screen
    if (!BitBlt(m_memoryDC, 0, 0, width, height, m_screenDC, x, y, SRCCOPY)) {
        std::cerr << "Failed to capture screen with BitBlt" << std::endl;
        SelectObject(m_memoryDC, oldBitmap);
        return false;
    }

    // Get bitmap data
    frame.width = width;
    frame.height = height;
    frame.bytes_per_pixel = 4;
    frame.timestamp = std::chrono::system_clock::now();
    frame.data.resize(width * height * 4);

    BITMAPINFO bmpInfo = m_bitmapInfo;
    bmpInfo.bmiHeader.biWidth = width;
    bmpInfo.bmiHeader.biHeight = -height;

    int result = GetDIBits(m_memoryDC, m_bitmap, 0, height,
                          frame.data.data(), &bmpInfo, DIB_RGB_COLORS);

    SelectObject(m_memoryDC, oldBitmap);

    if (result == 0) {
        std::cerr << "Failed to get bitmap bits" << std::endl;
        return false;
    }

    // Convert BGRA to RGBA
    ConvertBGRAToRGBA(frame.data);

    return true;
}

bool Win32ScreenCapture::ConvertBGRAToRGBA(std::vector<uint8_t>& data) {
    if (data.size() % 4 != 0) {
        return false;
    }

    for (size_t i = 0; i < data.size(); i += 4) {
        // Swap B and R channels
        std::swap(data[i], data[i + 2]);
    }

    return true;
}

} // namespace work_assistant