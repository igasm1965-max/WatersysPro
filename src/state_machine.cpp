/**
 * @file state_machine.cpp
 * @brief Реализация автомата состояний системы
 * @details Функции обработки переходов состояний и управления циклом обработки воды
 */

#include "state_machine.h"

#include "config.h"
#include "emergency.h"
#include "structures.h"
#include "utils.h"

// FSM state accessed via `systemContext` (no module-level aliases).

// ============ ВСПОМОГАТЕЛЬНЫЕ ПРОВЕРКИ ============

static bool isValidTransition(SystemState from, SystemState to) {
    if (from == to) return true;
    if (to == STATE_IDLE) return true;

    switch (from) {
        case STATE_IDLE:
            return (to == STATE_FILLING_TANK1 || to == STATE_FILTRATION || to == STATE_BACKWASH);
        case STATE_FILLING_TANK1:
            return (to == STATE_OZONATION || to == STATE_IDLE);
        case STATE_OZONATION:
            return (to == STATE_AERATION || to == STATE_IDLE);
        case STATE_AERATION:
            return (to == STATE_SETTLING || to == STATE_IDLE);
        case STATE_SETTLING:
            return (to == STATE_FILTRATION || to == STATE_IDLE);
        case STATE_FILTRATION:
            return (to == STATE_IDLE);
        case STATE_BACKWASH:
            return (to == STATE_IDLE || to == STATE_FILTRATION);
        default:
            return false;
    }
}

// Centralized state transition helper — applies outputs, updates timestamps and logs.
void changeState(SystemState newState) {
    // FSM state uses `systemContext` explicitly.

    if (newState == systemContext.currentState) return;

    // Validate transition
    if (!isValidTransition(systemContext.currentState, newState)) {
        saveEventLog(LOG_WARNING, EVENT_UNEXPECTED_STATE_CHANGE, (uint16_t)newState);
    }

    Serial.printf("[FSM] State change: %d -> %d\n", systemContext.currentState, newState);
    systemContext.previousState = systemContext.currentState;
    systemContext.currentState = newState;

    // Apply outputs immediately and set start time
    applyStateOutputs(systemContext.currentState);
    systemContext.stateStartTime = millis();

    // Log state change
    saveEventLog(LOG_INFO, EVENT_SYSTEM_STATE_CHANGE, (uint16_t)systemContext.currentState);
}

void applyStateOutputs(SystemState state) {
    extern void turnOffAllRelays();
    extern void turnOnPump1();
    extern void turnOffPump1();
    extern void turnOnPump2();
    extern void turnOffPump2();
    extern void turnOnOzone();
    extern void turnOffOzone();
    extern void turnOnAeration();
    extern void turnOffAeration();
    extern void turnOnFilter();
    extern void turnOffFilter();
    extern void turnOnBackwash();
    extern void turnOffBackwash();

    switch (state) {
        case STATE_IDLE:
            turnOffAllRelays();
            break;
        case STATE_FILLING_TANK1:
            turnOnPump1();
            turnOffPump2();
            turnOffOzone();
            turnOffAeration();
            turnOffFilter();
            turnOffBackwash();
            break;
        case STATE_OZONATION:
            turnOffPump1();
            turnOffPump2();
            turnOnOzone();
            turnOnAeration();
            turnOffFilter();
            turnOffBackwash();
            break;
        case STATE_AERATION:
            turnOffPump1();
            turnOffPump2();
            turnOffOzone();
            turnOnAeration();
            turnOffFilter();
            turnOffBackwash();
            break;
        case STATE_SETTLING:
            turnOffAllRelays();
            break;
        case STATE_FILTRATION:
            turnOffPump1();
            turnOnPump2();
            turnOffOzone();
            turnOffAeration();
            turnOnFilter();
            turnOffBackwash();
            break;
        case STATE_BACKWASH:
            turnOffPump1();
            turnOnPump2();
            turnOffOzone();
            turnOffAeration();
            turnOffFilter();
            turnOnBackwash();
            break;
        default:
            break;
    }
}

