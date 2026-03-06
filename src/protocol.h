#pragma once

#include <stdint.h>

// =============================================================================
// ESP-NOW Protocol — Shared between Feather sender and QT Py receiver
// =============================================================================

enum class CommandType : uint8_t {
    BUTTON_PRESS = 0x01,
    PING         = 0x02,  // Future: heartbeat/connectivity check
};

struct __attribute__((packed)) EspNowMessage {
    uint8_t     magic[2];    // {'B', 'B'} — Big Bus identifier
    CommandType command;
    uint16_t    sequenceNum; // Increments per unique event; receiver deduplicates
};

static constexpr uint8_t ESPNOW_MAGIC_0 = 'B';
static constexpr uint8_t ESPNOW_MAGIC_1 = 'B';
