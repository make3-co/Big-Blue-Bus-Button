# Components — Big Blue Bus Button

## Component List

| # | Component | Quantity | Role |
|---|-----------|----------|------|
| 1 | Adafruit Feather ESP32-S3 (4MB Flash + 2MB PSRAM) | 1 | Main MCU (built-in LC709203F fuel gauge) |
| 2 | Adafruit NeoPXL8 FeatherWing | 1 | 8-channel parallel LED driver (stacked on Feather) |
| 3 | Adafruit Feather ESP32-S3 (receiver) | 1 | Wireless receiver → USB HID spacebar |
| 4 | IP2312 5V 3A Lithium Battery Charging Board | 1 | Fast charges battery pack at 3A |
| 5 | HXYP-1S-6033 3.7V 30A BMS | 1 | Battery management (overcharge/overdischarge/overcurrent/short) |
| 6 | TPS61088 Boost Converter (JESSINIE) | 3 | Boost 3.7V → 5V for LED power (10A each) |
| 7 | MAX98357A I2S Amplifier | 1 | Audio output from ESP32 |
| 8 | Speaker | 1 | Sound output |
| 9 | WS2812B LED Panel 16x16 | 2 | Front left + front right displays |
| 10 | WS2812B LED Panel 8x32 | 2 | Side left + side right displays |
| 11 | Momentary Push Button | 1 | Trigger input (NO, wired to GND) |
| 12 | Molicel 21700 P42A 4200mAh 45A | 8 | Battery pack (1S8P, 33.6Ah, spot-welded) |
| 13 | 25A Fuse | 1 | Overcurrent protection between BMS and loads |
| 14 | Flat Ethernet Cable (AWM E212689) | 1 | Data + GND from NeoPXL8 to LED panels |
| 15 | 5V 15A 75W Power Supply | 1 | External wall power for charging or direct LED power |
| 16 | JD DPDT Rocker Switch (ON-OFF-ON, 30A) | 1 | Switches PSU between charge mode, off, and wall power mode |
| 17 | XT60H Connector Pair (M+F) | 2 | PSU input jack on enclosure + PSU cable end |
| 18 | Wago 221 Lever-Nut Connectors | 4 | Power distribution bus (see wiring diagram) |
| 19 | 20K Potentiometer | 1 | Volume control (ADC on GPIO 8) |

## Wiring Summary

### Power Chain

```
                         DPDT Switch (ON-OFF-ON)
                        ┌──────────────────────┐
5V 15A PSU ── XT60 ─────┤ CTR-A          CTR-B ├──── Wago 3 (system +)
                        │  1A: IP2312+in  1B ──├──── Wago 1 (battery +)
                        │  2A: Wago 3     2B ──├──── nothing
                        └──────────────────────┘

Position 1 (CHARGE): PSU → IP2312 → battery, battery → system
Center (OFF):        Everything disconnected
Position 2 (WALL):   PSU → LEDs/ESP32 directly, battery isolated

IP2312 (+out) ──► Wago 1 ──► 25A Fuse ──► BMS (+) ──► Battery
                    │
        (switch 1B in pos 1)
                    │
                  Wago 3 ──► System loads
                    │
    ┌───────────────┼───────────────┬──────────────┐
    ▼               ▼               ▼              ▼
Feather BAT    TPS61088 #1    TPS61088 #2    TPS61088 #3
(3.3V reg)     (5V) Front     (5V) Side L    (5V) Side R

GND bus (Wago 2): PSU(-) + IP2312(-) + BMS(-)
                         │
                       Wago 4 ──► All LEDs(-) + ESP32(-)
```

### Feather ESP32-S3 Pinout

```
                    ┌──────────────────────────────────┐
                    │    Adafruit Feather ESP32-S3      │
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
                    │         WiFi / ESP-NOW ─ ─ ─ ─ ─ ─ ─ ► Receiver
                    │         BAT pin ◄── Wago 3 (+)   │
                    └──────────────────────────────────┘
  * GPIO 10, 11: NeoPXL8 FeatherWing header pins cut
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
| BCLK | Feather A3 | 15 |
| LRC | Feather A4 | 14 |
| DIN | Feather A2 | 16 |
| SD | Feather A0 | 18 |
| GAIN | GND (15dB) | — |
| VIN | Feather 3V3 | — |
| GND | Feather GND | — |

### Button

Momentary NO switch. One leg → GPIO 10 (D10), other leg → GND. Internal pullup. NeoPXL8 FeatherWing header pin cut for this GPIO.

### VBUS Charger Detection

GPIO 11 (D11) via 100K/100K voltage divider from IP2312 5V input. HIGH when charger connected. NeoPXL8 FeatherWing header pin cut for this GPIO.

### Volume Potentiometer

20K pot wiper → GPIO 8 (A5). 3.3V and GND to pot outer pins. 3-pin JST connector.

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
- IP2312 GND
- BMS GND
- TPS61088 GND (all 3 converters)
- LED panel GND (all 4 panels)
- MAX98357A GND
- Ethernet cable GND wires
