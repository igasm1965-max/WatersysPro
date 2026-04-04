

#include "web_server.h"
#include "config.h"
#include "event_logging.h"
#include "prefs.h"
#include "relay_control.h"
#include "sensors.h"
#include "vqtt.h"
#include "wifi_manager.h"
#include "utils.h"
#include "structures.h"
#include "state_machine.h"
#include "emergency.h"

extern Preferences preferences;

// helper used by handlers to read strings from NVS without triggering
// "NOT_FOUND" errors when a key has not yet been written.
static String safeGetPref(const char *key, const String &def = "") {
    if (preferences.isKey(key)) return preferences.getString(key, def);
    return def;
}

#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// removed SPIFFS-specific code
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Helper to stringify macro values into adjacent string literals inside raw HTML
#define _STR(x) #x
#define STR(x) _STR(x)

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// ---------------------------------------------------------------------------
// Filesystem helpers --------------------------------------------------------
// ---------------------------------------------------------------------------

// Try to send a static resource from SD card first, then SPIFFS.  Returns
// true if a file was found and queued, false otherwise.
static bool sendStaticResource(AsyncWebServerRequest* request,
                               const char* path,
                               const char* contentType) {
  if (sdPresent && SD.exists(path)) {
#ifdef DEBUG_SERIAL
    Serial.printf("[Web] serve %s from SD\n", path);
#endif
    request->send(SD, path, contentType);
    return true;
  }
// removed SPIFFS-specific code
  return false;
}


static void broadcastStatus();
static void webMonitorPeriodic();
// Expose a periodic broadcaster that will be called from main loop
void webServerPeriodic() {
  broadcastStatus();
  webMonitorPeriodic();
}


static unsigned long lastBroadcast = 0;

static bool isStaReallyConnected() {
  wifi_mode_t mode = WiFi.getMode();
  bool staEnabled = (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA);
  if (!staEnabled) return false;
  if (wifi_getSSID().length() == 0) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  IPAddress staIp = WiFi.localIP();
  if ((uint32_t)staIp == 0) return false;
  return true;
}

static void broadcastStatus() {
  if (millis() - lastBroadcast < 2000)
    return;
  lastBroadcast = millis();

  // Ограничиваем количество WS клиентов (макс 4), закрываем старые соединения
  ws.cleanupClients(4);
  DynamicJsonDocument doc(768);
  doc["uptime"] = millis() / 1000;
  doc["ip"] = wifi_getIP();
  doc["mac"] = getMacString();

  WifiState wifiSt = wifi_getState();
  bool wifiConnected = isStaReallyConnected();
  wifi_mode_t mode = WiFi.getMode();
  bool apActive = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["connected"] = wifiConnected ? 1 : 0;
  wifi["state"] = (int)wifiSt;
  wifi["ap_active"] = apActive ? 1 : 0;

  // Relay states
  JsonObject rel = doc.createNestedObject("relays");
  struct {
      const char* name;
      uint8_t idx;
  } rels[] = {
    {"pump1", 0},
    {"pump2", 1},
    {"aeration", 2},
    {"ozone", 3},
    {"filter", 4},
    {"backwash", 5}};
  for (int i = 0; i < 6; i++)
    rel[rels[i].name] = getRelayState(rels[i].idx) ? 1 : 0;

  // Tank levels (сырые значения с датчиков, см)
  extern int tank1Level, tank2Level;
  doc["tank1"] = tank1Level;
  doc["tank2"] = tank2Level;

  // Состояние автомата и таймеры фаз
  extern SystemContext systemContext;
  doc["state"] = getStateText(systemContext.currentState);
  doc["state_code"] = (int)systemContext.currentState;

  extern uint32_t currentAerationRemaining;
  extern uint32_t currentOzonationRemaining;
  extern uint32_t currentSetlingRemaining;
  extern uint32_t currentBackwashRemaining;
  extern unsigned long filterOperationStartTime;
  extern uint32_t filterCleaningInterval;
  extern SystemFlags flags;
  JsonObject timers = doc.createNestedObject("timers");
  timers["aeration"] = currentAerationRemaining;
  timers["ozonation"] = currentOzonationRemaining;
  timers["settling"] = currentSetlingRemaining;
  timers["backwash"] = currentBackwashRemaining;
  {
    uint32_t filterRemaining = 0;
    if (!flags.filterCleaningNeeded && filterOperationStartTime != 0) {
      unsigned long elapsedSec = (millis() - filterOperationStartTime) / 1000UL;
      filterRemaining = (elapsedSec < filterCleaningInterval) ? (filterCleaningInterval - elapsedSec) : 0;
    }
    timers["filterRemaining"] = filterRemaining;
  }

  // Флаги, связанные с основным экраном
  JsonObject f = doc.createNestedObject("flags");
  f["tank1Empty"] = flags.tank1Empty;
  f["tank2Empty"] = flags.tank2Empty;
  f["filterCleaningNeeded"] = flags.filterCleaningNeeded;
  f["backwashInProgress"] = flags.backwashInProgress;
  f["blinkState"] = flags.blinkState;
  f["manualMode"] = flags.manualMode;
  doc["control_mode"] = flags.manualMode ? "manual" : "auto";

  // MQTT status
  JsonObject mqtt = doc.createNestedObject("mqtt");
  mqtt["enabled"] = preferences.getBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED);
  mqtt["connected"] = vqtt_isConnected();
  mqtt["broker"] = vqtt_getBroker();
  mqtt["topic_base"] = vqtt_getTopicBase();
  // SD card presence
  extern bool sdPresent;
  doc["sd"] = sdPresent ? 1 : 0;
 
  // Emergency status
  JsonObject emergency = doc.createNestedObject("emergency");
  emergency["active"] = isInEmergencyMode();
  const char* emMsg = getEmergencyMessage();
  emergency["message"] = (emMsg && emMsg[0]) ? emMsg : "";
 
  String out;
  serializeJson(doc, out);
  // Only broadcast if there are connected websocket clients to save CPU/network
  if (ws.count() > 0) {
    ws.textAll(out);
  }
}

