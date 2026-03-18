#include "animation.h"
#include "led_manager.h"
#include "audio_manager.h"
#include "battery_indicator.h"

AnimationManager animationManager;

static const char* pressStyleNames[] = {
    "Top-Down Fill",
    "Left-Right Sweep",
    "Rainbow Sweep",
};

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

    Serial.printf("Animation: %s\n", pressStyleNames[(uint8_t)pressStyle]);
}

void AnimationManager::stop() {
    current = AnimationType::NONE;
    complete = false;
}

// Logo section colors (matching spreadsheet)
static const uint32_t COLOR_ORANGE = 0xFF6600;  // Section 1: front left rows 2-5
static const uint32_t COLOR_BLUE   = 0x0066FF;  // Section 2: front left rows 6-9
static const uint32_t COLOR_GREEN  = 0x00CC00;  // Section 3: front left rows 10-13
static const uint32_t COLOR_YELLOW = 0xFFCC00;  // Section 4: front right rows 2-13

static uint32_t scaleColor(uint32_t raw, float b) {
    return Adafruit_NeoPixel::Color(
        (uint8_t)(((raw >> 16) & 0xFF) * b),
        (uint8_t)(((raw >> 8) & 0xFF) * b),
        (uint8_t)((raw & 0xFF) * b));
}

// Startup: sides fade front-to-back together, then front left bottom-to-top, then front right
// Phase 0: sides (0.0-0.4), Phase 1-3: front left sections bottom-top (0.4-0.8), Phase 4: front right (0.8-1.0)
static constexpr float SIDE_END    = 0.4f;
static constexpr float FL_START    = 0.4f;
static constexpr float FL_END      = 0.8f;
static constexpr float FR_START    = 0.8f;

// Fade helper: returns 0.0-1.0 for a value within a range
static float fadeBrightness(float progress, float start, float end) {
    if (progress < start) return 0.0f;
    if (progress >= end) return 1.0f;
    return (progress - start) / (end - start);
}

void AnimationManager::renderStartup() {
    uint32_t elapsed = millis() - startTime;

    if (elapsed >= STARTUP_RAMP_DURATION_MS) {
        complete = true;
        return;
    }

    float progress = (float)elapsed / (float)STARTUP_RAMP_DURATION_MS;
    uint32_t warmWhite = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);

    ledManager.clear();

    // Phase 0: Both sides — fade leaves front-to-back (high x first)
    // Each pixel's fade start is based on its x position: x=20 starts first, x=0 last
    if (progress < SIDE_END || progress >= 0.0f) {
        for (uint8_t p = PANEL_SIDE_LEFT; p <= PANEL_SIDE_RIGHT; p++) {
            PanelId panel = static_cast<PanelId>(p);
            uint8_t w = SIDE_PANEL_COLS;
            uint8_t h = ledManager.getPanelHeight(panel);
            for (uint8_t x = 0; x < w; x++) {
                // Front-to-back: x=20 fades at 0.0, x=0 fades at ~0.3 (within SIDE_END)
                float pixStart = (float)(SIDE_PANEL_COLS - 1 - x) / (float)SIDE_PANEL_COLS * SIDE_END * 0.7f;
                float pixEnd = pixStart + SIDE_END * 0.5f;
                float b = fadeBrightness(progress, pixStart, pixEnd);
                if (b <= 0.0f) continue;
                uint32_t c = Adafruit_NeoPixel::Color(
                    (uint8_t)(IDLE_COLOR_R * b), (uint8_t)(IDLE_COLOR_G * b), (uint8_t)(IDLE_COLOR_B * b));
                for (uint8_t y = 0; y < h; y++) {
                    if (ledManager.isMasked(panel, x, y)) {
                        ledManager.setPixelXY(panel, x, y, c);
                    }
                }
            }
        }
    }

    // Phase 1-3: Front left sections — top to bottom (orange first, then blue, then green)
    static constexpr uint8_t secRowStart[] = {2, 6, 10};   // top to bottom
    static constexpr uint8_t secRowEnd[]   = {5, 9, 13};
    uint8_t flW = ledManager.getPanelWidth(PANEL_FRONT_LEFT);
    float flSecDur = (FL_END - FL_START) / 3.0f;

    for (uint8_t sec = 0; sec < 3; sec++) {
        float secStart = FL_START + sec * flSecDur;
        float secEnd = secStart + flSecDur;
        float b = fadeBrightness(progress, secStart, secEnd);
        if (b <= 0.0f) continue;
        uint32_t c = Adafruit_NeoPixel::Color(
            (uint8_t)(IDLE_COLOR_R * b), (uint8_t)(IDLE_COLOR_G * b), (uint8_t)(IDLE_COLOR_B * b));
        for (uint8_t x = 0; x < flW; x++) {
            for (uint8_t y = secRowStart[sec]; y <= secRowEnd[sec]; y++) {
                if (!ledManager.isMasked(PANEL_FRONT_LEFT, x, y)) continue;
                ledManager.setPixelXY(PANEL_FRONT_LEFT, x, y, c);
            }
        }
    }

    // Phase 4: Front right — fade up bottom to top by row
    if (progress > FR_START) {
        uint8_t frW = ledManager.getPanelWidth(PANEL_FRONT_RIGHT);
        uint8_t frH = ledManager.getPanelHeight(PANEL_FRONT_RIGHT);
        float frProgress = (progress - FR_START) / (1.0f - FR_START);
        for (uint8_t y = 0; y < frH; y++) {
            // Bottom to top: high y fades first
            float rowStart = (float)(frH - 1 - y) / (float)frH;
            float rowEnd = rowStart + 1.0f / (float)frH * 2.0f;  // overlap for smooth fade
            float b = fadeBrightness(frProgress, rowStart, rowEnd);
            if (b <= 0.0f) continue;
            uint32_t c = Adafruit_NeoPixel::Color(
                (uint8_t)(IDLE_COLOR_R * b), (uint8_t)(IDLE_COLOR_G * b), (uint8_t)(IDLE_COLOR_B * b));
            for (uint8_t x = 0; x < frW; x++) {
                if (ledManager.isMasked(PANEL_FRONT_RIGHT, x, y)) {
                    ledManager.setPixelXY(PANEL_FRONT_RIGHT, x, y, c);
                }
            }
        }
    }

    ledManager.show();
    audioManager.pumpAudio();
}

