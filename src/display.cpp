/**
 * @file display.cpp
 * @brief Реализация управления дисплеем и меню LCD
 * @details Функции вывода на LCD дисплей (20x4) и обработки меню
 */

#include "display.h"
#include "config.h"
#include "structures.h"
#include "menu.h"
#include "encoder.h"

// ============ FORWARD DECLARATIONS ============

static void displayStaticHeaders();
static void displayFilterWashScreen1();
static void displayFilterWashScreen2();
static void displayFilterWashScreen3();
static void displayFilterWashScreen4();
void drawProgressBar(int x, int y, int width, int percent);
extern void checkEncoderForMenu();
extern void restoreSystemStateAfterMenu();
extern sMenuItem mainMenu[];
extern const int mainMenuLength;

// Флаг для сброса инициализации экранов при смене страницы
static bool resetScreenInit = false;

// ============ ИНИЦИАЛИЗАЦИЯ ДИСПЛЕЯ ============

/// Флаг, что дисплей нужно перерисовать
static bool displayNeedsUpdate = true;

void initDisplay() {
  // Дисплей уже инициализирован в setup()

}

/// Принудительно перерисовать дисплей
void forceRedisplay() {
  displayNeedsUpdate = true;
}

// ============ ВЫВОД НА ДИСПЛЕЙ ============

/// Обновляет дисплей (основной экран состояния)
void updateDisplay() {
  extern SystemFlags flags;
  // migrated to systemContext.currentState
  
  if (!flags.displayOn) return;
  if (flags.inMenu || flags.inFilterWashInfo || flags.emergencyMode) return;
  
  displayDynamicData();
}

/// Печать двух цифр с ведущим нулем
static void printTwoDigits(int number) {
  extern SafeLCD lcd;
  if (number < 10) lcd.print('0');
  lcd.print(number);
}

/// Печать трех символов (с выравниванием по правому краю)
static void printThreeDigits(int number) {
  extern SafeLCD lcd;
  if (number < 100) lcd.print(' ');
  if (number < 10) lcd.print('0');
  lcd.print(number);
}

/// Преобразование секунд в часы, минуты, секунды
static void secondsToHMS(uint32_t seconds, uint8_t& hours, uint8_t& minutes, uint8_t& secs) {
  hours = seconds / 3600;
  minutes = (seconds % 3600) / 60;
  secs = seconds % 60;
}

/// Статические заголовки основного экрана (1-я и 3-я строки)
static void displayStaticHeaders() {
  extern SafeLCD lcd;
  extern SystemFlags flags;
  
  if (!flags.displayInitialized) lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aeration  Ozon   Lv1");
  lcd.setCursor(0, 2);
  lcd.print("Setling  PNAOFC  Lv2");
  flags.displayInitialized = 1;
}

