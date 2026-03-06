# Wiring Diagram — Big Blue Bus Button

## Power Distribution

```
  USB-C
    │
    ▼
┌──────────────────┐         ┌───────────────────────┐
│ Adafruit BQ24074 │         │     LiPo 3.7V          │
│ LiPo Charger     │────────►│     Battery             │
│ (up to 1.5A)     │ CHARGE  └───────┬───────┬─────────┘
└──────────────────┘                 │       │
                                     │       │
                       ┌─────────────┘       └──────────────┐
                       ▼                                     ▼
             ┌──────────────────┐                  ┌─────────────────┐
             │ Feather ESP32-S3 │                  │ TPS61088 Boost  │
             │   (JST input)    │                  │ 3.7V → 5V, 10A  │
             │ Regulates to 3V3 │                  └────────┬────────┘
             └────────┬─────────┘                           │ 5V
                      │ 3V3                     ┌───────────┼───────────┐
                      ▼                         ▼           ▼           ▼
             ┌─────────────────┐          ┌──────────┐ ┌──────────┐   ...
             │  MAX98357A Amp  │          │ LED Panel│ │ LED Panel│  (all 4)
             │  (VIN from 3V3) │          │  5V + GND│ │  5V + GND│
             └─────────────────┘          └──────────┘ └──────────┘
```

USB-C plugs into the BQ24074, which charges the LiPo at up to 1.5A. The LiPo connects to **both** the Feather JST input (for MCU/amp power) and the TPS61088 boost converter input (for LED power). WS2812B LEDs require 5V — the boost converter steps 3.7V up to 5V and can supply up to 10A. The system can run while charging.

**LED power budget:** 1024 LEDs x 60mA max = 61A at full white. At `BRIGHTNESS_ACTIVE = 180` (~70%), peak draw is ~43A. At `BRIGHTNESS_IDLE_MAX = 60` (~24%), idle draw is ~15A. Actual draw depends on color/pattern — logo masks reduce lit pixels significantly.

## Feather ESP32-S3 Pin Connections

```
                    ┌──────────────────────────────────┐
                    │    Adafruit Feather ESP32-S3      │
                    │      4MB Flash + 2MB PSRAM        │
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
                    │         WiFi / ESP-NOW  ─ ─ ─ ─ ─ ─ ─ ─ ► QT Py
                    │         JST Battery ◄── LiPo 3.7V │
                    └──────────────────────────────────┘
```

## Signal Flow

```
              USB-C
                │
           ┌────┴─────┐
           │ BQ24074  │        LiPo 3.7V
           │ Charger  ├──────►(battery)
           └──────────┘        │      │
                               │      └─────────────────────────┐
                               ▼                                 ▼
┌────────┐ GPIO 15 ┌─────────────┐  ESP-NOW  ┌──────────┐  ┌───────────┐
│ Button ├────────►│   Feather   ├─ ─ ─ ─ ─►│  QT Py   │  │ TPS61088  │
└────────┘         │  ESP32-S3   │ (wireless)│ ESP32-S3 │  │ 3.7V → 5V │
                   └──┬──────┬───┘           └─────┬────┘  └─────┬─────┘
                      │      │                     │          5V  │
          GPIO 36/5/9/6    GPIO 18/17/16/14        │              │
                      │      │                USB HID      ┌──────┴──────┐
                      ▼      ▼                     │       │  5V + GND   │
                ┌─────────┐ ┌───────────┐    ┌─────┴──┐   │  to all 4   │
                │NeoPXL8  │ │ MAX98357A │───►│Speaker │   │  LED panels │
                │FeatherWg│ │  I2S Amp  │    └────────┘   └──────┬──────┘
                └────┬────┘ └───────────┘                        │
                     │ data                                      │ power
          ┌──────────┼──────────┬──────────┐   ┌────────────────┘
          ▼          ▼          ▼          ▼   ▼
     ┌─────────┐┌─────────┐┌────────┐┌─────────┐
     │Front L. ││Front R. ││Side L. ││Side R.  │
     │ 16x16   ││ 16x16   ││ 8x32   ││ 8x32   │
     └─────────┘└─────────┘└────────┘└─────────┘
```

## NeoPXL8 FeatherWing → LED Panels

