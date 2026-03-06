#pragma once

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Power Manager — Brightness control, amp shutdown, WiFi modem sleep
// =============================================================================

class PowerManager {
public:
    void begin();
    void enterIdleMode();    // Low brightness, amp off, modem sleep
    void enterActiveMode();  // Higher brightness, amp on
    void update();           // Periodic power management tasks

private:
    bool isActive = false;
};

extern PowerManager powerManager;