/// Динамическое обновление главного экрана
void displayDynamicData() {
  extern SafeLCD lcd;
  extern SystemFlags flags;
  // FSM state migrated to systemContext.currentState
  extern uint32_t currentAerationRemaining;
  extern uint32_t currentOzonationRemaining;
  extern uint32_t currentSetlingRemaining;
  extern int tank1Level, tank2Level;
  extern TankParams tank1, tank2;
  extern bool getRelayState(uint8_t relay);
  
  // Если дисплей заблокирован (меню или ручное управление) - не обновляем
  if (flags.displayLocked) {
    return;
  }
  
  // Обновляем мигающий флаг раз в секунду (как в оригинале)
  static unsigned long lastBlinkTime = 0;
  if (millis() - lastBlinkTime >= 1000) {
    flags.blinkState = !flags.blinkState;
    lastBlinkTime = millis();
  }
  
  static SystemState lastState = STATE_IDLE;
  static bool needClearScreen = false;

  if (lastState == STATE_BACKWASH && systemContext.currentState != STATE_BACKWASH)
    needClearScreen = true;
  lastState = systemContext.currentState;

  if (needClearScreen && flags.displayOn) {
    lcd.clear();
    flags.displayInitialized = 0;
    flags.backwashScreenInitialized = 0;
    needClearScreen = false;
    if (flags.displayOn) displayStaticHeaders();
  }

  if (systemContext.currentState == STATE_BACKWASH) {
    displayBackwashScreen();
    return;
  }

  if (!flags.displayInitialized && flags.displayOn) {
    displayStaticHeaders();
  }

  lcd.setCursor(0, 0);
  lcd.print("Aeration  Ozon   Lv1");

  lcd.setCursor(0, 1);
  if (systemContext.currentState == STATE_AERATION) {
    uint8_t h, m, s;
    secondsToHMS(currentAerationRemaining, h, m, s);
    printTwoDigits(h);
    lcd.print(':');
    printTwoDigits(m);
    lcd.print(':');
    printTwoDigits(s);
  } else
    lcd.print("  STOP  ");

  lcd.setCursor(9, 1);
  if (systemContext.currentState == STATE_OZONATION) {
    uint8_t h, m, s;
    secondsToHMS(currentOzonationRemaining, h, m, s);
    printTwoDigits(m);
    lcd.print(':');
    printTwoDigits(s);
  } else
    lcd.print(" STOP   ");

  // Уровень бака 1 с миганием если пуст (как в оригинале)
  lcd.setCursor(17, 1);
  if (flags.tank1Empty) {
    if (flags.blinkState)
      printThreeDigits(tank1Level);
    else
      lcd.print("   ");
  } else
    printThreeDigits(tank1Level);

  lcd.setCursor(0, 2);
  if (flags.filterCleaningNeeded && !flags.backwashInProgress && flags.blinkState) {
    lcd.print("Setling          Lv2");
  } else
    lcd.print("Setling  PNAOFC  Lv2");

  lcd.setCursor(0, 3);
  if (systemContext.currentState == STATE_SETTLING) {
    uint8_t h, m, s;
    secondsToHMS(currentSetlingRemaining, h, m, s);
    printTwoDigits(h);
    lcd.print(':');
    printTwoDigits(m);
    lcd.print(':');
    printTwoDigits(s);
  } else
    lcd.print("  STOP  ");

  lcd.setCursor(9, 3);
  for (int i = 0; i < 6; i++)
    lcd.print(getRelayState(i) ? '1' : '0');

  // Уровень бака 2 с миганием если пуст (как в оригинале)
  lcd.setCursor(17, 3);
  if (flags.tank2Empty) {
    if (flags.blinkState)
      printThreeDigits(tank2Level);
    else
      lcd.print("   ");
  } else
    printThreeDigits(tank2Level);
}

/// Отображение меню с использованием энкодера
int showMenuESP32(const void* menu, int menuLength, int startIndex) {
  if (menu == nullptr || menuLength <= 0) return -1;
  const sMenuItem* items = static_cast<const sMenuItem*>(menu);
  return showMenu(items, menuLength, startIndex);
}

/// Выводит экран ввода значения через энкодер
int inputValESP32(const char* title, int minVal, int maxVal, int initialVal) {
  return inputVal(title, minVal, maxVal, initialVal);
}

/// Вход в режим меню
void enterMenu() {
  extern SystemFlags flags;
  flags.inMenu = true;
  runMenu(mainMenu, mainMenuLength, mkRoot);
  flags.inMenu = false;
  forceRedisplay();
}

/// Выход из режима меню
void exitMenu() {
  extern SystemFlags flags;
  flags.inMenu = false;
  forceRedisplay();
}

/// Обработка меню (обёртка)
void handleMenu() {
  enterMenu();
}

/// Отображение меню (обёртка)
void displayMenu() {
  runMenu(mainMenu, mainMenuLength, mkRoot);
}

/// Обновление отображения меню (обёртка)
void updateMenuDisplay() {
  // Перерисовка выполняется внутри showMenu()
}

/// Обработка выбора в меню (обёртка)
void handleMenuSelection() {
  // Логика выбора реализована в showMenu()
}