// Send status immediately (no rate limiting) — used after control actions
static void sendStatusNow() {
  DynamicJsonDocument doc(768);
  doc["uptime"] = millis() / 1000;
  doc["ip"] = wifi_getIP();
  doc["mac"] = getMacString();

  WifiState wifiSt = wifi_getState();
  bool wifiConnected = isStaReallyConnected();
  wifi_mode_t mode = WiFi.getMode();
  bool apActive = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["connected"] = wifiConnected ? 1 : 0;
  wifi["state"] = (int)wifiSt;
  wifi["ap_active"] = apActive ? 1 : 0;

  // Relay states
  JsonObject rel = doc.createNestedObject("relays");
  struct {
      const char* name;
      uint8_t idx;
  } rels[] = {
    {"pump1", 0},
    {"pump2", 1},
    {"aeration", 2},
    {"ozone", 3},
    {"filter", 4},
    {"backwash", 5}};
  for (int i = 0; i < 6; i++)
    rel[rels[i].name] = getRelayState(rels[i].idx) ? 1 : 0;

  // Tank levels
  extern int tank1Level, tank2Level;
  doc["tank1"] = tank1Level;
  doc["tank2"] = tank2Level;

  // Состояние автомата и таймеры
  extern SystemContext systemContext;
  doc["state"] = getStateText(systemContext.currentState);
  doc["state_code"] = (int)systemContext.currentState;

  extern uint32_t currentAerationRemaining;
  extern uint32_t currentOzonationRemaining;
  extern uint32_t currentSetlingRemaining;
  extern uint32_t currentBackwashRemaining;
  extern unsigned long filterOperationStartTime;
  extern uint32_t filterCleaningInterval;
  extern SystemFlags flags;
  JsonObject timers = doc.createNestedObject("timers");
  timers["aeration"] = currentAerationRemaining;
  timers["ozonation"] = currentOzonationRemaining;
  timers["settling"] = currentSetlingRemaining;
  timers["backwash"] = currentBackwashRemaining;
  {
    uint32_t filterRemaining = 0;
    if (!flags.filterCleaningNeeded && filterOperationStartTime != 0) {
      unsigned long elapsedSec = (millis() - filterOperationStartTime) / 1000UL;
      filterRemaining = (elapsedSec < filterCleaningInterval) ? (filterCleaningInterval - elapsedSec) : 0;
    }
    timers["filterRemaining"] = filterRemaining;
  }

  // Флаги для главного экрана
  JsonObject f = doc.createNestedObject("flags");
  f["tank1Empty"] = flags.tank1Empty;
  f["tank2Empty"] = flags.tank2Empty;
  f["filterCleaningNeeded"] = flags.filterCleaningNeeded;
  f["backwashInProgress"] = flags.backwashInProgress;
  f["blinkState"] = flags.blinkState;
  f["manualMode"] = flags.manualMode;
  doc["control_mode"] = flags.manualMode ? "manual" : "auto";
 
  // Emergency status
  JsonObject emergency = doc.createNestedObject("emergency");
  emergency["active"] = isInEmergencyMode();
  const char* emMsg = getEmergencyMessage();
  emergency["message"] = (emMsg && emMsg[0]) ? emMsg : "";
 
  String out;
  serializeJson(doc, out);
  if (ws.count() > 0) {
    ws.textAll(out);
  }
}

static bool isKnownRelay(const String& name) {
  return name == "pump1" || name == "pump2" || name == "aeration" ||
         name == "ozone" || name == "filter" || name == "backwash";
}

static bool applyRelayToggleByName(const String& name, uint16_t& opCode) {
  extern SystemFlags flags;
  opCode = 0;
  if (name == "pump1") {
    if (getRelayState(0)) {
      turnOffPump1();
      opCode = MANUAL_PUMP1_OFF;
    } else {
      turnOnPump1();
      opCode = MANUAL_PUMP1_ON;
    }
    return true;
  }
  if (name == "pump2") {
    if (getRelayState(1)) {
      turnOffPump2();
      opCode = MANUAL_PUMP2_OFF;
    } else {
      turnOnPump2();
      opCode = MANUAL_PUMP2_ON;
    }
    return true;
  }
  if (name == "aeration") {
    if (getRelayState(2)) {
      turnOffAeration();
      opCode = MANUAL_AERATION_OFF;
    } else {
      turnOnAeration();
      opCode = MANUAL_AERATION_ON;
    }
    return true;
  }
  if (name == "ozone") {
    // Озон управляет и аэрацией (как на устройстве)
    if (getRelayState(3)) {
      turnOffOzone();
      turnOffAeration();
      opCode = MANUAL_OZONE_OFF;
    } else {
      turnOnOzone();
      turnOnAeration();
      opCode = MANUAL_OZONE_ON;
    }
    return true;
  }
  if (name == "filter") {
    if (getRelayState(4)) {
      // Запрет выключения фильтра во время автоцикла (как на устройстве)
      if (flags.waterTreatmentInProgress) {
        opCode = MANUAL_FILTER_OFF;
        return true;
      }
      turnOffFilter();
      if (getRelayState(1) && !getRelayState(5)) {
        turnOffPump2();
        saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_PUMP2_OFF, SRC_WEB);
      }
      opCode = MANUAL_FILTER_OFF;
    } else {
      turnOnFilter();
      if (!getRelayState(1)) {
        turnOnPump2();
        saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_PUMP2_ON, SRC_WEB);
      }
      opCode = MANUAL_FILTER_ON;
    }
    return true;
  }
  if (name == "backwash") {
    if (getRelayState(5)) {
      turnOffBackwash();
      if (getRelayState(1) && !getRelayState(4)) {
        turnOffPump2();
        saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_PUMP2_OFF, SRC_WEB);
      }
      opCode = MANUAL_BACKWASH_OFF;
    } else {
      turnOnBackwash();
      if (!getRelayState(1)) {
        turnOnPump2();
        saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_PUMP2_ON, SRC_WEB);
      }
      opCode = MANUAL_BACKWASH_ON;
    }
    return true;
  }
  return false;
}

static bool setControlMode(const String& mode, uint16_t& opCode) {
  extern SystemFlags flags;
  extern SystemContext systemContext;
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

  if (mode == "manual") {
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
    // При входе в ручной режим останавливаем автоцикл и очищаем зависимые флаги.
    if (systemContext.currentState != STATE_IDLE) {
      changeState(STATE_IDLE);
    }
    turnOffAllRelays();
    FLAGS_LOCK();
    flags.waterTreatmentInProgress = 0;
    flags.backwashInProgress = 0;
    FLAGS_UNLOCK();
    opCode = MANUAL_SET_MANUAL;
    return true;
  }

  if (mode == "auto") {
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
      Serial.printf("[WebServer] Restored state %d after manual mode\n", savedStateBeforeManual);
    }

    savedStateBeforeManual = STATE_IDLE;
    opCode = MANUAL_SET_AUTOMATIC;
    return true;
  }

  return false;
}

