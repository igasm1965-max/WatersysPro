/**
 * @file menu.cpp
 * @brief Реализация системы меню для ESP32
 * @details Навигация, ввод значений, обработчики меню
 */

#include "menu.h"
#include "config.h"
#include "structures.h"
#include "display.h"
#include "encoder.h"
#include "prefs.h"
#include "event_logging.h"
#include "timers.h"
#include "relay_control.h"
#include "sensors.h"
#include "utils.h"
#include "engineer_menu.h"
#include "emergency.h"

// Флаг для перехода в инженерное подменю после ввода пароля
extern bool engineerMenuRequested;

// ============ ВНЕШНИЕ ПЕРЕМЕННЫЕ ============

extern SafeLCD lcd;
extern RTC_DS3231 rtc;
extern SystemFlags flags;
extern volatile int encoderPos;
extern volatile bool encoderButtonPressed;

// ============ FORWARD DECLARATIONS ============


/// Обработчик статистики логов
void EventStatsHandler() {
  displayEventStats();
}


/// Обработчик графика логов
void EventGraphHandler() {
  displayEventGraph();
}


/// Обработчик фильтрации логов
void EventFilterHandler() {
  showFilterTypeMenu();
}
// Функции из preferences.cpp
void saveTimerSetting(uint8_t timerType, uint8_t hours, uint8_t minutes, uint8_t seconds);
void saveTankSettings(uint8_t tankNumber, int minLevel, int maxLevel);
void saveScheduleSettings();
void saveFilterCleaningPeriod(uint32_t days);
void resetAllSettings();

// ============ ВНЕШНИЕ ПЕРЕМЕННЫЕ ============

extern SafeLCD lcd;
extern RTC_DS3231 rtc;
extern SystemFlags flags;
extern volatile int encoderPos;
extern volatile bool encoderButtonPressed;

// Таймеры
extern uint32_t ozonationDuration;
extern uint32_t aerationDuration;
extern uint32_t setlingDuration;
extern uint32_t filterWashDuration;
extern uint32_t backlightDuration;
extern uint32_t filterCleaningInterval;

// Уровни баков
extern TankSettings tank1;
extern TankSettings tank2;

// Расписание
extern byte scheduleStart[3];
extern byte scheduleEnd[3];
extern byte nightModeStart[2];
extern byte nightModeEnd[2];

// ============ ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ ============

static int currentMenuItemIndex = 0;
static int selectedMenuItemIndex = -1;

// Локальные помощники
static int findMenuItemIndexById(const sMenuItem* menu, int menuLength, int id) {
  for (int i = 0; i < menuLength; i++) {
    if (menu[i].id == id) return i;
  }
  return -1;
}

// Вспомогательная функция для отображения пунктов меню
static void displayMenuItems(const sMenuItem* menu, int* visibleItems, int currentSelection, int scrollOffset, int visibleCount) {
  for (int i = 0; i < 4; i++) {
    lcd.setCursor(0, i);
    
    if ((scrollOffset + i) < visibleCount) {
      int itemIndex = visibleItems[scrollOffset + i];
      
      if (scrollOffset + i == currentSelection) {
        lcd.print(">");
      } else {
        lcd.print(" ");
      }
      
      // Печатаем заголовок (макс 19 символов)
      int captionLen = strlen(menu[itemIndex].caption);
      if (captionLen > 19) captionLen = 19;
      for (int c = 0; c < captionLen; c++) {
        lcd.print(menu[itemIndex].caption[c]);
      }
      
      // Очистка остатка строки пробелами
      for (int j = captionLen + 1; j < 20; j++) {
        lcd.print(" ");
      }
    } else {
      // Пустая строка - полностью очищаем
      lcd.print("                    ");
    }
  }
}

// Вспомогательная функция для отображения сообщения о сохранении
static void showSavedMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Saved!");
  delay(1000);
}

// Показать короткое состояние включено/выключено
static void showToggleStatus(const char* name, bool enabled) {
  extern SafeLCD lcd;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(name);
  lcd.print(": ");
  lcd.print(enabled ? "ON" : "OFF");
  delay(1000);
}

// ============ НАВИГАЦИЯ ПО МЕНЮ ============

