# План улучшений мобильного приложения WatersysPro

Дата: 2026-09-08
Цель: комплексное улучшение mobile-клиента — безопасность, типизация, рефакторинг, чистка мёртвого кода, локализация, синхронизация версий.

## Контекст

Мобильное приложение: React Native / Expo (`expo ~55`, `react-native 0.83`, TypeScript, React Navigation, react-native-paper). Архитектура на данный момент:

- `App.tsx` — корень, PIN-гейт, Tab-навигация (Главная / Логи / Настройки)
- `src/screens/*` — DashboardScreen, LogsScreen, SettingsScreen, LoginScreen (мёртвый), PinScreen
- `src/services/*` — api.ts, websocket.ts (мёртвый), mqttClient.ts (самописный MQTT v3.1.1), mqttService.ts (singleton), secureStorage.ts
- `src/context/AppContext.ts` — глобальный контекст (lockApp, deviceIp)

---

## Блок 1. Безопасность (критично)

### 1.1 Вынести PIN-код в SecureStore
- Файл: `src/screens/PinScreen.tsx`
- `AsyncStorage.setItem(PIN_KEY, ...)` → `SecureStore.setItemAsync(PIN_KEY, ...)`
- `AsyncStorage.getItem(PIN_KEY)` → `SecureStore.getItemAsync(PIN_KEY)`
- Импорт `import * as SecureStore from 'expo-secure-store'`

### 1.2 Проверка старого PIN при смене + защита от brute-force
- Файл: `src/screens/PinScreen.tsx`, `App.tsx`
- `lockApp` (App.tsx) — сейчас просто удаляет PIN. Изменить флоу:
  - Добавить режим `'change'`: сначала ввести старый PIN, затем дважды новый.
  - Только после успешной проверки старого PIN разрешить установку нового.
- Защита от подбора:
  - Хранить счётчик неудачных попыток и timestamp блокировки (в SecureStore).
  - После N неудач (например, 5) — блокировать ввод на 30–60 с (с экраном таймера), либо инкрементальная задержка между попытками.
  - После блокировки требовать повторную авторизацию.

### 1.3 Cleartext трафик к локальному устройству
- Файл: `app.json`, возможно `android/app/src/main/res/xml/network_security_config.xml`
- Проблема: `usesCleartextTraffic: false` блокирует прямые HTTP-запросы к `http://<deviceIp>` (fetchDeviceLogsDirect, openWebInterface).
- Решение: добавить `networkSecurityConfig`, разрешающий cleartext только для приватных диапазонов локальной сети (192.168.x.x, 10.x.x.x, 172.16-31.x.x), либо включить `usesCleartextTraffic: true` с осознанным риском. Предпочтителен первый вариант.
- Также согласовать прямой HTTP-доступ с фактом использования WSS для брокера.

---

## Блок 2. Типизация данных

### 2.1 Ввести интерфейсы вместо `any`
- Новый файл `src/types/index.ts` (или `src/types.ts`) с интерфейсами:
  - `Telemetry` — поля телеметрии (device_id, ip, tank1, tank2, uptime, state, control_mode, relays, timers, emergency, flags)
  - `SystemState` — enum-значения состояний
  - `RelayState` / `TimersState` / `EmergencyState`
  - `LogEvent` — `{ ts, type, level, desc }`
  - `Settings` — serverUrl, adminToken, notifications, autoFlush, mqttBroker, mqttPort, mqttUser, mqttPass
- Применить в: `mqttService.ts` (normalizeTelemetry, колбэки), `DashboardScreen.tsx` (status), `LogsScreen.tsx` (события), `api.ts` (ответы), `SettingsScreen.tsx`
- Заменить `let iconName: any` в `App.tsx` на типизированный маппинг иконок.

---

## Блок 3. Рефакторинг

### 3.1 Централизовать управление MQTT-подключением
- Файл: `mqttService.ts`, `DashboardScreen.tsx`, `LogsScreen.tsx`
- Проблема: 3 точки вызова `connect()` (useFocusEffect, useEffect, onConnectionChange) и размазанная логика.
- Решение: сделать MqttService единственным владельцем жизненного цикла подключения:
  - Убрать лишние вызовы `connect()` из экранов (оставить один, например в `App.tsx` при входе).
  - Экспонировать простые методы `connect()`, `disconnect()`, `reconnect()`, `get connected`.
  - Экраны только подписываются на события (telemetry/logs/connection) и отписываются.
