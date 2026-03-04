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
  esp_task_wdt_init(&_wdt_cfg);
  esp_task_wdt_add(NULL); // Добавляем текущую задачу
  flags.watchdogEnabled = 1;
  
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