/// Показывает меню и возвращает выбранный индекс
int showMenu(const sMenuItem* menu, int menuLength, int startIndex) {
  if (menu == nullptr || menuLength <= 0) return -1;
  
  currentMenuItemIndex = startIndex;
  int scrollOffset = 0;
  bool menuActive = true;
  unsigned long lastUpdate = millis();
  unsigned long menuStartTime = millis();
  
  // Найти корневой элемент
  int rootId = menu[startIndex].parentId;
  int visibleItems[20];
  int visibleCount = findMenuItems(menu, menuLength, rootId, visibleItems, 20);
  
  if (visibleCount == 0) return -1;
  
  int currentSelection = 0;
  encoderPos = 0;
  encoderButtonPressed = false;
  bool firstIteration = true;
  
  lcd.clear();
  Serial.printf("[MENU] showMenu started, visibleCount=%d, rootId=%d\n", visibleCount, rootId);
  for (int vi = 0; vi < visibleCount; vi++) {
    int idx = visibleItems[vi];
    Serial.printf("[MENU] visible[%d]=id=%d caption='%s'\n", vi, menu[idx].id, menu[idx].caption);
  }
  
  while (menuActive) {
    WDT_RESET();
    // Таймаут меню
    if (millis() - menuStartTime > MAX_MENU_TIME) {
      Serial.println("[MENU] Timeout!");
      return -1;
    }
    
    // Сбросить накопленные повороты энкодера в первой итерации
    if (firstIteration) {
      readEncoderDelta();
      firstIteration = false;
    }
    
    // Чтение энкодера
    int delta = readEncoderDelta();
    static unsigned long lastDeltaTime = 0;
    if (delta != 0) {
      unsigned long now = millis();

      // Игнорируем повторные дельты, приходящие быстрее указанного интервала
      if (now - lastDeltaTime < 40) {
        Serial.printf("[MENU] Ignored rapid delta=%d (dt=%lums)\n", delta, now - lastDeltaTime);
        // Потеряем эту дельту (она уже накопилась в pos и будет прочитана позже)
        lastDeltaTime = now;
        continue;
      }

      // Ограничиваем дельту до одного шага, чтобы избежать пропусков при бурном вращении
      int origDelta = delta;
      if (delta > 1) delta = 1;
      if (delta < -1) delta = -1;
      if (delta != origDelta) {
        Serial.printf("[MENU] Clamped delta %d -> %d\n", origDelta, delta);
      }

      Serial.printf("[MENU] Encoder delta=%d, selection %d->", delta, currentSelection);
      currentSelection += delta;
      if (currentSelection < 0) currentSelection = 0;
      if (currentSelection >= visibleCount) currentSelection = visibleCount - 1;
      Serial.printf("%d\n", currentSelection);

      lastDeltaTime = now;      

      // Прокрутка экрана
      if (currentSelection < scrollOffset) {
        scrollOffset = currentSelection;
      }
      if (currentSelection >= scrollOffset + 4) {
        scrollOffset = currentSelection - 3;
      }
      
      lastUpdate = 0; // Форсировать перерисовку
    }
    
    // Проверка нажатия кнопки (используем ISR-флаг для надежности)
    if (encoderButtonPressed) {
      encoderButtonPressed = false;
      waitForEncoderRelease();
      int itemIndex = visibleItems[currentSelection];
      selectedMenuItemIndex = menu[itemIndex].id;
      Serial.printf("[MENU] Button pressed! Selected item id=%d\n", selectedMenuItemIndex);
      // сброс таймаута
      menuStartTime = millis();
      return selectedMenuItemIndex;
    }
    
    // Обновление экрана
    if (millis() - lastUpdate > 100) {
      displayMenuPage(menu, menuLength, currentSelection, scrollOffset);
      
      // Отображаем видимые пункты
      displayMenuItems(menu, visibleItems, currentSelection, scrollOffset, visibleCount);
      
      lastUpdate = millis();
    }
    
    delay(20);
  }
  
  return -1;
}

/// Показывает меню для заданного parentId
int showMenuByParent(const sMenuItem* menu, int menuLength, int parentId) {
  int visibleItems[20];
  int visibleCount = findMenuItems(menu, menuLength, parentId, visibleItems, 20);
  if (visibleCount <= 0) return -1;
  return showMenu(menu, menuLength, visibleItems[0]);
}

/// Отображает страницу меню
void displayMenuPage(const sMenuItem* menu, int menuLength, int currentIndex, int scrollOffset) {
  // прежняя версия ничего не рисовала – заголовок находится в первой строке пункта
}

/// Находит все пункты меню с указанным родителем
int findMenuItems(const sMenuItem* menu, int menuLength, int parentId, int* itemIndices, int maxItems) {
  int count = 0;
  
  for (int i = 0; i < menuLength && count < maxItems; i++) {
    if (menu[i].parentId == parentId) {
      itemIndices[count++] = i;
    }
  }
  
  return count;
}

