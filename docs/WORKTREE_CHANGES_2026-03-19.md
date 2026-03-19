# Сводка всех изменений рабочей копии

Дата: 2026-03-19
Статус: незакоммиченные изменения в рабочем дереве

## Общая статистика
- Изменено tracked-файлов: 17
- Новых untracked-файлов: 3
- Суммарно по tracked (git diff --stat): +1452 / -966 строк

## Backend и инфраструктура

### backend/.env.example
- Добавлены настройки удаленного управления:
  - MQTT_TOPIC_BASE
  - REMOTE_REQUIRE_TOKEN
  - REMOTE_ADMIN_TOKEN

### backend/README.md
- Документированы новые remote-эндпоинты:
  - /remote/
  - /api/remote/config
  - /api/remote/latest
  - /api/remote/command
- Добавлен quick setup для remote web control.
- Добавлен MQTT topic contract для текущей прошивки.

### backend/src/server.js
- Добавлен статический remote UI: раздача /remote из backend/public.
- Добавлены переменные и логика для remote control:
  - MQTT_TOPIC_BASE
  - REMOTE_ADMIN_TOKEN
  - REMOTE_REQUIRE_TOKEN
- Добавлен парсинг новой топик-схемы watersystem/<kind> с обратной совместимостью legacy device/<id>/<kind>.
- Добавлен кеш latestRemote и endpoint-ы:
  - GET /api/remote/config
  - GET /api/remote/latest (token-gated)
  - POST /api/remote/command (token-gated)
- Добавлена подписка на MQTT_TOPIC_BASE/#, обработка state/status/telemetry.

### docker-compose.yml
- Для backend добавлены переменные окружения:
  - MQTT_TOPIC_BASE
  - REMOTE_REQUIRE_TOKEN
  - REMOTE_ADMIN_TOKEN

### backend/public/index.html (new)
- Новый remote web panel для управления реле через backend API.
- Реализованы:
  - хранение token в localStorage,
  - загрузка latest статуса,
  - отправка команд ON/OFF,
  - socket.io live updates,
  - визуализация relay state.

## Прошивка (ESP32)

### platformio.ini
- Добавлен флаг компиляции: -Wno-deprecated-declarations.

### include/timers.h
- Добавлены декларации функций преобразования времени:
  - utcToLocal
  - localToUTC
- Добавлена декларация recalculateRTCForTimezone.

### src/timers.cpp
- Добавлена реализация utcToLocal/localToUTC.
- Изменено формирование POSIX TZ-строки для NTP (инвертированный знак для UTC offsets).
- Логика syncTimeWithNTP обновлена: RTC корректируется локальным временем.
- Добавлена recalculateRTCForTimezone для пересчета RTC при смене часового пояса.

### src/web_server.cpp
- Удален debug endpoint /api/admin_token_debug.
- Добавлен API для часового пояса:
  - GET /api/timezone
  - POST /api/timezone (с проверкой admin token).
- В POST /api/timezone добавлен пересчет RTC и сохранение timezone.

### src/engineer_menu.cpp
- Ввод (LCD hints): укорочены и унифицированы подсказки в экранах ввода.
- В string editor изменен рендер подсказок (динамический символ вынесен в строку позиции).
- Изменена процедура clear Wi-Fi: переход на wifi_clearCredentials + restartWebServer.
- В editTimeZone добавлена явная логика пересчета RTC при смене timezone и сохранение в Preferences.

### src/menu.cpp
- В inputVal подсказка изменена на короткую строку для LCD: Turn=chg Click=ok.

### src/encoder.cpp
- Добавлен helper getEventTimeWithTimezone.
- Отображение времени последних событий адаптировано под timezone offset.

### src/state_machine.cpp
- В isValidTransition разрешен переход BACKWASH -> FILTRATION.
- Усилена защита от застрявших флагов в STATE_IDLE:
  - очистка waterTreatmentInProgress,
  - очистка backwashInProgress.
- Большой объем форматирования и реорганизации кода (без изменения назначения большинства блоков).

### src/sensors.cpp
- Изменена обработка NewPing ping_cm()==0:
  - сырое значение 0 сохраняется в raw,
  - level не перезаписывается нулем (сохраняется предыдущее валидное значение).
- Большой объем форматирования кода.

### src/main.cpp
- Добавлена очистка флагов waterTreatmentInProgress/backwashInProgress при старте в IDLE.
- Дополнительные правки инициализации, реорганизация include/форматирования.
- Большой объем автоформатирования.

### src/preferences.cpp
- Исправлены строки логирования timezone (\n вместо литерального \\n).
- Минорная очистка форматирования.

### src/event_logging.cpp
- getEventText переведен на фиксированный английский текст (без runtime-переключения RU/EN).

## UI на SD

### закинуть на сд/index.html
- В шапке добавлены часы (clock pill).
- В блоке настроек добавлен timezone input и кнопка Save Timezone.
- Убраны lang_en элементы из saveWifi flow.
- Добавлены функции loadTimezone/saveTimezone через /api/timezone.
- Добавлены стилистические и структурные правки (включая class chart-card).

## Логи и служебные файлы

### monitor.log
- Добавлен runtime лог с устройства (tracked-файл изменен).

### monitor_error.txt (new)
- Добавлен лог ошибок serial monitor reconnect.

### monitor_output.txt (new)
- Добавлен полный дамп serial monitor (~960 KB).

## Риски и примечания
- В рабочей копии присутствуют крупные форматирующие изменения (особенно src/main.cpp и src/state_machine.cpp), что усложняет ревью функциональных изменений.
- Для безопасного мержа рекомендуется разделение на тематические коммиты:
  1) timezone/NTP,
  2) FSM/sensors bugfix,
  3) remote backend + UI,
  4) чистое форматирование,
  5) журнальные файлы (по необходимости, либо исключить из VCS).
