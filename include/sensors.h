/*
 * @file sensors.h
 * @brief Интерфейс работы с датчиками уровня воды
 */

#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"

// ============ ДАТЧИКИ ============

/// Инициализация датчиков
void initSensors();

/// Чтение ультразвукового датчика
long readUltrasonic(int trigPin, int echoPin);

/// Чтение всех датчиков и обновление уровней баков
void readUltrasonicSensors();

/// Проверка статуса пустоты баков с гистерезисом
void checkTankEmptyStatus();

/// Читает значение датчика 1 (текущий режим — возвращает raw, фильтрация отключена)
int readFilteredSensor1();

/// Читает значение датчика 2 (текущий режим — возвращает raw, фильтрация отключена)
int readFilteredSensor2();

/// Проверяет, находимся ли в рабочем окне расписания
bool isWithinSchedule();

/// Проверяет, включен ли ночной режим
bool isNightMode();

/// Количество ошибок чтения датчика 1
uint16_t getSensor1ErrorCount();

/// Количество ошибок чтения датчика 2
uint16_t getSensor2ErrorCount();

/// Сброс статистики ошибок датчиков
void resetSensorErrorStats();

#endif // SENSORS_H