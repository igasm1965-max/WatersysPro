/**
 * @file encoder.cpp
 * @brief Реализация управления энкодером и его обработки
 * @details Функции чтения поворотного энкодера и кнопки
 */

#include "encoder.h"
#include "config.h"
#include "structures.h"
#include "display.h"
#include "menu.h"
#include "event_logging.h"
#include "relay_control.h"
#include "timers.h"
#include "utils.h"
#include "state_machine.h"

// Faster, ISR-safe GPIO + timer access
#include "driver/gpio.h"
#include "esp_timer.h"

// ============ ЭКСТЕРНЫ ============
// FSM state migrated to `systemContext.currentState`

// ============ ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ЭНКОДЕРА ============

volatile int encoderPos = 0;              ///< Текущая позиция энкодера
volatile bool encoderButtonPressed = false; ///< Флаг нажатия кнопки
volatile unsigned long lastEncoderInterrupt = 0; ///< Время последнего прерывания

// Для информационных экранов при нажатии кнопки
static int infoScreenIndex = 0;  ///< Текущий информационный экран (0=System, 1=Tanks, 2=Emergency)
#define NUM_INFO_SCREENS 3  ///< Количество информационных экранов

// Для главного меню (устаревшая статическая реализация удалена)
// Используется data-driven меню из `mainMenu[]` (см. src/menu_data.cpp + runMenu)
#define LCD_ROWS 4                  ///< Количество строк на LCD

// ...legacy mainMenuItems[] removed. Use `runMenu(mainMenu, mainMenuLength, mkRoot)` instead.


// ============ FORWARD DECLARATIONS ============
void onEncoderRight();
void onEncoderLeft();
void onEncoderButton();
void initInfoScreen();  ///< Инициализирует статичную часть экрана
void updateInfoScreen(); ///< Обновляет только динамичные значения
void displayMainMenu(); ///< Отображает главное меню на LCD
void handleMenuSelection(int itemIndex); ///< Обрабатывает выбор пункта меню
void showInfoScreen_System();
void showInfoScreen_Tanks();
void showInfoScreen_Emergency();
extern void ManualOperation(); ///< Ручное управление реле (из menu.cpp)

// ============ ИНИЦИАЛИЗАЦИЯ ЭНКОДЕРА ============

/// Инициализирует энкодер (подключает прерывания)
void initEncoder() {
  // Прерывание на CLK при изменении
  attachInterrupt(digitalPinToInterrupt(pinCLK), handleEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pinSW), handleButtonISR, FALLING);
}

// ============ ОБРАБОТКА ПРЕРЫВАНИЙ ============

// Дополнительные счётчики для кнопки (для диагностики)
volatile int encoderISRButtonCount = 0;   // успешные события ISR
volatile int encoderISRButtonIgnored = 0; // игнорированные события ISR (debounce)

/// ISR обработчик энкодера (минимальная, быстрая реализация)
void IRAM_ATTR handleEncoderISR() {
  static uint32_t lastISR_ms = 0;
  static int lastCLK = HIGH;

  // Быстрые чтения GPIO (ISR-safe)
  int clk = gpio_get_level((gpio_num_t)pinCLK);
  int dt  = gpio_get_level((gpio_num_t)pinDT);

  // Реагируем только на изменение CLK
  if (clk == lastCLK) return;
  lastCLK = clk;

  // Debounce — используем esp_timer_get_time() (ISR-safe) и миллисекунды
  uint64_t now_us = esp_timer_get_time();
  uint32_t now_ms = (uint32_t)(now_us / 1000ULL);
  if ((uint32_t)(now_ms - lastISR_ms) < 5) return; // 5 ms
  lastISR_ms = now_ms;

  // Определяем направление на спаде CLK
  if (clk == LOW) {
    if (dt == HIGH) encoderPos++;
    else encoderPos--;
  }

  // Обновляем время последнего события (для подавления ложных нажатий)
  lastEncoderInterrupt = now_ms;
}

