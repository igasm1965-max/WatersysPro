#include "config.h"
#include "vqtt.h"
#include "structures.h"

// MQTT topic base must be valid UTF-8 and consist of a handful of safe
// characters (letters, digits, underscore, hyphen and slash).  Mosquitto
// will reject a connection if the will/topic string contains an invalid
// code sequence, which was the cause of the log entries shown in the bug
// report.  Use this helper to sanity‑check user-provided topic bases.
static bool mqttTopicBaseValid(const String &s) {
  if (s.length() == 0) return false;
  for (size_t i = 0; i < s.length(); ++i) {
    uint8_t c = (uint8_t)s[i];
    // allow ASCII letters, digits, '-' '_' and '/'
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '/') {
      continue;
    }
    return false;
  }
  return true;
}


#ifdef ENABLE_WIFI
#include "prefs.h"
#include "event_logging.h"
#include "sensors.h"
#include "relay_control.h"
#include "state_machine.h"

extern Preferences preferences;

// helper to avoid Preferences errors when a key is absent
static String safeGetPref(const char *key, const String &def = "") {
    if (preferences.isKey(key)) {
        return preferences.getString(key, def);
    }
    return def;
}
#endif

// Externs from main and other modules
extern int tank1Level;
extern int tank2Level;
// FSM state accessed via systemContext
extern String wifi_getIP();

#ifdef ENABLE_WIFI
#include "prefs.h"
#include "event_logging.h"
#include "sensors.h"
#include "relay_control.h"
#include "state_machine.h"

#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

extern Preferences preferences;

static AsyncMqttClient mqttClient;
static bool mqttConnected = false;
static unsigned long lastTelemetryMs = 0;
static unsigned long pubIntervalMs = 10000; // default 10s
static String mqttTopicBase;
static String mqttClientId;

static String makeDeviceId() {
  uint64_t mac = ESP.getEfuseMac();
  char id[16];
  sprintf(id, "WS-%02X%02X", (uint8_t)(mac>>16), (uint8_t)(mac>>8));
  return String(id);
}

static void connectToBroker() {
  if (!preferences.getBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED)) return;
  if (WiFi.status() != WL_CONNECTED) return;
  String broker = safeGetPref(PREF_KEY_MQTT_BROKER, "");
  uint32_t port = preferences.getUInt(PREF_KEY_MQTT_PORT, DEFAULT_MQTT_PORT);
  if (broker.length() == 0) return;

  // make sure the topic base we read earlier is sane.  if not, reset it
  // immediately (both in RAM and persistent storage) so the broken value
  // cannot be used again.
  if (!mqttTopicBaseValid(mqttTopicBase)) {
    Serial.println("[VQTT] warning: mqtt topic base invalid, resetting to default");
    mqttTopicBase = DEFAULT_MQTT_TOPIC_BASE;
    preferences.putString(PREF_KEY_MQTT_TOPIC_BASE, mqttTopicBase);
  }

  mqttClient.setServer(broker.c_str(), (uint16_t)port);
  mqttClient.setClientId(mqttClientId.c_str());
  const String user = safeGetPref(PREF_KEY_MQTT_USER, "");
  const String pass = safeGetPref(PREF_KEY_MQTT_PASS, "");
  if (user.length() > 0) mqttClient.setCredentials(user.c_str(), pass.c_str());
  // LWT
  String statusTopic = mqttTopicBase + "/status";
  mqttClient.setWill(statusTopic.c_str(), 0, true, "offline");
  Serial.printf("[VQTT] Connecting to %s:%u as %s\n", broker.c_str(), (unsigned int)port, mqttClientId.c_str());
  mqttClient.connect();
}

static void onMqttConnect(bool sessionPresent) {
  Serial.println("[VQTT] Connected to broker");
  mqttConnected = true;
  saveEventLog(LOG_INFO, EVENT_SETTINGS_CHANGE, 0);
  // Subscribe to command namespace
  String cmdTopic = mqttTopicBase + "/cmd/#";
  mqttClient.subscribe(cmdTopic.c_str(), 0);
  // Publish availability
  String statusTopic = mqttTopicBase + "/status";
  mqttClient.publish(statusTopic.c_str(), 0, true, "online");
  // Publish initial state
  vqtt_publishState();
  vqtt_publishTelemetry();
}

static void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.printf("[VQTT] Disconnected (%d)\n", (int)reason);
  mqttConnected = false;
}

