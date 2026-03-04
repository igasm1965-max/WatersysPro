/**
 * @file preferences.cpp
 * @brief Реализация управления настройками системы
 * @details Функции сохранения и загрузки параметров в Preferences (NVRAM)
 */

#include "prefs.h"
#include "config.h"
#include "structures.h"

// ============ ИНИЦИАЛИЗАЦИЯ ХРАНИЛИЩА ============

/// Инициализирует Preferences хранилище
void initializePreferences() {
  extern Preferences preferences;
  
  if (!preferences.begin("water_sys")) {
    Serial.println("[ERROR] Preferences initialization failed");
    return;
  }

  // Ensure admin token exists in Preferences. Do NOT overwrite an existing token.
  // If no token stored, set repository default; otherwise preserve stored value.
  {
    String stored = preferences.getString(PREF_KEY_ADMIN_TOKEN, "");
    if (stored.length() == 0) {
      preferences.putString(PREF_KEY_ADMIN_TOKEN, DEFAULT_ADMIN_TOKEN);
      Serial.println("[INFO] Admin token set to default (was empty)");
    } else {
      Serial.println("[INFO] Admin token preserved");
    }
  }

  // Pre-create a few common string keys so that early "getString"
  // calls don't trigger the Preferences library error messages when
  // the key is missing (first boot / cleared NVS).
  // We intentionally write empty strings, which still count as valid
  // entries and will suppress the NOT_FOUND log from the underlying
  // nvs_get_str implementation.
  if (!preferences.isKey(PREF_KEY_WIFI_SSID)) {
    preferences.putString(PREF_KEY_WIFI_SSID, "");
  }
  if (!preferences.isKey(PREF_KEY_WIFI_PASS)) {
    preferences.putString(PREF_KEY_WIFI_PASS, "");
  }
  if (!preferences.isKey(PREF_KEY_MQTT_TOPIC_BASE)) {
    preferences.putString(PREF_KEY_MQTT_TOPIC_BASE, DEFAULT_MQTT_TOPIC_BASE);
  }
}


// ============ ЗАГРУЗКА ВСЕХ НАСТРОЕК ============

