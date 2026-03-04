/*
 * @file engineer_menu.h
 * @brief Интерфейс инженерного меню с защитой паролем
 * @details Позволяет настраивать параметры безопасности системы
 */

#ifndef ENGINEER_MENU_H
#define ENGINEER_MENU_H

#include "config.h"
#include "structures.h"

// ============ ФУНКЦИИ АВТОРИЗАЦИИ ============

/// Проверяет введённый пароль
bool checkEngineerPassword(uint32_t enteredPassword);

/// Показывает экран ввода пароля, возвращает true при успехе
bool showPasswordScreen();

/// Сбрасывает авторизацию инженерного режима
void lockEngineerMode();

/// Проверяет, авторизован ли инженерный режим
bool isEngineerModeUnlocked();

// ============ ФУНКЦИИ ИНЖЕНЕРНОГО МЕНЮ ============

/// Показывает главное инженерное меню
void showEngineerMenu();

/// Показывает меню настройки таймаутов безопасности
void showSafetyTimeoutsMenu();

/// Редактирует таймаут заполнения
void editTimeoutFilling();

/// Редактирует таймаут озонации
void editTimeoutOzonation();

/// Редактирует таймаут аэрации
void editTimeoutAeration();

/// Редактирует таймаут отстаивания
void editTimeoutSettling();

/// Редактирует таймаут фильтрации
void editTimeoutFiltration();

/// Редактирует таймаут промывки
void editTimeoutBackwash();

/// Редактирует таймаут сухого хода насоса (сек)
void editPumpDryTimeout();

/// Редактирует минимальное изменение уровня для детекции насоса (см)
void editPumpMinLevelDelta();

/// Редактирует количество подряд неудачных проверок для сухого хода
void editPumpDryConsecutiveChecks();


/// Показывает меню настроек Watchdog
void showWatchdogMenu();

/// Переключает Watchdog вкл/выкл
void toggleWatchdog();

/// Редактирует таймаут Watchdog
void editWatchdogTimeout();

/// Показывает экран смены пароля
void showChangePasswordScreen();

/// Показывает системную информацию
void showSystemInfo();

/// Показывает информацию о сетевом подключении и токен администратора
void showNetworkInfo();

/// Wi-Fi setup (SSID / Password)
void showWiFiSetup();
void editWiFiSSID();
void editWiFiPassword();
void clearWifiSettings();

// MQTT setup
void showMqttSetup();
void toggleMqttEnabled();
void editMqttBroker();
void editMqttPort();
void editMqttUser();
void editMqttPass();
void editMqttTopicBase();
void editMqttInterval();
void clearMqttSettings();

// Test publish
void testMqttPublish();

/// Event Log settings (select which event types are persisted to SPIFFS)
void showEventLogSettings();



/// Выполняет сброс к заводским настройкам
void performFactoryReset();

// ============ СОХРАНЕНИЕ/ЗАГРУЗКА ============

/// Сохраняет настройки безопасности в Preferences
void saveSafetySettings();

/// Загружает настройки безопасности из Preferences
void loadSafetySettings();

#endif // ENGINEER_MENU_H