// Warm white glow with slow brightness undulation
// Cycle: hold peak 5s → fade down 5s → hold low 5s → fade up 5s = 20s total
// Starts at peak since startup animation ends at full brightness
// Idle glow cycle: initial fade from startup brightness → peak → normal cycle
static constexpr uint32_t GLOW_INITIAL_HOLD_MS = 1000;  // Hold at startup brightness
static constexpr uint32_t GLOW_INITIAL_FADE_MS = 2000;  // Fade from startup → peak
static constexpr uint32_t GLOW_HOLD_PEAK_MS    = 5000;
static constexpr uint32_t GLOW_FADE_DOWN_MS    = 5000;
static constexpr uint32_t GLOW_HOLD_LOW_MS     = 5000;
static constexpr uint32_t GLOW_FADE_UP_MS      = 5000;
static constexpr uint32_t GLOW_CYCLE_MS        = GLOW_HOLD_PEAK_MS + GLOW_FADE_DOWN_MS + GLOW_HOLD_LOW_MS + GLOW_FADE_UP_MS;
static constexpr float GLOW_BRIGHT_MIN     = 0.40f;
static constexpr float GLOW_BRIGHT_MAX     = 0.65f;
static constexpr float GLOW_BRIGHT_STARTUP = 1.00f;  // Startup ends at full