// ============ ОБРАБОТКА СОСТОЯНИЙ ============

/// Обрабатывает состояние IDLE (ожидание)
void handleIdleState() {
    // Все реле выключены
}

/// Обрабатывает состояние FILLING (заполнение)
void handleFillingState() {
    // Включаем первый насос для заполнения
}

/// Обрабатывает состояние OZONATION (озонация)
void handleOzonationState() {
    // Включаем озонатор, фильтр и второй насос
}

/// Обрабатывает состояние AERATION (аэрация)
void handleAerationState() {
    // Включаем аэратор и фильтр
}

/// Обрабатывает состояние SETTLING (отстаивание)
void handleSettlingState() {
    // Все выключено, ждем осаждения частиц
}

/// Обрабатывает состояние FILTRATION (фильтрация)
void handleFiltrationState() {
    // Включаем фильтр
}

/// Обрабатывает состояние BACKWASH (промывка фильтра)
void handleBackwashState() {
    // Включаем обратную промывку
}

// ============ ПЕРЕХОДЫ СОСТОЯНИЙ ============

/// Переходит в состояние IDLE
void transitionToIdle() {
    // Выключаем все реле
}

/// Переходит в состояние FILLING
void transitionToFilling() {
    // Проверяем, что бак не полный
}

/// Переходит в состояние OZONATION
void transitionToOzonation() {
    // Проверяем, что бак полный
}

/// Переходит в состояние AERATION
void transitionToAeration() {
    // Проверяем озонацию завершена
}

/// Переходит в состояние SETTLING
void transitionToSettling() {
    // Выключаем все активные процессы
}

/// Переходит в состояние FILTRATION
void transitionToFiltration() {
    // Проверяем, что вода готова
}

/// Переходит в состояние BACKWASH
void transitionToBackwash() {
    // Проверяем загрязненность фильтра
}

// ============ ОСНОВНОЙ ЦИКЛ АВТОМАТА ============

