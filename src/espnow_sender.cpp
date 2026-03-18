#include "espnow_sender.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

EspNowSender espNowSender;

static void onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
    // Optional: log send status
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("ESP-NOW send failed");
    }
}

bool EspNowSender::begin() {
    // Initialize WiFi in station mode (required for ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Set fixed WiFi channel for ESP-NOW
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    // Enable modem sleep to save power between sends
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ERROR: ESP-NOW init failed");
        return false;
    }

    esp_now_register_send_cb(onDataSent);

    // Add receiver peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, RECEIVER_MAC, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("ERROR: Failed to add ESP-NOW peer");
        return false;
    }

    Serial.printf("Sender MAC: %s\n", WiFi.macAddress().c_str());
    Serial.printf("Receiver MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        RECEIVER_MAC[0], RECEIVER_MAC[1], RECEIVER_MAC[2],
        RECEIVER_MAC[3], RECEIVER_MAC[4], RECEIVER_MAC[5]);
    return true;
}

void EspNowSender::sendButtonPress() {
    sendMessage(CommandType::BUTTON_PRESS);
}

void EspNowSender::sendBatteryStatus(float voltage, uint8_t percent) {
    sendMessage(CommandType::BATTERY_STATUS, (uint16_t)(voltage * 1000.0f), percent);
}

void EspNowSender::sendMessage(CommandType cmd, uint16_t voltage_mv, uint8_t percent) {
    EspNowMessage msg;
    msg.magic[0]    = ESPNOW_MAGIC_0;
    msg.magic[1]    = ESPNOW_MAGIC_1;
    msg.command     = cmd;
    msg.sequenceNum = sequenceNum++;
    msg.voltage_mv  = voltage_mv;
    msg.percent     = percent;

    // Send redundant packets for reliability
    for (uint8_t i = 0; i < ESPNOW_REDUNDANT_SENDS; i++) {
        esp_now_send(RECEIVER_MAC, (const uint8_t*)&msg, sizeof(msg));
        if (i < ESPNOW_REDUNDANT_SENDS - 1) {
            delay(ESPNOW_SEND_INTERVAL_MS);
        }
    }

    Serial.printf("ESP-NOW sent: cmd=%d seq=%d (%d packets)\n",
        (int)cmd, msg.sequenceNum, ESPNOW_REDUNDANT_SENDS);
}
