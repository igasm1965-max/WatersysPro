/**
 * @file engineer_menu.cpp
 * @brief Реализация инженерного меню с защитой паролем
 * @details Настройка параметров безопасности, watchdog, смена пароля
 */

#include "engineer_menu.h"
#include "config.h"
#include "structures.h"
#include "display.h"
#include "encoder.h"
#include "event_logging.h"
#include "menu.h"
#include "prefs.h"
#include "utils.h"

#ifdef ENABLE_WIFI
#include <WiFi.h>
#include "web_server.h"
#include "vqtt.h"
extern void initWiFi();
extern void restartWebServer();
#endif

// ============ ВНЕШНИЕ ПЕРЕМЕННЫЕ ============

extern SafeLCD lcd;
extern SystemFlags flags;
extern SafetySettings safetySettings;
extern bool engineerModeUnlocked;
extern Preferences preferences;

// Флаг для сигнала системе меню о переходе в подменю
bool engineerMenuRequested = false;

// ============ ФУНКЦИИ АВТОРИЗАЦИИ ============

bool checkEngineerPassword(uint32_t enteredPassword) {
  return (enteredPassword == safetySettings.engineerPassword);
}

bool isEngineerModeUnlocked() {
  return engineerModeUnlocked;
}

void lockEngineerMode() {
  engineerModeUnlocked = false;
}

/**
 * @brief Показывает экран ввода пароля
 * @return true если пароль введён верно
 */
bool showPasswordScreen() {
  // If saved password is 000000 (zero), allow direct access without prompting
  if (safetySettings.engineerPassword == 0) {
    engineerModeUnlocked = true;
    engineerMenuRequested = true;
    flags.displayLocked = 0;
    saveEventLog(LOG_INFO, EVENT_MENU_ACTION, (uint16_t)mkEngineerMenu);
    return true;
  }

  uint8_t digits[PASSWORD_DIGITS] = {0};
  uint8_t currentDigit = 0;
  unsigned long startTime = millis();
  bool needsRedraw = true;
  uint8_t lastDigits[PASSWORD_DIGITS] = {0};
  uint8_t lastCurrentDigit = 255;
  uint8_t attempts = 0;
  const uint8_t MAX_ATTEMPTS = 3;
  
  flags.displayLocked = 1;
  engineerMenuRequested = false;
  
  while (attempts < MAX_ATTEMPTS) {
    // Сброс для новой попытки
    currentDigit = 0;
    for (int i = 0; i < PASSWORD_DIGITS; i++) {
      digits[i] = 0;
      lastDigits[i] = 255;  // Форсируем перерисовку
    }
    lastCurrentDigit = 255;
    needsRedraw = true;
    startTime = millis();
    
    // Начальная отрисовка статической части
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("===ENGINEER MODE===");
    lcd.setCursor(0, 1);
    lcd.print("Enter password:");
    lcd.setCursor(0, 3);
    if (attempts > 0) {
      lcd.print("Attempts left: ");
      lcd.print(MAX_ATTEMPTS - attempts);
    } else {
      lcd.print("Turn=digit Click=nxt");
    }
    
    // Compute centered position for password field
    int pwdFieldWidth = PASSWORD_DIGITS + 2; // [digits]
    int pwdStart = (LCD_COLS - pwdFieldWidth) / 2;    
    bool passwordEntered = false;
    bool passwordCorrect = false;
    
    while (!passwordEntered && (millis() - startTime < PASSWORD_TIMEOUT)) {
      // Обновляем только поле ввода если что-то изменилось
      bool digitsChanged = (currentDigit != lastCurrentDigit);
      for (int i = 0; i < PASSWORD_DIGITS && !digitsChanged; i++) {
        if (digits[i] != lastDigits[i]) digitsChanged = true;
      }
      
      if (digitsChanged || needsRedraw) {
        // Draw centered password field
        lcd.setCursor(pwdStart, 2);
        lcd.print("[");
        for (int i = 0; i < PASSWORD_DIGITS; i++) {
          lcd.print(digits[i]);
        }
        lcd.print("]");
        
        // Курсор под текущей цифрой
        lcd.setCursor(pwdStart + 1 + currentDigit, 2);
        lcd.blink();        
        // Запоминаем состояние
        lastCurrentDigit = currentDigit;
        for (int i = 0; i < PASSWORD_DIGITS; i++) {
          lastDigits[i] = digits[i];
        }
        needsRedraw = false;
      }
      
      // Обработка энкодера
      eEncoderState action = getEncoderState();
      
      if (action == eRight) {
        digits[currentDigit] = (digits[currentDigit] + 1) % 10;
      } else if (action == eLeft) {
        digits[currentDigit] = (digits[currentDigit] + 9) % 10;  // -1 mod 10
      } else if (action == eButton) {
        currentDigit++;
        if (currentDigit >= PASSWORD_DIGITS) {
          // Все цифры введены - проверяем пароль
          uint32_t entered = 0;
          for (int i = 0; i < PASSWORD_DIGITS; i++) {
            entered = entered * 10 + digits[i];
          }
          
          passwordEntered = true;
          passwordCorrect = checkEngineerPassword(entered);
        }
      }
      
      delay(50);
    }
    
    // Таймаут - выход
    if (!passwordEntered) {
      lcd.noBlink();
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("    TIMEOUT!");
      delay(1500);
      flags.displayLocked = 0;
      return false;
    }
    
    // Проверка результата
    if (passwordCorrect) {
      engineerModeUnlocked = true;
      engineerMenuRequested = true;
      lcd.noBlink();
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("  ACCESS GRANTED!");
      delay(1500);
      flags.displayLocked = 0;
      saveEventLog(LOG_INFO, EVENT_MENU_ACTION, (uint16_t)mkEngineerMenu);
      return true;
    } else {
      attempts++;
      lcd.noBlink();
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("  WRONG PASSWORD!");
      if (attempts < MAX_ATTEMPTS) {
        lcd.setCursor(0, 2);
        lcd.print("  Try again...");
        delay(1500);
        // Продолжаем цикл - новая попытка
      } else {
        lcd.setCursor(0, 2);
        lcd.print("  Access denied");
        delay(2000);
      }
    }
  }
  
  // Исчерпаны все попытки
  flags.displayLocked = 0;
  return false;
}

