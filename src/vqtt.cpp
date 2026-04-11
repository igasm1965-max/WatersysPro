#include "config.h"
#include "vqtt.h"
#include "structures.h"

#ifdef ENABLE_WIFI
#include "prefs.h"
#include "event_logging.h"
#include "sensors.h"
#include "relay_control.h"
#include "state_machine.h"
#include "emergency.h"

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <esp_task_wdt.h>

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
    const String value = preferences.getString(key, def);
    if (value.length() > 0) {
      return value;
    }
    }
    return def;
}

// MQTT Callback handler
static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  extern SystemFlags flags;
  extern SystemContext systemContext;

  String t(topic);
  String p((char*)payload, length);
  Serial.printf("[VQTT] Message received: topic=%s payload=%s\n", t.c_str(), p.c_str());

  // Strip base
  String prefix = mqttTopicBase + "/cmd/";
  if (!t.startsWith(prefix)) return;
  String cmd = t.substring(prefix.length());

  // set_mode and reset_emergency always allowed
  if (cmd == "set_mode") {
    extern SystemState savedStateBeforeManual;
    extern unsigned long savedStateStartTime;
    extern uint8_t savedWaterTreatmentInProgress;
    extern uint8_t savedBackwashInProgress;
    extern uint32_t savedOzonationRemaining;
    extern uint32_t savedAerationRemaining;
    extern uint32_t savedSetlingRemaining;
    extern uint32_t savedBackwashRemaining;
    extern unsigned long manualModeEntryTime;
    extern uint32_t currentOzonationRemaining;
    extern uint32_t currentAerationRemaining;
    extern uint32_t currentSetlingRemaining;
    extern uint32_t currentBackwashRemaining;

    if (p == "manual") {
      // Сохраняем текущее состояние для восстановления при возврате в авто
      savedStateBeforeManual = systemContext.currentState;
      savedStateStartTime = systemContext.stateStartTime;
      FLAGS_LOCK();
      savedWaterTreatmentInProgress = flags.waterTreatmentInProgress;
      savedBackwashInProgress = flags.backwashInProgress;
      FLAGS_UNLOCK();
      savedOzonationRemaining = currentOzonationRemaining;
      savedAerationRemaining = currentAerationRemaining;
      savedSetlingRemaining = currentSetlingRemaining;
      savedBackwashRemaining = currentBackwashRemaining;
      manualModeEntryTime = millis();

      FLAGS_LOCK();
      flags.manualMode = 1;
      FLAGS_UNLOCK();
      if (systemContext.currentState != STATE_IDLE) changeState(STATE_IDLE);
      turnOffAllRelays();
      FLAGS_LOCK();
      flags.waterTreatmentInProgress = 0;
      flags.backwashInProgress = 0;
      FLAGS_UNLOCK();
      saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_SET_MANUAL, SRC_MQTT);
      Serial.println("[VQTT] Mode set to MANUAL");
    } else if (p == "auto") {
      FLAGS_LOCK();
      flags.manualMode = 0;
      FLAGS_UNLOCK();
      turnOffAllRelays();

      // Восстанавливаем состояние, которое было до ручного режима
      if (savedStateBeforeManual != STATE_IDLE) {
        unsigned long timeInManual = millis() - manualModeEntryTime;
        systemContext.previousState = systemContext.currentState;
        systemContext.currentState = savedStateBeforeManual;
        systemContext.stateStartTime = savedStateStartTime + timeInManual;

        FLAGS_LOCK();
        flags.waterTreatmentInProgress = savedWaterTreatmentInProgress;
        flags.backwashInProgress = savedBackwashInProgress;
        FLAGS_UNLOCK();
        currentOzonationRemaining = savedOzonationRemaining;
        currentAerationRemaining = savedAerationRemaining;
        currentSetlingRemaining = savedSetlingRemaining;
        currentBackwashRemaining = savedBackwashRemaining;

        applyStateOutputs(systemContext.currentState);
        Serial.printf("[VQTT] Restored state %d after manual mode\n", savedStateBeforeManual);
      }

      savedStateBeforeManual = STATE_IDLE;
      saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_SET_AUTOMATIC, SRC_MQTT);
      Serial.println("[VQTT] Mode set to AUTO");
    }
    return;
  }

  if (cmd == "reset_emergency") {
    resetEmergency();
    saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, 0, SRC_MQTT);
    Serial.println("[VQTT] Emergency reset via MQTT");
    return;
  }

  // Relay commands require manual mode
  if (!flags.manualMode) {
    Serial.println("[VQTT] Command rejected: not in manual mode");
    saveEventLog(LOG_WARNING, EVENT_MANUAL_OPERATION, MANUAL_COMMAND_REJECTED_AUTO, SRC_MQTT);
    return;
  }

  uint16_t opCode = 0;

  if (cmd == "pump1") {
    bool newState = (p == "on");
    if (newState) { turnOnPump1(); opCode = MANUAL_PUMP1_ON; }
    else          { turnOffPump1(); opCode = MANUAL_PUMP1_OFF; }
  } else if (cmd == "pump2") {
    bool newState = (p == "on");
    if (newState) { turnOnPump2(); opCode = MANUAL_PUMP2_ON; }
    else          { turnOffPump2(); opCode = MANUAL_PUMP2_OFF; }
  } else if (cmd == "aeration") {
    bool newState = (p == "on");
    if (newState) { turnOnAeration(); opCode = MANUAL_AERATION_ON; }
    else          { turnOffAeration(); opCode = MANUAL_AERATION_OFF; }
  } else if (cmd == "ozone") {
    // Озон управляет и аэрацией (как на устройстве)
    bool newState = (p == "on");
    if (newState) {
      turnOnOzone();
      turnOnAeration();
      opCode = MANUAL_OZONE_ON;
    } else {
      turnOffOzone();
      turnOffAeration();
      opCode = MANUAL_OZONE_OFF;
    }
  } else if (cmd == "filter") {
    bool newState = (p == "on");
    if (!newState && flags.waterTreatmentInProgress) {
      // Запрет выключения фильтра во время автоцикла (как на устройстве)
      Serial.println("[VQTT] Filter OFF rejected: auto cycle active");
      opCode = MANUAL_FILTER_OFF;
    } else {
      if (newState) {
        turnOnFilter();
        if (!getRelayState(RELAY_PUMP2)) {
          turnOnPump2();
          saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_PUMP2_ON, SRC_MQTT);
        }
        opCode = MANUAL_FILTER_ON;
      } else {
        turnOffFilter();
        if (getRelayState(RELAY_PUMP2) && !getRelayState(RELAY_BACKWASH)) {
          turnOffPump2();
          saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_PUMP2_OFF, SRC_MQTT);
        }
        opCode = MANUAL_FILTER_OFF;
      }
    }
  } else if (cmd == "backwash") {
    bool newState = (p == "on");
    if (newState) {
      turnOnBackwash();
      if (!getRelayState(RELAY_PUMP2)) {
        turnOnPump2();
        saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_PUMP2_ON, SRC_MQTT);
      }
      opCode = MANUAL_BACKWASH_ON;
    } else {
      turnOffBackwash();
      if (getRelayState(RELAY_PUMP2) && !getRelayState(RELAY_FILTER)) {
        turnOffPump2();
        saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_PUMP2_OFF, SRC_MQTT);
      }
      opCode = MANUAL_BACKWASH_OFF;
    }
  }

  if (opCode > 0) {
    saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, opCode, SRC_MQTT);
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

  String broker = safeGetPref(PREF_KEY_MQTT_BROKER, DEFAULT_MQTT_BROKER);
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
    wifiClientSecure.setTimeout(5); // TLS handshake timeout: 5 секунд (вместо дефолтных 30)
    mqttClientSecure.setServer(broker.c_str(), port);
    mqttClientSecure.setCallback(onMqttMessage);
    activeMqtt = &mqttClientSecure;
  } else {
    wifiClient.setTimeout(5); // TCP connect timeout: 5 секунд
    mqttClient.setServer(broker.c_str(), port);
    mqttClient.setCallback(onMqttMessage);
    activeMqtt = &mqttClient;
  }

  // Attempt connect
  Serial.printf("[VQTT] Connecting to %s:%u (TLS=%s)...\n", broker.c_str(), (unsigned int)port, useTLS ? "ON" : "OFF");
  
  const String user = safeGetPref(PREF_KEY_MQTT_USER, DEFAULT_MQTT_USER);
  const String pass = safeGetPref(PREF_KEY_MQTT_PASS, DEFAULT_MQTT_PASS);
  
  // TLS handshake может занять 15+ секунд — снимаем задачу с WDT на время connect
  esp_task_wdt_delete(NULL);
  
  bool connectOk = false;
  if (user.length() > 0) {
    Serial.printf("[VQTT] Using credentials: user=%s\n", user.c_str());
    connectOk = activeMqtt->connect(mqttClientId.c_str(), user.c_str(), pass.c_str());
  } else {
    connectOk = activeMqtt->connect(mqttClientId.c_str());
  }
  
  // Возвращаем задачу под контроль WDT
  esp_task_wdt_add(NULL);
  esp_task_wdt_reset();
  
  if (connectOk) {
    Serial.println("[VQTT] ✓ Connected to broker!");
    mqttConnected = true;
    saveEventLog(LOG_INFO, EVENT_SETTINGS_CHANGE, 0);
    
    // Subscribe to commands with QoS 1 (guaranteed delivery)
    String cmdTopic = mqttTopicBase + "/cmd/#";
    activeMqtt->subscribe(cmdTopic.c_str(), 1);
    Serial.printf("[VQTT] Subscribed to: %s (QoS 1)\n", cmdTopic.c_str());
    
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
  // One-time migration: update broker to current defaults if outdated value is stored.
  const String oldBroker = preferences.getString(PREF_KEY_MQTT_BROKER, "");
  bool needsMigration = (oldBroker == "REDACTED_HIVEMQ_HOST")
                     || (oldBroker == "192.168.0.103");
  if (needsMigration) {
    preferences.putBool(PREF_KEY_MQTT_ENABLED, true);
    preferences.putString(PREF_KEY_MQTT_BROKER, DEFAULT_MQTT_BROKER);
    preferences.putUInt(PREF_KEY_MQTT_PORT, DEFAULT_MQTT_PORT);
    preferences.putBool(PREF_KEY_MQTT_SECURE, false);
    preferences.putUInt(PREF_KEY_MQTT_TLS_PORT, DEFAULT_MQTT_TLS_PORT);
    preferences.putString(PREF_KEY_MQTT_USER, DEFAULT_MQTT_USER);
    preferences.putString(PREF_KEY_MQTT_PASS, DEFAULT_MQTT_PASS);
    preferences.putString(PREF_KEY_MQTT_TOPIC_BASE, DEFAULT_MQTT_TOPIC_BASE);
    Serial.printf("[VQTT] Migrated MQTT config from '%s' to '%s'\n", oldBroker.c_str(), DEFAULT_MQTT_BROKER);
  }

  // Seed defaults once for first-time devices and for users without manual MQTT setup.
  if (!preferences.isKey(PREF_KEY_MQTT_ENABLED)) preferences.putBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED);
  if (!preferences.isKey(PREF_KEY_MQTT_BROKER)) preferences.putString(PREF_KEY_MQTT_BROKER, DEFAULT_MQTT_BROKER);
  if (!preferences.isKey(PREF_KEY_MQTT_USER)) preferences.putString(PREF_KEY_MQTT_USER, DEFAULT_MQTT_USER);
  if (!preferences.isKey(PREF_KEY_MQTT_PASS)) preferences.putString(PREF_KEY_MQTT_PASS, DEFAULT_MQTT_PASS);
  if (!preferences.isKey(PREF_KEY_MQTT_TOPIC_BASE)) preferences.putString(PREF_KEY_MQTT_TOPIC_BASE, DEFAULT_MQTT_TOPIC_BASE);
  if (!preferences.isKey(PREF_KEY_MQTT_SECURE)) preferences.putBool(PREF_KEY_MQTT_SECURE, DEFAULT_MQTT_SECURE);
  if (!preferences.isKey(PREF_KEY_MQTT_TLS_PORT)) preferences.putUInt(PREF_KEY_MQTT_TLS_PORT, DEFAULT_MQTT_TLS_PORT);

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
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(120);          // 120s keepalive (default 15s too aggressive)
  mqttClientSecure.setBufferSize(512);
  mqttClientSecure.setKeepAlive(120);
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
  
  extern SystemFlags flags;
  extern uint32_t currentOzonationRemaining;
  extern uint32_t currentAerationRemaining;
  extern uint32_t currentSetlingRemaining;
  extern uint32_t currentBackwashRemaining;
  extern unsigned long filterOperationStartTime;
  extern uint32_t filterCleaningInterval;

  DynamicJsonDocument doc(768);
  doc["uptime"] = millis() / 1000;
  doc["ip"] = wifi_getIP();
  doc["tank1"] = tank1Level;
  doc["tank2"] = tank2Level;
  doc["mode"] = flags.manualMode ? "manual" : "auto";
  doc["state"] = (int)systemContext.currentState;

  JsonObject emergency = doc.createNestedObject("emergency");
  emergency["active"] = isInEmergencyMode();
  const char* emMsg = getEmergencyMessage();
  emergency["message"] = (emMsg && emMsg[0]) ? emMsg : "";

  JsonObject fl = doc.createNestedObject("flags");
  fl["tank1Empty"] = flags.tank1Empty;
  fl["tank2Empty"] = flags.tank2Empty;
  fl["filterCleaningNeeded"] = flags.filterCleaningNeeded;
  fl["backwashInProgress"] = flags.backwashInProgress;
  fl["manualMode"] = flags.manualMode;

  JsonObject rel = doc.createNestedObject("relays");
  rel["pump1"] = getRelayState(RELAY_PUMP1) ? 1 : 0;
  rel["pump2"] = getRelayState(RELAY_PUMP2) ? 1 : 0;
  rel["aeration"] = getRelayState(RELAY_AERATION) ? 1 : 0;
  rel["ozone"] = getRelayState(RELAY_OZONE) ? 1 : 0;
  rel["filter"] = getRelayState(RELAY_FILTER) ? 1 : 0;
  rel["backwash"] = getRelayState(RELAY_BACKWASH) ? 1 : 0;

  JsonObject tmr = doc.createNestedObject("timers");
  tmr["ozonation"] = currentOzonationRemaining;
  tmr["aeration"] = currentAerationRemaining;
  tmr["settling"] = currentSetlingRemaining;
  tmr["backwash"] = currentBackwashRemaining;
  // Время до следующей промывки фильтра (секунды, 0 если не запущен)
  if (filterOperationStartTime != 0 && filterCleaningInterval > 0) {
    unsigned long elapsedSec = (millis() - filterOperationStartTime) / 1000UL;
    uint32_t remaining = (elapsedSec < filterCleaningInterval)
                         ? (filterCleaningInterval - (uint32_t)elapsedSec) : 0;
    tmr["filter_next"] = remaining;
  } else {
    tmr["filter_next"] = 0;
  }
  
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
