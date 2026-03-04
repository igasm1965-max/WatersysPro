/**
 * @file relay_control.cpp
 * @brief Реализация управления реле системы
 * @details Функции для управления помпами, аэрацией, озонацией и фильтрацией
 */

#include "relay_control.h"
#include "config.h"
#include "structures.h"
#include "emergency.h"

// ============ УПРАВЛЕНИЕ НАСОСОМ 1 ============

/// Состояние насоса 1
static bool pump1_state = false;

/// Включает насос 1
void turnOnPump1() {
  extern SystemFlags flags;
  if (flags.emergencyMode) {
    digitalWrite(PUMP_PIN, LOW);
    pump1_state = false;
    return;
  }
  if (!pump1_state) {
    digitalWrite(PUMP_PIN, HIGH);
    pump1_state = true;
    // Запускаем мониторинг сухого хода при любом включении насоса
    startPumpEmergencyMonitoring();
  }
}

/// Выключает насос 1
void turnOffPump1() {
  if (pump1_state) {
    digitalWrite(PUMP_PIN, LOW);
    pump1_state = false;
    stopPumpEmergencyMonitoring();
  }
}

/// Возвращает состояние насоса 1
bool getPump1State() {
  return pump1_state;
}

// ============ УПРАВЛЕНИЕ НАСОСОМ 2 ============

/// Состояние насоса 2
static bool pump2_state = false;

/// Включает насос 2
void turnOnPump2() {
  extern SystemFlags flags;
  if (flags.emergencyMode) {
    digitalWrite(PUMP2_PIN, LOW);
    pump2_state = false;
    return;
  }
  if (!pump2_state) {
    digitalWrite(PUMP2_PIN, HIGH);
    pump2_state = true;
    // Запускаем мониторинг сухого хода для насоса 2
    startPump2EmergencyMonitoring();
  }
}

/// Выключает насос 2
void turnOffPump2() {
  if (pump2_state) {
    digitalWrite(PUMP2_PIN, LOW);
    pump2_state = false;
    stopPump2EmergencyMonitoring();
  }
}

/// Возвращает состояние насоса 2
bool getPump2State() {
  return pump2_state;
}

// ============ УПРАВЛЕНИЕ АЭРАЦИЕЙ ============

/// Состояние аэрации
static bool aeration_state = false;

/// Включает аэрацию
void turnOnAeration() {
  extern SystemFlags flags;
  if (flags.emergencyMode) {
    digitalWrite(AERATION_PIN, LOW);
    aeration_state = false;
    return;
  }
  if (!aeration_state) {
    digitalWrite(AERATION_PIN, HIGH);
    aeration_state = true;
  }
}

/// Выключает аэрацию
void turnOffAeration() {
  if (aeration_state) {
    digitalWrite(AERATION_PIN, LOW);
    aeration_state = false;
  }
}

/// Возвращает состояние аэрации
bool getAerationState() {
  return aeration_state;
}

// ============ УПРАВЛЕНИЕ ОЗОНАЦИЕЙ ============

/// Состояние озонации
static bool ozone_state = false;

/// Включает озонацию
void turnOnOzone() {
  extern SystemFlags flags;
  if (flags.emergencyMode) {
    digitalWrite(OZONE_PIN, LOW);
    ozone_state = false;
    return;
  }
  if (!ozone_state) {
    digitalWrite(OZONE_PIN, HIGH);
    ozone_state = true;
  }
}

/// Выключает озонацию
void turnOffOzone() {
  if (ozone_state) {
    digitalWrite(OZONE_PIN, LOW);
    ozone_state = false;
  }
}

/// Возвращает состояние озонации
bool getOzoneState() {
  return ozone_state;
}

// ============ УПРАВЛЕНИЕ ФИЛЬТРОМ ============

/// Состояние фильтра
static bool filter_state = false;

/// Включает фильтр
void turnOnFilter() {
  extern SystemFlags flags;
  if (flags.emergencyMode) {
    digitalWrite(FILTER_PIN, LOW);
    filter_state = false;
    return;
  }
  if (!filter_state) {
    digitalWrite(FILTER_PIN, HIGH);
    filter_state = true;
  }
}

/// Выключает фильтр
void turnOffFilter() {
  if (filter_state) {
    digitalWrite(FILTER_PIN, LOW);
    filter_state = false;
  }
}

/// Возвращает состояние фильтра
bool getFilterState() {
  return filter_state;
}

// ============ УПРАВЛЕНИЕ ОБРАТНОЙ ПРОМЫВКОЙ ============

/// Состояние обратной промывки
static bool backwash_state = false;

/// Включает обратную промывку
void turnOnBackwash() {
  extern SystemFlags flags;
  if (flags.emergencyMode) {
    digitalWrite(BACKWASH_PIN, LOW);
    backwash_state = false;
    return;
  }
  if (!backwash_state) {
    digitalWrite(BACKWASH_PIN, HIGH);
    backwash_state = true;
  }
}

/// Выключает обратную промывку
void turnOffBackwash() {
  if (backwash_state) {
    digitalWrite(BACKWASH_PIN, LOW);
    backwash_state = false;
  }
}

/// Возвращает состояние обратной промывки
bool getBackwashState() {
  return backwash_state;
}

// ============ УПРАВЛЕНИЕ ГРУППОЙ РЕЛЕ ============

/// Выключает все реле (экстренный останов)
void turnOffAllRelays() {
  turnOffPump1();
  turnOffPump2();
  turnOffAeration();
  turnOffOzone();
  turnOffFilter();
  turnOffBackwash();
}

/// Устанавливает состояние конкретного реле
void setRelayState(uint8_t relay, bool state) {
  switch(relay) {
    case RELAY_PUMP1:
      state ? turnOnPump1() : turnOffPump1();
      break;
    case RELAY_PUMP2:
      state ? turnOnPump2() : turnOffPump2();
      break;
    case RELAY_AERATION:
      state ? turnOnAeration() : turnOffAeration();
      break;
    case RELAY_OZONE:
      state ? turnOnOzone() : turnOffOzone();
      break;
    case RELAY_FILTER:
      state ? turnOnFilter() : turnOffFilter();
      break;
    case RELAY_BACKWASH:
      state ? turnOnBackwash() : turnOffBackwash();
      break;
  }
}

/// Получает состояние конкретного реле
bool getRelayState(uint8_t relay) {
  switch(relay) {
    case RELAY_PUMP1: return getPump1State();
    case RELAY_PUMP2: return getPump2State();
    case RELAY_AERATION: return getAerationState();
    case RELAY_OZONE: return getOzoneState();
    case RELAY_FILTER: return getFilterState();
    case RELAY_BACKWASH: return getBackwashState();
    default: return false;
  }
}

