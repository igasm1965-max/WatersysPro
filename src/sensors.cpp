/**
 * @file sensors.cpp
 * @brief Реализация работы с датчиками системы
 * @details Функции для чтения УЗ датчиков уровня воды
 */

#include "sensors.h"

#include <NewPing.h>

#include "config.h"
#include "structures.h"

extern SafetySettings safetySettings;
static NewPing sonar1(HC_TRIG1, HC_ECHO1, TANK_MAX_SENSOR_DISTANCE);
static NewPing sonar2(HC_TRIG2, HC_ECHO2, TANK_MAX_SENSOR_DISTANCE);

// ============ ИНИЦИАЛИЗАЦИЯ ДАТЧИКОВ ============

void initSensors() {
    // УЗ датчики уже инициализированы в initPins()
}

// ============ ЧТЕНИЕ УЗ ДАТЧИКОВ ============

// Legacy pulseIn() implementation removed — using NewPing (see readUltrasonicSensor1/2)

/// Читает УЗ датчик 1 (raw via NewPing)
int readUltrasonicSensor1() {
    return (int)sonar1.ping_cm();  // 0 == no echo
}

/// Читает УЗ датчик 2 (raw via NewPing)
int readUltrasonicSensor2() {
    int value = (int)sonar2.ping_cm();  // 0 == no echo
    // Debug monitoring for sensor 2
    static unsigned long lastDebugTime = 0;
    if (millis() - lastDebugTime > 5000) {  // Print every 5 seconds
        lastDebugTime = millis();
        // Serial.printf("[SENSOR2_DEBUG] raw=%d, TRIG=%d, ECHO=%d, MAX_DIST=%d\n",
        //               value, HC_TRIG2, HC_ECHO2, TANK_MAX_SENSOR_DISTANCE);
    }
    return value;
}

// ============ КАЛИБРАЦИЯ ДАТЧИКОВ ============

/// Калибрует датчик 1 (устанавливает нулевое значение на пустом баке)
void calibrateSensor1(int emptyDistance) {
    static int sensor1EmptyDistance = 0;
    sensor1EmptyDistance = emptyDistance;
#ifdef DEBUG_SERIAL
    Serial.print("[DEBUG] Sensor 1 calibrated: empty=");
    Serial.println(sensor1EmptyDistance);
#endif
}

/// Калибрует датчик 2
void calibrateSensor2(int emptyDistance) {
    static int sensor2EmptyDistance = 0;
    sensor2EmptyDistance = emptyDistance;
#ifdef DEBUG_SERIAL
    Serial.print("[DEBUG] Sensor 2 calibrated: empty=");
    Serial.println(sensor2EmptyDistance);
#endif
}

// Фильтрация отключена — буферы сняты (используются сырые значения датчиков)

// ============ СТАТИСТИКА ОШИБОК ДАТЧИКОВ ============

static uint16_t sensor1ErrorCount = 0;
static uint16_t sensor2ErrorCount = 0;
static uint8_t sensor1ConsecutiveErrors = 0;
static uint8_t sensor2ConsecutiveErrors = 0;
static unsigned long lastSensor1ErrorLogTime = 0;
static unsigned long lastSensor2ErrorLogTime = 0;

// Фильтрация отключена — возвращаем сырые значения от ультразвуковых датчиков
int readFilteredSensor1() {
    return readUltrasonicSensor1();  // raw value, 0 == no echo
}

int readFilteredSensor2() {
    return readUltrasonicSensor2();  // raw value, 0 == no echo
}

// ============ ЧТЕНИЕ УЗ ДАТЧИКОВ С ОБРАБОТКОЙ ============

