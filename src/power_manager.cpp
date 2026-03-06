#include "power_manager.h"
#include "led_manager.h"
#include "audio_manager.h"
#include <esp_wifi.h>

PowerManager powerManager;

void PowerManager::begin() {
    enterIdleMode();
    Serial.println("Power manager initialized");
}

void PowerManager::enterIdleMode() {
    isActive = false;
    ledManager.setBrightness(BRIGHTNESS_IDLE_MAX);
    audioManager.ampOff();
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    Serial.println("Power: idle mode");
}

void PowerManager::enterActiveMode() {
    isActive = true;
    ledManager.setBrightness(BRIGHTNESS_ACTIVE);
    esp_wifi_set_ps(WIFI_PS_NONE);  // Disable power saving for reliable ESP-NOW
    Serial.println("Power: active mode");
}

void PowerManager::update() {
    // Future: battery monitoring, auto-dimming, etc.
}
