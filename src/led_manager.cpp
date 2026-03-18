#include "led_manager.h"

LedManager ledManager;

static int8_t neopxl8Pins[8] = {
    NEOPXL8_PIN_0, NEOPXL8_PIN_1, NEOPXL8_PIN_2, NEOPXL8_PIN_3,
    NEOPXL8_PIN_4, NEOPXL8_PIN_5, NEOPXL8_PIN_6, NEOPXL8_PIN_7
};

// Logo masks extracted from "Big Blue Button Front LED Mapping.xlsx"
// Bit order: byte[y * (width/8) + x/8], bit (7 - x%8)

// Front Left (16x16, strand 0) — 180 lit pixels
// Orange rows 2-5 cols 0-14, Blue rows 6-9 cols 0-14, Green rows 10-13 cols 0-14
static const uint8_t FRONT_LEFT_MASK[32] = {
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFE, 0xFF, 0xFE,
    0xFF, 0xFE, 0xFF, 0xFE, 0xFF, 0xFE, 0xFF, 0xFE,
    0xFF, 0xFE, 0xFF, 0xFE, 0xFF, 0xFE, 0xFF, 0xFE,
    0xFF, 0xFE, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00,
};

// Front Right (8x32 rotated sideways, display: 8 wide × 16 tall, strand 1) — 96 lit pixels
// Yellow section: all 8 cols × rows 2-13 (physical columns 2-13 of 8x32 panel)
// 8 pixels wide = 1 byte per row, 16 rows = 16 bytes
static const uint8_t FRONT_RIGHT_MASK[16] = {
    0x00,  // row 0: empty
    0x00,  // row 1: empty
    0xFF,  // row 2: all 8 cols
    0xFF,  // row 3
    0xFF,  // row 4
    0xFF,  // row 5
    0xFF,  // row 6
    0xFF,  // row 7
    0xFF,  // row 8
    0xFF,  // row 9
    0xFF,  // row 10
    0xFF,  // row 11
    0xFF,  // row 12
    0xFF,  // row 13
    0x00,  // row 14: empty
    0x00,  // row 15: empty
};

// Side Left (32x8, strand 2) — mirrored leaf pattern, 77 lit pixels
static const uint8_t SIDE_LEFT_MASK[32] = {
    0x00, 0x00, 0x00, 0x00,  // row 0
    0xEC, 0x8F, 0xF0, 0x00,  // row 1
    0x6E, 0xB7, 0xF8, 0x00,  // row 2
    0xFE, 0xF7, 0xE8, 0x00,  // row 3
    0x7F, 0x7D, 0xC0, 0x00,  // row 4
    0x7D, 0xA1, 0xC8, 0x00,  // row 5
    0x00, 0x00, 0x30, 0x00,  // row 6
    0x00, 0x00, 0x00, 0x00,  // row 7
};

// Side Right (32x8, strand 3) — all 11 leaves, 77 lit pixels
static const uint8_t SIDE_RIGHT_MASK[32] = {
    0x00, 0x00, 0x00, 0x00,  // row 0
    0x7F, 0x89, 0xB8, 0x00,  // row 1
    0xFF, 0x6B, 0xB0, 0x00,  // row 2
    0xBF, 0x7B, 0xF8, 0x00,  // row 3
    0x1D, 0xF7, 0xF0, 0x00,  // row 4
    0x9C, 0x2D, 0xF0, 0x00,  // row 5
    0x60, 0x00, 0x00, 0x00,  // row 6
    0x00, 0x00, 0x00, 0x00,  // row 7
};

bool LedManager::begin() {
    strip = new Adafruit_NeoPXL8(PIXELS_PER_STRAND, neopxl8Pins, NEO_GRB + NEO_KHZ800);
    if (!strip->begin()) {
        Serial.println("ERROR: NeoPXL8 init failed");
        return false;
    }
    strip->setBrightness(BRIGHTNESS_IDLE_MAX);
    strip->clear();
    strip->show();

    setPanelMask(PANEL_FRONT_LEFT,  FRONT_LEFT_MASK);
    setPanelMask(PANEL_FRONT_RIGHT, FRONT_RIGHT_MASK);
    setPanelMask(PANEL_SIDE_LEFT,   SIDE_LEFT_MASK);
    setPanelMask(PANEL_SIDE_RIGHT,  SIDE_RIGHT_MASK);

    Serial.println("LED Manager initialized");
    return true;
}

void LedManager::show() {
    strip->show();
}

void LedManager::clear() {
    strip->clear();
}

void LedManager::setBrightness(uint8_t b) {
    strip->setBrightness(b);
}

uint16_t LedManager::strandOffset(PanelId panel) const {
    switch (panel) {
        case PANEL_FRONT_LEFT:  return STRAND_OFFSET_FRONT_LEFT;
        case PANEL_FRONT_RIGHT: return STRAND_OFFSET_FRONT_RIGHT;
        case PANEL_SIDE_LEFT:   return STRAND_OFFSET_SIDE_LEFT;
        case PANEL_SIDE_RIGHT:  return STRAND_OFFSET_SIDE_RIGHT;
        default:                return 0;
    }
}

