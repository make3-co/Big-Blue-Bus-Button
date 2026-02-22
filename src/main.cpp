#include <Arduino.h>
#include <FastLED.h>

// =============================================================================
// SECTION 1: Hardware constants
// =============================================================================

static constexpr uint8_t  LED_DATA_PIN   = 5;    // GPIO 5 / D5 on Feather header
static constexpr uint16_t PANEL_COLS     = 32;   // LEDs per row
static constexpr uint8_t  PANEL_ROWS     = 8;    // Rows per physical panel
static constexpr uint8_t  NUM_PANELS     = 2;    // Panels stacked vertically
static constexpr uint16_t TOTAL_ROWS     = PANEL_ROWS * NUM_PANELS;  // 16
static constexpr uint16_t NUM_LEDS       = PANEL_COLS * TOTAL_ROWS;  // 512

// LED strip brightness 0–255. Use 255 with 5V supply; lower if on 3.3V or limited current.
static constexpr uint8_t LED_BRIGHTNESS = 255;

// How long each block stays lit before cycling to the next (milliseconds).
static constexpr uint16_t CYCLE_DELAY_MS = 800;

CRGB leds[NUM_LEDS];

// =============================================================================
// SECTION 2: Coordinate mapping
//
// Maps logical (x, y) grid coordinates to a FastLED array index.
//
// Coordinate system:
//   x : 0 (left)  to 31 (right)
//   y : 0 (top)   to 15 (bottom)
//
// Physical wiring: COLUMN-MAJOR SERPENTINE, per panel.
//
//   TOP PANEL — DIN at top-left, serpentine left → right:
//     col 0 (x=0):  down  (y=0 → y=7), LEDs   0–7
//     col 1 (x=1):  up    (y=7 → y=0), LEDs   8–15
//     ...
//     col 31 (x=31): up,               LEDs 248–255  ← DOUT exits top-right
//
//   BOTTOM PANEL — strip wired right → left (first LED 256 = physical right).
//     CSV is logical L→R like the top; we mirror x so logical (0,8)=504..511 (left).
//     physical right: 256–263, 264–271, … physical left: 504–511
//
// =============================================================================

uint16_t xy_to_index(uint8_t x, uint8_t y) {
    uint8_t panel_id = y / PANEL_ROWS;
    uint8_t local_y  = y % PANEL_ROWS;

    if (panel_id == 0) {
        // Top panel: L → R, even cols down, odd cols up
        uint16_t col_base = (uint16_t)x * PANEL_ROWS;
        if (x % 2 == 0)
            return col_base + local_y;
        else
            return col_base + (PANEL_ROWS - 1 - local_y);
    } else {
        // Bottom panel: strip wired R→L (mirror x) and row 0 = physical BOTTOM.
        // So mirror x (k = 31 - x) and flip y so logical y=8 = physical top.
        uint8_t  k            = (PANEL_COLS - 1) - x;
        uint8_t  local_y_inv = (PANEL_ROWS - 1) - local_y;
        uint16_t col_base    = 256 + (uint16_t)k * PANEL_ROWS;
        if (k % 2 == 0)
            return col_base + local_y_inv;
        else
            return col_base + (PANEL_ROWS - 1 - local_y_inv);
    }
}

// =============================================================================
// SECTION 3: Block definitions
//
// Each block is a rectangle: { x, y, width, height }
//   x, y   : top-left corner in the logical 32x16 grid (0-based)
//   width  : number of columns  (x + width  <= 32)
//   height : number of rows     (y + height <= 16)
//
// Corner LED indices used to derive each block (top-left, bottom-right):
//   B1: LED  13 = (x= 1, y= 2)  →  LED 133 = (x=16, y= 5)
//   B2: LED   9 = (x= 1, y= 6)  →  LED 385 = (x=16, y= 9)
//   B3: LED 269 = (x= 1, y=10)  →  LED 389 = (x=16, y=13)
//   B4: LED 141 = (x=17, y= 2)  →  LED 442 = (x=23, y=13)
// =============================================================================

struct LedBlock {
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
};

static constexpr uint8_t NUM_BLOCKS = 4;

LedBlock blocks[NUM_BLOCKS] = {
    {  1,  2, 16,  4 },   // B1: x=1..16,  y=2..5   (left, top band)
    {  1,  6, 16,  4 },   // B2: x=1..16,  y=6..9   (left, middle band)
    {  1, 10, 16,  4 },   // B3: x=1..16,  y=10..13 (left, bottom band)
    { 17,  2,  7, 12 },   // B4: x=17..23, y=2..13  (right, tall block)
};

// Color shown for each block.
CRGB block_colors[NUM_BLOCKS] = {
    CRGB::White,
    CRGB::White,
    CRGB::White,
    CRGB::White,
};

// =============================================================================
// SECTION 4: LED utility functions
// =============================================================================

// Turns off every LED and pushes the update immediately.
void clear_all() {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
}

// Fills every pixel in a rectangular block with the given color.
// Does NOT call FastLED.show() -- the caller decides when to push so that
// multiple blocks can be staged before a single hardware update.
void fill_block(const LedBlock& block, CRGB color) {
    for (uint8_t row = block.y; row < block.y + block.height; row++) {
        for (uint8_t col = block.x; col < block.x + block.width; col++) {
            leds[xy_to_index(col, row)] = color;
        }
    }
}

// =============================================================================
// SECTION 5: Arduino entry points
// =============================================================================

// Test mode: 0 = show block(s), 1 = pixel order test (one LED at a time, index 0..511)
static constexpr uint8_t PIXEL_ORDER_TEST = 0;

// Block cycle: ms each block is shown (when not in pixel order test)
static constexpr uint16_t BLOCK_CYCLE_DELAY_MS = 1000;

// Pixel order test: ms per LED (lower = faster sweep)
static constexpr uint16_t PIXEL_TEST_DELAY_MS = 50;

void setup() {
    Serial.begin(115200);
    delay(500);

    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(LED_BRIGHTNESS);
    clear_all();

    if (PIXEL_ORDER_TEST) {
        Serial.println("Pixel order test: one LED at a time, index 0..511");
    } else {
        Serial.println("Cycling blocks B1..B4, 1s each");
    }
}

void loop() {
    if (PIXEL_ORDER_TEST) {
        static uint16_t idx = 0;
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        leds[idx] = CRGB::White;
        FastLED.show();
        idx = (idx + 1) % NUM_LEDS;
        delay(PIXEL_TEST_DELAY_MS);
        return;
    }

    static uint8_t block_idx = 0;
    clear_all();
    fill_block(blocks[block_idx], block_colors[block_idx]);
    FastLED.show();
    delay(BLOCK_CYCLE_DELAY_MS);
    block_idx = (block_idx + 1) % NUM_BLOCKS;
}
