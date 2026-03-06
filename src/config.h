#pragma once

#include <Arduino.h>

// =============================================================================
// GPIO Pin Assignments — Adafruit Feather ESP32-S3
// =============================================================================

// NeoPXL8 LED outputs (directly driven by NeoPXL8 library)
static constexpr int8_t NEOPXL8_PIN_0 = 36;  // SCK  — Front left 16x16 panel
static constexpr int8_t NEOPXL8_PIN_1 =  5;  // D5   — Front right 16x16 panel
static constexpr int8_t NEOPXL8_PIN_2 =  9;  // D9   — Left side 8x32 panel
static constexpr int8_t NEOPXL8_PIN_3 =  6;  // D6   — Right side 8x32 panel
static constexpr int8_t NEOPXL8_PIN_4 = -1;  // Unused (physically routed on FeatherWing)
static constexpr int8_t NEOPXL8_PIN_5 = -1;  // Unused
static constexpr int8_t NEOPXL8_PIN_6 = -1;  // Unused
static constexpr int8_t NEOPXL8_PIN_7 = -1;  // Unused

// I2S audio output to MAX98357A
static constexpr uint8_t I2S_BCLK_PIN = 18;  // A0 — Bit clock
static constexpr uint8_t I2S_LRC_PIN  = 17;  // A1 — Word select (LRCLK)
static constexpr uint8_t I2S_DOUT_PIN = 16;  // A2 — Data out

// Amplifier shutdown control
static constexpr uint8_t AMP_SHUTDOWN_PIN = 14;  // A4 — HIGH=on, LOW=off

// Button input
static constexpr uint8_t BUTTON_PIN = 15;  // A3 — INPUT_PULLUP, wire to GND

// =============================================================================
// Panel Geometry
// =============================================================================

// Panel identifiers
enum PanelId : uint8_t {
    PANEL_FRONT_LEFT  = 0,
    PANEL_FRONT_RIGHT = 1,
    PANEL_SIDE_LEFT   = 2,
    PANEL_SIDE_RIGHT  = 3,
    PANEL_COUNT       = 4
};

// Front panels: 16x16
static constexpr uint8_t FRONT_PANEL_WIDTH  = 16;
static constexpr uint8_t FRONT_PANEL_HEIGHT = 16;
static constexpr uint16_t FRONT_PANEL_PIXELS = FRONT_PANEL_WIDTH * FRONT_PANEL_HEIGHT;  // 256

// Side panels: 8x32
static constexpr uint8_t SIDE_PANEL_WIDTH  = 8;
static constexpr uint8_t SIDE_PANEL_HEIGHT = 32;
static constexpr uint16_t SIDE_PANEL_PIXELS = SIDE_PANEL_WIDTH * SIDE_PANEL_HEIGHT;  // 256

// NeoPXL8 total buffer: 8 strands × 256 pixels each
static constexpr uint8_t  NEOPXL8_STRANDS     = 8;
static constexpr uint16_t PIXELS_PER_STRAND   = 256;
static constexpr uint16_t TOTAL_PIXEL_BUFFER  = NEOPXL8_STRANDS * PIXELS_PER_STRAND;  // 2048

// Strand offsets in the NeoPXL8 pixel buffer
static constexpr uint16_t STRAND_OFFSET_FRONT_LEFT  = 0 * PIXELS_PER_STRAND;  // 0
static constexpr uint16_t STRAND_OFFSET_FRONT_RIGHT = 1 * PIXELS_PER_STRAND;  // 256
static constexpr uint16_t STRAND_OFFSET_SIDE_LEFT   = 2 * PIXELS_PER_STRAND;  // 512
static constexpr uint16_t STRAND_OFFSET_SIDE_RIGHT  = 3 * PIXELS_PER_STRAND;  // 768

// =============================================================================
// Brightness
// =============================================================================

static constexpr uint8_t BRIGHTNESS_IDLE_MIN  = 20;   // Pulse trough
static constexpr uint8_t BRIGHTNESS_IDLE_MAX  = 60;   // Pulse peak (tune for outdoor visibility vs battery)
static constexpr uint8_t BRIGHTNESS_ACTIVE    = 180;  // During button press animation
static constexpr uint8_t BRIGHTNESS_MAX       = 255;  // Absolute max

// =============================================================================
// Timing
// =============================================================================

static constexpr uint32_t IDLE_PULSE_PERIOD_MS  = 3000;   // Full breath cycle
static constexpr uint32_t ANIMATION_DURATION_MS = 2000;   // Button press animation length
static constexpr uint32_t COOLDOWN_DURATION_MS  = 1000;   // Ignore button after animation
static constexpr uint32_t BUTTON_DEBOUNCE_MS    = 50;     // Debounce time

// =============================================================================
// ESP-NOW
// =============================================================================

static constexpr uint8_t ESPNOW_REDUNDANT_SENDS = 3;  // Send each message N times for reliability
static constexpr uint32_t ESPNOW_SEND_INTERVAL_MS = 5; // Delay between redundant sends

// QT Py receiver MAC address — UPDATE THIS after discovering receiver MAC
static constexpr uint8_t RECEIVER_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // Broadcast until configured
