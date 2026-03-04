/**
 * @file emergency.cpp
 * @brief Реализация аварийного режима и мониторинга
 * @details Функции обнаружения аварийных ситуаций и их обработки
 */

#include "emergency.h"
#include "config.h"
#include "structures.h"
#include "relay_control.h"
#include "event_logging.h"
#include "encoder.h"
#include "utils.h"
#include "state_machine.h"

// ============ ИНИЦИАЛИЗАЦИЯ АВАРИЙНОГО МОНИТОРИНГА ============

/// Локальные переменные мониторинга насосов (видимы в рамках файла для диагностики)
static bool pumpMonitoringActive[2] = {false, false};
static int  pumpStartLevel[2] = {0, 0};
static unsigned long pumpStartTime[2] = {0, 0};
static int  pumpConsecutiveFails[2] = {0, 0};

/// Инициализирует систему аварийного мониторинга
void initEmergencyMonitoring() {
  extern SystemFlags flags;
  extern char emergencyMessage[41];
  
  flags.emergencyMode = 0;
  emergencyMessage[0] = '\0';
  flags.emergencyScreenInitialized = 0;
  flags.emergencyBlinkState = 1;
  
}

// ============ ОБНАРУЖЕНИЕ АВАРИЙНЫХ СИТУАЦИЙ ============

/// Проверяет аварийные ситуации (сухой ход насосов)
void checkEmergencySituations() {
  extern int tank1Level, tank2Level;
  extern bool getPump1State();
  extern bool getPump2State();

  extern SafetySettings safetySettings;
  const unsigned long timeoutMs = (unsigned long)safetySettings.pumpDryTimeoutSeconds * 1000UL;
  const int minDelta = safetySettings.pumpMinLevelDeltaCm;

  // Generic pump monitoring for pump index 0 (pump1) and 1 (pump2)
  for (int p = 0; p < 2; ++p) {
    bool pumpState = (p == 0) ? getPump1State() : getPump2State();
    int currentLevel = (p == 0) ? tank1Level : tank2Level;
    const char* pumpName = (p == 0) ? "Pump1" : "Pump2";

    if (pumpState) {
      if (!pumpMonitoringActive[p]) {
        pumpMonitoringActive[p] = true;
        pumpStartLevel[p] = currentLevel;
        pumpStartTime[p] = millis();
        pumpConsecutiveFails[p] = 0;
      } else {
        unsigned long elapsedMs = millis() - pumpStartTime[p];
        if (elapsedMs >= timeoutMs) {
          int delta = abs(currentLevel - pumpStartLevel[p]);
          if (delta < minDelta) {
            pumpConsecutiveFails[p]++;
            if (pumpConsecutiveFails[p] >= safetySettings.pumpDryConsecutiveChecks) {
              Serial.printf("[EMERGENCY] %s dry-run detected: start=%d now=%d delta=%d elapsed=%lus timeout=%ds\n", pumpName, pumpStartLevel[p], currentLevel, delta, elapsedMs / 1000, safetySettings.pumpDryTimeoutSeconds);
              saveEventLog(LOG_ERROR, (p == 0) ? EVENT_PUMP1_DRYRUN : EVENT_PUMP2_DRYRUN, (uint8_t)delta);
              triggerEmergency((p == 0) ? "PUMP1 DRY RUN" : "PUMP2 DRY RUN");
              pumpMonitoringActive[p] = false;
            }
          } else {
            if (pumpConsecutiveFails[p] > 0) pumpConsecutiveFails[p] = 0;
          }
        }
      }
    } else {
      if (pumpMonitoringActive[p]) {
        pumpMonitoringActive[p] = false;
        pumpConsecutiveFails[p] = 0;
      }
    }
  }
}

