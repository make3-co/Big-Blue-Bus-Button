# Wiring Diagram — Big Blue Bus Button

## Power Distribution

```
  USB-C (5V)
    │
    ▼
┌──────────────┐
│   IP2312     │
│ 5V 3A Fast   │
│ Charger      │
└──────┬───────┘
       │
       ▼
┌──────────────────────────────────────────┐
│  8x Molicel 21700 P42A (parallel)        │
│  33,600mAh (33.6Ah), 3.7V nominal       │
│  360A max discharge, spot-welded         │
└──────────────────┬───────────────────────┘
                   │
                   ▼
          ┌────────────────┐
          │ HXYP-1S-6033   │
          │ BMS (30A)      │
          └───────┬────────┘
                  │
                  ▼
           ┌────────────┐
           │  25A Fuse   │
           └──────┬─────┘
                  │
      ┌───────────┼──────────────┬──────────────┐
      ▼           ▼              ▼              ▼
┌───────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│ Feather   │ │TPS61088  │ │TPS61088  │ │TPS61088  │
│ ESP32-S3  │ │#1 → 5V   │ │#2 → 5V   │ │#3 → 5V   │
│ (BAT pin) │ │Front L+R │ │Side Left │ │Side Right│
└─────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘
      │ 3V3        │ 5V         │ 5V         │ 5V
      ▼            ▼            ▼            ▼
┌───────────┐ ┌─────────┐ ┌────────┐ ┌─────────┐
│MAX98357A  │ │Front L. │ │Side L. │ │Side R.  │
│I2S Amp    │ │Front R. │ │ 8x32   │ │ 8x32   │
└───────────┘ └─────────┘ └────────┘ └─────────┘
```

8x Molicel P42A 21700 cells in parallel provide 33.6Ah at 3.7V with 360A max discharge. The BMS provides overcharge, overdischarge, overcurrent, and short protection at 30A. A 25A fuse provides additional safety. The IP2312 charges the pack at 3A from USB-C (~11 hours from empty). The system can run while charging.

**LED power budget:** 612 logo LEDs × 60mA max = ~37A at full white. At `BRIGHTNESS_ACTIVE = 60` (~24%), measured draw is ~4.6A at 5V. At 3.7V input, total input current is ~7A across all three TPS61088s. Well within the BMS 30A and fuse 25A limits.

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
                    │         WiFi / ESP-NOW  ─ ─ ─ ─ ─ ─ ─ ─ ► Receiver
                    │         BAT pin ◄── BMS → Fuse │
                    └──────────────────────────────────┘
```

## Signal Flow

```
              USB-C (5V)
                │
           ┌────┴─────┐
           │  IP2312   │     8x Molicel P42A
           │ Charger   ├───►(21700 parallel)
           └──────────┘        │
                          ┌────┴────┐
                          │  BMS    │
                          │  30A    │
                          └────┬────┘
                          ┌────┴────┐
                          │ 25A Fuse│
                          └────┬────┘
                    ┌──────────┼──────────────┬──────────────┐
                    ▼          ▼              ▼              ▼
┌────────┐  ┌─────────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐
│ Button ├─►│   Feather   │ │ TPS61088  │ │ TPS61088  │ │ TPS61088  │
└────────┘  │  ESP32-S3   │ │ #1 → 5V   │ │ #2 → 5V   │ │ #3 → 5V   │
            └──┬──────┬───┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘
               │      │          │              │              │
   GPIO 36/5/9/6  GPIO 18/17/16/14             │              │
               │      │          │              │              │
               ▼      ▼          ▼              ▼              ▼
         ┌─────────┐ ┌───────┐ ┌─────────┐ ┌────────┐ ┌─────────┐
         │NeoPXL8  │ │MAX amp│ │Front L. │ │Side L. │ │Side R.  │
         │FeatherWg│ │►Spkr  │ │Front R. │ │ 8x32   │ │ 8x32   │
         └────┬────┘ └───────┘ │ 16x16   │ └────────┘ └─────────┘
              │ data           └─────────┘
   ┌──────────┼──────────┬──────────┐
   ▼          ▼          ▼          ▼
 Front L.  Front R.  Side L.   Side R.