// ============ РЕДАКТИРОВАНИЕ ЗНАЧЕНИЙ ============

/**
 * @brief Универсальный редактор числового значения
 * @param title Заголовок экрана
 * @param value Указатель на редактируемое значение
 * @param minVal Минимальное значение
 * @param maxVal Максимальное значение
 * @param step Шаг изменения
 * @param unit Единица измерения ("min", "sec", etc.)
 */
static void editValue(const char* title, uint16_t* value, uint16_t minVal, uint16_t maxVal, uint16_t step, const char* unit) {
  uint16_t tempValue = *value;
  uint16_t lastDisplayedValue = tempValue + 1;  // Гарантирует первую отрисовку
  bool editing = true;
  
  flags.displayLocked = 1;

  // Prevent the menu-selection press from being seen as an immediate save/click
  if (isEncoderButtonPressed()) {
    waitForButtonRelease();
    wasEncoderButtonPressed();
    delay(50);
  }
  
  // Начальная отрисовка статической части
  lcd.clear();
  lcd.setCursor(0, 0);
  char tbuf[21];
  strncpy(tbuf, title, 20);
  tbuf[20] = '\0';
  lcd.print(tbuf);
  lcd.setCursor(0, 2);
  lcd.print("Range: ");
  lcd.print(minVal);
  lcd.print("-");
  lcd.print(maxVal);
  lcd.print(" ");
  lcd.print(unit);
  lcd.setCursor(0, 3);
  lcd.print("Turn=edit Click=save");
  
  while (editing) {
    // Обновляем только значение если изменилось
    if (tempValue != lastDisplayedValue) {
      lcd.setCursor(0, 1);
      lcd.print("Value: ");
      lcd.print(tempValue);
      lcd.print(" ");
      lcd.print(unit);
      lcd.print("     ");  // Очистка остатка строки
      lastDisplayedValue = tempValue;
    }
    
    eEncoderState action = getEncoderState();
    
    if (action == eRight) {
      if (tempValue + step <= maxVal) {
        tempValue += step;
      }
    } else if (action == eLeft) {
      if (tempValue >= minVal + step) {
        tempValue -= step;
      } else {
        tempValue = minVal;
      }
    } else if (action == eButton) {
      // wait for release so long-presses or bounce won't cause unreliable saves
      waitForEncoderRelease();
      *value = tempValue;
      editing = false;
      
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("    SAVED!");
      delay(1000);
    }
    
    delay(50);
  }
  
  flags.displayLocked = 0;
}

// ============ ФУНКЦИИ РЕДАКТИРОВАНИЯ ТАЙМАУТОВ ============

void editTimeoutFilling() {
  editValue("Timeout: FILLING", &safetySettings.timeoutFilling, 5, 120, 5, "min");
  saveSafetySettings();
}

void editTimeoutOzonation() {
  editValue("Timeout: OZONATION", &safetySettings.timeoutOzonation, 10, 480, 10, "min");
  saveSafetySettings();
}

void editTimeoutAeration() {
  editValue("Timeout: AERATION", &safetySettings.timeoutAeration, 10, 480, 10, "min");
  saveSafetySettings();
}

void editTimeoutSettling() {
  editValue("Timeout: SETTLING", &safetySettings.timeoutSettling, 60, 2880, 60, "min");
  saveSafetySettings();
}

void editTimeoutFiltration() {
  editValue("Timeout: FILTRATION", &safetySettings.timeoutFiltration, 10, 480, 10, "min");
  saveSafetySettings();
}

void editTimeoutBackwash() {
  editValue("Timeout: BACKWASH", &safetySettings.timeoutBackwash, 5, 60, 5, "min");
  saveSafetySettings();
}

void editPumpDryTimeout() {
  uint16_t temp = safetySettings.pumpDryTimeoutSeconds;
  editValue("Pump Dry Timeout", &temp, 5, 600, 5, "sec");
  safetySettings.pumpDryTimeoutSeconds = temp;
  saveSafetySettings();
}

void editPumpMinLevelDelta() {
  uint8_t temp = safetySettings.pumpMinLevelDeltaCm;
  uint16_t temp16 = temp; // reuse editValue signature
  editValue("Pump Min Delta", &temp16, 0, 20, 1, "cm");
  safetySettings.pumpMinLevelDeltaCm = (uint8_t)temp16;
  saveSafetySettings();
}

void editPumpDryConsecutiveChecks() {
  uint8_t temp = safetySettings.pumpDryConsecutiveChecks;
  uint16_t temp16 = temp;
  editValue("Pump Dry Checks", &temp16, 1, 10, 1, "cnt");
  safetySettings.pumpDryConsecutiveChecks = (uint8_t)temp16;
  saveSafetySettings();
}


// ============ WATCHDOG НАСТРОЙКИ ============

void toggleWatchdog() {
  safetySettings.watchdogEnabled = !safetySettings.watchdogEnabled;
  
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("  Watchdog: ");
  lcd.print(safetySettings.watchdogEnabled ? "ON" : "OFF");
  delay(1500);
  
  saveSafetySettings();
}

void editWatchdogTimeout() {
  uint16_t temp = safetySettings.watchdogTimeout;
  editValue("Watchdog Timeout", &temp, 1, 30, 1, "sec");
  safetySettings.watchdogTimeout = (uint8_t)temp;
  saveSafetySettings();
}

// ============ СМЕНА ПАРОЛЯ ============

