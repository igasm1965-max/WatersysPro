
// Для delay()
#include <Arduino.h>
// FreeRTOS primitives (mutex for SD access)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
// Для lcd
#include "display.h"
#include "menu.h"
#include "event_logging.h"
#include "utils.h"  // for sdMux

// helper to determine whether we should write/read logs from SD card
static bool logsOnSD() {
    extern bool sdPresent;
    return sdPresent;
}

void displayEventStats() {
  extern SafeLCD lcd;
  int total = getEventCount();
  // Подсчёт количества по типам (максимум 32 уникальных типа)
  uint8_t typeCounts[32] = {0};
  char typeNames[32][17];
  int uniqueTypes = 0;
  for (int i = 0; i < total; ++i) {
    EventLog* ev = getEventByIndexSimple(i);
    if (!ev) continue;
    // Группируем по коду события (без учёта уровня)
    int idx = -1;
    for (int t = 0; t < uniqueTypes; ++t) {
      if (typeNames[t][0] == 0) continue;
      char buf[17];
      const char* txt = getEventText(ev->eventType);
      strncpy(buf, txt, sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
      if (strncmp(typeNames[t], buf, sizeof(buf)) == 0) {
        idx = t;
        break;
      }
    }
    if (idx == -1 && uniqueTypes < 32) {
      // Новый тип (по тексту события)
      char buf[17];
      const char* txt = getEventText(ev->eventType);
      strncpy(buf, txt, sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
      strncpy(typeNames[uniqueTypes], buf, sizeof(typeNames[0]) - 1);
      typeNames[uniqueTypes][sizeof(typeNames[0]) - 1] = 0;
      typeCounts[uniqueTypes] = 1;
      uniqueTypes++;
    } else if (idx != -1) {
      typeCounts[idx]++;
    }
  }

  int page = 0;
  bool exit = false;
  bool needUpdate = true;
  while (!exit) {
    if (needUpdate) {
      lcd.clear();
      // Формируем и печатаем заголовок целиком, с очисткой остатка строки
      // Заголовок на ASCII, ограниченный 20 байтами (без многобайтовых символов)
      char headerBuf[21];
      snprintf(headerBuf, sizeof(headerBuf), "Tot:%d T:%d", total, uniqueTypes);
      if (strlen(headerBuf) > 20) headerBuf[20] = '\0';
      lcd.setCursor(0, 0);
      lcd.print(headerBuf);
      for (int _p = strlen(headerBuf); _p < 20; ++_p) lcd.print(' ');

      // Показываем по 3 типа на страницу с барами
      for (int i = 0; i < 3; ++i) {
        int idx = page * 3 + i;
        if (idx >= uniqueTypes) {
          // Очистка строки, если нет элемента
          lcd.setCursor(0, i + 1);
          for (int k = 0; k < 20; ++k) lcd.print(' ');
          continue;
        }
        // Короткое имя типа
        char shortName[5];
        strncpy(shortName, typeNames[idx], 4);
        shortName[4] = '\0';

        // Простой бар (макс 10 символов)
        int barLen = min(10, (typeCounts[idx] * 10) / (total > 0 ? total : 1));
        char bar[11];
        for (int b = 0; b < barLen; ++b) bar[b] = '#';
        bar[barLen] = '\0';

        char line[21];
        snprintf(line, sizeof(line), "%-4s:%2d %s", shortName, typeCounts[idx], bar);
        // Гарантируем, что строка не длиннее 20 байт
        if (strlen(line) > 20) line[20] = '\0';
        lcd.setCursor(0, i + 1);
        lcd.print(line);
        for (int k = strlen(line); k < 20; ++k) lcd.print(' ');
      }

      // Нижняя строка с подсказкой (показываем ASCII-совместную подсказку и очищаем остаток)
      // Нижняя строка - ASCII подсказка, не более 20 символов
      const char* bottom = "< >:nav Hold:exit";
      char bottomBuf[21];
      strncpy(bottomBuf, bottom, 20);
      bottomBuf[20] = '\0';
      lcd.setCursor(0, 3);
      lcd.print(bottomBuf);
      for (int _p = strlen(bottomBuf); _p < 20; ++_p) lcd.print(' ');
      needUpdate = false;
    }
    int delta = readEncoderDelta();
    if (delta != 0) {
      if (delta < 0 && page > 0) page--;
      else if (delta > 0 && (page + 1) * 3 < uniqueTypes) page++;
      needUpdate = true;
    }
    
    // Обработка нажатия кнопки
    static bool prevButtonHigh = true;
    bool currButtonLow = (digitalRead(pinSW) == LOW);
    if (currButtonLow && prevButtonHigh) {
      // Новое нажатие
      unsigned long pressStart = millis();
      while (digitalRead(pinSW) == LOW) {
        WDT_RESET();
        if (millis() - pressStart > 700) {
          // Долгое удержание — выход
          while (digitalRead(pinSW) == LOW) {
            delay(10);
          }
          exit = true;
          break;
        }
        delay(10);
      }
    }
    prevButtonHigh = !currButtonLow;
    
    WDT_RESET();
    delay(120);
  }
}

void selectEventTypeForFilter() {
  extern uint8_t selectedEventType;
  extern SafeLCD lcd;
  const char* types[] = {"All", "Wash Start", "Emergency", "Sys Start", "Sys Stop", "Stats Clear", "Settings", "State Chg", "Manual Op", "Menu Act", "Watchdog", "RTC Lost", "Sensor Err", "Unexp Chg", "State Rest"};
  int selected = selectedEventType;
  bool exit = false;
  bool needUpdate = true;
  while (!exit) {
    if (needUpdate) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Select Event Type:");
      lcd.setCursor(0, 1);
      if (selected == 0) lcd.print(">");
      else lcd.print(" ");
      lcd.print(types[selected]);
      if (selected < 14) {
        lcd.setCursor(0, 2);
        lcd.print(" ");
        lcd.print(types[selected + 1]);
      }
      needUpdate = false;
    }
    int delta = readEncoderDelta();
    if (delta != 0) {
      if (delta > 0 && selected < 14) selected++;
      else if (delta < 0 && selected > 0) selected--;
      needUpdate = true;
    }
    // Обработка нажатия кнопки
    static bool prevButtonHigh = true;
    bool currButtonLow = (digitalRead(pinSW) == LOW);
    if (currButtonLow && prevButtonHigh) {
      // Новое нажатие
      unsigned long pressStart = millis();
      while (digitalRead(pinSW) == LOW) {
        WDT_RESET();
        if (millis() - pressStart > 700) {
          // Долгое удержание — выход без выбора
          while (digitalRead(pinSW) == LOW) {
            delay(10);
          }
          exit = true;
          break;
        }
        delay(10);
      }
      if (!exit) {
        // Короткое нажатие — выбор
        selectedEventType = selected;
        exit = true;
      }
    }
    prevButtonHigh = !currButtonLow;
    WDT_RESET();
    delay(120);
  }
}

void selectDateRangeForFilter() {
  extern DateTime filterStartDate;
  extern DateTime filterEndDate;
  extern SafeLCD lcd;
  extern RTC_DS3231 rtc;
  // Простая настройка даты (только день/месяц/год)
  DateTime now = rtc.now();
  filterStartDate = DateTime(now.year(), now.month(), 1, 0, 0, 0); // Начало месяца
  filterEndDate = now;
  // Для простоты, используем текущий месяц
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Filter: This Month");
  lcd.setCursor(0, 1);
  char bufFrom[24];
  snprintf(bufFrom, sizeof(bufFrom), "From: %02d.%02d.%04d", filterStartDate.day(), filterStartDate.month(), filterStartDate.year());
  lcd.print(bufFrom);
  lcd.setCursor(0, 2);
  char bufTo[24];
  snprintf(bufTo, sizeof(bufTo), "To: %02d.%02d.%04d", filterEndDate.day(), filterEndDate.month(), filterEndDate.year());
  lcd.print(bufTo);
  delay(3000);
}

void displayFilteredEvents(uint8_t filterTypeParam) {
  extern uint8_t filterType;
  extern uint8_t selectedEventType;
  extern DateTime filterStartDate;
  extern DateTime filterEndDate;
  extern EventLog eventLogs[200];
  extern SafeLCD lcd;
  int total = getEventCount();
  EventLog filtered[200];
  int filteredCount = 0;

  // Фильтрация
  for (int i = 0; i < total; i++) {
    EventLog* ev = getEventByIndexSimple(i);
    if (!ev) continue;
    bool include = true;
    if (filterType == 1 && selectedEventType != 0 && ev->eventType != selectedEventType - 1) include = false;
    if (filterType == 2) {
      DateTime evDate(ev->year, ev->month, ev->day, ev->hour, ev->minute, 0);
      if (evDate < filterStartDate || evDate > filterEndDate) include = false;
    }
    if (include && filteredCount < 200) {
      // Insert at front to have newest-first order
      for (int k = filteredCount; k > 0; --k) filtered[k] = filtered[k-1];
      filtered[0] = *ev;
      filteredCount++;
    }
  }

  // Отображение (новейшие сперва)
  int currentIndex = 0;
  bool exit = false;
  while (!exit) {
    if (filteredCount == 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("No events match");
      lcd.setCursor(0, 1);
      lcd.print("filter criteria");
      delay(2000);
      exit = true;
      continue;
    }
    EventLog* event = &filtered[currentIndex];
    lcd.clear();
    // Line 0: date
    lcd.setCursor(0, 0);
    if (event->day < 10) lcd.print('0'); lcd.print(event->day); lcd.print('.');
    if (event->month < 10) lcd.print('0'); lcd.print(event->month); lcd.print('.'); lcd.print(event->year);
    // Line 1: time
    lcd.setCursor(0, 1);
    if (event->hour < 10) lcd.print('0'); lcd.print(event->hour); lcd.print(':');
    if (event->minute < 10) lcd.print('0'); lcd.print(event->minute);
    // Line 2: event name
    lcd.setCursor(0, 2);
    char eventDesc[21];
    strncpy(eventDesc, getEventText(event->eventType), sizeof(eventDesc)-1);
    eventDesc[sizeof(eventDesc)-1] = '\0';
    if (strlen(eventDesc) > 20) { eventDesc[20] = '\0'; }
    lcd.print(eventDesc);
    for (int i = strlen(eventDesc); i < 20; ++i) lcd.print(' ');
    // Line 3: code and importance with short level label
    lcd.setCursor(0, 3);
    char idxBuf[21];
    char levelLabel[4] = "[ ]";
    switch (event->level) {
      case LOG_DEBUG: levelLabel[1] = 'D'; break;
      case LOG_INFO: levelLabel[1] = 'I'; break;
      case LOG_WARNING: levelLabel[1] = 'W'; break;
      case LOG_ERROR: levelLabel[1] = 'E'; break;
      default: levelLabel[1] = ' '; break;
    }
    snprintf(idxBuf, sizeof(idxBuf), "Code:%03d %s", event->eventType, levelLabel);
    lcd.print(idxBuf);
    for (int i = strlen(idxBuf); i < 20; ++i) lcd.print(' ');

    int delta = readEncoderDelta();
    if (delta < 0 && currentIndex > 0) currentIndex--;
    else if (delta > 0 && currentIndex < filteredCount - 1) currentIndex++;
    if (isEncoderButtonPressed()) {
      waitForEncoderRelease();
      exit = true;
    }
    WDT_RESET();
    delay(120);
  }
}

void displayEventGraph() {
  extern SafeLCD lcd;
  extern RTC_DS3231 rtc;
  int total = getEventCount();
  if (total == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No events");
    delay(2000);
    return;
  }

  DateTime now = rtc.now();
  int currentDay = now.day();
  int currentMonth = now.month();
  int currentYear = now.year();

  // Счетчик событий по дням (последние 7 дней)
  int eventCounts[7] = {0};

  for (int i = 0; i < total; i++) {
    EventLog* ev = getEventByIndexSimple(i);
    if (!ev) continue;
    // Рассчитываем разницу в днях
    DateTime evDate(ev->year, ev->month, ev->day, 0, 0, 0);
    DateTime today(currentYear, currentMonth, currentDay, 0, 0, 0);
    TimeSpan diff = today - evDate;
    int daysDiff = diff.days();
    if (daysDiff >= 0 && daysDiff < 7) {
      eventCounts[6 - daysDiff]++;
    }
  }

  // Отображение
  // Обновляем экран один раз, так как информация статическая
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Events last 7 days:");
  for (int i = 0; i < 3; i++) {
    int dayIdx = 6 - i * 2; // 6,4,2
    if (dayIdx >= 0) {
      DateTime d = now - TimeSpan(dayIdx, 0, 0, 0);
      lcd.setCursor(0, i + 1);
      char perDayBuf[20];
      snprintf(perDayBuf, sizeof(perDayBuf), "%02d.%02d:%2d ", d.day(), d.month(), eventCounts[dayIdx]);
      lcd.print(perDayBuf);
      // Бар
      int barLen = min(10, eventCounts[dayIdx]);
      for (int b = 0; b < barLen; b++) lcd.print("#");
    }
  }
  lcd.setCursor(0, 3);
  lcd.print("Hold:exit");

  bool exit = false;
  while (!exit) {
    // Обработка нажатия кнопки
    static bool prevButtonHigh = true;
    bool currButtonLow = (digitalRead(pinSW) == LOW);
    if (currButtonLow && prevButtonHigh) {
      // Новое нажатие
      unsigned long pressStart = millis();
      while (digitalRead(pinSW) == LOW) {
        WDT_RESET();
        if (millis() - pressStart > 700) {
          // Долгое удержание — выход
          while (digitalRead(pinSW) == LOW) {
            delay(10);
          }
          exit = true;
          break;
        }
        delay(10);
      }
    }
    prevButtonHigh = !currButtonLow;
    
    WDT_RESET();
    delay(120);
  }
}

void showFilterTypeMenu() {
  extern uint8_t filterType;
  extern SafeLCD lcd;
  const char* options[] = {"All Events", "By Type", "By Date"};
  int selected = filterType;
  bool exit = false;
  bool needUpdate = true;
  while (!exit) {
    if (needUpdate) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Select Filter:");
      for (int i = 0; i < 3; i++) {
        lcd.setCursor(0, i + 1);
        if (i == selected) lcd.print(">");
        else lcd.print(" ");
        lcd.print(options[i]);
      }
      needUpdate = false;
    }
    int delta = readEncoderDelta();
    if (delta != 0) {
      if (delta > 0 && selected < 2) selected++;
      else if (delta < 0 && selected > 0) selected--;
      needUpdate = true;
    }
    // Обработка нажатия кнопки
    static bool prevButtonHigh = true;
    bool currButtonLow = (digitalRead(pinSW) == LOW);
    if (currButtonLow && prevButtonHigh) {
      // Новое нажатие
      unsigned long pressStart = millis();
      while (digitalRead(pinSW) == LOW) {
        WDT_RESET();
        if (millis() - pressStart > 700) {
          // Долгое удержание — выход без выбора
          while (digitalRead(pinSW) == LOW) {
            delay(10);
          }
          exit = true;
          break;
        }
        delay(10);
      }
      if (!exit) {
        // Короткое нажатие — выбор
        filterType = selected;
        if (filterType == 1) {
          // Выбор типа события
          selectEventTypeForFilter();
        } else if (filterType == 2) {
          // Выбор даты
          selectDateRangeForFilter();
        }
        exit = true;
      }
    }
    prevButtonHigh = !currButtonLow;
    WDT_RESET();
    delay(120);
  }
}
/**
 * @file event_logging.cpp
 * @brief Реализация логирования событий в SPIFFS
 * @details Функции сохранения, чтения и управления событиями
 */