/// Получает индекс выбранного пункта
int getSelectedMenuItemIndex() {
  return selectedMenuItemIndex;
}

/// Запускает многоуровневое меню с обработчиками
void runMenu(const sMenuItem* menu, int menuLength, int rootId) {
  if (menu == nullptr || menuLength <= 0) return;

  int parentStack[10];
  int stackTop = 0;
  int currentParentId = rootId;

  while (true) {
    int visibleItems[20];
    int visibleCount = findMenuItems(menu, menuLength, currentParentId, visibleItems, 20);
    if (visibleCount <= 0) return;

    int selectedId = showMenu(menu, menuLength, visibleItems[0]);
    if (selectedId < 0) return; // таймаут или отмена
    int selIdxTmp = findMenuItemIndexById(menu, menuLength, selectedId);
    if (selIdxTmp >= 0) {
      Serial.printf("[MENU] runMenu selected id=%d caption='%s'\n", selectedId, menu[selIdxTmp].caption);
    }

    if (selectedId == mkEXIT) {
      return;
    }

    if (selectedId == mkBack) {
      if (stackTop > 0) {
        currentParentId = parentStack[--stackTop];
        continue;
      }
      return;
    }

    int selectedIndex = findMenuItemIndexById(menu, menuLength, selectedId);
    if (selectedIndex < 0) {
      continue;
    }

    // Если у пункта есть дочерние элементы, обычно открываем подменю.
    // Для инженерного меню нужна предварительная авторизация через handler().
    int childCount = findMenuItems(menu, menuLength, selectedId, visibleItems, 20);
    if (childCount > 0) {
      // Специальная обработка Engineer Menu: вызов handler() перед входом
      if (selectedId == mkEngineerMenu && menu[selectedIndex].handler != nullptr) {
        menu[selectedIndex].handler();
        // handler() (showPasswordScreen) выставит engineerMenuRequested=true при успешном вводе
        if (engineerMenuRequested) {
          engineerMenuRequested = false;
          if (stackTop < 10) {
            parentStack[stackTop++] = currentParentId;
          }
          currentParentId = selectedId;
        }
        // В любом случае, не выполняем стандартный автоматический вход без авторизации
        continue;
      }

      // Обычное поведение для пунктов с дочерними элементами
      if (stackTop < 10) {
        parentStack[stackTop++] = currentParentId;
      }
      currentParentId = selectedId;
      continue;
    }

    // Если пункта нет дочерних элементов, вызываем handler если он есть
    if (menu[selectedIndex].handler != nullptr) {
      menu[selectedIndex].handler();
      continue;
    }
  }
}

// ============ ВВОД ЗНАЧЕНИЙ ============

/// Ввод целого числа через энкодер
int inputVal(const char* title, int minVal, int maxVal, int initialVal) {
  if (initialVal < minVal) initialVal = minVal;
  if (initialVal > maxVal) initialVal = maxVal;
  
  int currentVal = initialVal;
  encoderPos = 0;
  encoderButtonPressed = false;
  unsigned long lastUpdate = millis();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  char tbuf[21];
  strncpy(tbuf, title, 20);
  tbuf[20] = '\0';
  lcd.print(tbuf);
  
  Serial.printf("[INPUT] inputVal started: min=%d, max=%d, init=%d\n", minVal, maxVal, initialVal);
  
  while (true) {
    WDT_RESET();
    int delta = readEncoderDelta();
    if (delta != 0) {
      Serial.printf("[INPUT] delta=%d, val %d->", delta, currentVal);
      currentVal += delta;
      if (currentVal < minVal) currentVal = minVal;
      if (currentVal > maxVal) currentVal = maxVal;
      Serial.printf("%d\n", currentVal);
      lastUpdate = 0;
    }
    
    // Используем ISR-флаг для надежного обнаружения нажатия
    if (encoderButtonPressed) {
      encoderButtonPressed = false;
      waitForEncoderRelease();
      Serial.printf("[INPUT] Button pressed, returning value=%d\n", currentVal);
      return currentVal;
    }
    
    if (millis() - lastUpdate > 100) {
      lcd.setCursor(0, 1);
      lcd.print("Value: ");
      lcd.print(currentVal);
      lcd.print("    ");
      
      lcd.setCursor(0, 3);
      lcd.print("Press to confirm");
      
      lastUpdate = millis();
    }
    
    delay(20);
  }
}