void showChangePasswordScreen() {
  uint8_t digits[PASSWORD_DIGITS];
  for (int i = 0; i < PASSWORD_DIGITS; ++i) digits[i] = 0;
  uint8_t currentDigit = 0;
  uint8_t lastDigits[PASSWORD_DIGITS];
  for (int i = 0; i < PASSWORD_DIGITS; ++i) lastDigits[i] = 255;
  uint8_t lastCurrentDigit = 255;
  bool confirmed = false;
  
  flags.displayLocked = 1;
  
  // Начальная отрисовка статической части
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("=== NEW PASSWORD ===");
  lcd.setCursor(0, 1);
  // Prompt dynamically reflects number of digits
  char prompt[20];
  snprintf(prompt, sizeof(prompt), "Enter %d digits:", PASSWORD_DIGITS);
  lcd.print(prompt);
  lcd.setCursor(0, 3);
  lcd.print("Long press = cancel");
  
  // Center field
  int pwdFieldWidth = PASSWORD_DIGITS + 2; // [digits]
  int pwdStart = (LCD_COLS - pwdFieldWidth) / 2;  
  while (!confirmed) {
    // Обновляем только если изменилось
    bool changed = (currentDigit != lastCurrentDigit);
    for (int i = 0; i < PASSWORD_DIGITS && !changed; i++) {
      if (digits[i] != lastDigits[i]) changed = true;
    }
    
    if (changed) {
      lcd.setCursor(pwdStart, 2);
      lcd.print("[");
      for (int i = 0; i < PASSWORD_DIGITS; i++) {
        lcd.print(digits[i]);
      }
      lcd.print("]  ");
      
      // Курсор под текущей цифрой
      lcd.setCursor(pwdStart + 1 + currentDigit, 2);
      lcd.blink();      
      lastCurrentDigit = currentDigit;
      for (int i = 0; i < PASSWORD_DIGITS; i++) {
        lastDigits[i] = digits[i];
      }
    }
    
    eEncoderState action = getEncoderState();
    
    if (action == eRight) {
      digits[currentDigit] = (digits[currentDigit] + 1) % 10;
    } else if (action == eLeft) {
      digits[currentDigit] = (digits[currentDigit] + 9) % 10;
    } else if (action == eButton) {
      currentDigit++;
      if (currentDigit >= PASSWORD_DIGITS) {
        // Compute new numeric password from digits
        uint32_t newPassword = 0;
        for (int i = 0; i < PASSWORD_DIGITS; i++) {
          newPassword = newPassword * 10 + digits[i];
        }
        safetySettings.engineerPassword = newPassword;
        saveSafetySettings();
        
        lcd.noBlink();
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print(" PASSWORD CHANGED!");
        lcd.setCursor(0, 2);
        lcd.print("   New: ");
        // Print with leading zeros if necessary
        char buf[16];
        snprintf(buf, sizeof(buf), "%0*u", PASSWORD_DIGITS, (unsigned int)newPassword);
        lcd.print(buf);
        delay(2000);
        confirmed = true;
      }
    }
    
    delay(50);
  }
  
  lcd.noBlink();
  flags.displayLocked = 0;
}



void showEventLogSettings() {
  extern Preferences preferences;
  extern SafeLCD lcd;

  const char* labels[] = {
    "Wi-Fi events",
    "Manual ops",
    "Emergency",
    "Watchdog",
    "Sensor Err",
    "Settings",
    "Web/API",
    "State Chg",
    "Errors only"
  };
  const uint32_t bits[] = {
    LOG_FILTER_WIFI,
    LOG_FILTER_MANUAL,
    LOG_FILTER_EMERGENCY,
    LOG_FILTER_WATCHDOG,
    LOG_FILTER_SENSOR,
    LOG_FILTER_SETTINGS,
    LOG_FILTER_WEB,
    LOG_FILTER_STATECHANGE,
    LOG_FILTER_ERRORS
  };
  const int numOptions = sizeof(bits) / sizeof(bits[0]);

  uint32_t mask = preferences.getULong(PREF_KEY_LOG_FILTER, (uint32_t)PREF_KEY_LOG_FILTER_DEFAULT);
  int selected = 0;
  bool exit = false;
  bool needUpdate = true;

  flags.displayLocked = 1;

  while (!exit) {
    if (needUpdate) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Event Log Settings");
      // show up to 3 options per page (lines 1..3)
      int page = selected / 3;
      int start = page * 3;
      for (int i = 0; i < 3; i++) {
        int idx = start + i;
        lcd.setCursor(0, i + 1);
        if (idx < numOptions) {
          if (idx == selected) lcd.print(">"); else lcd.print(" ");
          // Label (max 16 chars, reserve 4 for status)
          char buf[17];
          strncpy(buf, labels[idx], 16);
          buf[16] = '\0';
          lcd.print(buf);
          int curLen = strlen(buf) + 1;
          // Status
          lcd.setCursor(16, i + 1);
          lcd.print((mask & bits[idx]) ? "ON" : "OFF");
        } else {
          // clear line
          for (int k = 0; k < 20; ++k) lcd.print(' ');
        }
      }
      needUpdate = false;
    }

    int delta = readEncoderDelta();
    if (delta != 0) {
      if (delta < 0 && selected > 0) selected--;
      else if (delta > 0 && selected < numOptions - 1) selected++;
      needUpdate = true;
    }

    // Button handling - short press toggles, long press exits
    static bool prevButtonHigh = true;
    bool currButtonLow = (digitalRead(pinSW) == LOW);
    if (currButtonLow && prevButtonHigh) {
      unsigned long pressStart = millis();
      bool longExit = false;
      while (digitalRead(pinSW) == LOW) {
        WDT_RESET();
        if (millis() - pressStart > 700) {
          // Long press - exit
          while (digitalRead(pinSW) == LOW) delay(10);
          longExit = true;
          break;
        }
        delay(10);
      }
      if (longExit) {
        exit = true;
        break;
      }
      // Short press - toggle
      mask ^= bits[selected];
      preferences.putULong(PREF_KEY_LOG_FILTER, mask);
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("  Preference saved");
      delay(800);
      needUpdate = true;
    }
    prevButtonHigh = !currButtonLow;

    WDT_RESET();
    delay(80);
  }

  flags.displayLocked = 0;
}