#include <Arduino.h>
#include "event_logging.h"
#include "config.h"
#include "structures.h"
// SPIFFS is still used for web UI assets; logs themselves are stored on
// SD when a card is present.  The old SPIFFS-only code paths remain as a
// fallback when no card is installed.
// removed SPIFFS-specific code
#include <SD.h>
#include "menu.h"
#include "display.h"
#include "encoder.h"

// forward declarations
static const char* getLogSourceString(LogSource src);

// ============ ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ДЛЯ ФИЛЬТРА ============

uint8_t filterType = 0; // 0 - все, 1 - по типу, 2 - по дате
uint8_t selectedEventType = 0; // Для фильтра по типу
DateTime filterStartDate; // Дата начала
DateTime filterEndDate;   // Дата конца

// ============ ИНИЦИАЛИЗАЦИЯ ФАЙЛОВОЙ СИСТЕМЫ ============

/// Инициализирует файловую систему SPIFFS
// attempt to convert any old binary-format log file to new text format
void migrateOldBinaryLog() {
    const char *path = "/events.log";
    // pick filesystem based on card presence
    File f;
    if (logsOnSD()) {
      f = SD.open(path, "r");
    } else {
// removed SPIFFS-specific code
    }
    if (!f) return;
    size_t sz = f.size();
    if (sz >= sizeof(EventLog) && (sz % sizeof(EventLog)) == 0) {
        EventLog tmp;
        if (f.read((uint8_t*)&tmp, sizeof(tmp)) == sizeof(tmp)) {
            // crude plausibility check on date fields
            if (tmp.month >= 1 && tmp.month <= 12 && tmp.day >= 1 && tmp.day <= 31) {
                // looks binary, perform conversion
                f.close();
                std::vector<EventLog> list;
                File fr;
                if (logsOnSD()) {
                  fr = SD.open(path, "r");
                } else {
// removed SPIFFS-specific code
                }
                while (fr && fr.available() >= sizeof(EventLog)) {
                    EventLog ev;
                    fr.read((uint8_t*)&ev, sizeof(ev));
                    list.push_back(ev);
                }
                if (fr) fr.close();
                File fw;
                if (logsOnSD()) {
                  fw = SD.open(path, "w");
                } else {
// removed SPIFFS-specific code
                }
                if (fw) {
                    for (auto &e : list) {
                        char buf[128];
                        int len = snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d - type:%02d param:%u lvl:%d\n",
                                           e.day, e.month, e.year, e.hour, e.minute,
                                           e.eventType, e.eventParam, (int)e.level);
                        fw.write((uint8_t*)buf, len);
                    }
                    fw.close();
                }
                Serial.println("[EventLog] migrated old binary log to text format");
            }
        }
    }
    f.close();
}

