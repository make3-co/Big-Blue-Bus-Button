#pragma once

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Animation — Non-blocking animation system with multiple button press styles
// =============================================================================

enum class AnimationType : uint8_t {
    NONE,
    STARTUP,          // Sequential panel ramp-up to limit inrush current
    IDLE_GLOW,        // Static warm white on masked pixels
    BUTTON_PRESS,     // Cycles through press animation styles
};

// Button press animation styles
enum class PressStyle : uint8_t {
    FILL_TOP_DOWN,      // Top-to-bottom fill
    FILL_LEFT_RIGHT,    // Left-to-right sweep
    RAINBOW_SWEEP,      // Rainbow hue sweeps across
    PRESS_STYLE_COUNT   // Must be last
};

class AnimationManager {
public:
    void begin();
    void update();    // Call every loop iteration — renders current frame

    void startStartup();
    void startIdleGlow();
    void startButtonPress();
    void stop();

    AnimationType currentAnimation() const { return current; }
    bool isComplete() const { return complete; }
    PressStyle currentPressStyle() const { return pressStyle; }

private:
    AnimationType current = AnimationType::NONE;
    PressStyle pressStyle = PressStyle::FILL_TOP_DOWN;
    uint32_t startTime = 0;
    bool complete = false;
    bool inIdentifyPhase = false;
    uint32_t identifyStartTime = 0;
    static constexpr uint32_t IDENTIFY_DURATION_MS = 2000;

    void renderStartup();
    void renderIdleGlow();
    void renderButtonPress();

    // Individual press animation renderers
    void renderFillTopDown(float progress);
    void renderFillLeftRight(float progress);
    void renderRainbowSweep(float progress);
};

extern AnimationManager animationManager;