// ============ СИСТЕМНАЯ ИНФОРМАЦИЯ ============

// ============ Wi-Fi SETUP UI ============

static const char wifiCharSet[] = " -_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void editString(const char* title, const char* prefKey, int maxLen, bool isPassword) {
  flags.displayLocked = 1;
  // If the selection button is still physically pressed (menu->editor transition),
  // wait for release so the editor won't immediately register that same press.
  if (isEncoderButtonPressed()) {
    waitForButtonRelease();      // wait until the user releases the switch pin
    wasEncoderButtonPressed();   // clear any ISR flag just in case
    delay(50);                   // debounce
  }
  // Enforce admin token maximum length of 20; otherwise respect caller maxLen
  int effectiveMaxLen = maxLen;
  if (strcmp(prefKey, PREF_KEY_ADMIN_TOKEN) == 0) {
    if (effectiveMaxLen > 20) effectiveMaxLen = 20;
  }

  String value = preferences.getString(prefKey, "");
  if (value.length() > effectiveMaxLen) value = value.substring(0, effectiveMaxLen);

  int pos = value.length();
  int charsetLen = strlen(wifiCharSet);
  int charIdx = 0;
  if (pos < value.length()) {
    char c = value.charAt(pos);
    for (int i=0; i<charsetLen; ++i) if (wifiCharSet[i] == c) { charIdx = i; break; }
  }

  bool editing = true;
  unsigned long lastDraw = 0;

  // For reducing flicker: track last displayed fields
  String lastDisp = "";
  int lastPos = -1;
  int lastCharIdx = -1;

  // Long-press detection threshold
  const unsigned long LONG_PRESS_MS = 800;

  // Double-click detection for cursor movement — increase window for reliability
  const unsigned long DOUBLE_CLICK_MS = 700;
  unsigned long lastClickTime = 0;
  bool cursorMode = false; // when true, encoder rotation moves cursor position

  while (editing) {
    // Handle rotation and short-clicks via getEncoderState
    eEncoderState action = getEncoderState();
    if (action == eRight) {
      if (cursorMode) {
        // move cursor right
        if (pos < value.length()) pos++;
        else if (value.length() < effectiveMaxLen) pos++;
      } else {
        // change current char
        charIdx = (charIdx + 1) % charsetLen;
      }
    } else if (action == eLeft) {
      if (cursorMode) {
        if (pos > 0) pos--;
      } else {
        charIdx = (charIdx + charsetLen - 1) % charsetLen;
      }
    }
    // NOTE: button (click/long-press/double-click) is handled below using
    // pin-state press/release logic to reliably distinguish short vs long press.

    // Button press handling: support short click, double-click and long-press save
    if (isEncoderButtonPressed()) {
      // consume ISR flag to avoid duplicate eButton events
      wasEncoderButtonPressed();

      unsigned long pressStart = millis();
      bool longSaved = false;
      // wait for release or long-press timeout
      while (isEncoderButtonPressed()) {
        WDT_RESET();
        if (millis() - pressStart >= LONG_PRESS_MS) {
          // long press confirmed — save and exit
          if (value.length() > 0) {
            preferences.putString(prefKey, value);
            lcd.clear(); lcd.setCursor(0,1); lcd.print("Saved");
          } else {
            preferences.remove(prefKey);
            lcd.clear(); lcd.setCursor(0,1); lcd.print("Cleared");
          }
          delay(CONFIRM_SCREEN_DELAY);
          editing = false;
          longSaved = true;
          break;
        }
        delay(10);
      }
      if (longSaved) break;

      // button released — treat as short click / double click
      unsigned long now = millis();
      if (now - lastClickTime <= DOUBLE_CLICK_MS) {
        // double-click -> toggle cursor movement mode
        cursorMode = !cursorMode;
        if (cursorMode && pos < value.length()) {
          char cur = value.charAt(pos);
          for (int i=0; i<charsetLen; ++i) if (wifiCharSet[i] == cur) { charIdx = i; break; }
        }
        lastClickTime = 0;
        continue;
      }

      lastClickTime = now;
      if (cursorMode) {
        cursorMode = false;
        if (pos < value.length()) {
          char cur = value.charAt(pos);
          for (int i=0;i<charsetLen;++i) if (wifiCharSet[i] == cur) { charIdx = i; break; }
        } else {
          charIdx = 0;
        }
        continue;
      }

      // Short click: select char and advance
      char c = wifiCharSet[charIdx];
      if (pos >= value.length() && c == ' ') {
        editing = false;
        break;
      }
      if (pos < value.length()) value.setCharAt(pos, c);
      else value += c;
      pos++;
      if (pos >= effectiveMaxLen) { editing = false; break; }
      charIdx = 0;
    }

    // Redraw only when something changed to avoid blinking
    if ((millis() - lastDraw >= 80) && (lastDisp != value || lastPos != pos || lastCharIdx != charIdx || cursorMode != (lastCharIdx==-2))) {
      lcd.clear();
      lcd.setCursor(0,0);
      char tbuf[21];
      strncpy(tbuf, title, 20);
      tbuf[20] = '\0';
      lcd.print(tbuf);
      lcd.setCursor(0,1);
      // show up to LCD_COLS chars, scroll if needed
      String disp = value;
      int visibleStart = 0;
      if (disp.length() > LCD_COLS) {
        visibleStart = disp.length() - LCD_COLS;
        disp = disp.substring(visibleStart);
      }
      if (isPassword) {
        for (int i=0;i<disp.length();++i) disp.setCharAt(i, '*');
      }
      lcd.print(disp);

      // show cursor position indicator and editing char
      lcd.setCursor(0,2);
      lcd.print("Pos:");
      lcd.print(pos);
      lcd.setCursor(0,3);
      if (cursorMode) {
        lcd.print("Turn=move Click=exit");
      } else {
        lcd.print("Char: ");
        lcd.print(wifiCharSet[charIdx]);
        lcd.print("Turn=chg Click=next");
      }

      // Visual cursor: blink at selected character when in cursor mode
      if (cursorMode) {
        int visCol = pos - visibleStart;
        if (visCol < 0) visCol = 0;
        if (visCol > LCD_COLS) visCol = LCD_COLS;
        lcd.setCursor(visCol, 1);
        lcd.blink();
      } else {
        lcd.noBlink();
      }

      // store last drawn
      lastDisp = value;
      lastPos = pos;
      lastCharIdx = cursorMode ? -2 : charIdx;
      lastDraw = millis();
    }

    delay(30);
  }

  // If editing finished normally (not via long press save we already did it), then save trimmed value
  if (!isEncoderButtonPressed()) {
    if (value.length() > 0) {
      preferences.putString(prefKey, value);
      lcd.clear(); lcd.setCursor(0,1); lcd.print("Saved");
    } else {
      preferences.remove(prefKey);
      lcd.clear(); lcd.setCursor(0,1); lcd.print("Cleared");
    }
    delay(CONFIRM_SCREEN_DELAY);
  }

  flags.displayLocked = 0;
}

