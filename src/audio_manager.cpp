#include "audio_manager.h"
#include <LittleFS.h>
#include <Audio.h>

AudioManager audioManager;

static Audio* audio = nullptr;

bool AudioManager::begin() {
    // Initialize amp shutdown pin — start with amp off
    pinMode(AMP_SHUTDOWN_PIN, OUTPUT);
    ampOff();

    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("ERROR: LittleFS mount failed");
        return false;
    }

    // Initialize I2S audio
    audio = new Audio();
    audio->setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);
    audio->setVolume(21);  // 0–21

    Serial.println("Audio manager initialized");
    return true;
}

void AudioManager::update() {
    if (audio) {
        audio->loop();
    }

    // Auto-shutdown amp when playback finishes
    if (playing && audio && !audio->isRunning()) {
        playing = false;
        ampOff();
        Serial.println("Audio playback complete, amp off");
    }
}

void AudioManager::play(const char* filename) {
    if (!audio) return;

    ampOn();
    delay(10);  // Brief delay for amp startup

    // ESP32-audioI2S expects path without leading slash for LittleFS
    audio->connecttoFS(LittleFS, filename);
    playing = true;
    Serial.printf("Playing: %s\n", filename);
}

bool AudioManager::isPlaying() const {
    return playing;
}

void AudioManager::stop() {
    if (audio) {
        audio->stopSong();
    }
    playing = false;
    ampOff();
}

void AudioManager::ampOn() {
    digitalWrite(AMP_SHUTDOWN_PIN, HIGH);
}

void AudioManager::ampOff() {
    digitalWrite(AMP_SHUTDOWN_PIN, LOW);
}