/// ISR обработчик кнопки энкодера
void handleButtonISR() {
  static unsigned long lastPressTime = 0;
  unsigned long currentTime = millis();
  
  // Увеличенный debounce для максимальной четкости
  if (currentTime - lastPressTime < 200) {
    return;
  }
  
  // Дополнительная проверка состояния кнопки
  if (digitalRead(pinSW) == LOW) {
    // Если нажали очень скоро после поворота — игнорируем как ложное срабатывание
    // уменьшаем окно до 40ms, чтобы быстрые намеренные нажатия не терялись
    if (currentTime - lastEncoderInterrupt < 40) {
      lastPressTime = currentTime;
      return;
    }
    encoderButtonPressed = true;
    lastPressTime = currentTime;
  }
}

// `pollEncoder()` removed — encoder is fully interrupt-driven.

/// Получение состояния энкодера (неблокирующий вызов)
eEncoderState getEncoderState() {
  extern volatile int encoderPos;
  extern volatile bool encoderButtonPressed;
  
  // Проверяем кнопку
  if (encoderButtonPressed) {
    encoderButtonPressed = false;
    return eButton;
  }
  
  // Проверяем вращение
  if (encoderPos > 0) {
    encoderPos = 0;
    return eRight;
  } else if (encoderPos < 0) {
    encoderPos = 0;
    return eLeft;
  }
  
  return eNone;
}


// ============ ОСНОВНАЯ ОБРАБОТКА ЭНКОДЕРА ============

/// Обрабатывает вращение энкодера (в главном цикле)
void handleEncoder() {
  extern volatile int encoderPos;
  extern volatile bool encoderButtonPressed;  // ISR устанавливает эту переменную
  extern unsigned long lastEncoderAction;
  extern unsigned long lastActivityTime;
  extern SystemFlags flags;
  
  static unsigned long infoScreenUpdateTime = 0;  // Таймер обновления информационного экрана
  
  unsigned long currentTime = millis();
  
  // ========== ЕСЛИ В МЕНЮ или В АВАРИЙНОМ РЕЖИМЕ - НЕ ОБРАБАТЫВАЕМ ЗДЕСЬ ==========
  // Энкодер в меню обрабатывается через readEncoderDelta() в showMenu()
  if (flags.inMenu || flags.emergencyMode) {
    return;
  }
  
  // Периодическое обновление информационного экрана (каждые 500мс)
  if (flags.displayLocked && (currentTime - infoScreenUpdateTime >= 500)) {
    updateInfoScreen();
    infoScreenUpdateTime = currentTime;
  }
  

  
  // Проверяем вращение (не чаще чем раз в 30мс) — чуть более отзывчиво
  if (currentTime - lastEncoderAction < 30) {
    return;
  }
  
  // Обновляем время последней активности
  if (encoderPos != 0 || encoderButtonPressed) {
    lastActivityTime = currentTime;
  }
  
  // Обработка вращения
  if (encoderPos > 0) {
    onEncoderRight();
    encoderPos = 0;
  } else if (encoderPos < 0) {
    onEncoderLeft();
    encoderPos = 0;
  }
  
  // Обработка кнопки
  if (encoderButtonPressed) {
    onEncoderButton();
    encoderButtonPressed = false;
  }
  
  lastEncoderAction = currentTime;
}

// ============ ОБРАБОТЧИКИ СОБЫТИЙ ЭНКОДЕРА ============