void showWiFiSetup() {
  flags.displayLocked = 1;
  const char* items[] = { "Set SSID", "Set Password", "Clear Wi-Fi", "Back" };
  int itemCount = 4;
  int selected = 0;
  bool exitMenu = false;
  int lastSelected = -1;

  while (!exitMenu) {
    if (selected != lastSelected) {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("=== Wi-Fi SETUP ===");
      for (int i=0;i<3;i++) {
        int idx = (selected/3)*3 + i;
        if (idx >= itemCount) break;
        int row = 1 + i;
        lcd.setCursor(0,row);
        if (idx == selected) lcd.print("> "); else lcd.print("  ");
        lcd.print(items[idx]);
      }
      lastSelected = selected;
    }

    eEncoderState action = getEncoderState();
    if (action == eRight) selected = (selected + 1) % itemCount;
    else if (action == eLeft) selected = (selected + itemCount - 1) % itemCount;
    else if (action == eButton) {
      switch (selected) {
        case 0:
          editString("Edit SSID", PREF_KEY_WIFI_SSID, 32, false);
          // flush encoder/button state left by editor and wait release to avoid "stuck" UI
          readEncoderDelta();
          wasEncoderButtonPressed();
          waitForEncoderRelease();
          lastSelected = -1; // force redraw of menu
          break;
        case 1:
          editString("Edit PASS", PREF_KEY_WIFI_PASS, 63, true);
          readEncoderDelta();
          wasEncoderButtonPressed();
          waitForEncoderRelease();
          lastSelected = -1;
          break;
        case 2:
          clearWifiSettings();
          readEncoderDelta();
          wasEncoderButtonPressed();
          waitForEncoderRelease();
          lastSelected = -1;
          break;
        case 3: exitMenu = true; break;
      }
    }

    delay(100);
  }

  flags.displayLocked = 0;
}

// ============ MQTT SETUP (ENGINEER MENU) ============

void showMqttSetup() {
  flags.displayLocked = 1;
  const char* items[] = { "Enable/Disable", "Broker", "Port", "User", "Pass", "Topic base", "Interval (s)", "Test publish", "Clear MQTT", "Back" };
  int itemCount = 10;
  int selected = 0;
  bool exitMenu = false;
  int lastSelected = -1;

  while (!exitMenu) {
    if (selected != lastSelected) {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("=== MQTT SETUP ===");
      for (int i=0;i<3;i++) {
        int idx = (selected/3)*3 + i;
        if (idx >= itemCount) break;
        int row = 1 + i;
        lcd.setCursor(0,row);
        if (idx == selected) lcd.print("> "); else lcd.print("  ");
        lcd.print(items[idx]);
      }
      lastSelected = selected;
    }

    eEncoderState action = getEncoderState();
    if (action == eRight) selected = (selected + 1) % itemCount;
    else if (action == eLeft) selected = (selected + itemCount - 1) % itemCount;
    else if (action == eButton) {
      switch (selected) {
        case 0: toggleMqttEnabled(); break;
        case 1: editMqttBroker(); break;
        case 2: editMqttPort(); break;
        case 3: editMqttUser(); break;
        case 4: editMqttPass(); break;
        case 5: editMqttTopicBase(); break;
        case 6: editMqttInterval(); break;
        case 7: testMqttPublish(); break;
        case 8: clearMqttSettings(); break;
        case 9: exitMenu = true; break;
      }
    }

    delay(100);
  }

  flags.displayLocked = 0;
}

void toggleMqttEnabled() {
  bool v = preferences.getBool(PREF_KEY_MQTT_ENABLED, DEFAULT_MQTT_ENABLED);
  preferences.putBool(PREF_KEY_MQTT_ENABLED, !v);
  lcd.clear(); lcd.setCursor(0,1); lcd.print("MQTT " ); lcd.print(!v?"ENABLED":"DISABLED");
  // re-init client
  initVQTT();
  delay(1000);
}

void editMqttBroker() {
  editString("MQTT Broker", PREF_KEY_MQTT_BROKER, 64, false);
  initVQTT();
}

void editMqttPort() {
  uint16_t tmp = preferences.getUInt(PREF_KEY_MQTT_PORT, DEFAULT_MQTT_PORT);
  editValue("MQTT Port", &tmp, 1, 65535, 1, "");
  preferences.putUInt(PREF_KEY_MQTT_PORT, tmp);
  initVQTT();
}

