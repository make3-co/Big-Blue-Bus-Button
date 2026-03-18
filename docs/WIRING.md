# Wiring Diagram — Big Blue Bus Button

## Power Distribution

```
  5V 15A 75W PSU
       │
    XT60 Jack
       │
       ▼
┌──────────────────────────────────────┐
│     DPDT Switch (ON-OFF-ON, 30A)     │
│                                      │
│  Pole A (PSU routing):               │
│    CTR-A ◄── XT60 (+)               │
│    1A ────► IP2312 (+in)   [CHARGE]  │
│    2A ────► Wago 3         [WALL]    │
│                                      │
│  Pole B (battery connect/isolate):   │
│    CTR-B ──► Wago 3 (system +)       │
│    1B ◄──── Wago 1 (batt +) [CHARGE]│
│    2B ────► nothing          [WALL]  │
└──────────────────────────────────────┘

Position 1 (CHARGE): PSU → IP2312 → battery, battery → system
Center    (OFF):     Everything disconnected
Position 2 (WALL):   PSU → system directly, battery isolated

                ┌──────────────┐
                │   IP2312     │
                │ 5V 3A Fast   │
                │ Charger      │
                └──────┬───────┘
                       │ (+out)
                       ▼
  Switch 1B ──► Wago 1 ──► 25A Fuse ──► BMS (+)
                                           │
                                           ▼
                                  8x Molicel 21700 P42A
                                  (1S8P, 33.6Ah, 3.7V)

  Wago 3 (system + bus)
      │
      ├──────────────┬──────────────┬──────────────┐
      ▼              ▼              ▼              ▼
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

  GND bus:
  PSU (-) ── XT60 (-) ──► Wago 2 ──► IP2312 (-)
                             │──────► BMS (-)
                             │
                           Wago 4 ──► All LEDs (-) + ESP32 (-)
```

8x Molicel P42A 21700 cells in parallel provide 33.6Ah at 3.7V with 360A max discharge. The BMS provides overcharge (4.28V), overdischarge (2.5V), overcurrent (30A), and short circuit protection. A 25A fuse provides additional safety. The IP2312 charges the pack at 3A (~11 hours from empty).

**Operating modes:**
- **Battery mode:** No PSU connected. Switch in position 1. Battery powers system through Wago 1 → switch 1B → CTR-B → Wago 3.
- **Charge mode:** PSU connected, switch in position 1. PSU charges battery via IP2312. Battery powers system. Firmware detects VBUS on GPIO 11 and kills LEDs so charger gets full 3A.
- **Wall mode:** PSU connected, switch in position 2. PSU powers LEDs/ESP32 directly at 5V via Wago 3. Battery isolated (switch 2B disconnected). TPS61088s receive 5V input (passthrough, no boost needed).
- **Off:** Switch in center position. All loads disconnected.

**LED power budget:** 612 logo LEDs (306 with checkerboard) × 60mA max. Measured draws: brightness 255 checkerboard = 12A (20A startup spike), brightness 60 all LEDs = 5.9A. Checkerboard at full brightness is the current operating mode — best brightness-to-power ratio behind diffuser panels.

## Feather ESP32-S3 Pin Connections

```
                    ┌──────────────────────────────────┐
                    │    Adafruit Feather ESP32-S3      │
                    │  4MB Flash + 2MB PSRAM + LC709203F│
                    │                                    │
  NeoPXL8 OUT 0 ◄──┤  GPIO 36 (SCK)    GPIO 18 (A0)  ├──► AMP Shutdown (SD)
  NeoPXL8 OUT 1 ◄──┤  GPIO 5  (D5)     GPIO 17 (A1)  ├──  (free)
  NeoPXL8 OUT 2 ◄──┤  GPIO 9  (D9)     GPIO 16 (A2)  ├──► MAX98357A DIN
  NeoPXL8 OUT 3 ◄──┤  GPIO 6  (D6)     GPIO 15 (A3)  ├──► I2S BCLK
  (FeatherWing) ····┤  GPIO 13 (D13)    GPIO 14 (A4)  ├──► I2S LRC
  (FeatherWing) ····┤  GPIO 12 (D12)    GPIO 8  (A5)  ├──◄ Volume Pot (ADC)
  VBUS Detect ◄─────┤  GPIO 11 (D11)*   3V3            ├──► MAX98357A VIN
  Button ◄──────────┤  GPIO 10 (D10)*   GND            ├──► MAX98357A GND
                    │                    USB-C          │
                    │         WiFi / ESP-NOW  ─ ─ ─ ─ ─ ─ ─ ─ ► Receiver
                    │         BAT pin ◄── Wago 3 (+)   │
                    └──────────────────────────────────┘
  * GPIO 10, 11: NeoPXL8 FeatherWing header pins cut
```