void initSPIFFS() {
  // still initialise SPIFFS because we serve UI pages from it; we no longer
  // create or touch the events.log here when an SD card is available.
// removed SPIFFS-specific code
}


// ============ СОХРАНЕНИЕ СОБЫТИЙ ============

/// Сохраняет событие в журнал (перегрузка с двумя параметрами)
void saveEventLog(LogLevel level, uint8_t eventType, uint16_t param) {
  saveEventLog(level, eventType, param, 0);
}

/// Сохраняет событие в журнал (в памяти и SPIFFS)
void saveEventLog(LogLevel level, uint8_t eventType, uint16_t param, uint16_t additionalInfo) {
  extern EventLog eventLogs[200];
  extern uint8_t eventLogIndex;
  extern RTC_DS3231 rtc;
  
  // dedupe state kept across calls
  static EventLog lastPersisted = {0};
  static uint16_t repeatCount = 0;

  // Получаем текущую дату/время (безопасно — может вызываться до инициализации RTC)
  DateTime now;
  extern SystemFlags flags;
  if (flags.rtcInitialized) {
    now = rtc.now();
  } else {
    // RTC ещё не инициализирован — используем заглушку и избегаем I2C-вызова
    now = DateTime(1970, 1, 1, 0, 0, 0);
  }
  
  // Сохраняем в ОЗУ буфер (циклический)
  EventLog entry;
  entry.day = now.day();
  entry.month = now.month();
  entry.year = now.year();
  entry.hour = now.hour();
  entry.minute = now.minute();
  entry.eventType = eventType;
  entry.eventParam = param;
  entry.level = level;
  entry.additionalInfo = additionalInfo; // treat as source or extra
  eventLogs[eventLogIndex] = entry;
  int savedIndex = eventLogIndex;
  eventLogIndex = (eventLogIndex + 1) % 200;

  // Debug: log to Serial that we saved to RAM.  This message is useful
  // during development but often looks like a duplicate of the "real"
  // event output.  Disable by defining EVENT_LOG_NO_SERIAL_DEBUG in the
  // build (or simply comment out the line below) if you want quiet
  // operation on a production device.
#ifndef EVENT_LOG_NO_SERIAL_DEBUG
  Serial.printf("[EventLog] saved to RAM idx=%d type=%d level=%d param=%u src=%u\n", savedIndex, eventType, level, (unsigned int)param, additionalInfo);
#endif

  // Determine whether logging should be persistent based on the filter
  extern Preferences preferences;
  uint32_t filterMask = preferences.getULong(PREF_KEY_LOG_FILTER, (uint32_t)PREF_KEY_LOG_FILTER_DEFAULT);

  auto mapEventToBit = [&](uint8_t evType, LogLevel lvl)->uint32_t {
    switch(evType) {
      case EVENT_EMERGENCY: return LOG_FILTER_EMERGENCY;
      case EVENT_WATCHDOG_RESET: return LOG_FILTER_WATCHDOG;
      case EVENT_SENSOR_ERROR: return LOG_FILTER_SENSOR;
      case EVENT_SETTINGS_CHANGE: return LOG_FILTER_SETTINGS;
      case EVENT_MANUAL_OPERATION: return LOG_FILTER_MANUAL;
      case EVENT_FILTER_WASH_START: return LOG_FILTER_MANUAL;
      case EVENT_WIFI_CONNECTED: return LOG_FILTER_WIFI;
      case EVENT_WIFI_AP_STARTED: return LOG_FILTER_WIFI;
      case EVENT_SYSTEM_START:
      case EVENT_SYSTEM_STOP: return LOG_FILTER_STATECHANGE;
      case EVENT_SYSTEM_STATE_CHANGE:
      case EVENT_UNEXPECTED_STATE_CHANGE: return LOG_FILTER_STATECHANGE;
      default: return 0;
    }
  };    

  bool shouldWrite = false;
  if (level == LOG_ERROR && (filterMask & LOG_FILTER_ERRORS)) shouldWrite = true;
  uint32_t bit = mapEventToBit(eventType, level);
  if (bit != 0 && (filterMask & bit)) shouldWrite = true;

  if (shouldWrite) {
    // dedupe: same event within same minute?
    if (entry.eventType == lastPersisted.eventType &&
        entry.eventParam == lastPersisted.eventParam &&
        entry.level == lastPersisted.level &&
        entry.additionalInfo == lastPersisted.additionalInfo &&
        entry.day == lastPersisted.day &&
        entry.month == lastPersisted.month &&
        entry.year == lastPersisted.year &&
        entry.hour == lastPersisted.hour &&
        entry.minute == lastPersisted.minute) {
      repeatCount++;
      return; // skip writing duplicate
    }
    // if we had repeats, write a summary line first
    if (repeatCount > 0) {
      char rpt[100];
      int rlen = snprintf(rpt, sizeof(rpt), "(previous event repeated %u times)\n", repeatCount);
      if (logsOnSD()) {
        File f = SD.open("/events.log", FILE_APPEND);
        if (f) { f.write((uint8_t*)rpt, rlen); f.close(); }
      } else {
// removed SPIFFS-specific code
      }
      repeatCount = 0;
    }
    // produce a descriptive string for the event
    char desc[80];
    getEventTypeLong(level, eventType, param, desc, sizeof(desc));

    // write a human-readable line including the description
    char buf[200];
    int len = snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d:%02d - type:%02d param:%u lvl:%d - %s",
                       entry.day, entry.month, entry.year,
                       entry.hour, entry.minute, now.second(),
                       entry.eventType, entry.eventParam, (int)entry.level,
                       desc);
    // append source if we've encoded one in additionalInfo
    if (additionalInfo != SRC_NONE && additionalInfo <= SRC_SYSTEM) {
      const char* srcStr = getLogSourceString((LogSource)additionalInfo);
      len += snprintf(buf + len, sizeof(buf) - len, " %s", srcStr);
    }
    // finish line break
    if (len < (int)sizeof(buf) - 1) {
      buf[len++] = '\n';
      buf[len] = '\0';
    }
    bool written = false;
    if (logsOnSD()) {
      // allow a few intermittent SD write failures before giving up
      static int sdWriteErrors = 0;
      const int MAX_SD_ERRORS = 3;

      File f = SD.open("/events.log", FILE_APPEND);
      if (f) {
        if (f.write((uint8_t*)buf, len) == len) {
          written = true;
          sdWriteErrors = 0;       // reset counter on success
        } else {
          sdWriteErrors++;
          Serial.printf("[ERROR] Partial write event %d to SD (err %d)\n", eventType, sdWriteErrors);
        }
        f.close();
      } else {
        sdWriteErrors++;
        Serial.printf("[ERROR] Failed to open SD for event %d (err %d)\n", eventType, sdWriteErrors);
      }

      if (!written && sdWriteErrors >= MAX_SD_ERRORS) {
        extern bool sdPresent;
        sdPresent = false;
        Serial.println("[SD] too many errors, disabling SD");
      }
    }
    if (!written) {
      // SPIFFS support dropped: nothing more we can do
      Serial.printf("[ERROR] Failed to save event %d\n", eventType);
    }
  } else {
    Serial.printf("[DEBUG] Event %d skipped by log filter\n", eventType);
  }
}


