# Big Blue Bus Button — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete firmware for battery-powered event button with LED animations, sound, ESP-NOW, battery indicator, and OTA updates.

**Architecture:** Modular single-threaded state machine on ESP32-S3. Each feature is an independent module with `begin()`/`update()` pattern called from `loop()`. OTA mode is a separate boot path triggered by holding the button on power-on.

**Tech Stack:** PlatformIO, Arduino framework, Adafruit NeoPXL8, ESP32-audioI2S v3.0.12, ESP-NOW, ESP32 OTA APIs, WebServer for captive portal.

**Note:** This is an embedded project with no unit test framework. Verification is: build succeeds (`pio run`) + flash and observe on hardware. Each task lists expected observable behavior.

---

### Task 1: Update config.h with new constants

**Files:**
- Modify: `src/config.h`

**Step 1: Add new configuration constants**

Add these blocks to `src/config.h`:

```cpp
// =============================================================================
// Colors
// =============================================================================

static constexpr uint8_t IDLE_COLOR_R = 255;
static constexpr uint8_t IDLE_COLOR_G = 180;
static constexpr uint8_t IDLE_COLOR_B = 80;

// =============================================================================
// Startup
// =============================================================================

static constexpr uint32_t STARTUP_RAMP_DURATION_MS = 1500;

// =============================================================================
// Battery Indicator
// =============================================================================

static constexpr PanelId  BATTERY_INDICATOR_PANEL = PANEL_SIDE_LEFT;  // Change to PANEL_SIDE_RIGHT if needed
static constexpr uint16_t BATTERY_INDICATOR_OFFSET = 168;             // First LED after last lit side pixel
static constexpr uint8_t  BATTERY_INDICATOR_COUNT  = 4;
static constexpr uint32_t BATTERY_READ_INTERVAL_MS = 30000;           // 30 seconds
static constexpr uint8_t  BATTERY_ADC_PIN = 35;                       // A13, Feather built-in voltage divider

// =============================================================================
// ESP-NOW Channel
// =============================================================================

static constexpr uint8_t ESPNOW_CHANNEL = 1;  // Both sender and receiver must match

// =============================================================================
// OTA / WiFi
// =============================================================================

static constexpr const char* FIRMWARE_VERSION    = "1.0.0";
static constexpr const char* FIRMWARE_UPDATE_URL = "https://example.com/firmware.json";  // UPDATE THIS
static constexpr const char* AP_SSID             = "BigBlueButton-Setup";
static constexpr const char* AP_PASSWORD         = "";                                   // Open network
static constexpr uint32_t   WIFI_CONNECT_TIMEOUT_MS = 15000;
static constexpr uint32_t   OTA_CHECK_TIMEOUT_MS    = 30000;

// =============================================================================
// Boot Mode Detection
// =============================================================================

static constexpr uint32_t BOOT_BUTTON_HOLD_MS = 2000;  // Hold button this long during boot for OTA mode
```

**Step 2: Update existing timing constants**

Change `ANIMATION_DURATION_MS` and remove the idle pulse constants that are no longer needed:

```cpp
// Replace:
static constexpr uint32_t IDLE_PULSE_PERIOD_MS  = 3000;
// With:
// (delete — no longer pulsing)

// Keep these as-is:
static constexpr uint32_t ANIMATION_DURATION_MS = 1200;   // Flash (200ms) + fill (1000ms)
static constexpr uint32_t COOLDOWN_DURATION_MS  = 500;    // Short cooldown after animation
static constexpr uint32_t BUTTON_DEBOUNCE_MS    = 50;
```

**Step 3: Build to verify**

Run: `pio run -e feather_esp32s3`
Expected: Build succeeds with no errors.

**Step 4: Commit**

```bash
git add src/config.h
git commit -m "config: add constants for battery, OTA, startup, idle color, ESP-NOW channel"
```

---

### Task 2: Update idle glow to static warm white

**Files:**
- Modify: `src/animation.cpp`
- Modify: `src/animation.h`

**Step 1: Replace idle pulse with static glow**

In `src/animation.h`, rename `IDLE_PULSE` to `IDLE_GLOW`:

```cpp
enum class AnimationType : uint8_t {
    NONE,
    IDLE_GLOW,        // Static warm white on masked pixels
    BUTTON_PRESS,     // Flash + bottom-to-top fill
};
```

Rename method in class:
```cpp
    void startIdleGlow();    // was startIdlePulse()
```

