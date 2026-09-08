#ifndef VQTT_H
#define VQTT_H

#include <Arduino.h>
#include "structures.h"  // for EventLog

#ifdef ENABLE_WIFI
void initVQTT();
void loopVQTT();
void vqtt_publishTelemetry();
void vqtt_publishState();
void vqtt_publishLogs();

/// Публикует логи + подтверждение в watersystem/logs/ack.
/// Используется для ответа на команду getlogs.
void vqtt_publishLogsWithAck();

// Publish a single log event as it occurs (streaming mode)
void vqtt_publishSingleEvent(const EventLog& event);

// Status helpers
bool vqtt_isConnected();
String vqtt_getBroker();
String vqtt_getTopicBase();

// Reset reconnect backoff/give-up state (call from web UI or engineer menu)
void vqtt_resetReconnectBackoff();

// Test publish - returns true if publish was queued
bool vqtt_testPublish(const char* payload);
#else
// Stubs when WiFi disabled
static inline void initVQTT() {}
static inline void loopVQTT() {}
static inline void vqtt_publishTelemetry() {}
static inline void vqtt_publishState() {}
static inline void vqtt_publishLogs() {}
static inline void vqtt_publishLogsWithAck() {}
static inline void vqtt_publishSingleEvent(const EventLog&) {}
static inline bool vqtt_isConnected() { return false; }
static inline String vqtt_getBroker() { return String(); }
static inline String vqtt_getTopicBase() { return String(); }
static inline bool vqtt_testPublish(const char* payload) { return false; }
static inline void vqtt_resetReconnectBackoff() {}
#endif

#endif // VQTT_H
