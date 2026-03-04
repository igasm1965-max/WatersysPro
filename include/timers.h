/*
 * @file timers.h
 * @brief Интерфейс управления таймерами и RTC
 */

#ifndef TIMERS_H
#define TIMERS_H

#include "config.h"
#include "structures.h"

// ============ ИНИЦИАЛИЗАЦИЯ ============

/// Инициализация RTC модуля
void initRTC();

/// Инициализация Watchdog
void initWatchdog();

/// Инициализация I2C шины
void initI2C();

/// Инициализация файловой системы SPIFFS
void initSPIFFS();

// ============ ТАЙМЕРЫ ============

/// Запуск таймера озонации
void startOzonationTimer();

/// Запуск таймера аэрации
void startAerationTimer();

/// Запуск таймера отстаивания
void startSetlingTimer();

/// Запуск таймера обратной промывки
void startBackwashTimer();

/// Остановка всех таймеров
void stopAllTimers();

/// Сброс всех таймеров
void resetAllTimers();

/// Обновление обратного отсчета таймеров
void updateCountdownTimers();

/// Универсальный запуск таймера
void startTimer(uint32_t &timerStart, uint32_t duration);

/// Проверка истечения таймера
bool isTimerExpired(unsigned long timerStart, uint32_t duration);

/// Получить оставшееся время таймера
uint32_t getTimerRemaining(unsigned long timerStart, uint32_t duration);

// ============ ВСПОМОГАТЕЛЬНЫЕ ============

/// Перевод секунд в часы, минуты, секунды
void secondsToHMS(uint32_t seconds, uint8_t& hours, uint8_t& minutes, uint8_t& secs);

/// Перевод времени в миллисекунды
unsigned long timeToMs(byte hours, byte minutes, byte seconds);

/// Проверка истечения интервала времени
bool isTimeElapsed(unsigned long &lastTime, unsigned long interval);

/// Обновление состояния мигания
void updateBlinkState();

/// Проверка зависания системы
void checkForHang();

/// Обработка сброса по Watchdog
void handleWatchdogReset();

/// Получить причину последнего сброса
const char* getResetReason();

/// Обновление статистики WDT
void updateWDTStats();

#endif // TIMERS_H