/// Загружает все настройки из хранилища
void loadAllSettings() {
  extern Preferences preferences;
  extern uint32_t setlingDuration;
  extern uint32_t aerationDuration;
  extern uint32_t ozonationDuration;
  extern uint32_t backlightDuration;
  extern uint32_t filterWashDuration;
  extern uint32_t filterCleaningInterval;
  extern unsigned long filterOperationStartTime;
  extern uint32_t lastBackwashTimestamp;
  extern byte scheduleStart[3];
  extern byte scheduleEnd[3];
  extern byte nightModeStart[2];
  extern byte nightModeEnd[2];
  extern struct TankParams tank1, tank2;
  
  // Таймеры (в секундах)
  setlingDuration = preferences.getULong("settling_dur", 10UL);
  aerationDuration = preferences.getULong("aeration_dur", 10UL);
  ozonationDuration = preferences.getULong("ozone_dur", 10UL);
  backlightDuration = preferences.getULong("backlight_dur", 5 * 60UL);
  extern unsigned long backlightTimeout;
  backlightTimeout = backlightDuration * 1000UL; // sync runtime timeout (s -> ms)
  filterWashDuration = preferences.getULong("filter_wash", 10UL);

  // migration from old key: previous implementation saved only the number of
  // days and used "filter_period".  If we detect that key we copy its value
  // (which is stored already in seconds) into the current key and remove the
  // obsolete entry.  This ensures that existing controllers don't drop their
  // configured interval when they reboot.
  const uint32_t defaultCleaning = 7 * 24UL * 3600UL;
  if (preferences.isKey("filter_clean")) {
    filterCleaningInterval = preferences.getULong("filter_clean", defaultCleaning);
  } else if (preferences.isKey("filter_period")) {
    uint32_t migrated = preferences.getULong("filter_period", defaultCleaning);
    filterCleaningInterval = migrated;
    preferences.putULong("filter_clean", migrated);
    preferences.remove("filter_period");
    Serial.println("[INFO] Migrated filter_period -> filter_clean");
  } else {
    filterCleaningInterval = defaultCleaning;
  }
  
  // Загружаем timestamp последней промывки (восстановление в restoreBackwashTimer после initRTC)
  lastBackwashTimestamp = preferences.getULong("last_backwash", 0);
  
  // Временно инициализируем filterOperationStartTime (будет пересчитано в restoreBackwashTimer)
  filterOperationStartTime = millis();
  
  // Расписание
  scheduleStart[0] = preferences.getUChar("sch_start_h", 8);
  scheduleStart[1] = preferences.getUChar("sch_start_m", 0);
  scheduleStart[2] = preferences.getUChar("sch_start_s", 0);
  
  scheduleEnd[0] = preferences.getUChar("sch_end_h", 22);
  scheduleEnd[1] = preferences.getUChar("sch_end_m", 0);
  scheduleEnd[2] = preferences.getUChar("sch_end_s", 0);
  
  // Ночной режим
  nightModeStart[0] = preferences.getUChar("night_start_h", 22);
  nightModeStart[1] = preferences.getUChar("night_start_m", 0);
  
  nightModeEnd[0] = preferences.getUChar("night_end_h", 6);
  nightModeEnd[1] = preferences.getUChar("night_end_m", 0);
  
  // Параметры баков
  tank1.minLevel = preferences.getUChar("tank1_min", 10);
  tank1.maxLevel = preferences.getUChar("tank1_max", 80);
  
  tank2.minLevel = preferences.getUChar("tank2_min", 10);
  tank2.maxLevel = preferences.getUChar("tank2_max", 80);
  

}

// ============ СОХРАНЕНИЕ НАСТРОЕК ============

/// Восстанавливает таймер промывки фильтра после инициализации RTC
void restoreBackwashTimer() {
  extern RTC_DS3231 rtc;
  extern SystemFlags flags;
  extern uint32_t lastBackwashTimestamp;
  extern unsigned long filterOperationStartTime;
  
  if (lastBackwashTimestamp > 0 && flags.rtcInitialized) {
    // Вычисляем сколько секунд прошло с последней промывки
    DateTime now = rtc.now();
    uint32_t currentTimestamp = now.unixtime();
    
    if (currentTimestamp > lastBackwashTimestamp) {
      uint32_t elapsedSeconds = currentTimestamp - lastBackwashTimestamp;
      // Преобразуем в millis() эквивалент: filterOperationStartTime = millis() - elapsed
      // Это создаст иллюзию, что система работала непрерывно
      unsigned long elapsedMs = elapsedSeconds * 1000UL;
      unsigned long currentMillis = millis();
      
      if (elapsedMs < currentMillis) {
        filterOperationStartTime = currentMillis - elapsedMs;
      } else {
        // Прошло больше времени чем millis() может отразить, ставим 1
        filterOperationStartTime = 1;
      }
      
      unsigned long days = elapsedSeconds / 86400UL;
      unsigned long hours = (elapsedSeconds % 86400UL) / 3600UL;

    } else {
      Serial.println("[INFO] Filter timer: RTC time < saved timestamp, resetting");
      filterOperationStartTime = millis();
    }
  } else {
    Serial.println("[INFO] Filter timer: no saved timestamp, starting fresh");
  }
}

/// Сохраняет timestamp последней промывки фильтра
void saveBackwashTimestamp() {
  extern Preferences preferences;
  extern RTC_DS3231 rtc;
  extern SystemFlags flags;
  extern uint32_t lastBackwashTimestamp;
  extern unsigned long filterOperationStartTime;
  
  if (flags.rtcInitialized) {
    DateTime now = rtc.now();
    lastBackwashTimestamp = now.unixtime();
    preferences.putULong("last_backwash", lastBackwashTimestamp);
    filterOperationStartTime = millis();  // Сброс таймера
    Serial.printf("[INFO] Backwash timestamp saved: %lu\n", lastBackwashTimestamp);
  }
}

