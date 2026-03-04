/**
 * @file event_logging.h
 * @brief Интерфейс логирования событий системы
 */

#ifndef EVENT_LOGGING_H
#define EVENT_LOGGING_H

#include "config.h"
#include "structures.h"

// event source encoded in `additionalInfo` when helpful
enum LogSource : uint16_t {
    SRC_NONE = 0,
    SRC_WEB  = 1,
    SRC_MQTT = 2,
    SRC_BUTTON = 3,
    SRC_SYSTEM = 4
};

// ============ ИНИЦИАЛИЗАЦИЯ ФАЙЛОВОЙ СИСТЕМЫ ============

// removed SPIFFS-specific code

/// Migrate any existing binary-format event log to the new text format.
/// Called automatically during initialization but can be invoked manually.
void migrateOldBinaryLog();

// ============ ЛОГИРОВАНИЕ СОБЫТИЙ ============

/// Сохранение события в журнал (с двумя параметрами)
void saveEventLog(LogLevel level, uint8_t eventType, uint16_t param);

/// Сохранение события в журнал (с тремя параметрами)
// `additionalInfo` is a free 16‑bit field; most callers use it to store
// an extra parameter, and manual‑operation/events may encode a LogSource
// value there as well.
void saveEventLog(LogLevel level, uint8_t eventType, uint16_t param, uint16_t additionalInfo);

// removed SPIFFS-specific code

/// Экспорт событий в удобочитаемом текстовом формате
String exportEventsAsText();

/// Поворот/архивация логов (events.log -> events.log.bak) и создание нового пустого журнала

/// Удаляет события старше указанного возраста (секунды) из памяти и диска
void pruneEventLogs(uint32_t maxAgeSeconds);

/// Загрузка журнала событий
void loadEventLogs();

/// Сохранение журнала в Preferences
void saveEventLogs();

/// Получение количества событий
int getEventCount();

/// Получение события по индексу
EventLog* getEventByIndexSimple(int index);

/// Сравнение двух событий по времени
int compareEvents(EventLog* event1, EventLog* event2);

// ============ ОПИСАНИЯ СОБЫТИЙ ============

/// Получить краткое описание события
void getEventTypeShort(LogLevel level, uint8_t eventType, uint16_t param, char* buffer, uint8_t size);

/// Получить развернутое описание события
void getEventTypeLong(LogLevel level, uint8_t eventType, uint16_t param, char* buffer, uint8_t size);

/// Описание типа события
void getEventTypeDescription(uint8_t eventType, char* buffer, uint8_t size);

/// Описание состояния системы
void getStateDescription(uint8_t state, char* buffer, uint8_t size);

/// Описание типа настройки
void getSettingDescription(uint8_t settingType, char* buffer, uint8_t size);

/// Описание ручной операции
void getManualOperationDescription(uint8_t operation, char* buffer, uint8_t size);

/// Получить строку описания события
const char* getEventText(uint16_t eventCode);

// ============ ПРОСМОТР СОБЫТИЙ ============

/// Отображение списка событий
void displayEventList(int currentIndex, int totalEvents);

/// Отображение деталей события
void displayEventDetails(int eventIndex, int totalEvents);

/// Браузер журнала событий
void eventLogBrowser(int totalEvents);

/// Очистка старых событий (старше 90 дней)
void cleanOldLogs();

/// Проверка и очистка старых логов (вызывается периодически)
void checkAndCleanOldLogs();

/// Меню выбора типа для фильтрации
void showFilterTypeMenu();

/// Просмотр отфильтрованных событий
void displayFilteredEvents(uint8_t filterType);

/// Статистика событий
void displayEventStats();

/// Получает время последнего события указанного типа и параметра
/// Возвращает true если найдено, заполняет hour и minute
bool getLastEventTime(uint8_t eventType, uint16_t param, uint8_t &hour, uint8_t &minute);

/// График событий
void displayEventGraph();

/// Очистка всех событий
void clearEventLogs();

/// Удаляет старые события (старше N секунд)
void deleteOldEventLogs(uint32_t maxAgeSeconds);


/// Меню выбора типа фильтра
void showFilterTypeMenu();

/// Просмотр отфильтрованных событий
void displayFilteredEvents(uint8_t filterType);



/// Экспортирует файлы журнала из SPIFFS на карту SD (если доступна).
/// Возвращает true при успешном копировании.
bool exportLogsToSD();

#endif // EVENT_LOGGING_H
