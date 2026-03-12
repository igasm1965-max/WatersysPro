#include "config.h"
#include "vqtt.h"
#include "structures.h"

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
  char id[32];
  sprintf(id, "user_c7d5b9fb_%02X%02X", (uint8_t)(mac>>16), (uint8_t)(mac>>8));
  return String(id);
}

static void connectToBroker() {
  if (!preferences.getBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED)) {
    Serial.println("[VQTT] MQTT disabled, skipping broker connect");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[VQTT] WiFi not connected, skipping broker connect");
    return;
  }
  String broker = safeGetPref(PREF_KEY_MQTT_BROKER, "");
  uint32_t port = preferences.getUInt(PREF_KEY_MQTT_PORT, DEFAULT_MQTT_PORT);
  if (broker.length() == 0) {
    Serial.println("[VQTT] No broker configured, skipping connect");
    return;
  }
  
  Serial.printf("[VQTT] Configuring MQTT client: broker=%s, port=%u, clientId=%s\n", 
                broker.c_str(), (unsigned int)port, mqttClientId.c_str());
  
  mqttClient.setServer(broker.c_str(), (uint16_t)port);
  mqttClient.setClientId(mqttClientId.c_str());  // ← ВАЖНО: установить Client ID
  
  const String user = safeGetPref(PREF_KEY_MQTT_USER, "");
  const String pass = safeGetPref(PREF_KEY_MQTT_PASS, "");
  if (user.length() > 0) {
    Serial.printf("[VQTT] Setting credentials for user: %s\n", user.c_str());
    mqttClient.setCredentials(user.c_str(), pass.c_str());
  } else {
    Serial.println("[VQTT] No credentials configured (anonymous connection)");
  }
  
  // LWT (Last Will and Testament) - отправляет offline при разрыве
  String statusTopic = mqttTopicBase + "/status";
  mqttClient.setWill(statusTopic.c_str(), 0, true, "offline");
  
  Serial.printf("[VQTT] Attempting to connect to %s:%u\n", broker.c_str(), (unsigned int)port);
  mqttClient.connect();
}

static void onMqttConnect(bool sessionPresent) {
  Serial.printf("[VQTT] Connected to broker (sessionPresent=%d)\n", sessionPresent);
  mqttConnected = true;
  saveEventLog(LOG_INFO, EVENT_SETTINGS_CHANGE, 0);
  
  // Subscribe to command namespace
  String cmdTopic = mqttTopicBase + "/cmd/#";
  Serial.printf("[VQTT] Subscribing to: %s\n", cmdTopic.c_str());
  mqttClient.subscribe(cmdTopic.c_str(), 0);
  
  // Publish availability
  String statusTopic = mqttTopicBase + "/status";
  Serial.printf("[VQTT] Publishing online status to: %s\n", statusTopic.c_str());
  mqttClient.publish(statusTopic.c_str(), 0, true, "online");
  
  // Publish initial state
  vqtt_publishState();
  vqtt_publishTelemetry();
}

static void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  mqttConnected = false;
  
  const char* reasonStr = "UNKNOWN";
  switch(reason) {
    case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
      reasonStr = "TCP connection lost";
      break;
    case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
      reasonStr = "Unacceptable protocol version";
      break;
    case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
      reasonStr = "Identifier rejected (Client ID conflict?)";
      break;
    case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
      reasonStr = "Server unavailable";
      break;
    case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
      reasonStr = "Malformed credentials";
      break;
    case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
      reasonStr = "Not authorized (check username/password)";
      break;
    case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
      reasonStr = "Not enough space";
      break;
    case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
      reasonStr = "TLS bad fingerprint (SSL/TLS error)";
      break;
    default:
      reasonStr = "Unable to determine reason";
  }
  
  Serial.printf("[VQTT] MQTT Disconnected (%d): %s\n", (int)reason, reasonStr);
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
  
  // Load Client ID: either custom from prefs, or auto-generated
  String customClientId = safeGetPref(PREF_KEY_MQTT_CLIENT_ID, "");
  if (customClientId.length() > 0) {
    mqttClientId = customClientId;
  } else {
    mqttClientId = makeDeviceId();
  }
  
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
  
  // Static reconnect state (declared outside if blocks for proper scope)
  static unsigned long lastConnectAttempt = 0;
  static unsigned long reconnectIntervalMs = 5000; // 5 seconds between attempts
  static bool wasConnected = false;
  
  if (!mqttClient.connected()) {
    // attempt to reconnect
    unsigned long now = millis();
    
    if (now - lastConnectAttempt >= reconnectIntervalMs) {
      Serial.printf("[VQTT] Attempting reconnect (WiFi=%s)...\n", 
                    WiFi.isConnected() ? "OK" : "DOWN");
      connectToBroker();
      lastConnectAttempt = now;
      
      // Exponential backoff: increase interval up to 60 seconds
      if (reconnectIntervalMs < 60000) {
        reconnectIntervalMs = (reconnectIntervalMs * 2 > 60000) ? 60000 : (reconnectIntervalMs * 2);
      }
    }
    wasConnected = false;
    return;
  }
  
  // Reset reconnect interval on successful connection
  if (mqttClient.connected() && !wasConnected) {
    wasConnected = true;
    // Reset backoff on successful connection
    reconnectIntervalMs = 5000;
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
