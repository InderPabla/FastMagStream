#pragma once

#include "CaptureEngine.h"

#include <string>

// Overlay callback that draws centre crosshairs (vertical + horizontal line through center).
void WithCenterCrosshairs(const CaptureOverlayContext& context);

// Returns the overlay callback for the given behaviour string, or nullptr if empty/unknown.
OverlayCallback GetOverlayForBehaviour(const std::string& behaviour);
