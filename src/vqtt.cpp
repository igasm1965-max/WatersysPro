#include "config.h"
#include "vqtt.h"
#include "structures.h"

#ifdef ENABLE_WIFI
#include "prefs.h"
#include "event_logging.h"
#include "sensors.h"
#include "relay_control.h"
#include "state_machine.h"

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <NetworkClientSecure.h>

extern Preferences preferences;

// Externs from main and other modules
extern int tank1Level;
extern int tank2Level;
extern String wifi_getIP();

// ============ PubSubClient MQTT Client ============
WiFiClient wifiClient;           // For plain MQTT
NetworkClientSecure wifiClientSecure; // For TLS/MQTT
PubSubClient mqttClient(wifiClient);
PubSubClient mqttClientSecure(wifiClientSecure);

// Pointer to active client
PubSubClient* activeMqtt = NULL;

static bool mqttConnected = false;
static bool wifiWasConnected = false;  // Track WiFi connection changes
static unsigned long lastTelemetryMs = 0;
static unsigned long pubIntervalMs = 10000;
static String mqttTopicBase = DEFAULT_MQTT_TOPIC_BASE;
static String mqttClientId;
static unsigned long lastConnectAttempt = 0;
static unsigned long reconnectIntervalMs = 5000;

// Helper function
static String safeGetPref(const char *key, const String &def = "") {
    if (preferences.isKey(key)) {
        return preferences.getString(key, def);
    }
    return def;
}

// MQTT Callback handler
static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String t(topic);
  String p((char*)payload, length);
  Serial.printf("[VQTT] Message received: topic=%s payload=%s\n", t.c_str(), p.c_str());
  saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, 0);

  // Strip base
  String prefix = mqttTopicBase + "/cmd/";
  if (!t.startsWith(prefix)) return;
  String cmd = t.substring(prefix.length());

  // Handle commands
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
  }
}

// Generate unique device ID
static String makeDeviceId() {
  uint64_t mac = ESP.getEfuseMac();
  uint32_t random = esp_random();
  char id[32];
  sprintf(id, "w_sys_%02X%02X_%08X", (uint8_t)(mac>>16), (uint8_t)(mac>>8), random);
  return String(id);
}

// MQTT Connection routine
static void connectToBroker() {
  if (!preferences.getBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED)) {
    Serial.println("[VQTT] MQTT disabled");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[VQTT] WiFi not connected");
    return;
  }

  String broker = safeGetPref(PREF_KEY_MQTT_BROKER, "");
  if (broker.length() == 0) {
    Serial.println("[VQTT] No broker configured");
    return;
  }

  bool useTLS = preferences.getBool(PREF_KEY_MQTT_SECURE, DEFAULT_MQTT_SECURE);
  uint32_t port = useTLS ? preferences.getUInt(PREF_KEY_MQTT_TLS_PORT, DEFAULT_MQTT_TLS_PORT)
                         : preferences.getUInt(PREF_KEY_MQTT_PORT, DEFAULT_MQTT_PORT);

  Serial.printf("[VQTT] Configuring MQTT: broker=%s, port=%u, TLS=%s, clientId=%s\n",
                broker.c_str(), (unsigned int)port, useTLS ? "ON" : "OFF", mqttClientId.c_str());

  // DNS resolution
  IPAddress brokerIP;
  if (!WiFi.hostByName(broker.c_str(), brokerIP)) {
    Serial.printf("[VQTT] ERROR: DNS failed for %s\n", broker.c_str());
    return;
  }
  Serial.printf("[VQTT] DNS resolved: %s -> %s\n", broker.c_str(), brokerIP.toString().c_str());

  // Select client and configure
  if (useTLS) {
    Serial.println("[VQTT] Configuring TLS client");
    wifiClientSecure.setInsecure(); // Skip cert verification
    mqttClientSecure.setServer(broker.c_str(), port);
    mqttClientSecure.setCallback(onMqttMessage);
    activeMqtt = &mqttClientSecure;
  } else {
    mqttClient.setServer(broker.c_str(), port);
    mqttClient.setCallback(onMqttMessage);
    activeMqtt = &mqttClient;
  }

  // Attempt connect
  Serial.printf("[VQTT] Connecting to %s:%u (TLS=%s)...\n", broker.c_str(), (unsigned int)port, useTLS ? "ON" : "OFF");
  
  const String user = safeGetPref(PREF_KEY_MQTT_USER, "");
  const String pass = safeGetPref(PREF_KEY_MQTT_PASS, "");
  
  bool connectOk = false;
  if (user.length() > 0) {
    Serial.printf("[VQTT] Using credentials: user=%s\n", user.c_str());
    connectOk = activeMqtt->connect(mqttClientId.c_str(), user.c_str(), pass.c_str());
  } else {
    connectOk = activeMqtt->connect(mqttClientId.c_str());
  }
  
  if (connectOk) {
    Serial.println("[VQTT] ✓ Connected to broker!");
    mqttConnected = true;
    saveEventLog(LOG_INFO, EVENT_SETTINGS_CHANGE, 0);
    
    // Subscribe to commands
    String cmdTopic = mqttTopicBase + "/cmd/#";
    activeMqtt->subscribe(cmdTopic.c_str());
    Serial.printf("[VQTT] Subscribed to: %s\n", cmdTopic.c_str());
    
    // Publish online status
    String statusTopic = mqttTopicBase + "/status";
    activeMqtt->publish(statusTopic.c_str(), "online", true);
    Serial.printf("[VQTT] Published status: %s = online\n", statusTopic.c_str());
    
    // Publish initial telemetry
    vqtt_publishTelemetry();
    vqtt_publishState();
  } else {
    Serial.printf("[VQTT] Connection FAILED (state=%d)\n", activeMqtt->state());
    mqttConnected = false;
  }
}