void AnimationManager::renderIdleGlow() {
    uint32_t elapsed = millis() - startTime;
    float bright;

    if (elapsed < GLOW_INITIAL_HOLD_MS) {
        // Hold at startup brightness to stabilize
        bright = GLOW_BRIGHT_STARTUP;
    } else if (elapsed < GLOW_INITIAL_HOLD_MS + GLOW_INITIAL_FADE_MS) {
        // Smooth fade from startup brightness down to peak
        float t = (float)(elapsed - GLOW_INITIAL_HOLD_MS) / (float)GLOW_INITIAL_FADE_MS;
        bright = GLOW_BRIGHT_STARTUP - t * (GLOW_BRIGHT_STARTUP - GLOW_BRIGHT_MAX);
    } else {
        // Normal repeating cycle
        uint32_t cycleElapsed = (elapsed - GLOW_INITIAL_HOLD_MS - GLOW_INITIAL_FADE_MS) % GLOW_CYCLE_MS;

        if (cycleElapsed < GLOW_HOLD_PEAK_MS) {
            bright = GLOW_BRIGHT_MAX;
        } else if (cycleElapsed < GLOW_HOLD_PEAK_MS + GLOW_FADE_DOWN_MS) {
            float t = (float)(cycleElapsed - GLOW_HOLD_PEAK_MS) / (float)GLOW_FADE_DOWN_MS;
            bright = GLOW_BRIGHT_MAX - t * (GLOW_BRIGHT_MAX - GLOW_BRIGHT_MIN);
        } else if (cycleElapsed < GLOW_HOLD_PEAK_MS + GLOW_FADE_DOWN_MS + GLOW_HOLD_LOW_MS) {
            bright = GLOW_BRIGHT_MIN;
        } else {
            float t = (float)(cycleElapsed - GLOW_HOLD_PEAK_MS - GLOW_FADE_DOWN_MS - GLOW_HOLD_LOW_MS) / (float)GLOW_FADE_UP_MS;
            bright = GLOW_BRIGHT_MIN + t * (GLOW_BRIGHT_MAX - GLOW_BRIGHT_MIN);
        }
    }

    // Signal low brightness window for battery check (most accurate under low load)
    batteryIndicator.setLowBrightnessWindow(bright <= GLOW_BRIGHT_MIN + 0.05f);

    uint32_t c = Adafruit_NeoPixel::Color(
        (uint8_t)(IDLE_COLOR_R * bright),
        (uint8_t)(IDLE_COLOR_G * bright),
        (uint8_t)(IDLE_COLOR_B * bright));
    ledManager.clear();
    ledManager.setMaskedColor(PANEL_SIDE_LEFT, c);
    ledManager.setMaskedColor(PANEL_SIDE_RIGHT, c);
    ledManager.setMaskedColor(PANEL_FRONT_LEFT, c);
    ledManager.setMaskedColor(PANEL_FRONT_RIGHT, c);
    batteryIndicator.render(batteryIndicator.getPercent());
    ledManager.show();
    audioManager.pumpAudio();
}


// Button press: dispatch to current style, then show identify color
void AnimationManager::renderButtonPress() {
    if (complete) return;  // Already finished, waiting for audio/state machine

    uint32_t elapsed = millis() - startTime;

    if (elapsed >= ANIMATION_DURATION_MS) {
        complete = true;
        // Advance to next style for next press
        pressStyle = static_cast<PressStyle>(((uint8_t)pressStyle + 1) % (uint8_t)PressStyle::PRESS_STYLE_COUNT);
        return;
    }

    float progress = (float)elapsed / (float)ANIMATION_DURATION_MS;
    ledManager.clear();

    switch (pressStyle) {
        case PressStyle::FILL_TOP_DOWN:     renderFillTopDown(progress); break;
        case PressStyle::FILL_LEFT_RIGHT:   renderFillLeftRight(progress); break;
        case PressStyle::RAINBOW_SWEEP:     renderRainbowSweep(progress); break;
        default: break;
    }

    ledManager.show();
    audioManager.pumpAudio();
}

// =============================================================================
// Individual button press animation renderers
// Side panels: front-to-back (high x first), no checkerboard
// Front panels: no checkerboard
// =============================================================================

// Helper: render side panels with front-to-back x-based progress
static void renderSidePanelsFrontToBack(float progress, uint32_t color) {
    for (uint8_t p = PANEL_SIDE_LEFT; p <= PANEL_SIDE_RIGHT; p++) {
        PanelId panel = static_cast<PanelId>(p);
        uint8_t w = SIDE_PANEL_COLS;
        uint8_t h = ledManager.getPanelHeight(panel);
        for (uint8_t x = 0; x < w; x++) {
            // Front-to-back: x=0 fills first, x=20 fills last
            float pixProgress = (float)x / (float)w;
            if (progress < pixProgress) continue;
            for (uint8_t y = 0; y < h; y++) {
                if (ledManager.isMasked(panel, x, y)) {
                    ledManager.setPixelXY(panel, x, y, color);
                }
            }
        }
    }
}