/// Обрабатывает автоматическое управление
void processAutomaticControl() {
    extern SystemFlags flags;
    // FSM globals are accessed via `systemContext` (module-level aliases above)
    extern int tank1Level, tank2Level;
    extern TankParams tank1, tank2;
    extern uint32_t currentOzonationRemaining;
    extern uint32_t currentAerationRemaining;
    extern uint32_t currentSetlingRemaining;
    extern uint32_t currentBackwashRemaining;
    extern unsigned long filterOperationStartTime;
    extern bool nightPauseActive;
    extern char emergencyMessage[41];
    extern uint32_t ozonationDuration;
    extern uint32_t aerationDuration;
    extern uint32_t setlingDuration;
    extern uint32_t filterWashDuration;
    extern SafetySettings safetySettings;  // Настройки таймаутов безопасности

    // Sensor values are updated in loopESP32() by configurable sensorPollPeriod.
    // FSM consumes the latest cached values here without forcing extra reads.

    // Внешние функции
    extern void turnOffAllRelays();
    extern void turnOnPump1();
    extern void turnOffPump1();
    extern void turnOnPump2();
    extern void turnOffPump2();
    extern void turnOnOzone();
    extern void turnOffOzone();
    extern void turnOnAeration();
    extern void turnOffAeration();
    extern void turnOnFilter();
    extern void turnOffFilter();
    extern void turnOnBackwash();
    extern void turnOffBackwash();
    extern void saveEventLog(LogLevel level, uint8_t eventType, uint16_t param);
    extern bool isWithinSchedule();
    extern void startPumpEmergencyMonitoring();
    extern void stopPumpEmergencyMonitoring();
    extern void startPump2EmergencyMonitoring();
    extern void stopPump2EmergencyMonitoring();
    extern void exitBackwashScreen();
    extern void saveBackwashTimestamp();

    // В ручном режиме автоматика полностью отключена, вмешательство FSM запрещено.
    if (flags.manualMode) {
        if (systemContext.currentState != STATE_IDLE) {
            turnOffAllRelays();
            changeState(STATE_IDLE);
        }
        if (flags.waterTreatmentInProgress || flags.backwashInProgress) {
            flags.waterTreatmentInProgress = 0;
            flags.backwashInProgress = 0;
        }
        return;
    }

    // Аварийный режим: просто выключаем всё
    if (flags.emergencyMode) {
        Serial.println("[FSM] BLOCKED by emergencyMode!");
        static unsigned long lastEmergencyCheck = 0;
        if (hasIntervalElapsed(lastEmergencyCheck, 1000)) {
            lastEmergencyCheck = millis();
            turnOffAllRelays();
        }
        return;
    }

    // Ночная тишина: блокируем всё, кроме завершения наполнения бака1
    // Если цикл обработки уже в прогрессе — НЕ останавливаем его (чтобы не прерывать включённые
    // реле)
    if (nightPauseActive && systemContext.currentState != STATE_FILLING_TANK1 &&
        !flags.waterTreatmentInProgress) {
        turnOffAllRelays();
        changeState(STATE_IDLE);
        flags.waterTreatmentInProgress = 0;
        flags.backwashInProgress = 0;
        return;
    }

    static SystemState lastState = STATE_IDLE;
    static bool firstRun = true;
    unsigned long currentTime = millis();

    // Отслеживаем смену состояния (для логирования; выходы уже применены в changeState)
    if (systemContext.currentState != lastState || firstRun) {
        Serial.printf("[FSM] State detected: %d -> %d\n", lastState, systemContext.currentState);
        lastState = systemContext.currentState;
        firstRun = false;
    }

    // Проверка расписания: если сейчас нельзя работать - переходим в IDLE
    // Если цикл обработки уже запущен (waterTreatmentInProgress) — не прерываем его даже при
    // расписании
    if (flags.scheduleEnabled && !isWithinSchedule()) {
        if (!flags.waterTreatmentInProgress) {
            if (systemContext.currentState != STATE_IDLE &&
                systemContext.currentState != STATE_BACKWASH) {
                turnOffAllRelays();
                changeState(STATE_IDLE);
                flags.waterTreatmentInProgress = 0;
            }
            return;
        } else {
            // разрешаем продолжить текущий цикл — ничего не делаем
        }
    }

    // Основной switch по состояниям автомата
    Serial.printf(
        "[FSM] currentState=%d, tank1Level=%d, tank2Level=%d, tank1Empty=%d, tank2Empty=%d\n",
        systemContext.currentState, tank1Level, tank2Level, flags.tank1Empty, flags.tank2Empty);
    // applyStateOutputs вызывается выше только при смене состояния
    switch (systemContext.currentState) {
        case STATE_IDLE:
            // Защита: если waterTreatmentInProgress остался =1, но мы в IDLE — сбрасываем
            if (flags.waterTreatmentInProgress) {
                Serial.println("[FSM] WARNING: waterTreatmentInProgress=1 in IDLE! Clearing.");
                saveEventLog(LOG_WARNING, EVENT_UNEXPECTED_STATE_CHANGE, 0);
                flags.waterTreatmentInProgress = 0;
            }
            if (flags.backwashInProgress) {
                Serial.println("[FSM] WARNING: backwashInProgress=1 in IDLE! Clearing.");
                flags.backwashInProgress = 0;
            }

            // Ночью из IDLE ничего нового не запускаем
            if (nightPauseActive) {
                break;
            }

            // Приоритет 1: Промывка фильтра (если требуется)
            if (flags.filterCleaningNeeded && !flags.tank1Empty &&
                !flags.waterTreatmentInProgress && !flags.backwashInProgress) {
                if (tank1Level <= tank1.minLevel) {  // Уровень достаточен (бак полон)
                    changeState(STATE_BACKWASH);
                    currentBackwashRemaining =
                        filterWashDuration;  // Инициализируем таймер промывки
                    flags.backwashInProgress = 1;
                    saveEventLog(LOG_INFO, 15, 0);  // EVENT_FILTER_WASH_START
                    break;
                }
            }

            // Приоритет 2: Наполнение бака 1
            if (!flags.waterTreatmentInProgress) {
                if (flags.tank1Empty) {
                    if (nightPauseActive) break;

                    changeState(STATE_FILLING_TANK1);
                    startPumpEmergencyMonitoring();
                }
                // Приоритет 3: Фильтрация
                else if (flags.tank2Empty && !flags.tank1Empty) {
                    if (nightPauseActive) break;

                    changeState(STATE_FILTRATION);
                    if (flags.firstFiltrationCycle) {
                        filterOperationStartTime =
                            systemContext
                                .stateStartTime;  // stateStartTime already set by changeState
                        flags.firstFiltrationCycle = 0;
                    }
                    startPump2EmergencyMonitoring();
                }
            }
            break;

        case STATE_FILLING_TANK1:
            // Заполнение завершено - переход к озонированию (бак полон = расстояние минимально)
            if (tank1Level <= tank1.minLevel) {
                stopPumpEmergencyMonitoring();
                changeState(STATE_OZONATION);
                currentOzonationRemaining = ozonationDuration;  // Инициализируем таймер озонации
                flags.waterTreatmentInProgress = 1;
            }
            // Таймаут заполнения - авария (настраиваемый из инженерного меню)
            else if (currentTime - systemContext.stateStartTime >
                     (unsigned long)safetySettings.timeoutFilling * 60UL * 1000UL) {
                stopPumpEmergencyMonitoring();
                changeState(STATE_IDLE);
                flags.waterTreatmentInProgress = 0;
                triggerEmergency("FILLING TIMEOUT");
            }
            break;

        case STATE_OZONATION:
            // Озонация завершена - переход к аэрации
            if (currentOzonationRemaining == 0) {
                changeState(STATE_AERATION);
                currentAerationRemaining = aerationDuration;  // Инициализируем таймер аэрации
            }
            // Таймаут безопасности (настраиваемый)
            else if (currentTime - systemContext.stateStartTime >
                     (unsigned long)safetySettings.timeoutOzonation * 60UL * 1000UL) {
                changeState(STATE_IDLE);
                flags.waterTreatmentInProgress = 0;
                triggerEmergency("OZONATION TIMEOUT");
            }
            break;

        case STATE_AERATION:
            // Аэрация завершена - переход к отстаиванию
            if (currentAerationRemaining == 0) {
                changeState(STATE_SETTLING);
                currentSetlingRemaining = setlingDuration;  // Инициализируем таймер отстаивания
            }
            // Таймаут безопасности (настраиваемый)
            else if (currentTime - systemContext.stateStartTime >
                     (unsigned long)safetySettings.timeoutAeration * 60UL * 1000UL) {
                changeState(STATE_IDLE);
                flags.waterTreatmentInProgress = 0;
                triggerEmergency("AERATION TIMEOUT");
            }
            break;

        case STATE_SETTLING:
            // Отстаивание завершено
            if (currentSetlingRemaining == 0) {
                flags.waterTreatmentInProgress = 0;

                // Проверка необходимости промывки фильтра
                if (flags.filterCleaningNeeded && !flags.tank1Empty && !flags.backwashInProgress) {
                    if (tank1Level <= tank1.minLevel) {  // Уровень достаточен (бак полон)
                        // Проверяем уровень бака 2 перед промывкой
                        if (tank2Level > tank2.minLevel) {
                            // Бак 2 не полный - наполняем его перед промывкой
                            changeState(STATE_FILTRATION);
                            filterOperationStartTime = systemContext.stateStartTime;
                            flags.firstFiltrationCycle = 0;
                            startPump2EmergencyMonitoring();
                        } else {
                            // Бак 2 уже полный - сразу запускаем промывку
                            changeState(STATE_BACKWASH);
                            currentBackwashRemaining = filterWashDuration;
                            flags.backwashInProgress = 1;
                            saveEventLog(LOG_INFO, 15, 0);  // EVENT_FILTER_WASH_START
                        }
                    }
                }
                // Иначе переход к фильтрации (если нужна)
                else if (flags.tank2Empty && !flags.tank1Empty) {
                    changeState(STATE_FILTRATION);
                }
                // Иначе в IDLE
                else {
                    changeState(STATE_IDLE);
                }
            }
            // Таймаут безопасности (настраиваемый)
            else if (currentTime - systemContext.stateStartTime >
                     (unsigned long)safetySettings.timeoutSettling * 60UL * 1000UL) {
                changeState(STATE_IDLE);
                flags.waterTreatmentInProgress = 0;
                triggerEmergency("SETTLING TIMEOUT");
            }
            break;

        case STATE_FILTRATION: {
            // Фильтрация завершена: бак 1 опустел или бак 2 заполнен
            if (tank1Level >= tank1.maxLevel || tank2Level <= tank2.minLevel) {
                stopPump2EmergencyMonitoring();
                
                // Если требуется промывка и бак 2 полный - переходим в BACKWASH
                // ПРИМЕЧАНИЕ: НЕ проверяем уровень бака 1, так как он естественно опустошается во время фильтрации
                if (flags.filterCleaningNeeded && !flags.backwashInProgress && 
                    tank2Level <= tank2.minLevel) {
                    // Бак 2 полный и нужна промывка - запускаем её
                    changeState(STATE_BACKWASH);
                    currentBackwashRemaining = filterWashDuration;
                    flags.backwashInProgress = 1;
                    saveEventLog(LOG_INFO, 15, 0);  // EVENT_FILTER_WASH_START
                } else {
                    changeState(STATE_IDLE);
                }
            }
            // Таймаут безопасности (настраиваемый)
            else if (currentTime - systemContext.stateStartTime >
                     (unsigned long)safetySettings.timeoutFiltration * 60UL * 1000UL) {
                stopPump2EmergencyMonitoring();
                changeState(STATE_IDLE);
                triggerEmergency("FILTRATION TIMEOUT");
            }
        } break;

        case STATE_BACKWASH:
            // Проверка на сухой ход - если бак 2 опустел ДО завершения промывки (опасно для фильтра)
            if (tank2Level <= tank2.minLevel && !flags.tank2Empty && currentBackwashRemaining > 10) {
                stopPump2EmergencyMonitoring();
                
                flags.filterCleaningNeeded = 0;
                flags.backwashInProgress = 0;
                flags.backwashDryRunError = 1;
                
                saveEventLog(LOG_WARNING, EVENT_BACKWASH_DRY_RUN, 0);
                // Остаемся на экране backwash для показа ошибки
            }
            // Промывка завершена: по таймеру или бак 2 опустел
            else if ((currentBackwashRemaining == 0) || tank2Level >= tank2.maxLevel) {
                stopPump2EmergencyMonitoring();

                flags.filterCleaningNeeded = 0;
                flags.backwashInProgress = 0;
                flags.backwashDryRunError = 0;
                filterOperationStartTime = millis();
                flags.firstFiltrationCycle = 1;

                // Сохраняем timestamp промывки для восстановления при перезагрузке
                saveBackwashTimestamp();

                exitBackwashScreen();
                flags.backwashScreenInitialized = 0;

                // После промывки сразу запускаем фильтрацию (если возможно)
                if (flags.tank2Empty && !flags.tank1Empty) {
                    changeState(STATE_FILTRATION);
                    filterOperationStartTime =
                        systemContext.stateStartTime;  // stateStartTime already set by changeState
                    flags.firstFiltrationCycle = 0;
                } else {
                    changeState(STATE_IDLE);
                }
            }
            // Таймаут безопасности (настраиваемый)
            else if (currentTime - systemContext.stateStartTime >
                     (unsigned long)safetySettings.timeoutBackwash * 60UL * 1000UL) {
                stopPump2EmergencyMonitoring();
                changeState(STATE_IDLE);
                flags.backwashInProgress = 0;
                triggerEmergency("BACKWASH TIMEOUT");
            }
            break;
    }
}

