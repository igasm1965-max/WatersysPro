# 🚀 WaterSysPro v1.3.5 — План подготовки к Production-релизу

> Дата составления: 2026-09-07
> Основание: аудит `docs/PRODUCTION_READINESS_REPORT_2026-04-20.md`, `docs/PRODUCTION_HARDENING_TRACKER.md`, кода firmware/backend/mobile.
> Вердикт аудита: ⚠️ **No-Go** до устранения блокеров P0 (секция «Критично»).

---

## Итоговый вердикт аудита

| Область | Статус |
|---------|--------|
| Firmware (ESP32) сборка | ✅ Собирается (Flash 66.2%, RAM 17.8%) |
| Backend синтаксис | ✅ Валиден |
| SQL инъекции / hardcoded креды в коде | ✅ Отсутствуют (параметризация) |
| Rate limiting + audit + metrics | ✅ Реализованы |
| 🔴 Утечка секретов в git | ❌ КРИТИЧНО |
| 🔴 Insecure TLS-fallback прошивки | ❌ КРИТИЧНО |
| 🔴 Plain HTTP мобильного приложения | ❌ КРИТИЧНО |
| 🔴 Слабые дефолтные токены/пароли | ❌ КРИТИЧНО |
| CI pipeline | ❌ Отсутствует |
| Liveness/readiness разделение | ❌ Отсутствует |
| Pre-release checklist | ❌ Не подписан |

---

## 🔴 Sprint 0 — Критические блокеры (P0) — обязательны ДО релиза

### Задача P0-A. Отозвать утёкший Telegram bot token
- **Файлы**: `.env.cloud.example`, `backend/.env.example`
- **Шаги**:
  1. Отозвать утёкший токен (значение больше не хранится в репозитории) через @BotFather (`/revoke`).
  2. Создать новый бот-токен, внести ТОЛЬКО в реальный `.env` / secret manager, НЕ в git.
  3. Placeholder `TG_BOT_TOKEN=<CHANGE_ME>` уже установлен в `.env.cloud.example`.
  4. Удалить файл из истории git (rewrite history), т.к. токен ранее попал в git-tracked файл — начисто удалить из индекса.
- **DoD**: grep по токену не находит его ни в одном git-tracked файле; в env-примерах только placeholders.

### Задача P0-B. Убрать insecure TLS-fallback из production-path прошивки
- **Файлы**: `src/vqtt.cpp`, `include/config.h`
- **Шаги**:
  1. Удалить блок `if (!connectOk && useTLS && !useInsecureTLS)` с переходом на `setInsecure()` (строки ~392–401).
  2. Вместо него — явная диагностика: при ошибке CA/парсинга логировать TLS lastError и НЕ подключаться небезопасно.
  3. Оставить `mqtt_insec` (PREF_KEY_MQTT_INSECURE) ТОЛЬКО как явный dev-флаг, недоступный в production-сборке (через build_flag `-DPROD_BUILD`).
  4. Подтвердить smoke: подключение к TLS-брокеру с валидацией ISRG Root X1 проходит; без CA — соединение отвергается.
- **DoD**: в production-пути нет ни одного вызова `setInsecure()`.

### Задача P0-C. Убрать plain HTTP из мобильного приложения
- **Файлы**: `mobile/app.json`, `mobile/src/services/api.ts`, `mobile/src/services/websocket.ts`, `mobile/src/screens/SettingsScreen.tsx`
- **Шаги**:
  1. Установить `usesCleartextTraffic: false` в `expo-build-properties`.
  2. Захардкоженные `http://192.168.0.103:3000` заменить на HTTPS-URL (или вынести в конфиг/secure storage).
  3. Обновить `SettingsScreen` — адрес сервера по умолчанию `https://...`.
  4. Прокси-пути к ESP32 (`http://${deviceIp}`) оставить, но передавать admin-token только по HTTPS поверх или через защищённый прокси бэкенда.
  5. Пересобрать APK и проверить, что трафик идёт по TLS (без cleartext).