/// Сохраняет таймеры
void saveTimerSettings() {
  extern Preferences preferences;
  extern uint32_t setlingDuration, aerationDuration, ozonationDuration;
  extern uint32_t backlightDuration, filterWashDuration, filterCleaningInterval;
  
  preferences.putULong("settling_dur", setlingDuration);
  preferences.putULong("aeration_dur", aerationDuration);
  preferences.putULong("ozone_dur", ozonationDuration);
  preferences.putULong("backlight_dur", backlightDuration);
  extern unsigned long backlightTimeout;
  backlightTimeout = backlightDuration * 1000UL; // apply runtime timeout
  preferences.putULong("filter_wash", filterWashDuration);
  preferences.putULong("filter_clean", filterCleaningInterval);
}

/// Сохраняет параметры расписания
void saveScheduleSettings() {
  extern Preferences preferences;
  extern byte scheduleStart[3], scheduleEnd[3];
  extern byte nightModeStart[2], nightModeEnd[2];
  
  preferences.putUChar("sch_start_h", scheduleStart[0]);
  preferences.putUChar("sch_start_m", scheduleStart[1]);
  preferences.putUChar("sch_start_s", scheduleStart[2]);
  
  preferences.putUChar("sch_end_h", scheduleEnd[0]);
  preferences.putUChar("sch_end_m", scheduleEnd[1]);
  preferences.putUChar("sch_end_s", scheduleEnd[2]);
  
  preferences.putUChar("night_start_h", nightModeStart[0]);
  preferences.putUChar("night_start_m", nightModeStart[1]);
  
  preferences.putUChar("night_end_h", nightModeEnd[0]);
  preferences.putUChar("night_end_m", nightModeEnd[1]);
}

// ============ СОХРАНЕНИЕ СОСТОЯНИЯ РАБОТЫ ============

void saveRuntimeState() {
  extern Preferences preferences;
  // Use `systemContext` for FSM globals in this module
  extern uint32_t currentOzonationRemaining;
  extern uint32_t currentAerationRemaining;
  extern uint32_t currentSetlingRemaining;
  extern uint32_t currentBackwashRemaining;
  extern SystemFlags flags;
  extern RTC_DS3231 rtc;

  DateTime now = rtc.now();

  preferences.putUChar("rt_valid", 1);
  preferences.putUChar("rt_state", (uint8_t)systemContext.currentState);
  preferences.putULong("rt_oz", currentOzonationRemaining);
  preferences.putULong("rt_aer", currentAerationRemaining);
  preferences.putULong("rt_set", currentSetlingRemaining);
  preferences.putULong("rt_bw", currentBackwashRemaining);
  preferences.putUChar("rt_wt", flags.waterTreatmentInProgress ? 1 : 0);
  preferences.putUChar("rt_bwip", flags.backwashInProgress ? 1 : 0);
  preferences.putULong("rt_ts", now.unixtime());
}

