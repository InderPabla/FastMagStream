#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "CaptureWindowHost.h"
#include "AppConfig.h"
#include "CaptureEngine.h"
#include "OverlayCallbacks.h"

#include <Windows.h>
#include <atomic>
#include <exception>
#include <string>
#include <thread>

namespace
{
std::atomic<bool> g_captureRunning{ true };
std::atomic<int> g_captureStatus{ kCaptureStatusSuccess };

void ShowError(const std::string& message, const char* title)
{
    MessageBoxA(nullptr, message.c_str(), title, MB_OK | MB_ICONERROR);
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

const char* CaptureStatusMessage(int status)
{
    switch (status)
    {
    case kCaptureStatusSuccess: return "";
    case kCaptureStatusInitFailure: return "Capture initialization failed.";
    case kCaptureStatusAccessLost: return "Capture access was lost.";
    case kCaptureStatusOverlayError: return "Overlay callback failed.";
    default: return "Unknown capture failure.";
    }
}
} // namespace

CaptureWindowHost::CaptureWindowHost(std::wstring window_class_name,
                                     std::wstring window_title,
                                     std::string error_title)
    : window_class_name_(std::move(window_class_name))
    , window_title_(std::move(window_title))
    , error_title_(std::move(error_title))
{
}

int CaptureWindowHost::Run(HINSTANCE hInstance, int nCmdShow)
{
    g_captureRunning = true;
    g_captureStatus = kCaptureStatusSuccess;

    AppConfig config{};
    try
    {
        const std::wstring configPath = GetConfigPathFromArgsOrFail();
        config = LoadConfigFromTomlOrFail(configPath);
        ValidateConfigOrFail(config);
    }
    catch (const std::exception& ex)
    {
        ShowError(ex.what(), (error_title_ + " Startup Error").c_str());
        return 1;
    }

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = window_class_name_.c_str();
    if (!RegisterClassW(&wc))
    {
        ShowError("Failed to register window class.", (error_title_ + " Startup Error").c_str());
        return 1;
    }

    HWND hwnd = CreateWindowW(
        wc.lpszClassName,
        window_title_.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        config.display_width, config.display_height,
        NULL, NULL, hInstance, NULL);

    if (!hwnd)
    {
        ShowError("Failed to create window.", (error_title_ + " Startup Error").c_str());
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    CaptureRuntimeOptions options{};
    options.overlay_callback = GetOverlayForBehaviour(config.behaviour);

    std::thread captureThread([&]() {
        const int status = RunCaptureLoop(hwnd, config, g_captureRunning, options);
        g_captureStatus = status;
        if (status != kCaptureStatusSuccess)
        {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
    });

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    captureThread.join();

    const int finalStatus = g_captureStatus.load();
    if (finalStatus != kCaptureStatusSuccess)
    {
        ShowError(CaptureStatusMessage(finalStatus), (error_title_ + " Capture Error").c_str());
        return 1;
    }

    return 0;
}
