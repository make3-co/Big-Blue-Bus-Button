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
