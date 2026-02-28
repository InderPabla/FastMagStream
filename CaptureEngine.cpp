#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "CaptureEngine.h"

#include <Windows.h>
#include <chrono>
#include <cstring>
#include <thread>
#include <d3d11.h>
#include <dxgi1_2.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

int RunCaptureLoop(HWND window, const AppConfig& config, std::atomic<bool>& running, const CaptureRuntimeOptions& options)
{
    const int captureWidth = static_cast<int>(static_cast<double>(config.display_width) / config.zoom_factor);
    const int captureHeight = static_cast<int>(static_cast<double>(config.display_height) / config.zoom_factor);
    const auto frameDelay = std::chrono::duration<double, std::milli>(ComputeFrameDelayMs(config));

    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &pDevice, &featureLevel, &pContext);
    if (FAILED(hr) || !pDevice || !pContext)
        return kCaptureStatusInitFailure;

    IDXGIDevice* pDxgiDevice = nullptr;
    hr = pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&pDxgiDevice));
    if (FAILED(hr) || !pDxgiDevice) { pContext->Release(); pDevice->Release(); return kCaptureStatusInitFailure; }

    IDXGIAdapter* pAdapter = nullptr;
    hr = pDxgiDevice->GetAdapter(&pAdapter);
    pDxgiDevice->Release();
    if (FAILED(hr) || !pAdapter) { pContext->Release(); pDevice->Release(); return kCaptureStatusInitFailure; }

    IDXGIOutput* pOutput = nullptr;
    hr = pAdapter->EnumOutputs(0, &pOutput);
    pAdapter->Release();
    if (FAILED(hr) || !pOutput) { pContext->Release(); pDevice->Release(); return kCaptureStatusInitFailure; }

    IDXGIOutput1* pOutput1 = nullptr;
    hr = pOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&pOutput1));
    if (FAILED(hr) || !pOutput1) { pOutput->Release(); pContext->Release(); pDevice->Release(); return kCaptureStatusInitFailure; }

    IDXGIOutputDuplication* pDuplication = nullptr;
    hr = pOutput1->DuplicateOutput(pDevice, &pDuplication);
    pOutput1->Release();
    pOutput->Release();
    if (FAILED(hr) || !pDuplication) { pContext->Release(); pDevice->Release(); return kCaptureStatusInitFailure; }

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
    if (FAILED(hr) || !pStaging) { pDuplication->Release(); pContext->Release(); pDevice->Release(); return kCaptureStatusInitFailure; }

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    ReleaseDC(NULL, hScreenDC);
    if (!hMemoryDC) { pStaging->Release(); pDuplication->Release(); pContext->Release(); pDevice->Release(); return kCaptureStatusInitFailure; }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = captureWidth;
    bmi.bmiHeader.biHeight = -captureHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* pDibBits = nullptr;
    HBITMAP hDib = CreateDIBSection(hMemoryDC, &bmi, DIB_RGB_COLORS, &pDibBits, nullptr, 0);
    if (!hDib || !pDibBits) {
        DeleteDC(hMemoryDC);
        pStaging->Release();
        pDuplication->Release();
        pContext->Release();
        pDevice->Release();
        return kCaptureStatusInitFailure;
    }
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hDib);

    const int dibPitch = captureWidth * 4;
    int status = kCaptureStatusSuccess;

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

    while (running.load())
    {
        int frameResult = copyFrameToDib();
        if (frameResult == 2)
        {
            status = kCaptureStatusAccessLost;
            break;
        }

        if (frameResult == 0)
        {
            if (options.overlay_callback)
            {
                try
                {
                    options.overlay_callback(CaptureOverlayContext{ hMemoryDC, captureWidth, captureHeight });
                }
                catch (...)
                {
                    status = kCaptureStatusOverlayError;
                    break;
                }
            }

            HDC hWindowDC = GetDC(window);
            StretchBlt(hWindowDC, 0, 0, config.display_width, config.display_height,
                hMemoryDC, 0, 0, captureWidth, captureHeight, SRCCOPY);
            ReleaseDC(window, hWindowDC);
        }

        std::this_thread::sleep_for(frameDelay);
    }

    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hDib);
    DeleteDC(hMemoryDC);
    pStaging->Release();
    pDuplication->Release();
    pContext->Release();
    pDevice->Release();
    return status;
}
