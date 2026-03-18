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
    // Read initial volume from pot
    pinMode(VOLUME_POT_PIN, INPUT);
    updateVolume();

    Serial.println("Audio manager initialized");
    return true;
}

void AudioManager::pumpAudio() {
    if (!audio) return;
    for (int i = 0; i < 20; i++) {
        audio->loop();
    }
}

void AudioManager::update() {
    pumpAudio();

    // Update volume from pot
    updateVolume();

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
    delay(20);  // MAX98357A needs time to wake from shutdown

    Serial.printf("[%lu] connecttoFS\n", millis());
    audio->connecttoFS(LittleFS, filename);
    playing = true;

    // Aggressively prime the I2S DMA buffer so audio starts immediately
    Serial.printf("[%lu] priming buffer\n", millis());
    for (int i = 0; i < 100; i++) {
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

void AudioManager::updateVolume() {
    if (!audio) return;
    if (millis() - lastVolumeReadTime < 200) return;
    lastVolumeReadTime = millis();

    // Average 8 reads for stability
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogRead(VOLUME_POT_PIN);
    }
    uint16_t raw = sum / 8;

    // Map ADC (0–4095) to volume (1–21), never fully silent
    uint8_t vol = map(raw, 0, 4095, 1, 21);

    if (vol != lastVolume) {
        lastVolume = vol;
        audio->setVolume(vol);
        // Serial.printf("Volume: %d/21\n", vol);
    }
}
