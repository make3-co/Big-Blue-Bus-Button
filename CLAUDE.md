# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Embedded firmware for a "Big Blue Bus" event button. When pressed: LED panels animate across backlit logos, a sound plays through a speaker, and a spacebar keypress is sent wirelessly to a PC running PowerPoint. Battery-powered (20Ah LiPo, 2x 10Ah in parallel), designed for 2-3 hours outdoor use.

**Hardware:**
- Main unit: Adafruit Feather ESP32-S3 (4MB Flash + 2MB PSRAM)
- Receiver: Adafruit QT Py ESP32-S3 (no PSRAM) → USB HID spacebar
- Display: 4 panels of WS2812B LEDs driven via Adafruit NeoPXL8 (parallel DMA via LCD_CAM peripheral)
  - 2x 16x16 front panels (front-left, front-right)
  - 2x 8x32 side panels (side-left, side-right)
- Audio: MAX98357A I2S amplifier, WAV playback from LittleFS
- Input: Momentary pushbutton (GPIO 15, active LOW)
- Wireless: ESP-NOW between Feather and QT Py
- Power: 2x 10,000mAh LiPo in parallel → BQ24074 charger, TPS61088 boost to 5V for LEDs

## Build Commands

PlatformIO project with two environments. **Must run `pio` from the project root directory** (or use `pio -d <path>`).

```bash
# Main button firmware (Feather ESP32-S3)
pio run -e feather_esp32s3                        # Build
pio run -e feather_esp32s3 -t upload              # Flash
pio run -e feather_esp32s3 -t uploadfs            # Upload WAV files to LittleFS
pio device monitor -b 115200                       # Serial monitor

# QT Py receiver firmware
pio run -e qtpy_esp32s3                           # Build
pio run -e qtpy_esp32s3 -t upload                 # Flash

# Build both environments
pio run                                            # Build all
```

No linter configured. No package.json or Makefile.

## Architecture

### File Structure

```
src/
├── main.cpp           # State machine: IDLE → ANIMATING → COOLDOWN
├── config.h           # All pins, constants, timing, panel geometry
├── protocol.h         # Shared ESP-NOW message struct
├── led_manager.h/cpp  # NeoPXL8 wrapper, panel mapping, pixel masks
├── animation.h/cpp    # Non-blocking animation system
├── audio_manager.h/cpp # I2S WAV playback from LittleFS
├── button_handler.h/cpp # Debounced button input
├── espnow_sender.h/cpp  # ESP-NOW transmission
└── power_manager.h/cpp  # Brightness control, amp shutdown, modem sleep

src_qtpy/
├── main.cpp           # ESP-NOW receiver → USB HID spacebar
└── protocol.h         # Copy of shared protocol (keep in sync with src/protocol.h)

data/                  # LittleFS filesystem (place WAV files here, upload with uploadfs)

docs/                  # Reference/planning files (not compiled)
├── Big Blue Button Front LED Mapping.xlsx  # Source spreadsheet for logo masks
├── COMPONENTS.md      # Component list and wiring summary
├── WIRING.md          # Detailed wiring diagrams
└── pictures/          # Reference photos of hardware
```

### State Machine (main.cpp)

```
IDLE (logos pulse white) ──button press──▶ ANIMATING (rainbow fill + sound + ESP-NOW)
     ▲                                          │
     │                                          ▼ animation complete
     └──────────── cooldown expires ◀── COOLDOWN (resume pulse, ignore button)
```

### Module Responsibilities

- **led_manager** — Wraps Adafruit_NeoPXL8. Manages 4 panels with `setPixelXY(panel, x, y, color)`. Column-major serpentine coordinate mapping. Logo pixel masks as packed bitmask arrays. `setMaskedColor()` lights only logo pixels.
- **animation** — Non-blocking animation runner using `millis()`. Built-in: idle white pulse (sinusoidal), button press rainbow sweep.
- **audio_manager** — Wraps ESP32-audioI2S library (pinned to v3.0.12). Plays WAV from LittleFS via I2S to MAX98357A. Controls amp shutdown pin.
- **button_handler** — Debounced GPIO input with edge detection. `wasPressed()` returns true once per press.
- **espnow_sender** — WiFi STA + ESP-NOW. Sends redundant packets with sequence numbers for reliability.
- **power_manager** — Controls brightness, amp shutdown, WiFi modem sleep.

