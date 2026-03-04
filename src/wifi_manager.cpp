#include "wifi_manager.h"
#include "prefs.h"
#include "event_logging.h"
#include "config.h"

extern Preferences preferences;

// Convenience wrapper that only calls getString when the key
// actually exists; this prevents the Preferences library from
// emitting "NOT_FOUND" errors during normal first-boot operation.
static String safeGet(const char *key, const String &def = "") {
    if (preferences.isKey(key)) {
        return preferences.getString(key, def);
    }
    return def;
}

static WifiState wifiState = WIFI_IDLE;
static String cachedSsid = "";
static String cachedPass = "";
static unsigned long attemptStartMs = 0;
static unsigned long lastAttemptEndMs = 0;
static int attemptCount = 0;
static bool attemptInProgress = false;
// flag to avoid repeatedly re‑starting/accessing AP mode and logging the same event
static bool apModeStarted = false;
static unsigned long currentBackoffMs = 2000;
static const unsigned long MAX_WIFI_BACKOFF_MS = 60000;
static const unsigned long WIFI_ATTEMPT_TIMEOUT_MS = 10000;
static const int WIFI_MAX_ATTEMPTS = 5;

static unsigned long lastProcessTime = 0;

static String makeApName() {
  uint64_t mac = ESP.getEfuseMac();
  char apName[24];
  sprintf(apName, "WSystem-%02X%02X", (uint8_t)(mac>>16), (uint8_t)(mac>>8));
  return String(apName);
}

static void startAP() {
  // if we have already brought up the fallback AP once, there's no need to
  // recreate it or flood the log.  other callers may invoke startAP when
  // the code drifts through various branches (max‑attempts, empty creds, etc.)
  // so guard against duplicate work and duplicate events here.
  if (apModeStarted) {
    return;
  }

  String apName = makeApName();
  IPAddress localIp(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);
  // ensure AP mode at least, preserve STA if already connected
  WiFi.softAPConfig(localIp, gateway, subnet);
  WiFi.mode(WIFI_AP_STA);
  if (WiFi.softAP(apName.c_str())) {
    apModeStarted = true;
    Serial.printf("[WiFi] AP started: %s\n", apName.c_str());
    Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    saveEventLog(LOG_INFO, EVENT_WIFI_AP_STARTED, 0);
    wifiState = WIFI_IDLE;
  } else {
    Serial.println("[WiFi] softAP failed");
    wifiState = WIFI_FAILED;
  }
}

static void startConnectAttempt() {
  // any attempt to switch to STA means we should clear the AP‑started flag;
  // the subsequent call to WiFi.mode(WIFI_AP_STA) may leave the AP running,
  // but we don't want to suppress a future *fallback* restart if STA fails
  apModeStarted = false;

  if (cachedSsid.length() == 0) {
    startAP();
    return;
  }
  Serial.printf("[WiFi] Starting connect attempt %d to '%s'...\n", attemptCount+1, cachedSsid.c_str());
  // keep AP running while attempting STA connection; use dual mode
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(cachedSsid.c_str(), cachedPass.c_str());
  attemptStartMs = millis();
  attemptInProgress = true;
  wifiState = WIFI_CONNECTING;
}

static void scheduleNextAttempt() {
  lastAttemptEndMs = millis();
  attemptInProgress = false;
  attemptCount++;
  if (currentBackoffMs < MAX_WIFI_BACKOFF_MS) {
    currentBackoffMs = currentBackoffMs * 2;
    if (currentBackoffMs > MAX_WIFI_BACKOFF_MS) currentBackoffMs = MAX_WIFI_BACKOFF_MS;
  }
  Serial.printf("[WiFi] Next WiFi backoff: %lu ms\n", currentBackoffMs);
}

// convert a wifi disconnect reason code to a brief message.  
// These strings are borrowed from the esp-idf documentation; the
// codes >200 vary by SDK version. 201 is commonly seen when the
// AP rejects association (wrong password, unsupported channel, etc.).
static const char *reasonToString(uint8_t r) {
  switch (r) {
    case 1: return "UNSPECIFIED";
    case 2: return "AUTH_EXPIRE";
    case 3: return "AUTH_LEAVE";
    case 4: return "ASSOC_EXPIRE";
    case 5: return "ASSOC_TOOMANY";
    case 6: return "NOT_AUTHED";
    case 7: return "NOT_ASSOCED";
    case 8: return "ASSOC_LEAVE";
    case 9: return "ASSOC_NOT_AUTHED";
    case 10: return "DISASSOC_PWRCAP_BAD";
    case 11: return "DISASSOC_SUPCHAN_BAD";
    case 12: return "";
    case 13: return "IE_INVALID";
    case 14: return "MIC_FAILURE";
    case 15: return "4WAY_HANDSHAKE_TIMEOUT";
    case 16: return "GROUP_KEY_HANDSHAKE_TIMEOUT";
    case 17: return "IE_IN_4WAY_DIFFERS";
    case 18: return "GROUP_CIPHER_INVALID";
    case 19: return "PAIRWISE_CIPHER_INVALID";
    case 20: return "AKMP_INVALID";
    case 21: return "UNSUPP_RSN_IE_VERSION";
    case 22: return "INVALID_RSN_IE_CAP";
    case 23: return "CIPHER_SUITE_REJECTED";
    case 24: return "BEACON_TIMEOUT";
    case 25: return "NO_AP_FOUND";
    case 26: return "AUTH_FAIL";
    case 31: return "ASSOC_FAIL";
    case 32: return "HANDSHAKE_TIMEOUT";
    case 201: return "AUTH_REJECT_OR_NO_AP (check SSID/password, band)";
    default: return "unknown";
  }
}