bool loadRuntimeState() {
  extern Preferences preferences;
  // Use `systemContext` for FSM globals in this module
  extern uint32_t currentOzonationRemaining;
  extern uint32_t currentAerationRemaining;
  extern uint32_t currentSetlingRemaining;
  extern uint32_t currentBackwashRemaining;
  extern SystemFlags flags;
  extern unsigned long filterOperationStartTime;

  if (preferences.getUChar("rt_valid", 0) == 0) {
    return false;
  }

  SystemState savedState = (SystemState)preferences.getUChar("rt_state", STATE_IDLE);
  uint32_t oz = preferences.getULong("rt_oz", 0);
  uint32_t aer = preferences.getULong("rt_aer", 0);
  uint32_t setl = preferences.getULong("rt_set", 0);
  uint32_t bw = preferences.getULong("rt_bw", 0);
  bool wt = preferences.getUChar("rt_wt", 0) != 0;
  bool bwip = preferences.getUChar("rt_bwip", 0) != 0;

  if (savedState == STATE_IDLE) {
    return false;
  }

  systemContext.currentState = savedState;
  currentOzonationRemaining = oz;
  currentAerationRemaining = aer;
  currentSetlingRemaining = setl;
  currentBackwashRemaining = bw;
  flags.waterTreatmentInProgress = wt ? 1 : 0;
  flags.backwashInProgress = bwip ? 1 : 0;

  systemContext.stateStartTime = millis();
  filterOperationStartTime = millis();

  return true;
}

/// Сохраняет параметры баков
void saveTankSettings() {
  extern Preferences preferences;
  extern struct TankParams tank1, tank2;
  
  preferences.putUChar("tank1_min", tank1.minLevel);
  preferences.putUChar("tank1_max", tank1.maxLevel);
  
  preferences.putUChar("tank2_min", tank2.minLevel);
  preferences.putUChar("tank2_max", tank2.maxLevel);
}

/// Сохраняет одиночное значение таймера
void saveSingleSetting(int settingCode, uint32_t value) {
  extern Preferences preferences;
  
  switch(settingCode) {
    case SETTING_TIMER_SETLING:
      preferences.putULong("settling_dur", value);
      break;
    case SETTING_TIMER_AERATION:
      preferences.putULong("aeration_dur", value);
      break;
    case SETTING_TIMER_OZONATION:
      preferences.putULong("ozone_dur", value);
      break;
    case SETTING_TIMER_FILTER_WASH:
      preferences.putULong("filter_wash", value);
      break;
    case SETTING_TIMER_BACKLIGHT:
      preferences.putULong("backlight_dur", value);
      extern uint32_t backlightDuration;
      extern unsigned long backlightTimeout;
      backlightDuration = value;
      backlightTimeout = backlightDuration * 1000UL;
      break;
    case SETTING_FILTER_PERIOD:
      preferences.putULong("filter_clean", value);
      break;
    case SETTING_TANK1_MIN:
      preferences.putUChar("tank1_min", value);
      break;
    case SETTING_TANK1_MAX:
      preferences.putUChar("tank1_max", value);
      break;
    case SETTING_TANK2_MIN:
      preferences.putUChar("tank2_min", value);
      break;
    case SETTING_TANK2_MAX:
      preferences.putUChar("tank2_max", value);
      break;
  }
}

// ============ СБРОС НАСТРОЕК ============

/// Сбрасывает все настройки на значения по умолчанию
void resetAllSettings() {
  extern Preferences preferences;
  
  preferences.clear();
  loadAllSettings();
  saveTimerSettings();
  saveScheduleSettings();
  saveTankSettings();
  
  Serial.println("[INFO] All settings reset to defaults");
}

/// Сбрасывает конкретную настройку
void resetSetting(int settingCode) {
  extern Preferences preferences;
  
  switch(settingCode) {
    case SETTING_TIMER_SETLING:
      preferences.putULong("settling_dur", 10UL);
      break;
    case SETTING_TIMER_AERATION:
      preferences.putULong("aeration_dur", 10UL);
      break;
    case SETTING_TIMER_OZONATION:
      preferences.putULong("ozone_dur", 10UL);
      break;
    case SETTING_TIMER_FILTER_WASH:
      preferences.putULong("filter_wash", 10UL);
      break;
    case SETTING_TIMER_BACKLIGHT:
      preferences.putULong("backlight_dur", 10 * 60UL);
      extern uint32_t backlightDuration;
      extern unsigned long backlightTimeout;
      backlightDuration = 10 * 60UL;
      backlightTimeout = backlightDuration * 1000UL;
      break;
    case SETTING_FILTER_PERIOD:
      preferences.putULong("filter_clean", 7 * 86400UL); // 7 дней
      break;
    case SETTING_TANK1_MIN:
      preferences.putUChar("tank1_min", 10);
      break;
    case SETTING_TANK1_MAX:
      preferences.putUChar("tank1_max", 80);
      break;
    case SETTING_TANK2_MIN:
      preferences.putUChar("tank2_min", 10);
      break;
    case SETTING_TANK2_MAX:
      preferences.putUChar("tank2_max", 80);
      break;
  }
  
  Serial.print("[PREF] Setting ");
  Serial.print(settingCode);
  Serial.println(" reset to default");
}