// Periodic monitor: log and optionally broadcast meta info every 10s (extended)
static unsigned long lastMonitorLog = 0;
void webMonitorPeriodic() {
  if (millis() - lastMonitorLog < 10000)
    return;
  lastMonitorLog = millis();
  unsigned int freeHeap = ESP.getFreeHeap();
  unsigned int maxAlloc = ESP.getMaxAllocHeap();
  int clients = ws.count();
  int wifiStateInt = (int)wifi_getState();
  int rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
  Serial.printf("[WebMon] freeHeap=%u maxAlloc=%u ws_clients=%d wifi_state=%d rssi=%d\n", freeHeap, maxAlloc, clients, wifiStateInt, rssi);

  if (clients > 0) {
    DynamicJsonDocument meta(256);
    meta["meta"] = true;
    meta["freeHeap"] = freeHeap;
    meta["maxAlloc"] = maxAlloc;
    meta["wsClients"] = clients;
    meta["wifiState"] = wifiStateInt;
    meta["rssi"] = rssi;
    String out;
    serializeJson(meta, out);
    ws.textAll(out);
  }
}

// Minimal rescue page shown when index.html is absent from SD card.
// Provides a file upload form so the user can install the UI without
// physically removing the SD card.
static const char RESCUE_PAGE[] PROGMEM =
  "<!doctype html><html><head><meta charset=utf-8>"
  "<meta name=viewport content='width=device-width,initial-scale=1'>"
  "<title>WSystem Setup</title>"
  "<style>body{background:#020617;color:#e5e7eb;font-family:sans-serif;padding:20px}"
  "h2{color:#22d3ee}code{color:#fdba74}input,button{display:block;margin:8px 0;"
  "padding:8px;width:100%;box-sizing:border-box;background:#0f172a;color:#e5e7eb;"
  "border:1px solid rgba(148,163,184,.4);border-radius:4px}button{background:#0891b2;"
  "cursor:pointer}button:hover{background:#0e7490}.note{color:#9ca3af;font-size:13px}"
  "</style></head><body>"
  "<h2>WSystem &#8212; Setup</h2>"
  "<p>&#x26A0; Файл <code>index.html</code> не найден на SD-карте.</p>"
  "<p>Загрузите файл ниже (URL: <code>POST /api/sd_upload?token=ТВОЙ_ТОКЕН</code>):</p>"
  "<form method=POST action='/api/sd_upload' enctype='multipart/form-data'>"
  "<input type=password name=token id=t placeholder='Admin token'>"
  "<input type=file name=file accept='.html'>"
  "<button type=submit>Загрузить на SD</button></form>"
  "<p class=note>После загрузки index.html перезагрузите страницу.<br>"
  "SD: %s | IP: %s</p></body></html>";

// root page handler (serves index.html from SD/SPIFFS)
static void handleRoot(AsyncWebServerRequest* request) {
  IPAddress clientIP = request->client()->remoteIP();
#ifdef DEBUG_SERIAL
  Serial.printf("[Web] GET / from %s\n", clientIP.toString().c_str());
#endif
  if (!sendStaticResource(request, "/index.html", "text/html")) {
    // SD card missing index.html – show embedded rescue/upload page
    extern bool sdPresent;
    char page[sizeof(RESCUE_PAGE) + 32];
    snprintf(page, sizeof(page), RESCUE_PAGE,
             sdPresent ? "OK" : "NOT FOUND",
             wifi_getIP().c_str());
    request->send(200, "text/html", page);
  }
}

// JSON status API
static void handleStatus(AsyncWebServerRequest* request) {
  IPAddress clientIP = request->client()->remoteIP();
  Serial.printf("[Web] GET /status from %s\n", clientIP.toString().c_str());

  String ip = wifi_getIP();
  DynamicJsonDocument doc(768);
  doc["ip"] = ip;
  doc["mac"] = getMacString();
  doc["uptime"] = millis() / 1000;

  WifiState wifiSt = wifi_getState();
  bool wifiConnected = isStaReallyConnected();
  wifi_mode_t mode = WiFi.getMode();
  bool apActive = (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["connected"] = wifiConnected ? 1 : 0;
  wifi["state"] = (int)wifiSt;
  wifi["ap_active"] = apActive ? 1 : 0;
  JsonObject arr = doc.createNestedObject("relays");
  struct {
      const char* name;
      uint8_t idx;
  } rel[] = {
    {"pump1", 0},
    {"pump2", 1},
    {"aeration", 2},
    {"ozone", 3},
    {"filter", 4},
    {"backwash", 5}};
  for (int i = 0; i < 6; i++) {
    bool st = getRelayState(rel[i].idx);
    arr[rel[i].name] = st ? 1 : 0;
  }
  // add tank levels if available
  extern int tank1Level, tank2Level;
  doc["tank1"] = tank1Level;
  doc["tank2"] = tank2Level;

  // state and timers
  extern SystemContext systemContext;
  doc["state"] = getStateText(systemContext.currentState);
  doc["state_code"] = (int)systemContext.currentState;

  extern uint32_t currentAerationRemaining;
  extern uint32_t currentOzonationRemaining;
  extern uint32_t currentSetlingRemaining;
  extern uint32_t currentBackwashRemaining;
  extern unsigned long filterOperationStartTime;
  extern uint32_t filterCleaningInterval;
  extern SystemFlags flags;
  JsonObject timers = doc.createNestedObject("timers");
  timers["aeration"] = currentAerationRemaining;
  timers["ozonation"] = currentOzonationRemaining;
  timers["settling"] = currentSetlingRemaining;
  timers["backwash"] = currentBackwashRemaining;
  {
    uint32_t filterRemaining = 0;
    if (!flags.filterCleaningNeeded && filterOperationStartTime != 0) {
      unsigned long elapsedSec = (millis() - filterOperationStartTime) / 1000UL;
      filterRemaining = (elapsedSec < filterCleaningInterval) ? (filterCleaningInterval - elapsedSec) : 0;
    }
    timers["filterRemaining"] = filterRemaining;
  }

  // flags for main screen
  JsonObject f = doc.createNestedObject("flags");
  f["tank1Empty"] = flags.tank1Empty;
  f["tank2Empty"] = flags.tank2Empty;
  f["filterCleaningNeeded"] = flags.filterCleaningNeeded;
  f["backwashInProgress"] = flags.backwashInProgress;
  f["blinkState"] = flags.blinkState;
  f["manualMode"] = flags.manualMode;
  doc["control_mode"] = flags.manualMode ? "manual" : "auto";

  // MQTT status
  JsonObject mqtt = doc.createNestedObject("mqtt");
  if (prefsIsAvailable()) {
    mqtt["enabled"] = preferences.getBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED);
    mqtt["connected"] = vqtt_isConnected();
    mqtt["broker"] = vqtt_getBroker();
    mqtt["topic_base"] = vqtt_getTopicBase();
  } else {
    mqtt["enabled"] = 0;
    mqtt["connected"] = 0;
    mqtt["broker"] = "";
    mqtt["topic_base"] = "";
  }
  extern bool sdPresent;
  doc["sd"] = sdPresent ? 1 : 0;

  JsonObject emergency = doc.createNestedObject("emergency");
  emergency["active"] = isInEmergencyMode();
  const char* emMsg = getEmergencyMessage();
  emergency["message"] = (emMsg && emMsg[0]) ? emMsg : "";

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

static void handleControl(AsyncWebServerRequest* request) {
  extern SystemFlags flags;

  if (!request->hasParam("op")) {
    request->send(400, "text/plain", "bad request");
    return;
  }

  String op = request->getParam("op")->value();

  if (op == "reset_emergency") {
    resetEmergency();
    request->send(200, "text/plain", "ok");
    sendStatusNow();
    return;
  }

  if (op == "set_mode") {
    String mode = request->hasParam("mode") ? request->getParam("mode")->value() : "";
    uint16_t modeCode = 0;
    if (!setControlMode(mode, modeCode)) {
      request->send(400, "text/plain", "unknown mode");
      return;
    }
    saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, modeCode, SRC_WEB);
    request->send(200, "text/plain", "ok");
    sendStatusNow();
    return;
  }

  if (!request->hasParam("name")) {
    request->send(400, "text/plain", "bad request");
    return;
  }

  String name = request->getParam("name")->value();
  if (!isKnownRelay(name)) {
    request->send(400, "text/plain", "unknown relay");
    return;
  }
  if (op != "toggle") {
    request->send(400, "text/plain", "unsupported op");
    return;
  }

  if (!flags.manualMode) {
    saveEventLog(LOG_WARNING, EVENT_MANUAL_OPERATION, MANUAL_COMMAND_REJECTED_AUTO, SRC_WEB);
    request->send(403, "text/plain", "manual control disabled in auto mode");
    sendStatusNow();
    return;
  }

  // Проверка токена для HTTP ручных команд.
  if (!request->hasParam("token")) {
    request->send(401, "text/plain", "Unauthorized: token missing");
    return;
  }
  String token = request->getParam("token")->value();
  String storedToken = safeGetPref(PREF_KEY_ADMIN_TOKEN, "");
  if (storedToken.length() == 0 || token != storedToken) {
    request->send(401, "text/plain", "Unauthorized: invalid token");
    return;
  }

  uint16_t opCode = 0;
  if (!applyRelayToggleByName(name, opCode) || opCode == 0) {
    request->send(400, "text/plain", "unsupported command");
    return;
  }

  saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, opCode, SRC_WEB);
  request->send(200, "text/plain", "ok");
  sendStatusNow();
}

