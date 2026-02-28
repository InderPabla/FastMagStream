// FastMagStream.cpp : Example.Basic executable entrypoint.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "CaptureWindowHost.h"

#include <Windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    CaptureWindowHost host(
        L"FastMagStreamBasicWindowClass",
        L"FastMagStream - Basic",
        "FastMagStream",
        nullptr);
    return host.Run(hInstance, nCmdShow);
}
