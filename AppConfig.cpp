#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "AppConfig.h"

#include <Windows.h>
#include <shellapi.h>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include "third_party/tomlplusplus/toml.hpp"

#pragma comment(lib, "shell32.lib")

namespace
{
int GetRequiredInt(const toml::table& table, const char* key)
{
    auto node = table[key];
    auto value = node.value<std::int64_t>();
    if (!value)
        throw std::runtime_error(std::string("Missing or invalid integer key: ") + key);

    if (*value < static_cast<std::int64_t>((std::numeric_limits<int>::min)()) ||
        *value > static_cast<std::int64_t>((std::numeric_limits<int>::max)()))
    {
        throw std::runtime_error(std::string("Integer out of range for key: ") + key);
    }

    return static_cast<int>(*value);
}

double GetRequiredNumber(const toml::table& table, const char* key)
{
    auto node = table[key];

    if (auto value = node.value<double>())
        return *value;

    if (auto value = node.value<std::int64_t>())
        return static_cast<double>(*value);

    throw std::runtime_error(std::string("Missing or invalid numeric key: ") + key);
}
}  // namespace

std::wstring GetConfigPathFromArgsOrFail()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        throw std::runtime_error("Failed to parse command line arguments.");

    std::wstring configPath;
    for (int i = 1; i < argc; ++i)
    {
        if (_wcsicmp(argv[i], L"--config") == 0)
        {
            if (i + 1 >= argc)
            {
                LocalFree(argv);
                throw std::runtime_error("Missing value for --config.");
            }
            configPath = argv[i + 1];
            break;
        }
    }

    LocalFree(argv);

    if (configPath.empty())
        throw std::runtime_error("Missing required argument: --config <path-to-fastmagstream.toml>");

    return configPath;
}

AppConfig LoadConfigFromTomlOrFail(const std::wstring& path)
{
    std::ifstream input(std::filesystem::path(path), std::ios::binary);
    if (!input.is_open())
        throw std::runtime_error("Unable to open config file.");

    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    toml::table table;
    try
    {
        table = toml::parse(content);
    }
    catch (const toml::parse_error& err)
    {
        throw std::runtime_error(std::string("TOML parse error: ") + err.what());
    }

    AppConfig config{};
    config.display_width = GetRequiredInt(table, "display_width");
    config.display_height = GetRequiredInt(table, "display_height");
    config.record_width = GetRequiredInt(table, "record_width");
    config.record_height = GetRequiredInt(table, "record_height");
    config.zoom_factor = GetRequiredNumber(table, "zoom_factor");
    config.frames_per_second = GetRequiredNumber(table, "frames_per_second");

    if (auto behaviour = table["behaviour"].value<std::string>())
        config.behaviour = *behaviour;

    return config;
}

void ValidateConfigOrFail(const AppConfig& config)
{
    if (config.display_width <= 0)
        throw std::runtime_error("display_width must be > 0.");
    if (config.display_height <= 0)
        throw std::runtime_error("display_height must be > 0.");
    if (config.record_width <= 0)
        throw std::runtime_error("record_width must be > 0.");
    if (config.record_height <= 0)
        throw std::runtime_error("record_height must be > 0.");
    if (!std::isfinite(config.zoom_factor) || config.zoom_factor <= 0.0)
        throw std::runtime_error("zoom_factor must be a finite number > 0.");
    if (!std::isfinite(config.frames_per_second) || config.frames_per_second <= 0.0)
        throw std::runtime_error("frames_per_second must be a finite number > 0.");
    if (!config.behaviour.empty() && config.behaviour != "crosshairs")
        throw std::runtime_error("behaviour must be \"crosshairs\" or omitted.");

    const int captureWidth = static_cast<int>(static_cast<double>(config.display_width) / config.zoom_factor);
    const int captureHeight = static_cast<int>(static_cast<double>(config.display_height) / config.zoom_factor);

    if (captureWidth < 1 || captureHeight < 1)
        throw std::runtime_error("Computed capture dimensions must be at least 1x1. Adjust display size or zoom_factor.");
}

double ComputeFrameDelayMs(const AppConfig& config)
{
    return 1000.0 / config.frames_per_second;
}