static void handleLegacy(AsyncWebServerRequest* request) {
  IPAddress clientIP = request->client()->remoteIP();
  Serial.printf("[Web] GET /legacy from %s\n", clientIP.toString().c_str());

  unsigned int freeHeap = ESP.getFreeHeap();
  if (freeHeap < 10000) {
    Serial.printf("[Web] /legacy unavailable - low heap: %u\n", freeHeap);
    request->send(503, "text/plain", "Service Unavailable: low memory");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("text/html");
  if (!response) {
    Serial.println("[Web] /legacy: beginResponseStream failed");
    request->send(500, "text/plain", "Internal Server Error");
    return;
  }

  response->printf("<!doctype html><html><head><meta name=viewport content='width=device-width, initial-scale=1'><title>WSystem - Legacy</title></head><body>");
  response->printf("<h3>ESP32 Water System - Legacy UI</h3>");
  response->printf("<div>IP: %s<br>MAC: %s</div>", wifi_getIP().c_str(), getMacString().c_str());
  response->printf("<div><h4>Relays</h4>");
  struct {
      const char* name;
      uint8_t idx;
  } rels[] = {
    {"pump1", 0},
    {"pump2", 1},
    {"aeration", 2},
    {"ozone", 3},
    {"filter", 4},
    {"backwash", 5}};
  for (int i = 0; i < 6; i++) {
    const char* nm = rels[i].name;
    const char* state = getRelayState(rels[i].idx) ? "ON" : "OFF";
    response->printf("<div>%s: %s - <a href=\"/control?name=%s&op=toggle\">Toggle</a></div>", nm, state, nm);
  }
  response->printf("</div>");
  response->printf("<div style=\"margin-top:10px;\"><a href=\"/\">Back</a> | <a href=\"/status\">JSON status</a></div>");
  response->printf("<div style=\"margin-top:8px;\"><a href=\"/api/sd_files\">List SD files (JSON)</a></div>");
  response->printf("<div style=\"margin-top:4px;\"><a href=\"/api/sd_file?name=/events.log\">Download current log</a></div>");
  response->printf("<div style=\"margin-top:4px;font-size:smaller;color:gray\">Prune operation available in SPA or via POST to /api/logs</div>");
  response->printf("</body></html>");
  request->send(response);
}

void onWsEvent(AsyncWebSocket* serverWS, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[Web] WS client connected: %u\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[Web] WS client disconnected: %u\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;

    // Ограничение размера WS фрейма (1 КБ) для защиты от OOM-атак
    if (len > 1024) {
      client->text("{\"responseTo\":\"error\",\"message\":\"payload too large\"}");
      return;
    }

    if (info->final && info->opcode == WS_TEXT) {
      // Copy payload into a String
      String msg = "";
      for (size_t i = 0; i < len; ++i)
        msg += (char)data[i];

      // Parse JSON and handle commands
      DynamicJsonDocument doc(512);
      DeserializationError err = deserializeJson(doc, msg);
      if (err) {
        client->text("{\"responseTo\":\"error\",\"message\":\"invalid json\"}");
        return;
      }

      const char* typeStr = doc["type"];
      if (!typeStr) {
        client->text("{\"responseTo\":\"error\",\"message\":\"missing type\"}");
        return;
      }

      if (strcmp(typeStr, "control") == 0) {
        extern SystemFlags flags;
        const char* op = doc["op"];
        if (!op) {
          client->text("{\"responseTo\":\"control\",\"ok\":false,\"message\":\"missing op\"}");
          return;
        }

        String sop = String(op);

        if (sop == "reset_emergency") {
          resetEmergency();
          client->text("{\"responseTo\":\"control\",\"ok\":true,\"message\":\"ok\"}");
          sendStatusNow();
          return;
        }

        if (sop == "set_mode") {
          const char* mode = doc["mode"];
          uint16_t modeCode = 0;
          if (!mode || !setControlMode(String(mode), modeCode)) {
            client->text("{\"responseTo\":\"control\",\"ok\":false,\"message\":\"unknown mode\"}");
            return;
          }
          saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, modeCode, SRC_WEB);
          client->text("{\"responseTo\":\"control\",\"ok\":true,\"message\":\"ok\"}");
          sendStatusNow();
          return;
        }

        const char* name = doc["name"];
        if (!name) {
          client->text("{\"responseTo\":\"control\",\"ok\":false,\"message\":\"missing name\"}");
          return;
        }

        String sname = String(name);
        if (!isKnownRelay(sname) || sop != "toggle") {
          client->text("{\"responseTo\":\"control\",\"ok\":false,\"message\":\"unsupported command\"}");
          return;
        }

        if (!flags.manualMode) {
          saveEventLog(LOG_WARNING, EVENT_MANUAL_OPERATION, MANUAL_COMMAND_REJECTED_AUTO, SRC_WEB);
          client->text("{\"responseTo\":\"control\",\"ok\":false,\"message\":\"manual control disabled in auto mode\"}");
          sendStatusNow();
          return;
        }

        uint16_t opCode = 0;
        if (!applyRelayToggleByName(sname, opCode) || opCode == 0) {
          client->text("{\"responseTo\":\"control\",\"ok\":false,\"message\":\"unsupported command\"}");
          return;
        }

        saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, opCode, SRC_WEB);
        client->text("{\"responseTo\":\"control\",\"ok\":true,\"message\":\"ok\"}");
        sendStatusNow();
        return;
      } else if (strcmp(typeStr, "wifi") == 0) {
        const char* ssid = doc["ssid"];
        const char* pass = doc["pass"];
        const char* token = doc["token"];
        if (!ssid) {
          client->text("{\"responseTo\":\"wifi\",\"ok\":false,\"message\":\"no ssid\"}");
          return;
        }
        String storedToken = safeGetPref(PREF_KEY_ADMIN_TOKEN, "");
        if (storedToken.length() == 0) {
          client->text("{\"responseTo\":\"wifi\",\"ok\":false,\"message\":\"no admin token set\"}");
          return;
        }
        if (!token || String(token) != storedToken) {
          client->text("{\"responseTo\":\"wifi\",\"ok\":false,\"message\":\"invalid token\"}");
          return;
        }
        wifi_setCredentials(String(ssid), String(pass ? pass : ""));
        client->text("{\"responseTo\":\"wifi\",\"ok\":true,\"queued\":true}");
        return;
      } else {
        client->text("{\"responseTo\":\"error\",\"message\":\"unknown type\"}");
        return;
      }
    }
  }
}