### LED Panel Layout and Logo Masks

The front display is a 32x16 grid split across two physical 16x16 panels (front-left = strand 0, front-right = strand 1). The logo spans both panels with 4 color sections:

```
Front Left (strand 0, 16x16)          Front Right (strand 1, 16x16)
┌─────────────────────────────┐  ┌──────────────────┐
│ row 0-1: empty              │  │ row 0-1: empty    │
│ row 2-5, cols 1-15: ORANGE  │  │ row 2-5, col 0: O │ cols 1-7: YELLOW
│ row 6-9, cols 1-15: BLUE    │  │ row 6-9, col 0: B │ cols 1-7: YELLOW
│ row 10-13, cols 1-15: GREEN │  │ row 10-13, col 0:G│ cols 1-7: YELLOW
│ row 14-15: empty            │  │ row 14-15: empty  │
└─────────────────────────────┘  └──────────────────┘
```

Side panels (8x32 each): first 21 columns fully lit (168 pixels per side).

**Mask data in `led_manager.cpp`** — packed bitmask arrays (32 bytes each):
- `FRONT_LEFT_MASK`: 180 lit pixels (rows 2-13, cols 1-15)
- `FRONT_RIGHT_MASK`: 96 lit pixels (rows 2-13, col 0 + cols 1-7)
- `SIDE_LEFT_MASK` / `SIDE_RIGHT_MASK`: 168 lit pixels each (cols 0-20)

**Total logo LEDs: 612** (180 + 96 + 168 + 168)

### LED Panel Wiring

Column-major serpentine per panel. Even columns top-to-bottom, odd columns bottom-to-top.
`index = strandOffset + x * panelHeight + (x%2==0 ? y : panelHeight-1-y)`

NeoPXL8 strands: 0=front-left (GPIO 36), 1=front-right (GPIO 5), 2=side-left (GPIO 9), 3=side-right (GPIO 6). Strands 4-7 unused (physically routed on FeatherWing — do not use those GPIOs for other purposes).

### Logo Mask Encoding

Packed bitmask arrays (32 bytes per panel = 256 bits). Bit order: `byte[y * (width/8) + x/8]`, bit `(7 - x%8)`.

The `isMasked()` function checks if a pixel is part of the logo:
```cpp
uint16_t bitIndex = y * width + x;
uint16_t byteIndex = bitIndex / 8;
uint8_t  bitOffset = 7 - (bitIndex % 8);  // MSB first
return (data[byteIndex] >> bitOffset) & 1;
```

Source data extracted from `docs/Big Blue Button Front LED Mapping.xlsx`.

## Power Budget

Real measurements at warm white (255, 180, 80) — 612 logo LEDs only:

| Brightness | Current Draw |
|------------|-------------|
| 35         | ~3.5A       |
| 55         | ~5.1A       |
| 65         | ~4.2A (warm white) |
| 100        | ~6.0A (warm white) |

Warm white draws ~30% less than pure white (255, 255, 255) at the same brightness.

With 20Ah battery (2x 10Ah parallel): ~3.3 hours at brightness 100 warm white.

## Coding Conventions

