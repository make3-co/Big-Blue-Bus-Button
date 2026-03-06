#pragma once

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Button Handler — Debounced input with edge detection
// =============================================================================

class ButtonHandler {
public:
    void begin();
    void update();       // Call every loop iteration
    bool wasPressed();   // Returns true once per press (consumed on read)

private:
    bool lastReading    = HIGH;
    bool stableState    = HIGH;
    bool pressed        = false;
    uint32_t lastChange = 0;
};

extern ButtonHandler button;