void editMqttUser() {
  editString("MQTT User", PREF_KEY_MQTT_USER, 32, false);
  initVQTT();
}

void editMqttPass() {
  editString("MQTT Pass", PREF_KEY_MQTT_PASS, 64, true);
  initVQTT();
}

static bool mqttTopicBaseValid(const String &s) {
  if (s.length() == 0) return false;
  for (size_t i = 0; i < s.length(); ++i) {
    uint8_t c = (uint8_t)s[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '/')
      continue;
    return false;
  }
  return true;
}

void editMqttTopicBase() {
  editString("MQTT Topic", PREF_KEY_MQTT_TOPIC_BASE, 32, false);
  String t = preferences.getString(PREF_KEY_MQTT_TOPIC_BASE, DEFAULT_MQTT_TOPIC_BASE);
  if (!mqttTopicBaseValid(t)) {
    lcd.clear(); lcd.setCursor(0,0);
    lcd.print("bad topic base");
    delay(1200);
    preferences.putString(PREF_KEY_MQTT_TOPIC_BASE, DEFAULT_MQTT_TOPIC_BASE);
  }
  initVQTT();
}

void editMqttInterval() {
  uint16_t tmp = preferences.getUInt(PREF_KEY_MQTT_PUB_INTERVAL, 10);
  editValue("MQTT Int(s)", &tmp, 1, 3600, 1, "s");
  preferences.putUInt(PREF_KEY_MQTT_PUB_INTERVAL, tmp);
  initVQTT();
}

void clearMqttSettings() {
  preferences.remove(PREF_KEY_MQTT_BROKER);
  preferences.remove(PREF_KEY_MQTT_PORT);
  preferences.remove(PREF_KEY_MQTT_USER);
  preferences.remove(PREF_KEY_MQTT_PASS);
  preferences.remove(PREF_KEY_MQTT_TOPIC_BASE);
  preferences.remove(PREF_KEY_MQTT_PUB_INTERVAL);
  preferences.putBool(PREF_KEY_MQTT_ENABLED, false);
  lcd.clear(); lcd.setCursor(0,1); lcd.print("MQTT cleared");
  initVQTT();
  delay(1000);
}

void testMqttPublish() {
  flags.displayLocked = 1;
  lcd.clear(); lcd.setCursor(0,0); lcd.print("MQTT: Test pub");
  // simple payload with uptime
  char payload[64];
  snprintf(payload, sizeof(payload), "test:uptime=%lu", millis()/1000);
  bool ok = vqtt_testPublish(payload);
  lcd.setCursor(0,1);
  if (ok) lcd.print("Published"); else lcd.print("Failed");
  delay(1200);
  flags.displayLocked = 0;
}

void editWiFiSSID() {
  editString("Edit SSID", PREF_KEY_WIFI_SSID, 32, false);
}

void editWiFiPassword() {
  editString("Edit PASS", PREF_KEY_WIFI_PASS, 63, true);
}

