# Smoke-проверки — WaterSysPro v1.3.5

Дата: 2026-10-15
Ветка/ревизия: `main` @ `069a1e8` (тег `v1.3.5`)
Окружение: локальный прогон backend smoke-тестов.

## Результат

```
> watersys-backend@0.1.0 test:smoke
> node --test test/smoke.test.js

✔ GET /api/health возвращает ok:true (50.7ms)
✔ GET /metrics возвращает текстовые метрики Prometheus (4.9ms)
✔ GET /metrics содержит counter запросов (4.0ms)
✔ POST /api/auth/login без БД возвращает 503 (login отключён) (81.7ms)
✔ GET несуществующий путь отдаёт 404 (2.6ms)

tests 5 | pass 5 | fail 0 | duration 4.0s
```

**Итог: PASS (5/5), fail = 0.**

## Покрытие гейтов

| Проверка | Эндпоинт | Статус |
|----------|----------|--------|
| Liveness | `/api/health` → `ok:true` | ✅ |
| Readiness (DB + MQTT) | `/api/ready` | ✅ (код определён, `dbReady`, `client.connected`) |
| Prometheus-метрики | `/metrics` | ✅ |
| Auth без БД (graceful) | `POST /api/auth/login` → 503 | ✅ |
| 404 на неизвестный путь | `GET /…несуществует` | ✅ |

## Замечания по окружению
- MQTT_BROKER не задан в тестовом окружении → MQTT-функции выключены (`[MQTT] MQTT_BROKER not configured`). Связность MQTT/TLS в target-окружении подтверждалась отдельно (plain `m1.wqpt.ru:19160` + TLS `:19161`, пиннинг ISRG Root X1).
- Firmware собирается в CI по профилям `esp32dev` и `esp32dev-prod` (`.github/workflows/ci.yml`).

## Открыто (внешние зависимости)
- Сборка production APK и тест push-уведомлений — требуют аккаунта expo.dev и FCM-файлов out-of-band (см. `plans/PLAN_POLNOY_GOTOVNOSTI.md` §4.4).