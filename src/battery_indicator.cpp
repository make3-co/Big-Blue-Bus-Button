#include "battery_indicator.h"
#include "led_manager.h"
#include "espnow_sender.h"
#include <Wire.h>
#include <Adafruit_LC709203F.h>
#include <Adafruit_MAX1704X.h>

BatteryIndicator batteryIndicator;

void BatteryIndicator::tryInitGauge() {
    if (gaugeType != GaugeType::NONE) return;  // Already found

    Wire.begin(3, 4);  // SDA=GPIO3, SCL=GPIO4 on Feather ESP32-S3

    // Try MAX17048 first (newer Feather boards), then LC709203F
    maxGauge = new Adafruit_MAX17048();
    if (maxGauge->begin(&Wire)) {
        gaugeType = GaugeType::MAX17048;
        Serial.println("MAX17048 fuel gauge initialized");
        return;
    }
    delete maxGauge;
    maxGauge = nullptr;

    lcGauge = new Adafruit_LC709203F();
    if (lcGauge->begin()) {
        gaugeType = GaugeType::LC709203F;
        lcGauge->setPackSize(LC709203F_APA_3000MAH);
        lcGauge->setThermistorB(3950);
        lcGauge->setAlarmVoltage(3.2f);
        Serial.println("LC709203F fuel gauge initialized");
        return;
    }
    delete lcGauge;
    lcGauge = nullptr;
    Serial.println("Fuel gauge not found yet (needs battery voltage)");
}

void BatteryIndicator::begin() {
    // VBUS charger detection pin
    pinMode(VBUS_DETECT_PIN, INPUT);

    tryInitGauge();

    // Initial reading
    charging = digitalRead(VBUS_DETECT_PIN) == HIGH;
    readGauge();
    render(lastPercent);

    const char* gaugeName = gaugeType == GaugeType::MAX17048 ? "MAX17048" :
                            gaugeType == GaugeType::LC709203F ? "LC709203F" : "NONE";
    Serial.printf("Battery: %.2fV (%d%%) %s [%s]\n", lastVoltage, lastPercent,
                  charging ? "CHARGING" : "", gaugeName);
}

void BatteryIndicator::update() {
    // Check more frequently when voltage is low (every 5s below 3.2V, else 30s)
    uint32_t interval = (avgVoltage < 3.2f && avgVoltage > 0.5f) ? 5000 : BATTERY_READ_INTERVAL_MS;
    if (millis() - lastReadTime < interval) return;
    lastReadTime = millis();

    charging = digitalRead(VBUS_DETECT_PIN) == HIGH;

    // Retry gauge init if not found yet (needs battery voltage on BAT pin)
    if (gaugeType == GaugeType::NONE) {
        tryInitGauge();
    }

    readGauge();

    // Rolling average voltage (smooths out load sag spikes)
    voltageSamples[sampleIndex] = lastVoltage;
    sampleIndex = (sampleIndex + 1) % AVG_SAMPLES;
    float sum = 0;
    for (uint8_t i = 0; i < AVG_SAMPLES; i++) sum += voltageSamples[i];
    avgVoltage = sum / (float)AVG_SAMPLES;

    // Only trigger low battery shutdown during low brightness window (less load sag)
    if (avgVoltage <= LOW_BATTERY_CUTOFF_V && !charging && lowBrightnessWindow) {
        lowBattery = true;
    }
    render(lastPercent);

    const char* gaugeName = gaugeType == GaugeType::MAX17048 ? "MAX17048" :
                            gaugeType == GaugeType::LC709203F ? "LC709203F" : "NO GAUGE";
    Serial.printf("Battery: %.2fV (avg:%.2fV) (%d%%) %s%s [%s]\n", lastVoltage, avgVoltage, lastPercent,
                  charging ? "CHARGING " : "", lowBattery ? "LOW!" : "", gaugeName);
    espNowSender.sendBatteryStatus(avgVoltage, hasFuelGauge() ? lastPercent : 255);
}

float BatteryIndicator::getVoltage() {
    return lastVoltage;
}

uint8_t BatteryIndicator::getPercent() {
    return lastPercent;
}

void BatteryIndicator::readGauge() {
    if (gaugeType == GaugeType::MAX17048 && maxGauge) {
        lastVoltage = maxGauge->cellVoltage();
        lastPercent = (uint8_t)constrain(maxGauge->cellPercent(), 0.0f, 100.0f);
    } else if (gaugeType == GaugeType::LC709203F && lcGauge) {
        lastVoltage = lcGauge->cellVoltage();
        lastPercent = (uint8_t)constrain(lcGauge->cellPercent(), 0.0f, 100.0f);
    }
}

void BatteryIndicator::render(uint8_t percent) {
    uint16_t baseIdx = STRAND_OFFSET_FRONT_LEFT + BATTERY_INDICATOR_OFFSET;

    Adafruit_NeoPXL8* strip = ledManager.getStrip();

    // Voltage-based thresholds using rolling average
    uint8_t lit = 0;
    uint32_t color = 0;
    float v = avgVoltage;

    if (v >= 3.75f) {
        lit = 4;
        color = Adafruit_NeoPixel::Color(0, 40, 0);      // Green
    } else if (v >= 3.55f) {
        lit = 3;
        color = Adafruit_NeoPixel::Color(0, 40, 0);
    } else if (v >= 3.35f) {
        lit = 2;
        color = Adafruit_NeoPixel::Color(0, 40, 0);
    } else if (v >= 3.1f) {
        lit = 1;
        color = Adafruit_NeoPixel::Color(40, 30, 0);     // Yellow
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
