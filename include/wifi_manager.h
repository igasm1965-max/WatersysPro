#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

enum WifiState { WIFI_IDLE=0, WIFI_CONNECTING=1, WIFI_CONNECTED=2, WIFI_FAILED=3 };
void initWifiManager();
void loopWifiManager();

// Trigger background connect attempt with provided credentials (saves to prefs)
void wifi_setCredentials(const String &ssid, const String &pass);
void wifi_clearCredentials();

// Query status helpers
WifiState wifi_getState();
String wifi_getSSID();
int wifi_getAttemptCount();
String wifi_getIP();

#endif // WIFI_MANAGER_H
