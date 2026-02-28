#pragma once

#include "AppConfig.h"
#include "CaptureEngine.h"

#include <Windows.h>
#include <string>

class CaptureWindowHost
{
public:
    CaptureWindowHost(std::wstring window_class_name,
                      std::wstring window_title,
                      std::string error_title,
                      OverlayCallback overlay_callback = nullptr);

    int Run(HINSTANCE hInstance, int nCmdShow);

private:
    std::wstring window_class_name_;
    std::wstring window_title_;
    std::string error_title_;
    OverlayCallback overlay_callback_;
};