/// Обновляет обратный отсчет таймеров
void updateCountdownTimers() {
    extern uint32_t currentOzonationRemaining;
    extern uint32_t currentAerationRemaining;
    extern uint32_t currentSetlingRemaining;
    extern uint32_t currentBackwashRemaining;

    // Уменьшаем каждый активный таймер на 1 секунду
    if (currentOzonationRemaining > 0) currentOzonationRemaining--;
    if (currentAerationRemaining > 0) currentAerationRemaining--;
    if (currentSetlingRemaining > 0) currentSetlingRemaining--;
    if (currentBackwashRemaining > 0) currentBackwashRemaining--;
}

/// Обновляет время работы фильтра и проверяет необходимость промывки
void updateFilterOperationTime() {
    extern SystemFlags flags;
    extern unsigned long filterOperationStartTime;
    extern uint32_t filterCleaningInterval;
    // FSM state accessed via `systemContext`

    if (filterOperationStartTime != 0 && !flags.filterCleaningNeeded && !flags.backwashInProgress &&
        systemContext.currentState != STATE_BACKWASH) {
        unsigned long cleaningPeriodMs = filterCleaningInterval * 1000UL;
        if (millis() - filterOperationStartTime >= cleaningPeriodMs) {
            // Порог достигнут - устанавливаем флаг необходимости промывки
            flags.filterCleaningNeeded = 1;
            flags.blinkState = 1;
        }
    }
}

