/*
 * @file menu.h
 * @brief Интерфейс системы меню для ESP32
 * @details Навигация по меню, ввод значений через энкодер
 */

#ifndef MENU_H
#define MENU_H

#include "config.h"
#include "structures.h"

// ============ СТРУКТУРЫ МЕНЮ ============

/// Структура пункта меню (совместима со sketch)
struct sMenuItem {
  int id;              ///< ID пункта меню
  int parentId;        ///< ID родительского пункта
  const char* caption; ///< Текст пункта
  void (*handler)();   ///< Указатель на функцию-обработчик
};

// ============ НАВИГАЦИЯ ПО МЕНЮ ============

/// Показывает меню и возвращает выбранный пункт
int showMenu(const sMenuItem* menu, int menuLength, int startIndex = 1);

/// Показывает меню для заданного parentId и возвращает выбранный пункт
int showMenuByParent(const sMenuItem* menu, int menuLength, int parentId);

/// Отображает текущее состояние меню на LCD
void displayMenuPage(const sMenuItem* menu, int menuLength, int currentIndex, int scrollOffset);

/// Находит все пункты с заданным родителем
int findMenuItems(const sMenuItem* menu, int menuLength, int parentId, int* itemIndices, int maxItems);

/// Получает индекс выбранного пункта меню
int getSelectedMenuItemIndex();

/// Запускает многоуровневое меню с обработчиками
void runMenu(const sMenuItem* menu, int menuLength, int rootId);

// ============ ВВОД ЗНАЧЕНИЙ ============

/// Ввод целого числа через энкодер
int inputVal(const char* title, int minVal, int maxVal, int initialVal);

/// Ввод времени (часы, минуты, секунды)
void inputTime(const char* title, uint8_t& hours, uint8_t& minutes, uint8_t& seconds);

/// Ввод даты (день, месяц, год)
void inputDate(const char* title, uint8_t& day, uint8_t& month, uint16_t& year);

/// Подтверждение действия (Yes/No)
bool confirmAction(const char* message);

// ============ ОБРАБОТЧИКИ МЕНЮ ============

/// Общий обработчик настройки таймеров
void CommonTimerHandler();

/// Обработчик настройки уровней баков
void TankSettingsHandler();

/// Обработчик настройки даты и времени
void DateTimeSettingsHandler();

/// Обработчик настройки расписания
void ScheduleSettingsHandler();

/// Обработчик настройки ночного режима
void NightModeSettingsHandler();

/// Обработчик периода очистки фильтра
void FilterCleaningPeriodHandler();

/// Обработчик ручного режима
void ManualOperation();

/// Обработчик просмотра/управления логами
/// Обработчик просмотра логов
void EventLogsHandler();
/// Обработчик статистики логов
void EventStatsHandler();
/// Обработчик графика логов
void EventGraphHandler();
/// Обработчик фильтрации логов
void EventFilterHandler();

/// Очистка всех логов событий (с подтверждением)
void clearEventLogsMenu();

/// Очистка старых логов событий (с подтверждением)
void clearOldEventsMenu();
void pruneEventLogsMenu();
void formatSdMenu();

/// Сброс настроек к значениям по умолчанию
void resetToDefaults();

// ============ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ============

/// Ожидание отпускания кнопки энкодера
void waitForEncoderRelease();

/// Чтение состояния энкодера (вращение/кнопка)
int readEncoderDelta();

/// Проверка нажатия кнопки энкодера
bool isEncoderButtonPressed();

// ============ ДАННЫЕ МЕНЮ ============

/// Основной массив меню
extern sMenuItem mainMenu[];

/// Количество пунктов в главном меню
extern const int mainMenuLength;

#endif // MENU_H