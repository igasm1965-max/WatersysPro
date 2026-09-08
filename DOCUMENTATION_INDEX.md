# 📚 Указатель документации WaterSysPro

## 🇷🇺 Документация на РУССКОМ

### Основные документы (русский)
1. **[README_RU.md](README_RU.md)** - Полный обзор проекта на русском
   - Обзор системы и возможности
   - Быстрый старт
   - Структура проекта
   - Конфигурация и настройка

2. **[CHANGELOG_RU.md](CHANGELOG_RU.md)** - История изменений на русском
   - Версия 1.3.0 - полный список изменений
   - Версия 1.2.2 - безопасность (P0 Hardening)
   - Версия 1.2.1 - исправления MQTT/TLS
   - Версия 1.2.0 - ручной режим, аварии
   - Версия 1.1.0 - MQTT TLS, мобильное приложение
   - Версия 1.0.0 - начальный релиз

3. **[RELEASE_NOTES_v1.1.0_RU.md](RELEASE_NOTES_v1.1.0_RU.md)** - Примечания к релизу v1.1.0 на русском
   - Эпические достижения (MQTT TLS решена!)
   - Технические детали
   - Что протестировано
   - Руководство миграции

### Проект (русский)
- **[PROJECT_STATUS.md](PROJECT_STATUS.md)** - Статус проекта v1.3.0
  - Что сделано в последнем сеансе
  - Память и оптимизация
  - Git commits
- **[NTP_SYNC_IMPLEMENTATION.md](NTP_SYNC_IMPLEMENTATION.md)** - Синхронизация времени через NTP
  - Реализация в timers.cpp и wifi_manager.cpp
  - Автоматический вызов при получении IP
  - Поддержка 3 NTP серверов
- **[VERSIONING.md](VERSIONING.md)** - Управление версиями
  - Semantic Versioning
  - Процесс релиза
  - Соглашение о коммитах
- **[docs/WORKTREE_CHANGES_2026-03-19.md](docs/WORKTREE_CHANGES_2026-03-19.md)** - Полная сводка всех текущих изменений рабочей копии
   - Все измененные и новые файлы
   - Классификация по подсистемам
   - Риски и рекомендации по разбиению на коммиты
- **[docs/PRODUCTION_HARDENING_TRACKER.md](docs/PRODUCTION_HARDENING_TRACKER.md)** - Трекер hardening-задач для production
   - Приоритеты P0/P1/P2
   - Owner, estimate, статус, DoD
   - Чекбоксы исполнения по каждой задаче
- **[docs/PRODUCTION_RELEASE_CHECKLIST.md](docs/PRODUCTION_RELEASE_CHECKLIST.md)** - Обязательный pre-release checklist перед production
   - Security gate
   - Build/smoke gate
   - Sign-off перед релизом
- **[docs/PRODUCTION_READINESS_REPORT_2026-04-20.md](docs/PRODUCTION_READINESS_REPORT_2026-04-20.md)** - Финальный отчёт готовности к production
   - Go/No-Go вердикт
   - Подтверждённые проверки
   - Оставшиеся обязательные шаги
- **[docs/SAFE_RELEASE_COMMIT_PLAN_2026-04-20.md](docs/SAFE_RELEASE_COMMIT_PLAN_2026-04-20.md)** - Безопасный план разбиения изменений на коммиты
   - Порядок коммитов перед релизом
   - Готовые команды `git add`/`git commit`

### Планы и анализ
- **[plans/DEPLOY_INSTRUCTIONS.md](plans/DEPLOY_INSTRUCTIONS.md)** - Инструкция по деплою на VPS и сборке APK
  - Деплой backend на VPS через Docker
  - Сборка APK (EAS cloud / локальная / Expo Go)
  - Настройка мобильного приложения
  - Диагностика проблем
- **[plans/logs_fix_plan.md](plans/logs_fix_plan.md)** - План исправления загрузки логов
  - Анализ несоответствия deviceId
  - Исправление backend endpoint
  - Исправление mobile api.ts и LogsScreen.tsx
