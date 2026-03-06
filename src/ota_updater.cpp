#include "ota_updater.h"
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

OtaUpdater otaUpdater;

OtaResult OtaUpdater::checkAndUpdate() {
    Serial.printf("OTA: checking %s\n", FIRMWARE_UPDATE_URL);

    HTTPClient http;
    http.setTimeout(OTA_CHECK_TIMEOUT_MS);

    http.begin(FIRMWARE_UPDATE_URL);
    http.addHeader("Authorization", String("Bearer ") + OTA_API_KEY);
    int httpCode = http.GET();

    if (httpCode != 200) {
        Serial.printf("OTA: manifest fetch failed, HTTP %d\n", httpCode);
        http.end();
        return OtaResult::NETWORK_ERROR;
    }

    String payload = http.getString();
    http.end();

    // Parse JSON: { "version": "1.2.0" }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("OTA: JSON parse error: %s\n", err.c_str());
        return OtaResult::NETWORK_ERROR;
    }

    String remoteVersion = doc["version"].as<String>();

    // Construct firmware URL from manifest URL base path
    String baseUrl = String(FIRMWARE_UPDATE_URL);
    baseUrl = baseUrl.substring(0, baseUrl.lastIndexOf('/'));
    String firmwareUrl = baseUrl + "/v" + remoteVersion + "/firmware.bin";

    Serial.printf("OTA: current=%s remote=%s\n", FIRMWARE_VERSION, remoteVersion.c_str());

    if (!isNewer(remoteVersion)) {
        Serial.println("OTA: already up to date");
        return OtaResult::NO_UPDATE;
    }

    Serial.printf("OTA: downloading %s\n", firmwareUrl.c_str());

    http.begin(firmwareUrl);
    http.addHeader("Authorization", String("Bearer ") + OTA_API_KEY);
    httpCode = http.GET();

    if (httpCode != 200) {
        Serial.printf("OTA: firmware download failed, HTTP %d\n", httpCode);
        http.end();
        return OtaResult::NETWORK_ERROR;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("OTA: invalid content length");
        http.end();
        return OtaResult::UPDATE_FAILED;
    }

    if (!Update.begin(contentLength)) {
        Serial.println("OTA: not enough space for update");
        http.end();
        return OtaResult::UPDATE_FAILED;
    }

    Serial.printf("OTA: content length=%d, writing to flash...\n", contentLength);

    WiFiClient* stream = http.getStreamPtr();

    // Manual chunked read+write with yield() to prevent WDT reset
    uint8_t buf[1024];
    size_t written = 0;
    while (written < (size_t)contentLength) {
        size_t toRead = min((size_t)sizeof(buf), (size_t)contentLength - written);
        int bytesRead = stream->readBytes(buf, toRead);
        if (bytesRead <= 0) {
            Serial.printf("OTA: read failed at %d/%d\n", written, contentLength);
            break;
        }
        size_t bytesWritten = Update.write(buf, bytesRead);
        if (bytesWritten != (size_t)bytesRead) {
            Serial.printf("OTA: write failed at %d/%d\n", written, contentLength);
            break;
        }
        written += bytesWritten;
        yield();
    }

    Serial.printf("OTA: write done, wrote %d bytes\n", written);
    http.end();
    Serial.println("OTA: http closed");

    if (written != (size_t)contentLength) {
        Serial.printf("OTA: wrote %d of %d bytes\n", written, contentLength);
        Update.abort();
        return OtaResult::UPDATE_FAILED;
    }

    Serial.println("OTA: calling Update.end()...");
    if (!Update.end(true)) {
        Serial.printf("OTA: update finalize failed: %s\n", Update.errorString());
        return OtaResult::UPDATE_FAILED;
    }

    Serial.println("OTA: Update.end() complete, returning SUCCESS");
    Serial.flush();
    return OtaResult::UPDATE_SUCCESS;
}

bool OtaUpdater::isNewer(const String& remoteVersion) {
    int rMajor = 0, rMinor = 0, rPatch = 0;
    int lMajor = 0, lMinor = 0, lPatch = 0;

    sscanf(remoteVersion.c_str(), "%d.%d.%d", &rMajor, &rMinor, &rPatch);
    sscanf(FIRMWARE_VERSION, "%d.%d.%d", &lMajor, &lMinor, &lPatch);

    if (rMajor != lMajor) return rMajor > lMajor;
    if (rMinor != lMinor) return rMinor > lMinor;
    return rPatch > lPatch;
}
