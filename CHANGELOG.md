# Changelog

## [1.0.0] - 2026-03-12

### Major Changes
- **SystemContext Refactor**: Migrated FSM globals into centralized `SystemContext` for better state management and cleaner architecture.
- **Text-based Event Logging**: Event logs rewritten from binary to human-readable text format with automatic migration.
- **microSD Support (SPI)**: Initial implementation with automatic fallback to SPIFFS when SD is unavailable.

### Added
- Central state transition helper `changeState()` for simplified FSM logic.
- New configuration constants: `ADMIN_TOKEN_MAX_LEN`, `ENCODER_POLL_MS`, `SCREEN_WIDTH`, `INFO_SCREEN_PAGES`.
- MAC address formatting helpers: `getMacString()` / `getMacBytes()`.
- `prefsIsAvailable()` guards for safe NVS (Preferences) access.
- Unified admin token validation in both embedded and SPIFFS UI.
- SD card initialization routine: `initSD()` with SPI pin definitions and automatic mounting.
- **Event Logging Features**:
  - Automatic binary-to-text log migration on startup.
  - Automatic fallback to SPIFFS when SD operations fail.
  - Log filtering via engineer menu ("Log Filter" item) and REST API (`/api/log_filter`).
  - Text log format: `DD.MM.YYYY HH:MM - type:XX param:YY lvl:Z`
- **Web Interface Enhancements**:
  - WebSocket push updates and intelligent polling (pauses when tab is hidden).
  - Log management UI: download, rotate, export to SD, list files, clear logs.
  - New API endpoints:
    - `GET /api/export_logs` — export logs to SD.
    - `GET /api/sd_files` — list files on SD card.
    - `GET /api/sd_file?name=<name>` — download specific file.
    - `GET /logs.txt` — direct log download.
    - `POST /api/sd_files` — delete files with admin token.
    - `POST /api/logs` — rotate/clear logs with admin token.
  - Status endpoint (`/status`) and WebSocket broadcasts now include SD card presence flag.
  - MQTT settings and status included in status JSON.

### Improved
- Encoder polling frequency configurable and reduced.
- Code stability and SPIFFS fallback robustness.
- Documentation: SD integration guide with pinout, testing steps, and future roadmap.
- UI assets (`data/index.html`) updated to support new log and SD management features.

### Fixed
- Preferences access safety with `prefsIsAvailable()` checks.
- Admin token validation consistency across UIs.
- Network stub cleanup for better maintainability.

### Cleanup
- Removed obsolete network stubs and outdated code.
- Deleted temporary backup files (`.bak`).
- Ensured all hardcoded values (like `SCREEN_WIDTH`) use constants from `config.h`.

### Testing
- Full build validation: `platformio run` successful.
- Firmware and SPIFFS upload tested on ESP32 (COM5).
- Serial logging verified for AP startup and web server initialization.
- Smoke tests: device boots correctly, IP assignment works, web UI accessible.

---

## Notes for Future Versions
- Dynamic token validation length from API endpoint (avoid desync).
- Enhanced CI/CD with unit and integration tests.
- Support for MQTT authentication and secure connections.
- Full SD-only mode build configuration ready (`env:esp32dev-sdonly`).
