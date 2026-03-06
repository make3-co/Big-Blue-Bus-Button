#pragma once

#include <Arduino.h>
#include "config.h"

class WifiManager {
public:
    bool begin();                    // Load saved credentials
    bool hasSavedCredentials();
    bool connectToSaved();           // Try connecting with saved creds
    void startCaptivePortal();       // Start AP + web server for WiFi setup
    void handlePortal();             // Call in loop during portal mode
    bool isConnected();
    bool portalDone();               // True when user has submitted WiFi creds
    void markPortalDone();           // Signal that portal form was submitted
    void disconnect();

private:
    String savedSSID;
    String savedPass;
    bool   portalComplete = false;
    void saveCredentials(const String& ssid, const String& pass);
    void loadCredentials();
};

extern WifiManager wifiManager;