/// Инициализирует СТАТИЧНУЮ часть информационного экрана (вызывается один раз)
void initInfoScreen() {
  extern SafeLCD lcd;
  
  lcd.clear();
  
  if (infoScreenIndex == 0) {
    // System Screen - заголовки
    lcd.setCursor(0, 0);
    lcd.print("State:          ");  // 16 символов для выравнивания
    lcd.setCursor(0, 1);
    lcd.print("Ozone:  s Air:  s");
    lcd.setCursor(0, 2);
    lcd.print("Setl:  s F.Wash:  ");
    lcd.setCursor(0, 3);
    lcd.print("< Tanks  |  Exit >");
    
  } else if (infoScreenIndex == 1) {
    // Tanks Screen
    lcd.setCursor(0, 0);
    lcd.print("TANK LEVELS");
    lcd.setCursor(0, 1);
    lcd.print("Tank1:  % (  -  )");
    lcd.setCursor(0, 2);
    lcd.print("Tank2:  % (  -  )");
    lcd.setCursor(0, 3);
    lcd.print("< System | Exit >");
    
  } else if (infoScreenIndex == 2) {
    // Emergency Screen
    lcd.setCursor(0, 0);
    lcd.print("EMERGENCY STATUS");
    lcd.setCursor(0, 1);
    lcd.print("Mode:            ");
    lcd.setCursor(0, 2);
    lcd.print("Tank1:       ");
    lcd.setCursor(0, 3);
    lcd.print("Tank2:       <");
  }
}

