/**
 * @file timers.cpp
 * @brief Реализация управления таймерами и RTC
 * @details Функции работы с часами реального времени и watchdog
 */

#include <Arduino.h>
#include "timers.h"
#include "config.h"
#include "structures.h"
#include <esp_system.h>  
#include <esp_task_wdt.h> 
#include <rom/rtc.h>
#include <time.h>
#include <sys/time.h>

// ============ ИНИЦИАЛИЗАЦИЯ RTC ============

/// Инициализирует модуль RTC
void initRTC() {
  extern RTC_DS3231 rtc;
  extern SystemFlags flags;
  
  if (!rtc.begin()) {
    Serial.println("[ERROR] RTC initialization failed");
    flags.rtcInitialized = 0;
    return;
  }
  
  // Проверяем, потеряна ли дата/время
  if (rtc.lostPower()) {
    Serial.println("[WARNING] RTC lost power, setting to compile time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    
    // Логируем событие потери питания RTC
    extern void saveEventLog(LogLevel level, uint8_t eventType, uint16_t param);
    saveEventLog(LOG_ERROR, EVENT_RTC_LOST_POWER, 0);
  }
  
  flags.rtcInitialized = 1;
}

/// Устанавливает дату и время в RTC
void setRTCDateTime(uint16_t year, uint8_t month, uint8_t day,
                    uint8_t hour, uint8_t minute, uint8_t second) {
  extern RTC_DS3231 rtc;
  rtc.adjust(DateTime(year, month, day, hour, minute, second));
}

/// Получает текущую дату и время
DateTime getRTCDateTime() {
  extern RTC_DS3231 rtc;
  return rtc.now();
}

/// Получает текущий час
uint8_t getCurrentHour() {
  return getRTCDateTime().hour();
}

/// Получает текущую минуту
uint8_t getCurrentMinute() {
  return getRTCDateTime().minute();
}

/// Получает текущую секунду
uint8_t getCurrentSecond() {
  return getRTCDateTime().second();
}

/// Возвращает true, если время истекло с момента startTime
bool isTimeElapsed(unsigned long& lastTime, unsigned long interval) {
  unsigned long currentTime = millis();
  if (currentTime - lastTime >= interval) {
    lastTime = currentTime;
    return true;
  }
  return false;
}

// ============ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ПРЕОБРАЗОВАНИЯ ВРЕМЕНИ ============

/// Преобразует время из UTC в локальное время с использованием часового пояса
/// @param utcSeconds UTC время в секундах (unix timestamp)
/// @param tzOffsetHours Смещение часового пояса в часах
/// @return DateTime объект с локальным временем
DateTime utcToLocal(time_t utcSeconds, int8_t tzOffsetHours) {
  // Применяем смещение часового пояса
  time_t localSeconds = utcSeconds + (tzOffsetHours * 3600);
  return DateTime(localSeconds);
}

/// Преобразует локальное время обратно в UTC, используя часовой пояс
/// @param localTime Локальное время (DateTime объект)
/// @param tzOffsetHours Смещение часового пояса в часах
/// @return Unix timestamp (UTC)
time_t localToUTC(const DateTime& localTime, int8_t tzOffsetHours) {
  // Преобразуем DateTime в unix timestamp (как будто это UTC)
  struct tm tm;
  tm.tm_year = localTime.year() - 1900;
  tm.tm_mon = localTime.month() - 1;
  tm.tm_mday = localTime.day();
  tm.tm_hour = localTime.hour();
  tm.tm_min = localTime.minute();
  tm.tm_sec = localTime.second();
  tm.tm_isdst = -1;
  
  time_t localAsUnix = mktime(&tm);
  
  // Вычитаем смещение часового пояса чтобы получить истинное UTC
  time_t utcSeconds = localAsUnix - (tzOffsetHours * 3600);
  
  return utcSeconds;
}

// ============ СИНХРОНИЗАЦИЯ ВРЕМЕНИ (NTP) ============

/// Синхронизирует время с NTP сервером после подключения WiFi
void syncTimeWithNTP() {
  extern RTC_DS3231 rtc;
  extern SafetySettings safetySettings;
  extern void saveEventLog(LogLevel level, uint8_t eventType, uint16_t param);
  
  Serial.println("[NTP] Requesting time synchronization...");
  
  // Строим строку часового пояса на основе safetySettings.timeZoneOffset
  char tzString[32];
  
  // POSIX TZ strings use inverted sign for fixed offsets:
  // UTC+5 (east) is encoded as "UTC-5".
  if (safetySettings.timeZoneOffset == 0) {
    strcpy(tzString, "UTC0");
  } else if (safetySettings.timeZoneOffset > 0) {
    snprintf(tzString, sizeof(tzString), "UTC-%d", safetySettings.timeZoneOffset);
  } else {
    snprintf(tzString, sizeof(tzString), "UTC+%d", -safetySettings.timeZoneOffset);
  }
  
  Serial.printf("[NTP] Using timezone: %s\n", tzString);
  
  // Настраиваем динамический часовой пояс с NTP серверами
  configTzTime(tzString, "pool.ntp.org", "time.nist.gov", "time.google.com");
  
  // Даем время SNTP клиенту на синхронизацию (макс 30 сек)
  time_t nowUtc = time(nullptr);  // UTC время из NTP
  int attempts = 0;
  const int maxAttempts = 60; // 30 сек при 500 мс задержке
  
  while (nowUtc < 24 * 3600 && attempts < maxAttempts) {
    delay(500);
    time(&nowUtc);
    attempts++;
  }
  
  if (nowUtc > 24 * 3600) {
    // Время успешно синхронизировано
    // nowUtc содержит UTC время
    struct tm timeinfo_utc = *gmtime(&nowUtc);  // UTC
    struct tm timeinfo_local = *localtime(&nowUtc);  // Локальное время
    
    Serial.printf("[NTP] UTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo_utc.tm_year + 1900,
                  timeinfo_utc.tm_mon + 1,
                  timeinfo_utc.tm_mday,
                  timeinfo_utc.tm_hour,
                  timeinfo_utc.tm_min,
                  timeinfo_utc.tm_sec);
    
    Serial.printf("[NTP] Local time: %04d-%02d-%02d %02d:%02d:%02d (TZ: %+d)\n",
                  timeinfo_local.tm_year + 1900,
                  timeinfo_local.tm_mon + 1,
                  timeinfo_local.tm_mday,
                  timeinfo_local.tm_hour,
                  timeinfo_local.tm_min,
                  timeinfo_local.tm_sec,
                  safetySettings.timeZoneOffset);
    
    // ВАЖНО: Теперь сохраняем ЛОКАЛЬНОЕ время в RTC (не UTC!)
    // Это позволяет коду отображения работать без коррекций
    DateTime localDateTime(
      timeinfo_local.tm_year + 1900,
      timeinfo_local.tm_mon + 1,
      timeinfo_local.tm_mday,
      timeinfo_local.tm_hour,
      timeinfo_local.tm_min,
      timeinfo_local.tm_sec
    );
    rtc.adjust(localDateTime);
    
    Serial.println("[NTP] RTC updated with LOCAL time");
    saveEventLog(LOG_INFO, EVENT_NTP_SYNC_SUCCESS, safetySettings.timeZoneOffset + 128);
  } else {
    Serial.println("[NTP] Time synchronization failed");
    saveEventLog(LOG_WARNING, EVENT_NTP_SYNC_FAILED, attempts);
  }
}

/// Пересчитывает время в RTC при смене часового пояса
/// Конвертирует текущее локальное время из старого пояса в новый
void recalculateRTCForTimezone(int8_t oldOffset, int8_t newOffset) {
  extern RTC_DS3231 rtc;
  extern void saveEventLog(LogLevel level, uint8_t eventType, uint16_t param);
  
  if (oldOffset == newOffset) {
    Serial.println("[Timezone] No change in offset, skipping recalculation");
    return;
  }
  
  // Получаем текущее время из RTC (это локальное время для oldOffset)
  DateTime currentLocal = rtc.now();
  
  // Преобразуем локальное время в UTC, используя старый offset
  // localTime + offset*3600s = UTC
  time_t utcSeconds = currentLocal.unixtime() - (oldOffset * 3600);
  
  // Теперь применяем новый offset для получения нового локального времени
  // UTC + newOffset*3600s = newLocalTime
  time_t newLocalSeconds = utcSeconds + (newOffset * 3600);
  DateTime newLocal = DateTime(newLocalSeconds);
  
  // Сохраняем новое локальное время в RTC
  rtc.adjust(newLocal);
  
  Serial.printf("[Timezone] RTC recalculated: old UTC%+d -> new UTC%+d\n", oldOffset, newOffset);
  Serial.printf("[Timezone] Old local: %04d-%02d-%02d %02d:%02d:%02d\n",
                currentLocal.year(), currentLocal.month(), currentLocal.day(),
                currentLocal.hour(), currentLocal.minute(), currentLocal.second());
  Serial.printf("[Timezone] New local: %04d-%02d-%02d %02d:%02d:%02d\n",
                newLocal.year(), newLocal.month(), newLocal.day(),
                newLocal.hour(), newLocal.minute(), newLocal.second());
  
  saveEventLog(LOG_INFO, EVENT_TIMEZONE_CHANGED, newOffset + 128);
}

// ============ ИНИЦИАЛИЗАЦИЯ WATCHDOG ============

/// Инициализирует watchdog таймер
void initWatchdog() {
  extern SystemFlags flags;

  if (!WATCHDOG_ENABLED) {
    flags.watchdogEnabled = 0;
    return;
  }

  // Инициализируем встроенный watchdog ESP32
  esp_task_wdt_config_t _wdt_cfg = { .timeout_ms = (WDT_TIMEOUT_SECONDS * 1000), .idle_core_mask = 0, .trigger_panic = true };
  // ESP-IDF v5 может уже инициализировать TWDT, используем reconfigure
  esp_err_t err = esp_task_wdt_reconfigure(&_wdt_cfg);
  if (err != ESP_OK) {
    err = esp_task_wdt_init(&_wdt_cfg);
  }
  esp_task_wdt_add(NULL); // Добавляем текущую задачу
  flags.watchdogEnabled = 1;
  Serial.printf("[WDT] Initialized, timeout=%ds\n", WDT_TIMEOUT_SECONDS);
  
  (void)WDT_TIMEOUT_SECONDS; // reserved for future reporting
}

/// Сбрасывает watchdog таймер
void feedWatchdog() {
  esp_task_wdt_reset();
}

/// Получает причину последнего сброса
const char* getResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  
  switch(reason) {
    case ESP_RST_UNKNOWN: return "Unknown reset";
    case ESP_RST_POWERON: return "Power on reset";
    case ESP_RST_EXT: return "External reset";
    case ESP_RST_SW: return "Software reset";
    case ESP_RST_PANIC: return "Panic reset";
    case ESP_RST_INT_WDT: return "Interrupt watchdog reset";
    case ESP_RST_TASK_WDT: return "Task watchdog reset";
    case ESP_RST_WDT: return "Watchdog reset";
    case ESP_RST_DEEPSLEEP: return "Deep sleep reset";
    case ESP_RST_BROWNOUT: return "Brownout reset";
    case ESP_RST_SDIO: return "SDIO reset";
    default: return "Unknown reason";
  }
}

