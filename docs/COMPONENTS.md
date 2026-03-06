# Components — Big Blue Bus Button

## Component List

| # | Component | Quantity | Role |
|---|-----------|----------|------|
| 1 | Adafruit Feather ESP32-S3 (4MB Flash + 2MB PSRAM) | 1 | Main MCU |
| 2 | Adafruit NeoPXL8 FeatherWing | 1 | 8-channel parallel LED driver (stacked on Feather) |
| 3 | Adafruit QT Py ESP32-S3 (no PSRAM) | 1 | Wireless receiver → USB HID spacebar |
| 4 | Adafruit BQ24074 LiPo Charger | 1 | Charges battery from USB-C at up to 1.5A |
| 5 | TPS61088 Boost Converter (JESSINIE) | 3 | Boost 3.7V → 5V for LED power (10A each) |
| 6 | MAX98357A I2S Amplifier | 1 | Audio output from ESP32 |
| 7 | Speaker | 1 | Sound output |
| 8 | WS2812B LED Panel 16x16 | 2 | Front left + front right displays |
| 9 | WS2812B LED Panel 8x32 | 2 | Side left + side right displays |
| 10 | Momentary Push Button | 1 | Trigger input (NO, wired to GND) |
| 11 | LiPo Battery 3.7V | 1 | Main power source |
| 12 | Flat Ethernet Cable (AWM E212689) | 1 | Data + GND from NeoPXL8 to LED panels |

## Wiring Summary

### Power Chain

```
USB-C ──► BQ24074 ──► LiPo Battery ──┬──► Feather JST (regulates to 3.3V)
                                      ├──► TPS61088 #1 (5V) ──► Front Left + Front Right
                                      └──► TPS61088 #2 (5V) ──► Side Left + Side Right

TPS61088 #3 ── spare
```

### Feather ESP32-S3 Pinout

```
                    ┌──────────────────────────────────┐
                    │    Adafruit Feather ESP32-S3      │
                    │                                    │
  NeoPXL8 OUT 0 ◄──┤  GPIO 36 (SCK)    GPIO 18 (A0)  ├──► MAX98357A BCLK
  NeoPXL8 OUT 1 ◄──┤  GPIO 5  (D5)     GPIO 17 (A1)  ├──► MAX98357A LRC
  NeoPXL8 OUT 2 ◄──┤  GPIO 9  (D9)     GPIO 16 (A2)  ├──► MAX98357A DIN
  NeoPXL8 OUT 3 ◄──┤  GPIO 6  (D6)     GPIO 15 (A3)  ├──◄ Button (→ GND)
     (unused) ·····┤  GPIO 13 (D13)    GPIO 14 (A4)  ├──► MAX98357A SD
     (unused) ·····┤  GPIO 12 (D12)    3V3            ├──► MAX98357A VIN
     (unused) ·····┤  GPIO 11 (D11)    GND            ├──► MAX98357A GND
     (unused) ·····┤  GPIO 10 (D10)    USB-C          │
                    │                                    │
                    │         WiFi / ESP-NOW ─ ─ ─ ─ ─ ─ ─ ► QT Py
                    │         JST Battery ◄── LiPo 3.7V │
                    └──────────────────────────────────┘
```

### Ethernet Cable (NeoPXL8 Jack 1 → LED Panels)

Solid color = data, striped = paired GND. Verified by continuity test.

| Output | Data Wire | GND Wire | Panel | Panel Size |
|--------|-----------|----------|-------|------------|
| OUT 0  | Brown | Brown/white | Front Left | 16x16 |
| OUT 1  | Green | Green/white | Front Right | 16x16 |
| OUT 2  | Blue | Blue/white | Side Left | 8x32 |
| OUT 3  | Orange | Orange/white | Side Right | 8x32 |

### LED Panel Connections (each panel)

```
Ethernet solid color wire ──► DATA in
Ethernet striped wire ──┬──► GND
5V from TPS61088 ───────┤
                        └──► GND (common ground)
5V from TPS61088 ──────────► VIN
```

### MAX98357A I2S Amp

| Amp Pin | Source | GPIO |
|---------|--------|------|
| BCLK | Feather A0 | 18 |
| LRC | Feather A1 | 17 |
| DIN | Feather A2 | 16 |
| SD | Feather A4 | 14 |
| VIN | Feather 3V3 | — |
| GND | Feather GND | — |

### Button

Momentary NO switch. One leg → GPIO 15 (A3), other leg → GND. Internal pullup.

### ESP-NOW Wireless Link

Feather (TX) ─ ─ ─ ESP-NOW ─ ─ ─ QT Py (RX) ──USB-C──► PC (spacebar HID)

## Breadboard Test Setup

For bench testing without the battery/charger/boost:

```
Bench Supply (Hylec HY50-06A)
  Set to 5V, current limit ~6A
    │
    ├── 5V ──► Feather "USB" pin (powers MCU via onboard regulator)
    ├── 5V ──► LED panel VIN (each panel, separate wires)
    └── GND ──► Common ground (Feather GND + all panel GND)

Ethernet cable from NeoPXL8 Jack 1 ──► LED panel DATA + GND
```

Keep brightness low in firmware during bench testing — 6A limit on the supply.

## Ground Rules

All GND connections must be tied together:
- Feather GND
- BQ24074 GND
- TPS61088 GND (all converters)
- LED panel GND (all 4 panels)
- MAX98357A GND
- Ethernet cable GND wires