void clearWifiSettings() {
  preferences.remove(PREF_KEY_WIFI_SSID);
  preferences.remove(PREF_KEY_WIFI_PASS);
  lcd.clear(); lcd.setCursor(0,1); lcd.print("Wi-Fi cleared");
#ifdef ENABLE_WIFI
  Serial.println("[WiFi] Cleared stored credentials, restarting WiFi as AP...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(200);
  initWiFi(); // initWiFi will start AP when no SSID saved
  restartWebServer();
  lcd.clear(); lcd.setCursor(0,1); lcd.print("AP mode");
#endif
  delay(1000);
}

void showSystemInfo() {
  flags.displayLocked = 1;
  
  extern uint32_t setlingDuration, aerationDuration, ozonationDuration;
  extern WDTStats wdtStats;
  
  int page = 0;
  int lastPage = -1;  // Для отслеживания смены страницы
  bool viewing = true;
  
  while (viewing) {
    // Перерисовываем только при смене страницы
    if (page != lastPage) {
      lcd.clear();
      
      switch (page) {
        case 0:
          lcd.setCursor(0, 0);
          lcd.print("===SYS INFO 1/3===");
          lcd.setCursor(0, 1);
          lcd.print("RAM: ");
          lcd.print(ESP.getFreeHeap() / 1024);
          lcd.print(" KB free");
          lcd.setCursor(0, 2);
          lcd.print("Flash: ");
          lcd.print(ESP.getFlashChipSize() / 1024 / 1024);
          lcd.print(" MB");
          lcd.setCursor(0, 3);
          lcd.print("Uptime: ");
          lcd.print(millis() / 1000 / 60);
          lcd.print(" min");
          break;
          
        case 1:
          lcd.setCursor(0, 0);
          lcd.print("=== TIMEOUTS 2/3 ===");
          lcd.setCursor(0, 1);
          lcd.print("Fill:");
          lcd.print(safetySettings.timeoutFilling);
          lcd.print(" Ozon:");
          lcd.print(safetySettings.timeoutOzonation);
          lcd.setCursor(0, 2);
          lcd.print("Aer:");
          lcd.print(safetySettings.timeoutAeration);
          lcd.print(" Setl:");
          lcd.print(safetySettings.timeoutSettling);
          lcd.setCursor(0, 3);
          lcd.print("Filt:");
          lcd.print(safetySettings.timeoutFiltration);
          lcd.print(" BW:");
          lcd.print(safetySettings.timeoutBackwash);
          break;
          
        case 2:
          lcd.setCursor(0, 0);
          lcd.print("=== WATCHDOG 3/3 ===");
          lcd.setCursor(0, 1);
          lcd.print("Status: ");
          lcd.print(safetySettings.watchdogEnabled ? "ENABLED" : "DISABLED");
          lcd.setCursor(0, 2);
          lcd.print("Timeout: ");
          lcd.print(safetySettings.watchdogTimeout);
          lcd.print(" sec");
          lcd.setCursor(0, 3);
          lcd.print("Resets: ");
          lcd.print(wdtStats.resetCount);
          break;
      }
      
      lastPage = page;
    }
    
    eEncoderState action = getEncoderState();
    
    if (action == eRight) {
      page = (page + 1) % 3;
    } else if (action == eLeft) {
      page = (page + 2) % 3;
    } else if (action == eButton) {
      viewing = false;
    }
    
    delay(100);
  }
  
  flags.displayLocked = 0;
}

// ============ ИНФОРМАЦИЯ О СЕТЕВОМ ПОДКЛЮЧЕНИИ ============

// Константы для настройки отображения
const int PAGE_COUNT = 3;
const int LCD_WIDTH = 20;
const int MAX_TOKEN_LEN = 20;          // Максимальная длина токена (должна совпадать с editString)
const int MAX_TOKEN_DISPLAY = 40;       // Сколько символов токена показывать на экране (2 строки по 20)

// forward declarations for helper drawing functions
static void updateNetworkInfoPage(int page);
static void drawPage1();
static void drawPage2();
static void drawPage3();

void showNetworkInfo() {
    flags.displayLocked = 1;

    int page = 0;
    int lastPage = -1;
    bool viewing = true;

    // Для неблокирующего опроса энкодера
    unsigned long lastEncoderCheck = 0;
    const unsigned long encoderInterval = 20; // мс

    while (viewing) {
        // Неблокирующий опрос энкодера
        if (millis() - lastEncoderCheck >= encoderInterval) {
            lastEncoderCheck = millis();
            eEncoderState action = getEncoderState();

            if (action == eRight) {
                page = (page + 1) % PAGE_COUNT;
            } else if (action == eLeft) {
                page = (page + PAGE_COUNT - 1) % PAGE_COUNT;
            } else if (action == eButton) {
                if (page == 2) {
                    // Editing removed on-device — show instruction
                    lcd.clear();
                    lcd.setCursor(0,1);
                    lcd.print("Use Web UI to edit");
                    lcd.setCursor(0,2);
                    lcd.print("Main page -> Admin token");
                    delay(1500);
                    lastPage = -1;
                } else {
                    viewing = false;
                }
            }
        }

        // Обновление экрана, если страница изменилась
        if (page != lastPage) {
            updateNetworkInfoPage(page);
            lastPage = page;
        }

        // Периодическое обновление страницы 1 (статус WiFi может измениться)
        static unsigned long lastWiFiCheck = 0;
        if (page == 0 && millis() - lastWiFiCheck >= 1000) { // раз в секунду
            lastWiFiCheck = millis();
            // Перерисовываем только если статус изменился (упрощённо — просто перерисовываем)
            updateNetworkInfoPage(page);
        }

        // Даём возможность фоновым задачам выполняться
        delay(10);  // небольшая задержка, но не блокирующая надолго
    }

    flags.displayLocked = 0;
}

// Вывод информации на экран для конкретной страницы
void updateNetworkInfoPage(int page) {
    lcd.clear();

    switch (page) {
        case 0:
            drawPage1();
            break;
        case 1:
            drawPage2();
            break;
        case 2:
            drawPage3();
            break;
    }

    // Вывод номера страницы в правом нижнем углу
    char pageBuf[10];
    snprintf(pageBuf, sizeof(pageBuf), "Pg %d/%d", page + 1, PAGE_COUNT);
    lcd.setCursor(LCD_WIDTH - strlen(pageBuf), 3);
    lcd.print(pageBuf);
}

void drawPage1() {
    lcd.setCursor(0, 0);
#ifdef ENABLE_WIFI
    if (WiFi.status() == WL_CONNECTED) {
        lcd.print("WiFi: CONNECTED");
    } else {
        lcd.print("WiFi: DISCONNECTED");
    }
#else
    lcd.print("WiFi: DISABLED");
#endif

    lcd.setCursor(0, 1);
#ifdef ENABLE_WIFI
    if (WiFi.status() == WL_CONNECTED) {
        lcd.print("IP: ");
        lcd.print(WiFi.localIP().toString());
    } else {
        lcd.print("IP: N/A");
    }
#else
    lcd.print("IP: N/A");
#endif
}

void drawPage2() {
    lcd.setCursor(0, 0);
    lcd.print("MAC address:");

    // Получение MAC-адреса (48 бит) из 64-битного числа efuse
    uint64_t mac = ESP.getEfuseMac();
    uint8_t macBytes[6];
    // Порядок байт: старший байт (OUI) — первый в строке
    macBytes[0] = (mac >> 40) & 0xFF;
    macBytes[1] = (mac >> 32) & 0xFF;
    macBytes[2] = (mac >> 24) & 0xFF;
    macBytes[3] = (mac >> 16) & 0xFF;
    macBytes[4] = (mac >> 8) & 0xFF;
    macBytes[5] = mac & 0xFF;

    char macBuf[18];
    snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
             macBytes[0], macBytes[1], macBytes[2],
             macBytes[3], macBytes[4], macBytes[5]);

    lcd.setCursor(0, 1);
    lcd.print(macBuf);
}

void drawPage3() {
    lcd.setCursor(0, 0);
    lcd.print("Admin token:");

    String token = preferences.getString(PREF_KEY_ADMIN_TOKEN, "");
    if (token.length() == 0) {
        token = "(not set)";
    }

    // Разбиваем токен для отображения (макс. 2 строки по 20 символов)
    String firstLine = token.substring(0, min(LCD_WIDTH, (int)token.length()));
    String secondLine = "";
    if (token.length() > LCD_WIDTH) {
        secondLine = token.substring(LCD_WIDTH, min(LCD_WIDTH * 2, (int)token.length()));
        // Если токен ещё длиннее, добавляем многоточие в конце второй строки
        if (token.length() > LCD_WIDTH * 2) {
            if (secondLine.length() >= 3) {
                secondLine = secondLine.substring(0, secondLine.length() - 3) + "...";
            } else {
                secondLine += "...";
            }
        }
    }

    lcd.setCursor(0, 1);
    lcd.print(firstLine);
    lcd.setCursor(0, 2);
    lcd.print(secondLine);

    lcd.setCursor(0, 3);
    lcd.print("Edit via Web UI");
}

