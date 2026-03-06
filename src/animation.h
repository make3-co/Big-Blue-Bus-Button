#pragma once

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Animation — Non-blocking animation system
// =============================================================================

enum class AnimationType : uint8_t {
    NONE,
    IDLE_PULSE,       // Sinusoidal white breathing on masked pixels
    BUTTON_PRESS,     // Green/rainbow fill sweep
};

class AnimationManager {
public:
    void begin();
    void update();    // Call every loop iteration — renders current frame

    void startIdlePulse();
    void startButtonPress();
    void stop();

    AnimationType currentAnimation() const { return current; }
    bool isComplete() const { return complete; }

private:
    AnimationType current = AnimationType::NONE;
    uint32_t startTime = 0;
    bool complete = false;

    void renderIdlePulse();
    void renderButtonPress();
};

extern AnimationManager animationManager;
