#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include "protocol.h"

// =============================================================================
// QT Py ESP32-S3 — ESP-NOW Receiver → USB HID Spacebar
//
// Receives BUTTON_PRESS commands via ESP-NOW and sends a spacebar keypress
// to the connected PC (advances PowerPoint slides).
// =============================================================================

USBHIDKeyboard keyboard;

static volatile bool sendKey = false;
static volatile uint16_t lastSeqNum = 0xFFFF;  // For deduplication

// ESP-NOW receive callback (runs in WiFi task context)
void IRAM_ATTR onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(EspNowMessage)) return;

    const EspNowMessage* msg = reinterpret_cast<const EspNowMessage*>(data);

    // Validate magic bytes
    if (msg->magic[0] != ESPNOW_MAGIC_0 || msg->magic[1] != ESPNOW_MAGIC_1) return;

    // Deduplicate: ignore if same sequence number as last processed
    if (msg->sequenceNum == lastSeqNum) return;

    if (msg->command == CommandType::BUTTON_PRESS) {
        lastSeqNum = msg->sequenceNum;
        sendKey = true;
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Big Blue Bus Button — QT Py Receiver ===");

    // Initialize USB HID Keyboard
    USB.begin();
    keyboard.begin();
    delay(1000);  // Allow USB enumeration

    // Initialize WiFi + ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("ERROR: ESP-NOW init failed");
        while (true) { yield(); }
    }

    esp_now_register_recv_cb(onDataRecv);

    // Print MAC address so user can configure it on the sender
    Serial.print("Receiver MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.println("Ready — waiting for button press events\n");
}

void loop() {
    if (sendKey) {
        sendKey = false;
        Serial.println("Sending spacebar keypress");
        keyboard.press(' ');
        delay(50);
        keyboard.release(' ');
    }
}