/// Выводит информацию о сбросе в Serial
void printResetReason() {
  // No serial output for clean builds; function retained for API compatibility
  (void)getResetReason();
}

/// Получает информацию по сбросам в структуру (для сохранения)
void updateWDTStats() {
  extern WDTStats wdtStats;
  
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_TASK_WDT || reason == ESP_RST_INT_WDT) {
    wdtStats.resetCount++;
    wdtStats.lastResetTime = millis();
  }
}

// ============ УПРАВЛЕНИЕ ТАЙМЕРАМИ ============

static uint32_t countdownTotal = 0;
static unsigned long countdownStart = 0;
static bool countdownActive = false;

/// Запускает таймер обратного отсчета
void startCountdownTimer(uint32_t seconds) {
  countdownTotal = seconds;
  countdownStart = millis();
  countdownActive = true;
}

/// Останавливает таймер обратного отсчета
void stopCountdownTimer() {
  countdownActive = false;
}

/// Получает оставшееся время для таймера (в секундах)
uint32_t getCountdownRemaining() {
  if (!countdownActive) return 0;
  unsigned long elapsed = (millis() - countdownStart) / 1000UL;
  if (elapsed >= countdownTotal) return 0;
  return countdownTotal - elapsed;
}

/// Проверяет, завершился ли таймер
bool isCountdownComplete() {
  if (!countdownActive) return false;
  unsigned long elapsed = (millis() - countdownStart) / 1000UL;
  if (elapsed >= countdownTotal) {
    countdownActive = false;
    return true;
  }
  return false;
}

