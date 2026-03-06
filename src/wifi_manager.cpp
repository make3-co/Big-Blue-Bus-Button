#include "wifi_manager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

WifiManager wifiManager;

static WebServer server(80);
static Preferences prefs;
static String pendingSSID;
static String pendingPass;

static const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
body{font-family:sans-serif;margin:20px;background:#1a1a2e;color:#eee}
h1{color:#4fc3f7}input,select{width:100%;padding:10px;margin:5px 0 15px;
box-sizing:border-box;font-size:16px;border-radius:5px;border:1px solid #555;
background:#16213e;color:#eee}
button{width:100%;padding:12px;background:#4fc3f7;color:#000;border:none;
border-radius:5px;font-size:18px;font-weight:bold;cursor:pointer}
button:hover{background:#81d4fa}.status{color:#4fc3f7;margin:10px 0}
</style>
</head>
<body>
<h1>Big Blue Button Setup</h1>
<p>Select your WiFi network:</p>
<form action='/save' method='POST'>
<select name='ssid' id='ssid'>%NETWORKS%</select>
<input type='password' name='pass' placeholder='WiFi Password'>
<button type='submit'>Connect</button>
</form>
</body>
</html>
)rawliteral";

static const char DONE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<style>body{font-family:sans-serif;margin:20px;background:#1a1a2e;color:#eee;text-align:center}
h1{color:#66bb6a}</style></head>
<body><h1>Saved!</h1><p>Connecting to WiFi and checking for updates...</p>
<p>You can disconnect from this network now.</p></body></html>
)rawliteral";

bool WifiManager::begin() {
    loadCredentials();
    return true;
}

bool WifiManager::hasSavedCredentials() {
    return savedSSID.length() > 0;
}

bool WifiManager::connectToSaved() {
    if (!hasSavedCredentials()) return false;

    Serial.printf("WiFi: connecting to '%s'...\n", savedSSID.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
        yield();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi: connected, IP=%s, channel=%d\n",
            WiFi.localIP().toString().c_str(), WiFi.channel());
        return true;
    }

    Serial.println("WiFi: connection failed");
    WiFi.disconnect();
    return false;
}

void WifiManager::startCaptivePortal() {
    portalComplete = false;

    Serial.println("WiFi: scanning networks...");
    WiFi.mode(WIFI_AP_STA);
    int n = WiFi.scanNetworks();

    String options;
    for (int i = 0; i < n; i++) {
        options += "<option value='" + WiFi.SSID(i) + "'>" +
                   WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)</option>";
    }

    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("WiFi: AP started '%s', IP=%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

    server.on("/", [options]() {
        String html = FPSTR(PORTAL_HTML);
        html.replace("%NETWORKS%", options);
        server.send(200, "text/html", html);
    });

    server.onNotFound([options]() {
        String html = FPSTR(PORTAL_HTML);
        html.replace("%NETWORKS%", options);
        server.send(200, "text/html", html);
    });

    server.on("/save", HTTP_POST, []() {
        pendingSSID = server.arg("ssid");
        pendingPass = server.arg("pass");
        server.send(200, "text/html", FPSTR(DONE_HTML));
        wifiManager.saveCredentials(pendingSSID, pendingPass);
        wifiManager.markPortalDone();
    });

    server.begin();
    Serial.println("WiFi: captive portal running");
}

void WifiManager::handlePortal() {
    server.handleClient();
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::portalDone() {
    return portalComplete;
}

void WifiManager::markPortalDone() {
    portalComplete = true;
}

void WifiManager::disconnect() {
    WiFi.disconnect();
    WiFi.softAPdisconnect();
    WiFi.mode(WIFI_OFF);
}

void WifiManager::saveCredentials(const String& ssid, const String& pass) {
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
    savedSSID = ssid;
    savedPass = pass;
    Serial.printf("WiFi: credentials saved for '%s'\n", ssid.c_str());
}

void WifiManager::loadCredentials() {
    prefs.begin("wifi", true);
    savedSSID = prefs.getString("ssid", "");
    savedPass = prefs.getString("pass", "");
    prefs.end();
    if (savedSSID.length() > 0) {
        Serial.printf("WiFi: loaded saved credentials for '%s'\n", savedSSID.c_str());
    }
}