/// Проверяет условия для смены состояния
void checkStateTransitions() {
    // Проверяем условия переходов между состояниями
}

/// Проверяет достижение целевого состояния
bool isTargetStateReached() {
    // Возвращает true, если цель достигнута
    return false;
}

// ============ УПРАВЛЕНИЕ ФАЗАМИ ОБРАБОТКИ ============

/// Запускает цикл обработки воды
void startWaterTreatment() {
    // Начинаем с озонации (если не требуется заполнение)
}

/// Останавливает цикл обработки воды
void stopWaterTreatment() {
    // Выключаем все реле и переходим в IDLE
}

/// Проверяет завершение текущей фазы
bool isCurrentPhaseComplete() {
    // Возвращает true, если текущая фаза завершена
    return false;
}

/// Выполняет дополнительную очистку воды
void performAdditionalCleanup() {
    // Дополнительная логика очистки
}

// ============ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ============

/// Возвращает текстовое описание состояния
const char* getStateText(SystemState state) {
    switch (state) {
        case STATE_IDLE:
            return "IDLE";
        case STATE_FILLING_TANK1:
            return "FILLING";
        case STATE_OZONATION:
            return "OZONATION";
        case STATE_AERATION:
            return "AERATION";
        case STATE_SETTLING:
            return "SETTLING";
        case STATE_FILTRATION:
            return "FILTRATION";
        case STATE_BACKWASH:
            return "BACKWASH";
        default:
            return "UNKNOWN";
    }
}

/// Возвращает время, оставшееся для текущего состояния
uint32_t getRemainingTime() {
    // Возвращает количество секунд до конца текущей фазы
    return 0;
}

/// Возвращает общее время для текущего состояния
uint32_t getTotalTime() {
    // Возвращает общее время фазы
    return 0;
}

/// Проверяет, завершена ли фильтрация
bool isFilteringComplete() { return false; }

/// Проверяет, завершена ли обработка воды
bool isWaterTreatmentComplete() { return false; }

/// Скипует текущую фазу (ручное управление)
void skipCurrentPhase() {
    // Переходим к следующему состоянию
}

/// Возвращает статус системы (для меню)
int getSystemStatus() {
    // `currentState` accessed via systemContext
    return (int)systemContext.currentState;
}