/// Получает общее время для текущего таймера
uint32_t getCountdownTotal() {
  return countdownTotal;
}

// ============ ЗАДЕРЖКИ И СИНХРОНИЗАЦИЯ ============

/// Мягкая задержка с обновлением watchdog
void delayWithWDT(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    feedWatchdog();
    delay(10);
  }
}

/// Синхронизация с RTC (для точности времени)
void syncWithRTC() {
  // Синхронизируем внутренние таймеры с RTC
}

// ============ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ============

/// Возвращает текущее время в миллисекундах
unsigned long getCurrentTimeMs() {
  return millis();
}

/// Возвращает текущее время RTC в секундах с начала эпохи
unsigned long getCurrentTimeRTC() {
  return getRTCDateTime().unixtime();
}

/// Форматирует время в строку (HH:MM:SS)
void formatTime(uint8_t hour, uint8_t minute, uint8_t second, char* buffer) {
  sprintf(buffer, "%02d:%02d:%02d", hour, minute, second);
}

/// Форматирует дату в строку (DD.MM.YYYY)
void formatDate(uint16_t year, uint8_t month, uint8_t day, char* buffer) {
  sprintf(buffer, "%02d.%02d.%04d", day, month, year);
}

/// Форматирует дату и время
void formatDateTime(uint16_t year, uint8_t month, uint8_t day,
                    uint8_t hour, uint8_t minute, uint8_t second,
                    char* buffer) {
  sprintf(buffer, "%02d.%02d.%04d %02d:%02d:%02d",
          day, month, year, hour, minute, second);
}

/// Выводит текущее время в Serial
void printCurrentTime() {
  DateTime now = getRTCDateTime();
  char buffer[30];
  formatDateTime(now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second(), buffer);
  Serial.println(buffer);
}