/// Обновляет состояние аварийного мониторинга
void updateEmergencyMonitoring() {
  extern SystemFlags flags;
  extern int tank1Level, tank2Level;
  
  // Сброс watchdog перед проверками
  if (flags.watchdogEnabled) {
    WDT_RESET();
  }
  
  // Периодическая проверка аварийных ситуаций
  static unsigned long lastCheck = 0;
  if (hasIntervalElapsed(lastCheck, 200)) { // Каждые 200 мс (5 раз в секунду)
    lastCheck = millis();
    checkEmergencySituations();
  }
  
  // Диагностический вывод состояния мониторинга каждые 5 секунд (оставляем менее частым, чтобы не засорять лог)
  static unsigned long lastDiag = 0;
  if (hasIntervalElapsed(lastDiag, 5000)) {
    lastDiag = millis();
  }
  
  // Проверяем пустые баки
  if (flags.tank1Empty || flags.tank2Empty) {
    // Это нормальная ситуация, не аварийная
  }
}

/// Проверяет переполнение бака
bool checkOverflow() {
  extern int tank1Level, tank2Level;
  extern TankParams tank1, tank2;
  
  if (tank1Level > tank1.maxLevel || tank2Level > tank2.maxLevel) {
    return true;
  }
  return false;
}

// Removed: unused hardware-check helpers (checkRelayFault/checkOverheating/checkLowVoltage).

// ============ ОБРАБОТКА АВАРИИ ============

/// Обрабатывает аварийный режим
void handleEmergency() {
  extern SystemFlags flags;
  extern char emergencyMessage[41];
  // FSM state accessed via systemContext
  extern void turnOffAllRelays();
  
  if (!flags.emergencyMode) return;

  // Выключаем все реле в аварийном режиме
  turnOffAllRelays();
  changeState(STATE_IDLE);
  flags.waterTreatmentInProgress = 0;
  flags.backwashInProgress = 0;
  flags.inMenu = 0;
  flags.inFilterWashInfo = 0;

  // Включаем подсветку и дисплей для видимости аварии (обновляем и аппаратное состояние)
  activateBacklight();
}

/// Входит в аварийный режим с сообщением
void enterEmergencyMode(const char* message) {
  extern SystemFlags flags;
  extern char emergencyMessage[41];
  
  flags.emergencyMode = true;
  strncpy(emergencyMessage, message, 40);
  emergencyMessage[40] = '\0';
  
  saveEventLog(LOG_ERROR, EVENT_EMERGENCY, 0); // EVENT_EMERGENCY
  Serial.printf("[EMERGENCY] %s\n", message);
  
  handleEmergency();
}

// Совместимая с заголовком обёртка для триггера аварии
void triggerEmergency(const char* message) {
  enterEmergencyMode(message);
}

/// Выходит из аварийного режима
void exitEmergencyMode() {
  extern SystemFlags flags;
  
  flags.emergencyMode = false;
  turnOffAllRelays();
  // Ensure the emergency screen will be reinitialized next time it appears
  flags.emergencyScreenInitialized = 0;
  // Reset global blink state for predictable behavior
  flags.blinkState = 1;
}

