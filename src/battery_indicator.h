#pragma once

#include <Arduino.h>
#include "config.h"

class Adafruit_LC709203F;

class BatteryIndicator {
public:
    void begin();
    void update();   // Call every loop — handles its own timing
    float getVoltage();
    uint8_t getPercent();
    bool hasFuelGauge() const { return fuelGaugeOk; }
    bool isCharging() const { return charging; }

private:
    Adafruit_LC709203F* fuelGauge = nullptr;
    bool fuelGaugeOk = false;
    bool charging = false;
    uint32_t lastReadTime = 0;
    uint8_t  lastPercent = 100;
    float    lastVoltage = 4.2f;

    float readVoltageADC();
    uint8_t percentFromVoltage(float voltage);
    void render(uint8_t percent);
};

extern BatteryIndicator batteryIndicator;
