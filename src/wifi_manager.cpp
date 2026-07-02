#include "wifi_manager.h"
#include "prefs.h"
#include "event_logging.h"
#include "config.h"
#include "timers.h"

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
static unsigned long currentBackoffMs = 2000;
static const unsigned long MAX_WIFI_BACKOFF_MS = 120000;
static const unsigned long WIFI_ATTEMPT_TIMEOUT_MS = 15000;
static const int WIFI_MAX_ATTEMPTS = 10;

static unsigned long lastProcessTime = 0;
static bool needsNTPSync = false;
static unsigned long lastAPCheckTime = 0;
static const unsigned long AP_CHECK_INTERVAL_MS = 30000; // Проверять AP каждые 30 секунд
static unsigned int apRestartCount = 0;

static String makeApName() {
  uint64_t mac = ESP.getEfuseMac();
  char apName[24];
  sprintf(apName, "WSystem-%02X%02X", (uint8_t)(mac>>16), (uint8_t)(mac>>8));
  return String(apName);
}

static void startAP() {
  String apName = makeApName();
  IPAddress localIp(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);
  // ensure AP mode at least, preserve STA if already connected
  WiFi.softAPConfig(localIp, gateway, subnet);
  WiFi.mode(WIFI_AP_STA);
  if (WiFi.softAP(apName.c_str())) {
    Serial.printf("[WiFi] AP started: %s\n", apName.c_str());
    Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    saveEventLog(LOG_INFO, EVENT_WIFI_AP_STARTED, 0);
    wifiState = WIFI_IDLE;
    apRestartCount++;
    lastAPCheckTime = millis();
  } else {
    Serial.println("[WiFi] softAP failed");
    wifiState = WIFI_FAILED;
  }
}

// Проверяет состояние точки доступа и перезапускает при необходимости
static void checkAPHealth() {
  unsigned long now = millis();
  if (now - lastAPCheckTime < AP_CHECK_INTERVAL_MS) {
    return;
  }
  lastAPCheckTime = now;
  
  // Проверяем, активна ли точка доступа
  if (WiFi.getMode() & WIFI_MODE_AP) {
    // AP должен быть активен, проверяем количество клиентов (опционально)
    int clients = WiFi.softAPgetStationNum();
    Serial.printf("[WiFi] AP health check: active, clients=%d, restarts=%u\n", clients, apRestartCount);
  } else {
    // AP не активен, но должен быть (если нет STA соединения)
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] AP is not active but should be - restarting...");
      startAP();
    }
  }
}

