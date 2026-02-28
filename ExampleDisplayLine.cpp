// ExampleDisplayLine.cpp : Example.DisplayLine executable entrypoint.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "AppConfig.h"
#include "CaptureEngine.h"

#include <Windows.h>
#include <atomic>
#include <exception>
#include <string>
#include <thread>

namespace
{
std::atomic<bool> g_captureRunning{ true };
std::atomic<int> g_captureStatus{ kCaptureStatusSuccess };

void ShowError(const std::string& message, const char* title = "FastMagStream Example.DisplayLine")
{
    MessageBoxA(nullptr, message.c_str(), title, MB_OK | MB_ICONERROR);
}

void DrawCenterLine(const CaptureOverlayContext& context)
{
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(context.memory_dc, hPen);
    MoveToEx(context.memory_dc, context.capture_width / 2, context.capture_height / 2 - 3, NULL);
    LineTo(context.memory_dc, context.capture_width / 2, context.capture_height / 2 + 3);
    SelectObject(context.memory_dc, hOldPen);
    DeleteObject(hPen);
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    AppConfig config{};
    try
    {
        const std::wstring configPath = GetConfigPathFromArgsOrFail();
        config = LoadConfigFromTomlOrFail(configPath);
        ValidateConfigOrFail(config);
    }
    catch (const std::exception& ex)
    {
        ShowError(ex.what(), "FastMagStream Startup Error");
        return 1;
    }

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"FastMagStreamDisplayLineWindowClass";
    if (!RegisterClassW(&wc))
    {
        ShowError("Failed to register window class.", "FastMagStream Startup Error");
        return 1;
    }

    HWND hwnd = CreateWindowW(
        wc.lpszClassName,
        L"FastMagStream - Display Line Example",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        config.display_width, config.display_height,
        NULL, NULL, hInstance, NULL);

    if (!hwnd)
    {
        ShowError("Failed to create window.", "FastMagStream Startup Error");
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    CaptureRuntimeOptions options{};
    options.overlay_callback = DrawCenterLine;

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
        ShowError(CaptureStatusMessage(finalStatus), "FastMagStream Capture Error");
        return 1;
    }

    return 0;
}
