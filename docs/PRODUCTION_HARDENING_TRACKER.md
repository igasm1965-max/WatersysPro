# Production Hardening Tracker

Статус: рабочий трекер исполнения
Дата старта: 2026-03-19

Правила статусов:
- `To Do` - не начато
- `In Progress` - в работе
- `Blocked` - есть внешний блокер
- `Done` - выполнено и проверено

Шаблон owner:
- `Backend` - backend/API
- `Firmware` - ESP32/прошивка
- `DevOps` - CI/CD, секреты, деплой
- `QA` - тесты, smoke/regression
- `Docs` - документация

## P0 - Блокеры релиза

### P0.1 Секреты и креды: удалить из кода/доков, оставить только env
- [ ] **Task ID**: P0.1
- **Owner**: DevOps + Backend + Docs
- **Estimate**: 1.0d
- **Status**: To Do
- **Files**: `backend/src/server.js`, `docker-compose.yml`, `backend/.env.example`, `README.md`, `README_RU.md`
- **DoD**:
  - Нет рабочих паролей/токенов в git-tracked файлах.
  - Все секреты читаются только из переменных окружения.
  - Документация содержит только placeholders (`<CHANGE_ME>`).
  - Пройден ручной grep по паттернам `password|token|secret|mqtt://.*:.*@`.

### P0.2 MQTT TLS: запретить небезопасный insecure по умолчанию
- [ ] **Task ID**: P0.2
- **Owner**: Firmware
- **Estimate**: 1.0d
- **Status**: To Do
- **Files**: `src/vqtt.cpp`, `include/config.h` (при необходимости), `README.md`, `README_RU.md`
- **DoD**:
  - Убрано безусловное `setInsecure()` из production-path.
  - Включена проверка сертификата (CA/pinning).
  - Insecure-режим возможен только отдельным явным dev-флагом.
  - Подключение к TLS брокеру подтверждено smoke-тестом.

### P0.3 Remote API auth: принимать admin token только из заголовка
- [ ] **Task ID**: P0.3
- **Owner**: Backend
- **Estimate**: 0.5d
- **Status**: To Do
- **Files**: `backend/src/server.js`, `backend/README.md`
- **DoD**:
  - Удалено чтение токена из query/body.
  - Допускается только заголовок `x-admin-token`.
  - Ошибки 401/403 покрыты smoke-сценарием.
  - Документация endpoint-ов обновлена.

### P0.4 CORS hardening: убрать wildcard в production
- [ ] **Task ID**: P0.4
- **Owner**: Backend
- **Estimate**: 0.5d
- **Status**: To Do
- **Files**: `backend/src/server.js`, `backend/.env.example`, `docker-compose.yml`
- **DoD**:
  - `cors()` настроен через allowlist origin-ов из env.
  - В production-конфиге нет `*`.
  - Проверены запросы с разрешенного/запрещенного origin.

### P0.5 Release security gate: обязательный чек перед production
- [ ] **Task ID**: P0.5
- **Owner**: DevOps + QA + Docs
- **Estimate**: 0.5d
- **Status**: To Do
- **Files**: `docs/PRODUCTION_HARDENING_TRACKER.md`, `DOCUMENTATION_INDEX.md`
- **DoD**:
  - Добавлен и используется pre-release checklist.
  - Перед релизом фиксируются результаты: build, smoke, secret-scan.

## P1 - Высокий приоритет

### P1.1 CI pipeline: build + smoke + security scan
- [ ] **Task ID**: P1.1
- **Owner**: DevOps
- **Estimate**: 1.5d
- **Status**: To Do
- **Files**: `.github/workflows/*` (new), `backend/package.json`, `platformio.ini`
- **DoD**:
  - На PR/merge автоматически запускаются проверки.
  - Firmware build и backend smoke проходят в CI.
  - Есть dependency/secret scan (минимум базовый).

### P1.2 Backend readiness: health зависит от DB/MQTT
- [ ] **Task ID**: P1.2
- **Owner**: Backend
- **Estimate**: 1.0d
- **Status**: To Do
- **Files**: `backend/src/server.js`, `backend/src/db.js`, `backend/README.md`
- **DoD**:
  - Разделены `liveness` и `readiness` endpoint-ы.
  - `readiness` учитывает доступность DB и MQTT.
  - Docker/оркестратор может проверять готовность корректно.

### P1.3 Remote command protections: rate-limit + audit
- [ ] **Task ID**: P1.3
- **Owner**: Backend
- **Estimate**: 1.0d
- **Status**: To Do
- **Files**: `backend/src/server.js`, `backend/README.md`
- **DoD**:
  - Введен rate-limit на `/api/remote/command`.
  - Логируется: время, источник, target, result.
  - Подтверждены корректные 429/401 сценарии.

### P1.4 FSM/sensors regression smoke pack
- [ ] **Task ID**: P1.4
- **Owner**: Firmware + QA
- **Estimate**: 1.5d
- **Status**: To Do
- **Files**: `src/state_machine.cpp`, `src/sensors.cpp`, `src/main.cpp`, `docs/*`
- **DoD**:
  - Есть фиксированный набор smoke/regression сценариев.
  - Проверены кейсы: `ping_cm()==0`, stuck flags, переходы BACKWASH->FILTRATION.
  - Результаты теста задокументированы.

## P2 - Улучшения надежности и эксплуатации

### P2.1 Env profiles: dev/stage/prod
- [ ] **Task ID**: P2.1
- **Owner**: DevOps
- **Estimate**: 1.0d
- **Status**: To Do
- **Files**: `docker-compose.yml`, `backend/.env.example`, `platformio.ini`
- **DoD**:
  - Явные профили конфигураций без смешивания production и dev.
  - Production-профиль не содержит дефолтных паролей.

### P2.2 Firmware config cleanup: убрать локальные привязки
- [ ] **Task ID**: P2.2
- **Owner**: Firmware
- **Estimate**: 0.5d
- **Status**: To Do
- **Files**: `platformio.ini`
- **DoD**:
  - `upload_port` и локальные dev-параметры вынесены из production-конфига.
  - Документирован стандарт сборки для CI и релиза.

### P2.3 Logging policy: исключить утечку чувствительных данных
- [ ] **Task ID**: P2.3
- **Owner**: Backend + Firmware
- **Estimate**: 1.0d
- **Status**: To Do
- **Files**: `backend/src/server.js`, `src/vqtt.cpp`, `src/web_server.cpp`
- **DoD**:
  - В логах нет токенов/паролей/секретов.
  - Уровни логов разделены `dev` и `prod`.

### P2.4 Security docs baseline
- [ ] **Task ID**: P2.4
- **Owner**: Docs
- **Estimate**: 0.5d
- **Status**: To Do
- **Files**: `README.md`, `README_RU.md`, `backend/README.md`
- **DoD**:
  - Добавлен раздел Security Baseline.
  - Описаны правила ротации секретов, TLS и admin-token.

## Sprint-план (рекомендуемый)
- [ ] **Sprint 1**: P0.1-P0.5
- [ ] **Sprint 2**: P1.1-P1.4
- [ ] **Sprint 3**: P2.1-P2.4

## Журнал выполнения
- 2026-03-19: Трекер создан.
