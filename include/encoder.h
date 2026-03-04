/*
 * @file encoder.h
 * @brief Интерфейс работы с энкодером
 */

#ifndef ENCODER_H
#define ENCODER_H

#include "config.h"
#include "structures.h"

// ============ ЭНКОДЕР ============

/// Инициализация энкодера
void initEncoder();

/// Обработка энкодера
void handleEncoder();

/// Получение состояния энкодера (неблокирующий вызов)
eEncoderState getEncoderState();

/// Обработчик прерывания для CLK
void IRAM_ATTR encoderISR();

/// Обработчик прерывания для кнопки
void IRAM_ATTR buttonISR();

/// Проверка состояния энкодера
void checkEncoderState();

/// Проверка энкодера для меню
void checkEncoderForMenu();

/// Проверка активности энкодера
bool isEncoderActive();

/// Проверка таймаута меню (автовыход при бездействии)
void checkMenuIdleTimeout();

// ============ ISR ОБРАБОТЧИКИ ============

/// Обработчик ISR энкодера
void handleEncoderISR();

/// Обработчик ISR кнопки энкодера
void handleButtonISR();

/// Обработчик события - вращение вправо
void onEncoderRight();

/// Обработчик события - вращение влево
void onEncoderLeft();

/// Обработчик события - нажатие кнопки
void onEncoderButton();

/// Получение позиции энкодера
int getEncoderPosition();

/// Проверка нажатия кнопки
bool wasEncoderButtonPressed();

/// Сброс позиции энкодера
void resetEncoderPosition();

/// Проверка: кнопка энкодера нажата сейчас
bool isEncoderButtonPressed();

/// Ждёт отпускания кнопки энкодера (блокирующая)
void waitForButtonRelease();

/// Восстановление состояния системы после меню
void restoreSystemStateAfterMenu();

// Тестирование энкодера удалено из релизной сборки

#endif // ENCODER_H