/// Сохраняет журнал статистики WDT
void saveWDTStats() {
  extern WDTStats wdtStats;
  
  if (logsOnSD()) {
    File f = SD.open("/wdt_stats.bin", "w");
    if (f) {
      f.write((uint8_t*)&wdtStats, sizeof(wdtStats));
      f.close();
    }
// removed SPIFFS-specific code
  }
}

// Export/rotation helpers consolidated later in this file (keep single implementations)

// ============ ЗАГРУЗКА СОБЫТИЙ ============

/// Загружает события из SPIFFS в ОЗУ буфер
/// Helper to parse a text line into an EventLog struct
static bool parseLogLine(const String &line, EventLog &out) {
    int d,m,y,h,mi,type,param,lvl;
    int scanned = sscanf(line.c_str(), "%d.%d.%d %d:%d - type:%d param:%d lvl:%d",
                         &d,&m,&y,&h,&mi,&type,&param,&lvl);
    if (scanned != 8) return false;
    out.day = d;
    out.month = m;
    out.year = y;
    out.hour = h;
    out.minute = mi;
    out.eventType = (uint8_t)type;
    out.eventParam = (uint8_t)param;
    out.level = (LogLevel)lvl;
    return true;
}

void loadEventLogs() {
  extern EventLog eventLogs[200];
  extern uint8_t eventLogIndex;
  
  File f;
  if (logsOnSD()) {
    f = SD.open("/events.log", "r");
  } else {
// removed SPIFFS-specific code
  }
  if (!f) return;

  int index = 0;
  while (f.available() && index < 200) {
    String line = f.readStringUntil('\n');
    if (line.length() == 0) continue;
    EventLog ev;
    if (parseLogLine(line, ev)) {
      eventLogs[index++] = ev;
    }
  }
  eventLogIndex = index % 200;
  f.close();
}

