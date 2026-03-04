

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
static void broadcastStatus() {
  if (millis() - lastBroadcast < 2000)
    return;
  lastBroadcast = millis();
  DynamicJsonDocument doc(768);
  doc["uptime"] = millis() / 1000;
  doc["ip"] = wifi_getIP();
  doc["mac"] = getMacString();

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
  JsonObject timers = doc.createNestedObject("timers");
  timers["aeration"] = currentAerationRemaining;
  timers["ozonation"] = currentOzonationRemaining;
  timers["settling"] = currentSetlingRemaining;

  // Флаги, связанные с основным экраном
  extern SystemFlags flags;
  JsonObject f = doc.createNestedObject("flags");
  f["tank1Empty"] = flags.tank1Empty;
  f["tank2Empty"] = flags.tank2Empty;
  f["filterCleaningNeeded"] = flags.filterCleaningNeeded;
  f["backwashInProgress"] = flags.backwashInProgress;
  f["blinkState"] = flags.blinkState;

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
  JsonObject timers = doc.createNestedObject("timers");
  timers["aeration"] = currentAerationRemaining;
  timers["ozonation"] = currentOzonationRemaining;
  timers["settling"] = currentSetlingRemaining;

  // Флаги для главного экрана
  extern SystemFlags flags;
  JsonObject f = doc.createNestedObject("flags");
  f["tank1Empty"] = flags.tank1Empty;
  f["tank2Empty"] = flags.tank2Empty;
  f["filterCleaningNeeded"] = flags.filterCleaningNeeded;
  f["backwashInProgress"] = flags.backwashInProgress;
  f["blinkState"] = flags.blinkState;
 
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

// root page handler (serves index.html from SD/SPIFFS)
static void handleRoot(AsyncWebServerRequest* request) {
  IPAddress clientIP = request->client()->remoteIP();
#ifdef DEBUG_SERIAL
  Serial.printf("[Web] GET / from %s\n", clientIP.toString().c_str());
#endif
  if (!sendStaticResource(request, "/index.html", "text/html")) {
    request->send(404, "text/plain", "index.html not found");
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
  JsonObject timers = doc.createNestedObject("timers");
  timers["aeration"] = currentAerationRemaining;
  timers["ozonation"] = currentOzonationRemaining;
  timers["settling"] = currentSetlingRemaining;

  // flags for main screen
  extern SystemFlags flags;
  JsonObject f = doc.createNestedObject("flags");
  f["tank1Empty"] = flags.tank1Empty;
  f["tank2Empty"] = flags.tank2Empty;
  f["filterCleaningNeeded"] = flags.filterCleaningNeeded;
  f["backwashInProgress"] = flags.backwashInProgress;
  f["blinkState"] = flags.blinkState;

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
  if (!request->hasParam("name") || !request->hasParam("op")) {
    request->send(400, "text/plain", "bad request");
    return;
  }
  String name = request->getParam("name")->value();
  String op = request->getParam("op")->value();
  IPAddress clientIP = request->client()->remoteIP();
#ifdef DEBUG_SERIAL
  Serial.printf("[Web] GET /control from %s (name=%s op=%s)\n", clientIP.toString().c_str(), name.c_str(), op.c_str());
#endif

  // Map name to actions
  uint16_t opCode = 0;
  if (name == "pump1") {
    Serial.printf("[Web] HTTP control received: name=%s op=%s\n", name.c_str(), op.c_str());
    if (op == "toggle") {
      if (getRelayState(0)) {
        turnOffPump1();
        opCode = MANUAL_PUMP1_OFF;
      } else {
        turnOnPump1();
        opCode = MANUAL_PUMP1_ON;
      }
    }
  } else if (name == "pump2") {
    Serial.printf("[Web] HTTP control received: name=%s op=%s\n", name.c_str(), op.c_str());
    if (op == "toggle") {
      if (getRelayState(1)) {
        turnOffPump2();
        opCode = MANUAL_PUMP2_OFF;
      } else {
        turnOnPump2();
        opCode = MANUAL_PUMP2_ON;
      }
    }
  } else if (name == "aeration") {
    if (op == "toggle") {
      if (getRelayState(2)) {
        turnOffAeration();
        opCode = MANUAL_AERATION_OFF;
      } else {
        turnOnAeration();
        opCode = MANUAL_AERATION_ON;
      }
    }
  } else if (name == "ozone") {
    if (op == "toggle") {
      if (getRelayState(3)) {
        turnOffOzone();
        opCode = MANUAL_OZONE_OFF;
      } else {
        turnOnOzone();
        opCode = MANUAL_OZONE_ON;
      }
    }
  } else if (name == "filter") {
    if (op == "toggle") {
      if (getRelayState(4)) {
        turnOffFilter();
        opCode = MANUAL_FILTER_OFF;
      } else {
        turnOnFilter();
        opCode = MANUAL_FILTER_ON;
      }
    }
  } else if (name == "backwash") {
    if (op == "toggle") {
      if (getRelayState(5)) {
        turnOffBackwash();
        opCode = MANUAL_BACKWASH_OFF;
      } else {
        turnOnBackwash();
        opCode = MANUAL_BACKWASH_ON;
      }
    }
  } else {
    request->send(400, "text/plain", "unknown relay");
    return;
  }

  saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, opCode, SRC_WEB);
  request->send(200, "text/plain", "ok");
  // Send immediate status update so clients see the change without waiting for regular broadcast
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
        const char* name = doc["name"];
        const char* op = doc["op"];
        if (!name || !op) {
          client->text("{\"responseTo\":\"control\",\"ok\":false,\"message\":\"missing params\"}");
          return;
        }
        String sname = String(name);
        String sop = String(op);
        Serial.printf("[Web] WS control received: name=%s op=%s\n", sname.c_str(), sop.c_str());
        // Perform same action as HTTP control
        if (sname == "pump1") {
          if (sop == "toggle") {
            if (getRelayState(0))
              turnOffPump1();
            else
              turnOnPump1();
          }
        } else if (sname == "pump2") {
          if (sop == "toggle") {
            if (getRelayState(1))
              turnOffPump2();
            else
              turnOnPump2();
          }
        } else if (sname == "aeration") {
          if (sop == "toggle") {
            if (getRelayState(2))
              turnOffAeration();
            else
              turnOnAeration();
          }
        } else if (sname == "ozone") {
          if (sop == "toggle") {
            if (getRelayState(3))
              turnOffOzone();
            else
              turnOnOzone();
          }
        } else if (sname == "filter") {
          if (sop == "toggle") {
            if (getRelayState(4))
              turnOffFilter();
            else
              turnOnFilter();
          }
        } else if (sname == "backwash") {
          if (sop == "toggle") {
            if (getRelayState(5))
              turnOffBackwash();
            else
              turnOnBackwash();
          }
        } else {
          client->text("{\"responseTo\":\"control\",\"ok\":false,\"message\":\"unknown relay\"}");
          return;
        }
        saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, 0);
        Serial.println("[Web] WS control executed and event logged");
        client->text("{\"responseTo\":\"control\",\"ok\":true,\"message\":\"ok\"}");
        // After action, send immediate updated status to all clients (no rate limit)
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

static bool mqttTopicBaseValid(const String &s) {
  if (s.length() == 0) return false;
  for (size_t i = 0; i < s.length(); ++i) {
    uint8_t c = (uint8_t)s[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '/')
      continue;
    return false;
  }
  return true;
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
  doc["topic_base"] = safeGetPref(PREF_KEY_MQTT_TOPIC_BASE, DEFAULT_MQTT_TOPIC_BASE);
  doc["interval"] = preferences.getUInt(PREF_KEY_MQTT_PUB_INTERVAL, 10);
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
  // Previously the handler required a valid admin token in the JSON body
  // to authorize changes.  The requirement has been removed so that MQTT
  // settings can be updated without supplying the token.
  // (The token is still used elsewhere, e.g. for Wi‑Fi or admin operations.)

  // Apply settings
  bool enabled = doc["enabled"] | false;
  const char* broker = doc["broker"] | "";
  int port = doc["port"] | DEFAULT_MQTT_PORT;
  const char* user = doc["user"] | "";
  const char* pass = doc["pass"] | "";
  String topic_base = doc["topic_base"] | DEFAULT_MQTT_TOPIC_BASE;
  int interval = doc["interval"] | 10;

  // validate the base; reject the request if it's garbage.  a malformed
  // topic was leading to "Malformed UTF-8 in will topic" errors on the
  // broker, so leaving invalid values around is a security risk.
  if (!mqttTopicBaseValid(topic_base)) {
    request->send(400, "application/json", "{\"error\":\"invalid topic_base\"}");
    return;
  }

  preferences.putBool(PREF_KEY_MQTT_ENABLED, enabled);
  preferences.putString(PREF_KEY_MQTT_BROKER, broker);
  preferences.putUInt(PREF_KEY_MQTT_PORT, (uint32_t)port);
  preferences.putString(PREF_KEY_MQTT_USER, user);
  if (strlen(pass) > 0)
    preferences.putString(PREF_KEY_MQTT_PASS, pass);
  preferences.putString(PREF_KEY_MQTT_TOPIC_BASE, topic_base);
  preferences.putUInt(PREF_KEY_MQTT_PUB_INTERVAL, (uint32_t)interval);
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
  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest* request) { request->send(200, "application/json", "{\"queued\":true}"); }, NULL, handlePostWifiBody);
  // language preference API
  server.on("/api/lang", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(128);
    doc["english"] = isEnglish();
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });
  server.on("/api/lang", HTTP_POST, [](AsyncWebServerRequest* request) { request->send(200, "application/json", "{\"queued\":true}"); }, NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total){
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

  // DEBUG: отладочный эндпоинт для просмотра текущего admin token из NVS.
  // ВНИМАНИЕ: использовать только для диагностики, затем удалить или закрыть.
  server.on("/api/admin_token_debug", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!prefsIsAvailable()) {
      request->send(503, "application/json", "{\"error\":\"preferences unavailable\"}");
      return;
    }
    String storedToken = preferences.getString(PREF_KEY_ADMIN_TOKEN, "");
    DynamicJsonDocument doc(128);
    doc["token"] = storedToken;
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // Register MQTT handlers
  server.on("/api/mqtt", HTTP_GET, [](AsyncWebServerRequest* request) { handleGetMqtt(request); });
  server.on("/api/mqtt", HTTP_POST, [](AsyncWebServerRequest* request) { request->send(200, "application/json", "{\"queued\":true}"); }, NULL, handlePostMqttBody);

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

  server.on("/api/log_filter", HTTP_POST, [](AsyncWebServerRequest* request) { request->send(200, "application/json", "{}"); }, NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
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
  server.on("/api/sd_files", HTTP_POST, [](AsyncWebServerRequest* request) { request->send(200, "application/json", "{}"); }, NULL,
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
  server.on("/api/logs", HTTP_POST, [](AsyncWebServerRequest* request) { request->send(200, "application/json", "{}"); }, NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
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
