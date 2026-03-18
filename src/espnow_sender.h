#pragma once

#include <Arduino.h>
#include "config.h"
#include "protocol.h"

// =============================================================================
// ESP-NOW Sender — Transmit button press events to QT Py receiver
// =============================================================================

class EspNowSender {
public:
    bool begin();
    void sendButtonPress();
    void sendBatteryStatus(float voltage, uint8_t percent);

private:
    uint16_t sequenceNum = 0;
    void sendMessage(CommandType cmd, uint16_t voltage_mv = 0, uint8_t percent = 0);
};

extern EspNowSender espNowSender;