static void registerStaEventHandlers() {
  // Log connection/disconnection events and reason codes to help
  // diagnose failures (password wrong, band unsupported, etc.).
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      Serial.printf("[WiFi] STA got IP: %s\n", WiFi.localIP().toString().c_str());
    }
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    // info.wifi_sta_disconnected.reason is only valid for disconnect events
    uint8_t r = info.wifi_sta_disconnected.reason;
    Serial.printf("[WiFi] STA disconnected (reason %d=%s)\n", r, reasonToString(r));
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

void initWifiManager() {
  // register one‑time event handlers for STA mode
  registerStaEventHandlers();

  // Load cached credentials using the safe helper so that missing
  // keys do not result in error messages from the Preferences stack.
  cachedSsid = safeGet(PREF_KEY_WIFI_SSID, "");
  cachedPass = safeGet(PREF_KEY_WIFI_PASS, "");

  if (cachedSsid.length() > 0) {
    attemptCount = 0;
    currentBackoffMs = 2000;
    startConnectAttempt();
  } else {
    startAP();
  }
}

void loopWifiManager() {
  // called frequently from main loop
  unsigned long now = millis();
  if (attemptInProgress) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiState = WIFI_CONNECTED;
      int rssi = WiFi.RSSI();
      Serial.printf("[WiFi] Connected, IP: %s RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), rssi);
      saveEventLog(LOG_INFO, EVENT_WIFI_CONNECTED, 0); // log connect
      attemptInProgress = false;
      attemptCount = 0; // reset
      currentBackoffMs = 2000;
      // update lastAttemptEndMs so we don't immediately schedule a new attempt
      lastAttemptEndMs = now;
      return;
    }
    if (now - attemptStartMs > WIFI_ATTEMPT_TIMEOUT_MS) {
      Serial.println("[WiFi] Connect attempt timed out");
      Serial.printf("[WiFi] status after timeout: %d\n", WiFi.status());
      scheduleNextAttempt();
    }
  } else {
    // not in progress - check if we need to schedule next attempt
    // If already connected, keep wifiState and do not start new attempts
    if (WiFi.status() == WL_CONNECTED) {
      wifiState = WIFI_CONNECTED;
      attemptInProgress = false;
      attemptCount = 0;
      currentBackoffMs = 2000;
      lastAttemptEndMs = now;
      return;
    }

    if (cachedSsid.length() > 0 && (attemptCount < WIFI_MAX_ATTEMPTS)) {
      if (now - lastAttemptEndMs >= currentBackoffMs) {
        Serial.printf("[WiFi] Scheduling next connect attempt (attempt %d)\n", attemptCount+1);
        startConnectAttempt();
      }
    } else if (cachedSsid.length() > 0 && attemptCount >= WIFI_MAX_ATTEMPTS && !apModeStarted) {
      // give up and start AP once; subsequent loops will skip because
      // startAP() is now a no‑op when apModeStarted==true
      Serial.println("[WiFi] Max attempts reached, starting AP");
      startAP();
    }
  }
}

void wifi_setCredentials(const String &ssid, const String &pass) {
  // persist and start connect sequence
  preferences.putString(PREF_KEY_WIFI_SSID, ssid);
  preferences.putString(PREF_KEY_WIFI_PASS, pass);
  cachedSsid = ssid;
  cachedPass = pass;
  attemptCount = 0;
  currentBackoffMs = 2000;
  startConnectAttempt();
  saveEventLog(LOG_INFO, EVENT_SETTINGS_CHANGE, 0);
}

void wifi_clearCredentials() {
  preferences.remove(PREF_KEY_WIFI_SSID);
  preferences.remove(PREF_KEY_WIFI_PASS);
  cachedSsid = "";
  cachedPass = "";
  attemptCount = 0;
  currentBackoffMs = 2000;
  startAP();
  saveEventLog(LOG_INFO, EVENT_SETTINGS_CHANGE, 0);
}

WifiState wifi_getState() { return wifiState; }
String wifi_getSSID() { return cachedSsid; }
int wifi_getAttemptCount() { return attemptCount; }
String wifi_getIP() { return (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : WiFi.softAPIP().toString(); }