- **DoD**: в приложении нет cleartext-конфигурации; бэкенд доступен только по HTTPS.

### Задача P0-D. Усилить дефолтные токены/пароли
- **Файлы**: `include/config.h`
- **Шаги**:
  1. `DEFAULT_ADMIN_TOKEN "admin"` — принудительно сменить при первой настройке (первый вход требует установку своего токена).
  2. `ENGINEER_PASSWORD 123456` — сделать обязательной смену или читать из Preferences с генерацией случайного при первом включении.
  3. Добавить предупреждение в лог/веб-UI при использовании дефолтных значений.
- **DoD**: система не работает с заводскими дефолтами после первой настройки.

---

## 🟠 Sprint 1 — Высокий приоритет (P1)

### Задача P1-A. Backend: защитить register, MQTT_BROKER, liveness/readiness
- **Файлы**: `backend/src/server.js`, `backend/src/db.js`
- **Шаги**:
  1. `/api/auth/register` — закрыть за env-флагом `ALLOW_REGISTRATION=false` (по умолчанию выключен) или удалить публичную регистрацию.
  2. `MQTT_BROKER` — если не задан, логировать фатальную ошибку и не стартовать MQTT-подписку (или fallback на deferred retry), чтобы не падать молча.
  3. Разделить `/api/health` (liveness) и `/api/ready` (readiness): readiness = DB доступна + MQTT connected.
  4. Обновить docker-compose/railway healthcheck на `/api/ready`.
- **DoD**: health/ready разделены; register выключен по умолчанию; старт корректен без broker.

### Задача P1-B. CI pipeline
- **Файлы**: `.github/workflows/*.yml`, `backend/package.json`, `platformio.ini`
- **Шаги**:
  1. Workflow на PR/merge: сборка firmware (`platformio run`), backend (`npm ci` + `node --check`), secret-scan (`gitleaks`).
  2. Smoke: lint, unit-тесты бэкенда, сборка прошивки.
- **DoD**: green-check обязателен для merge; утечка секрета падает в CI.

---

## 🟡 Sprint 2 — Надёжность и эксплуатация (P2)

### Задача P2-A. Env-профили dev/stage/prod
- **Файлы**: `docker-compose.yml`, `.env.cloud.example`, `backend/.env.example`, `platformio.ini`
- **Шаги**: разнести dev/prod конфигурации, убрать дефолтные пароли из production-профиля.

### Задача P2-B. Логирование без чувствительных данных
- **Файлы**: `backend/src/server.js`, `src/vqtt.cpp`, `src/web_server.cpp`
- **Шаги**: убедиться, что в логах нет токенов/паролей; разделить уровни dev/prod.

### Задача P2-C. Security Baseline в документации
- **Файлы**: `README.md`, `README_RU.md`, `backend/README.md`
- **Шаги**: раздел про ротацию секретов, TLS, admin-token.

---

## 📦 Структура релиза (порядок коммитов)

1. **`security/hardening`** — P0-A…P0-D + ignore-обновление + secret-scan. *(блокирует всё)*
2. **`firmware`** — изменения `src/*`, `include/*` (TLS-fallback, дефолты).
3. **`backend`** — P1-A, безопасность register/MQTT/health.
4. **`mobile`** — P0-C, HTTPS-URL, пересборка APK.
5. **`ci`** — P1-B workflow.
6. **`docs`** — P2, обновить трекер/checklist, подписать sign-off.
7. **Tag + release notes** — `v1.4.0`.

## ✅ До релиза обязательно
- [ ] Провести secret-scan (gitleaks) — 0 находок.
- [ ] Ротация всех секретов (TG, MQTT, JWT, admin, Firebase, WQTT).
- [ ] Заполнить и подписать `docs/PRODUCTION_RELEASE_CHECKLIST.md`.
- [ ] Финальный smoke: device + backend + remote web + mobile push по TLS.