/// Вход в режим меню с сохранением состояния
void enterMenuMode() {
  extern SystemFlags flags;
  flags.inMenu = true;
}

/// Выход из режима меню с восстановлением состояния
void exitMenuMode() {
  extern SystemFlags flags;
  flags.inMenu = false;
  restoreSystemStateAfterMenu();
  forceRedisplay();
}

/// Безопасное выполнение операции в меню
void safeMenuOperation() {
  enterMenuMode();
  runMenu(mainMenu, mainMenuLength, mkRoot);
  exitMenuMode();
}

/// Выводит информацию о промывке фильтра
void displayFilterWashInfo() {
  extern SafeLCD lcd;
  extern SystemFlags flags;
  extern int filterWashScrollOffset;
  extern unsigned long lastEncoderButtonTime;
  
  static unsigned long lastUpdate = 0;
  static int lastScrollOffset = -1;  // Для отслеживания изменений

  if (!flags.filterWashDisplayInitialized) {
    lcd.clear();
    flags.filterWashDisplayInitialized = 1;
    filterWashScrollOffset = 0;
    lastScrollOffset = -1;  // Форсируем первую отрисовку
    lastUpdate = millis();
    lastEncoderButtonTime = millis();
  }

  unsigned long currentTime = millis();

  // Энкодер обрабатывается в onEncoderRight/onEncoderLeft через прерывания
  // Здесь только отслеживаем изменения filterWashScrollOffset

  // Нажатие кнопки — выход из инфо режима (дебаунс 200мс)
  if (isEncoderButtonPressed() && (currentTime - lastEncoderButtonTime) > 200) {
    flags.inFilterWashInfo = 0;
    flags.filterWashDisplayInitialized = 0;
    lastEncoderButtonTime = currentTime;
    lcd.clear();
    flags.displayInitialized = 0;
    return;
  }
  
  // Мгновенная перерисовка при смене экрана ИЛИ обновление раз в секунду
  bool pageChanged = (filterWashScrollOffset != lastScrollOffset);
  bool needRedraw = pageChanged || (millis() - lastUpdate >= 1000);
  
  if (needRedraw) {
    // Сбрасываем флаги инициализации при смене страницы
    if (pageChanged) {
      lcd.clear();
      resetScreenInit = true;
    }
    
    lastUpdate = millis();
    lastScrollOffset = filterWashScrollOffset;
    
    switch (filterWashScrollOffset) {
      case 0: displayFilterWashScreen1(); break;
      case 1: displayFilterWashScreen2(); break;
      case 2: displayFilterWashScreen3(); break;
      case 3: displayFilterWashScreen4(); break;
    }
    
    resetScreenInit = false;
    
    lcd.setCursor(17, 3);
    lcd.print(filterWashScrollOffset + 1);
    lcd.print("/4");
  }
}

