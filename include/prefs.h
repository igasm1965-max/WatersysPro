/**
 * @file prefs.h
 * @brief Интерфейс управления настройками (Preferences) - renamed to avoid header name collision with native library
 */

#ifndef PREFS_H
#define PREFS_H

#include <Preferences.h>
#include "config.h"
#include "structures.h"

// ============ УПРАВЛЕНИЕ НАСТРОЙКАМИ ============

/// Инициализация системы Preferences
void initializePreferences();

/// Загрузка всех настроек
void loadAllSettings();

/// Загрузка настроек из хранилища (старый метод)
void loadSettings();

/// Сохранение всех настроек
void saveAllSettings();

/// Сохранение настроек в хранилище (старый метод)
void saveSettings();

/// Запись настройки по умолчанию
void writeDefaultSettings();

/// Безопасная запись одной настройки
void safeWritePreference(const char* key, uint32_t value);

// ============ ИНИЦИАЛИЗАЦИЯ ПАРАМЕТРОВ ============

/// Загрузка таймеров из Preferences
void initTimersFromPreferences();

/// Загрузка параметров баков
void initTankParameters();

/// Загрузка расписания и ночного режима
void initScheduleAndNightMode();

/// Проверка корректности настроек
bool validateSettings();

/// Проверка валидности настроек в Preferences
bool validatePreferenceSettings();

/// Проверяет, инициализированы ли Preferences и можно ли безопасно к ним обращаться
bool prefsIsAvailable();

// ============ СОХРАНЕНИЕ ОТДЕЛЬНЫХ НАСТРОЕК ============

/// Сохранение настройки таймера
void saveTimerSetting(uint8_t timerType, uint8_t hours, uint8_t minutes, uint8_t seconds);

/// Сохранение настроек бака
void saveTankSettings(uint8_t tankNumber, int minLevel, int maxLevel);

/// Сохранение настроек расписания
void saveScheduleSettings();

/// Сохранение периода очистки фильтра
///
/// Historically this function only stored the *days* value using the wrong
/// preference key ("filter_period").  As a result the full interval
/// (days+h:m) was never persisted and settings reverted to the default on
/// reboot.  The argument is now the complete interval in _seconds_ and the
/// correct key ("filter_clean") is used.  Callers should pass
/// `filterCleaningInterval` rather than just the day component.
void saveFilterCleaningPeriod(uint32_t intervalSeconds);

/// Сохранение timestamp последней промывки фильтра
void saveBackwashTimestamp();

/// Восстановление таймера промывки после инициализации RTC
void restoreBackwashTimer();

/// Сброс всех настроек к умолчаниям
void resetAllSettings();

// ============ СОХРАНИЕ СОСТОЯНИЯ РАБОТЫ ============

/// Сохранение состояния системы для восстановления после перезагрузки
void saveRuntimeState();

/// Загрузка сохраненного состояния системы
bool loadRuntimeState();

// ============ СТАТИСТИКА WATCHDOG ============

/// Сохранение статистики WDT в Preferences
void saveWDTStats();

/// Загрузка статистики WDT из Preferences
void loadWDTStats();

#endif // PREFS_H