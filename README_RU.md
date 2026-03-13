# WaterSysPro - Система управления водоочисткой v1.1.0

IoT контроллер обработки воды с прошивкой ESP32, backend на Node.js и мобильным приложением React Native.

![Версия](https://img.shields.io/badge/версия-1.1.0-blue)
![Статус сборки](https://img.shields.io/badge/сборка-успешно-brightgreen)
![Лицензия](https://img.shields.io/badge/лицензия-MIT-green)

---

## 🎯 Обзор системы

**WaterSysPro** - это полнофункциональная система управления водоочисткой, включающая:

### 📱 **Оборудование (ESP32)**
- Мониторинг в реальном времени через двойные ультразвуковые датчики уровня
- Управление 7 реле (насос1, насос2, аэрация, озон, фильтр, промывка, аварийное)
- MQTT телеметрия во внешний брокер (поддержка plain + TLS)
- REST API для управления и мониторинга
- Логирование событий на карту microSD или SPIFFS
- LCD дисплей с управлением энкодером
- Инженерное меню с pin-защитой

### 🌐 **Backend (Node.js)**
- RESTful API для управления прошивкой
- WebSocket обновления в реальном времени
- Аутентификация пользователей через JWT
- История оборудования и мониторинг
- Готов к развертыванию через Docker

### 📱 **Мобильное приложение (React Native)**
- Android APK готов к сборке
- Панель управления устройством в реальном времени
- Удаленное управление реле
- Визуализация исторических данных
- Push-уведомления
- Аутентификация пользователей

---

## 🚀 Быстрый старт

### 1. Прошивка (ESP32)

```bash
cd d:\WatersysPro
platformio run           # Компилирование
platformio run -t upload # Загрузка на COM5
platformio device monitor # Просмотр логов (115200 baud)
```

**IP устройства**: http://192.168.3.13/

### 2. Backend сервер

```bash
cd d:\WatersysPro\backend
npm install
npm start
# Сервер работает на http://localhost:3000/api

# или используя Docker
docker-compose up -d
```

### 3. Мобильное приложение (Android APK)

```bash
cd d:\WatersysPro\mobile
npm install -g eas-cli
eas login
eas build -p android    # Облачная сборка (рекомендуется)
# Получите ссылку для скачивания APK за 5-15 минут
```

Или используйте готовый батник (Windows):
```bash
d:\WatersysPro\mobile\build-apk.bat
```

---

## 📋 Возможности

### Возможности прошивки
- ✅ **Двойные ультразвуковые датчики** - мониторинг уровня в реальном времени
- ✅ **Настраиваемые таймеры** - гибкое планирование для всех циклов
- ✅ **MQTT телеметрия** - поддержка plain + TLS/SSL шифрования
- ✅ **Логирование событий** - читаемые текстовые логи с фильтрацией
- ✅ **Защищенный веб интерфейс** - токен администратора + защита паролем
- ✅ **Поддержка SD карты** - полное управление SD из веб-интерфейса
- ✅ **Автомат состояний (FSM)** - 5-состояний цикл обработки воды
- ✅ **Инженерное меню** - защищено PIN-кодом расширенная конфигурация

### Возможности MQTT
- Подключение к внешним брокерам (протестировано: m1.wqpt.ru)
- Поддержка both режимов: plain (порт 19160) и TLS (порт 19161)
- Автоматическое пропускание проверки сертификатов для самоподписанных сертификатов
- Экспоненциальная задержка при переподключении (5s → 60s)
- Публикация телеметрии каждые 10 секунд (настраивается)
- Подписка на команды для удаленного управления
- Автообнаружение устройства через ID клиента

### Возможности мобильного приложения
- 📊 Мониторинг устройства в реальном времени
- 🎮 Удаленное управление реле
- 📈 Графики исторических данных
- 🔔 Push-уведомления
- 🔐 Аутентификация пользователя
- 📱 Навигация с вкладками

---

## 📁 Структура проекта

```
d:\WatersysPro/
├── src/                          # Исходный код прошивки ESP32
│   ├── main.cpp                  # Основной цикл устройства
│   ├── vqtt.cpp                  # MQTT клиент (PubSubClient v2.8)
│   ├── web_server.cpp            # REST API endpoints
│   ├── state_machine.cpp         # Реализация автомата состояний
│   ├── sensors.cpp               # Чтение ультразвуковых датчиков
│   ├── relay_control.cpp         # Переключение GPIO реле
│   └── event_logging.cpp         # Управление текстовыми логами
│
├── include/                      # Файлы заголовков
│   ├── config.h                  # MQTT и системные константы
│   ├── structures.h              # Структуры данных
│   └── state_machine.h           # Определения автомата состояний
│
├── backend/                      # Node.js сервер
│   ├── src/
│   │   ├── server.js             # Express приложение
│   │   └── db.js                 # Интерфейс БД
│   ├── package.json              # Зависимости
│   └── Dockerfile                # Docker образ
│
├── mobile/                       # React Native приложение
│   ├── src/
│   │   ├── screens/              # UI экраны (Login, Dashboard, Settings)
│   │   └── services/             # Сервисы API и WebSocket
│   ├── app.json                  # Конфигурация приложения
│   ├── eas.json                  # Конфигурация EAS сборки
│   └── build-apk.bat            # Windows скрипт сборки
│
├── mosquitto/                    # MQTT брокер (Docker)
│   ├── config/
│   │   └── mosquitto.conf
│   └── data/                     # Хранение данных
│
├── docs/                         # Документация
│   ├── SD_INTEGRATION.md         # Руководство SD карты
│   └── REFRACTOR_AND_SMOKE_SUMMARY.md
│
├── platformio.ini               # Конфигурация сборки
├── docker-compose.yml           # Развертывание полного стека
├── VERSION                       # Отслеживание версии
└── CHANGELOG_RU.md              # История изменений
```

---

## 🔧 Конфигурация

### Настройки MQTT (Веб-интерфейс: /api/mqtt)

**Через веб-интерфейс:**
```json
{
  "enabled": true,
  "broker": "m1.wqpt.ru",
  "port": 19160,
  "tls_port": 19161,
  "secure": true,
  "user": "REDACTED_MQTT_USER",
  "pass": "REDACTED_MQTT_PASS",
  "topic_base": "watersystem",
  "interval": 10,
  "insecure": false
}
```

**Через инженерное меню:**
- Меню > MQTT Setup > TLS Enable/Disable
- Меню > MQTT Setup > TLS Port
- Меню > MQTT Setup > Broker (имя хоста)
- Меню > MQTT Setup > Port (plain MQTT)

### Флаги компилятора (platformio.ini)

```ini
build_flags =
    -Oz                                # Агрессивная оптимизация размера
    -ffunction-sections               # Сборка мусора на уровне функций
    -fdata-sections                   # Сборка мусора на уровне данных
    -fno-exceptions                   # Удалить исключения C++
    -fno-rtti                         # Удалить RTTI
    -Wl,--gc-sections                 # Сборка мусора во время линковки
    -Wl,--strip-all                   # Удалить символы отладки
```

**Результат**: Flash сокращена с 99.8% → 94.6% (освобождено 68 KB)

---

## 📊 Статус памяти

**Устройство**: ESP32 Dev Module (240MHz CPU, 320KB RAM, 4MB Flash)

| Метрика | Значение | Статус |
|---------|----------|--------|
| Flash используется | 1,240,256 байт (94.6%) | ✅ Безопасно |
| RAM используется | 53,540 байт (16.3%) | ✅ Здорово |
| Свободная куча | ~172 KB | ✅ Хорошо |

---

## 🔐 Безопасность MQTT

### Протестированные конфигурации

1. **Plain MQTT** (Порт 19160)
   ```
   Брокер: m1.wqpt.ru:19160
   Пользователь: REDACTED_MQTT_USER
   Пароль: REDACTED_MQTT_PASS
   Статус: ✅ Подключено и публикует
   ```

2. **TLS MQTT** (Порт 19161)
   ```
   Брокер: m1.wqpt.ru:19161
   TLS: Включен с setInsecure()
   Пользователь: REDACTED_MQTT_USER
   Пароль: REDACTED_MQTT_PASS
   Статус: ✅ Подключено и публикует
   ```

### Обработка сертификатов
- Использует `NetworkClientSecure` для TLS
- `setInsecure()` включает прием самоподписанных сертификатов
- Будущее: поддержка pinning сертификатов

---

## 📚 Документация

### Прошивка
- [Руководство интеграции SD карты](docs/SD_INTEGRATION.md)
- [Резюме рефакторинга](docs/REFRACTOR_AND_SMOKE_SUMMARY.md)
- [История изменений](CHANGELOG_RU.md)

### Мобильное приложение
- [Быстрый старт сборки](mobile/START_HERE.md)
- [Полное руководство сборки](mobile/BUILD_APK_GUIDE.md)
- [Чек-лист перед сборкой](mobile/CHECKLIST.md)
- [Руководство управления устройством](mobile/DEVICE_CONTROL_GUIDE.md)

### Backend
- [README](backend/README.md)

---

## 🛠️ Разработка

### Добавление новых MQTT тем

**Файл**: `src/vqtt.cpp`
```cpp
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    // Логика разбора темы
    String t(topic);
    String p((char*)payload, length);
    
    if (cmd == "my_command") {
        // Обработка команды
    }
}
```

### Добавление REST API endpoints

**Файл**: `src/web_server.cpp`
```cpp
server.on("/api/my_endpoint", HTTP_GET, handler_name);
```

### Создание экранов мобильного приложения

**Файл**: `mobile/src/screens/MyScreen.tsx`
```typescript
import React from 'react';
export default function MyScreen() {
  return (/* JSX */);
}
```

---

## 🧪 Тестирование

### Тесты прошивки
```bash
# Компилирование без загрузки
platformio run -e esp32dev

# Проверка на ошибки
platformio check -e esp32dev

# Полная сборка с подробным выводом
pio run -v
```

### Тестирование MQTT
```bash
# Тест plain подключения
mosquitto_pub -h m1.wqpt.ru -p 19160 -u REDACTED_MQTT_USER -P REDACTED_MQTT_PASS -t test/topic -m "hello"

# Тест TLS подключения
mosquitto_pub -h m1.wqpt.ru -p 19161 -u REDACTED_MQTT_USER -P REDACTED_MQTT_PASS --insecure -t test/topic -m "hello"
```

### Тестирование мобильного приложения
```bash
# Тест в Expo Go
npm start

# Отсканируйте QR-код приложением Expo Go на телефоне
```

---

## 🚢 Развертывание

### Docker стек (включает MQTT брокер)

```bash
cd d:\WatersysPro
docker-compose up -d
```

Это запускает:
- Backend API на http://localhost:3000
- MQTT брокер на localhost:1883
- Хранение Mosquitto в `mosquitto/data/`

### Ручное развертывание

1. **Собрать прошивку**:
   ```bash
   cd src
   platformio run
   ```

2. **Запустить Backend**:
   ```bash
   cd backend
   npm install && npm start
   ```

3. **Развернуть мобильное приложение**:
   ```bash
   cd mobile
   eas build -p android
   # Установите APK на Android устройство
   ```

---

## 📈 История версий

| Версия | Дата | Изменения |
|--------|------|----------|
| **1.1.0** | 2026-03-13 | MQTT TLS поддержка, Мобильное приложение готово, Оптимизация памяти |
| **1.0.0** | 2026-03-12 | Первоначальный релиз с интеграцией SD, Логирование событий |

Смотрите [CHANGELOG_RU.md](CHANGELOG_RU.md) для подробной истории версий.

---

## 🤝 Внесение вклада

1. Создайте ветку функции: `git checkout -b feature/my-feature`
2. Сделайте commit изменений: `git commit -m "Описание"`
3. Push в ветку: `git push origin feature/my-feature`
4. Отправьте pull request

---

## 📞 Поддержка

- **Проблемы**: Проверьте существующие файлы документации
- **Прошивка**: Отредактируйте файлы в `src/` и `include/`, затем пересоберите
- **Мобильное приложение**: Смотрите `mobile/START_HERE.md` для быстрого старта
- **Backend**: Смотрите `backend/README.md`

---

## 📄 Лицензия

MIT Лицензия - Смотрите файл LICENSE для подробностей

---

**IP устройства**: http://192.168.3.13/
**MQTT брокер**: m1.wqpt.ru (порты 19160=plain, 19161=TLS)
**Мобильное приложение**: Готово собирать с `eas build -p android`

Последнее обновление: 13 марта 2026 г.