/// Читает оба датчика и обновляет глобальные переменные уровней
void readUltrasonicSensors() {
    extern int tank1Level, tank2Level;
    extern int tank1RawDistance, tank2RawDistance;

    // Читаем сырые значения (0 == нет эхо/вне диапазона)
    int raw1 = readFilteredSensor1();
    int raw2 = readFilteredSensor2();
    tank1RawDistance = raw1;
    tank2RawDistance = raw2;

    // Защита от ложных нулей отключена: применяем сырое значение сразу.
    tank1Level = raw1;
    tank2Level = raw2;

    // Extended logging for sensor debugging
    static unsigned long lastExtendedLog = 0;
    if (millis() - lastExtendedLog > 10000) {  // Print every 10 seconds
        lastExtendedLog = millis();
        // Serial.printf("[SENSOR_EXTENDED] Sensor1: raw=%d, level=%d | Sensor2: raw=%d, level=%d | GPIO2_TRIG=%d, GPIO2_ECHO=%d\n",
        //               raw1, tank1Level, raw2, tank2Level, digitalRead(HC_TRIG2), digitalRead(HC_ECHO2));
    }

    Serial.printf("[SENSOR] raw1=%d raw2=%d -> tank1=%d tank2=%d\n", tank1RawDistance,
                  tank2RawDistance, tank1Level, tank2Level);
}

// ============ ПРОВЕРКА СТАТУСА БАКОВ ============

/// Проверяет, пусты ли баки — без дебаунса/фильтрации, используем текущие значения
void checkTankEmptyStatus() {
    extern SystemFlags flags;
    extern int tank1Level, tank2Level;
    extern TankParams tank1, tank2;

    const int hysteresis = TANK_HYSTERESIS;

    uint8_t prev1 = flags.tank1Empty;
    uint8_t prev2 = flags.tank2Empty;

    flags.tank1Empty = (tank1Level >= tank1.maxLevel + hysteresis) ? 1 : 0;
    flags.tank2Empty = (tank2Level >= tank2.maxLevel + hysteresis) ? 1 : 0;

    // Debug: лог при изменении флагов пустоты
    if (flags.tank1Empty != prev1 || flags.tank2Empty != prev2) {
        Serial.printf("[SENSOR] flag change: tank1Empty=%d (lvl=%d) tank2Empty=%d (lvl=%d)\n",
                      flags.tank1Empty, tank1Level, flags.tank2Empty, tank2Level);
    }
}

// ============ ПРОВЕРКА РАСПИСАНИЯ ============

/// Проверяет, находимся ли в рабочем окне расписания
bool isWithinSchedule() {
    extern SystemFlags flags;
    extern RTC_DS3231 rtc;
    extern byte scheduleStart[3];
    extern byte scheduleEnd[3];

    if (!flags.scheduleEnabled || !flags.rtcInitialized) return true;

    DateTime now = rtc.now();
    int currentTimeInMinutes = now.hour() * 60 + now.minute();
    int start = scheduleStart[0] * 60 + scheduleStart[1];
    int end = scheduleEnd[0] * 60 + scheduleEnd[1];

    // Обычный случай: интервал не пересекает полночь
    if (start < end)
        return (currentTimeInMinutes >= start && currentTimeInMinutes < end);
    else
        // Интервал через полночь
        return (currentTimeInMinutes >= start || currentTimeInMinutes < end);
}

// ============ СТАТИСТИКА ОШИБОК ДАТЧИКОВ ============

uint16_t getSensor1ErrorCount() { return sensor1ErrorCount; }

uint16_t getSensor2ErrorCount() { return sensor2ErrorCount; }

void resetSensorErrorStats() {
    sensor1ErrorCount = 0;
    sensor2ErrorCount = 0;
    sensor1ConsecutiveErrors = 0;
    sensor2ConsecutiveErrors = 0;
    lastSensor1ErrorLogTime = 0;
    lastSensor2ErrorLogTime = 0;
}

/// Проверяет, включен ли ночной режим
bool isNightMode() {
    extern SystemFlags flags;
    extern RTC_DS3231 rtc;
    extern byte nightModeStart[2];
    extern byte nightModeEnd[2];

    if (!flags.nightModeEnabled || !flags.rtcInitialized) return false;

    DateTime now = rtc.now();
    int currentTimeInMinutes = now.hour() * 60 + now.minute();
    int start = nightModeStart[0] * 60 + nightModeStart[1];
    int end = nightModeEnd[0] * 60 + nightModeEnd[1];

    if (start < end)
        return (currentTimeInMinutes >= start && currentTimeInMinutes < end);
    else
        return (currentTimeInMinutes >= start || currentTimeInMinutes < end);
}
