# Changelog

## Unreleased
- Major refactor: migrate FSM globals into `SystemContext`.
- Added `changeState()` central transition helper.
- Added constants: `ADMIN_TOKEN_MAX_LEN`, `ENCODER_POLL_MS`, `SCREEN_WIDTH`, `INFO_SCREEN_PAGES`.
- Standardized MAC formatting: `getMacString()` / `getMacBytes()`.
- Added `prefsIsAvailable()` guards for Preferences access.
- Client UI: unified admin token validation (embedded + SPIFFS UI).
- Added initial microSD (SPI) support: `initSD()` + wiring defines; serial mount/test on startup.
- **Event logging rewritten to text format** and now stored on SD (fallback to SPIFFS when card absent).
  - Binary-format migration routine added to convert old logs automatically.
  - Automatic fallback to SPIFFS when SD operations fail (disables `sdPresent`).
  - Engineer menu gained “Log Filter” item for modifying persistence mask.
  - Logging functions updated to reopen/append and preserve previous entries.
- Web interface enhancements:
  - Status page now prefers WebSocket push updates and pauses polling when tab hidden.
  - Added log management buttons: download, rotate, export to SD, list SD files, clear logs.
  - New API endpoints `GET /api/export_logs`, `GET /api/sd_files`, `GET /logs.txt`.
  - Added `GET /api/sd_file?name=<name>` for downloading arbitrary card files and
    `POST /api/sd_files` with `{token,action:"delete",name}` for deletion.
  - `/status` and websocket broadcasts now include `sd` flag indicating card
    presence; UI displays SD status.
  - `/api/logs` extended to support POST actions (`rotate`, `clear`) with admin token.
  - `/status` JSON now includes MQTT settings/status for UI display.
- Documentation updated:
  - `docs/SD_INTEGRATION.md` fleshed out with pinout, usage, testing steps and future ideas.
  - Frontend HTML (`data/index.html`) revised to match new features.
- Build and smoke tests performed; firmware and SPIFFS uploaded.

---