- Устранить гонку в `reconnect()` (setTimeout на 500 мс) — дождаться закрытия старого клиента.

### 3.2 Устранить повторяющийся Alert аварии
- Файл: `DashboardScreen.tsx`
- `prevEmergencyRef` сбрасывается на `false` при каждом монтировании → повторный Alert.
- Решение: baseline брать из последнего полученного статуса; показывать Alert только на фронте перехода `false → true` в рамках текущей сессии MQTT, не при монтировании.

### 3.3 Оптимизировать ре-рендер Dashboard
- Файл: `DashboardScreen.tsx`
- `useCurrentDateTime` с `setInterval(1000)` перерисовывает весь экран каждую секунду.
- Решение: вынести часы/дату в отдельный компонент `Clock.tsx`, который перерисовывается сам; остальной экран не перерисовывать по таймеру.

### 3.4 Вынести стили в StyleSheet + централизованная тема
- Файл: `DashboardScreen.tsx`, `SettingsScreen.tsx`, `LogsScreen.tsx`
- Заменить инлайн-стили на `StyleSheet.create`.
- Новый файл `src/theme.ts` с палитрой (цвета уже используются: #2196F3, #2e7d32, #c62828, #f5f5f5 и т.д.) и константами отступов/радиусов.
- Единые цвета кнопок реле, статусов, аварий.

---

## Блок 4. Чистка мёртвого кода

### 4.1 Удалить/пометить неиспользуемые модули
- `src/services/websocket.ts` — socket.io-сервис нигде не используется → удалить (и пакет `socket.io-client` из package.json, если не нужен).
- `src/screens/LoginScreen.tsx` — не подключён → удалить, ИЛИ довести до ума и подключить JWT-поток. По умолчанию — удалить (приложение работает на локальном PIN).
- npm-пакет `mqtt` — самописный клиент `mqttClient.ts` уже используется; проверить, используется ли где-то пакет `mqtt`, если нет — удалить из зависимостей.
- `ws-shim.js` — проверить, нужен ли (для Metro); если нет — удалить.
- `api.ts` методы `login/register/getDevices/...` — оставить, но отметить как неиспользуемые основным потоком (используются при будущем облачном подключении).

---

## Блок 5. Локализация и диагностика

### 5.1 Единообразная локализация (RU)
- Файлы: `SettingsScreen.tsx`, `LoginScreen.tsx` (если остаётся)
- «Settings saved» → «Настройки сохранены»; привести все строки UI к русскому.
- Вынести пользовательские строки в константы/строковый модуль (по желанию).

### 5.2 Убрать диагностические console.log из продакшн
- Файлы: `mqttService.ts`, `mqttClient.ts`, `LogsScreen.tsx`, `api.ts`, `DashboardScreen.tsx`
- Удалить теги `[MQTT-DIAG]`, `[LOGS-DIAG]`, `[API]`, `[MqttWs]` либо обернуть в `__DEV__`-гард (log только в development).

---

## Блок 6. Версии и конфигурация

### 6.1 Синхронизировать версии
- `app.json` version = 1.1.0, `package.json` version = 0.1.0.
- Привести к единому значению (например, 1.1.0) или согласовать с корневым `VERSION`.
- Обновить `versionCode` в `app.json` соответственно.

---

## Порядок реализации

1. Блок 1 (безопасность) — критично, делать первым.
2. Блок 2 (типизация) — создаёт фундамент для рефакторинга.
3. Блок 3 (рефакторинг MQTT, Alert, Clock, стили/тема).
4. Блок 4 (чистка мёртвого кода) — после рефакторинга, чтобы не сломать зависимости.
5. Блок 5 (локализация, console.log).
6. Блок 6 (версии).

После каждого блока — сборка/прогон TS-компилятора (`npx tsc --noEmit` в `mobile/`) и ручная проверка ключевых сценариев (PIN, подключение MQTT, телеметрия, логи, настройки).