/// Обновляет ДИНАМИЧНУЮ часть информационного экрана (вызывается каждые 500мс)
void updateInfoScreen() {
  extern SafeLCD lcd;
  // FSM state migrated to `systemContext.currentState`
  extern uint32_t currentAerationRemaining;
  extern uint32_t currentOzonationRemaining;
  extern uint32_t currentSetlingRemaining;
  extern int tank1Level, tank2Level;
  extern TankParams tank1, tank2;
  extern SystemFlags flags;
  extern unsigned long filterOperationStartTime;
  extern uint32_t filterCleaningInterval;
  
  if (infoScreenIndex == 0) {
    // ===== System Screen - обновляем только значения =====
    
    // Строка 0: Состояние (6 символов для State)
    lcd.setCursor(6, 0);
    const char* stateStr = "IDLE    ";
    if (systemContext.currentState == STATE_FILLING_TANK1) stateStr = "FILLING ";
    else if (systemContext.currentState == STATE_OZONATION) stateStr = "OZONE   ";
    else if (systemContext.currentState == STATE_AERATION) stateStr = "AERATE  ";
    else if (systemContext.currentState == STATE_SETTLING) stateStr = "SETTLE  ";
    else if (systemContext.currentState == STATE_FILTRATION) stateStr = "FILTER  ";
    else if (systemContext.currentState == STATE_BACKWASH) stateStr = "BACKWASH";
    lcd.print(stateStr);
    
    // Строка 1: Озон (7 символов для числа)
    lcd.setCursor(6, 1);
    if (currentOzonationRemaining < 10) lcd.print(" ");
    if (currentOzonationRemaining < 100) lcd.print(" ");
    lcd.print(currentOzonationRemaining);

    // Вставляем время последнего старта озонации в пустую область (между Ozone и Air)
    {
      uint8_t hh = 0, mm = 0;
      if (getLastEventTime(EVENT_SYSTEM_STATE_CHANGE, (uint8_t)STATE_OZONATION, hh, mm)) {
        lcd.setCursor(9, 1);
        if (hh < 10) lcd.print('0');
        lcd.print(hh);
        lcd.print(':');
        if (mm < 10) lcd.print('0');
        lcd.print(mm);
      } else {
        lcd.setCursor(9, 1);
        lcd.print("     ");
      }
    }

    // Строка 1: Воздух (14 символов)
    lcd.setCursor(14, 1);
    if (currentAerationRemaining < 10) lcd.print(" ");
    if (currentAerationRemaining < 100) lcd.print(" ");
    lcd.print(currentAerationRemaining);

    // Мини-индикация времени старта аэрации (минуты) если есть место
    {
      uint8_t hh = 0, mm = 0;
      if (getLastEventTime(EVENT_SYSTEM_STATE_CHANGE, (uint8_t)STATE_AERATION, hh, mm)) {
        lcd.setCursor(18, 1);
        if (mm < 10) lcd.print('0');
        lcd.print(mm);
      } else {
        lcd.setCursor(18, 1);
        lcd.print(' ');
      }
    }

    // Строка 2: Settling (5 символов)
    lcd.setCursor(5, 2);
    if (currentSetlingRemaining < 10) lcd.print(" ");
    if (currentSetlingRemaining < 100) lcd.print(" ");
    lcd.print(currentSetlingRemaining);

    // Показать время старта Settling
    {
      uint8_t hh = 0, mm = 0;
      if (getLastEventTime(EVENT_SYSTEM_STATE_CHANGE, (uint8_t)STATE_SETTLING, hh, mm)) {
        lcd.setCursor(10, 2);
        if (hh < 10) lcd.print('0');
        lcd.print(hh);
        lcd.print(':');
        if (mm < 10) lcd.print('0');
        lcd.print(mm);
      } else {
        lcd.setCursor(10, 2);
        lcd.print("     ");
      }
    }
    
    // Строка 2: F.Wash таймер (позиция 16, 4 символа)
    lcd.setCursor(16, 2);
    if (filterCleaningInterval == 0 || filterOperationStartTime == 0) {
      lcd.print("--d ");
    } else if (flags.filterCleaningNeeded) {
      lcd.print("NOW!");
    } else {
      unsigned long cleaningPeriodMs = filterCleaningInterval * 1000UL;
      unsigned long elapsed = millis() - filterOperationStartTime;
      if (elapsed > cleaningPeriodMs) elapsed = cleaningPeriodMs;
      unsigned long remaining = (cleaningPeriodMs > elapsed) ? (cleaningPeriodMs - elapsed) : 0;
      unsigned long days = remaining / 1000UL / 86400UL;
      if (days > 99) {
        lcd.print(">99d");
      } else {
        if (days < 10) lcd.print(" ");
        lcd.print(days);
        lcd.print("d ");
      }
    }
    
  } else if (infoScreenIndex == 1) {
    // ===== Tanks Screen - обновляем только значения =====
    
    // Строка 1: Tank1 уровень (6 символов)
    lcd.setCursor(6, 1);
    if (tank1Level < 10) lcd.print(" ");
    if (tank1Level < 100) lcd.print(" ");
    lcd.print(tank1Level);
    
    // Tank1 min (9 символов)
    lcd.setCursor(10, 1);
    if (tank1.minLevel < 10) lcd.print(" ");
    if (tank1.minLevel < 100) lcd.print(" ");
    lcd.print(tank1.minLevel);
    
    // Tank1 max (12 символов)
    lcd.setCursor(12, 1);
    if (tank1.maxLevel < 10) lcd.print(" ");
    if (tank1.maxLevel < 100) lcd.print(" ");
    lcd.print(tank1.maxLevel);
    
    // Строка 2: Tank2 уровень (6 символов)
    lcd.setCursor(6, 2);
    if (tank2Level < 10) lcd.print(" ");
    if (tank2Level < 100) lcd.print(" ");
    lcd.print(tank2Level);
    
    // Tank2 min (9 символов)
    lcd.setCursor(10, 2);
    if (tank2.minLevel < 10) lcd.print(" ");
    if (tank2.minLevel < 100) lcd.print(" ");
    lcd.print(tank2.minLevel);
    
    // Tank2 max (12 символов)
    lcd.setCursor(12, 2);
    if (tank2.maxLevel < 10) lcd.print(" ");
    if (tank2.maxLevel < 100) lcd.print(" ");
    lcd.print(tank2.maxLevel);
    
  } else if (infoScreenIndex == 2) {
    // ===== Emergency Screen - обновляем только значения =====
    
    // Строка 1: Mode (5 символов)
    lcd.setCursor(5, 1);
    if (flags.emergencyMode) {
      lcd.print("ACTIVE      ");
    } else {
      lcd.print("NORMAL      ");
    }
    
    // Строка 2: Tank1 (6 символов)
    lcd.setCursor(6, 2);
    if (flags.tank1Empty) {
      lcd.print("EMPTY ");
    } else {
      lcd.print("OK    ");
    }
    
    // Строка 3: Tank2 (6 символов)
    lcd.setCursor(6, 3);
    if (flags.tank2Empty) {
      lcd.print("EMPTY ");
    } else {
      lcd.print("OK    ");
    }
  }
}

