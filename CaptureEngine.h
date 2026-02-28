#pragma once

#include "AppConfig.h"

#include <Windows.h>
#include <atomic>
#include <functional>

struct CaptureOverlayContext
{
    HDC memory_dc;
    int capture_width;
    int capture_height;
};

using OverlayCallback = std::function<void(const CaptureOverlayContext&)>;

struct CaptureRuntimeOptions
{
    OverlayCallback overlay_callback;
};

enum CaptureRunStatus
{
    kCaptureStatusSuccess = 0,
    kCaptureStatusInitFailure = 1,
    kCaptureStatusAccessLost = 2,
    kCaptureStatusOverlayError = 3
};

// Runs on the capture worker thread and invokes overlay_callback (if provided)
// before presenting each successful frame.
int RunCaptureLoop(HWND window, const AppConfig& config, std::atomic<bool>& running, const CaptureRuntimeOptions& options);
