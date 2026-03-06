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

// Sequential panel ramp-up to limit inrush current
void AnimationManager::renderStartup() {
    uint32_t elapsed = millis() - startTime;

    if (elapsed >= STARTUP_RAMP_DURATION_MS) {
        complete = true;
        return;
    }

    float progress = (float)elapsed / (float)STARTUP_RAMP_DURATION_MS;
    uint32_t color = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);

    ledManager.clear();

    // Ramp panels sequentially: each panel fades in during its 1/4 of the duration
    for (uint8_t p = 0; p < PANEL_COUNT; p++) {
        float panelStart = (float)p / (float)PANEL_COUNT;
        float panelEnd   = (float)(p + 1) / (float)PANEL_COUNT;

        if (progress < panelStart) continue;

        float panelProgress = (progress - panelStart) / (panelEnd - panelStart);
        if (panelProgress > 1.0f) panelProgress = 1.0f;

        ledManager.setMaskedColorScaled(static_cast<PanelId>(p), color, panelProgress);
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

    // Phase 1: Flash (first 200ms) — all logo pixels full white
    if (elapsed < 200) {
        uint32_t white = Adafruit_NeoPixel::Color(255, 255, 255);
        for (uint8_t p = 0; p < PANEL_COUNT; p++) {
            ledManager.setMaskedColor(static_cast<PanelId>(p), white);
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