Rename private method:
```cpp
    void renderIdleGlow();   // was renderIdlePulse()
```

**Step 2: Update animation.cpp**

Replace `renderIdlePulse()` with `renderIdleGlow()`:

```cpp
void AnimationManager::startIdleGlow() {
    current = AnimationType::IDLE_GLOW;
    startTime = millis();
    complete = false;
}

void AnimationManager::renderIdleGlow() {
    uint32_t color = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);
    ledManager.clear();
    for (uint8_t p = 0; p < PANEL_COUNT; p++) {
        ledManager.setMaskedColor(static_cast<PanelId>(p), color);
    }
    ledManager.show();
}
```

Update the `update()` switch case to use `IDLE_GLOW` and `renderIdleGlow()`.

**Step 3: Update main.cpp references**

Replace all `startIdlePulse()` calls with `startIdleGlow()`.

**Step 4: Build and flash**

Run: `pio run -e feather_esp32s3`
Expected: Build succeeds. On hardware: logo lights solid warm white, no pulsing.

**Step 5: Commit**

```bash
git add src/animation.h src/animation.cpp src/main.cpp
git commit -m "animation: replace idle pulse with static warm white glow"
```

---

### Task 3: Add startup animation

**Files:**
- Modify: `src/animation.h`
- Modify: `src/animation.cpp`
- Modify: `src/main.cpp`

**Step 1: Add STARTUP animation type**

In `animation.h`, add to the enum:

```cpp
enum class AnimationType : uint8_t {
    NONE,
    STARTUP,          // Gradual ramp-up to limit inrush current
    IDLE_GLOW,
    BUTTON_PRESS,
};
```

Add to class:
```cpp
    void startStartup();
private:
    void renderStartup();
```

**Step 2: Implement startup animation**

In `animation.cpp`:

```cpp
void AnimationManager::startStartup() {
    current = AnimationType::STARTUP;
    startTime = millis();
    complete = false;
}

void AnimationManager::renderStartup() {
    uint32_t elapsed = millis() - startTime;

    if (elapsed >= STARTUP_RAMP_DURATION_MS) {
        complete = true;
        return;
    }

    float progress = (float)elapsed / (float)STARTUP_RAMP_DURATION_MS;  // 0.0 → 1.0
    uint32_t color = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);

    ledManager.clear();

    // Ramp panels sequentially: front-left, front-right, side-left, side-right
    // Each panel gets 1/4 of the total duration to fade in
    for (uint8_t p = 0; p < PANEL_COUNT; p++) {
        float panelStart = (float)p / (float)PANEL_COUNT;
        float panelEnd   = (float)(p + 1) / (float)PANEL_COUNT;

        if (progress < panelStart) continue;  // Panel not started yet

        float panelProgress = (progress - panelStart) / (panelEnd - panelStart);
        if (panelProgress > 1.0f) panelProgress = 1.0f;

        ledManager.setMaskedColorScaled(static_cast<PanelId>(p), color, panelProgress);
    }

    ledManager.show();
}
```

Add `STARTUP` case to `update()` switch.

**Step 3: Add STARTUP state to main.cpp**

Add `STARTUP` to the `DeviceState` enum. In `setup()`, call `changeState(DeviceState::STARTUP)` instead of `IDLE`. In `changeState()`:

```cpp
case DeviceState::STARTUP:
    Serial.println("State: STARTUP");
    ledManager.setBrightness(BRIGHTNESS_IDLE_MAX);
    animationManager.startStartup();
    break;
```

In the loop switch, add:

```cpp
case DeviceState::STARTUP:
    if (animationManager.isComplete()) {
        changeState(DeviceState::IDLE);
    }
    break;
```

**Step 4: Build and flash**

Run: `pio run -e feather_esp32s3 -t upload`
Expected: On boot, panels light up one by one over ~1.5s, then hold steady warm white.

**Step 5: Commit**

```bash
git add src/animation.h src/animation.cpp src/main.cpp
git commit -m "animation: add startup ramp to limit inrush current"
```

---

### Task 4: New button press animation (flash + bottom-to-top fill)

**Files:**
- Modify: `src/animation.cpp`

**Step 1: Replace renderButtonPress()**

