#pragma once

#include <Arduino.h>
#include "config.h"

enum class OtaResult : uint8_t {
    NO_UPDATE,
    UPDATE_SUCCESS,
    UPDATE_FAILED,
    NETWORK_ERROR,
};

class OtaUpdater {
public:
    OtaResult checkAndUpdate();

private:
    bool isNewer(const String& remoteVersion);
};

extern OtaUpdater otaUpdater;