/// Ввод времени (часы, минуты, секунды)
void inputTime(const char* title, uint8_t& hours, uint8_t& minutes, uint8_t& seconds) {
  hours = (uint8_t)inputVal("Hours", 0, 24, hours);
  minutes = (uint8_t)inputVal("Minutes", 0, 59, minutes);
  seconds = (uint8_t)inputVal("Seconds", 0, 59, seconds);
}

/// Ввод даты
void inputDate(const char* title, uint8_t& day, uint8_t& month, uint16_t& year) {
  day = (uint8_t)inputVal("Day", 1, 31, day);
  month = (uint8_t)inputVal("Month", 1, 12, month);
  year = (uint16_t)inputVal("Year", 2020, 2099, year);
}

/// Подтверждение действия
bool confirmAction(const char* message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
  lcd.setCursor(0, 2);
  lcd.print("Press: YES");
  lcd.setCursor(0, 3);
  lcd.print("Rotate: NO");
  
  encoderPos = 0;
  encoderButtonPressed = false;
  unsigned long startTime = millis();
  
  while (millis() - startTime < 5000) {
    WDT_RESET();
    // Используем ISR-флаг для надежности
    if (encoderButtonPressed) {
      encoderButtonPressed = false;
      waitForEncoderRelease();
      return true;
    }
    
    if (readEncoderDelta() != 0) {
      return false;
    }
    
    delay(20);
  }
  
  return false;
}

// ============ ОБРАБОТЧИКИ МЕНЮ ============

/// Общий обработчик настройки таймеров
void CommonTimerHandler() {
  int item = getSelectedMenuItemIndex();
  if (item < 0) return;
  
  char title[20] = "";
  uint32_t* targetDuration = nullptr;
  bool hasHours = true;
  uint8_t timerType = 0;
  
  // Определяем какой таймер настраивается (по enum из structures.h)
  switch (item) {
    case mkSetlingWaterTimer:
      strncpy(title, "Settling", sizeof(title) - 1);
      title[sizeof(title) - 1] = '\0';
      targetDuration = &setlingDuration;
      hasHours = true;
      timerType = 0;
      break;
      
    case mkAerationTimer:
      strncpy(title, "Aeration", sizeof(title) - 1);
      title[sizeof(title) - 1] = '\0';
      targetDuration = &aerationDuration;
      hasHours = true;
      timerType = 1;
      break;
      
    case mkOzonationTimer:
      strncpy(title, "Ozonation", sizeof(title) - 1);
      title[sizeof(title) - 1] = '\0';
      targetDuration = &ozonationDuration;
      hasHours = false;
      timerType = 2;
      break;
      
    case mkFilterCleaningTimer:
      strncpy(title, "Filter Clean", sizeof(title) - 1);
      title[sizeof(title) - 1] = '\0';
      targetDuration = &filterWashDuration;
      hasHours = false;
      timerType = 3;
      break;
      
    case mkBacklightTimer:
      strncpy(title, "Backlight", sizeof(title) - 1);
      title[sizeof(title) - 1] = '\0';
      targetDuration = &backlightDuration;
      hasHours = false;
      timerType = 4;
      break;
      
    default:
      return;
  }
  
  if (targetDuration != nullptr) {
    uint32_t current = *targetDuration;
    
    // Разбиваем на компоненты
    uint8_t h = current / 3600;
    uint8_t m = (current % 3600) / 60;
    uint8_t s = current % 60;
    
    // Ввод значений
    if (!hasHours) {
      h = 0;
      inputTime(title, h, m, s);
    } else {
      inputTime(title, h, m, s);
    }
    
    // Сохраняем новое значение
    uint32_t newDuration = h * 3600UL + m * 60UL + s;
    
    if (newDuration != *targetDuration) {
      *targetDuration = newDuration;
      saveTimerSetting(timerType, h, m, s);
      // Apply runtime change immediately for backlight timer
      if (timerType == 4) {
        extern unsigned long backlightTimeout;
        backlightTimeout = newDuration * 1000UL;
      }
      
      showSavedMessage();
    }
  }
}