/// Информационный экран 1: Дата/время и статус фильтра
static void displayFilterWashScreen1() {
  extern SafeLCD lcd;
  extern RTC_DS3231 rtc;
  extern SystemFlags flags;
  extern unsigned long filterOperationStartTime;
  extern uint32_t filterCleaningInterval;
  // FSM state migrated to systemContext.currentState
  extern uint32_t currentBackwashRemaining;
  extern char emergencyMessage[41];
  
  lcd.setCursor(0, 0);
  if (flags.rtcInitialized) {
    DateTime now = rtc.now();
    printTwoDigits(now.day());
    lcd.print("/");
    printTwoDigits(now.month());
    lcd.print("/");
    lcd.print(now.year());
    lcd.print(" ");
    printTwoDigits(now.hour());
    lcd.print(":");
    printTwoDigits(now.minute());
    lcd.print(":");
    printTwoDigits(now.second());
  } else
    lcd.print("RTC Not Set      ");

  lcd.setCursor(0, 1);
  lcd.print("F.Wash:  ");
  if (filterCleaningInterval == 0)
    lcd.print("Not set ");
  else if (flags.filterCleaningNeeded)
    lcd.print("NOW!     ");
  else if (filterOperationStartTime == 0)
    lcd.print("0:00:00:00");
  else {
    unsigned long cleaningPeriodMs = filterCleaningInterval * 1000UL;
    unsigned long elapsed = millis() - filterOperationStartTime;
    if (elapsed > cleaningPeriodMs) elapsed = cleaningPeriodMs;
    unsigned long remaining = (cleaningPeriodMs > elapsed) ? (cleaningPeriodMs - elapsed) : 0;

    unsigned long totalSeconds = remaining / 1000;
    unsigned long days = totalSeconds / (24UL * 3600UL);
    totalSeconds %= (24UL * 3600UL);
    unsigned long hours = totalSeconds / 3600UL;
    totalSeconds %= 3600UL;
    unsigned long minutes = totalSeconds / 60UL;
    unsigned long seconds = totalSeconds % 60UL;

    if (days > 99)
      lcd.print(">99d    ");
    else {
      if (days < 10) lcd.print("0");
      lcd.print(days);
      lcd.print("d ");
      if (hours < 10) lcd.print("0");
      lcd.print(hours);
      lcd.print("h ");
      if (minutes < 10) lcd.print("0");
      lcd.print(minutes);
      lcd.print("m");
    }
  }

  lcd.setCursor(0, 2);
  if (systemContext.currentState == STATE_BACKWASH) {
    lcd.print("Backwash: ");
    uint8_t h, m, s;
    secondsToHMS(currentBackwashRemaining, h, m, s);
    printTwoDigits(m);
    lcd.print("m ");
    printTwoDigits(s);
    lcd.print("s    ");
  } else if (flags.backwashInProgress) {
    lcd.print("Wash in progress  ");
  } else {
    lcd.print("System: ");
    switch (systemContext.currentState) {
      case STATE_IDLE: lcd.print("IDLE        "); break;
      case STATE_FILLING_TANK1: lcd.print("FILLING T1  "); break;
      case STATE_OZONATION: lcd.print("OZONATION   "); break;
      case STATE_AERATION: lcd.print("AERATION    "); break;
      case STATE_SETTLING: lcd.print("SETTLING    "); break;
      case STATE_FILTRATION: lcd.print("FILTRATION  "); break;
      default: lcd.print("UNKNOWN     "); break;
    }
  }

  lcd.setCursor(0, 3);
  lcd.print("Rotate:Scroll   ");
}

/// Информационный экран 2: Статус расписания и ночного режима
static void displayFilterWashScreen2() {
  extern SafeLCD lcd;
  extern SystemFlags flags;
  extern byte scheduleStart[3];
  extern byte scheduleEnd[3];
  extern byte nightModeStart[2];
  extern byte nightModeEnd[2];
  extern bool isWithinSchedule();
  extern bool isNightMode();
  
  // Статические переменные для отслеживания изменений
  static bool lastScheduleEnabled = false;
  static bool lastNightEnabled = false;
  static bool initialized = false;
  
  // Сброс при смене страницы
  if (resetScreenInit) {
    initialized = false;
  }
  
  // Полная перерисовка при первом входе
  if (!initialized) {
    // Строка 0: Schedule ON/OFF
    lcd.setCursor(0, 0);
    lcd.print("Schedule: ");
    lcd.print(flags.scheduleEnabled ? "ON        " : "OFF       ");

    // Строка 1: Time:HH:MM-HH:MM
    lcd.setCursor(0, 1);
    lcd.print("Time:");
    printTwoDigits(scheduleStart[0]);
    lcd.print(":");
    printTwoDigits(scheduleStart[1]);
    lcd.print("-");
    printTwoDigits(scheduleEnd[0]);
    lcd.print(":");
    printTwoDigits(scheduleEnd[1]);
    lcd.print("      ");

    // Строка 2: Night ON/OFF
    lcd.setCursor(0, 2);
    lcd.print("Night: ");
    lcd.print(flags.nightModeEnabled ? "ON           " : "OFF          ");
    
    // Строка 3: Time:HH:MM-HH:MM
    lcd.setCursor(0, 3);
    lcd.print("Time:");
    printTwoDigits(nightModeStart[0]);
    lcd.print(":");
    printTwoDigits(nightModeStart[1]);
    lcd.print("-");
    printTwoDigits(nightModeEnd[0]);
    lcd.print(":");
    printTwoDigits(nightModeEnd[1]);
    
    lastScheduleEnabled = flags.scheduleEnabled;
    lastNightEnabled = flags.nightModeEnabled;
    initialized = true;
    return;
  }
  
  // Обновляем только если статус изменился
  if (flags.scheduleEnabled != lastScheduleEnabled) {
    lcd.setCursor(10, 0);
    lcd.print(flags.scheduleEnabled ? "ON " : "OFF");
    lastScheduleEnabled = flags.scheduleEnabled;
  }
  
  if (flags.nightModeEnabled != lastNightEnabled) {
    lcd.setCursor(7, 2);
    lcd.print(flags.nightModeEnabled ? "ON " : "OFF");
    lastNightEnabled = flags.nightModeEnabled;
  }
}

