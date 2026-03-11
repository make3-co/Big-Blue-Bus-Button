#include "battery_indicator.h"
#include "led_manager.h"
#include <Adafruit_LC709203F.h>

BatteryIndicator batteryIndicator;

void BatteryIndicator::begin() {
    // VBUS charger detection pin
    pinMode(VBUS_DETECT_PIN, INPUT);

    fuelGauge = new Adafruit_LC709203F();

    if (fuelGauge->begin()) {
        fuelGauge->setPackSize(LC709203F_APA_3000MAH);  // Closest available APA for our cells
        fuelGauge->setAlarmVoltage(3.2f);
        fuelGaugeOk = true;
        Serial.println("LC709203F fuel gauge found");
    } else {
        delete fuelGauge;
        fuelGauge = nullptr;
        fuelGaugeOk = false;
        Serial.println("LC709203F not found — falling back to ADC");
        analogReadResolution(12);
    }

    // Initial reading
    charging = digitalRead(VBUS_DETECT_PIN) == HIGH;
    if (fuelGaugeOk) {
        lastVoltage = fuelGauge->cellVoltage();
        lastPercent = (uint8_t)constrain(fuelGauge->cellPercent(), 0.0f, 100.0f);
    } else {
        lastVoltage = readVoltageADC();
        lastPercent = percentFromVoltage(lastVoltage);
    }
    render(lastPercent);
    Serial.printf("Battery: %.2fV (%d%%) %s [%s]\n", lastVoltage, lastPercent,
                  charging ? "CHARGING" : "", fuelGaugeOk ? "LC709203F" : "ADC");
}

void BatteryIndicator::update() {
    if (millis() - lastReadTime < BATTERY_READ_INTERVAL_MS) return;
    lastReadTime = millis();

    charging = digitalRead(VBUS_DETECT_PIN) == HIGH;
    if (fuelGaugeOk) {
        lastVoltage = fuelGauge->cellVoltage();
        lastPercent = (uint8_t)constrain(fuelGauge->cellPercent(), 0.0f, 100.0f);
    } else {
        lastVoltage = readVoltageADC();
        lastPercent = percentFromVoltage(lastVoltage);
    }
    render(lastPercent);
    Serial.printf("Battery: %.2fV (%d%%) %s [%s]\n", lastVoltage, lastPercent,
                  charging ? "CHARGING" : "", fuelGaugeOk ? "LC709203F" : "ADC");
}

float BatteryIndicator::getVoltage() {
    return lastVoltage;
}

uint8_t BatteryIndicator::getPercent() {
    return lastPercent;
}

float BatteryIndicator::readVoltageADC() {
    // Feather ESP32-S3 has a voltage divider on A13 (GPIO 35)
    // ADC reads 0-4095 for 0-3.3V, actual battery voltage is 2x the reading
    uint32_t raw = 0;
    for (int i = 0; i < 16; i++) {
        raw += analogRead(BATTERY_ADC_PIN);
    }
    raw /= 16;

    float adcVoltage = (float)raw / 4095.0f * 3.3f;
    return adcVoltage * 2.0f;
}

uint8_t BatteryIndicator::percentFromVoltage(float voltage) {
    // LiPo discharge curve (3.0V empty, 4.2V full)
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
    uint16_t baseIdx;
    switch (BATTERY_INDICATOR_PANEL) {
        case PANEL_SIDE_LEFT:  baseIdx = STRAND_OFFSET_SIDE_LEFT + BATTERY_INDICATOR_OFFSET; break;
        case PANEL_SIDE_RIGHT: baseIdx = STRAND_OFFSET_SIDE_RIGHT + BATTERY_INDICATOR_OFFSET; break;
        default:               baseIdx = STRAND_OFFSET_SIDE_LEFT + BATTERY_INDICATOR_OFFSET; break;
    }

    Adafruit_NeoPXL8* strip = ledManager.getStrip();

    uint8_t lit = 0;
    uint32_t color = 0;

    if (percent > 75) {
        lit = 4;
        color = Adafruit_NeoPixel::Color(0, 40, 0);      // Green (dim)
    } else if (percent > 50) {
        lit = 3;
        color = Adafruit_NeoPixel::Color(0, 40, 0);
    } else if (percent > 25) {
        lit = 2;
        color = Adafruit_NeoPixel::Color(0, 40, 0);
    } else if (percent > 15) {
        lit = 1;
        color = Adafruit_NeoPixel::Color(40, 30, 0);     // Yellow (dim)
    } else {
        lit = 1;
        bool blink = (millis() / 500) % 2;
        color = blink ? Adafruit_NeoPixel::Color(40, 0, 0) : 0;  // Red blink
    }

    for (uint8_t i = 0; i < BATTERY_INDICATOR_COUNT; i++) {
        strip->setPixelColor(baseIdx + i, (i < lit) ? color : 0);
    }
    // Don't call show() — main loop animation calls show() each frame
}