// NOTE: after moving logs to SD we no longer need a generic SPIFFS->SD
// copy helper.  The only use of copyFileToSD was in exportLogsToSD, which is
// now reimplemented to rotate the SD logfile directly.  We'll keep this
// function around in case older code paths still refer to it but mark it
// deprecated.
static bool copyFileToSD(const char* spiffsPath, const char* sdPath) {
  // deprecated; behave like false
  (void)spiffsPath;
  (void)sdPath;
  return false;
}

/// Экспортирует весь лог событий из SPIFFS на SD и очищает SPIFFS-файл.
bool exportLogsToSD() {
  // Robustly archive the current text log to a dated file on SD card
  WDT_RESET();
  extern bool sdPresent;
  if (!sdPresent) {
    Serial.println("[SD] exportLogsToSD: sdPresent false");
    return false;
  }
  extern RTC_DS3231 rtc;
  DateTime now = rtc.now();
  char archiveName[64];
  // place archives under /logs directory
  snprintf(archiveName, sizeof(archiveName), "/logs/events-%04d%02d%02d.log",
           now.year(), now.month(), now.day());

  if (logsOnSD()) {
    // ensure archive directory exists
    if (!SD.exists("/logs")) {
      SD.mkdir("/logs");
    }
    // open source log; if missing, just recreate an empty one and succeed
    File src = SD.open("/events.log", FILE_READ);
    if (!src) {
      File f = SD.open("/events.log", FILE_WRITE);
      if (f) f.close();
      return true;
    }

    // remove any existing archive (could alternatively add a suffix)
    if (SD.exists(archiveName)) {
      SD.remove(archiveName);
    }

    File dst = SD.open(archiveName, FILE_WRITE);
    if (!dst) {
      src.close();
      return false;
    }

    // copy in small chunks to avoid large RAM usage
    uint8_t buf[64];
    size_t n;
    while ((n = src.read(buf, sizeof(buf))) > 0) {
      if (dst.write(buf, n) != n) {
        src.close();
        dst.close();
        return false;
      }
    }
    src.close();
    dst.close();

    // clear the live log by recreating it empty
    File f = SD.open("/events.log", FILE_WRITE);
    if (f) f.close();

    Serial.printf("[SD] exportLogsToSD -> ok, target=%s\n", archiveName);
    return true;
  } else {
    // no SPIFFS support; nothing to do
    return false;
  }
}

/// Загружает статистику WDT из хранилища (SD или SPIFFS)
void loadWDTStats() {
  extern WDTStats wdtStats;
  
  if (logsOnSD()) {
    if (!SD.exists("/wdt_stats.bin")) {
      memset(&wdtStats, 0, sizeof(wdtStats));
      return;
    }
    File f = SD.open("/wdt_stats.bin", "r");
    if (f) {
      f.read((uint8_t*)&wdtStats, sizeof(wdtStats));
      f.close();
    }
  }
}

// ============ УПРАВЛЕНИЕ ЛОГАМИ ============

/// Удаляет все события из журнала
void clearEventLogs() {
  extern EventLog eventLogs[200];
  extern uint8_t eventLogIndex;
  
  memset(eventLogs, 0, sizeof(eventLogs));
  eventLogIndex = 0;
  
  if (logsOnSD()) {
    SD.remove("/events.log");
    File f = SD.open("/events.log", "w");
    if (f) f.close();
// removed SPIFFS-specific code
  }
}

/// Удаляет старые события (старше N дней)
void deleteOldEventLogs(uint32_t maxAgeSeconds) {
  extern EventLog eventLogs[200];
  extern uint8_t eventLogIndex;
  extern RTC_DS3231 rtc;

  DateTime now = rtc.now();

  EventLog filtered[200];
  int newCount = 0;

  for (int i = 0; i < 200; i++) {
    if (eventLogs[i].eventType == 0) continue;

    DateTime eventTime(eventLogs[i].year,
                       eventLogs[i].month,
                       eventLogs[i].day,
                       eventLogs[i].hour,
                       eventLogs[i].minute,
                       0);

    uint32_t age = 0;
    if (now.unixtime() >= eventTime.unixtime()) {
      age = now.unixtime() - eventTime.unixtime();
    }

    if (age <= maxAgeSeconds) {
      if (newCount < 200) {
        filtered[newCount++] = eventLogs[i];
      }
    }
  }

  memset(eventLogs, 0, sizeof(eventLogs));
  for (int i = 0; i < newCount; i++) {
    eventLogs[i] = filtered[i];
  }
  eventLogIndex = newCount % 200;

  // rewrite persistent log in text format on appropriate medium
  if (logsOnSD()) {
    SD.remove("/events.log");
    File f = SD.open("/events.log", "w");
    if (f) {
      for (int i = 0; i < newCount; i++) {
        EventLog &e = eventLogs[i];
        char buf[128];
        int len = snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d - type:%02d param:%u lvl:%d\n",
                           e.day, e.month, e.year, e.hour, e.minute,
                           e.eventType, e.eventParam, (int)e.level);
        f.write((uint8_t*)buf, len);
      }
      f.close();
    }
  } else {
// removed SPIFFS-specific code
  }
}

/// Получает количество событий в журнале
int getEventLogCount() {
  extern EventLog eventLogs[200];
  
  int count = 0;
  for (int i = 0; i < 200; i++) {
    if (eventLogs[i].eventType != 0) {
      count++;
    }
  }
  return count;
}

