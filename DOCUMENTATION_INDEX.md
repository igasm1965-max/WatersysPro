# 📚 Указатель документации WaterSysPro

## 🇷🇺 Документация на РУССКОМ

### Основные документы (русский)
1. **[README_RU.md](README_RU.md)** - Полный обзор проекта на русском
   - Обзор системы и возможности
   - Быстрый старт
   - Структура проекта
   - Конфигурация и настройка

2. **[CHANGELOG_RU.md](CHANGELOG_RU.md)** - История изменений на русском
   - Версия 1.1.0 - полный список изменений
   - Версия 1.0.0 - начальный релиз
   - Что было добавлено, улучшено, исправлено

3. **[RELEASE_NOTES_v1.1.0_RU.md](RELEASE_NOTES_v1.1.0_RU.md)** - Примечания к релизу v1.1.0 на русском
   - Эпические достижения (MQTT TLS решена!)
   - Технические детали
   - Что протестировано
   - Руководство миграции

### Проект (русский)
- **[PROJECT_STATUS.md](PROJECT_STATUS.md)** - Финальный статус проекта v1.1.0
  - Что сделано в этом сеансе
  - Память и оптимизация
  - Git commits
- **[docs/WORKTREE_CHANGES_2026-03-19.md](docs/WORKTREE_CHANGES_2026-03-19.md)** - Полная сводка всех текущих изменений рабочей копии
   - Все измененные и новые файлы
   - Классификация по подсистемам
   - Риски и рекомендации по разбиению на коммиты

---

## 🇬🇧 Documentation in ENGLISH

### Main Documents (English)
1. **[README.md](README.md)** - Full project overview
   - System overview and features
   - Quick start
   - Project structure
   - Configuration

2. **[CHANGELOG.md](CHANGELOG.md)** - Change history
   - Version 1.1.0 - complete changelog
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

### Backend
- **[backend/README.md](backend/README.md)** - Backend сервер

### Конфигурация
- **[platformio.ini](platformio.ini)** - Конфигурация PlatformIO
- **[docker-compose.yml](docker-compose.yml)** - Docker стек
- **[VERSION](VERSION)** - Текущая версия (1.1.0)

---

## 🚀 Быстрые ссылки

### Для новичков
1. Читайте **[README_RU.md](README_RU.md)** - основная информация
2. Смотрите **[mobile/START_HERE.md](mobile/START_HERE.md)** - как собрать APK

### Для разработчиков
1. **[CHANGELOG_RU.md](CHANGELOG_RU.md)** - что изменилось
2. **[platformio.ini](platformio.ini)** - конфигурация сборки
3. **[src/](src/)** - исходный код прошивки

### Для опытных пользователей
1. **[RELEASE_NOTES_v1.1.0_RU.md](RELEASE_NOTES_v1.1.0_RU.md)** - технические детали
2. **[docs/](docs/)** - расширенная документация

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
│
└── mobile/                         📱 Мобильное приложение
    ├── START_HERE.md               ⭐ Начните отсюда
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

### Хотите развернуть систему?
→ Используйте **[README_RU.md](README_RU.md)** раздел "Быстрый старт"

### Хотите отремонтировать прошивку?
→ Проверьте **[docs/SD_INTEGRATION.md](docs/SD_INTEGRATION.md)** и **[platformio.ini](platformio.ini)**

### Хотите развить бэкенд?
→ Смотрите **[backend/README.md](backend/README.md)**

---

## 🔗 Важные IP и порты

- **Устройство**: http://192.168.3.13/
- **MQTT Broker**: m1.wqpt.ru
  - Plain: порт 19160
  - TLS: порт 19161
- **Backend API**: http://localhost:3000/api (Docker)

---

## 💾 Текущая версия: 1.1.0

- **Дата**: 13 марта 2026 г.
- **Статус**: ✅ Полностью готово
- **Git**: коммиты 3e499a3, 4b76e4e, eeee0e6

---

## 🆘 Не нашли ответ?

1. **Проверьте CHANGELOG** - может быть ваш вопрос уже решен
2. **Посмотрите CHECKLIST** - может быть что-то упущено
3. **Читайте BUILD_APK_GUIDE** - раздел "Решение проблем"

---

## 📞 Контакты поддержки

- 🐛 Баги: Проверьте GitHub issues
- 📱 Мобильное приложение: [mobile/START_HERE.md](mobile/START_HERE.md)
- 🔧 Прошивка: Смотрите встроенные комментарии в коде
- 🌐 Backend: [backend/README.md](backend/README.md)

---

**Последнее обновление: 13 марта 2026 г.**

Вся документация актуальна и полна! 🚀
