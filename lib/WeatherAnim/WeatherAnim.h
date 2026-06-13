#pragma once

#include "Adafruit_GFX.h"

// Draw weather animations into a 40×40 GFXcanvas1.
// Call draw() on every display refresh, passing millis() as phaseMs.
// The phase drives all internal animation timers — no state to maintain.
class WeatherAnim {
public:
    static const int W = 40;
    static const int H = 40;

    // Render one animation frame for the given WMO weather code.
    // isDay affects clear/partly-cloudy (sun vs moon).
    void draw(GFXcanvas1& canvas, int weatherCode, bool isDay, unsigned long phaseMs) const;
};
