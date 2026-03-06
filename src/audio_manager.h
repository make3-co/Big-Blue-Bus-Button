#pragma once

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Audio Manager — I2S WAV playback from LittleFS via MAX98357A
// =============================================================================

class AudioManager {
public:
    bool begin();
    void update();                       // Call every loop iteration (feeds I2S buffer)
    void play(const char* filename);     // Play a WAV file from LittleFS
    bool isPlaying() const;
    void stop();
    void ampOn();
    void ampOff();

private:
    bool playing = false;
};

extern AudioManager audioManager;
