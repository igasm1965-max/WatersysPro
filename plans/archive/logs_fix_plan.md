# План исправления загрузки логов в мобильном приложении

## Анализ проблемы

После изучения всего кода (ESP32 → MQTT → Backend → Mobile) выявлены следующие потенциальные проблемы:

### 1. Несоответствие deviceId (КРИТИЧЕСКАЯ)

**ESP32** публикует логи в топик `watersystem/logs` (vqtt.cpp:645).
**Backend** в `parseIncomingTopic()` (server.js:115-129) парсит этот топик и возвращает `deviceId = MQTT_TOPIC_BASE` (т.е. `"watersystem"`).

**Mobile** получает список устройств через `GET /api/devices` (server.js:457-470), который возвращает `device_id` из таблицы `telemetry`. 

**Проблема:** Если в telemetry deviceId отличается от `"watersystem"` (например, ESP32 шлёт телеметрию с другим ID), то при запросе `/api/device-logs-db/watersystem` — логи найдутся, а при запросе `/api/device-logs-db/<реальный_id>` — нет.

### 2. Backend возвращает text/plain, axios может его не распарсить

Endpoint `/api/device-logs-db/:deviceId` (server.js:550-584) возвращает `Content-Type: text/plain`.
Mobile вызывает его через `api.get()` (axios) в api.ts:107. Axios по умолчанию пытается парсить JSON. Если ответ не JSON — может выбросить ошибку парсинга.

### 3. Прямое подключение (RAM/SD) — IP может отсутствовать

Mobile вызывает `api.getDeviceIp()` (api.ts:50-58), который берёт IP из `status.payload.ip`.
ESP32 отправляет `doc["ip"] = wifi_getIP()` в телеметрии (vqtt.cpp:568) — это работает.
НО: если телефон не в той же сети, что ESP32 — прямое подключение не сработает.

### 4. Прокси-режим (backend → ESP32)

Backend в server.js:591-656 пытается подключиться к ESP32 напрямую по IP.
Если ESP32 за NAT — не сработает.

## План исправлений

### Шаг 1: Исправить backend — endpoint /api/device-logs-db/:deviceId

**Файл:** `backend/src/server.js`
**Строки:** 550-584

Изменить:
- Возвращать JSON-объект `{ deviceId, ts, content }` вместо plain text
- Установить `Content-Type: application/json`
- В случае отсутствия логов возвращать `{ error: "no logs", deviceId }` с HTTP 404

### Шаг 2: Исправить mobile — api.ts метод getDeviceLogsFromDb

**Файл:** `mobile/src/services/api.ts`
**Строки:** 106-109

Изменить:
- Извлечь `content` из JSON-ответа
- Добавить обработку ошибок с распознаванием 404

### Шаг 3: Исправить mobile — LogsScreen.tsx

**Файл:** `mobile/src/screens/LogsScreen.tsx`
**Строки:** 36-61

Изменить:
- Улучшить обработку ошибок для MQTT-режима
- Добавить диагностическую информацию (какой deviceId используется)
- Улучшить сообщения об ошибках

### Шаг 4: Проверить endpoint /api/device-logs/:deviceId (прокси)

**Файл:** `backend/src/server.js`
**Строки:** 591-656

Убедиться, что прокси тоже возвращает JSON с полем `content`, а не plain text, для единообразия.

### Шаг 5: Проверить deviceId в телеметрии

Убедиться, что ESP32 публикует телеметрию с тем же deviceId, под которым сохраняются логи.