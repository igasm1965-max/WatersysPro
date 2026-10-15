# Production Release Checklist

Дата: 2026-04-20 (актуализировано: 2026-10-15)
Версия релиза: **v1.3.5**
Назначение: обязательный предрелизный gate перед выкладкой в production.

## 1. Security Gate
- [x] В git нет секретов (ключей, токенов, приватных сертификатов, service account JSON).
- [x] Проверен grep по паттернам: `PRIVATE KEY|AIza|password|token|secret|mqtt://.*:.*@` — реальных утечек не найдено (`mobile/google-services*.json`, `firebase-adminsdk*.json` покрыты `.gitignore`).
- [x] Все production-секреты передаются только через env/secret manager.
- [x] Выполнена ротация секретов, если ранее был риск утечки (2026-09-08: заменён утёкший Telegram-токен, admin-token больше не использует дефолт `admin`).

## 2. Firmware Gate
- [x] `platformio run` проходит без ошибок (профили `esp32dev` и `esp32dev-prod` собираются; прогон в CI).
- [x] Нет локальных привязок в production-конфиге (`upload_port` и локальные dev-параметры вынесены из production-профиля).
- [x] Проверены критичные сценарии FSM и аварийных состояний.
- [x] Проверена MQTT/TLS связность в target окружении (plain `m1.wqpt.ru:19160` + TLS `:19161`, пиннинг CA ISRG Root X1).

## 3. Backend Gate
- [x] CORS ограничен allowlist-ом (`ALLOWED_ORIGINS` из env), wildcard не используется в production.
- [x] Remote команды защищены admin token в заголовке `x-admin-token` (приём токена из query/body убран).
- [x] Включены rate limits на auth (5/15мин) и команды (30/мин).
- [x] `/api/health` (liveness) и `/api/ready` (readiness: DB + MQTT) доступны и возвращают корректный статус; `/metrics` отдаёт метрики Prometheus.

## 4. Data/Runtime Hygiene
- [x] Runtime-логи и broker DB (`mosquitto.log`, `mosquitto.db`) не версионируются.
- [x] Локальные диагностики и IDE-артефакты не попадают в git.
- [x] Проверен `.gitignore` на актуальность.

## 5. Docs/Release Gate
- [x] Обновлены release notes и changelog.
- [x] Обновлен `docs/archive/PRODUCTION_HARDENING_TRACKER.md` по факту (2026-10-15).
- [x] Зафиксированы результаты smoke-проверок с датой (см. `docs/SMOKE_RESULTS_2026-10-15.md`).

## Открытые (внешние) пункты
- [ ] Сборка production APK (EAS / `mobile/build-apk.bat`) — требуется аккаунт expo.dev.
- [ ] Настройка Firebase FCM (`google-services.json`, credentials в EAS) — файлы out-of-band.
- [ ] Тест push-уведомлений на реальном устройстве.

## Sign-off
- Tech Lead: ____________________
- QA: ___________________________
- DevOps: _______________________
- Дата/время релиза: 2026-10-15, WaterSysPro v1.3.5 🏷️
