#pragma once

#include <stdint.h>

// =============================================================================
// ESP-NOW Protocol — Shared between Feather sender and QT Py receiver
// =============================================================================

enum class CommandType : uint8_t {
    BUTTON_PRESS   = 0x01,
    PING           = 0x02,  // Future: heartbeat/connectivity check
    BATTERY_STATUS = 0x03,
};

struct __attribute__((packed)) EspNowMessage {
    uint8_t     magic[2];    // {'B', 'B'} — Big Bus identifier
    CommandType command;
    uint16_t    sequenceNum; // Increments per unique event; receiver deduplicates
    uint16_t    voltage_mv;  // Battery voltage in millivolts (0 if not BATTERY_STATUS)
    uint8_t     percent;     // Battery percentage (0 if not BATTERY_STATUS)
};

static constexpr uint8_t ESPNOW_MAGIC_0 = 'B';
static constexpr uint8_t ESPNOW_MAGIC_1 = 'B';
