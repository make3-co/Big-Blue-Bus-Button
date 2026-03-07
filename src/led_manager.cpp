#include "led_manager.h"

LedManager ledManager;

static int8_t neopxl8Pins[8] = {
    NEOPXL8_PIN_0, NEOPXL8_PIN_1, NEOPXL8_PIN_2, NEOPXL8_PIN_3,
    NEOPXL8_PIN_4, NEOPXL8_PIN_5, NEOPXL8_PIN_6, NEOPXL8_PIN_7
};

// Logo masks extracted from "Big Blue Button Front LED Mapping.xlsx"
// Bit order: byte[y * (width/8) + x/8], bit (7 - x%8)

// Front Left (16x16, strand 0) — 180 lit pixels
// Orange rows 2-5 cols 1-15, Blue rows 6-9 cols 1-15, Green rows 10-13 cols 1-15
static const uint8_t FRONT_LEFT_MASK[32] = {
    0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0x7F, 0xFF,
    0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF,
    0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF, 0x7F, 0xFF,
    0x7F, 0xFF, 0x7F, 0xFF, 0x00, 0x00, 0x00, 0x00,
};

// Front Right (16x16, strand 1) — 96 lit pixels
// Orange col 0 rows 2-5, Blue col 0 rows 6-9, Green col 0 rows 10-13, Yellow cols 1-7 rows 2-13
static const uint8_t FRONT_RIGHT_MASK[32] = {
    0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
    0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
    0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
    0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Side Left (32x8, strand 2) — 168 lit pixels (cols 0-20 all 8 rows)
// Width=32 → 4 bytes/row, 8 rows = 32 bytes. Cols 0-20 lit: 0xFF 0xFF 0xF8 0x00
static const uint8_t SIDE_LEFT_MASK[32] = {
    0xFF, 0xFF, 0xF8, 0x00,  // row 0
    0xFF, 0xFF, 0xF8, 0x00,  // row 1
    0xFF, 0xFF, 0xF8, 0x00,  // row 2
    0xFF, 0xFF, 0xF8, 0x00,  // row 3
    0xFF, 0xFF, 0xF8, 0x00,  // row 4
    0xFF, 0xFF, 0xF8, 0x00,  // row 5
    0xFF, 0xFF, 0xF8, 0x00,  // row 6
    0xFF, 0xFF, 0xF8, 0x00,  // row 7
};

// Side Right (32x8, strand 3) — 168 lit pixels (cols 0-20 all 8 rows)
static const uint8_t SIDE_RIGHT_MASK[32] = {
    0xFF, 0xFF, 0xF8, 0x00,  // row 0
    0xFF, 0xFF, 0xF8, 0x00,  // row 1
    0xFF, 0xFF, 0xF8, 0x00,  // row 2
    0xFF, 0xFF, 0xF8, 0x00,  // row 3
    0xFF, 0xFF, 0xF8, 0x00,  // row 4
    0xFF, 0xFF, 0xF8, 0x00,  // row 5
    0xFF, 0xFF, 0xF8, 0x00,  // row 6
    0xFF, 0xFF, 0xF8, 0x00,  // row 7
};

bool LedManager::begin() {
    strip = new Adafruit_NeoPXL8(PIXELS_PER_STRAND, neopxl8Pins, NEO_GRB + NEO_KHZ800);
    if (!strip->begin()) {
        Serial.println("ERROR: NeoPXL8 init failed");
        return false;
    }

    // Start at brightness 0 and send multiple clears to kill any boot noise
    // that WS2812B may have latched from GPIO state during ESP32 startup.
    // This prevents inrush current from crashing the boost converter.
    strip->setBrightness(0);
    strip->clear();
    strip->show();
    delay(2);   // WS2812B needs ~300us latch time; give extra margin
    strip->show();  // Second clear for reliability
    delay(10);  // Let power supply stabilize with all LEDs confirmed off

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
    return (panel <= PANEL_FRONT_RIGHT) ? FRONT_PANEL_HEIGHT : SIDE_PANEL_HEIGHT;
}

uint8_t LedManager::panelWidth(PanelId panel) const {
    return (panel <= PANEL_FRONT_RIGHT) ? FRONT_PANEL_WIDTH : SIDE_PANEL_WIDTH;
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
uint16_t LedManager::panelXYtoPixel(PanelId panel, uint8_t x, uint8_t y) const {
    uint16_t offset = strandOffset(panel);
    uint8_t h = panelHeight(panel);
    uint16_t colBase = offset + (uint16_t)x * h;

    if (x % 2 == 0) {
        return colBase + y;           // Even column: top to bottom
    } else {
        return colBase + (h - 1 - y); // Odd column: bottom to top
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

void LedManager::setMaskedColorScaled(PanelId panel, uint32_t color, float brightness) {
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 1.0f) brightness = 1.0f;

    uint8_t r = ((color >> 16) & 0xFF) * brightness;
    uint8_t g = ((color >>  8) & 0xFF) * brightness;
    uint8_t b = ( color        & 0xFF) * brightness;
    uint32_t scaled = Adafruit_NeoPixel::Color(r, g, b);

    setMaskedColor(panel, scaled);
}