- **[plans/wifi_stability_analysis.md](plans/wifi_stability_analysis.md)** - Анализ стабильности WiFi
- **[plans/wifi_troubleshooting_guide.md](plans/wifi_troubleshooting_guide.md)** - Руководство по устранению проблем WiFi
- **[plans/open_project_vscode.md](plans/open_project_vscode.md)** - Инструкция по открытию проекта в VS Code

---

## 🇬🇧 Documentation in ENGLISH

### Main Documents (English)
1. **[README.md](README.md)** - Full project overview
   - System overview and features
   - Quick start
   - Project structure
   - Configuration

2. **[CHANGELOG.md](CHANGELOG.md)** - Change history
   - Version 1.3.0 - complete changelog
   - Version 1.2.2 - security hardening
   - Version 1.2.1 - MQTT/TLS fixes
   - Version 1.2.0 - manual mode, emergencies
   - Version 1.1.0 - MQTT TLS, mobile app
   - Version 1.0.0 - initial release

3. **[RELEASE_NOTES_v1.1.0.md](RELEASE_NOTES_v1.1.0.md)** - Release notes v1.1.0
   - Major achievements
   - Technical details
   - Testing status
   - Migration guide

---

## 📱 Мобильное приложение (РУССКИЙ)

### Документация и руководства
- **[mobile/START_HERE.md](mobile/START_HERE.md)** ⭐ - Начните отсюда! (5 минут)
- **[mobile/MOBILE_APP_CREATION_GUIDE_RU.md](mobile/MOBILE_APP_CREATION_GUIDE_RU.md)** - Полный гайд по созданию мобильного приложения
- **[mobile/QUICK_START_BUILD.md](mobile/QUICK_START_BUILD.md)** - Пошаговая инструкция
- **[mobile/BUILD_APK_GUIDE.md](mobile/BUILD_APK_GUIDE.md)** - Полное руководство сборки
- **[mobile/CHECKLIST.md](mobile/CHECKLIST.md)** - Чек-лист перед сборкой
- **[mobile/DEVICE_CONTROL_GUIDE.md](mobile/DEVICE_CONTROL_GUIDE.md)** - Как использовать приложение
- **[mobile/FILES_CREATED.md](mobile/FILES_CREATED.md)** - Что было создано
- **[mobile/FINAL_SUMMARY.md](mobile/FINAL_SUMMARY.md)** - Финальное резюме

### Скрипты сборки
- **[mobile/build-apk.bat](mobile/build-apk.bat)** - Windows батник (просто нажмите!)
- **[mobile/build-apk.sh](mobile/build-apk.sh)** - Linux/Mac bash скрипт
- **[mobile/COMMANDS.sh](mobile/COMMANDS.sh)** - Быстрые команды для копирования

---

## 📋 Другая документация

### Прошивка
- **[docs/SD_INTEGRATION.md](docs/SD_INTEGRATION.md)** - Интеграция SD карты
- **[docs/REFRACTOR_AND_SMOKE_SUMMARY.md](docs/REFRACTOR_AND_SMOKE_SUMMARY.md)** - Рефакторинг и smoke тесты
- **[NTP_SYNC_IMPLEMENTATION.md](NTP_SYNC_IMPLEMENTATION.md)** - Синхронизация времени через NTP

### Backend
- **[backend/README.md](backend/README.md)** - Backend сервер
- **[plans/DEPLOY_INSTRUCTIONS.md](plans/DEPLOY_INSTRUCTIONS.md)** - Инструкция по деплою на VPS

### Мониторинг
- **[monitoring/prometheus.yml](monitoring/prometheus.yml)** - Конфигурация Prometheus
- **Grafana** - порт 3001 (в docker-compose)