static void handlePostWifiBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  IPAddress clientIP = request->client()->remoteIP();
  Serial.printf("[Web] POST /api/wifi from %s len=%d\n", clientIP.toString().c_str(), (int)len);
  if (len == 0) {
    Serial.println("[Web] /api/wifi empty body");
    request->send(400, "application/json", "{\"error\":\"empty body\"}");
    return;
  }
  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    Serial.printf("[Web] /api/wifi invalid json: %s\n", err.c_str());
    request->send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }
  if (!prefsIsAvailable()) {
    Serial.println("[Web] /api/wifi called but preferences unavailable");
    request->send(503, "application/json", "{\"error\":\"preferences unavailable\"}");
    return;
  }

  const char* ssid = doc["ssid"];
  const char* pass = doc["pass"];
  const char* token = doc["token"];
  bool lang_en = doc["lang_en"] | false;
  if (!ssid) {
    Serial.println("[Web] /api/wifi missing ssid");
    request->send(400, "application/json", "{\"error\":\"no ssid\"}");
    return;
  }
  String storedToken = safeGetPref(PREF_KEY_ADMIN_TOKEN, "");
  if (storedToken.length() == 0) {
    Serial.println("[Web] /api/wifi no admin token set");
    request->send(403, "application/json", "{\"error\":\"no admin token set\"}");
    return;
  }
  if (!token || String(token) != storedToken) {
    Serial.println("[Web] /api/wifi invalid token");
    request->send(403, "application/json", "{\"error\":\"invalid token\"}");
    return;
  }
  wifi_setCredentials(String(ssid), String(pass ? pass : ""));
  // update language preference if requested
  if (lang_en) {
    setLanguageEnglish(true);
  } else {
    setLanguageEnglish(false);
  }
  // Ensure WiFi credentials are committed to NVS
  preferences.end();
  preferences.begin("water_sys");
  Serial.printf("[Web] /api/wifi accepted ssid=%s lang_en=%d\n", ssid, lang_en);
  request->send(200, "application/json", "{\"ok\":true, \"queued\":true}\n");
}

// API: Change admin/master token
static void handlePostAdminTokenBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  IPAddress clientIP = request->client()->remoteIP();
  Serial.printf("[Web] POST /api/admin_token from %s len=%d\n", clientIP.toString().c_str(), (int)len);
  if (!prefsIsAvailable()) {
    Serial.println("[Web] admin token change: preferences unavailable");
    request->send(503, "application/json", "{\"error\":\"preferences unavailable\"}");
    return;
  }

  if (len == 0) {
    Serial.println("[Web] admin token change: empty body");
    request->send(400, "application/json", "{\"error\":\"empty body\"}");
    return;
  }
  // log entire body to aid debugging (encoded JSON may contain unescaped data)
  String body((char*)data, len);
  Serial.printf("[Web] admin token change body: %s\n", body.c_str());

  // use a larger buffer to avoid surprises and dump result for debugging
  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, data, len);
  if (!err) {
    Serial.print("[Web] admin token parsed JSON: ");
    serializeJson(doc, Serial);
    Serial.println();
  }
  if (err) {
    Serial.printf("[Web] admin token change: invalid json: %s\n", err.c_str());
    request->send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }

  JsonVariant token_admin_var = doc["token_admin"];
  JsonVariant new_token_var = doc["new_token"];

  if (new_token_var.isNull()) {
    Serial.println("[Web] admin token change: missing new_token field");
    request->send(400, "application/json", "{\"error\":\"new_token required\"}");
    return;
  }

  const char* token_admin = token_admin_var.as<const char*>();
  const char* new_token = new_token_var.as<const char*>();

  Serial.printf("[Web] admin token change request token_admin_len=%u new_token_len=%u\n",
                token_admin ? strlen(token_admin) : 0,
                new_token ? strlen(new_token) : 0);
  // Also print actual strings (beware of sensitive info in logs)
  if (token_admin) Serial.printf("[Web] token_admin='%s'\n", token_admin);
  if (new_token) Serial.printf("[Web] new_token='%s'\n", new_token);
  String storedToken = preferences.getString(PREF_KEY_ADMIN_TOKEN, "");
  if (storedToken.length() == 0) {
    Serial.println("[Web] admin token change: no admin token set");
    request->send(403, "application/json", "{\"error\":\"no admin token set\"}");
    return;
  }
  if (!token_admin || String(token_admin) != storedToken) {
    Serial.println("[Web] admin token change: invalid current token");
    request->send(403, "application/json", "{\"error\":\"invalid token\"}");
    return;
  }
  size_t nt_len = strlen(new_token);
  if (nt_len == 0 || nt_len > ADMIN_TOKEN_MAX_LEN) {
    char err_buf[64];
    snprintf(err_buf, sizeof(err_buf), "{\"error\":\"new_token length must be 1..%d\"}", ADMIN_TOKEN_MAX_LEN);
    request->send(400, "application/json", err_buf);
    return;
  }
  // enforce alphanumeric characters only
  for (size_t _i = 0; _i < nt_len; ++_i) {
    char _c = new_token[_i];
    if (!((_c >= '0' && _c <= '9') || (_c >= 'A' && _c <= 'Z') || (_c >= 'a' && _c <= 'z'))) {
      request->send(400, "application/json", "{\"error\":\"new_token must be alphanumeric\"}");
      return;
    }
  }
  bool ok = preferences.putString(PREF_KEY_ADMIN_TOKEN, String(new_token));
  Serial.printf("[Web] admin token putString result=%d\n", ok);
  saveEventLog(LOG_INFO, EVENT_SETTINGS_CHANGE, 0);
  request->send(200, "application/json", "{\"ok\":true}");
}

