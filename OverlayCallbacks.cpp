#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "OverlayCallbacks.h"
#include "CaptureEngine.h"

#include <Windows.h>

void WithCenterCrosshairs(const CaptureOverlayContext& context)
{
    const int cx = context.capture_width / 2;
    const int cy = context.capture_height / 2;
    const int halfLen = 3;

    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(context.memory_dc, hPen);

    MoveToEx(context.memory_dc, cx, cy - halfLen, NULL);
    LineTo(context.memory_dc, cx, cy + halfLen);
    MoveToEx(context.memory_dc, cx - halfLen, cy, NULL);
    LineTo(context.memory_dc, cx + halfLen, cy);

    SelectObject(context.memory_dc, hOldPen);
    DeleteObject(hPen);
}

OverlayCallback GetOverlayForBehaviour(const std::string& behaviour)
{
    if (behaviour == "crosshairs")
        return WithCenterCrosshairs;
    return nullptr;
}