### Скрипты
- **[scripts/backup_db.sh](scripts/backup_db.sh)** - Бэкап базы данных
- **[scripts/analyze_libs.py](scripts/analyze_libs.py)** - Анализ библиотек
- **[scripts/archive_project.py](scripts/archive_project.py)** - Архивация проекта
- **[scripts/find_unused_symbols.py](scripts/find_unused_symbols.py)** - Поиск неиспользуемых символов
- **[scripts/remove_spiffs.py](scripts/remove_spiffs.py)** - Удаление SPIFFS
- **[scripts/dump_lines.py](scripts/dump_lines.py)** - Дамп строк кода

### Конфигурация
- **[platformio.ini](platformio.ini)** - Конфигурация PlatformIO
- **[docker-compose.yml](docker-compose.yml)** - Docker стек (локальный)
- **[docker-compose.cloud.yml](docker-compose.cloud.yml)** - Docker стек (VPS/production)
- **[.env.cloud.example](.env.cloud.example)** - Шаблон переменных для облака
- **[VERSION](VERSION)** - Текущая версия (1.3.5)

---

## 🚀 Быстрые ссылки

### Интерактивный просмотр документации
- **[docs/app/index.html](docs/app/index.html)** - Документация как отдельное mini-app
   - Запуск через backend: `http://localhost:3000/docs/`
   - Левое меню документов
   - Оглавление текущей страницы
   - Навигация по главам и страницам
   - Поиск по названиям документов

### Для новичков
1. Читайте **[README_RU.md](README_RU.md)** - основная информация
2. Смотрите **[mobile/START_HERE.md](mobile/START_HERE.md)** - как собрать APK
3. Для полного пути от идеи до APK: **[mobile/MOBILE_APP_CREATION_GUIDE_RU.md](mobile/MOBILE_APP_CREATION_GUIDE_RU.md)**

### Для разработчиков
1. **[CHANGELOG_RU.md](CHANGELOG_RU.md)** - что изменилось
2. **[platformio.ini](platformio.ini)** - конфигурация сборки
3. **[src/](src/)** - исходный код прошивки
4. **[plans/](plans/)** - планы и анализ

### Для опытных пользователей
1. **[RELEASE_NOTES_v1.1.0_RU.md](RELEASE_NOTES_v1.1.0_RU.md)** - технические детали
2. **[docs/](docs/)** - расширенная документация
3. **[plans/DEPLOY_INSTRUCTIONS.md](plans/DEPLOY_INSTRUCTIONS.md)** - деплой на VPS

---

## 📊 Структура документации

```
Документация/
├── README_RU.md                    🇷🇺 Основной обзор (русский)
├── README.md                       🇬🇧 Основной обзор (английский)
├── CHANGELOG_RU.md                 🇷🇺 История (русский)
├── CHANGELOG.md                    🇬🇧 История (английский)
├── RELEASE_NOTES_v1.1.0_RU.md     🇷🇺 Релиз (русский)
├── RELEASE_NOTES_v1.1.0.md        🇬🇧 Релиз (английский)
├── PROJECT_STATUS.md               Статус проекта
├── DOCUMENTATION_INDEX.md           📍 Этот файл
├── NTP_SYNC_IMPLEMENTATION.md      Синхронизация NTP
├── VERSIONING.md                   Управление версиями
│
├── plans/                          📋 Планы и анализ
│   ├── DEPLOY_INSTRUCTIONS.md      Деплой на VPS
│   ├── logs_fix_plan.md            Исправление логов
│   ├── wifi_stability_analysis.md  Анализ WiFi
│   ├── wifi_troubleshooting_guide.md
│   └── open_project_vscode.md
│
├── docs/                           📚 Расширенная документация
│   ├── SD_INTEGRATION.md
│   ├── PRODUCTION_HARDENING_TRACKER.md
│   ├── PRODUCTION_RELEASE_CHECKLIST.md
│   ├── PRODUCTION_READINESS_REPORT_2026-04-20.md
│   ├── SAFE_RELEASE_COMMIT_PLAN_2026-04-20.md
│   ├── REFRACTOR_AND_SMOKE_SUMMARY.md
│   └── WORKTREE_CHANGES_2026-03-19.md
│
├── monitoring/                     📊 Мониторинг
│   └── prometheus.yml
│
├── scripts/                        🔧 Скрипты
│   ├── backup_db.sh
│   ├── analyze_libs.py
│   ├── archive_project.py
│   ├── find_unused_symbols.py
│   ├── remove_spiffs.py
│   └── dump_lines.py
│
└── mobile/                         📱 Мобильное приложение
    ├── START_HERE.md               ⭐ Начните отсюда
    ├── MOBILE_APP_CREATION_GUIDE_RU.md
    ├── QUICK_START_BUILD.md
    ├── BUILD_APK_GUIDE.md
    ├── CHECKLIST.md
    ├── DEVICE_CONTROL_GUIDE.md
    ├── FILES_CREATED.md
    ├── FINAL_SUMMARY.md
    ├── README_BUILD.md
    ├── build-apk.bat               🪟 Windows скрипт
    ├── build-apk.sh                🐧 Linux/Mac скрипт
    └── COMMANDS.sh                 Быстрые команды
```