static void handleGetMqtt(AsyncWebServerRequest* request) {
  if (!prefsIsAvailable()) {
    request->send(503, "application/json", "{\"error\":\"preferences unavailable\"}");
    return;
  }
  DynamicJsonDocument doc(512);
  doc["enabled"] = preferences.getBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED);
  doc["broker"] = safeGetPref(PREF_KEY_MQTT_BROKER, "");
  doc["port"] = preferences.getUInt(PREF_KEY_MQTT_PORT, DEFAULT_MQTT_PORT);
  doc["user"] = safeGetPref(PREF_KEY_MQTT_USER, "");
  doc["client_id"] = safeGetPref(PREF_KEY_MQTT_CLIENT_ID, "");
  doc["topic_base"] = safeGetPref(PREF_KEY_MQTT_TOPIC_BASE, DEFAULT_MQTT_TOPIC_BASE);
  doc["interval"] = preferences.getUInt(PREF_KEY_MQTT_PUB_INTERVAL, 10);
  doc["secure"] = preferences.getBool(PREF_KEY_MQTT_SECURE, DEFAULT_MQTT_SECURE);
  doc["tls_port"] = preferences.getUInt(PREF_KEY_MQTT_TLS_PORT, DEFAULT_MQTT_TLS_PORT);
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

static void handlePostMqttBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (!prefsIsAvailable()) {
    request->send(503, "application/json", "{\"error\":\"preferences unavailable\"}");
    return;
  }
  IPAddress clientIP = request->client()->remoteIP();
  Serial.printf("[Web] POST /api/mqtt from %s len=%d\n", clientIP.toString().c_str(), (int)len);
  if (len == 0) {
    Serial.println("[Web] /api/mqtt empty body");
    request->send(400, "application/json", "{\"error\":\"empty body\"}");
    return;
  }
  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    Serial.printf("[Web] /api/mqtt invalid json: %s\n", err.c_str());
    request->send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }
  // Apply settings
  bool enabled = doc["enabled"] | false;
  const char* broker = doc["broker"] | "";
  int port = doc["port"] | DEFAULT_MQTT_PORT;
  const char* user = doc["user"] | "";
  const char* pass = doc["pass"] | "";
  const char* client_id = doc["client_id"] | "";
  const char* topic_base = doc["topic_base"] | DEFAULT_MQTT_TOPIC_BASE;
  int interval = doc["interval"] | 10;
  bool secure = doc["secure"] | false;
  int tls_port = doc["tls_port"] | DEFAULT_MQTT_TLS_PORT;
  preferences.putBool(PREF_KEY_MQTT_ENABLED, enabled);
  preferences.putString(PREF_KEY_MQTT_BROKER, broker);
  preferences.putUInt(PREF_KEY_MQTT_PORT, (uint32_t)port);
  preferences.putString(PREF_KEY_MQTT_USER, user);
  if (strlen(pass) > 0)
    preferences.putString(PREF_KEY_MQTT_PASS, pass);
  preferences.putString(PREF_KEY_MQTT_CLIENT_ID, client_id);
  preferences.putString(PREF_KEY_MQTT_TOPIC_BASE, topic_base);
  preferences.putUInt(PREF_KEY_MQTT_PUB_INTERVAL, (uint32_t)interval);
  preferences.putBool(PREF_KEY_MQTT_SECURE, secure);
  preferences.putUInt(PREF_KEY_MQTT_TLS_PORT, (uint32_t)tls_port);
  saveEventLog(LOG_INFO, EVENT_SETTINGS_CHANGE, 0);
  request->send(200, "application/json", "{\"ok\":true}\n");
  // Re-init VQTT so new settings are applied immediately
  initVQTT();
}

