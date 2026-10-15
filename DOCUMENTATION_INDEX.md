# 📚 Указатель документации WaterSysPro

Полный обзор системы: [README.md](README.md) · История версий: [CHANGELOG.md](CHANGELOG.md) · Архитектура: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

---

## 🏠 Корневые документы

- **[README.md](README.md)** — единый канонический обзор проекта (русский): возможности, быстрый старт, структура, настройка
- **[CHANGELOG.md](CHANGELOG.md)** — история изменений (v1.3.6 → v1.0.0)
- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — архитектура системы и статус компонентов
- **[VERSIONING.md](VERSIONING.md)** — управление версиями и соглашение о коммитах
- **[NTP_SYNC_IMPLEMENTATION.md](NTP_SYNC_IMPLEMENTATION.md)** — синхронизация времени через NTP
- **[LICENSE](LICENSE)** — лицензия MIT
- **[VERSION](VERSION)** — текущая версия (1.3.5)

---

## 📋 Актуальные руководства (plans/)

- **[plans/DEPLOY_INSTRUCTIONS.md](plans/DEPLOY_INSTRUCTIONS.md)** — деплой backend на VPS и сборка APK
- **[plans/wifi_troubleshooting_guide.md](plans/wifi_troubleshooting_guide.md)** — устранение проблем WiFi
- **[plans/open_project_vscode.md](plans/open_project_vscode.md)** — открытие проекта в VS Code

> Разовые планы исправлений перенесены в `plans/archive/`.

---

## 📚 Справочники (docs/)

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — архитектура и компоненты системы
- **[docs/SD_INTEGRATION.md](docs/SD_INTEGRATION.md)** — интеграция SD-карты
- **[docs/NOTIFICATIONS_SETUP_RU.md](docs/NOTIFICATIONS_SETUP_RU.md)** — настройка уведомлений (Telegram / MAX / SMS)
- **[docs/PRODUCTION_RELEASE_CHECKLIST.md](docs/PRODUCTION_RELEASE_CHECKLIST.md)** — pre-release чек-лист
- **docs/schematics/** — электрические схемы и распиновка
- **docs/app/index.html** — документация как отдельное mini-app

> Отчёты процесса разработки перенесены в `docs/archive/`.

---

## 📱 Мобильное приложение (mobile/)

- **[mobile/README.md](mobile/README.md)** — точка входа по приложению
- **[mobile/BUILD.md](mobile/BUILD.md)** — сборка APK (EAS cloud / локально / Expo Go)
- **[mobile/DEVICE_CONTROL_GUIDE.md](mobile/DEVICE_CONTROL_GUIDE.md)** — использование приложения
- **[mobile/FCM_SETUP_RU.md](mobile/FCM_SETUP_RU.md)** — настройка push-уведомлений (Firebase)
- Скрипты сборки: **build-apk.bat** (Windows), **build-apk.sh** (Linux/Mac)

---

## ⚙️ Backend и мониторинг

- **[backend/README.md](backend/README.md)** — backend-сервер
- **monitoring/prometheus.yml** — конфигурация Prometheus
- **docker-compose.yml** — Docker-стек (локальный) · **docker-compose.cloud.yml** — production-стек
- **.env.example** / **.env.cloud.example** — шаблоны переменных окружения

---

## 🛠 Скрипты

- **scripts/backup_db.sh** — бэкап базы данных (gzip + S3)
- **scripts/analyze_libs.py** — анализ библиотек
- **scripts/find_unused_symbols.py** — поиск неиспользуемых символов
- **scripts/rotate_mosquitto_pass.py** — ротация паролей Mosquitto
- **scripts/dump_lines.py** — дамп строк кода
- **scripts/remove_spiffs.py** — удаление SPIFFS

---

## 🔗 Важные адреса

| Ресурс | Адрес |
|--------|-------|
| Устройство (web UI) | http://192.168.3.13/ |
| MQTT Broker | m1.wqpt.ru — plain :19160, TLS :19161 |
| Backend API (local) | http://localhost:3000/api |
| Prometheus | порт 9090 |
| Grafana | порт 3001 |

---

## 🎯 Быстрые переходы

- **Собрать APK** → [mobile/BUILD.md](mobile/BUILD.md)
- **Что изменилось** → [CHANGELOG.md](CHANGELOG.md)
- **Развернуть на VPS** → [plans/DEPLOY_INSTRUCTIONS.md](plans/DEPLOY_INSTRUCTIONS.md)
- **Архитектура** → [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- **Настройка уведомлений** → [docs/NOTIFICATIONS_SETUP_RU.md](docs/NOTIFICATIONS_SETUP_RU.md)
- **Проблемы с WiFi** → [plans/wifi_troubleshooting_guide.md](plans/wifi_troubleshooting_guide.md)

---

## 💾 Версия: 1.3.5

- **Статус:** ✅ Финализирован
- **Стек:** ESP32 (C++/PlatformIO) + Node.js/Express + React Native/Expo
- **Последнее обновление:** 11 сентября 2026 г.
