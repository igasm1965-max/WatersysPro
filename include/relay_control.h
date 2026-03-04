/*
 * @file relay_control.h
 * @brief Интерфейс управления реле
 */

#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

#include "config.h"
#include "structures.h"

// ============ УПРАВЛЕНИЕ РЕЛЕ ============

/// Получить состояние реле по индексу (0..5)
bool getRelayState(uint8_t index);

/// Установить состояние реле (только во флагах)
void setRelayState(uint8_t index, bool state);

/// Выключить все реле
void turnOffAllRelays();

/// Включить насос 1
void turnOnPump1();

/// Выключить насос 1
void turnOffPump1();

/// Включить насос 2
void turnOnPump2();

/// Выключить насос 2
void turnOffPump2();

/// Включить аэрацию
void turnOnAeration();

/// Выключить аэрацию
void turnOffAeration();

/// Включить озонатор (и аэрацию одновременно)
void turnOnOzone();

/// Выключить озонатор
void turnOffOzone();

/// Включить фильтр
void turnOnFilter();

/// Выключить фильтр
void turnOffFilter();

/// Включить обратную промывку
void turnOnBackwash();

/// Выключить обратную промывку
void turnOffBackwash();

/// Безопасное управление реле с проверкой аварийного режима
void safeRelayControl(uint8_t relayPin, bool state, const char* relayName = "");

/// Проверка конфликтов между реле (например, фильтр и промывка одновременно)
bool checkRelayConflicts();

/// Валидация состояний реле
void validateRelayStates();

/// Получить текущую потребляемую мощность (мВт)
float calculatePowerConsumption();

#endif // RELAY_CONTROL_H