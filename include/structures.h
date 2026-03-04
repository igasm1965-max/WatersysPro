/**
 * @file structures.h
 * @brief Структуры данных для системы управления водоочисткой
 */

#ifndef STRUCTURES_H
#define STRUCTURES_H

#include "config.h"

// ============ ПЕРЕЧИСЛЕНИЯ СОСТОЯНИЙ ============

/// Состояния системы (автомат состояний)
enum SystemState {
  STATE_IDLE,              ///< Ожидание команды
  STATE_FILLING_TANK1,     ///< Заполнение бака 1
  STATE_OZONATION,         ///< Озонация воды
  STATE_AERATION,          ///< Аэрация воды
  STATE_SETTLING,          ///< Отстаивание воды
  STATE_FILTRATION,        ///< Фильтрация воды
  STATE_BACKWASH           ///< Обратная промывка фильтра
};

/// Уровни логирования
enum LogLevel {
  LOG_DEBUG,    ///< Отладочная информация
  LOG_INFO,     ///< Информационные сообщения
  LOG_WARNING,  ///< Предупреждения
  LOG_ERROR     ///< Ошибки
};

/// Пункты меню
enum MenuItem {
  mkBack = 0,
  mkRoot = 1,
  mksettingUpTimers = 2,
  mkAerationTimer = 3,
  mkOzonationTimer = 4,
  mkSetlingWaterTimer = 5,
  mkBacklightTimer = 6,
  mkCleaningTheFilter = 7,
  mkFilterCleaningTimer = 8,
  mkPeriodCleaningTheFilter = 9,
  mkSettingTankLevels = 10,
  mkTank1 = 11,
  mkTank2 = 12,
  mkManualOperation = 13,
  mkDateTimeSettings = 14,
  mkSetCurrentTime = 15,
  mkSetCurrentDate = 16,
  mkViewDateTime = 17,
  mkSystemLogging = 18,
  mkEventLogs = 19,
  mkClearEventLogs = 20,
  mkScheduleSettings = 21,
  mkSetScheduleStart = 22,
  mkSetScheduleEnd = 23,
  mkToggleSchedule = 24,
  mkNightModeSettings = 25,
  mkSetNightModeStart = 26,
  mkSetNightModeEnd = 27,
  mkToggleNightMode = 28,
  mkResetSettings = 29,
  mkEXIT = 30,
  mkViewEventLogs = 200,
  mkEventStats = 201,
  mkEventGraph = 202,
  mkFilterByType = 203,
  mkClearOldEvents = 204,
  mkPruneEventLogs = 205,
  mkFormatSdCard = 206,
  
  // Инженерное меню (требует пароль)
  mkEngineerMenu = 300,
  mkEngineerPassword = 301,
  mkSafetyTimeouts = 302,
  mkTimeoutFilling = 303,
  mkTimeoutOzonation = 304,
  mkTimeoutAeration = 305,
  mkTimeoutSettling = 306,
  mkTimeoutFiltration = 307,
  mkTimeoutBackwash = 308,
  mkPumpDryTimeout = 320,
  mkPumpMinLevelDelta = 321,
  mkPumpDryConsecutiveChecks = 322,
  mkWatchdogSettings = 309,
  mkToggleWatchdog = 310,
  mkWatchdogTimeout = 311,
  mkChangePassword = 312,
  mkSystemInfo = 313,
  mkFactoryReset = 314,
  // Network & MQTT engineer items
  mkNetworkInfo = 330,
  mkWiFiSetup = 331,
  mkMqttSetup = 332,
  mkSetAdminToken = 333,

  // engineer menu: log filter configuration
  mkEventLogSettings = 334,
};

// ============ СТРУКТУРЫ ДАННЫХ ============

/// Структура для пункта меню
struct MenuItemStruct {
  int id;              ///< ID пункта меню
  int parentId;        ///< ID родительского пункта
  const char* label;   ///< Текст пункта
  void (*handler)();   ///< Указатель на функцию-обработчик
};

/// Структура меню для простой реализации
struct Menu {
  const char* caption;   ///< Название пункта
  void (*handler)();     ///< Обработчик пункта
};

/// Флаги состояния системы (битовые поля для экономии памяти)
struct SystemFlags {
  // Байт 0: флаги фильтра и воды
  uint8_t filterCleaningNeeded : 1;       ///< Требуется чистка фильтра
  uint8_t waterTreatmentInProgress : 1;   ///< Обработка воды в процессе
  uint8_t firstFiltrationCycle : 1;       ///< Первый цикл фильтрации
  uint8_t backwashInProgress : 1;         ///< Промывка в процессе
  uint8_t inMenu : 1;                     ///< В режиме меню
  uint8_t displayInitialized : 1;         ///< Дисплей инициализирован
  uint8_t timersActive : 1;               ///< Таймеры активны
  uint8_t inFilterWashInfo : 1;           ///< В режиме инфо по фильтру
  
