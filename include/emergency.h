/*
 * @file emergency.h
 * @brief Интерфейс системы аварийного управления
 */

#ifndef EMERGENCY_H
#define EMERGENCY_H

#include "config.h"
#include "structures.h"

// ============ АВАРИЙНОЕ УПРАВЛЕНИЕ ============
 
/// Инициализация аварийного мониторинга
void initEmergencyMonitoring();
 
/// Проверка аварийных ситуаций
void checkEmergencySituations();
 
/// Обработка аварийного режима
void handleEmergency();
 
/// Обновление аварийного мониторинга
void updateEmergencyMonitoring();
 
/// Проверка нажатия кнопки для сброса аварии
void checkEmergencyReset();
 
/// Сброс аварийного режима
void resetEmergency();
 
/// Триггер аварийного режима
void triggerEmergency(const char* message);
 
/// Возвращает текст текущего аварийного сообщения
const char* getEmergencyMessage();
 
/// Проверяет, активирован ли аварийный режим
bool isInEmergencyMode();
 
// ============ МОНИТОРИНГ НАСОСОВ ============

/// Запуск мониторинга насоса 1
void startPumpEmergencyMonitoring();

/// Остановка мониторинга насоса 1
void stopPumpEmergencyMonitoring();

/// Запуск мониторинга насоса 2
void startPump2EmergencyMonitoring();

/// Остановка мониторинга насоса 2
void stopPump2EmergencyMonitoring();

#endif // EMERGENCY_H