/// Старая функция - больше не используется, но оставляю для совместимости
void showInfoScreen_System() {
  // Теперь используется двухуровневый подход: initInfoScreen() + updateInfoScreen()
}

void showInfoScreen_Tanks() {
  // Теперь используется двухуровневый подход: initInfoScreen() + updateInfoScreen()
}

void showInfoScreen_Emergency() {
  // Теперь используется двухуровневый подход: initInfoScreen() + updateInfoScreen()
}

// Legacy menu display and selection handling removed.
// The data-driven menu (`mainMenu[]` + `runMenu()`) now controls all menu navigation, display and handlers.
// See src/menu.cpp for showMenu(), runMenu(), and handler implementations.


/// Вращение энкодера вправо
void onEncoderRight() {
  extern unsigned long menuIdleTimerStart;
  extern SystemFlags flags;
  extern SafeLCD lcd;
  extern int filterWashScrollOffset;
  
  // ========== РЕЖИМ FILTERWASHINFO (как в оригинале) ==========
  if (flags.inFilterWashInfo) {
    // Прокрутка информационных экранов
    filterWashScrollOffset++;
    if (filterWashScrollOffset > 3) filterWashScrollOffset = 0;
    return;
  }
  
  if (flags.inMenu) {
    // Меню теперь обрабатывается блокирующим `runMenu()` (data-driven).
    // Здесь просто обновляем таймер неактивности и выходим.
    menuIdleTimerStart = millis();
    return;
  } else {
    // Поворот вправо в обычном режиме → открыть главное меню

    
    // Включаем подсветку если выключена
    if (!flags.backlightOn) {
      flags.backlightOn = true;
      lcd.backlight();
      lcd.display();

    }
    
    // Очищаем дисплей перед входом в меню
    lcd.clear();
    
    // ВХОДИМ В МЕНЮ
    flags.inMenu = true;
    flags.displayLocked = false;  // Снимаем блокировку дисплея
    flags.displayInitialized = false;
    menuIdleTimerStart = millis();  // Устанавливаем таймер меню
    

    
    // Выключаем все реле при входе в меню для безопасности
    extern void turnOffAllRelays();
    turnOffAllRelays();
    
    runMenu(mainMenu, mainMenuLength, mkRoot);
    
    // Очистим возможные накопленные шаги энкодера, чтобы избежать «прилипания» поворота после выхода
    extern int readEncoderDelta();
    readEncoderDelta();

    // Восстанавливаем состояния реле при выходе из меню
    extern void applyStateOutputs(SystemState state);
    // FSM state accessed via systemContext
    applyStateOutputs(systemContext.currentState);
    
    flags.inMenu = false;
    forceRedisplay();
  }
}

/// Вращение энкодера влево
void onEncoderLeft() {
  extern unsigned long menuIdleTimerStart;
  extern SystemFlags flags;
  extern SafeLCD lcd;
  extern int filterWashScrollOffset;
  
  // ========== РЕЖИМ FILTERWASHINFO (как в оригинале) ==========
  if (flags.inFilterWashInfo) {
    // Прокрутка информационных экранов назад
    filterWashScrollOffset--;
    if (filterWashScrollOffset < 0) filterWashScrollOffset = 3;
    return;
  }
  
  if (flags.inMenu) {
    // Меню обрабатывается `runMenu()`; здесь только обновляем таймер и выходим
    menuIdleTimerStart = millis();
    return;
  } else {
    // Left rotation in normal mode - no action
  }
}