// Сброс флага инициализации экрана 2 при смене страницы
static void resetScreen2Init() {
  // Вызывается при смене страницы
}

/// Информационный экран 3: Уровни баков
static void displayFilterWashScreen3() {
  extern SafeLCD lcd;
  extern SystemFlags flags;
  extern int tank1Level, tank2Level;
  extern int tank1RawDistance, tank2RawDistance;
  extern bool getRelayState(uint8_t relay);
  
  lcd.setCursor(0, 0);
  lcd.print("=== LEVELS ===      ");

  lcd.setCursor(0, 1);
  lcd.print("Tank1: ");
  printThreeDigits(tank1RawDistance);
  lcd.print("cm ");
  if (flags.tank1Empty)
    lcd.print("EMPTY ");
  else
    lcd.print("OK    ");

  lcd.setCursor(0, 2);
  lcd.print("Tank2: ");
  printThreeDigits(tank2RawDistance);
  lcd.print("cm ");
  if (flags.tank2Empty)
    lcd.print("EMPTY ");
  else
    lcd.print("OK    ");

  lcd.setCursor(0, 3);
  lcd.print("Relay: ");
  for (int i = 0; i < 6; i++)
    lcd.print(getRelayState(i) ? '1' : '0');
  lcd.print("       ");
}

/// Информационный экран 4: Статистика
static void displayFilterWashScreen4() {
  extern SafeLCD lcd;
  
  lcd.setCursor(0, 0);
  lcd.print("=== STATS ===       ");

  lcd.setCursor(0, 1);
  lcd.print("Uptime: ");
  unsigned long uptime = millis() / 1000;
  unsigned long hours = uptime / 3600;
  unsigned long minutes = (uptime % 3600) / 60;
  lcd.print(hours);
  lcd.print("h ");
  lcd.print(minutes);
  lcd.print("m         ");

  lcd.setCursor(0, 2);
  lcd.print("System OK           ");

  lcd.setCursor(0, 3);
  lcd.print("                    ");
}

/// Выход из экрана обратной промывки
void exitBackwashScreen() {
  extern SystemFlags flags;
  flags.backwashScreenInitialized = 0;
  forceRedisplay();
}

