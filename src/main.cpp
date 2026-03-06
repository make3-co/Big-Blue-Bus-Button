#include <Arduino.h>
#include "config.h"
#include "led_manager.h"
#include "animation.h"
#include "audio_manager.h"
#include "button_handler.h"
#include "espnow_sender.h"
#include "power_manager.h"

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
            audioManager.play("press.wav");
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
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Big Blue Bus Button ===");

    if (!ledManager.begin()) {
        Serial.println("FATAL: LED init failed, halting");
        while (true) { yield(); }
    }

    button.begin();
    audioManager.begin();
    animationManager.begin();
    espNowSender.begin();
    powerManager.begin();

    changeState(DeviceState::STARTUP);
    Serial.println("Setup complete\n");
}

// =============================================================================
// Loop
// =============================================================================

void loop() {
    button.update();
    audioManager.update();
    animationManager.update();
    powerManager.update();

    switch (state) {
        case DeviceState::STARTUP:
            if (animationManager.isComplete()) {
                changeState(DeviceState::IDLE);
            }
            break;

        case DeviceState::IDLE:
            if (button.wasPressed()) {
                Serial.println("Button pressed!");
                changeState(DeviceState::ANIMATING);
            }
            break;

        case DeviceState::ANIMATING:
            if (animationManager.isComplete()) {
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
