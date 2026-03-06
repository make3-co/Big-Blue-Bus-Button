# Big Blue Bus Button — Firmware Design

## Overview

Event button that backlights logo panels, plays a sound, and wirelessly advances a PowerPoint presentation. Battery-powered, client-operated, OTA-updatable.

## Boot Flow

```
Power on
    │
    ├─ Button NOT held → Startup animation (gradual ramp) → IDLE (warm white glow)
    │
    └─ Button held → Try saved WiFi
                        ├─ Connected → Check firmware URL → Update if newer → Reboot
                        └─ Failed/none → Start AP + captive portal
                                         → Client picks WiFi → Save creds
                                         → Check firmware URL → Reboot
```

## State Machine (Normal Mode)

```
STARTUP (gradual brightness ramp)
    │
    ▼
IDLE (warm white glow, battery indicator updates) ◄─── cooldown expires
    │                                                        │
    button press                                             │
    │                                                        │
    ▼                                                        │
ANIMATING (flash + bottom-to-top fill + sound + ESP-NOW) ───►
    │                                                 COOLDOWN
    animation + sound complete ──────────────────────►(button locked)
```

Button presses are ignored during ANIMATING and COOLDOWN states.

## Modules

### 1. Startup Animation

- Ramp brightness from 0 to operating level over ~1.5s
- Light panels sequentially (e.g., front-left → front-right → sides) to limit inrush current
- Prevents brown-out from 612 LEDs hitting 5V rail simultaneously
- Configurable: `STARTUP_RAMP_DURATION_MS` in config.h

### 2. Idle Glow

- All 612 logo pixels at warm white (255, 180, 80)
- Static — max efficient brightness, not a pulsing effect
- Brightness level set in config.h

### 3. Button Press Animation

- Brief full-white flash (~100ms)
- Bottom-to-top fill across all panels (~1s)
- Designed as a swappable function — isolated from state machine logic
- Color TBD (warm white for now)
- Duration configurable: `ANIMATION_DURATION_MS`

### 4. Sound

- Play WAV from LittleFS on button press via MAX98357A I2S amp
- Sound and animation must both complete before button re-enables
- Placeholder file until real sound provided
- Amp shutdown pin (GPIO 14) managed automatically — on during playback, off otherwise

### 5. Battery Indicator

- 4 WS2812B LEDs daisy-chained after a side panel
- Configurable panel in config.h: `BATTERY_INDICATOR_PANEL`
- Pixel indices: strand offset + 168 through + 171
- Battery voltage read via ADC (Feather A13 / GPIO 35, built-in voltage divider)
- Update interval: ~30 seconds
- Color scheme:
  - 100–75%: 4 green LEDs
  - 75–50%: 3 green LEDs
  - 50–25%: 2 green LEDs
  - 25–15%: 1 yellow LED
  - Below 15%: 1 red LED (blinking)

### 6. ESP-NOW

- Fixed WiFi channel for both sender and receiver: `ESPNOW_CHANNEL` in config.h
- Both devices call `esp_wifi_set_channel()` on boot
- Sender (Feather): sends BUTTON_PRESS command with redundant packets (3x)
- Receiver (any ESP32-S3 for testing, QT Py for production): receives command → USB HID spacebar
- Receiver MAC configurable in config.h — broadcast until configured
- ESP-NOW is only active in normal mode, not during OTA mode

### 7. OTA Firmware Update

**Trigger:** Hold button during power-on.

**WiFi provisioning:**
- Saved credentials stored in NVS via Preferences library
- On first use (no saved creds) or if saved WiFi fails: start SoftAP ("BigBlueButton-Setup")
- Captive portal serves HTML page — scan networks, pick one, enter password
- Save credentials to NVS for future use

**Firmware check:**
- HTTP GET to configured URL (e.g., GitHub release or simple web server)
- JSON manifest: `{ "version": "1.2.0", "url": "https://...firmware.bin" }`
- Compare against compiled-in `FIRMWARE_VERSION` string
- If newer: download to secondary OTA partition → verify checksum → set boot partition → reboot
- If verification fails: stay on current partition (ESP32 built-in rollback)
- Battery indicator LEDs show download progress during update

**Partition table:**
- Switch from `default_8MB.csv` to OTA-capable layout with two app slots
- Approximate layout: 2x ~3MB app partitions + NVS + LittleFS

**Channel management:**
- Normal mode: fixed channel (e.g., 1) — ESP-NOW works
- OTA mode: connects to AP (channel assigned by AP) — ESP-NOW not needed
- One reboot separates modes — no runtime channel juggling

### 8. Configuration (config.h additions)

```
ESPNOW_CHANNEL           — fixed WiFi channel for ESP-NOW (both devices)
BATTERY_INDICATOR_PANEL   — which side panel has the 4 indicator LEDs
BATTERY_READ_INTERVAL_MS  — how often to read battery voltage
FIRMWARE_VERSION          — compiled-in version string for OTA comparison
FIRMWARE_UPDATE_URL       — URL to check for firmware manifest
STARTUP_RAMP_DURATION_MS  — duration of startup brightness ramp
IDLE_COLOR                — warm white color value (255, 180, 80)
```

## Not In Scope

- No BLE
- No multiple sound files or sound selection
- No animation editor
- No remote control beyond OTA
- No sleep mode (device always on when powered)
- No deep battery management (just voltage reading for indicator)

## Hardware Notes

- Receiver: any ESP32-S3 for testing, Adafruit QT Py ESP32-S3 for production
- Battery: 2x 10,000mAh LiPo in parallel (20Ah total)
- Charging: BQ24074 at 1.5A via USB-C
- LED power: TPS61088 boost 3.7V → 5V
- At warm white brightness 100: ~6A draw, ~3.3 hours runtime on 20Ah