/// Приводит RAM и файлы в соответствие, удаляя события старше указанного возраста.
/// Архивные файлы на SD также очищаются.
void pruneEventLogs(uint32_t maxAgeSeconds) {
  // сначала RAM
  deleteOldEventLogs(maxAgeSeconds);
  int total = getEventLogCount();

  if (logsOnSD()) {
    SD.remove("/events.log");
    File f = SD.open("/events.log", "w");
    if (f) {
      for (int i = 0; i < total; i++) {
        EventLog *e = getEventByIndexSimple(i);
        if (!e) continue;
        char buf[128];
        int len = snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d - type:%02d param:%u lvl:%d\n",
                           e->day, e->month, e->year, e->hour, e->minute,
                           e->eventType, e->eventParam, (int)e->level);
        f.write((uint8_t*)buf, len);
      }
      f.close();
    }
    // prune archives
    File root = SD.open("/");
    if (root) {
      File file = root.openNextFile();
      while (file) {
        String name = String(file.name());
        if (name.startsWith("/events-") && name.endsWith(".log")) {
          int y,m,d;
          if (sscanf(name.c_str(), "/events-%4d%2d%2d.log", &y, &m, &d) == 3) {
            extern RTC_DS3231 rtc;
            DateTime now = rtc.now();
            DateTime dt(y,m,d,0,0,0);
            uint32_t age = now.unixtime() - dt.unixtime();
            if (age > maxAgeSeconds) {
              SD.remove(name);
            }
          }
        }
        file.close();
        file = root.openNextFile();
      }
      root.close();
    }
// removed SPIFFS-specific code
  }
}

/// Экспортирует весь журнал в читаемый текст (строки разделены \n)
String exportEventsAsText() {
  extern EventLog eventLogs[200];
  String out = "";
  for (int i = 0; i < 200; ++i) {
    EventLog &e = eventLogs[i];
    if (e.eventType == 0) continue;
    char desc[80];
    getEventTypeLong(e.level, e.eventType, e.eventParam, desc, sizeof(desc));
    char buf[200];
    snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d - type:%02d param:%u lvl:%d - %s\n",
             e.day, e.month, e.year, e.hour, e.minute,
             e.eventType, e.eventParam, (int)e.level,
             desc);
    out += String(buf);
  }
  if (out.length() == 0) out = "(no events)\n";
  return out;
}

/// Получает количество событий (обертка для совместимости)
int getEventCount() {
  return getEventLogCount();
}

/// Получает событие по индексу
EventLog* getEventLog(int index) {
  extern EventLog eventLogs[200];
  
  if (index < 0 || index >= 200) {
    return NULL;
  }
  return &eventLogs[index];
}

/// Получает событие по индексу (возвращает n-ое событие в хронологическом порядке: 0=oldest, N-1=newest)
EventLog* getEventByIndexSimple(int index) {
  extern EventLog eventLogs[200];
  extern uint8_t eventLogIndex; // next write position

  int total = getEventLogCount();
  if (index < 0 || index >= total) return NULL;

  // Если буфер ещё не заполнился полностью — записи упакованы в [0..total-1]
  int start = (total < 200) ? 0 : eventLogIndex; // старейший элемент

  int storageIndex = (start + index) % 200;
  // На всякий случай: если слот пустой — пытаемся найти ближайшую непустую запись
  if (eventLogs[storageIndex].eventType == 0) {
    // Ищем вперёд до тех пор, пока не найдём непустой слот
    int i = storageIndex;
    for (int k = 0; k < 200; ++k) {
      if (eventLogs[i].eventType != 0) return &eventLogs[i];
      i = (i + 1) % 200;
    }
    return NULL;
  }
  return &eventLogs[storageIndex];
}

// ============ ФИЛЬТРАЦИЯ СОБЫТИЙ ============

/// Фильтрует события по кодам
int filterEventsByCode(uint16_t eventCode, int* outIndices, int maxCount) {
  extern EventLog eventLogs[200];
  extern int filteredEventCount;
  extern int filteredEventIndices[200];
  
  filteredEventCount = 0;
  
  for (int i = 0; i < 200 && filteredEventCount < maxCount; i++) {
    if (eventLogs[i].eventType == (uint8_t)eventCode) {
      filteredEventIndices[filteredEventCount] = i;
      filteredEventCount++;
    }
  }
  
  if (outIndices) {
    memcpy(outIndices, filteredEventIndices, filteredEventCount * sizeof(int));
  }
  
  return filteredEventCount;
}

/// Получает текст события по его коду
const char* getEventText(uint16_t eventCode) {
  // Return English or Russian description depending on language preference
  bool en = isEnglish();
  switch(eventCode) {
    case EVENT_FILTER_WASH_START: return en ? "Filter wash started"          : "Начало промывки фильтра";
    case EVENT_EMERGENCY:          return en ? "Emergency triggered"          : "Аварийный режим";
    case EVENT_SYSTEM_START:       return en ? "System started"               : "Система запущена";
    case EVENT_SYSTEM_STOP:        return en ? "System stopped"               : "Система остановлена";
    case EVENT_STATS_CLEAR:        return en ? "Statistics cleared"           : "Статистика очищена";
    case EVENT_SETTINGS_CHANGE:    return en ? "Settings changed"             : "Изменены настройки";
    case EVENT_SYSTEM_STATE_CHANGE:return en ? "State change"                 : "Смена состояния";
    case EVENT_MANUAL_OPERATION:   return en ? "Manual operation"             : "Ручная операция";
    case EVENT_MENU_ACTION:        return en ? "Menu action"                 : "Действие меню";
    case EVENT_WATCHDOG_RESET:     return en ? "Watchdog reset"              : "Сброс сторожевого таймера";
    case EVENT_RTC_LOST_POWER:     return en ? "RTC lost power"              : "RTC потерял питание";
    case EVENT_SENSOR_ERROR:       return en ? "Sensor error"                : "Ошибка датчика";
    case EVENT_UNEXPECTED_STATE_CHANGE:
                                  return en ? "Unexpected state change"    : "Неожиданная смена";
    case EVENT_RUNTIME_STATE_RESTORED:
                                  return en ? "Runtime state restored"     : "Восстановлено состояние";
    case EVENT_WIFI_CONNECTED:     return en ? "Wi-Fi connected"             : "Wi-Fi подключен";
    case EVENT_WIFI_AP_STARTED:    return en ? "Wi-Fi AP started"            : "Wi-Fi AP запущена";
    default:                      return en ? "Unknown event"               : "Неизвестное событие";
  }
}

/// Получить краткое описание события
void getEventTypeShort(LogLevel level, uint8_t eventType, uint16_t param, char* buffer, uint8_t size) {
  if (!buffer || size == 0) return;
  
  // Префикс уровня
  const char* levelPrefix = "";
  switch (level) {
    case LOG_DEBUG: levelPrefix = "[D] "; break;
    case LOG_INFO: levelPrefix = "[I] "; break;
    case LOG_WARNING: levelPrefix = "[W] "; break;
    case LOG_ERROR: levelPrefix = "[E] "; break;
  }
  
  const char* text = getEventText(eventType);
  char temp[32];
  if (eventType == EVENT_SYSTEM_STATE_CHANGE) {
    char stateDesc[16];
    getStateDescription(param, stateDesc, sizeof(stateDesc));
    snprintf(temp, sizeof(temp), "%s%s", levelPrefix, stateDesc);
  } else {
    snprintf(temp, sizeof(temp), "%s%s", levelPrefix, text);
  }
  strncpy(buffer, temp, size - 1);
  buffer[size - 1] = '\0';
}

