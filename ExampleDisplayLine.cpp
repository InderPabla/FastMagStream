// ExampleDisplayLine.cpp : Example.DisplayLine executable entrypoint.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "CaptureEngine.h"
#include "CaptureWindowHost.h"

#include <Windows.h>

namespace
{
void DrawCenterLine(const CaptureOverlayContext& context)
{
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(context.memory_dc, hPen);
    MoveToEx(context.memory_dc, context.capture_width / 2, context.capture_height / 2 - 3, NULL);
    LineTo(context.memory_dc, context.capture_width / 2, context.capture_height / 2 + 3);
    SelectObject(context.memory_dc, hOldPen);
    DeleteObject(hPen);
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    CaptureWindowHost host(
        L"FastMagStreamDisplayLineWindowClass",
        L"FastMagStream - Display Line Example",
        "FastMagStream Example.DisplayLine",
        DrawCenterLine);
    return host.Run(hInstance, nCmdShow);
}
