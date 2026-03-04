#ifndef VQTT_H
#define VQTT_H

#include <Arduino.h>

#ifdef ENABLE_WIFI
void initVQTT();
void loopVQTT();
void vqtt_publishTelemetry();
void vqtt_publishState();

// Status helpers
bool vqtt_isConnected();
String vqtt_getBroker();
String vqtt_getTopicBase();

// Test publish - returns true if publish was queued
bool vqtt_testPublish(const char* payload);
#else
// Stubs when WiFi disabled
static inline void initVQTT() {}
static inline void loopVQTT() {}
static inline void vqtt_publishTelemetry() {}
static inline void vqtt_publishState() {}
static inline bool vqtt_isConnected() { return false; }
static inline String vqtt_getBroker() { return String(); }
static inline String vqtt_getTopicBase() { return String(); }
static inline bool vqtt_testPublish(const char* payload) { return false; }
#endif

#endif // VQTT_H
