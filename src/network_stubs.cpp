#include "vqtt.h"
#include "wifi_manager.h"

// Provide weak stubs only when WiFi/MQTT support is enabled but implementations are missing.
#ifdef ENABLE_WIFI

// Weak stub implementations to satisfy linker if real modules are absent.
// These are marked weak so real implementations (if present) override them.

void initVQTT() __attribute__((weak));
void initVQTT() { }

bool vqtt_testPublish(const char* payload) __attribute__((weak));
bool vqtt_testPublish(const char* payload) { (void)payload; return false; }

void loopVQTT() __attribute__((weak));
void loopVQTT() { }

void vqtt_publishTelemetry() __attribute__((weak));
void vqtt_publishTelemetry() { }

void vqtt_publishState() __attribute__((weak));
void vqtt_publishState() { }

bool vqtt_isConnected() __attribute__((weak));
bool vqtt_isConnected() { return false; }

String vqtt_getBroker() __attribute__((weak));
String vqtt_getBroker() { return String(); }

String vqtt_getTopicBase() __attribute__((weak));
String vqtt_getTopicBase() { return String(); }

bool vqtt_fetchBrokerFromWqtt() __attribute__((weak));
bool vqtt_fetchBrokerFromWqtt() { return false; }

void initWiFi() __attribute__((weak));
void initWiFi() { }

void initWifiManager() __attribute__((weak));
void initWifiManager() { }

#endif // ENABLE_WIFI
