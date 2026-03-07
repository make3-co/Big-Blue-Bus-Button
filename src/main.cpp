#include <Arduino.h>
#include <math.h>
#include <WiFi.h>
#include "config.h"
#include "led_manager.h"
#include "animation.h"
#include "audio_manager.h"
#include "button_handler.h"
#include "espnow_sender.h"
#include "power_manager.h"
#include "battery_indicator.h"
#include "wifi_manager.h"
#include "ota_updater.h"

// =============================================================================
// State Machine
// =============================================================================

enum class DeviceState : uint8_t {
    STARTUP,     // Sequential panel ramp-up
    IDLE,        // Logos glow warm white, waiting for button press
    ANIMATING,   // Green/rainbow fill + sound + ESP-NOW send
    COOLDOWN,    // Resume pulse, ignore button input
};

static DeviceState state = DeviceState::IDLE;
static uint32_t stateStartTime = 0;
static bool macPrinted = false;

void changeState(DeviceState newState) {
    state = newState;
    stateStartTime = millis();

    switch (newState) {
        case DeviceState::STARTUP:
            Serial.println("State: STARTUP");
            ledManager.setBrightness(0);  // Animation ramps brightness gradually
            animationManager.startStartup();
            break;

        case DeviceState::IDLE:
            Serial.println("State: IDLE");
            powerManager.enterIdleMode();
            animationManager.startIdleGlow();
            break;

        case DeviceState::ANIMATING:
            Serial.println("State: ANIMATING");
            powerManager.enterActiveMode();
            animationManager.startButtonPress();
            espNowSender.sendButtonPress();
            break;

        case DeviceState::COOLDOWN:
            Serial.println("State: COOLDOWN");
            powerManager.enterIdleMode();
            animationManager.startIdleGlow();
            break;
    }
}

// =============================================================================
// OTA Boot Mode
// =============================================================================

// Check if button is held during boot for OTA mode
bool isOtaBootRequested() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    delay(100);  // Let pullup settle

    if (digitalRead(BUTTON_PIN) != LOW) return false;

    Serial.println("Button held -- hold for OTA mode...");
    uint32_t start = millis();
    while ((millis() - start) < BOOT_BUTTON_HOLD_MS) {
        if (digitalRead(BUTTON_PIN) != LOW) return false;
        delay(50);
        yield();
    }

    Serial.println("OTA mode confirmed");
    return true;
}

// Pulse all logo LEDs with a color N times (blocking)
void otaPulse(uint32_t color, uint8_t times) {
    for (uint8_t t = 0; t < times; t++) {
        // Fade up
        for (int b = 0; b <= 80; b += 4) {
            float scale = (float)b / 80.0f;
            uint8_t r = ((color >> 16) & 0xFF) * scale;
            uint8_t g = ((color >> 8) & 0xFF) * scale;
            uint8_t bl = (color & 0xFF) * scale;
            uint32_t c = Adafruit_NeoPixel::Color(r, g, bl);
            ledManager.clear();
            for (uint8_t p = 0; p < PANEL_COUNT; p++) {
                ledManager.setMaskedColor(static_cast<PanelId>(p), c);
            }
            ledManager.show();
            delay(10);
        }
        // Fade down
        for (int b = 80; b >= 0; b -= 4) {
            float scale = (float)b / 80.0f;
            uint8_t r = ((color >> 16) & 0xFF) * scale;
            uint8_t g = ((color >> 8) & 0xFF) * scale;
            uint8_t bl = (color & 0xFF) * scale;
            uint32_t c = Adafruit_NeoPixel::Color(r, g, bl);
            ledManager.clear();
            for (uint8_t p = 0; p < PANEL_COUNT; p++) {
                ledManager.setMaskedColor(static_cast<PanelId>(p), c);
            }
            ledManager.show();
            delay(10);
        }
        delay(100);  // Gap between pulses
    }
}

// Rainbow sweep across all logo LEDs (blocking, ~2 seconds)
void otaRainbow() {
    for (uint16_t frame = 0; frame < 512; frame++) {
        ledManager.clear();
        for (uint8_t p = 0; p < PANEL_COUNT; p++) {
            PanelId panel = static_cast<PanelId>(p);
            uint8_t w = ledManager.getPanelWidth(panel);
            uint8_t h = ledManager.getPanelHeight(panel);
            for (uint8_t x = 0; x < w; x++) {
                for (uint8_t y = 0; y < h; y++) {
                    if (!ledManager.isMasked(panel, x, y)) continue;
                    uint16_t hue = (frame * 128 + (x + y) * 1024) % 65536;
                    uint32_t c = Adafruit_NeoPixel::ColorHSV(hue, 255, 80);
                    ledManager.setPixelXY(panel, x, y, c);
                }
            }
        }
        ledManager.show();
        delay(4);
        yield();
    }
}

// Show OTA result with LED feedback
void otaShowResult(OtaResult result) {
    switch (result) {
        case OtaResult::NO_UPDATE:
            Serial.println("OTA: no update available");
            otaPulse(Adafruit_NeoPixel::Color(255, 180, 0), 2);  // Yellow x2
            break;
        case OtaResult::UPDATE_SUCCESS:
            Serial.println("OTA: update successful!");
            otaRainbow();
            // Try to play chime after rainbow
            if (audioManager.begin()) {
                audioManager.play("chime.wav");
                while (audioManager.isPlaying()) {
                    audioManager.update();
                    delay(10);
                }
            }
            delay(500);
            ESP.restart();
            break;
        case OtaResult::UPDATE_FAILED:
        case OtaResult::NETWORK_ERROR:
            Serial.printf("OTA: error (result=%d)\n", (int)result);
            otaPulse(Adafruit_NeoPixel::Color(255, 0, 0), 3);  // Red x3
            break;
    }
}