static void startConnectAttempt() {
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

// Forward declaration for NTP sync function
extern void syncTimeWithNTP();

static void registerStaEventHandlers() {
  // Log connection/disconnection events and reason codes to help
  // diagnose failures (password wrong, band unsupported, etc.).
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      Serial.printf("[WiFi] STA got IP: %s\n", WiFi.localIP().toString().c_str());
      
      // Установляем флаг для синхронизации NTP в главном loop (избегаем проблем TCPIP блокировки)
      Serial.println("[WiFi] Запланирована синхронизация времени по NTP...");
      needsNTPSync = true;
    }
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    // info.wifi_sta_disconnected.reason is only valid for disconnect events
    uint8_t r = info.wifi_sta_disconnected.reason;
    Serial.printf("[WiFi] STA disconnected (reason %d=%s)\n", r, reasonToString(r));
    // Дополнительная диагностика
    Serial.printf("[WiFi] RSSI before disconnect: %d\n", WiFi.RSSI());
    Serial.printf("[WiFi] AP SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("[WiFi] BSSID: %s\n", WiFi.BSSIDstr().c_str());
    
    // Адаптивный backoff в зависимости от причины отключения
    if (r == 201 || r == 2 || r == 15) {
      // Аутентификация/рукопожатие - серьезная проблема, увеличиваем backoff
      unsigned long newBackoff = currentBackoffMs * 3;
      currentBackoffMs = (newBackoff < 300000UL) ? newBackoff : 300000UL; // до 5 минут
      Serial.printf("[WiFi] Authentication issue, increased backoff to %lu ms\n", currentBackoffMs);
    } else if (r == 200 || r == 202) {
      // Временная проблема (beacon timeout, assoc fail) - умеренный backoff
      unsigned long newBackoff = currentBackoffMs * 2;
      currentBackoffMs = (newBackoff < 120000UL) ? newBackoff : 120000UL; // до 2 минут
      Serial.printf("[WiFi] Temporary issue, increased backoff to %lu ms\n", currentBackoffMs);
    } else {
      // Другие причины - стандартный backoff
      unsigned long newBackoff = currentBackoffMs * 2;
      currentBackoffMs = (newBackoff < MAX_WIFI_BACKOFF_MS) ? newBackoff : MAX_WIFI_BACKOFF_MS;
    }
    
    // Сбрасываем флаг attemptInProgress чтобы loopWifiManager мог запланировать новую попытку
    attemptInProgress = false;
    lastAttemptEndMs = millis();
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

// Оптимизация режима Wi-Fi: отключаем AP если STA подключен и нет клиентов
static void optimizeWiFiMode() {
  static unsigned long lastOptimizeCheck = 0;
  unsigned long now = millis();
  
  // Проверяем не чаще чем раз в 60 секунд
  if (now - lastOptimizeCheck < 60000) {
    return;
  }
  lastOptimizeCheck = now;
  
  // Если STA подключен и нет клиентов AP, переключаемся в режим STA только
  if (WiFi.status() == WL_CONNECTED && WiFi.softAPgetStationNum() == 0) {
    if (WiFi.getMode() == WIFI_AP_STA) {
      WiFi.mode(WIFI_STA);
      Serial.println("[WiFi] AP disabled, STA only mode for better stability");
      saveEventLog(LOG_INFO, EVENT_SETTINGS_CHANGE, 0);
    }
  }
}

// Мониторинг качества сигнала Wi-Fi
static void monitorWiFiSignal() {
  static unsigned long lastRSSICheck = 0;
  static int lastReportedRSSI = 0;
  unsigned long now = millis();
  
  // Проверяем не чаще чем раз в 30 секунд
  if (now - lastRSSICheck < 30000) {
    return;
  }
  lastRSSICheck = now;
  
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    
    // Логируем только если RSSI изменился значительно (более 5 dBm) или слишком слабый
    if (abs(rssi - lastReportedRSSI) > 5 || rssi < -75) {
      lastReportedRSSI = rssi;
      
      if (rssi < -75) {
        Serial.printf("[WiFi] Warning: Weak signal (%d dBm)\n", rssi);
        // Сохраняем событие в лог для последующего анализа
        saveEventLog(LOG_WARNING, EVENT_WIFI_WEAK_SIGNAL, rssi);
      }
    }
  }
}

void loopWifiManager() {
  // called frequently from main loop
  unsigned long now = millis();
  
  // Проверяем здоровье точки доступа
  checkAPHealth();
  
  // Оптимизация режима Wi-Fi (отключение AP при необходимости)
  optimizeWiFiMode();
  
  // Мониторинг качества сигнала
  monitorWiFiSignal();
  
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
    } else if (cachedSsid.length() > 0 && attemptCount >= WIFI_MAX_ATTEMPTS) {
      // give up and start AP
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

bool wifi_needsNTPSync() {
  return needsNTPSync;
}

void wifi_clearNTPSyncFlag() {
  needsNTPSync = false;
}

// Возвращает статистику Wi-Fi для мониторинга
String wifi_getStats() {
  String stats = "";
  stats += "State: ";
  switch(wifiState) {
    case WIFI_IDLE: stats += "IDLE"; break;
    case WIFI_CONNECTING: stats += "CONNECTING"; break;
    case WIFI_CONNECTED: stats += "CONNECTED"; break;
    case WIFI_FAILED: stats += "FAILED"; break;
  }
  stats += ", Attempts: " + String(attemptCount);
  stats += ", AP restarts: " + String(apRestartCount);
  stats += ", RSSI: " + String(WiFi.RSSI());
  stats += ", Clients: " + String(WiFi.softAPgetStationNum());
  return stats;
}

// Возвращает детальную информацию о Wi-Fi соединении
String wifi_getDetailedInfo() {
  String info = "";
  info += "Mode: ";
  wifi_mode_t mode = WiFi.getMode();
  if (mode & WIFI_MODE_AP) info += "AP ";
  if (mode & WIFI_MODE_STA) info += "STA ";
  
  if (WiFi.status() == WL_CONNECTED) {
    info += "\nSTA IP: " + WiFi.localIP().toString();
    info += "\nGateway: " + WiFi.gatewayIP().toString();
    info += "\nSSID: " + WiFi.SSID();
    info += "\nBSSID: " + WiFi.BSSIDstr();
    info += "\nChannel: " + String(WiFi.channel());
  }
  
  info += "\nAP IP: " + WiFi.softAPIP().toString();
  info += "\nAP Clients: " + String(WiFi.softAPgetStationNum());
  info += "\nAP SSID: " + makeApName();
  
  return info;
}