/// Обработчик настройки уровней баков
void TankSettingsHandler() {
  int item = getSelectedMenuItemIndex();
  if (item < 0) return;
  
  TankSettings* tank = nullptr;
  const char* tankName = "";
  
  if (item == mkTank1) {
    tank = &tank1;
    tankName = "Tank 1";
  } else if (item == mkTank2) {
    tank = &tank2;
    tankName = "Tank 2";
  } else {
    return;
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(tankName);
  lcd.print(" Settings");
  delay(1000);
  
  int newMin = inputVal("Min Level (cm)", 0, 200, tank->minLevel);
  int newMax = inputVal("Max Level (cm)", 0, 200, tank->maxLevel);
  
  if (newMin >= newMax) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: Min >= Max");
    delay(2000);
    return;
  }
  
  tank->minLevel = newMin;
  tank->maxLevel = newMax;
  
  saveTankSettings(item == mkTank1 ? 1 : 2, newMin, newMax);
  
  showSavedMessage();
}

/// Обработчик настройки даты и времени
void DateTimeSettingsHandler() {
  int item = getSelectedMenuItemIndex();
  if (item < 0) return;
  
  DateTime now = rtc.now();
  
  switch (item) {
    case mkSetCurrentTime: {
      uint8_t h = now.hour();
      uint8_t m = now.minute();
      uint8_t s = now.second();
      
      inputTime("Set Time", h, m, s);
      
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), h, m, s));
      
      showSavedMessage();
      break;
    }
    
    case mkSetCurrentDate: {
      uint8_t d = now.day();
      uint8_t m = now.month();
      uint16_t y = now.year();
      
      inputDate("Set Date", d, m, y);
      
      rtc.adjust(DateTime(y, m, d, now.hour(), now.minute(), now.second()));
      
      showSavedMessage();
      break;
    }
    
    // Fix: reliable exit from "View Date/Time" — respond to ISR flag, wasEncoderButtonPressed(), pin state and rotation.
    // Reason: short presses or quick rotations could be missed by previous single-condition check; keep behavior consistent with other info screens.
    case mkViewDateTime: {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Current Date/Time:");
      lcd.setCursor(0, 1);
      
      char buf[20];
      snprintf(buf, sizeof(buf), "%02d/%02d/%04d", now.day(), now.month(), now.year());
      lcd.print(buf);
      
      lcd.setCursor(0, 2);
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
      lcd.print(buf);
      
      lcd.setCursor(0, 3);
      lcd.print("Press/Rot to exit");
      
      // Убедиться, что предыдущее нажатие отпущено и ждать нового ввода (кнопка или вращение)
      waitForEncoderRelease();
      unsigned long buttonWaitStart = millis();
      while (true) {
        WDT_RESET();  // Сброс watchdog в цикле ожидания

        // Используем wasEncoderButtonPressed() — она надёжно сообщает о событии ISR даже если
        // кнопка уже отпущена; также реагируем на вращение (readEncoderDelta) и на текущее
        // состояние контакта (isEncoderButtonPressed) для максимальной отзывчивости.
        int delta = readEncoderDelta();
        bool wasBtn = wasEncoderButtonPressed();
        bool pinNow = isEncoderButtonPressed();
        if (wasBtn || pinNow || delta != 0) {
          Serial.printf("[VIEWDT] exit trigger: wasBtn=%d pinNow=%d delta=%d\n", wasBtn, pinNow, delta);
          break;
        }

        // Таймаут 30 сек для безопасности
        if (millis() - buttonWaitStart > 30000) break;

        delay(50);
      }
      waitForEncoderRelease();
      break;
    }
  }
}

/// Обработчик настройки расписания
void ScheduleSettingsHandler() {
  int item = getSelectedMenuItemIndex();
  if (item < 0) return;
  
  switch (item) {
    case mkSetScheduleStart: {
      uint8_t h = scheduleStart[0];
      uint8_t m = scheduleStart[1];
      uint8_t s = 0;
      
      inputTime("Schedule Start", h, m, s);
      
      scheduleStart[0] = h;
      scheduleStart[1] = m;
      scheduleStart[2] = s;
      
      saveScheduleSettings();
      
      showSavedMessage();
      break;
    }
    
    case mkSetScheduleEnd: {
      uint8_t h = scheduleEnd[0];
      uint8_t m = scheduleEnd[1];
      uint8_t s = 0;
      
      inputTime("Schedule End", h, m, s);
      
      scheduleEnd[0] = h;
      scheduleEnd[1] = m;
      scheduleEnd[2] = s;
      
      saveScheduleSettings();
      
      showSavedMessage();
      break;
    }
    
    case mkToggleSchedule: {
      flags.scheduleEnabled = !flags.scheduleEnabled;
      
      saveScheduleSettings();
      
      showToggleStatus("Schedule", flags.scheduleEnabled);
      forceRedisplay();
      break;
    }
  }
}