## Signal Flow

```
  5V 15A PSU ── XT60 ── DPDT Switch ──┬── pos 1: IP2312 → Battery
                                       ├── off:   disconnected
                                       └── pos 2: Wago 3 (direct 5V)

  Battery ── BMS ── Fuse ── Wago 1 ── Switch 1B ──┐
                               │                    │
                         IP2312 (+out)              │
                                          Switch CTR-B
                                                │
                                             Wago 3 (system + bus)
                                                │
                    ┌──────────┬──────────────┬──┴─────────┐
                    ▼          ▼              ▼            ▼
┌────────┐  ┌─────────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐
│ Button ├─►│   Feather   │ │ TPS61088  │ │ TPS61088  │ │ TPS61088  │
│Vol Pot ├─►│  ESP32-S3   │ │ #1 → 5V   │ │ #2 → 5V   │ │ #3 → 5V   │
│VBUS Det├─►│             │ │ Front L+R │ │ Side Left │ │ Side Right│
└────────┘  └──┬──────┬───┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘
               │      │          │              │              │
    GPIO 36/5/9/6  GPIO 15/14/16/18            │              │
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

NeoPXL8 FeatherWing stacks directly on the Feather — no external wiring needed. GPIOs 13/12 are physically routed on the FeatherWing. GPIOs 10/11 had their FeatherWing header pins cut to allow independent use (button and VBUS detect).

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
| +in | DPDT switch 1A (PSU +5V in charge mode) |
| -in | Wago 2 (GND bus) |
| +out | Wago 1 → 25A fuse → BMS (+) |
| -out | Wago 2 (GND bus) |

Charges the 21700 pack at up to 3A from external 5V 15A PSU via XT60 + DPDT switch. Full charge from empty: ~11 hours. In charge mode, firmware detects VBUS on GPIO 11 and shuts down LEDs to give charger full current.

## HXYP-1S-6033 BMS (30A)

| Pin | Connection |
|-----|------------|
| B+ / B- | Battery pack positive / negative |
| P+ / P- | Load output positive / negative (to 25A fuse) |

Provides overcharge (4.25V), overdischarge (2.5V), overcurrent (30A), and short circuit protection for the 21700 pack.

## TPS61088 Boost Converters (LED Power)

Three TPS61088 boost converters, each set to **5V** output. All powered from Wago 3 (system + bus). In battery mode, input is ~3.7V and converters boost to 5V. In wall mode, input is 5V and converters pass through. Connect all GND to Wago 4 (common ground).

| Converter | VIN | VOUT | Panels |
|-----------|-----|------|--------|
| TPS61088 #1 | Wago 3 | 5V | Front Left + Front Right (panels 0 & 1) |
| TPS61088 #2 | Wago 3 | 5V | Side Left (panel 2) |
| TPS61088 #3 | Wago 3 | 5V | Side Right (panel 3) |

Each converter pin connections:

| Pin | Connection |
|-----|------------|
| VIN | Wago 3 (system + bus) |
| GND | Wago 4 (common ground) |
| VOUT | 5V to assigned LED panel(s) |
| EN | Tie HIGH (always on) or connect to Feather GPIO for power control |

Max 10A continuous per converter. Splitting panels across converters reduces per-converter current draw and improves stability at low battery voltage.

## MAX98357A I2S Amp

| Amp Pin | Feather Pin | GPIO |
|---------|-------------|------|
| BCLK    | A3          | 15   |
| LRC     | A4          | 14   |
| DIN     | A2          | 16   |
| SD      | A0          | 18   |
| GAIN    | GND         | —    |
| VIN     | 3V3         | —    |
| GND     | GND         | —    |

SD pin controls amp shutdown: HIGH = on, LOW = off. GAIN tied to GND for maximum 15dB gain (floating = 9dB, VIN = 12dB). Speaker connects to amp output terminals.

## Button

| Signal   | Feather Pin | GPIO |
|----------|-------------|------|
| Button   | D10         | 10   |

Momentary normally-open switch. One leg to GPIO 10, other leg to GND. Uses internal INPUT_PULLUP. NeoPXL8 FeatherWing header pin cut. Connected via 3-pin JST connector (button / VBUS / GND).

## VBUS Charger Detection

| Signal   | Feather Pin | GPIO |
|----------|-------------|------|
| VBUS     | D11         | 11   |

100K/100K voltage divider from IP2312 5V input. HIGH when external PSU is connected and switch is in charge position. NeoPXL8 FeatherWing header pin cut. Connected via 3-pin JST connector (shared with button).

## Volume Control

| Signal   | Feather Pin | GPIO |
|----------|-------------|------|
| Pot wiper | A5         | 8    |

20K potentiometer. 3.3V and GND to outer pins, wiper to GPIO 8 (ADC). Connected via 3-pin JST connector. 8-sample averaging, read every 200ms.

## ESP-NOW Wireless Link

| Device  | Role        | Connection |
|---------|-------------|------------|
| Feather | Transmitter | WiFi STA mode, sends button press events |
| Feather (receiver) | Receiver | WiFi STA mode, receives events → USB HID spacebar |

Receiver connects to PC via USB-C. Receiver MAC: `DC:54:75:DD:45:74`.

## Wago Connector Map

| Wago | Connections | Purpose |
|------|------------|---------|
| Wago 1 | Switch 1B, IP2312 (+out), 25A fuse → BMS (+) | Battery positive bus |
| Wago 2 | PSU (-) via XT60, IP2312 (-), BMS (-), → Wago 4 | Ground bus (input side) |
| Wago 3 | Switch CTR-B, Switch 2A, → Feather BAT, → TPS61088 VIN (×3) | System positive bus |
| Wago 4 | ← Wago 2, → All LED GND, → ESP32 GND, → MAX98357A GND | Ground bus (load side) |

## Power Summary

| Rail | Source | Supplies | Notes |
|------|--------|----------|-------|
| 5V ext | 75W PSU via XT60 | DPDT switch input | Wall power or charging |
| 3.7V | 21700 pack → BMS → Fuse → Wago 1 | Wago 3 (via switch) | Battery voltage (3.0–4.2V) |
| 5V direct | PSU via switch pos 2 | Wago 3 → TPS61088 VIN (passthrough) | Wall power mode |
| 3.3V | Feather regulator | ESP32-S3, MAX98357A, NeoPXL8 logic | ~500mA max from Feather |
| 5V | TPS61088 boost (×3) | LED panels (split across 3 converters) | Up to 10A each, set VOUT=5V |

**Common ground** — All GND connections (PSU, Feather, IP2312, BMS, TPS61088s, LED panels, MAX98357A) share common ground through Wago 2 → Wago 4.

## Battery Pack

| Spec | Value |
|------|-------|
| Cells | 8× Molicel 21700 P42A |
| Configuration | 1S8P (all parallel) |
| Capacity | 33,600mAh (33.6Ah) |
| Nominal voltage | 3.7V |
| Max voltage | 4.2V |
| Min voltage | 2.5V (BMS cutoff 2.5V ±0.08V) |
| Max continuous discharge | 360A (45A per cell) |
| Assembly | Spot-welded nickel strips |
| Protection | HXYP-1S-6033 BMS (30A) + 25A fuse |
| Charging | IP2312 at 3A via external 5V 15A PSU |
| Firmware low-battery cutoff | 2.9V → LEDs off + ESP32 deep sleep |
| Measured runtime | ~2.5 hours (checkerboard, brightness 255, 4.24V → brownout) |