/// Получить развернутое описание события
// Convert numeric source to human text
static const char* getLogSourceString(LogSource src) {
  switch (src) {
    case SRC_WEB: return "[web]";
    case SRC_MQTT: return "[mqtt]";
    case SRC_BUTTON: return "[button]";
    case SRC_SYSTEM: return "[system]";
    default: return "";
  }
}

void getEventTypeLong(LogLevel level, uint8_t eventType, uint16_t param, char* buffer, uint8_t size) {
  if (!buffer || size == 0) return;
  // Start with short description to include level prefix
  getEventTypeShort(level, eventType, param, buffer, size);
  // Append parameter details for certain events
  char extra[80] = "";
  switch(eventType) {
    case EVENT_SYSTEM_STATE_CHANGE: {
      char stateDesc[24];
      getStateDescription(param, stateDesc, sizeof(stateDesc));
      snprintf(extra, sizeof(extra), "; state=%s", stateDesc);
      break;
    }
    case EVENT_MANUAL_OPERATION: {
      char opDesc[32];
      getManualOperationDescription(param, opDesc, sizeof(opDesc));
      snprintf(extra, sizeof(extra), "; op=%s", opDesc);
      break;
    }
    case EVENT_SENSOR_ERROR: {
      snprintf(extra, sizeof(extra), "; code=%u", (unsigned)param);
      break;
    }
    case EVENT_SETTINGS_CHANGE: {
      // param may hold setting id, but new value stored in additionalInfo
      char setDesc[32];
      getSettingDescription(param, setDesc, sizeof(setDesc));
      snprintf(extra, sizeof(extra), "; %s", setDesc);
      break;
    }
    default:
      break;
  }
  if (extra[0] != '\0') {
    strncat(buffer, extra, size - strlen(buffer) - 1);
  }
  // append source if available (additionalInfo handled externally when calling)
  // user of this function should append source separately if they want
}

/// Описание типа события
void getEventTypeDescription(uint8_t eventType, char* buffer, uint8_t size) {
  getEventTypeShort(LOG_INFO, eventType, 0, buffer, size);
}

/// Описание состояния системы
void getStateDescription(uint8_t state, char* buffer, uint8_t size) {
  if (!buffer || size == 0) return;
  const char* text = "Неизв.";
  switch (state) {
    case STATE_IDLE: text = "Ожидание"; break;
    case STATE_FILLING_TANK1: text = "Заполнение"; break;
    case STATE_OZONATION: text = "Озонирование"; break;
    case STATE_AERATION: text = "Аэрация"; break;
    case STATE_SETTLING: text = "Отстаивание"; break;
    case STATE_FILTRATION: text = "Фильтрация"; break;
    case STATE_BACKWASH: text = "Обратная промывка"; break;
    default: text = "Неизв."; break;
  }
  strncpy(buffer, text, size - 1);
  buffer[size - 1] = '\0';
}

/// Описание типа настройки
void getSettingDescription(uint8_t settingType, char* buffer, uint8_t size) {
  if (!buffer || size == 0) return;
  const char* text = "Setting";
  switch (settingType) {
    case SETTING_TIMER_SETLING: text = "Settling"; break;
    case SETTING_TIMER_AERATION: text = "Aeration"; break;
    case SETTING_TIMER_OZONATION: text = "Ozonation"; break;
    case SETTING_TIMER_FILTER_WASH: text = "Filter Wash"; break;
    case SETTING_TIMER_BACKLIGHT: text = "Backlight"; break;
    case SETTING_FILTER_PERIOD: text = "Filter Period"; break;
    case SETTING_TANK1_MIN: text = "Tank1 Min"; break;
    case SETTING_TANK1_MAX: text = "Tank1 Max"; break;
    case SETTING_TANK2_MIN: text = "Tank2 Min"; break;
    case SETTING_TANK2_MAX: text = "Tank2 Max"; break;
    default: text = "Unknown"; break;
  }
  strncpy(buffer, text, size - 1);
  buffer[size - 1] = '\0';
}

/// Описание ручной операции
void getManualOperationDescription(uint8_t operation, char* buffer, uint8_t size) {
  if (!buffer || size == 0) return;
  const char* text = "Неизв.";
  switch (operation) {
    case MANUAL_PUMP1_ON:  text = "Насос1 ВКЛ"; break;
    case MANUAL_PUMP1_OFF: text = "Насос1 ВЫКЛ"; break;
    case MANUAL_PUMP2_ON:  text = "Насос2 ВКЛ"; break;
    case MANUAL_PUMP2_OFF: text = "Насос2 ВЫКЛ"; break;
    case MANUAL_AERATION_ON:  text = "Аэрация ВКЛ"; break;
    case MANUAL_AERATION_OFF: text = "Аэрация ВЫКЛ"; break;
    case MANUAL_OZONE_ON:  text = "Озон ВКЛ"; break;
    case MANUAL_OZONE_OFF: text = "Озон ВЫКЛ"; break;
    case MANUAL_FILTER_ON:  text = "Фильтр ВКЛ"; break;
    case MANUAL_FILTER_OFF: text = "Фильтр ВЫКЛ"; break;
    case MANUAL_BACKWASH_ON:  text = "Обратка ВКЛ"; break;
    case MANUAL_BACKWASH_OFF: text = "Обратка ВЫКЛ"; break;
    default: text = "Неизв."; break;
  }
  strncpy(buffer, text, size - 1);
  buffer[size - 1] = '\0';
}

// ============ ПРОВЕРКА И ОЧИСТКА ============

/// Проверяет и удаляет старые логи (если нужно)
void checkAndCleanOldLogs() {
  // Очищаем логи старше 30 дней, но не чаще 1 раза в 6 часов
  static unsigned long lastCleanup = 0;
  const unsigned long cleanupInterval = 6UL * 60UL * 60UL * 1000UL; // 6 часов

  unsigned long now = millis();
  if (lastCleanup == 0 || (now - lastCleanup) >= cleanupInterval) {
    lastCleanup = now;
    deleteOldEventLogs(30 * 24 * 3600UL);
  }
}

// `printEventLogs()` and `exportEventLogsCSV()` removed — use `exportEventsAsText()` or SPIFFS export utilities.


// Возвращает время последнего события указанного типа и параметра
// Ищет от конца к началу и заполняет hour/minute
bool getLastEventTime(uint8_t eventType, uint16_t param, uint8_t &hour, uint8_t &minute) {
  int total = getEventCount();
  for (int i = total - 1; i >= 0; --i) {
    EventLog* ev = getEventByIndexSimple(i);
    if (!ev) continue;
    if (ev->eventType == eventType && ev->eventParam == param) {
      hour = ev->hour;
      minute = ev->minute;
      return true;
    }
  }
  return false;
}