/// Обработчик настройки ночного режима
void NightModeSettingsHandler() {
  int item = getSelectedMenuItemIndex();
  if (item < 0) return;
  
  switch (item) {
    case mkSetNightModeStart: {
      uint8_t h = nightModeStart[0];
      uint8_t m = nightModeStart[1];
      uint8_t s = 0;
      
      inputTime("Night Start Time", h, m, s);
      
      nightModeStart[0] = h;
      nightModeStart[1] = m;
      
      saveScheduleSettings();
      
      showSavedMessage();
      break;
    }
    
    case mkSetNightModeEnd: {
      uint8_t h = nightModeEnd[0];
      uint8_t m = nightModeEnd[1];
      uint8_t s = 0;
      
      inputTime("Night End Time", h, m, s);
      
      nightModeEnd[0] = h;
      nightModeEnd[1] = m;
      
      saveScheduleSettings();
      
      showSavedMessage();
      break;
    }
    
    case mkToggleNightMode: {
      flags.nightModeEnabled = !flags.nightModeEnabled;
      
      saveScheduleSettings();
      
      showToggleStatus("Night Mode", flags.nightModeEnabled);
      forceRedisplay();
      break;
    }
  }
}

/// Обработчик периода очистки фильтра
void FilterCleaningPeriodHandler() {
  uint32_t oldInterval = filterCleaningInterval;
  uint32_t current = filterCleaningInterval;
  
  // Разбиваем интервал в секундах на дни, часы и минуты
  uint8_t currentDays = current / (24UL * 3600UL);
  uint8_t currentHours = (current % (24UL * 3600UL)) / 3600UL;
  uint8_t currentMinutes = (current % 3600UL) / 60UL;
  
  // Ввод новых значений (дни от 0 до 255)
  int newDays = inputVal("Days", 0, 255, (int)currentDays);
  int newHours = inputVal("Hours", 0, 23, (int)currentHours);
  int newMinutes = inputVal("Minutes", 0, 59, (int)currentMinutes);
  
  // Сборка нового значения в секундах
  uint32_t newInterval = newDays * 24UL * 3600UL + newHours * 3600UL + newMinutes * 60UL;
  
  // Проверка на нулевой период
  if (newInterval == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error: Period > 0");
    delay(1500);
    return;
  }
  
  if (newInterval != filterCleaningInterval) {
    filterCleaningInterval = newInterval;
    // remember new interval as seconds
    saveFilterCleaningPeriod(newInterval);
    saveEventLog(LOG_INFO, EVENT_SETTINGS_CHANGE, (uint16_t)SETTING_FILTER_PERIOD, newInterval);

    // restart the filter operation timer so the new value starts counting from
    // now.  If the previous runtime had already exceeded the new interval, we
    // mark cleaning needed immediately so the user sees the effect.
    extern unsigned long filterOperationStartTime;
    extern SystemFlags flags;
    unsigned long elapsed = millis() - filterOperationStartTime;
    if (elapsed >= (unsigned long)newInterval * 1000UL) {
      flags.filterCleaningNeeded = 1;
      flags.blinkState = 1;
    }
    filterOperationStartTime = millis();
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Period set:");
  lcd.setCursor(0, 1);
  lcd.print(newDays);
  lcd.print("d ");
  if (newHours < 10) lcd.print("0");
  lcd.print(newHours);
  lcd.print("h ");
  if (newMinutes < 10) lcd.print("0");
  lcd.print(newMinutes);
  lcd.print("m");
  lcd.setCursor(0, 2);
  lcd.print("Total: ");
  lcd.print(newInterval);
  lcd.print(" sec");
  if (newInterval != oldInterval) {
    lcd.setCursor(0, 3);
    lcd.print("Change logged");
  }
  delay(2000);
}

/// Обработчик ручного режима
void ManualOperation() {
  // На входе в ручной режим гасим все реле
  turnOffAllRelays();
  
  // Список управляемых устройств
  const char* devices[] = { "PUMP", "AERATION", "OZONE", "FILTER", "BACKWASH", "EXIT" };
  int selected = 0;
  bool exitMenu = false;
  
  while (!exitMenu) {
    WDT_RESET();
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Manual Control");
    
    // Отображаем текущий выбранный пункт
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(devices[selected]);
    
    // Если это не EXIT, показываем состояние соответствующего реле
    if (selected < 5) {
      lcd.setCursor(15, 1);
      bool state = false;
      switch (selected) {
        case 0: state = getRelayState(0); break;  // PUMP
        case 1: state = getRelayState(2); break;  // AERATION
        case 2: state = getRelayState(3); break;  // OZONE
        case 3: state = getRelayState(4); break;  // FILTER
        case 4: state = getRelayState(5); break;  // BACKWASH
      }
      lcd.print(state ? "ON " : "OFF");
    }
    
    lcd.setCursor(0, 2);
    lcd.print("Rotate: Change");
    lcd.setCursor(0, 3);
    lcd.print("Press: Toggle/Exit");
    
    bool actionTaken = false;
    unsigned long startWait = millis();
    
    // Ожидание действия пользователя до 30 секунд
    while (!actionTaken && millis() - startWait < 30000) {
      WDT_RESET();
      
      int delta = readEncoderDelta();
      if (delta < 0) {
        // Переключение вверх (по кругу)
        selected = (selected + 5) % 6;
        actionTaken = true;
        delay(150);
      } else if (delta > 0) {
        // Переключение вниз
        selected = (selected + 1) % 6;
        actionTaken = true;
        delay(150);
      }
      
      if (encoderButtonPressed) {
        encoderButtonPressed = false;
        waitForEncoderRelease();
        actionTaken = true;
        
        if (selected == 5) {
          // EXIT: выключаем все реле и выходим
          exitMenu = true;
          turnOffAllRelays();
          delay(100);
        } else {
          bool newState;
          uint8_t eventCode;
          switch (selected) {
            case 0:
              // Переключение насоса 1
              newState = !getRelayState(0);
              if (newState) {
                turnOnPump1();
                startPumpEmergencyMonitoring();
              } else {
                turnOffPump1();
                stopPumpEmergencyMonitoring();
              }
              eventCode = newState ? MANUAL_PUMP1_ON : MANUAL_PUMP1_OFF;
              break;
              
            case 1:
              // Переключение аэрации
              newState = !getRelayState(2);
              if (newState) turnOnAeration(); else turnOffAeration();
              eventCode = newState ? MANUAL_AERATION_ON : MANUAL_AERATION_OFF;
              break;
              
            case 2:
              // Переключение озона — одновременно управляем и аэрацией
              newState = !getRelayState(3);
              if (newState) {
                turnOnOzone();
                turnOnAeration();
              } else {
                turnOffOzone();
                turnOffAeration();
              }
              eventCode = newState ? MANUAL_OZONE_ON : MANUAL_OZONE_OFF;
              break;
              
            case 3:
              // Переключение фильтра
              newState = !getRelayState(4);
              // Если идёт автоматический цикл обработки — запрещаем ручное выключение фильтра
              if (!newState && flags.waterTreatmentInProgress) {
                // Показать сообщение на экране и не выключать
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("Auto cycle active");
                lcd.setCursor(0,1);
                lcd.print("Cannot turn off");
                delay(1500);
                eventCode = MANUAL_FILTER_OFF; // логируем попытку
              } else {
                if (newState) {
                  turnOnFilter();
                  if (!getRelayState(1)) {
                    turnOnPump2();
                    startPump2EmergencyMonitoring();
                    saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, (uint16_t)(uint16_t)MANUAL_PUMP2_ON);
                  }
                } else {
                  turnOffFilter();
                  if (getRelayState(1) && !getRelayState(5)) {
                    turnOffPump2();
                    stopPump2EmergencyMonitoring();
                    saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, (uint16_t)(uint16_t)MANUAL_PUMP2_OFF);
                  }
                }
                eventCode = newState ? MANUAL_FILTER_ON : MANUAL_FILTER_OFF;
              }
              break;
              
            case 4:
              // Переключение обратной промывки
              newState = !getRelayState(5);
              if (newState) {
                turnOnBackwash();
                if (!getRelayState(1)) {
                  turnOnPump2();
                  startPump2EmergencyMonitoring();
                  saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_PUMP2_ON);
                }
              } else {
                turnOffBackwash();
                if (getRelayState(1) && !getRelayState(4)) {
                  turnOffPump2();
                  stopPump2EmergencyMonitoring();
                  saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, MANUAL_PUMP2_OFF);
                }
              }
              eventCode = newState ? MANUAL_BACKWASH_ON : MANUAL_BACKWASH_OFF;
              break;
          }
          // Логируем действие пользователя
          saveEventLog(LOG_INFO, EVENT_MANUAL_OPERATION, (uint16_t)eventCode);
          delay(150);
        }
      }
      delay(50);
    }
    
    // Если пользователь ничего не делал более 30 сек — выходим из ручного режима
    if (!actionTaken) {
      lcd.clear();
      lcd.print("Timeout - exiting");
      delay(300);
      turnOffAllRelays();
      exitMenu = true;
    }
  }
  lcd.clear();
}