```cpp
void AnimationManager::renderButtonPress() {
    uint32_t elapsed = millis() - startTime;

    if (elapsed >= ANIMATION_DURATION_MS) {
        complete = true;
        return;
    }

    uint32_t color = Adafruit_NeoPixel::Color(IDLE_COLOR_R, IDLE_COLOR_G, IDLE_COLOR_B);
    ledManager.clear();

    // Phase 1: Flash (first 200ms) — all logo pixels full white
    if (elapsed < 200) {
        uint32_t white = Adafruit_NeoPixel::Color(255, 255, 255);
        for (uint8_t p = 0; p < PANEL_COUNT; p++) {
            ledManager.setMaskedColor(static_cast<PanelId>(p), white);
        }
        ledManager.show();
        return;
    }

    // Phase 2: Bottom-to-top fill (200ms to end)
    float fillProgress = (float)(elapsed - 200) / (float)(ANIMATION_DURATION_MS - 200);

    for (uint8_t p = 0; p < PANEL_COUNT; p++) {
        PanelId panel = static_cast<PanelId>(p);
        uint8_t w = ledManager.getPanelWidth(panel);
        uint8_t h = ledManager.getPanelHeight(panel);

        // fillRow: the row up to which we've filled (from bottom)
        uint8_t fillRow = (uint8_t)(fillProgress * h);

        for (uint8_t x = 0; x < w; x++) {
            for (uint8_t y = 0; y < h; y++) {
                if (!ledManager.isMasked(panel, x, y)) continue;

                // y=0 is top, y=h-1 is bottom. Fill from bottom up.
                if (y >= (h - 1 - fillRow)) {
                    ledManager.setPixelXY(panel, x, y, color);
                }
            }
        }
    }

    ledManager.show();
}
```

**Step 2: Build and flash**

Run: `pio run -e feather_esp32s3 -t upload`
Expected: Button press → brief white flash → warm white fills from bottom to top → holds steady.

**Step 3: Commit**

```bash
git add src/animation.cpp
git commit -m "animation: button press flash + bottom-to-top fill"
```

---

### Task 5: Update state machine — sound + animation completion gate

**Files:**
- Modify: `src/main.cpp`

**Step 1: Gate COOLDOWN on both animation AND sound completion**

In the `ANIMATING` case in `loop()`:

```cpp
case DeviceState::ANIMATING:
    if (animationManager.isComplete() && !audioManager.isPlaying()) {
        changeState(DeviceState::COOLDOWN);
    }
    break;
```

This ensures the button stays locked until both the LED animation and the sound finish.

**Step 2: Build**

