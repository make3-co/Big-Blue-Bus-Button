#include "animation.h"
#include "led_manager.h"
#include "audio_manager.h"

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

// Startup: front panels fade by logo section, side panels fade together
// Sections: Orange (rows 2-5), Blue (rows 6-9), Green (rows 10-13), Yellow (right cols 1-7 rows 2-13)
// Side panels fade in during the first section slot

static constexpr uint8_t NUM_SECTIONS = 4;

// Check which section a front panel pixel belongs to (0-3), or -1 if none
static int8_t getFrontSection(PanelId panel, uint8_t x, uint8_t y) {
    if (panel == PANEL_FRONT_LEFT) {
        if (y >= 2 && y <= 5 && x >= 1)  return 0;  // Orange
        if (y >= 6 && y <= 9 && x >= 1)  return 1;  // Blue
        if (y >= 10 && y <= 13 && x >= 1) return 2;  // Green
    } else if (panel == PANEL_FRONT_RIGHT) {
        // Yellow: cols 1-7, rows 2-13 (section 3)
        if (y >= 2 && y <= 13 && x >= 1 && x <= 7) return 3;
        // Orange/Blue/Green: col 0 only
        if (x == 0 && y >= 2 && y <= 5)   return 0;
        if (x == 0 && y >= 6 && y <= 9)   return 1;
        if (x == 0 && y >= 10 && y <= 13) return 2;
    }
    return -1;
}

void AnimationManager::renderStartup() {
    uint32_t elapsed = millis() - startTime;

    if (elapsed >= STARTUP_RAMP_DURATION_MS) {
        complete = true;
        return;
    }

    float progress = (float)elapsed / (float)STARTUP_RAMP_DURATION_MS;
    uint32_t color = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);

    ledManager.clear();

    // Side panels: fade in during first section slot (0 to 1/NUM_SECTIONS) — checkerboard
    for (uint8_t p = PANEL_SIDE_LEFT; p <= PANEL_SIDE_RIGHT; p++) {
        PanelId sPanel = static_cast<PanelId>(p);
        float sideEnd = 1.0f / (float)NUM_SECTIONS;
        float sideBright = (progress < sideEnd) ? (progress / sideEnd) : 1.0f;
        uint8_t w = ledManager.getPanelWidth(sPanel);
        uint8_t h = ledManager.getPanelHeight(sPanel);
        uint8_t r = IDLE_COLOR_R * sideBright;
        uint8_t g = IDLE_COLOR_G * sideBright;
        uint8_t b = IDLE_COLOR_B * sideBright;
        uint32_t scaled = Adafruit_NeoPixel::Color(r, g, b);
        for (uint8_t x = 0; x < w; x++) {
            for (uint8_t y = 0; y < h; y++) {
                if (ledManager.isMasked(sPanel, x, y) && (x + y) % 2 == 0) {
                    ledManager.setPixelXY(sPanel, x, y, scaled);
                }
            }
        }
    }

    // Front panels: fade by section
    for (uint8_t p = PANEL_FRONT_LEFT; p <= PANEL_FRONT_RIGHT; p++) {
        PanelId panel = static_cast<PanelId>(p);
        uint8_t w = ledManager.getPanelWidth(panel);
        uint8_t h = ledManager.getPanelHeight(panel);

        for (uint8_t x = 0; x < w; x++) {
            for (uint8_t y = 0; y < h; y++) {
                if (!ledManager.isMasked(panel, x, y)) continue;
                if ((x + y) % 2 != 0) continue;  // Checkerboard

                int8_t section = getFrontSection(panel, x, y);
                if (section < 0) continue;

                float secStart = (float)section / (float)NUM_SECTIONS;
                float secEnd   = (float)(section + 1) / (float)NUM_SECTIONS;

                if (progress < secStart) continue;

                float secBright = (progress - secStart) / (secEnd - secStart);
                if (secBright > 1.0f) secBright = 1.0f;

                uint8_t r = IDLE_COLOR_R * secBright;
                uint8_t g = IDLE_COLOR_G * secBright;
                uint8_t b = IDLE_COLOR_B * secBright;
                ledManager.setPixelXY(panel, x, y, Adafruit_NeoPixel::Color(r, g, b));
            }
        }
    }

    ledManager.show();
    audioManager.pumpAudio();  // Feed I2S buffer after show() blocks ~10ms
}

// Static warm white glow on masked pixels
void AnimationManager::renderIdleGlow() {
    uint32_t color = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);
    ledManager.clear();
    for (uint8_t p = 0; p < PANEL_COUNT; p++) {
        ledManager.setMaskedColorCheckerboard(static_cast<PanelId>(p), color);
    }
    ledManager.show();
    audioManager.pumpAudio();  // Feed I2S buffer after show() blocks ~10ms
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

    // Phase 1: Flash (first 200ms) — checkerboard bright warm white
    if (elapsed < 200) {
        uint32_t flash = Adafruit_NeoPixel::Color(255, 220, 140);
        for (uint8_t p = 0; p < PANEL_COUNT; p++) {
            ledManager.setMaskedColorCheckerboard(static_cast<PanelId>(p), flash);
        }
        ledManager.show();
        audioManager.pumpAudio();  // Feed I2S buffer after show() blocks ~10ms
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
                if ((x + y) % 2 != 0) continue;  // Checkerboard
                // y=0 is top, y=h-1 is bottom. Fill from bottom up.
                if (y >= (h - 1 - fillRow)) {
                    ledManager.setPixelXY(panel, x, y, color);
                }
            }
        }
    }

    ledManager.show();
    audioManager.pumpAudio();  // Feed I2S buffer after show() blocks ~10ms
}
