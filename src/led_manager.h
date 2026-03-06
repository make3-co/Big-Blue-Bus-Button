#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPXL8.h>
#include "config.h"

// =============================================================================
// LED Manager — NeoPXL8 wrapper for 4 panels with coordinate mapping
// =============================================================================

// Logo pixel mask: packed bitmask array.
// For a 16x16 panel: 16 rows × 16 cols = 256 bits = 32 bytes.
// For an 8x32 panel:  32 rows × 8 cols = 256 bits = 32 bytes.
// Bit order: byte[y * (width/8) + x/8], bit (7 - x%8).
struct PanelMask {
    const uint8_t* data;    // Packed bitmask
    uint8_t        width;
    uint8_t        height;
};

class LedManager {
public:
    bool begin();
    void show();
    void clear();

    // Set a single pixel by panel + local (x, y) coordinate
    void setPixelXY(PanelId panel, uint8_t x, uint8_t y, uint32_t color);

    // Get the global pixel index for a panel + local coordinate
    uint16_t panelXYtoPixel(PanelId panel, uint8_t x, uint8_t y) const;

    // Fill entire panel with a color
    void fillPanel(PanelId panel, uint32_t color);

    // Fill only masked (logo) pixels on a panel
    void setMaskedColor(PanelId panel, uint32_t color);

    // Fill masked pixels with per-pixel brightness scaling (0.0–1.0)
    void setMaskedColorScaled(PanelId panel, uint32_t color, float brightness);

    // Set the panel mask (logo bitmap)
    void setPanelMask(PanelId panel, const uint8_t* maskData);

    // Check if a pixel is part of the logo mask
    bool isMasked(PanelId panel, uint8_t x, uint8_t y) const;

    // Get panel dimensions
    uint8_t getPanelWidth(PanelId panel) const;
    uint8_t getPanelHeight(PanelId panel) const;

    // Global brightness (applied by NeoPXL8)
    void setBrightness(uint8_t b);

    // Access underlying pixel buffer (for advanced animation)
    Adafruit_NeoPXL8* getStrip() { return strip; }

private:
    Adafruit_NeoPXL8* strip = nullptr;

    // Panel masks (one per panel, user-provided)
    PanelMask masks[PANEL_COUNT] = {};

    // Strand offset for each panel
    uint16_t strandOffset(PanelId panel) const;

    // Panel height for serpentine math
    uint8_t panelHeight(PanelId panel) const;
    uint8_t panelWidth(PanelId panel) const;
};

extern LedManager ledManager;
