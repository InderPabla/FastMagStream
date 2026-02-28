#pragma once

#include <string>

struct AppConfig
{
    int display_width;
    int display_height;
    int record_width;
    int record_height;
    double zoom_factor;
    double frames_per_second;
    std::string behaviour;  // optional: "crosshairs" or empty
};

std::wstring GetConfigPathFromArgsOrFail();
AppConfig LoadConfigFromTomlOrFail(const std::wstring& path);
void ValidateConfigOrFail(const AppConfig& config);
double ComputeFrameDelayMs(const AppConfig& config);