void initVQTT() {
  // Load Client ID
  String customClientId = safeGetPref(PREF_KEY_MQTT_CLIENT_ID, "");
  mqttClientId = (customClientId.length() > 0) ? customClientId : makeDeviceId();
  
  // Load topic base
  mqttTopicBase = safeGetPref(PREF_KEY_MQTT_TOPIC_BASE, DEFAULT_MQTT_TOPIC_BASE);
  
  // Load publish interval
  uint32_t intervalSec = preferences.getUInt(PREF_KEY_MQTT_PUB_INTERVAL, 10);
  pubIntervalMs = (intervalSec > 0) ? intervalSec * 1000UL : 10000UL;
  
  Serial.printf("[VQTT] Initialized: clientId=%s, topic_base=%s, interval=%lums\n",
                mqttClientId.c_str(), mqttTopicBase.c_str(), pubIntervalMs);
  
  // Initialize default MQTT client (will switch to TLS if needed)
  mqttClient.setServer("", 0);
  mqttClient.setCallback(onMqttMessage);
  activeMqtt = &mqttClient;
  
  // Initial connection attempt only if WiFi is ready
  if (preferences.getBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED) && 
      WiFi.status() == WL_CONNECTED) {
    connectToBroker();
  }
}

void loopVQTT() {
  if (!preferences.getBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED)) return;
  if (!activeMqtt) return;

  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  // Detect WiFi connection changes
  if (wifiConnected && !wifiWasConnected) {
    Serial.println("[VQTT] WiFi connected, initiating MQTT connection...");
    wifiWasConnected = true;
    lastConnectAttempt = 0;  // Force immediate connection
    connectToBroker();
    return;
  }
  
  // Detect WiFi disconnection
  if (!wifiConnected && wifiWasConnected) {
    Serial.println("[VQTT] WiFi disconnected");
    wifiWasConnected = false;
  }
  
  if (!wifiConnected) return;

  // Process any pending MQTT messages
  activeMqtt->loop();

  // Reconnect if needed
  if (!activeMqtt->connected()) {
    unsigned long now = millis();
    
    if (now - lastConnectAttempt >= reconnectIntervalMs) {
      Serial.printf("[VQTT] Reconnecting (wifi=%s)...\n", wifiConnected ? "OK" : "DOWN");
      connectToBroker();
      lastConnectAttempt = now;
      
      // Exponential backoff
      if (reconnectIntervalMs < 60000) {
        reconnectIntervalMs = (reconnectIntervalMs * 2 > 60000) ? 60000 : (reconnectIntervalMs * 2);
      }
    }
    return;
  }

  // Reset backoff on successful connection
  if (reconnectIntervalMs > 5000) {
    reconnectIntervalMs = 5000;
  }

  // Periodic telemetry publish
  if (millis() - lastTelemetryMs >= pubIntervalMs) {
    lastTelemetryMs = millis();
    vqtt_publishTelemetry();
  }
}

bool vqtt_isConnected() {
  return activeMqtt ? activeMqtt->connected() : false;
}

String vqtt_getBroker() {
  return safeGetPref(PREF_KEY_MQTT_BROKER, "");
}

String vqtt_getTopicBase() {
  return mqttTopicBase;
}

bool vqtt_testPublish(const char* payload) {
  if (!activeMqtt || !activeMqtt->connected()) return false;
  String topic = mqttTopicBase + "/test";
  bool ok = activeMqtt->publish(topic.c_str(), payload);
  Serial.printf("[VQTT] Test publish to %s: %s\n", topic.c_str(), ok ? "OK" : "FAILED");
  return ok;
}

void vqtt_publishTelemetry() {
  if (!activeMqtt || !activeMqtt->connected()) {
    Serial.println("[VQTT] Not connected, skipping telemetry");
    return;
  }
  
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
  
  String out;
  serializeJson(doc, out);
  String topic = mqttTopicBase + "/telemetry";
  
  bool ok = activeMqtt->publish(topic.c_str(), out.c_str());
  Serial.printf("[VQTT] Published telemetry to %s (%d bytes): %s\n", 
                topic.c_str(), out.length(), ok ? "OK" : "FAILED");
}

void vqtt_publishState() {
  if (!activeMqtt || !activeMqtt->connected()) {
    Serial.println("[VQTT] Not connected, skipping state");
    return;
  }
  
  DynamicJsonDocument doc(192);
  doc["state"] = (int)systemContext.currentState;
  doc["flags"] = 0;
  
  String out;
  serializeJson(doc, out);
  String topic = mqttTopicBase + "/state";
  
  bool ok = activeMqtt->publish(topic.c_str(), out.c_str());
  Serial.printf("[VQTT] Published state to %s (%d bytes): %s\n",
                topic.c_str(), out.length(), ok ? "OK" : "FAILED");
}

#endif // ENABLE_WIFI