// Admin token editing removed from on-device menu; managed via Web UI/API now.
// (Previously editAdminToken() was here.)

// ============ СБРОС К ЗАВОДСКИМ ============

void performFactoryReset() {
  flags.displayLocked = 1;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("!! FACTORY RESET !!");
  lcd.setCursor(0, 1);
  lcd.print("All settings will be");
  lcd.setCursor(0, 2);
  lcd.print("Erased! Click=OK");
  lcd.setCursor(0, 3);
  lcd.print("Turn encoder=cancel");
  
  unsigned long startTime = millis();
  
  while (millis() - startTime < 10000) {
    eEncoderState action = getEncoderState();
    
    if (action == eButton) {
      // Подтверждение - сбрасываем всё
      preferences.clear();
      
      // Восстанавливаем настройки по умолчанию
      safetySettings.timeoutFilling = DEFAULT_TIMEOUT_FILLING;
      safetySettings.timeoutOzonation = DEFAULT_TIMEOUT_OZONATION;
      safetySettings.timeoutAeration = DEFAULT_TIMEOUT_AERATION;
      safetySettings.timeoutSettling = DEFAULT_TIMEOUT_SETTLING;
      safetySettings.timeoutFiltration = DEFAULT_TIMEOUT_FILTRATION;
      safetySettings.timeoutBackwash = DEFAULT_TIMEOUT_BACKWASH;
      safetySettings.engineerPassword = ENGINEER_PASSWORD;
      safetySettings.watchdogEnabled = false;
      safetySettings.watchdogTimeout = WDT_TIMEOUT_SECONDS;
      
      saveSafetySettings();
      
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("  RESET COMPLETE!");
      lcd.setCursor(0, 2);
      lcd.print("  Rebooting...");
      delay(2000);
      
      ESP.restart();
    } else if (action == eLeft || action == eRight) {
      // Отмена
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("    CANCELLED");
      delay(1500);
      flags.displayLocked = 0;
      return;
    }
    
    delay(50);
  }
  
  // Таймаут - отмена
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("    CANCELLED");
  delay(1500);
  flags.displayLocked = 0;
}

// ============ СОХРАНЕНИЕ/ЗАГРУЗКА ============

void saveSafetySettings() {
  preferences.putUShort("sf_fill", safetySettings.timeoutFilling);
  preferences.putUShort("sf_ozon", safetySettings.timeoutOzonation);
  preferences.putUShort("sf_aer", safetySettings.timeoutAeration);
  preferences.putUShort("sf_setl", safetySettings.timeoutSettling);
  preferences.putUShort("sf_filt", safetySettings.timeoutFiltration);
  preferences.putUShort("sf_bw", safetySettings.timeoutBackwash);
  preferences.putUShort("sf_pump_to", safetySettings.pumpDryTimeoutSeconds);
  preferences.putUChar("sf_pump_delta", safetySettings.pumpMinLevelDeltaCm);
  preferences.putUChar("sf_pump_cons", safetySettings.pumpDryConsecutiveChecks);
  preferences.putULong("sf_pwd", safetySettings.engineerPassword);
  preferences.putUChar("sf_wdt_en", safetySettings.watchdogEnabled);
  preferences.putUChar("sf_wdt_to", safetySettings.watchdogTimeout);

  // Sensor filter settings
  // sensor filter settings removed; keep poll period only
  preferences.putUShort("sf_sen_poll", safetySettings.sensorPollPeriod);

}

void loadSafetySettings() {
  safetySettings.timeoutFilling = preferences.getUShort("sf_fill", DEFAULT_TIMEOUT_FILLING);
  safetySettings.timeoutOzonation = preferences.getUShort("sf_ozon", DEFAULT_TIMEOUT_OZONATION);
  safetySettings.timeoutAeration = preferences.getUShort("sf_aer", DEFAULT_TIMEOUT_AERATION);
  safetySettings.timeoutSettling = preferences.getUShort("sf_setl", DEFAULT_TIMEOUT_SETTLING);
  safetySettings.timeoutFiltration = preferences.getUShort("sf_filt", DEFAULT_TIMEOUT_FILTRATION);
  safetySettings.timeoutBackwash = preferences.getUShort("sf_bw", DEFAULT_TIMEOUT_BACKWASH);
  safetySettings.engineerPassword = preferences.getULong("sf_pwd", ENGINEER_PASSWORD);
  safetySettings.watchdogEnabled = preferences.getUChar("sf_wdt_en", false);
  safetySettings.watchdogTimeout = preferences.getUChar("sf_wdt_to", WDT_TIMEOUT_SECONDS);
  safetySettings.pumpDryTimeoutSeconds = preferences.getUShort("sf_pump_to", DEFAULT_PUMP_DRY_TIMEOUT_SECONDS);
  safetySettings.pumpMinLevelDeltaCm = preferences.getUChar("sf_pump_delta", DEFAULT_PUMP_MIN_LEVEL_DELTA_CM);
  safetySettings.pumpDryConsecutiveChecks = preferences.getUChar("sf_pump_cons", DEFAULT_PUMP_DRY_CONSECUTIVE_CHECKS);

  // Sensor filter settings
  safetySettings.sensorPollPeriod = preferences.getUShort("sf_sen_poll", DEFAULT_SENSOR_POLL_PERIOD);
  

}