// ============ ПОЛУЧЕНИЕ НАСТРОЕК ============

/// Утилиты

/// Проверяет, инициализирован ли объект Preferences и можно ли к нему обращаться
bool prefsIsAvailable() {
  extern Preferences preferences;
  (void)preferences; // избегаем предупреждения
  return true;
}


/// Получает значение таймера
uint32_t getTimerValue(int timerCode) {
  extern Preferences preferences;
  
  switch(timerCode) {
    case SETTING_TIMER_SETLING: 
      return preferences.getULong("settling_dur", 5 * 3600UL);
    case SETTING_TIMER_AERATION:
      return preferences.getULong("aeration_dur", 1 * 3600UL + 22 * 60UL);
    case SETTING_TIMER_OZONATION:
      return preferences.getULong("ozone_dur", 5 * 60UL);
    case SETTING_TIMER_FILTER_WASH:
      return preferences.getULong("filter_wash", 5 * 60UL);
    default:
      return 0;
  }
}

/// Получает уровень бака
int getTankLevel(int tankNumber) {
  extern Preferences preferences;
  
  if (tankNumber == 1) {
    return preferences.getUChar("tank1_max", 80);
  } else {
    return preferences.getUChar("tank2_max", 80);
  }
}

// ============ ВАЛИДАЦИЯ НАСТРОЕК ============

/// Проверяет валидность значения настройки
bool isValidSettingValue(int settingCode, uint32_t value) {
  // Базовые проверки диапазонов
  switch(settingCode) {
    case SETTING_TANK1_MIN:
    case SETTING_TANK1_MAX:
    case SETTING_TANK2_MIN:
    case SETTING_TANK2_MAX:
      return value >= 0 && value <= 200; // см
    default:
      return value > 0;
  }
}

/// Проверяет целостность хранилища
bool validatePreferencesIntegrity() {
  extern Preferences preferences;

  // Таймеры (секунды)
  uint32_t settling = preferences.getULong("settling_dur", 5 * 3600UL);
  uint32_t aeration = preferences.getULong("aeration_dur", 1 * 3600UL + 22 * 60UL);
  uint32_t ozone = preferences.getULong("ozone_dur", 5 * 60UL);
  uint32_t filterWash = preferences.getULong("filter_wash", 5 * 60UL);
  uint32_t backlight = preferences.getULong("backlight_dur", 10 * 60UL);

  if (!isValidSettingValue(SETTING_TIMER_SETLING, settling)) return false;
  if (!isValidSettingValue(SETTING_TIMER_AERATION, aeration)) return false;
  if (!isValidSettingValue(SETTING_TIMER_OZONATION, ozone)) return false;
  if (!isValidSettingValue(SETTING_TIMER_FILTER_WASH, filterWash)) return false;
  if (!isValidSettingValue(SETTING_TIMER_BACKLIGHT, backlight)) return false;

  // Параметры баков
  uint8_t t1min = preferences.getUChar("tank1_min", 10);
  uint8_t t1max = preferences.getUChar("tank1_max", 80);
  uint8_t t2min = preferences.getUChar("tank2_min", 10);
  uint8_t t2max = preferences.getUChar("tank2_max", 80);

  if (!isValidSettingValue(SETTING_TANK1_MIN, t1min)) return false;
  if (!isValidSettingValue(SETTING_TANK1_MAX, t1max)) return false;
  if (!isValidSettingValue(SETTING_TANK2_MIN, t2min)) return false;
  if (!isValidSettingValue(SETTING_TANK2_MAX, t2max)) return false;
  if (t1min > t1max || t2min > t2max) return false;

  // Расписание и ночной режим
  uint8_t schStartH = preferences.getUChar("sch_start_h", 8);
  uint8_t schStartM = preferences.getUChar("sch_start_m", 0);
  uint8_t schEndH = preferences.getUChar("sch_end_h", 22);
  uint8_t schEndM = preferences.getUChar("sch_end_m", 0);
  uint8_t nightStartH = preferences.getUChar("night_start_h", 22);
  uint8_t nightStartM = preferences.getUChar("night_start_m", 0);
  uint8_t nightEndH = preferences.getUChar("night_end_h", 6);
  uint8_t nightEndM = preferences.getUChar("night_end_m", 0);

  if (schStartH > 23 || schEndH > 23 || nightStartH > 23 || nightEndH > 23) return false;
  if (schStartM > 59 || schEndM > 59 || nightStartM > 59 || nightEndM > 59) return false;

  return true;
}

