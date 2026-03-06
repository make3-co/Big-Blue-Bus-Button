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
            ledManager.setBrightness(BRIGHTNESS_IDLE_MAX);
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

void runOtaMode() {
    Serial.println("\n=== OTA UPDATE MODE ===");

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
            OtaResult result = otaUpdater.checkAndUpdate();
            if (result == OtaResult::UPDATE_SUCCESS) {
                ESP.restart();
            }
            Serial.printf("OTA result: %d\n", (int)result);
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
        OtaResult result = otaUpdater.checkAndUpdate();
        if (result == OtaResult::UPDATE_SUCCESS) {
            ESP.restart();
        }
        Serial.printf("OTA result: %d\n", (int)result);
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
    Serial.println("\n=== Big Blue Bus Button ===");

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