Feather ─ ─ ESP-NOW ─ ─ ► Receiver ──USB──► PC (spacebar HID)
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

## IP2312 Fast Charger

| Pin | Connection |
|-----|------------|
| USB-C | 5V power input |
| BAT+ | Battery pack positive (through BMS) |
| BAT- | Battery pack negative (through BMS) |

Charges the 21700 pack at up to 3A from USB-C. Full charge from empty: ~11 hours. The Feather's built-in charger is bypassed since battery power goes directly to the BAT pin.

## HXYP-1S-6033 BMS (30A)

| Pin | Connection |
|-----|------------|
| B+ / B- | Battery pack positive / negative |
| P+ / P- | Load output positive / negative (to 25A fuse) |

Provides overcharge (4.25V), overdischarge (2.5V), overcurrent (30A), and short circuit protection for the 21700 pack.

## TPS61088 Boost Converters (LED Power)

Three TPS61088 boost converters, each set to **5V** output. All powered from BMS output through 25A fuse. Connect all GND to common ground.

| Converter | VIN | VOUT | Panels |
|-----------|-----|------|--------|
| TPS61088 #1 | BMS → Fuse | 5V | Front Left + Front Right (panels 0 & 1) |
| TPS61088 #2 | BMS → Fuse | 5V | Side Left (panel 2) |
| TPS61088 #3 | BMS → Fuse | 5V | Side Right (panel 3) |

Each converter pin connections:

| Pin | Connection |
|-----|------------|
| VIN | BMS output (through 25A fuse) |
| GND | Common ground (shared with Feather) |
| VOUT | 5V to assigned LED panel(s) |
| EN | Tie HIGH (always on) or connect to Feather GPIO for power control |

Max 10A continuous per converter. Splitting panels across converters reduces per-converter current draw and improves stability at low battery voltage.

## MAX98357A I2S Amp

| Amp Pin | Feather Pin | GPIO |
|---------|-------------|------|
| BCLK    | A0          | 18   |
| LRC     | A1          | 17   |
| DIN     | A2          | 16   |
| SD      | A4          | 14   |
| GAIN    | GND         | —    |
| VIN     | 3V3         | —    |
| GND     | GND         | —    |

SD pin controls amp shutdown: HIGH = on, LOW = off. GAIN tied to GND for maximum 15dB gain (floating = 9dB, VIN = 12dB). Speaker connects to amp output terminals.

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
| 5V USB | USB-C | IP2312 charger input | Charging only (3A) |
| 3.7V | 21700 pack → BMS → Fuse | Feather BAT, TPS61088 VIN (×3) | Raw battery voltage (3.0–4.2V) |
| 3.3V | Feather regulator | ESP32-S3, MAX98357A, NeoPXL8 logic | ~500mA max from Feather |
| 5V | TPS61088 boost (×3) | LED panels (split across 3 converters) | Up to 10A each, set VOUT=5V |

**Common ground** — All GND connections (Feather, IP2312, BMS, TPS61088s, LED panels, MAX98357A) must share a common ground.

## Battery Pack

| Spec | Value |
|------|-------|
| Cells | 8× Molicel 21700 P42A |
| Configuration | 1S8P (all parallel) |
| Capacity | 33,600mAh (33.6Ah) |
| Nominal voltage | 3.7V |
| Max voltage | 4.2V |
| Min voltage | 3.0V (BMS cutoff ~2.5V) |
| Max continuous discharge | 360A (45A per cell) |
| Assembly | Spot-welded nickel strips |
| Protection | HXYP-1S-6033 BMS (30A) + 25A fuse |
| Charging | IP2312 at 3A via USB-C |
| Estimated runtime | ~4.8 hours at idle (brightness 60, ~7A total draw) |