/// Обработчик логов событий
// Показывает подробный экран события (локальная helper функция)
static void showEventLogDetailsLocalFunc(EventLog* event) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Event Details");
  lcd.setCursor(0, 1);
  char evBuf[21];
  strncpy(evBuf, getEventText(event->eventType), 20);
  evBuf[20] = '\0';
  lcd.print(evBuf);
  lcd.setCursor(0, 2);
  lcd.print("Date: ");
  if (event->day < 10) lcd.print("0");
  lcd.print(event->day); lcd.print(".");
  if (event->month < 10) lcd.print("0");
  lcd.print(event->month); lcd.print(".");
  lcd.print(event->year);
  lcd.setCursor(0, 3);
  lcd.print("Time: ");
  if (event->hour < 10) lcd.print("0");
  lcd.print(event->hour); lcd.print(":");
  if (event->minute < 10) lcd.print("0");
  lcd.print(event->minute);
  delay(1800);
}

void EventLogsHandler() {
  int eventCount = getEventCount();

  if (eventCount == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Event Logs");
    lcd.setCursor(0, 1);
    lcd.print("No events yet");
    delay(2000);
    return;
  }
  // Используем общий браузер логов из event_logging.cpp
  eventLogBrowser(eventCount);
}

/// Очистка всех логов (с подтверждением)
void clearEventLogsMenu() {
  if (!confirmAction("Clear logs?")) {
    return;
  }

  clearEventLogs();
  saveEventLog(LOG_INFO, EVENT_STATS_CLEAR, 0);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Logs cleared");
  delay(1200);
}

