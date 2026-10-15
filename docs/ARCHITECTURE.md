# Архитектура WaterSysPro

Постоянный справочник по архитектуре и компонентам системы. Версия: **1.3.5**.

## Общая архитектура

```
┌─────────────────────────────────────────────────────────────────┐
│                      ESP32 (прошивка)                           │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────────────┐  │
│  │ FSM      │ │ MQTT     │ │ Web      │ │ LCD + Encoder     │  │
│  │ (7 сост.)│ │ (TLS/   │ │ Server   │ │ (20x4, меню)      │  │
│  │          │ │  plain)  │ │ (REST+WS)│ │                   │  │
│  └──────────┘ └──────────┘ └──────────┘ └───────────────────┘  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────────────┐  │
│  │ Sensors  │ │ SD Card  │ │ NTP/RTC  │ │ Emergency System  │  │
│  │ (HC-SR04)│ │ (логи)   │ │ (DS3231) │ │ (pump dry-run)    │  │
│  └──────────┘ └──────────┘ └──────────┘ └───────────────────┘  │
└──────────────────────┬──────────────────────────────────────────┘
                       │ MQTT (watersystem/#)
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│                   Backend (Node.js/Express)                      │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────────────┐  │
│  │ REST API │ │ MQTT     │ │ Socket.IO│ │ PostgreSQL + mem   │  │
│  │ (15+     │ │ Client   │ │ (realtime│ │ (in-memory fallback)│  │
│  │  endpoints)│ │          │ │  updates)│ │                   │  │
│  └──────────┘ └──────────┘ └──────────┘ └───────────────────┘  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────────────┐  │
│  │ Rate     │ │ Audit    │ │Prometheus│ │ JWT Auth + Push   │  │
│  │ Limiting │ │ Log      │ │ Metrics  │ │ Notifications      │  │
│  └──────────┘ └──────────┘ └──────────┘ └───────────────────┘  │
└──────────────────────┬──────────────────────────────────────────┘
                       │ REST API / WSS
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│              Mobile App (React Native / Expo)                    │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────────────┐  │
│  │Dashboard │ │ Settings │ │   Logs   │ │   PIN Screen      │  │
│  │(MQTT live│ │(MQTT,    │ │(MQTT-    │ │   (4-digit code)  │  │
│  │ telemetry)│ │  server) │ │  канал)  │ │                   │  │
│  └──────────┘ └──────────┘ └──────────┘ └───────────────────┘  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────────────┐  │
│  │ MQTT WSS │ │ REST API │ │ Secure   │ │ Push Notifications│  │
│  │ (custom  │ │ (axios)  │ │ Storage  │ │ (Expo)            │  │
│  │  client) │ │          │ │(Keychain)│ │                   │  │
│  └──────────┘ └──────────┘ └──────────┘ └───────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Поток данных

1. ESP32 собирает показания датчиков и состояние исполнительных узлов.
2. FSM выбирает переходы между рабочими режимами.
3. Контроллер публикует телеметрию в MQTT (`watersystem/#`) и отдаёт состояние через web/API.
4. Backend агрегирует данные (PostgreSQL, in-memory fallback), шлёт realtime-обновления через Socket.IO.
5. Мобильное приложение отображает состояние/историю и отправляет команды через MQTT WSS и REST API.

## Статус компонентов

### ESP32 Firmware (C++ / PlatformIO)
| Компонент | Статус | Примечание |
|-----------|--------|------------|
| FSM (7 состояний) | ✅ Работает | IDLE → FILLING → OZONATION → AERATION → SETTLING → FILTRATION → BACKWASH |
| MQTT (PubSubClient) | ✅ Работает | Plain + TLS, CA pinning (ISRG Root X1), keepalive 120s |
| Web Server | ✅ Работает | REST API + WebSocket, LittleFS для UI |
| LCD + Encoder | ✅ Работает | 20x4, инженерное меню |
| Sensors (HC-SR04) | ✅ Работает | 2 датчика, гистерезис, защита от сухого хода |
| SD Card | ✅ Работает | Логирование, fallback на LittleFS |
| Emergency System | ✅ Работает | Pump dry-run, аварийные флаги |
| NTP Sync | ✅ Работает | 3 NTP сервера, часовой пояс |
| OTA Updates | ✅ Работает | С паролем |
| Watchdog | ✅ Работает | 60s таймаут |
| Flash Usage | ⚠️ 94.6% | Критично мало места для новых функций |
| RAM Usage | ✅ 16.3% | Достаточно |

### Backend (Node.js / Express)
| Компонент | Статус | Примечание |
|-----------|--------|------------|
| REST API | ✅ Работает | 15+ endpoints |
| MQTT Client | ✅ Работает | Подписка на device/# и watersystem/# |
| PostgreSQL | ✅ Работает | + in-memory fallback |
| Socket.IO | ✅ Работает | Realtime updates |
| Rate Limiting | ✅ Работает | 3 уровня (API/auth/command) |
| Audit Log | ✅ Работает | Все команды и логины |
| Prometheus Metrics | ✅ Работает | /metrics endpoint |
| JWT Auth | ✅ Работает | Login/register |
| Push Notifications | ✅ Работает | Expo push tokens |
| SQL Migrations | ✅ Работает | Авто-применение |
| Firmware OTA | ✅ Работает | /api/firmware/latest |

### Mobile App (React Native / Expo)
| Компонент | Статус | Примечание |
|-----------|--------|------------|
| DashboardScreen | ✅ Работает | MQTT live telemetry, relay control |
| SettingsScreen | ✅ Работает | Server, MQTT, push, PIN |
| LogsScreen | ✅ Работает | Логи через MQTT-канал (`watersystem/logs`, `watersystem/logs/event`) |
| PinScreen | ✅ Работает | 4-digit PIN |
| MQTT over WSS | ✅ Работает | Custom pure-JS client |
| Secure Storage | ✅ Работает | expo-secure-store |
| Push Notifications | ✅ Работает | Expo push |

### Infrastructure
| Компонент | Статус | Примечание |
|-----------|--------|------------|
| Docker Compose (local) | ✅ Работает | Backend + PostgreSQL |
| Docker Compose (cloud) | ✅ Работает | Production стек |
| Prometheus | ✅ Работает | Порт 9090 |
| Grafana | ✅ Работает | Порт 3001 |
| Mosquitto | ✅ Работает | С аутентификацией |
| Nginx config | ✅ Работает | HTTPS, HSTS, WebSocket proxy |

## Ключевые технологии

- **Прошивка**: ESP32, PlatformIO, Arduino framework, PubSubClient, AsyncWebServer
- **Backend**: Node.js, Express, Socket.IO, mqtt, express-rate-limit, prom-client
- **Mobile**: React Native, Expo, expo-secure-store, MQTT over WSS
- **Инфраструктура**: Docker Compose, PostgreSQL, Prometheus, Grafana, Mosquitto, Nginx

## Связанные документы

- [README.md](../README.md) - общий обзор проекта
- [SD_INTEGRATION.md](SD_INTEGRATION.md) - интеграция SD-карты
- [NOTIFICATIONS_SETUP_RU.md](NOTIFICATIONS_SETUP_RU.md) - настройка уведомлений
- [../CHANGELOG.md](../CHANGELOG.md) - история изменений