/// Нажатие кнопки энкодера
void onEncoderButton() {
  extern unsigned long menuIdleTimerStart;
  extern SystemFlags flags;
  extern unsigned long lastActivityTime;
  extern SafeLCD lcd;
  extern int filterWashScrollOffset;
  
  lastActivityTime = millis();
  
  // Сохраняем состояние экрана ДО нажатия (как в оригинале)
  bool wasBacklightOn = flags.backlightOn;
  bool wasDisplayOn = flags.displayOn;
  
  // Если в аварийном режиме — игнорируем короткие нажатия (для сброса используется длительное нажатие в checkEmergencyReset())
  if (flags.emergencyMode) {
    return;
  }
  
  // Всегда включаем подсветку и дисплей при нажатии (как в оригинале - activateBacklight)
  if (!flags.displayOn) {
    lcd.display();
    flags.displayOn = 1;
    flags.displayInitialized = 0;
  }
  if (!flags.backlightOn) {
    lcd.backlight();
    flags.backlightOn = 1;
  }
  
  // ========== РЕЖИМ МЕНЮ ==========
  // Обновляем таймер активности меню
  menuIdleTimerStart = millis();
    
  if (flags.inMenu) {
    // Меню теперь обрабатывается блокирующим `runMenu()` (data-driven).
    // Нажатие кнопки в режиме меню просто обновляет таймер неактивности.
    menuIdleTimerStart = millis();
    return;
  }
  
  // ========== РЕЖИМ ИНФОРМАЦИОННОГО ЭКРАНА (inFilterWashInfo) ==========
  // Выход из инфо режима обрабатывается в displayFilterWashInfo()
  if (flags.inFilterWashInfo) {
    return;
  }
  
  // ========== ОБЫЧНЫЙ РЕЖИМ (как в оригинале) ==========
  if (wasBacklightOn && wasDisplayOn) {
    flags.inFilterWashInfo = 1;
    flags.filterWashDisplayInitialized = 0;
    flags.displayInitialized = 0;
    filterWashScrollOffset = 0;
  } else {
    flags.displayInitialized = 0;
  }
}

// ============ ПОЛУЧЕНИЕ СОСТОЯНИЯ ЭНКОДЕРА ============

/// Получает текущую позицию энкодера
int getEncoderPosition() {
  extern volatile int encoderPos;
  return encoderPos;
}

/// Проверяет, была ли нажата кнопка (читает и очищает флаг)
bool wasEncoderButtonPressed() {
  extern volatile bool encoderButtonPressed;
  bool was = encoderButtonPressed;
  encoderButtonPressed = false;
  return was;
}

/// Сбрасывает позицию энкодера
void resetEncoderPosition() {
  extern volatile int encoderPos;
  encoderPos = 0;
}

/// Сбрасывает флаг кнопки
void resetButtonFlag() {
  extern volatile bool encoderButtonPressed;
  encoderButtonPressed = false;
}

// ============ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ============

/// Проверяет, нажата ли кнопка энкодера (простая проверка)
bool isEncoderButtonPressed() {
  return digitalRead(pinSW) == LOW;
}

/// Ждет отпускания кнопки
void waitForButtonRelease() {
  while (digitalRead(pinSW) == LOW) {
    WDT_RESET();
  }
}

/// Проверяет меню таймаут и выходит из меню если нужно
void checkMenuIdleTimeout() {
  extern SystemFlags flags;
  extern unsigned long menuIdleTimerStart;
  
  if (flags.inMenu) {
    if (hasIntervalElapsed(menuIdleTimerStart, MAX_MENU_TIME)) {
      flags.inMenu = false;
      forceRedisplay();
    }
  }
}
/// Проверяет энкодер для входа в меню (из обычного режима)
void checkEncoderForMenu() {
  // Реальная работа с меню происходит в sketch через методы LiquidCrystal_I2C_Menu
  // Эта функция вызывается из loop(), но меню обрабатывается через checkEncoderState()
}

// testEncoderDebug removed — debug helper eliminated to reduce noisy diagnostics