/// Восстанавливает потерянные значения по умолчанию
void repairPreferences() {
  if (!validatePreferencesIntegrity()) {
    resetAllSettings();
  }
}

// ============ СОХРАНЕНИЕ ОТДЕЛЬНЫХ НАСТРОЕК ============

/// Сохранение настройки таймера
void saveTimerSetting(uint8_t timerType, uint8_t hours, uint8_t minutes, uint8_t seconds) {
  extern Preferences preferences;
  uint32_t duration = hours * 3600UL + minutes * 60UL + seconds;
  
  switch (timerType) {
    case 0: preferences.putULong("settling_dur", duration); break;
    case 1: preferences.putULong("aeration_dur", duration); break;
    case 2: preferences.putULong("ozone_dur", duration); break;
    case 3: preferences.putULong("filter_wash", duration); break;
    case 4: preferences.putULong("backlight_dur", duration); break;
  }
  
  Serial.print("[PREF] Timer ");
  Serial.print(timerType);
  Serial.print(" saved: ");
  Serial.print(hours);
  Serial.print("h ");
  Serial.print(minutes);
  Serial.print("m ");
  Serial.print(seconds);
  Serial.println("s");
}

/// Сохранение настроек бака
void saveTankSettings(uint8_t tankNumber, int minLevel, int maxLevel) {
  extern Preferences preferences;
  
  if (tankNumber == 1) {
    preferences.putUChar("tank1_min", (uint8_t)minLevel);
    preferences.putUChar("tank1_max", (uint8_t)maxLevel);
  } else if (tankNumber == 2) {
    preferences.putUChar("tank2_min", (uint8_t)minLevel);
    preferences.putUChar("tank2_max", (uint8_t)maxLevel);
  }
  
  Serial.print("[PREF] Tank ");
  Serial.print(tankNumber);
  Serial.print(" saved: min=");
  Serial.print(minLevel);
  Serial.print(" max=");
  Serial.println(maxLevel);
}

/// Сохранение периода очистки фильтра
///
/// The argument is the full interval in seconds.  Prior versions only wrote
/// the number of days to the wrong key ("filter_period"), so the setting
/// would be lost on reboot.  We now store the value under the same key used
/// for loading ("filter_clean").
void saveFilterCleaningPeriod(uint32_t intervalSeconds) {
  extern Preferences preferences;

  preferences.putULong("filter_clean", intervalSeconds);

  Serial.print("[PREF] Filter cleaning period saved: ");
  Serial.print(intervalSeconds);
  Serial.println(" sec");
}