// 1. Top-to-bottom fill (front left top-down, front right bottom-up)
void AnimationManager::renderFillTopDown(float progress) {
    uint32_t color = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);
    // Front left: top-to-bottom
    {
        uint8_t w = ledManager.getPanelWidth(PANEL_FRONT_LEFT);
        uint8_t h = ledManager.getPanelHeight(PANEL_FRONT_LEFT);
        uint8_t fillRow = (uint8_t)(progress * h);
        for (uint8_t x = 0; x < w; x++) {
            for (uint8_t y = 0; y < h; y++) {
                if (!ledManager.isMasked(PANEL_FRONT_LEFT, x, y)) continue;
                if (y <= fillRow) {
                    ledManager.setPixelXY(PANEL_FRONT_LEFT, x, y, color);
                }
            }
        }
    }
    // Front right: bottom-to-top
    {
        uint8_t w = ledManager.getPanelWidth(PANEL_FRONT_RIGHT);
        uint8_t h = ledManager.getPanelHeight(PANEL_FRONT_RIGHT);
        uint8_t fillRow = (uint8_t)(progress * h);
        for (uint8_t x = 0; x < w; x++) {
            for (uint8_t y = 0; y < h; y++) {
                if (!ledManager.isMasked(PANEL_FRONT_RIGHT, x, y)) continue;
                if (y >= (h - 1 - fillRow)) {
                    ledManager.setPixelXY(PANEL_FRONT_RIGHT, x, y, color);
                }
            }
        }
    }
    // Side panels: front-to-back
    renderSidePanelsFrontToBack(progress, color);
}

// 2. Left-to-right sweep
void AnimationManager::renderFillLeftRight(float progress) {
    uint32_t color = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);
    // Front panels
    for (uint8_t p = PANEL_FRONT_LEFT; p <= PANEL_FRONT_RIGHT; p++) {
        PanelId panel = static_cast<PanelId>(p);
        uint8_t w = ledManager.getPanelWidth(panel);
        uint8_t h = ledManager.getPanelHeight(panel);
        uint8_t fillCol = (uint8_t)(progress * w);
        for (uint8_t x = 0; x < w; x++) {
            for (uint8_t y = 0; y < h; y++) {
                if (!ledManager.isMasked(panel, x, y)) continue;
                if (x <= fillCol) {
                    ledManager.setPixelXY(panel, x, y, color);
                }
            }
        }
    }
    // Side panels: front-to-back
    renderSidePanelsFrontToBack(progress, color);
}

// 3. Rainbow sweep — hue shifts across all panels
void AnimationManager::renderRainbowSweep(float progress) {
    // Front panels
    for (uint8_t p = PANEL_FRONT_LEFT; p <= PANEL_FRONT_RIGHT; p++) {
        PanelId panel = static_cast<PanelId>(p);
        uint8_t w = ledManager.getPanelWidth(panel);
        uint8_t h = ledManager.getPanelHeight(panel);
        for (uint8_t x = 0; x < w; x++) {
            for (uint8_t y = 0; y < h; y++) {
                if (!ledManager.isMasked(panel, x, y)) continue;
                uint16_t hue = (uint16_t)((progress * 65536.0f) + (x + y) * 2048) % 65536;
                uint32_t c = Adafruit_NeoPixel::ColorHSV(hue, 255, 200);
                ledManager.setPixelXY(panel, x, y, c);
            }
        }
    }
    // Side panels: rainbow front-to-back
    for (uint8_t p = PANEL_SIDE_LEFT; p <= PANEL_SIDE_RIGHT; p++) {
        PanelId panel = static_cast<PanelId>(p);
        uint8_t w = SIDE_PANEL_COLS;
        uint8_t h = ledManager.getPanelHeight(panel);
        for (uint8_t x = 0; x < w; x++) {
            for (uint8_t y = 0; y < h; y++) {
                if (!ledManager.isMasked(panel, x, y)) continue;
                uint16_t hue = (uint16_t)((progress * 65536.0f) + (x + y) * 2048) % 65536;
                uint32_t c = Adafruit_NeoPixel::ColorHSV(hue, 255, 200);
                ledManager.setPixelXY(panel, x, y, c);
            }
        }
    }
}

