# Production Release Checklist

Дата: 2026-04-20
Назначение: обязательный предрелизный gate перед выкладкой в production.

## 1. Security Gate
- [ ] В git нет секретов (ключей, токенов, приватных сертификатов, service account JSON).
- [ ] Проверен grep по паттернам: `PRIVATE KEY|AIza|password|token|secret|mqtt://.*:.*@`.
- [ ] Все production-секреты передаются только через env/secret manager.
- [ ] Выполнена ротация секретов, если ранее был риск утечки.

## 2. Firmware Gate
- [ ] `platformio run` проходит без ошибок.
- [ ] Нет локальных привязок в production-конфиге (`upload_port`, локальные COM и т.д.).
- [ ] Проверены критичные сценарии FSM и аварийных состояний.
- [ ] Проверена MQTT/TLS связность в target окружении.

## 3. Backend Gate
- [ ] CORS ограничен allowlist-ом, wildcard не используется в production.
- [ ] Remote команды защищены admin token в заголовке `x-admin-token`.
- [ ] Включены rate limits на auth и команды.
- [ ] `/api/health` и `/metrics` доступны и возвращают корректный статус.

## 4. Data/Runtime Hygiene
- [ ] Runtime-логи и broker DB (`mosquitto.log`, `mosquitto.db`) не версионируются.
- [ ] Локальные диагностики и IDE-артефакты не попадают в git.
- [ ] Проверен `.gitignore` на актуальность.

## 5. Docs/Release Gate
- [ ] Обновлены release notes и changelog.
- [ ] Обновлен `docs/PRODUCTION_HARDENING_TRACKER.md` по факту.
- [ ] Зафиксированы результаты smoke-проверок (с датой).

## Sign-off
- Tech Lead: ____________________
- QA: ___________________________
- DevOps: _______________________
- Дата/время релиза: ____________