| Output | GPIO | Panel       | Size  | LEDs |
|--------|------|-------------|-------|------|
| OUT 0  | 36   | Front Left  | 16x16 | 256  |
| OUT 1  | 5    | Front Right | 16x16 | 256  |
| OUT 2  | 9    | Side Left   | 8x32  | 256  |
| OUT 3  | 6    | Side Right  | 8x32  | 256  |
| OUT 4  | 13   | unused      | —     | —    |
| OUT 5  | 12   | unused      | —     | —    |
| OUT 6  | 11   | unused      | —     | —    |
| OUT 7  | 10   | unused      | —     | —    |

NeoPXL8 FeatherWing stacks directly on the Feather — no external wiring needed. GPIOs 13/12/11/10 are physically routed on the FeatherWing; do not use for other purposes.

### Ethernet Cable Mapping (Jack 1 → LED Panels)

Verified by continuity test. Solid color = data, striped = GND. Each twisted pair carries one output + its ground.

| Output | Data Wire | GND Wire | Panel |
|--------|-----------|----------|-------|
| OUT 0  | Brown | Brown/white | Front Left |
| OUT 1  | Green | Green/white | Front Right |
| OUT 2  | Blue | Blue/white | Side Left |
| OUT 3  | Orange | Orange/white | Side Right |

### Per-Panel Connections

Each LED panel needs **3 connections**:
- **Data** — solid color wire from Ethernet cable (NeoPXL8 output)
- **5V** — from TPS61088 boost converter (separate power wire)
- **GND** — striped wire from Ethernet + common ground to 5V supply

## Adafruit BQ24074 LiPo Charger

| Pin | Connection |
|-----|------------|
| USB | USB-C power input (5V) |
| VBAT | LiPo battery +/- |
| CE | Tie LOW (charge enable) or connect to GPIO for charge control |
| EN1/EN2 | Set charge rate (both HIGH = 1.5A, EN1 HIGH + EN2 LOW = 500mA) |
| PG | Power good output (optional — monitor via GPIO) |
| STAT1/STAT2 | Charge status LEDs (optional) |

Charges the LiPo from USB-C at up to 1.5A. The BQ24074 handles charge management independently — the Feather's built-in charger is bypassed since we're feeding the battery directly from the BQ24074.

## TPS61088 Boost Converter (LED Power)

| Pin | Connection |
|-----|------------|
| VIN | LiPo 3.7V (direct from battery) |
| GND | Common ground (shared with Feather) |
| VOUT | 5V to all LED panel VIN |
| EN | Tie HIGH (always on) or connect to Feather GPIO for power control |

Set output to **5V**. Max 10A continuous. Connect GND to Feather GND to ensure common ground reference for data signals.

## MAX98357A I2S Amp

| Amp Pin | Feather Pin | GPIO |
|---------|-------------|------|
| BCLK    | A0          | 18   |
| LRC     | A1          | 17   |
| DIN     | A2          | 16   |
| SD      | A4          | 14   |
| VIN     | 3V3         | —    |
| GND     | GND         | —    |

SD pin controls amp shutdown: HIGH = on, LOW = off. Speaker connects to amp output terminals.

## Button

| Signal   | Feather Pin | GPIO |
|----------|-------------|------|
| Button   | A3          | 15   |

Momentary normally-open switch. One leg to GPIO 15, other leg to GND. Uses internal INPUT_PULLUP.

## ESP-NOW Wireless Link

| Device  | Role        | Connection |
|---------|-------------|------------|
| Feather | Transmitter | WiFi STA mode, sends button press events |
| QT Py   | Receiver    | WiFi STA mode, receives events → USB HID spacebar |

QT Py connects to PC via USB-C. Receiver MAC in `config.h` currently set to broadcast — update after flashing QT Py.

## Power Summary

| Rail | Source | Supplies | Notes |
|------|--------|----------|-------|
| 5V USB | USB-C | BQ24074 charger input | Charging only |
| 3.7V | LiPo battery | Feather JST, TPS61088 VIN | Raw battery voltage |
| 3.3V | Feather regulator | ESP32-S3, MAX98357A, NeoPXL8 logic | ~500mA max from Feather |
| 5V | TPS61088 boost | All 4 LED panels (WS2812B VIN) | Up to 10A, set VOUT=5V |

**Common ground** — All GND connections (Feather, BQ24074, TPS61088, LED panels, MAX98357A) must share a common ground.