/// Специальный экран для режима обратной промывки
void displayBackwashScreen() {
  extern SafeLCD lcd;
  extern SystemFlags flags;
  extern uint32_t currentBackwashRemaining;
  extern int tank1Level;
  
  static unsigned long lastUpdate = 0;

  if (!flags.backwashScreenInitialized) {
    lcd.clear();
    flags.backwashScreenInitialized = 1;
    lastUpdate = millis();
  }

  if (millis() - lastUpdate >= 500) {
    lastUpdate = millis();
    
    // Обновляем blinkState каждые 500 мс (как в оригинале)
    flags.blinkState = !flags.blinkState;
    
    lcd.setCursor(0, 0);
    lcd.print("== BACKWASH MODE ==");

    lcd.setCursor(0, 1);
    lcd.print("Time remaining:     ");

    lcd.setCursor(0, 2);
    lcd.print("   ");
    lcd.setCursor(0, 2);
    uint8_t h, m, s;
    secondsToHMS(currentBackwashRemaining, h, m, s);
    if (m < 10) lcd.print("0");
    lcd.print(m);
    lcd.print(":");
    if (s < 10) lcd.print("0");
    lcd.print(s);
    lcd.print("  (mm:ss)       ");

    // Нижняя строка: уровень бака 1 + подсказка «нажмите для отмены» (мигает)
    lcd.setCursor(0, 3);
    if (flags.blinkState) {
      lcd.print("Tank1:");
      printTwoDigits(tank1Level);
      lcd.print("Press:Cancel");
    } else
      lcd.print("                    ");
  }
}

/// Отображает экран аварийного режима
void displayEmergencyScreen() {
  extern SafeLCD lcd;
  extern SystemFlags flags;
  extern char emergencyMessage[41];
  extern int tank1Level;
  
  static unsigned long lastEmergencyUpdate = 0;

  // Use the shared flag so exitEmergencyMode() can force reinitialization
  extern SystemFlags flags;
  if (!flags.emergencyScreenInitialized) {
    lcd.clear();
    flags.emergencyScreenInitialized = 1;
    lastEmergencyUpdate = millis();
  }

  if (millis() - lastEmergencyUpdate >= 500) {
    lastEmergencyUpdate = millis();

    lcd.setCursor(0, 0);
    lcd.print("EMERGENCY STOP!     ");

    lcd.setCursor(0, 1);
    char emBuf[21];
    strncpy(emBuf, emergencyMessage, 20);
    emBuf[20] = '\0';
    lcd.print(emBuf);

    lcd.setCursor(0, 2);
    lcd.print("Tank1: ");
    printThreeDigits(tank1Level);
    lcd.print(" cm         ");

    lcd.setCursor(0, 3);
    if (flags.blinkState)
      lcd.print("Hold 3s to reset    ");
    else
      lcd.print("                    ");
  }
}

/// Выводит экран выбора пункта меню
void showMenuItemSelect(const char* title) {
  extern SafeLCD lcd;
  lcd.clear();
  lcd.setCursor(0, 0);
  char tbuf[21];
  strncpy(tbuf, title, 20);
  tbuf[20] = '\0';
  lcd.print(tbuf);
  lcd.setCursor(0, 2);
  lcd.print("Select item");
}

// ============ ПОСТРОЕНИЕ МЕНЮ ============

/// Получает выбранный пункт меню
int getSelectedMenuItem() {
  return getSelectedMenuItemIndex();
}

/// Построение главного меню
void buildMainMenu() {
  extern SystemFlags flags;
  flags.inMenu = true;
  runMenu(mainMenu, mainMenuLength, mkRoot);
  flags.inMenu = false;
}

/// Построение меню настроек
void buildSettingsMenu() {
  extern SystemFlags flags;
  flags.inMenu = true;
  showMenuByParent(mainMenu, mainMenuLength, mksettingUpTimers);
  flags.inMenu = false;
}

/// Построение меню статистики
void buildStatsMenu() {
  extern SystemFlags flags;
  flags.inMenu = true;
  showMenuByParent(mainMenu, mainMenuLength, mkSystemLogging);
  flags.inMenu = false;
}

/// Построение меню информации о системе
void buildInfoMenu() {
  extern SafeLCD lcd;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Info");
  lcd.setCursor(0, 1);
  lcd.print("ESP32 Water v12");
  lcd.setCursor(0, 2);
  lcd.print("Menu via encoder");
  lcd.setCursor(0, 3);
  lcd.print("Rotate to enter");
}