/// Очистка старых логов (с подтверждением)
void clearOldEventsMenu() {
  if (!confirmAction("Clear old logs?")) {
    return;
  }

  deleteOldEventLogs(30 * 24 * 3600UL);
  saveEventLog(LOG_INFO, EVENT_STATS_CLEAR, 1);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Old logs cleared");
  delay(1200);
}

void pruneEventLogsMenu() {
  if (!confirmAction("Prune logs older than 30d?")) {
    return;
  }
  pruneEventLogs(30 * 24 * 3600UL);
  saveEventLog(LOG_INFO, EVENT_STATS_CLEAR, 2);
  lcd.clear(); lcd.setCursor(0,0); lcd.print("Logs pruned"); delay(1200);
}

void formatSdMenu() {
  if (!confirmAction("Format SD? All files lost!")) return;
  bool ok = formatSDCard();
  lcd.clear(); lcd.setCursor(0,0);
  lcd.print(ok ? "SD formatted" : "Format failed");
  delay(1500);
}

/// Сброс настроек
void resetToDefaults() {
  if (confirmAction("Reset settings?")) {
    resetAllSettings();
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Settings reset!");
    lcd.setCursor(0, 1);
    lcd.print("Restarting...");
    delay(2000);
    
    softReset();
  }
}

// ============ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ============

/// Ожидание отпускания кнопки
void waitForEncoderRelease() {
  while (encoderButtonPressed) {
    delay(10);
  }
  encoderButtonPressed = false;
  delay(100); // Debounce
}

/// Чтение дельты энкодера
int readEncoderDelta() {
  noInterrupts();  // Атомарное чтение
  int delta = encoderPos;
  encoderPos = 0;
  interrupts();
  return delta;
}

    // Вспомогательная функция для подробного экрана
    auto showEventLogDetailsLocal = [](EventLog* event) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Event Details");
  lcd.setCursor(0, 1);
  char evBuf[21];
  strncpy(evBuf, getEventText(event->eventType), 20);
  evBuf[20] = '\0';
  lcd.print(evBuf);
  lcd.setCursor(0, 2);
  lcd.print("Date: ");
  if (event->day < 10) lcd.print("0");
  lcd.print(event->day); lcd.print(".");
  if (event->month < 10) lcd.print("0");
  lcd.print(event->month); lcd.print(".");
  lcd.print(event->year);
  lcd.setCursor(0, 3);
  lcd.print("Time: ");
  if (event->hour < 10) lcd.print("0");
  lcd.print(event->hour); lcd.print(":");
  if (event->minute < 10) lcd.print("0");
  lcd.print(event->minute);
  delay(1800);
    };

void showEventDetails(EventLog* event);
// removed global prototype to avoid conflicts
