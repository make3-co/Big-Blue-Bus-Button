#include "animation.h"
#include "led_manager.h"

AnimationManager animationManager;

void AnimationManager::begin() {
    Serial.println("Animation manager initialized");
}

void AnimationManager::update() {
    switch (current) {
        case AnimationType::STARTUP:
            renderStartup();
            break;
        case AnimationType::IDLE_GLOW:
            renderIdleGlow();
            break;
        case AnimationType::BUTTON_PRESS:
            renderButtonPress();
            break;
        case AnimationType::NONE:
        default:
            break;
    }
}

void AnimationManager::startStartup() {
    current = AnimationType::STARTUP;
    startTime = millis();
    complete = false;
}

void AnimationManager::startIdleGlow() {
    current = AnimationType::IDLE_GLOW;
    startTime = millis();
    complete = false;
}

void AnimationManager::startButtonPress() {
    current = AnimationType::BUTTON_PRESS;
    startTime = millis();
    complete = false;
}

void AnimationManager::stop() {
    current = AnimationType::NONE;
    complete = false;
}

// Startup: staggered panel wake-up to limit inrush current.
// Each panel gets its own time slot to fade in, so the boost converter
// never has to supply current for more than one panel ramping simultaneously.
// Order: side-left → side-right → front-left → front-right
// Global brightness also ramps from 0 to BRIGHTNESS_IDLE_MAX during startup.

static constexpr uint8_t STARTUP_PANEL_ORDER[PANEL_COUNT] = {
    PANEL_SIDE_LEFT, PANEL_SIDE_RIGHT, PANEL_FRONT_LEFT, PANEL_FRONT_RIGHT
};

void AnimationManager::renderStartup() {
    uint32_t elapsed = millis() - startTime;

    if (elapsed >= STARTUP_RAMP_DURATION_MS) {
        ledManager.setBrightness(BRIGHTNESS_IDLE_MAX);
        complete = true;
        return;
    }

    float progress = (float)elapsed / (float)STARTUP_RAMP_DURATION_MS;

    // Ramp global brightness from 0 to target over the full startup duration
    uint8_t globalBright = (uint8_t)(progress * BRIGHTNESS_IDLE_MAX);
    ledManager.setBrightness(globalBright);

    uint32_t color = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);
    float slotDuration = 1.0f / (float)PANEL_COUNT;

    ledManager.clear();

    for (uint8_t i = 0; i < PANEL_COUNT; i++) {
        PanelId panel = static_cast<PanelId>(STARTUP_PANEL_ORDER[i]);
        float slotStart = (float)i * slotDuration;
        float slotEnd   = slotStart + slotDuration;

        if (progress < slotStart) continue;  // This panel hasn't started yet

        float panelBright = (progress - slotStart) / (slotEnd - slotStart);
        if (panelBright > 1.0f) panelBright = 1.0f;

        ledManager.setMaskedColorScaled(panel, color, panelBright);
    }

    ledManager.show();
}

// Static warm white glow on masked pixels
void AnimationManager::renderIdleGlow() {
    uint32_t color = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);
    ledManager.clear();
    for (uint8_t p = 0; p < PANEL_COUNT; p++) {
        ledManager.setMaskedColor(static_cast<PanelId>(p), color);
    }
    ledManager.show();
}

// Flash then bottom-to-top fill on masked pixels
void AnimationManager::renderButtonPress() {
    uint32_t elapsed = millis() - startTime;

    if (elapsed >= ANIMATION_DURATION_MS) {
        complete = true;
        return;
    }

    uint32_t color = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);
    ledManager.clear();

    // Phase 1: Flash (first 200ms) — all logo pixels bright warm white
    if (elapsed < 200) {
        uint32_t flash = Adafruit_NeoPixel::Color(255, 220, 140);
        for (uint8_t p = 0; p < PANEL_COUNT; p++) {
            ledManager.setMaskedColor(static_cast<PanelId>(p), flash);
        }
        ledManager.show();
        return;
    }

    // Phase 2: Bottom-to-top fill (200ms to end)
    float fillProgress = (float)(elapsed - 200) / (float)(ANIMATION_DURATION_MS - 200);

    for (uint8_t p = 0; p < PANEL_COUNT; p++) {
        PanelId panel = static_cast<PanelId>(p);
        uint8_t w = ledManager.getPanelWidth(panel);
        uint8_t h = ledManager.getPanelHeight(panel);
        uint8_t fillRow = (uint8_t)(fillProgress * h);

        for (uint8_t x = 0; x < w; x++) {
            for (uint8_t y = 0; y < h; y++) {
                if (!ledManager.isMasked(panel, x, y)) continue;
                // y=0 is top, y=h-1 is bottom. Fill from bottom up.
                if (y >= (h - 1 - fillRow)) {
                    ledManager.setPixelXY(panel, x, y, color);
                }
            }
        }
    }

    ledManager.show();
}
