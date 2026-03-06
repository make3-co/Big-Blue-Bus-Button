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
    void sendButtonPress();  // Sends redundant packets with incrementing sequence number

private:
    uint16_t sequenceNum = 0;
    void sendMessage(CommandType cmd);
};

extern EspNowSender espNowSender;