uint8_t LedManager::panelHeight(PanelId panel) const {
    if (panel == PANEL_FRONT_LEFT)  return FRONT_LEFT_HEIGHT;
    if (panel == PANEL_FRONT_RIGHT) return FRONT_RIGHT_HEIGHT;
    return SIDE_PANEL_HEIGHT;
}

uint8_t LedManager::panelWidth(PanelId panel) const {
    if (panel == PANEL_FRONT_LEFT)  return FRONT_LEFT_WIDTH;
    if (panel == PANEL_FRONT_RIGHT) return FRONT_RIGHT_WIDTH;
    return SIDE_PANEL_WIDTH;
}

uint8_t LedManager::getPanelWidth(PanelId panel) const {
    return panelWidth(panel);
}

uint8_t LedManager::getPanelHeight(PanelId panel) const {
    return panelHeight(panel);
}

// Column-major serpentine mapping:
// Even columns: top-to-bottom (y=0 at start)
// Odd columns: bottom-to-top (y=height-1 at start)
//
// Front right panel is an 8x32 panel rotated 90°:
//   display x (0-7) = physical row, display y (0-15) = physical column
//   pixel = offset + y * 8 + (y%2==0 ? x : 7-x)
uint16_t LedManager::panelXYtoPixel(PanelId panel, uint8_t x, uint8_t y) const {
    uint16_t offset = strandOffset(panel);

    if (panel == PANEL_FRONT_RIGHT) {
        // Rotated 8x32: display_y = physical column, display_x = physical row
        uint16_t colBase = offset + (uint16_t)y * FRONT_RIGHT_PHYS_COL_HEIGHT;
        if (y % 2 == 0) {
            return colBase + x;
        } else {
            return colBase + (FRONT_RIGHT_PHYS_COL_HEIGHT - 1 - x);
        }
    }

    // Side panels need coordinate corrections for physical mounting
    uint8_t h = panelHeight(panel);
    uint8_t mx = x;
    uint8_t my = y;
    if (panel == PANEL_SIDE_LEFT) {
        // Left side: flip y only (mirror rows)
        my = h - 1 - y;
    } else if (panel == PANEL_SIDE_RIGHT) {
        // Right side: flip both x and y (180° rotation)
        if (x >= SIDE_PANEL_COLS) return strandOffset(panel);
        mx = SIDE_PANEL_COLS - 1 - x;
        my = h - 1 - y;
    }

    // Standard column-major serpentine
    uint16_t colBase = offset + (uint16_t)mx * h;

    if (mx % 2 == 0) {
        return colBase + my;           // Even column: top to bottom
    } else {
        return colBase + (h - 1 - my); // Odd column: bottom to top
    }
}

void LedManager::setPixelXY(PanelId panel, uint8_t x, uint8_t y, uint32_t color) {
    uint16_t idx = panelXYtoPixel(panel, x, y);
    strip->setPixelColor(idx, color);
}

void LedManager::fillPanel(PanelId panel, uint32_t color) {
    uint8_t w = panelWidth(panel);
    uint8_t h = panelHeight(panel);
    for (uint8_t x = 0; x < w; x++) {
        for (uint8_t y = 0; y < h; y++) {
            setPixelXY(panel, x, y, color);
        }
    }
}

void LedManager::setPanelMask(PanelId panel, const uint8_t* maskData) {
    masks[panel].data   = maskData;
    masks[panel].width  = panelWidth(panel);
    masks[panel].height = panelHeight(panel);
}

bool LedManager::isMasked(PanelId panel, uint8_t x, uint8_t y) const {
    const PanelMask& m = masks[panel];
    if (!m.data || x >= m.width || y >= m.height) return false;

    uint16_t bitIndex = (uint16_t)y * m.width + x;
    uint16_t byteIndex = bitIndex / 8;
    uint8_t  bitOffset = 7 - (bitIndex % 8);  // MSB first
    return (m.data[byteIndex] >> bitOffset) & 1;
}

void LedManager::setMaskedColor(PanelId panel, uint32_t color) {
    uint8_t w = panelWidth(panel);
    uint8_t h = panelHeight(panel);
    for (uint8_t x = 0; x < w; x++) {
        for (uint8_t y = 0; y < h; y++) {
            if (isMasked(panel, x, y)) {
                setPixelXY(panel, x, y, color);
            }
        }
    }
}

void LedManager::setMaskedColorCheckerboard(PanelId panel, uint32_t color) {
    uint8_t w = panelWidth(panel);
    uint8_t h = panelHeight(panel);
    for (uint8_t x = 0; x < w; x++) {
        for (uint8_t y = 0; y < h; y++) {
            if (isMasked(panel, x, y) && ((x + y) % 2 == 0)) {
                setPixelXY(panel, x, y, color);
            }
        }
    }
}

void LedManager::setMaskedColorScaled(PanelId panel, uint32_t color, float brightness) {
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 1.0f) brightness = 1.0f;

    uint8_t r = ((color >> 16) & 0xFF) * brightness;
    uint8_t g = ((color >>  8) & 0xFF) * brightness;
    uint8_t b = ( color        & 0xFF) * brightness;
    uint32_t scaled = Adafruit_NeoPixel::Color(r, g, b);

    setMaskedColor(panel, scaled);
}
