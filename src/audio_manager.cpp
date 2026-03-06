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
        // Call loop() multiple times to keep I2S DMA buffer fed.
        // NeoPXL8 show() blocks ~10ms per frame, so a single loop()
        // call per main loop iteration isn't enough at 22050Hz.
        for (int i = 0; i < 20; i++) {
            audio->loop();
        }
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

    Serial.printf("[%lu] ampOn\n", millis());
    ampOn();
    delay(10);

    Serial.printf("[%lu] connecttoFS\n", millis());
    audio->connecttoFS(LittleFS, filename);
    playing = true;

    Serial.printf("[%lu] priming buffer\n", millis());
    for (int i = 0; i < 20; i++) {
        audio->loop();
    }

    Serial.printf("[%lu] play started: %s\n", millis(), filename);
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
