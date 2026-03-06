#include "animation.h"
#include "led_manager.h"
#include <math.h>

AnimationManager animationManager;

void AnimationManager::begin() {
    Serial.println("Animation manager initialized");
}

void AnimationManager::update() {
    switch (current) {
        case AnimationType::IDLE_PULSE:
            renderIdlePulse();
            break;
        case AnimationType::BUTTON_PRESS:
            renderButtonPress();
            break;
        case AnimationType::NONE:
        default:
            break;
    }
}

void AnimationManager::startIdlePulse() {
    current = AnimationType::IDLE_PULSE;
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

// Sinusoidal white breathing on masked pixels
void AnimationManager::renderIdlePulse() {
    uint32_t elapsed = millis() - startTime;
    float phase = (float)(elapsed % IDLE_PULSE_PERIOD_MS) / (float)IDLE_PULSE_PERIOD_MS;
    float sineVal = (sinf(phase * 2.0f * M_PI) + 1.0f) / 2.0f;  // 0.0 to 1.0

    // Map sine to brightness range
    float brightness = (float)BRIGHTNESS_IDLE_MIN / 255.0f +
                       sineVal * ((float)(BRIGHTNESS_IDLE_MAX - BRIGHTNESS_IDLE_MIN) / 255.0f);

    uint32_t white = Adafruit_NeoPixel::Color(255, 255, 255);

    ledManager.clear();
    for (uint8_t p = 0; p < PANEL_COUNT; p++) {
        ledManager.setMaskedColorScaled(static_cast<PanelId>(p), white, brightness);
    }
    ledManager.show();
}

// Green/rainbow fill sweep across all panels
void AnimationManager::renderButtonPress() {
    uint32_t elapsed = millis() - startTime;

    if (elapsed >= ANIMATION_DURATION_MS) {
        complete = true;
        return;
    }

    float progress = (float)elapsed / (float)ANIMATION_DURATION_MS;  // 0.0 to 1.0

    ledManager.clear();

    for (uint8_t p = 0; p < PANEL_COUNT; p++) {
        PanelId panel = static_cast<PanelId>(p);
        uint8_t w = ledManager.getPanelWidth(panel);
        uint8_t h = ledManager.getPanelHeight(panel);

        // Sweep columns left-to-right based on progress
        uint8_t sweepCol = (uint8_t)(progress * w);

        for (uint8_t x = 0; x < w; x++) {
            for (uint8_t y = 0; y < h; y++) {
                if (!ledManager.isMasked(panel, x, y)) continue;

                if (x <= sweepCol) {
                    // Swept area: rainbow hue based on position + time
                    uint16_t hue = (uint16_t)((float)x / (float)w * 65535.0f + elapsed * 20) % 65536;
                    uint32_t color = Adafruit_NeoPixel::ColorHSV(hue, 255, 255);
                    ledManager.setPixelXY(panel, x, y, color);
                } else {
                    // Ahead of sweep: green glow
                    uint32_t green = Adafruit_NeoPixel::Color(0, 80, 0);
                    ledManager.setPixelXY(panel, x, y, green);
                }
            }
        }
    }
    ledManager.show();
}