/// Проверяет возможность выхода из аварии (при длительном нажатии кнопки)
void checkEmergencyReset() {
  extern SystemFlags flags;
  static unsigned long pressStart = 0;
  static bool pressing = false;
  
  // Сброс watchdog перед долгой операцией
  if (flags.watchdogEnabled) {
    WDT_RESET();
  }

  // --- АВТОМАТИЧЕСКИЙ СБРОС АВАРИИ ---
  // Если условия аварии устранены (например, уровень воды в норме), сбрасываем emergencyMode
  extern int tank1Level, tank2Level;
  extern TankParams tank1, tank2;
  // Пример: если уровень воды в баке 1 и 2 в допустимых пределах — сброс аварии
  if (flags.emergencyMode) {
    // Use inclusive bounds to consider exact min/max as OK
    bool tank1Ok = (tank1Level >= tank1.minLevel && tank1Level <= tank1.maxLevel);
    bool tank2Ok = (tank2Level >= tank2.minLevel && tank2Level <= tank2.maxLevel);
    if (tank1Ok && tank2Ok) {
      exitEmergencyMode();
      Serial.println("[EMERGENCY] Auto-reset: levels normal");
    }
  }

  // --- РУЧНОЙ СБРОС (долгое нажатие) ---
  if (isEncoderButtonPressed()) {
    // Избегаем ложных срабатываний сразу после поворота энкодера
    extern volatile unsigned long lastEncoderInterrupt;
    unsigned long now = millis();
    if (now - lastEncoderInterrupt < 200) {
      // ignore presses too soon after rotation
      // don't start pressing timer
    } else {
      if (!pressing) {
        pressing = true;
        pressStart = now;
      } else if (now - pressStart >= 3000) {
        // Длительное нажатие 3 сек — сброс аварии
        exitEmergencyMode();
        pressing = false;
        pressStart = 0;
        Serial.println("[EMERGENCY] Reset by long press");
      }
    }
  } else {
    pressing = false;
    pressStart = 0;
  }
}

// ============ МОНИТОРИНГ НАСОСОВ ============

/// Запускает аварийный мониторинг насоса 1
void startPumpEmergencyMonitoring() {
  extern int tank1Level;
  if (pumpMonitoringActive[0]) return;
  pumpMonitoringActive[0] = true;
  pumpStartLevel[0] = tank1Level;
  pumpStartTime[0] = millis();
  pumpConsecutiveFails[0] = 0;
}

/// Останавливает аварийный мониторинг насоса 1
void stopPumpEmergencyMonitoring() {
  pumpMonitoringActive[0] = false;
}

/// Запускает аварийный мониторинг насоса 2
void startPump2EmergencyMonitoring() {
  extern int tank2Level;
  if (pumpMonitoringActive[1]) return;
  pumpMonitoringActive[1] = true;
  pumpStartLevel[1] = tank2Level;
  pumpStartTime[1] = millis();
  pumpConsecutiveFails[1] = 0;
}

/// Останавливает аварийный мониторинг насоса 2
void stopPump2EmergencyMonitoring() {
  pumpMonitoringActive[1] = false;
}

// ============ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ============

/// Возвращает текст аварийного сообщения
const char* getEmergencyMessage() {
  extern char emergencyMessage[41];
  return emergencyMessage;
}

/// Устанавливает текст аварийного сообщения
void setEmergencyMessage(const char* message) {
  extern char emergencyMessage[41];
  strncpy(emergencyMessage, message, 40);
  emergencyMessage[40] = '\0';
}

/// Проверяет, находимся ли мы в аварийном режиме
bool isInEmergencyMode() {
  extern SystemFlags flags;
  return flags.emergencyMode;
}

// Removed: getLastErrorCode()/clearErrorCode() — not referenced.

// ============ ЛОГИРОВАНИЕ АВАРИЙ ============

/// Логирует аварийное событие
void logEmergencyEvent(uint16_t errorCode, uint32_t context) {
  saveEventLog(LOG_ERROR, EVENT_EMERGENCY, (uint16_t)errorCode);
}

// Removed: printEmergencyStats()/exportEmergencyLogs() — use central event log export.

/// Сбрасывает аварию (публичная функция, совместимая с заголовком)
void resetEmergency() {
  exitEmergencyMode();
  saveEventLog(LOG_INFO, EVENT_EMERGENCY, 1); // param=1 -> вручную сброшено
}

// ============ ЗАЩИТА ОТ ЗАВИСАНИЯ ============

/// Проверяет зависание системы
void checkForHang() {
  extern unsigned long loopStartTime, hangDetectionTime;
  
  // Если loop() не обновляется более 5 секунд, это зависание
  unsigned long currentTime = millis();
  if (currentTime - loopStartTime > 5000) {
    // Возможно зависание - требуется watchdog сброс
  }
}

// Removed: setHangTimeout()/sendHeartbeat() — unused in release.