---

## 🎯 Выбирите по задаче

### Хотите собрать APK для Android?
→ Начните с **[mobile/START_HERE.md](mobile/START_HERE.md)**

### Хотите понять что изменилось?
→ Читайте **[CHANGELOG_RU.md](CHANGELOG_RU.md)**

### Хотите разобраться с MQTT TLS?
→ Смотрите **[RELEASE_NOTES_v1.1.0_RU.md](RELEASE_NOTES_v1.1.0_RU.md)** раздел "MQTT TLS Support"

### Хотите развернуть систему на VPS?
→ Используйте **[plans/DEPLOY_INSTRUCTIONS.md](plans/DEPLOY_INSTRUCTIONS.md)**

### Хотите отремонтировать прошивку?
→ Проверьте **[docs/SD_INTEGRATION.md](docs/SD_INTEGRATION.md)** и **[platformio.ini](platformio.ini)**

### Хотите развить бэкенд?
→ Смотрите **[backend/README.md](backend/README.md)**

### Хотите настроить мониторинг?
→ Смотрите **[monitoring/prometheus.yml](monitoring/prometheus.yml)** и docker-compose (Grafana :3001)

### Проблемы с WiFi?
→ Читайте **[plans/wifi_troubleshooting_guide.md](plans/wifi_troubleshooting_guide.md)**

### Не загружаются логи в приложении?
→ Смотрите **[plans/logs_fix_plan.md](plans/logs_fix_plan.md)**

---

## 🔗 Важные IP и порты

- **Устройство**: http://192.168.3.13/
- **MQTT Broker**: m1.wqpt.ru
  - Plain: порт 19160
  - TLS: порт 19161
- **Backend API**: http://localhost:3000/api (Docker)
- **Prometheus**: порт 9090
- **Grafana**: порт 3001

---

## 💾 Текущая версия: 1.3.5

- **Дата**: 6 июля 2026 г.
- **Статус**: ✅ Активная разработка
- **Предыдущие версии**: 1.3.0, 1.2.2, 1.2.1, 1.2.0, 1.1.0, 1.0.0

---

## 🆘 Не нашли ответ?

1. **Проверьте CHANGELOG** - может быть ваш вопрос уже решен
2. **Посмотрите CHECKLIST** - может быть что-то упущено
3. **Читайте BUILD_APK_GUIDE** - раздел "Решение проблем"
4. **Проверьте планы в [plans/](plans/)** - там могут быть ответы

---

## 📞 Контакты поддержки

- 🐛 Баги: Проверьте GitHub issues
- 📱 Мобильное приложение: [mobile/START_HERE.md](mobile/START_HERE.md)
- 🔧 Прошивка: Смотрите встроенные комментарии в коде
- 🌐 Backend: [backend/README.md](backend/README.md)
- 📋 Планы и анализ: [plans/](plans/)

---

**Последнее обновление: 6 июля 2026 г.**

Вся документация актуальна и полна! 🚀
