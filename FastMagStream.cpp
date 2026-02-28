// FastMagStream.cpp : Real-time screen capture zoom (live view).

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <d3d11.h>
#include <dxgi1_2.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Configurable: source screen size, display window size, zoom factor
constexpr int SOURCE_WIDTH = 2560;
constexpr int SOURCE_HEIGHT = 1440;
constexpr int DISPLAY_WIDTH = 1920;
constexpr int DISPLAY_HEIGHT = 1080;
constexpr float ZOOM_FACTOR = 2.0f;

// ~60 fps
constexpr int DELAY_MS = 16;

std::atomic<bool> g_captureRunning{ true };

constexpr bool DISPLAY_LINE = false;  // set to false to hide center crosshair

void CaptureScreenAndDisplay(HWND windowHandle)
{
    const int captureWidth = static_cast<int>(static_cast<float>(DISPLAY_WIDTH) / ZOOM_FACTOR);
    const int captureHeight = static_cast<int>(static_cast<float>(DISPLAY_HEIGHT) / ZOOM_FACTOR);

    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &pDevice, &featureLevel, &pContext);
    if (FAILED(hr) || !pDevice || !pContext) return;

    IDXGIDevice* pDxgiDevice = nullptr;
    hr = pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&pDxgiDevice));
    if (FAILED(hr) || !pDxgiDevice) { pContext->Release(); pDevice->Release(); return; }

    IDXGIAdapter* pAdapter = nullptr;
    hr = pDxgiDevice->GetAdapter(&pAdapter);
    pDxgiDevice->Release();
    if (FAILED(hr) || !pAdapter) { pContext->Release(); pDevice->Release(); return; }

    IDXGIOutput* pOutput = nullptr;
    hr = pAdapter->EnumOutputs(0, &pOutput);
    pAdapter->Release();
    if (FAILED(hr) || !pOutput) { pContext->Release(); pDevice->Release(); return; }

    IDXGIOutput1* pOutput1 = nullptr;
    hr = pOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&pOutput1));
    if (FAILED(hr) || !pOutput1) { pOutput->Release(); pContext->Release(); pDevice->Release(); return; }

    IDXGIOutputDuplication* pDuplication = nullptr;
    hr = pOutput1->DuplicateOutput(pDevice, &pDuplication);
    pOutput1->Release();
    pOutput->Release();
    if (FAILED(hr) || !pDuplication) { pContext->Release(); pDevice->Release(); return; }

    DXGI_OUTDUPL_DESC dupDesc;
    pDuplication->GetDesc(&dupDesc);
    const int desktopWidth = dupDesc.ModeDesc.Width;
    const int desktopHeight = dupDesc.ModeDesc.Height;
    const int cropX = (desktopWidth - captureWidth) / 2;
    const int cropY = (desktopHeight - captureHeight) / 2;
    const int clipCropX = (cropX < 0) ? 0 : (cropX + captureWidth > desktopWidth ? desktopWidth - captureWidth : cropX);
    const int clipCropY = (cropY < 0) ? 0 : (cropY + captureHeight > desktopHeight ? desktopHeight - captureHeight : cropY);

    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = desktopWidth;
    stagingDesc.Height = desktopHeight;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = dupDesc.ModeDesc.Format;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* pStaging = nullptr;
    hr = pDevice->CreateTexture2D(&stagingDesc, nullptr, &pStaging);
    if (FAILED(hr) || !pStaging) { pDuplication->Release(); pContext->Release(); pDevice->Release(); return; }

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    ReleaseDC(NULL, hScreenDC);
    if (!hMemoryDC) { pStaging->Release(); pDuplication->Release(); pContext->Release(); pDevice->Release(); return; }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = captureWidth;
    bmi.bmiHeader.biHeight = -captureHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* pDibBits = nullptr;
    HBITMAP hDib = CreateDIBSection(hMemoryDC, &bmi, DIB_RGB_COLORS, &pDibBits, nullptr, 0);
    if (!hDib || !pDibBits) { DeleteDC(hMemoryDC); pStaging->Release(); pDuplication->Release(); pContext->Release(); pDevice->Release(); return; }
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hDib);

    const int dibPitch = captureWidth * 4;

    auto copyFrameToDib = [&]() -> int {
        DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
        IDXGIResource* pResource = nullptr;
        hr = pDuplication->AcquireNextFrame(100, &frameInfo, &pResource);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) return 1;
        if (hr == DXGI_ERROR_ACCESS_LOST) return 2;
        if (FAILED(hr) || !pResource) return 1;

        ID3D11Texture2D* pDesktopTexture = nullptr;
        hr = pResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pDesktopTexture));
        pResource->Release();
        if (FAILED(hr) || !pDesktopTexture) { pDuplication->ReleaseFrame(); return 1; }

        pContext->CopyResource(pStaging, pDesktopTexture);
        pDesktopTexture->Release();

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        hr = pContext->Map(pStaging, 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) { pDuplication->ReleaseFrame(); return 1; }

        const char* pSrc = static_cast<const char*>(mapped.pData) + clipCropY * mapped.RowPitch + clipCropX * 4;
        char* pDst = static_cast<char*>(pDibBits);
        for (int y = 0; y < captureHeight; ++y) {
            memcpy(pDst, pSrc, dibPitch);
            pSrc += mapped.RowPitch;
            pDst += dibPitch;
        }
        pContext->Unmap(pStaging, 0);
        pDuplication->ReleaseFrame();
        return 0;
    };

    if constexpr (DISPLAY_LINE)
    {
        while (g_captureRunning)
        {
            int frameResult = copyFrameToDib();
            if (frameResult == 2) break;
            if (frameResult == 0)
            {
                HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
                HPEN hOldPen = (HPEN)SelectObject(hMemoryDC, hPen);
                MoveToEx(hMemoryDC, captureWidth / 2, captureHeight / 2 - 3, NULL);
                LineTo(hMemoryDC, captureWidth / 2, captureHeight / 2 + 3);
                SelectObject(hMemoryDC, hOldPen);
                DeleteObject(hPen);

                HDC hWindowDC = GetDC(windowHandle);
                StretchBlt(hWindowDC, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                    hMemoryDC, 0, 0, captureWidth, captureHeight, SRCCOPY);
                ReleaseDC(windowHandle, hWindowDC);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS));
        }
    }
    else
    {
        while (g_captureRunning)
        {
            int frameResult = copyFrameToDib();
            if (frameResult == 2) break;
            if (frameResult == 0)
            {
                HDC hWindowDC = GetDC(windowHandle);
                StretchBlt(hWindowDC, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                    hMemoryDC, 0, 0, captureWidth, captureHeight, SRCCOPY);
                ReleaseDC(windowHandle, hWindowDC);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(DELAY_MS));
        }
    }

    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hDib);
    DeleteDC(hMemoryDC);
    pStaging->Release();
    pDuplication->Release();
    pContext->Release();
    pDevice->Release();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        g_captureRunning = false;
        PostQuitMessage(0);
        break;
    case WM_NCHITTEST:
        return HTCAPTION;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ScreenCaptureWindowClass";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        L"ScreenCaptureWindowClass",
        L"FastMagStream",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        DISPLAY_WIDTH, DISPLAY_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    std::thread captureThread(CaptureScreenAndDisplay, hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    captureThread.join();
    return 0;
}