// ============ УПРАВЛЕНИЕ МЕНЮ ============

/// Показывает меню с прокруткой
void showMenu(int scrollOffset) {
  extern SafeLCD lcd;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Menu scroll: ");
  lcd.print(scrollOffset);
}

/// Обновляет меню при изменении энкодера
void updateMenuWithEncoder(int encoderDelta) {
  extern SafeLCD lcd;
  lcd.setCursor(0, 3);
  lcd.print("Delta: ");
  lcd.print(encoderDelta);
  lcd.print("   ");
}

/// Обновляет счетчики времени и шкал
void updateTimerDisplay(uint32_t remaining, uint32_t total) {
  extern SafeLCD lcd;
  uint8_t h, m, s;
  secondsToHMS(remaining, h, m, s);
  lcd.setCursor(0, 0);
  lcd.print("Time left: ");
  printTwoDigits(h);
  lcd.print(':');
  printTwoDigits(m);
  lcd.print(':');
  printTwoDigits(s);
  
  int percent = (total > 0) ? (int)((100UL * (total - remaining)) / total) : 0;
  drawProgressBar(0, 1, 20, percent);
}

/// Выводит информацию о баках
void displayTankInfo(int level1, int level2, bool empty1, bool empty2) {
  extern SafeLCD lcd;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tank1: ");
  if (empty1) lcd.print("EMPTY ");
  else lcd.print(level1);
  lcd.print("cm");
  
  lcd.setCursor(0, 1);
  lcd.print("Tank2: ");
  if (empty2) lcd.print("EMPTY ");
  else lcd.print(level2);
  lcd.print("cm");
}

/// Выводит информацию о текущем состоянии
void displayStateInfo(int state) {
  extern SafeLCD lcd;
  const char* text = "UNKNOWN";
  switch (state) {
    case STATE_IDLE: text = "IDLE"; break;
    case STATE_FILLING_TANK1: text = "FILLING"; break;
    case STATE_OZONATION: text = "OZONATION"; break;
    case STATE_AERATION: text = "AERATION"; break;
    case STATE_SETTLING: text = "SETTLING"; break;
    case STATE_FILTRATION: text = "FILTRATION"; break;
    case STATE_BACKWASH: text = "BACKWASH"; break;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("State:");
  lcd.setCursor(0, 1);
  lcd.print(text);
}

// ============ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ============

/// Очищает строку дисплея
void clearLcdLine(int line) {
  extern SafeLCD lcd;
  if (line < 0 || line > 3) return;
  lcd.setCursor(0, line);
  lcd.print("                    ");
}

/// Выводит строку на дисплей с выравниванием
void printLcdCentered(int line, const char* text) {
  extern SafeLCD lcd;
  if (line < 0 || line > 3 || text == nullptr) return;
  int len = strlen(text);
  if (len > 20) len = 20;
  int start = (20 - len) / 2;
  clearLcdLine(line);
  lcd.setCursor(start, line);
  for (int i = 0; i < len; i++) lcd.print(text[i]);
}

/// Выводит полосу прогресса
void drawProgressBar(int x, int y, int width, int percent) {
  extern SafeLCD lcd;
  if (width <= 0) return;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  int filled = (width * percent) / 100;
  lcd.setCursor(x, y);
  for (int i = 0; i < width; i++) {
    lcd.print(i < filled ? '#' : '-');
  }
}

/// Показывает сообщение об ошибке
void showErrorMessage(const char* message) {
  extern SafeLCD lcd;
  lcd.clear();
  printLcdCentered(1, "ERROR");
  printLcdCentered(2, message ? message : "Unknown");
  delay(1500);
}

/// Показывает сообщение подтверждения
void showConfirmation(const char* message) {
  extern SafeLCD lcd;
  lcd.clear();
  printLcdCentered(1, "OK");
  printLcdCentered(2, message ? message : "Done");
  delay(1000);
}