Run: `pio run -e feather_esp32s3`
Expected: Build succeeds. (Audio can't be tested until WAV file is added.)

**Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "state: gate cooldown on both animation and sound completion"
```

---

### Task 6: Battery indicator module

**Files:**
- Create: `src/battery_indicator.h`
- Create: `src/battery_indicator.cpp`
- Modify: `src/main.cpp`

**Step 1: Create battery_indicator.h**

```cpp
#pragma once

#include <Arduino.h>
#include "config.h"

class BatteryIndicator {
public:
    void begin();
    void update();   // Call every loop — handles its own timing
    float readVoltage();
    uint8_t percentFromVoltage(float voltage);

private:
    uint32_t lastReadTime = 0;
    uint8_t  lastPercent = 100;
    void render(uint8_t percent);
};

extern BatteryIndicator batteryIndicator;
```

**Step 2: Create battery_indicator.cpp**

```cpp
#include "battery_indicator.h"
#include "led_manager.h"

BatteryIndicator batteryIndicator;

void BatteryIndicator::begin() {
    analogReadResolution(12);
    // Read initial value
    lastPercent = percentFromVoltage(readVoltage());
    render(lastPercent);
    Serial.printf("Battery: %.2fV (%d%%)\n", readVoltage(), lastPercent);
}

void BatteryIndicator::update() {
    if (millis() - lastReadTime < BATTERY_READ_INTERVAL_MS) return;
    lastReadTime = millis();

    float voltage = readVoltage();
    lastPercent = percentFromVoltage(voltage);
    render(lastPercent);
    Serial.printf("Battery: %.2fV (%d%%)\n", voltage, lastPercent);
}

float BatteryIndicator::readVoltage() {
    // Feather ESP32-S3 has a 1:1 voltage divider on A13 (GPIO 35) ← VERIFY ON HARDWARE
    // ADC reads 0-4095 for 0-3.3V, actual battery voltage is 2x the reading
    uint32_t raw = 0;
    for (int i = 0; i < 16; i++) {
        raw += analogRead(BATTERY_ADC_PIN);
    }
    raw /= 16;  // Average 16 samples to reduce noise

    float adcVoltage = (float)raw / 4095.0f * 3.3f;
    return adcVoltage * 2.0f;  // Voltage divider ratio
}

uint8_t BatteryIndicator::percentFromVoltage(float voltage) {
    // LiPo discharge curve approximation (3.7V nominal, 4.2V full, 3.0V empty)
    if (voltage >= 4.15f) return 100;
    if (voltage >= 4.00f) return 85;
    if (voltage >= 3.85f) return 70;
    if (voltage >= 3.75f) return 55;
    if (voltage >= 3.65f) return 40;
    if (voltage >= 3.55f) return 25;
    if (voltage >= 3.45f) return 15;
    if (voltage >= 3.35f) return 8;
    if (voltage >= 3.20f) return 3;
    return 0;
}

void BatteryIndicator::render(uint8_t percent) {
    // Calculate strand offset for the indicator LEDs
    uint16_t baseIdx;
    switch (BATTERY_INDICATOR_PANEL) {
        case PANEL_SIDE_LEFT:  baseIdx = STRAND_OFFSET_SIDE_LEFT + BATTERY_INDICATOR_OFFSET; break;
        case PANEL_SIDE_RIGHT: baseIdx = STRAND_OFFSET_SIDE_RIGHT + BATTERY_INDICATOR_OFFSET; break;
        default:               baseIdx = STRAND_OFFSET_SIDE_LEFT + BATTERY_INDICATOR_OFFSET; break;
    }

    Adafruit_NeoPXL8* strip = ledManager.getStrip();

    // Determine how many LEDs to light and what color
    uint8_t lit = 0;
    uint32_t color = 0;

    if (percent > 75) {
        lit = 4;
        color = Adafruit_NeoPixel::Color(0, 40, 0);     // Green (dim)
    } else if (percent > 50) {
        lit = 3;
        color = Adafruit_NeoPixel::Color(0, 40, 0);
    } else if (percent > 25) {
        lit = 2;
        color = Adafruit_NeoPixel::Color(0, 40, 0);
    } else if (percent > 15) {
        lit = 1;
        color = Adafruit_NeoPixel::Color(40, 30, 0);    // Yellow (dim)
    } else {
        lit = 1;
        bool blink = (millis() / 500) % 2;              // Blink at 1Hz
        color = blink ? Adafruit_NeoPixel::Color(40, 0, 0) : 0;  // Red blink
    }

    for (uint8_t i = 0; i < BATTERY_INDICATOR_COUNT; i++) {
        strip->setPixelColor(baseIdx + i, (i < lit) ? color : 0);
    }
    // Don't call show() here — main loop animation calls show() each frame
}
```

**Step 3: Integrate into main.cpp**

Add `#include "battery_indicator.h"` at the top.

In `setup()`, after `powerManager.begin()`:
```cpp
batteryIndicator.begin();
```

In `loop()`, add:
```cpp
batteryIndicator.update();
```

**Step 4: Build and flash**

Run: `pio run -e feather_esp32s3 -t upload`
Expected: On serial monitor, see battery voltage readings every 30s. If 4 LEDs are connected after the side panel, they show green.

**Step 5: Commit**

```bash
git add src/battery_indicator.h src/battery_indicator.cpp src/main.cpp
git commit -m "feat: battery indicator with 4 LEDs and voltage-to-percent mapping"
```

---

### Task 7: Fix ESP-NOW channel

**Files:**
- Modify: `src/espnow_sender.cpp`
- Modify: `src_qtpy/main.cpp`

**Step 1: Set fixed channel on sender**

In `espnow_sender.cpp`, after `WiFi.disconnect()` in `begin()`:

```cpp
// Set fixed WiFi channel for ESP-NOW
esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
```

Also update the peer info channel:
```cpp
peerInfo.channel = ESPNOW_CHANNEL;
```

**Step 2: Set fixed channel on receiver**

In `src_qtpy/main.cpp`, after `WiFi.disconnect()`:

```cpp
#include <esp_wifi.h>

// Set fixed WiFi channel — must match sender
esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);  // Channel 1 — match ESPNOW_CHANNEL in sender config
```

**Step 3: Build both environments**

Run: `pio run`
Expected: Both feather_esp32s3 and qtpy_esp32s3 build successfully.

**Step 4: Commit**

```bash
git add src/espnow_sender.cpp src_qtpy/main.cpp
git commit -m "espnow: fix channel — hard-code matching channel on sender and receiver"
```

---

### Task 8: OTA partition table

**Files:**
- Create: `partitions_ota.csv`
- Modify: `platformio.ini`

**Step 1: Create OTA-capable partition table**

Create `partitions_ota.csv` in project root:

```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x300000,
app1,     app,  ota_1,   0x310000,0x300000,
spiffs,   data, spiffs,  0x610000,0x1F0000,
```

This gives: 2x 3MB app slots + 1.9MB LittleFS + NVS. Total fits in 8MB flash.

**Step 2: Update platformio.ini**

Change the feather env:
```ini
board_build.partitions = partitions_ota.csv
```

**Step 3: Build**

Run: `pio run -e feather_esp32s3`
Expected: Build succeeds. Flash layout now supports OTA.

**Important:** After changing partition table, do a full flash (erase + upload) to avoid corruption:
```bash
pio run -e feather_esp32s3 -t erase
pio run -e feather_esp32s3 -t upload
pio run -e feather_esp32s3 -t uploadfs
```

**Step 4: Commit**

```bash
git add partitions_ota.csv platformio.ini
git commit -m "partitions: switch to OTA-capable layout (2x 3MB app + 1.9MB LittleFS)"
```

---

### Task 9: WiFi provisioning (captive portal)

**Files:**
- Create: `src/wifi_manager.h`
- Create: `src/wifi_manager.cpp`

**Step 1: Create wifi_manager.h**

```cpp
#pragma once

#include <Arduino.h>
#include "config.h"

class WifiManager {
public:
    bool begin();                    // Load saved credentials
    bool hasSavedCredentials();
    bool connectToSaved();           // Try connecting with saved creds
    void startCaptivePortal();       // Start AP + web server for WiFi setup
    void handlePortal();             // Call in loop during portal mode
    bool isConnected();
    bool portalDone();               // True when user has submitted WiFi creds
    void disconnect();

private:
    String savedSSID;
    String savedPass;
    bool   portalComplete = false;
    void saveCredentials(const String& ssid, const String& pass);
    void loadCredentials();
};

extern WifiManager wifiManager;
```

**Step 2: Create wifi_manager.cpp**

```cpp
#include "wifi_manager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

WifiManager wifiManager;

static WebServer server(80);
static Preferences prefs;
static String pendingSSID;
static String pendingPass;

static const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
body{font-family:sans-serif;margin:20px;background:#1a1a2e;color:#eee}
h1{color:#4fc3f7}input,select{width:100%;padding:10px;margin:5px 0 15px;
box-sizing:border-box;font-size:16px;border-radius:5px;border:1px solid #555;
background:#16213e;color:#eee}
button{width:100%;padding:12px;background:#4fc3f7;color:#000;border:none;
border-radius:5px;font-size:18px;font-weight:bold;cursor:pointer}
button:hover{background:#81d4fa}.status{color:#4fc3f7;margin:10px 0}
</style>
</head>
<body>
<h1>Big Blue Button Setup</h1>
<p>Select your WiFi network:</p>
<form action='/save' method='POST'>
<select name='ssid' id='ssid'>%NETWORKS%</select>
<input type='password' name='pass' placeholder='WiFi Password'>
<button type='submit'>Connect</button>
</form>
</body>
</html>
)rawliteral";

static const char DONE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<style>body{font-family:sans-serif;margin:20px;background:#1a1a2e;color:#eee;text-align:center}
h1{color:#66bb6a}</style></head>
<body><h1>Saved!</h1><p>Connecting to WiFi and checking for updates...</p>
<p>You can disconnect from this network now.</p></body></html>
)rawliteral";

bool WifiManager::begin() {
    loadCredentials();
    return true;
}

bool WifiManager::hasSavedCredentials() {
    return savedSSID.length() > 0;
}

bool WifiManager::connectToSaved() {
    if (!hasSavedCredentials()) return false;

    Serial.printf("WiFi: connecting to '%s'...\n", savedSSID.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
        yield();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi: connected, IP=%s, channel=%d\n",
            WiFi.localIP().toString().c_str(), WiFi.channel());
        return true;
    }

    Serial.println("WiFi: connection failed");
    WiFi.disconnect();
    return false;
}

void WifiManager::startCaptivePortal() {
    portalComplete = false;

    // Scan for networks
    Serial.println("WiFi: scanning networks...");
    WiFi.mode(WIFI_AP_STA);
    int n = WiFi.scanNetworks();

    String options;
    for (int i = 0; i < n; i++) {
        options += "<option value='" + WiFi.SSID(i) + "'>" +
                   WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)</option>";
    }

    // Start AP
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("WiFi: AP started '%s', IP=%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

    // Serve portal page
    server.on("/", [options]() {
        String html = FPSTR(PORTAL_HTML);
        html.replace("%NETWORKS%", options);
        server.send(200, "text/html", html);
    });

    // Handle captive portal redirects
    server.onNotFound([options]() {
        String html = FPSTR(PORTAL_HTML);
        html.replace("%NETWORKS%", options);
        server.send(200, "text/html", html);
    });

    // Handle form submission
    server.on("/save", HTTP_POST, []() {
        pendingSSID = server.arg("ssid");
        pendingPass = server.arg("pass");
        server.send(200, "text/html", FPSTR(DONE_HTML));
        wifiManager.saveCredentials(pendingSSID, pendingPass);
        wifiManager.portalComplete = true;
    });

    server.begin();
    Serial.println("WiFi: captive portal running");
}

void WifiManager::handlePortal() {
    server.handleClient();
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::portalDone() {
    return portalComplete;
}

void WifiManager::disconnect() {
    WiFi.disconnect();
    WiFi.softAPdisconnect();
    WiFi.mode(WIFI_OFF);
}

void WifiManager::saveCredentials(const String& ssid, const String& pass) {
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
    savedSSID = ssid;
    savedPass = pass;
    Serial.printf("WiFi: credentials saved for '%s'\n", ssid.c_str());
}

void WifiManager::loadCredentials() {
    prefs.begin("wifi", true);
    savedSSID = prefs.getString("ssid", "");
    savedPass = prefs.getString("pass", "");
    prefs.end();
    if (savedSSID.length() > 0) {
        Serial.printf("WiFi: loaded saved credentials for '%s'\n", savedSSID.c_str());
    }
}
```

**Step 3: Build**

Run: `pio run -e feather_esp32s3`
Expected: Build succeeds. (Not integrated into main.cpp yet — that's Task 11.)

**Step 4: Commit**

```bash
git add src/wifi_manager.h src/wifi_manager.cpp
git commit -m "feat: WiFi provisioning with captive portal and NVS credential storage"
```

---

### Task 10: OTA firmware updater

**Files:**
- Create: `src/ota_updater.h`
- Create: `src/ota_updater.cpp`

**Step 1: Create ota_updater.h**

```cpp
#pragma once

#include <Arduino.h>
#include "config.h"

enum class OtaResult : uint8_t {
    NO_UPDATE,        // Already on latest version
    UPDATE_SUCCESS,   // Downloaded and verified — will reboot
    UPDATE_FAILED,    // Download or verification failed
    NETWORK_ERROR,    // Couldn't reach update server
};

class OtaUpdater {
public:
    OtaResult checkAndUpdate();   // Blocking — checks URL, downloads if newer, returns result

private:
    bool isNewer(const String& remoteVersion);
};

extern OtaUpdater otaUpdater;
```

**Step 2: Create ota_updater.cpp**

```cpp
#include "ota_updater.h"
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

OtaUpdater otaUpdater;

OtaResult OtaUpdater::checkAndUpdate() {
    Serial.printf("OTA: checking %s\n", FIRMWARE_UPDATE_URL);

    HTTPClient http;
    http.setTimeout(OTA_CHECK_TIMEOUT_MS);

    // Fetch manifest
    http.begin(FIRMWARE_UPDATE_URL);
    int httpCode = http.GET();

    if (httpCode != 200) {
        Serial.printf("OTA: manifest fetch failed, HTTP %d\n", httpCode);
        http.end();
        return OtaResult::NETWORK_ERROR;
    }

    String payload = http.getString();
    http.end();

    // Parse JSON manifest: { "version": "1.2.0", "url": "https://..." }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("OTA: JSON parse error: %s\n", err.c_str());
        return OtaResult::NETWORK_ERROR;
    }

    String remoteVersion = doc["version"].as<String>();
    String firmwareUrl   = doc["url"].as<String>();

    Serial.printf("OTA: current=%s remote=%s\n", FIRMWARE_VERSION, remoteVersion.c_str());

    if (!isNewer(remoteVersion)) {
        Serial.println("OTA: already up to date");
        return OtaResult::NO_UPDATE;
    }

    // Download and flash firmware
    Serial.printf("OTA: downloading %s\n", firmwareUrl.c_str());

    http.begin(firmwareUrl);
    httpCode = http.GET();

    if (httpCode != 200) {
        Serial.printf("OTA: firmware download failed, HTTP %d\n", httpCode);
        http.end();
        return OtaResult::NETWORK_ERROR;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("OTA: invalid content length");
        http.end();
        return OtaResult::UPDATE_FAILED;
    }

    if (!Update.begin(contentLength)) {
        Serial.println("OTA: not enough space for update");
        http.end();
        return OtaResult::UPDATE_FAILED;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);
    http.end();

    if (written != contentLength) {
        Serial.printf("OTA: wrote %d of %d bytes\n", written, contentLength);
        Update.abort();
        return OtaResult::UPDATE_FAILED;
    }

    if (!Update.end(true)) {
        Serial.printf("OTA: update finalize failed: %s\n", Update.errorString());
        return OtaResult::UPDATE_FAILED;
    }

    Serial.println("OTA: update successful, rebooting...");
    return OtaResult::UPDATE_SUCCESS;
}

bool OtaUpdater::isNewer(const String& remoteVersion) {
    // Simple semver comparison: "major.minor.patch"
    int rMajor = 0, rMinor = 0, rPatch = 0;
    int lMajor = 0, lMinor = 0, lPatch = 0;

    sscanf(remoteVersion.c_str(), "%d.%d.%d", &rMajor, &rMinor, &rPatch);
    sscanf(FIRMWARE_VERSION, "%d.%d.%d", &lMajor, &lMinor, &lPatch);

    if (rMajor != lMajor) return rMajor > lMajor;
    if (rMinor != lMinor) return rMinor > lMinor;
    return rPatch > lPatch;
}
```

**Step 3: Add ArduinoJson to lib_deps**

In `platformio.ini`, under feather env `lib_deps`, add:

```ini
    bblanchon/ArduinoJson
```

**Step 4: Build**

Run: `pio run -e feather_esp32s3`
Expected: Build succeeds. (Not integrated into boot flow yet — that's Task 11.)

**Step 5: Commit**

```bash
git add src/ota_updater.h src/ota_updater.cpp platformio.ini
git commit -m "feat: OTA updater with semver comparison and HTTP download"
```

---

### Task 11: Boot mode detection and OTA integration

**Files:**
- Modify: `src/main.cpp`

**Step 1: Add boot mode detection and OTA flow to setup()**

Replace the current `setup()` with:

```cpp
#include "wifi_manager.h"
#include "ota_updater.h"
#include "battery_indicator.h"

// Check if button is held during boot
bool isOtaBootRequested() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    delay(100);  // Let pullup settle

    if (digitalRead(BUTTON_PIN) != LOW) return false;

    // Button is held — wait for BOOT_BUTTON_HOLD_MS to confirm
    Serial.println("Button held — hold for OTA mode...");
    uint32_t start = millis();
    while ((millis() - start) < BOOT_BUTTON_HOLD_MS) {
        if (digitalRead(BUTTON_PIN) != LOW) return false;  // Released early
        delay(50);
        yield();
    }

    Serial.println("OTA mode confirmed");
    return true;
}

void runOtaMode() {
    Serial.println("\n=== OTA UPDATE MODE ===");

    // Show a distinctive LED pattern so user knows they're in OTA mode
    ledManager.clear();
    uint32_t blue = Adafruit_NeoPixel::Color(0, 0, 80);
    for (uint8_t p = 0; p < PANEL_COUNT; p++) {
        ledManager.setMaskedColor(static_cast<PanelId>(p), blue);
    }
    ledManager.show();

    wifiManager.begin();

    // Try saved credentials first
    if (wifiManager.hasSavedCredentials()) {
        if (wifiManager.connectToSaved()) {
            // Connected — check for update
            OtaResult result = otaUpdater.checkAndUpdate();
            if (result == OtaResult::UPDATE_SUCCESS) {
                ESP.restart();
            }
            Serial.printf("OTA result: %d\n", (int)result);
            wifiManager.disconnect();
            return;  // Continue to normal boot
        }
    }

    // No saved creds or connection failed — start portal
    wifiManager.startCaptivePortal();

    // Show portal pattern (pulsing blue)
    Serial.println("Waiting for WiFi setup via portal...");

    while (!wifiManager.portalDone()) {
        wifiManager.handlePortal();
        // Slow blue pulse to indicate portal mode
        uint8_t brightness = (millis() / 8) % 255;
        uint32_t pulseBlue = Adafruit_NeoPixel::Color(0, 0, brightness > 127 ? 255 - brightness : brightness);
        ledManager.clear();
        for (uint8_t p = 0; p < PANEL_COUNT; p++) {
            ledManager.setMaskedColor(static_cast<PanelId>(p), pulseBlue);
        }
        ledManager.show();
        yield();
    }

    // Portal done — connect to the new network
    delay(1000);  // Let the "Saved!" page load on the phone
    WiFi.softAPdisconnect();

    if (wifiManager.connectToSaved()) {
        OtaResult result = otaUpdater.checkAndUpdate();
        if (result == OtaResult::UPDATE_SUCCESS) {
            ESP.restart();
        }
        Serial.printf("OTA result: %d\n", (int)result);
    }

    wifiManager.disconnect();
    // Fall through to normal boot
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Big Blue Bus Button ===");

    if (!ledManager.begin()) {
        Serial.println("FATAL: LED init failed, halting");
        while (true) { yield(); }
    }

    // Check for OTA mode BEFORE initializing other modules
    if (isOtaBootRequested()) {
        runOtaMode();
    }

    // Normal boot — initialize all modules
    button.begin();
    audioManager.begin();
    animationManager.begin();
    espNowSender.begin();
    powerManager.begin();
    batteryIndicator.begin();

    changeState(DeviceState::STARTUP);
    Serial.println("Setup complete\n");
}
```

**Step 2: Build and flash**

Run: `pio run -e feather_esp32s3 -t upload`
Expected:
- Normal power-on: startup ramp → warm white idle (same as before)
- Hold button + power on: LEDs turn blue → serial shows OTA messages → falls through to normal after timeout

**Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: boot mode detection — hold button for OTA, normal boot otherwise"
```

---

### Task 12: Final integration test

**Files:** None (testing only)

**Step 1: Test normal boot flow**

Power on without holding button.
Expected:
1. Startup animation ramps panels one by one (~1.5s)
2. Steady warm white glow on all logo pixels
3. Battery indicator LEDs show charge level
4. Serial prints battery voltage

**Step 2: Test button press**

Press the button during idle.
Expected:
1. White flash (~200ms)
2. Bottom-to-top warm white fill (~1s)
3. Returns to steady warm white
4. Serial prints "Button pressed!" and "ESP-NOW sent"
5. Pressing button again during animation does nothing

**Step 3: Test OTA mode**

Hold button, power on, keep holding for 2 seconds.
Expected:
1. LEDs turn blue
2. Serial prints "OTA mode confirmed"
3. If no saved WiFi: AP "BigBlueButton-Setup" appears
4. Connect phone, portal loads, select network, submit
5. Serial shows WiFi connection + OTA check
6. Falls through to normal operation after check

**Step 4: Test ESP-NOW**

Flash receiver (another ESP32-S3) with `pio run -e qtpy_esp32s3 -t upload`.
Open serial on receiver. Press button on main unit.
Expected: Receiver prints "Sending spacebar keypress".

**Step 5: Final commit (if any tweaks needed)**

```bash
git add -A
git commit -m "fix: integration test adjustments"
```

---

## Task Dependency Graph

```
Task 1 (config) ─────┬──► Task 2 (idle glow) ──► Task 3 (startup) ──► Task 4 (button anim)
                      │                                                        │
                      ├──► Task 5 (battery) ────────────────────────────────────┤
                      │                                                        │
                      ├──► Task 6 (ESP-NOW channel) ────────────────────────────┤
                      │                                                        ▼
                      ├──► Task 7 (partition table)                      Task 5 (completion gate)
                      │         │                                              │
                      │         ▼                                              │
                      ├──► Task 9 (WiFi provisioning)                          │
                      │         │                                              │
                      │         ▼                                              │
                      └──► Task 10 (OTA updater) ──────────────────────────────┤
                                                                               ▼
                                                                    Task 11 (boot mode + integration)
                                                                               │
                                                                               ▼
                                                                    Task 12 (integration test)
```

Tasks 2-6 and 7-10 can be worked in parallel. Task 11 depends on all previous tasks. Task 12 is final verification.