void initWebServer() {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/control", HTTP_GET, handleControl);
  server.on("/legacy", HTTP_GET, handleLegacy);
  // Admin UI moved to SPIFFS (data/logfilter.html) — embedded copy removed to save flash

  server.on("/admin/log_filter", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!sendStaticResource(request, "/logfilter.html", "text/html")) {
      request->send(404, "text/plain", "logfilter.html not found");
    }
  });
  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest* request) {
    // Response is sent by handlePostWifiBody()
  }, NULL, handlePostWifiBody);
  // language preference API
  server.on("/api/lang", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(128);
    doc["english"] = isEnglish();
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });
  server.on("/api/lang", HTTP_POST, [](AsyncWebServerRequest* request) {
    // Response is sent by body handler
  }, NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total){
    if(len==0) { request->send(400,"application/json","{\"error\":\"empty\"}"); return; }
    DynamicJsonDocument doc(128);
    DeserializationError err = deserializeJson(doc, data, len);
    if(err){ request->send(400,"application/json","{\"error\":\"invalid\"}"); return; }
    bool eng = doc["lang_en"] | false;
    setLanguageEnglish(eng);
    DynamicJsonDocument resp(64);
    resp["ok"] = true;
    resp["english"] = eng;
    String out; serializeJson(resp,out);
    request->send(200,"application/json",out);
  });
  // API: change Admin (master) token via Web UI - POST { token_admin: <current>, new_token: <new> }
  // Важно: реальный ответ формируется в handlePostAdminTokenBody(), поэтому onRequest здесь не отправляет ответ.
  server.on("/api/admin_token", HTTP_POST, [](AsyncWebServerRequest* request) {
    // Ничего не отправляем здесь, обработка и send() происходят в handlePostAdminTokenBody().
  }, NULL, handlePostAdminTokenBody);

  // Register MQTT handlers
  server.on("/api/mqtt", HTTP_GET, [](AsyncWebServerRequest* request) { handleGetMqtt(request); });
  server.on("/api/mqtt", HTTP_POST, [](AsyncWebServerRequest* request) {
    // Response is sent by handlePostMqttBody()
  }, NULL, handlePostMqttBody);

  // Log filter API: GET to read mask and options, POST to set mask (requires admin token)
  server.on("/api/log_filter", HTTP_GET, [](AsyncWebServerRequest* request) {
    uint32_t mask = preferences.getULong(PREF_KEY_LOG_FILTER, (uint32_t)PREF_KEY_LOG_FILTER_DEFAULT);
    DynamicJsonDocument doc(512);
    doc["mask"] = mask;
    JsonArray opts = doc.createNestedArray("options");
    struct {
        const char* label;
        uint32_t bit;
    } optsList[] = {
      {"Wi-Fi events", LOG_FILTER_WIFI},
      {"Manual ops", LOG_FILTER_MANUAL},
      {"Emergency", LOG_FILTER_EMERGENCY},
      {"Watchdog", LOG_FILTER_WATCHDOG},
      {"Sensor Err", LOG_FILTER_SENSOR},
      {"Settings", LOG_FILTER_SETTINGS},
      {"Web/API", LOG_FILTER_WEB},
      {"State Chg", LOG_FILTER_STATECHANGE},
      {"Errors only", LOG_FILTER_ERRORS}};
    for (int i = 0; i < (int)(sizeof(optsList) / sizeof(optsList[0])); ++i) {
      JsonObject o = opts.createNestedObject();
      o["label"] = optsList[i].label;
      o["bit"] = optsList[i].bit;
      o["on"] = (mask & optsList[i].bit) ? true : false;
    }
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  server.on("/api/log_filter", HTTP_POST, [](AsyncWebServerRequest* request) {
    // Response is sent by body handler
  }, NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (len == 0) { request->send(400, "application/json", "{\"error\":\"empty body\"}"); return; }
    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) { request->send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    const char* token = doc["token"];
    if (!token) { request->send(403, "application/json", "{\"error\":\"token required\"}"); return; }
    String storedToken = preferences.getString(PREF_KEY_ADMIN_TOKEN, "");
    if (storedToken.length() == 0) { request->send(403, "application/json", "{\"error\":\"no admin token set\"}"); return; }
    if (String(token) != storedToken) { request->send(403, "application/json", "{\"error\":\"invalid token\"}"); return; }
    if (doc.containsKey("mask")) {
      uint32_t mask = doc["mask"] | 0;
      preferences.putULong(PREF_KEY_LOG_FILTER, mask);
      String resp = "{\"ok\":true,\"mask\":" + String(mask) + "}";
      request->send(200, "application/json", resp);
      Serial.printf("[Web] /api/log_filter updated mask=%lu\n", mask);
      return;
    }
    request->send(400, "application/json", "{\"error\":\"no mask provided\"}"); });

  // Export logs to SD card (returns {"ok":true} if successful)
  server.on("/api/export_logs", HTTP_GET, [](AsyncWebServerRequest* request) {
    bool ok = exportLogsToSD();
    DynamicJsonDocument doc(128);
    doc["ok"] = ok;
    if (!ok) {
      extern bool sdPresent;
      if (!sdPresent) {
        doc["error"] = "no sd card";
      } else {
        doc["error"] = "write failure";
      }
    }
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // List files on SD card
  server.on("/api/sd_files", HTTP_GET, [](AsyncWebServerRequest* request) {
    extern bool sdPresent;
    int count = 0;

    // Build the JSON response manually to reduce memory pressure
    String out = "{\"files\": [";
    bool first = true;

    if (sdPresent) {
      // serialize with other SD users
      // acquire spinlock
      WDT_RESET();
      // open root of SD card; the mount point itself is "/sd" so we want
      // the directory inside it, not a literal "/sd" subdirectory.
      File root = SD.open("/");
      if (root) {
        File file = root.openNextFile();
        while (file) {
          String entry = String(file.name());
          // strip leading mount prefix if present (e.g. "/sd/")
          if (entry.startsWith("/sd/")) entry = entry.substring(4);
          entry += " (" + String(file.size()) + " bytes)";
          if (!first) out += ',';
          first = false;
          out += '"';
          out += entry;
          out += '"';
          file.close();
          count++;
          WDT_RESET();
          delay(1);
          file = root.openNextFile();
        }
        root.close();
      }
    }

    out += "]}"
;
    Serial.printf("[SD] /api/sd_files returned %d entries\n", count);
    request->send(200, "application/json", out);
  });

  // Download a single SD file by name
  server.on("/api/sd_file", HTTP_GET, [](AsyncWebServerRequest* request) {
    extern bool sdPresent;
    if (!request->hasParam("name")) {
      request->send(400, "application/json", "{\"error\":\"name required\"}");
      return;
    }
    String name = request->getParam("name")->value();
    if (name.indexOf("..") >= 0) {
      request->send(400, "application/json", "{\"error\":\"invalid name\"}");
      return;
    }
    if (!sdPresent) {
      request->send(404, "text/plain", "no sd card");
      return;
    }
    String path = name;
    if (!path.startsWith("/")) path = "/" + path;
    if (!SD.exists(path)) {
      request->send(404, "text/plain", "not found");
      return;
    }
    request->send(SD, path);
  });

  // Manage SD files (delete)
  server.on("/api/sd_files", HTTP_POST, [](AsyncWebServerRequest* request) {
    // Response is sent by body handler
  }, NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      if (len == 0) { request->send(400, "application/json", "{\"error\":\"empty body\"}"); return; }
      DynamicJsonDocument doc(256);
      DeserializationError err = deserializeJson(doc, data, len);
      if (err) { request->send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
      const char* token = doc["token"];
      if (!token) { request->send(403, "application/json", "{\"error\":\"token required\"}"); return; }
      String storedToken = preferences.getString(PREF_KEY_ADMIN_TOKEN, "");
      if (storedToken.length() == 0) { request->send(403, "application/json", "{\"error\":\"no admin token set\"}"); return; }
      if (String(token) != storedToken) { request->send(403, "application/json", "{\"error\":\"invalid token\"}"); return; }
      const char* action = doc["action"];
      if (!action) { request->send(400, "application/json", "{\"error\":\"no action\"}"); return; }
      String act = String(action);
      if (act == "delete") {
        const char* name = doc["name"];
        if (!name) { request->send(400, "application/json", "{\"error\":\"no name\"}"); return; }
        String fname = String(name);
        if (fname.indexOf("..") >= 0) { request->send(400, "application/json", "{\"error\":\"invalid name\"}"); return; }
        if (!fname.startsWith("/")) fname = "/" + fname;
        bool ok = false;
        extern bool sdPresent;
        if (sdPresent && SD.exists(fname)) {
          ok = SD.remove(fname);
        }
        DynamicJsonDocument resp(128);
        resp["ok"] = ok;
        if (!ok) resp["error"] = "delete failed";
        String out;
        serializeJson(resp, out);
        request->send(200, "application/json", out);
        return;
      } else {
        request->send(400, "application/json", "{\"error\":\"unknown action\"}");
        return;
      }
    });

  // Export readable logs
  server.on("/logs.txt", HTTP_GET, [](AsyncWebServerRequest* request) {
    String txt = exportEventsAsText();
    request->send(200, "text/plain", txt);
  });


  // Clear/prune logs (POST {"action":"clear"|"prune","token":"...",days:<n>})
  server.on("/api/logs", HTTP_POST, [](AsyncWebServerRequest* request) {
    // Response is sent by body handler
  }, NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (len == 0) { request->send(400, "application/json", "{\"error\":\"empty body\"}"); return; }
    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) { request->send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    const char* token = doc["token"];
    if (!token) { request->send(403, "application/json", "{\"error\":\"token required\"}"); return; }
    String storedToken = preferences.getString(PREF_KEY_ADMIN_TOKEN, "");
    if (storedToken.length() == 0) { request->send(403, "application/json", "{\"error\":\"no admin token set\"}"); return; }
    if (String(token) != storedToken) { request->send(403, "application/json", "{\"error\":\"invalid token\"}"); return; }
    const char* action = doc["action"];
    if (!action) { request->send(400, "application/json", "{\"error\":\"no action\"}"); return; }
    String act = String(action);
    if (act == "clear") {
      clearEventLogs();
      request->send(200, "application/json", "{\"ok\":true}");
      return;
    } else if (act == "prune") {
      uint32_t days = doc["days"] | 0;
      uint32_t secs = days * 86400UL;
      pruneEventLogs(secs);
      request->send(200, "application/json", "{\"ok\":true}");
      return;
    } else {
      request->send(400, "application/json", "{\"error\":\"unknown action\"}");
      return;
    } });

  // API: Get/Set timezone (GET returns {"timezone": <hours>}, POST requires admin token)
  server.on("/api/timezone", HTTP_GET, [](AsyncWebServerRequest* request) {
    extern SafetySettings safetySettings;
    DynamicJsonDocument doc(128);
    doc["timezone"] = safetySettings.timeZoneOffset;
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });
  server.on("/api/timezone", HTTP_POST, [](AsyncWebServerRequest* request) {
    // Response is sent by body handler
  }, NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (len == 0) { request->send(400, "application/json", "{\"error\":\"empty body\"}"); return; }
    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) { request->send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    const char* token = doc["token"];
    if (!token) { request->send(403, "application/json", "{\"error\":\"token required\"}"); return; }
    String storedToken = preferences.getString(PREF_KEY_ADMIN_TOKEN, "");
    if (storedToken.length() == 0) { request->send(403, "application/json", "{\"error\":\"no admin token set\"}"); return; }
    if (String(token) != storedToken) { request->send(403, "application/json", "{\"error\":\"invalid token\"}"); return; }
    
    extern SafetySettings safetySettings;
    extern void recalculateRTCForTimezone(int8_t, int8_t);
    extern void saveEventLog(LogLevel, uint8_t, uint16_t);
    
    int8_t newTz = doc["timezone"] | 0;
    if (newTz < -12 || newTz > 12) { 
      request->send(400, "application/json", "{\"error\":\"timezone must be -12 to +12\"}"); 
      return; 
    }
    
    int8_t oldTz = safetySettings.timeZoneOffset;
    if (oldTz != newTz) {
      recalculateRTCForTimezone(oldTz, newTz);
      safetySettings.timeZoneOffset = newTz;
      saveTimeZoneSetting();
      saveEventLog(LOG_INFO, EVENT_TIMEZONE_CHANGED, newTz + 128);
      Serial.printf("[Web] Timezone changed from UTC%+d to UTC%+d\n", oldTz, newTz);
    }
    
    DynamicJsonDocument resp(128);
    resp["ok"] = true;
    resp["timezone"] = newTz;
    String out; serializeJson(resp, out);
    request->send(200, "application/json", out);
  });

  // SD file upload endpoint — POST /api/sd_upload?token=xxx  (multipart/form-data, field "file")
  static File sdUploadFile;
  static bool sdUploadOk = false;
  server.on("/api/sd_upload", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      if (!sdUploadOk) {
        request->send(403, "application/json", "{\"error\":\"unauthorized or SD missing\"}");
      } else {
        request->send(200, "application/json", "{\"ok\":true}");
      }
      sdUploadOk = false;
    },
    [&sdUploadFile, &sdUploadOk](AsyncWebServerRequest* request, const String& filename,
                                  size_t index, uint8_t* data, size_t len, bool final) {
      extern bool sdPresent;
      if (index == 0) {
        sdUploadOk = false;
        // Validate token from URL query param or form field
        String tok = "";
        if (request->hasParam("token")) tok = request->getParam("token")->value();
        else if (request->hasParam("token", true)) tok = request->getParam("token", true)->value();
        String stored = safeGetPref(PREF_KEY_ADMIN_TOKEN, "");
        if (stored.length() == 0 || tok != stored) {
          Serial.println("[Web] sd_upload: invalid token");
          return;
        }
        if (!sdPresent) { Serial.println("[Web] sd_upload: SD not present"); return; }
        if (filename.indexOf("..") >= 0) { Serial.println("[Web] sd_upload: invalid filename"); return; }
        String path = "/" + filename;
        Serial.printf("[Web] sd_upload: writing %s\n", path.c_str());
        sdUploadFile = SD.open(path, FILE_WRITE);
        if (!sdUploadFile) { Serial.println("[Web] sd_upload: open failed"); return; }
        sdUploadOk = true;
      }
      if (sdUploadOk && sdUploadFile) {
        if (len) sdUploadFile.write(data, len);
        if (final) {
          sdUploadFile.close();
          Serial.printf("[Web] sd_upload: %s written OK\n", filename.c_str());
        }
      }
    }
  );

  server.begin();
  Serial.println("[Web] Async server started on port 80");
}

void restartWebServer() {
  Serial.println("[Web] Restarting async web server...");
  server.end();
  delay(50);
  server.begin();
  Serial.println("[Web] Server restarted on port 80");
}