  // Байт 1: флаги дисплея и режимов
  uint8_t filterWashDisplayInitialized : 1;  ///< Экран промывки инициализирован
  uint8_t backwashScreenInitialized : 1;     ///< Экран обратной промывки инициализирован
  uint8_t backlightOn : 1;                   ///< Подсветка включена
  uint8_t displayOn : 1;                     ///< Дисплей включен
  uint8_t emergencyMode : 1;                 ///< Аварийный режим
  uint8_t emergencyScreenInitialized : 1;    ///< Экран аварии инициализирован
  uint8_t emergencyBlinkState : 1;           ///< Состояние мигания в аварии
  uint8_t tank1Empty : 1;                    ///< Бак 1 пуст
  
  // Байт 2: флаги состояния
  uint8_t tank2Empty : 1;                 ///< Бак 2 пуст
  uint8_t watchdogEnabled : 1;            ///< Watchdog включен
  uint8_t systemHangDetected : 1;         ///< Обнаружено зависание
  uint8_t blinkState : 1;                 ///< Состояние мигания
  uint8_t setupComplete : 1;              ///< Инициализация завершена
  uint8_t rtcInitialized : 1;             ///< RTC инициализирован
  uint8_t nightModeEnabled : 1;           ///< Ночной режим включен
  uint8_t scheduleEnabled : 1;            ///< Расписание включено
  
  // Байт 3: дополнительные флаги
  uint8_t displayLocked : 1;          ///< Дисплей заблокирован (не обновляется в фоне)
  uint8_t reserved_flags2 : 1;
  uint8_t reserved_flags3 : 1;
  uint8_t reserved_flags4 : 1;
  uint8_t reserved_flags5 : 1;
  uint8_t reserved_flags6 : 1;
  uint8_t reserved_flags7 : 1;
  uint8_t reserved_flags8 : 1;
  
  // Байт 4: состояния реле (битовая маска)
  uint8_t relayStates;  ///< Состояния всех 6 реле (биты 0-5)
};

/// Параметры каждого бака
struct TankParams {
  int minLevel;   ///< Минимальный уровень (см)
  int maxLevel;   ///< Максимальный уровень (см)
};

/// Альтернативное имя для совместимости
typedef TankParams TankSettings;

/// Статистика сбросов Watchdog
struct WDTStats {
  uint32_t resetCount;      ///< Количество сбросов
  uint32_t lastResetTime;   ///< Время последнего сброса
  uint8_t lastResetCause;   ///< Причина сброса
  uint32_t totalUptime;     ///< Общее время работы в секундах
};

/// Структура события в журнале
struct EventLog {
  uint8_t day;        ///< День месяца
  uint8_t month;      ///< Месяц
  uint16_t year;      ///< Год
  uint8_t hour;       ///< Час
  uint8_t minute;     ///< Минута
  uint8_t eventType;  ///< Тип события
  uint16_t eventParam; ///< Параметр события (расширено до 16 бит)
  uint16_t additionalInfo; ///< Дополнительная информация (источник, доп. параметр)
  LogLevel level;     ///< Уровень логирования; 
};

/// Настройки безопасности (инженерное меню)
struct SafetySettings {
  uint16_t timeoutFilling;     ///< Таймаут заполнения (минуты)
  uint16_t timeoutOzonation;   ///< Таймаут озонации (минуты)
  uint16_t timeoutAeration;    ///< Таймаут аэрации (минуты)
  uint16_t timeoutSettling;    ///< Таймаут отстаивания (минуты)
  uint16_t timeoutFiltration;  ///< Таймаут фильтрации (минуты)
  uint16_t timeoutBackwash;    ///< Таймаут промывки (минуты)
  uint32_t engineerPassword;   ///< Пароль инженерного меню (8 цифр)
  uint8_t watchdogEnabled;     ///< Watchdog включен
  uint8_t watchdogTimeout;     ///< Таймаут watchdog (секунды)
  uint16_t pumpDryTimeoutSeconds; ///< Pump dry-run timeout (секунды)
  uint8_t pumpMinLevelDeltaCm; ///< Минимальное изменение уровня (см) для детекции
  uint8_t pumpDryConsecutiveChecks; ///< Количество подряд неудачных проверок для триггера

  // Настройки опроса датчиков (фильтрация удалена — используется только период опроса)
  uint16_t sensorPollPeriod;      ///< Период опроса датчиков в секундах
};

/// Контекст системы — используется для поэтапной миграции глобалов
struct SystemContext {
  SystemState currentState;       ///< Текущее состояние автомата
  SystemState previousState;      ///< Предыдущее состояние автомата
  unsigned long stateStartTime;   ///< Время начала текущего состояния (ms)
};

extern SystemContext systemContext;

#endif // STRUCTURES_H