// ============ ОТОБРАЖЕНИЕ НА LCD ============

/// Отображение одной записи в списке (стандартный формат)
void displayEventList(int currentIndex, int totalEvents) {
  extern SafeLCD lcd;
  // Map display index (0 = newest) to storage index (0 = oldest)
  int mappedIndex = (totalEvents > 0) ? (totalEvents - 1 - currentIndex) : currentIndex;
  EventLog* event = getEventByIndexSimple(mappedIndex);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Event ");
  lcd.print(currentIndex + 1);
  lcd.print("/");
  lcd.print(totalEvents);

  char eventDesc[20];
  getEventTypeShort(event->level, event->eventType, event->eventParam, eventDesc, sizeof(eventDesc));
  lcd.setCursor(0, 1);
  lcd.print(eventDesc);

  // Show time on line 2 (HH:MM) and date on line 3 (DD.MM)
  lcd.setCursor(0, 2);
  if (event->eventType != 0) {
    if (event->hour < 10) lcd.print('0');
    lcd.print(event->hour);
    lcd.print(":");
    if (event->minute < 10) lcd.print('0');
    lcd.print(event->minute);
  } else {
    lcd.print("No data");
  }

  lcd.setCursor(0, 3);
  if (event->eventType != 0) {
    if (event->day < 10) lcd.print('0');
    lcd.print(event->day); lcd.print('.');
    if (event->month < 10) lcd.print('0');
    lcd.print(event->month);
  } else {
    for (int i = 0; i < 5; ++i) lcd.print(' ');
  }
}

/// Отображение деталей события
void displayEventDetails(int eventIndex, int totalEvents) {
  extern SafeLCD lcd;
  // Map display index (0 = newest) to storage index
  int mappedIndex = (totalEvents > 0) ? (totalEvents - 1 - eventIndex) : eventIndex;
  EventLog* event = getEventByIndexSimple(mappedIndex);
  if (!event) return;

  lcd.clear();
  // Line 0: date DD.MM.YYYY
  lcd.setCursor(0, 0);
  if (event->day < 10) lcd.print('0');
  lcd.print(event->day); lcd.print('.');
  if (event->month < 10) lcd.print('0');
  lcd.print(event->month); lcd.print('.');
  lcd.print(event->year);

  // Line 1: time HH:MM
  lcd.setCursor(0, 1);
  if (event->hour < 10) lcd.print('0');
  lcd.print(event->hour);
  lcd.print(':');
  if (event->minute < 10) lcd.print('0');
  lcd.print(event->minute);

  // Line 2: event name (possibly truncated)
  lcd.setCursor(0, 2);
  {
    String txt = getEventText(event->eventType);
    if (txt.length() > 20) txt = txt.substring(0, 20);
    lcd.print(txt);
    for (int i = txt.length(); i < 20; ++i) lcd.print(' ');
  }

  // Line 3: code and importance (level)
  lcd.setCursor(0, 3);
  char buf[21];
  // Format level label as [D]/[I]/[W]/[E]
  char levelLabel[4] = "[ ]";
  switch (event->level) {
    case LOG_DEBUG: levelLabel[1] = 'D'; break;
    case LOG_INFO: levelLabel[1] = 'I'; break;
    case LOG_WARNING: levelLabel[1] = 'W'; break;
    case LOG_ERROR: levelLabel[1] = 'E'; break;
    default: levelLabel[1] = ' '; break;
  }
  snprintf(buf, sizeof(buf), "Code:%03d %s", event->eventType, levelLabel);
  lcd.print(buf);
  for (int i = strlen(buf); i < 20; ++i) lcd.print(' ');
}

// Простой браузер логов (использует энкодер)
void eventLogBrowser(int totalEvents) {
  extern SafeLCD lcd;
  if (totalEvents <= 0) return;

  int currentIndex = 0;
  int lastIndex = -1;
  unsigned long viewStart = millis();

  // Если кнопка всё ещё нажата (например, от входа в меню), дождёмся отпускания
  unsigned long waitStart = millis();
  while (digitalRead(pinSW) == LOW) {
    if (millis() - waitStart > 1000) break; // не вешаемся, если долго держат
    delay(10);
  }
  delay(50); // небольшая стабилизация после отпускания

  while (true) {
    WDT_RESET();
    // Таймаут 10 минут
    if (millis() - viewStart > 10UL * 60UL * 1000UL) break;

    int delta = readEncoderDelta();
    if (delta != 0) {
      currentIndex += delta;
      if (currentIndex < 0) currentIndex = 0;
      if (currentIndex >= totalEvents) currentIndex = totalEvents - 1;
      lastIndex = -1; // форсировать перерисовку
    }
    if (currentIndex != lastIndex) {
      displayEventList(currentIndex, totalEvents);
      lastIndex = currentIndex;
    }

    // Обрабатываем только новое нажатие (переход HIGH->LOW), игнорируя удержание
    static bool prevButtonHigh = true;
    bool currButtonLow = (digitalRead(pinSW) == LOW);
    if (currButtonLow && prevButtonHigh) {
      // Новое падение — начало нажатия
      unsigned long pressStart = millis();
      while (digitalRead(pinSW) == LOW) {
        WDT_RESET();
        if (millis() - pressStart > 700) {
          // Долгое удержание — дождаться отпускания и выйти
          while (digitalRead(pinSW) == LOW) {
            delay(10);
          }
          extern volatile bool encoderButtonPressed;
          encoderButtonPressed = false;
          return; // выход из браузера
        }
        delay(10);
      }
      // Короткое нажатие — показать детали
      {
        extern volatile bool encoderButtonPressed;
        encoderButtonPressed = false;
      }
          displayEventDetails(currentIndex, totalEvents);

      // Ждём явного нажатия кнопки для выхода из детального просмотра
      unsigned long waitStart = millis();
      while (true) {
        WDT_RESET();
        if (digitalRead(pinSW) == LOW) {
          // Ожидаем отпускания кнопки
          unsigned long pressStart = millis();
          while (digitalRead(pinSW) == LOW) {
            WDT_RESET();
            // Защита от залипания - 10 секунд
            if (millis() - pressStart > 10000) break;
            delay(10);
          }
          break; // Возврат в список
        }
        // Защита: если прошло очень много времени (10 минут), выходим автоматом
        if (millis() - waitStart > 10UL * 60UL * 1000UL) break;
        delay(50);
      }
      lastIndex = -1; // обновить список после возврата
    }
    prevButtonHigh = !currButtonLow;

    delay(50);
  }
}










