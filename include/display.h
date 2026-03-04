/*
 * @file display.h
 * @brief Интерфейс управления дисплеем и меню
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include "structures.h"

// ============ УПРАВЛЕНИЕ ДИСПЛЕЕМ ============

/// Инициализация дисплея LCD
void initDisplay();

/// Обновление дисплея
void updateDisplay();

/// Обновление динамических данных на дисплее
void displayDynamicData();

/// Экран аварийного режима
void displayEmergencyScreen();

/// Специальный экран для режима обратной промывки
void displayBackwashScreen();

/// Выводит информацию о промывке фильтра
void displayFilterWashInfo();

/// Активация подсветки
void activateBacklight();

/// Деактивация подсветки
void deactivateBacklight();

/// Автоматическое управление подсветкой
void updateBacklight();

/// Принудительная переотрисовка экрана
void forceRedisplay();

/// Выход из режима экрана backwash
void exitBackwashScreen();

/// Выводит информацию о промывке фильтра
void displayFilterWashInfo();

// ============ МЕНЮ ============

/// Вход в режим меню
void enterMenu();

/// Выход из режима меню
void exitMenu();

/// Обработка меню
void handleMenu();

/// Отображение меню
void displayMenu();

/// Обновление отображения меню
void updateMenuDisplay();

/// Обработка выбора в меню
void handleMenuSelection();

/// Простое меню
int showMenuESP32(const void* menu, int menuLength, int startIndex = 0);

/// Ввод значения через энкодер
int inputValESP32(const char* title, int minVal, int maxVal, int initialVal);

/// Получение выбранного пункта меню
int getSelectedMenuItem();

/// Вход в режим меню с сохранением состояния
void enterMenuMode();

/// Выход из режима меню с восстановлением состояния
void exitMenuMode();

/// Безопасное выполнение операции в меню
void safeMenuOperation();

#endif // DISPLAY_H