static void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  // Build strings
  String t(topic);
  String p(payload, len);
  Serial.printf("[VQTT] MSG topic=%s payload=%s\n", t.c_str(), p.c_str());
  saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, 0);

  // Strip base
  String prefix = mqttTopicBase + "/cmd/";
  if (!t.startsWith(prefix)) return;
  String cmd = t.substring(prefix.length());

  // support simple commands: pump1, pump2, aeration, ozone, filter, backwash
  if (cmd == "pump1") {
    if (p == "on") turnOnPump1(); else if (p == "off") turnOffPump1();
  } else if (cmd == "pump2") {
    if (p == "on") turnOnPump2(); else if (p == "off") turnOffPump2();
  } else if (cmd == "aeration") {
    if (p == "on") turnOnAeration(); else if (p == "off") turnOffAeration();
  } else if (cmd == "ozone") {
    if (p == "on") turnOnOzone(); else if (p == "off") turnOffOzone();
  } else if (cmd == "filter") {
    if (p == "on") turnOnFilter(); else if (p == "off") turnOffFilter();
  } else if (cmd == "backwash") {
    if (p == "on") turnOnBackwash(); else if (p == "off") turnOffBackwash();
  } else if (cmd == "state") {
    // state change requests currently not supported via MQTT - log request
    saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, 0);
  }

  // Publish immediate state after handling
  vqtt_publishState();
}

void initVQTT() {
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClientId = makeDeviceId();
  mqttTopicBase = safeGetPref(PREF_KEY_MQTT_TOPIC_BASE, DEFAULT_MQTT_TOPIC_BASE);
  uint32_t intervalSec = preferences.getUInt(PREF_KEY_MQTT_PUB_INTERVAL, 10);
  pubIntervalMs = (intervalSec > 0) ? intervalSec * 1000UL : 10000UL;

  // If MQTT disabled, ensure disconnect
  if (!preferences.getBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED)) {
    if (mqttClient.connected()) mqttClient.disconnect();
    mqttConnected = false;
    return;
  }

  // Attempt connect if Wi-Fi is up
  connectToBroker();
}

void loopVQTT() {
  // Called regularly from main loop
  if (!preferences.getBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED)) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (!mqttClient.connected()) {
    // attempt to connect
    connectToBroker();
    return;
  }

  // Periodic telemetry
  if (millis() - lastTelemetryMs >= pubIntervalMs) {
    lastTelemetryMs = millis();
    vqtt_publishTelemetry();
  }
}

bool vqtt_isConnected() { return mqttClient.connected(); }

String vqtt_getBroker() { return safeGetPref(PREF_KEY_MQTT_BROKER, ""); }

String vqtt_getTopicBase() { return mqttTopicBase; }

bool vqtt_testPublish(const char* payload) {
  if (!mqttClient.connected()) return false;
  String topic = mqttTopicBase + "/test";
  // Fire-and-forget publish (QoS 0)
  int msgId = mqttClient.publish(topic.c_str(), 0, false, payload);
  Serial.printf("[VQTT] Test publish to %s msgId=%d payload=%s\n", topic.c_str(), msgId, payload);
  return (msgId > 0);
}

void vqtt_publishTelemetry() {
  if (!mqttClient.connected()) return;
  DynamicJsonDocument doc(256);
  doc["uptime"] = millis() / 1000;
  doc["ip"] = wifi_getIP();
  doc["tank1"] = tank1Level;
  doc["tank2"] = tank2Level;
  JsonObject rel = doc.createNestedObject("relays");
  rel["pump1"] = getRelayState(RELAY_PUMP1) ? 1 : 0;
  rel["pump2"] = getRelayState(RELAY_PUMP2) ? 1 : 0;
  rel["aeration"] = getRelayState(RELAY_AERATION) ? 1 : 0;
  rel["ozone"] = getRelayState(RELAY_OZONE) ? 1 : 0;
  rel["filter"] = getRelayState(RELAY_FILTER) ? 1 : 0;
  rel["backwash"] = getRelayState(RELAY_BACKWASH) ? 1 : 0;
  String out; serializeJson(doc, out);
  String topic = mqttTopicBase + "/telemetry";
  mqttClient.publish(topic.c_str(), 0, false, out.c_str());
}

void vqtt_publishState() {
  if (!mqttClient.connected()) return;
  DynamicJsonDocument doc(192);
  doc["state"] = (int)systemContext.currentState;
  doc["flags"] = 0; // add flags if needed
  String out; serializeJson(doc, out);
  String topic = mqttTopicBase + "/state";
  mqttClient.publish(topic.c_str(), 0, false, out.c_str());
}

#endif // ENABLE_WIFI
