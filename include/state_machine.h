/**
 * @file state_machine.h
 * @brief Интерфейс автомата состояний системы
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "config.h"
#include "structures.h"

// ============ АВТОМАТ СОСТОЯНИЙ ============

/// Главный автомат состояний
void stateMachine();

/// Обработка состояния IDLE
void handleIdleState();

/// Обработка состояния FILLING_TANK1
void handleFillingState();

/// Обработка состояния OZONATION
void handleOzonationState();

/// Обработка состояния AERATION
void handleAerationState();

/// Обработка состояния SETTLING
void handleSettlingState();

/// Обработка состояния FILTRATION
void handleFiltrationState();

/// Обработка состояния BACKWASH
void handleBackwashState();

// ============ УПРАВЛЕНИЕ ПЕРЕХОДАМИ ============

/// Проверка расписания и запуск нужных операций
void checkSchedule();

/// Проверка нахождения в рабочем окне расписания
bool isWithinSchedule();

/// Проверка активного ночного режима
bool isNightMode();

/// Запуск цикла обработки воды
void startWaterTreatmentCycle();

/// Применение выходов для заданного состояния
void applyStateOutputs(SystemState state);

/// Централизованный переход состояний — применяет выходы, логирует и обновляет таймеры
void changeState(SystemState newState);

/// Перезапуск цикла обработки воды
void restartWaterTreatmentCycle();

/// Проверка условий для фильтрации
void checkFiltrationConditions();

/// Проверка условий после заполнения бака
void proceedToNextStateAfterFilling();

/// Автоматическое управление системой
void processAutomaticControl();

/// Обновление учета времени работы фильтра
void updateFilterOperationTime();

/// Обновление обратного отсчета таймеров
void updateCountdownTimers();

/// Получить текстовое описание состояния
const char* getStateText(SystemState state);

#endif // STATE_MACHINE_H