- **Non-blocking loop:** Use `millis()` for timing in `loop()`. Never use `delay()` in `loop()` (short `delay()` in `setup()` is fine).
- **Pin definitions:** All in `config.h` with `static constexpr`.
- **NeoPXL8:** Call `show()` only after staging all frame changes.
- **Naming:** camelCase for variables/functions, PascalCase for classes. Prefer `const`/`constexpr`.
- **Libraries:** Declare in `platformio.ini` under `lib_deps`.
- **WDT safety:** Call `yield()` in long loops to avoid watchdog timer resets.
- **Credentials:** Store in gitignored files; never commit.
- **ESP32 ISRs:** Keep short, use `IRAM_ATTR`.
- **Storage:** Use `Preferences.h` for NVS, not `EEPROM.h`.

## GPIO Pin Map

| Function | GPIO | Notes |
|---|---|---|
| NeoPXL8 Out 0 | 36 | Front left 16x16 |
| NeoPXL8 Out 1 | 5 | Front right 16x16 |
| NeoPXL8 Out 2 | 9 | Side left 8x32 |
| NeoPXL8 Out 3 | 6 | Side right 8x32 |
| NeoPXL8 Out 4-7 | 13/12/11/10 | Unused — physically routed on FeatherWing, do not use |
| I2S BCLK | 18 | MAX98357A bit clock |
| I2S LRC | 17 | MAX98357A word select |
| I2S DOUT | 16 | MAX98357A data |
| AMP Shutdown | 14 | HIGH=on, LOW=off |
| Button | 15 | INPUT_PULLUP, wire to GND |
| Spare | 8 | Available (A5) |

## Known Issues & Build Notes

### ESP32-audioI2S version must be pinned to 3.0.12
The latest ESP32-audioI2S (v3.4.x) requires C++20 `<span>`, which is unavailable with GCC 8.4 (the toolchain shipped with `espressif32` platform 6.7.0 / Arduino Core 2.0.16). Adding `-std=gnu++2a` does not help because GCC 8.4's standard library lacks the `<span>` header entirely. Version 3.0.12 is the last release compatible with Arduino Core 2.0.x. Version 3.1.0+ requires Arduino Core 3.x.

### NeoPXL8 pulls SAMD-only dependencies
The NeoPXL8 `library.properties` declares dependencies for all supported platforms (SAMD, RP2040, ESP32). On ESP32-S3, several of these fail to compile: Adafruit Zero DMA Library, Adafruit ZeroTimer Library, Adafruit TinyUSB Library, FlashStorage, Adafruit InternalFlash, Adafruit CPFS. All are excluded via `lib_ignore` in `platformio.ini`. NeoPXL8 itself uses the ESP32-S3 LCD_CAM peripheral for DMA — no SAMD code is needed.

### QT Py board already defines ARDUINO_USB_CDC_ON_BOOT
The `adafruit_qtpy_esp32s3_nopsram` board definition sets `ARDUINO_USB_CDC_ON_BOOT=1`. Only `-DARDUINO_USB_MODE=0` needs to be added in `build_flags` to enable TinyUSB mode for USB HID. Do not re-define `ARDUINO_USB_CDC_ON_BOOT` or you get redefinition warnings on every compiled file.

### FTHRS3BOOT USB drive mounting
The Feather ESP32-S3 ships with TinyUF2 bootloader at address 0x2d0000 which causes the board to mount as a USB drive (FTHRS3BOOT). This is normal Adafruit behavior and does not affect firmware execution.

### Receiver MAC address not yet configured
`config.h` has `RECEIVER_MAC` set to broadcast (`FF:FF:FF:FF:FF:FF`). After flashing the QT Py, its MAC address is printed to serial on boot. Update `RECEIVER_MAC` in `config.h` with the actual address for unicast delivery.

### No WAV file in data/ yet
The `data/` directory exists but is empty. Place `press.wav` there and upload with `pio run -e feather_esp32s3 -t uploadfs` before testing audio.

## Resource Usage (as of initial build)

| Environment | RAM | Flash |
|---|---|---|
| feather_esp32s3 | 18.3% (60KB / 320KB) | 37.0% (1.2MB / 3.3MB) |
| qtpy_esp32s3 | 16.9% (55KB / 320KB) | 33.9% (711KB / 2.1MB) |
