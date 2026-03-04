/**
 * @file menu_data.cpp
 * @brief Данные меню - структура и пункты
 * @details Определяет иерархию меню и обработчики
 */

#include "menu.h"
#include "engineer_menu.h"

// ============ ОБРАБОТЧИК ИНЖЕНЕРНОГО МЕНЮ ============

/// Обработчик входа в инженерное меню (проверка пароля)
void EngineerMenuHandler() {
  // Просто показываем экран пароля - если пройдёт, меню откроется
  showPasswordScreen();
}

// ============ МАССИВ ПУНКТОВ МЕНЮ ============
// Enum MenuItem определён в structures.h

sMenuItem mainMenu[] = {
  // Главное меню (mkRoot)
  { mksettingUpTimers, mkRoot, "Setting up timers", NULL },
  { mkCleaningTheFilter, mkRoot, "Cleaning the filter", NULL },
  { mkSettingTankLevels, mkRoot, "Setting tank level", NULL },
  { mkManualOperation, mkRoot, "Manual operation", ManualOperation },
  { mkDateTimeSettings, mkRoot, "Date & Time", NULL },
  { mkSystemLogging, mkRoot, "System Logging", NULL },
  { mkScheduleSettings, mkRoot, "Schedule Settings", NULL },
  { mkNightModeSettings, mkRoot, "Night Mode", NULL },
  { mkResetSettings, mkRoot, "Reset Settings", resetToDefaults },
  { mkEngineerMenu, mkRoot, "Engineer Menu", EngineerMenuHandler },
  { mkEXIT, mkRoot, "EXIT", NULL },

  // Настройка таймеров
  { mkSetlingWaterTimer, mksettingUpTimers, "Setling Water timer", CommonTimerHandler },
  { mkAerationTimer, mksettingUpTimers, "Aeration timer", CommonTimerHandler },
  { mkOzonationTimer, mksettingUpTimers, "Ozonation timer", CommonTimerHandler },
  { mkBacklightTimer, mksettingUpTimers, "Backlight timer", CommonTimerHandler },
  { mkBack, mksettingUpTimers, "Back", NULL },

  // Очистка фильтра
  { mkFilterCleaningTimer, mkCleaningTheFilter, "Filter wash timer", CommonTimerHandler },
  { mkPeriodCleaningTheFilter, mkCleaningTheFilter, "Wash period", FilterCleaningPeriodHandler },
  { mkBack, mkCleaningTheFilter, "Back", NULL },

  // Уровни баков
  { mkTank1, mkSettingTankLevels, "Tank 1", TankSettingsHandler },
  { mkTank2, mkSettingTankLevels, "Tank 2", TankSettingsHandler },
  { mkBack, mkSettingTankLevels, "Back", NULL },

  // Дата и время
  { mkSetCurrentTime, mkDateTimeSettings, "Set Current Time", DateTimeSettingsHandler },
  { mkSetCurrentDate, mkDateTimeSettings, "Set Current Date", DateTimeSettingsHandler },
  { mkViewDateTime, mkDateTimeSettings, "View Date/Time", DateTimeSettingsHandler },
  { mkBack, mkDateTimeSettings, "Back", NULL },

  // Логирование системы
  { mkEventLogs, mkSystemLogging, "Event Logs", EventLogsHandler },
  { mkClearEventLogs, mkSystemLogging, "Clear Event Logs", clearEventLogsMenu },
  { mkPruneEventLogs, mkSystemLogging, "Prune Event Logs", pruneEventLogsMenu },
  { mkFormatSdCard, mkSystemLogging, "Format SD Card", formatSdMenu },
  { mkBack, mkSystemLogging, "Back", NULL },

  // Подменю логов
  { mkViewEventLogs, mkEventLogs, "View Event Logs", EventLogsHandler },
  { mkEventStats, mkEventLogs, "Event Statistics", EventStatsHandler },
  { mkEventGraph, mkEventLogs, "Event Graph", EventGraphHandler },
  { mkFilterByType, mkEventLogs, "Filter by Type", EventFilterHandler },
  { mkClearOldEvents, mkEventLogs, "Clear Old Events", clearOldEventsMenu },
  { mkBack, mkEventLogs, "Back", NULL },

  // Расписание
  { mkSetScheduleStart, mkScheduleSettings, "Set Start Time", ScheduleSettingsHandler },
  { mkSetScheduleEnd, mkScheduleSettings, "Set End Time", ScheduleSettingsHandler },
  { mkToggleSchedule, mkScheduleSettings, "Enable/Disable", ScheduleSettingsHandler },
  { mkBack, mkScheduleSettings, "Back", NULL },

  // Ночной режим
  { mkSetNightModeStart, mkNightModeSettings, "Set Start Time", NightModeSettingsHandler },
  { mkSetNightModeEnd, mkNightModeSettings, "Set End Time", NightModeSettingsHandler },
  { mkToggleNightMode, mkNightModeSettings, "Enable/Disable", NightModeSettingsHandler },
  { mkBack, mkNightModeSettings, "Back", NULL },
  
  // ============ ИНЖЕНЕРНОЕ МЕНЮ (требует пароль) ============
  { mkSafetyTimeouts, mkEngineerMenu, "Safety Timeouts", NULL },
  { mkWatchdogSettings, mkEngineerMenu, "Watchdog Settings", NULL },
  { mkChangePassword, mkEngineerMenu, "Change Password", showChangePasswordScreen },
  { mkSystemInfo, mkEngineerMenu, "System Info", showSystemInfo },
  { mkNetworkInfo, mkEngineerMenu, "Network Info", showNetworkInfo },
  { mkEventLogSettings, mkEngineerMenu, "Log Filter", showEventLogSettings },
  { mkWiFiSetup, mkEngineerMenu, "Wi-Fi Setup", showWiFiSetup },
  { mkMqttSetup, mkEngineerMenu, "MQTT Setup", showMqttSetup },
  
  { mkFactoryReset, mkEngineerMenu, "Factory Reset (!)", performFactoryReset },
  { mkBack, mkEngineerMenu, "Back", NULL },
  
  // Подменю таймаутов безопасности (каждый со своим обработчиком)
  { mkTimeoutFilling, mkSafetyTimeouts, "Filling Timeout", editTimeoutFilling },
  { mkTimeoutOzonation, mkSafetyTimeouts, "Ozonation Timeout", editTimeoutOzonation },
  { mkTimeoutAeration, mkSafetyTimeouts, "Aeration Timeout", editTimeoutAeration },
  { mkTimeoutSettling, mkSafetyTimeouts, "Settling Timeout", editTimeoutSettling },
  { mkTimeoutFiltration, mkSafetyTimeouts, "Filtration Timeout", editTimeoutFiltration },
  { mkTimeoutBackwash, mkSafetyTimeouts, "Backwash Timeout", editTimeoutBackwash },
  { mkPumpDryTimeout, mkSafetyTimeouts, "Pump Dry Timeout", editPumpDryTimeout },
  { mkPumpMinLevelDelta, mkSafetyTimeouts, "Pump Min Delta", editPumpMinLevelDelta },
  { mkPumpDryConsecutiveChecks, mkSafetyTimeouts, "Pump Dry Consec.", editPumpDryConsecutiveChecks },
  { mkBack, mkSafetyTimeouts, "Back", NULL },
  
  // Подменю Watchdog
  { mkToggleWatchdog, mkWatchdogSettings, "Enable/Disable", toggleWatchdog },
  { mkWatchdogTimeout, mkWatchdogSettings, "Timeout (sec)", editWatchdogTimeout },
  { mkBack, mkWatchdogSettings, "Back", NULL }
};

// Количество элементов меню
const int mainMenuLength = sizeof(mainMenu) / sizeof(mainMenu[0]);
