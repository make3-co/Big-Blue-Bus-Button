#pragma once

#include <Arduino.h>
#include "config.h"

class Adafruit_LC709203F;
class Adafruit_MAX17048;

enum class GaugeType : uint8_t { NONE, LC709203F, MAX17048 };

class BatteryIndicator {
public:
    void begin();
    void update();   // Call every loop — handles its own timing
    float getVoltage();
    uint8_t getPercent();
    bool hasFuelGauge() const { return gaugeType != GaugeType::NONE; }
    bool isCharging() const { return charging; }
    bool isLowBattery() const { return lowBattery; }
    void render(uint8_t percent);  // Write indicator LEDs to buffer (call before show)
    void setLowBrightnessWindow(bool low) { lowBrightnessWindow = low; }

private:
    GaugeType gaugeType = GaugeType::NONE;
    Adafruit_LC709203F* lcGauge = nullptr;
    Adafruit_MAX17048* maxGauge = nullptr;
    bool charging = false;
    bool lowBattery = false;
    void readGauge();
    void tryInitGauge();
    uint32_t lastReadTime = 0;
    uint8_t  lastPercent = 100;
    float    lastVoltage = 4.2f;
    float    avgVoltage = 4.2f;        // Rolling average for stable readings
    static constexpr uint8_t AVG_SAMPLES = 4;
    float    voltageSamples[AVG_SAMPLES] = {4.2f, 4.2f, 4.2f, 4.2f};
    uint8_t  sampleIndex = 0;
    bool     lowBrightnessWindow = false;  // Set by animation when brightness is low
};

extern BatteryIndicator batteryIndicator;
