# Safe Release Commit Plan (2026-04-20)

Цель: разбить текущие изменения на управляемые коммиты перед production релизом.

## Принципы
- Сначала коммитим security/hygiene.
- Затем firmware изменения.
- Затем backend/mobile.
- Документацию и презентационный пакет в конце.

## Commit 1: Security + Release Hygiene

### Файлы
- `.gitignore`
- `.env.example`
- `.env.cloud.example`
- `backend/.env.example`
- `platformio.ini`
- `docs/PRODUCTION_RELEASE_CHECKLIST.md`
- `docs/PRODUCTION_READINESS_REPORT_2026-04-20.md`
- `docs/PRODUCTION_HARDENING_TRACKER.md`
- `DOCUMENTATION_INDEX.md`
- `mosquitto/data/mosquitto.db` (удаление из git)
- `mosquitto/log/mosquitto.log` (удаление из git)

### Команды
```powershell
git add -- .gitignore .env.example .env.cloud.example backend/.env.example platformio.ini docs/PRODUCTION_RELEASE_CHECKLIST.md docs/PRODUCTION_READINESS_REPORT_2026-04-20.md docs/PRODUCTION_HARDENING_TRACKER.md DOCUMENTATION_INDEX.md
git add --all -- mosquitto/data/mosquitto.db mosquitto/log/mosquitto.log
git commit -m "chore(security): harden release hygiene and remove runtime artifacts"
```

## Commit 2: Firmware (FSM, sensors, display, web security)

### Файлы
- `include/config.h`
- `include/engineer_menu.h`
- `include/structures.h`
- `src/display.cpp`
- `src/emergency.cpp`
- `src/engineer_menu.cpp`
- `src/main.cpp`
- `src/menu_data.cpp`
- `src/sensors.cpp`
- `src/state_machine.cpp`
- `src/vqtt.cpp`
- `src/web_server.cpp`

### Команды
```powershell
git add -- include/config.h include/engineer_menu.h include/structures.h src/display.cpp src/emergency.cpp src/engineer_menu.cpp src/main.cpp src/menu_data.cpp src/sensors.cpp src/state_machine.cpp src/vqtt.cpp src/web_server.cpp
git commit -m "feat(firmware): finalize FSM/sensor fixes, MQTT TLS resiliency and settings auth flow"
```

## Commit 3: Backend + Mobile

### Файлы
- `backend/package.json`
- `backend/src/db.js`
- `backend/src/server.js`
- `mobile/app.json`
- `mobile/package.json`
- `mobile/src/screens/SettingsScreen.tsx`

### Команды
```powershell
git add -- backend/package.json backend/src/db.js backend/src/server.js mobile/app.json mobile/package.json mobile/src/screens/SettingsScreen.tsx
git commit -m "feat(app): strengthen backend remote controls and mobile push registration flow"
```

## Commit 4: UI + Release Docs Bundle

### Файлы
- `RELEASE_NOTES_v1.1.0_RU.md`
- `documentation_bundle/README_PRESENTATION.md`
- `documentation_bundle/WatersysPro_Presentation.html`
- `documentation_bundle/WatersysPro_Presentation_Executive.html`
- `закинуть на сд/index.html`
- `закинуть на сд/index — копия.html`

### Команды
```powershell
git add -- RELEASE_NOTES_v1.1.0_RU.md documentation_bundle/README_PRESENTATION.md documentation_bundle/WatersysPro_Presentation.html documentation_bundle/WatersysPro_Presentation_Executive.html "закинуть на сд/index.html" "закинуть на сд/index — копия.html"
git commit -m "docs(ui): finalize release notes, presentation bundle and SD web UI updates"
```

## Финальная проверка перед tag
```powershell
git status
git log --oneline -n 8
```

Если `git status` чистый, можно ставить release tag.
