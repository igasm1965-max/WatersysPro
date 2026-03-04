/**
 * @file utils.h
 * @brief Вспомогательные утилиты и функции
 */

#ifndef UTILS_H
#define UTILS_H

#include "config.h"
#include "event_logging.h"  // Для saveEventLog

// ============ ПЕЧАТЬ НА ДИСПЛЕЙ ============

/// Печать двухзначного числа с нулем впереди
void printTwoDigits(int number);

/// Печать трехзначного числа с пробелом впереди
void printThreeDigits(int number);

// ============ ИНФОРМАЦИЯ О СИСТЕМЕ ============

/// Возвращает MAC-адрес устройства в виде строки (формат XX:XX:XX:XX:XX:XX)
String getMacString();

/// Заполняет массив `out[6]` байтами MAC (MSB-first)
void getMacBytes(uint8_t out[6]);

/// Информация о сбросе
void printResetInfo();

/// Вывод информации о памяти
void printMemoryInfo();

/// Получить температуру чипа (внутренний датчик)
float getChipTemperature();

/// Проверка статуса питания
void checkPowerStatus();

// ============ ЛОКАЛИЗАЦИЯ / ЯЗЫК ============

/// true если вывод ведётся на английском, false — русский (читается из Preferences)
bool isEnglish();

/// Установить язык вывода (и сохранить в Preferences). Возвращает старое значение.
bool setLanguageEnglish(bool eng);


// ============ УПРАВЛЕНИЕ СИСТЕМОЙ ============



/// Инициализирует I2C шину
void initI2C();

/// Выполняет диагностику системы при загрузке
void runSystemDiagnostics();

/// Вывод информации о причине сброса
void printResetReason();

/// Мягкий сброс всей логики (перезагрузка ESP32)
void softReset();

/// Проверяет, истёк ли интервал времени (millis()).
/// Корректно работает при переполнении millis().
/// Не обновляет переданный таймер — просто проверяет (используйте `isTimeElapsed` в timers.h
/// если требуется одновременное обновление таймера).
bool hasIntervalElapsed(unsigned long start, unsigned long intervalMs);

/// Дамп полной диагностики
void dumpFullDiagnostics();

/// Включить подсветку и сбросить таймер бездействия
void activateBacklight();

/// Выключить подсветку
void deactivateBacklight();

/// Обновить подсветку по таймауту
void updateBacklight();

#ifdef ENABLE_WIFI
  /// Инициализация WiFi
  void setupWiFi();
  
  /// Синхронизация времени с NTP
  void syncTimeWithNTP();
#endif

/// Инициализация SD (SPI mode) — монтирует карту и выводит статус в Serial
bool initSD();

/// После монтирования карта может быть использована для хранения веб‑интерфейса.
/// Эта функция копирует необходимые HTML‑файлы из SPIFFS на SD, если там ещё
/// нет актуальной версии.  Вызывать после initSD() когда карта доступна.
void migrateUiToSd();

/// Форматирует смонтированную SD‑карту (SPIFFS не трогается).
/// Возвращает true при успехе.
bool formatSDCard();

/// Истинно, если карта успешно инициализирована (см. initSD)
extern bool sdPresent;

/// Spinlock guarding all SD/SPIFFS operations
extern portMUX_TYPE sdMux;


#endif // UTILS_H