// Pulsing green while downloading
void otaPulseGreenFrame() {
    uint32_t now = millis();
    uint8_t wave = (uint8_t)((sinf((float)now / 500.0f * 3.14159f) + 1.0f) * 40.0f);
    uint32_t green = Adafruit_NeoPixel::Color(0, wave, 0);
    ledManager.clear();
    for (uint8_t p = 0; p < PANEL_COUNT; p++) {
        ledManager.setMaskedColor(static_cast<PanelId>(p), green);
    }
    ledManager.show();
}

void runOtaMode() {
    Serial.println("\n=== OTA UPDATE MODE ===");
    Serial.printf("Current firmware: v%s\n", FIRMWARE_VERSION);

    // Show blue LEDs so user knows they're in OTA mode
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
            // Pulse green while checking/downloading
            otaPulseGreenFrame();
            Serial.println("OTA: calling checkAndUpdate...");
            OtaResult result = otaUpdater.checkAndUpdate();
            Serial.printf("OTA: result=%d, showing feedback...\n", (int)result);
            otaShowResult(result);
            Serial.println("OTA: feedback done, disconnecting...");
            wifiManager.disconnect();
            return;
        }
    }

    // No saved creds or connection failed -- start portal
    wifiManager.startCaptivePortal();
    Serial.println("Waiting for WiFi setup via portal...");

    while (!wifiManager.portalDone()) {
        wifiManager.handlePortal();

        // Pulsing blue to indicate portal mode
        uint32_t now = millis();
        uint8_t wave = (uint8_t)((sinf((float)now / 1000.0f * 3.14159f) + 1.0f) * 40.0f);
        uint32_t pulseBlue = Adafruit_NeoPixel::Color(0, 0, wave);
        ledManager.clear();
        for (uint8_t p = 0; p < PANEL_COUNT; p++) {
            ledManager.setMaskedColor(static_cast<PanelId>(p), pulseBlue);
        }
        ledManager.show();
        yield();
    }

    // Portal done -- connect to the new network
    delay(1000);  // Let the "Saved!" page load on the phone
    WiFi.softAPdisconnect();

    if (wifiManager.connectToSaved()) {
        otaPulseGreenFrame();
        OtaResult result = otaUpdater.checkAndUpdate();
        otaShowResult(result);
    }

    wifiManager.disconnect();
    // Fall through to normal boot
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    // Immediately kill LED data pins to prevent WS2812B from latching boot noise
    static constexpr int8_t ledPins[] = {NEOPXL8_PIN_0, NEOPXL8_PIN_1, NEOPXL8_PIN_2, NEOPXL8_PIN_3};
    for (auto pin : ledPins) {
        if (pin >= 0) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    }

    // Initialize LEDs ASAP — clear before anything else to kill boot noise
    Serial.begin(115200);
    if (!ledManager.begin()) {
        Serial.println("FATAL: LED init failed, halting");
        while (true) { yield(); }
    }
    delay(100);
    Serial.printf("\n=== Big Blue Bus Button v%s ===\n", FIRMWARE_VERSION);

    // Check for OTA mode BEFORE initializing other modules
    if (isOtaBootRequested()) {
        runOtaMode();
    }

    // Normal boot -- initialize all modules
    button.begin();
    audioManager.begin();
    animationManager.begin();
    espNowSender.begin();
    powerManager.begin();
    batteryIndicator.begin();

    // Re-clear LEDs after all module init (audio/WiFi DMA may have corrupted output)
    ledManager.clear();
    ledManager.show();
    delay(5);

    changeState(DeviceState::STARTUP);
    Serial.println("Setup complete\n");
}

// =============================================================================
// Loop
// =============================================================================

void loop() {
    // Print MAC once after USB CDC has time to reconnect
    if (!macPrinted && millis() > 5000) {
        macPrinted = true;
        Serial.printf("Sender MAC: %s\n", WiFi.macAddress().c_str());
    }

    button.update();
    audioManager.update();
    animationManager.update();
    audioManager.update();  // Extra audio pump after LED show() blocking call
    powerManager.update();
    batteryIndicator.update();

    switch (state) {
        case DeviceState::STARTUP:
            if (animationManager.isComplete()) {
                changeState(DeviceState::IDLE);
            }
            break;

        case DeviceState::IDLE:
            if (button.wasPressed()) {
                Serial.println("Button pressed!");
                audioManager.play("press.wav");
                changeState(DeviceState::ANIMATING);
            }
            break;

        case DeviceState::ANIMATING:
            if (animationManager.isComplete() && !audioManager.isPlaying()) {
                changeState(DeviceState::COOLDOWN);
            }
            break;

        case DeviceState::COOLDOWN:
            // Discard any button presses during cooldown
            button.wasPressed();

            if ((millis() - stateStartTime) >= COOLDOWN_DURATION_MS) {
                changeState(DeviceState::IDLE);
            }
            break;
    }
}
