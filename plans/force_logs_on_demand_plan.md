# План исправления: принудительный запрос логов при входе на экран

## Проблема
Устройство публикует полный лог в `watersystem/logs` только:
1. При подключении к MQTT брокеру
2. Раз в 60 секунд по таймеру

Если пользователь заходит на экран логов в произвольный момент, первый полный лог может прийти с задержкой до 60 секунд. Всё это время экран показывает "Ожидание логов...".

## Решение
Добавить команду `getlogs`, которая заставит устройство немедленно опубликовать полный лог.

## Изменения

### 1. Прошивка ESP32 — `src/vqtt.cpp`

Добавить обработку команды `getlogs` в функцию `onMqttMessage()`:

```cpp
// Вставить после обработки reset_emergency (после строки 157)
if (cmd == "getlogs") {
    Serial.println("[VQTT] Forced log publish via getlogs command");
    vqtt_publishLogs();
    return;
}
```

Команда `getlogs` не требует ручного режима — должна работать всегда.

### 2. Мобильное приложение — `mobile/src/screens/LogsScreen.tsx`

При входе на экран логов отправлять команду `watersystem/cmd/getlogs`:

```typescript
// После подключения к MQTT отправляем команду на получение логов
MqttService.publish('getlogs', '1');
```

### 3. MQTT Service — `mobile/src/services/mqttService.ts`

Метод `publish()` уже существует и публикует в `watersystem/cmd/{target}`. Дополнительных изменений не требуется.

## Порядок действий

1. Добавить обработку `getlogs` в `src/vqtt.cpp`
2. Добавить отправку команды `getlogs` в `mobile/src/screens/LogsScreen.tsx`
3. Перепрошить ESP32
4. Пересобрать APK
5. Проверить

## Диаграмма

```mermaid
sequenceDiagram
    participant Mobile as Mobile App
    participant MQTT as MQTT Broker
    participant ESP32 as ESP32 Device

    Note over Mobile: Вход на экран логов
    Mobile->>MQTT: Subscribe watersystem/logs
    Mobile->>MQTT: Subscribe watersystem/logs/event
    Mobile->>MQTT: Publish watersystem/cmd/getlogs
    
    MQTT->>ESP32: watersystem/cmd/getlogs
    ESP32->>ESP32: vqtt_publishLogs()
    ESP32->>MQTT: Publish watersystem/logs (полный лог)
    MQTT-->>Mobile: watersystem/logs (последние ~30 событий)
    
    Note over ESP32: Новое событие
    ESP32->>MQTT: watersystem/logs/event
    MQTT-->>Mobile: Добавить в конец