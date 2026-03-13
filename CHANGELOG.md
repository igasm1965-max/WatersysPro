# Changelog

## [1.1.0] - 2026-03-13

### Major Features
- **MQTT TLS Support**: Complete migration from AsyncMqttClient to PubSubClient v2.8.0 with full TLS/SSL support
- **Mobile App Ready**: Full React Native Android application created and ready for APK building
- **Memory Optimization**: Flash usage reduced from 99.8% to 94.6% through aggressive compilation flags

### Added (Firmware)
- **MQTT Enhancements**:
  - TLS/SSL encryption support for secure MQTT connections (port 19161 tested with m1.wqpt.ru)
  - Dual MQTT port configuration (plain and TLS)
  - Web API for MQTT configuration: `/api/mqtt` (GET/POST)
  - MQTT menu items in engineer menu: TLS Enable, TLS Port
  - Automatic TLS client selection based on configuration
  - WiFi state change detection for proper MQTT initialization
  - Exponential backoff reconnection (5s → 60s)
  - Certificate verification skip support via `setInsecure()`

- **Configuration**:
  - New preferences keys: `PREF_KEY_MQTT_SECURE`, `PREF_KEY_MQTT_TLS_PORT`, `PREF_KEY_MQTT_INSECURE`
  - Default TLS port: 8883
  - Persistent storage of all MQTT settings

- **Web Interface**:
  - Two HTML versions: main interface and simplified version
  - Both updated with TLS Port and TLS Enable fields
  - Form-based MQTT configuration in web UI
  - Live MQTT status display

### Added (Mobile App)
- **Complete React Native Application**:
  - LoginScreen: Authentication (login/register)
  - DashboardScreen: Real-time device monitoring
  - SettingsScreen: User settings and preferences
  - Complete API service integration
  - WebSocket support for live updates
  - Push notification support
  - Bottom Tab Navigation

- **Documentation** (7 comprehensive guides):
  - START_HERE.md: Quick 5-minute overview
  - QUICK_START_BUILD.md: Step-by-step APK building
  - BUILD_APK_GUIDE.md: Complete build documentation
  - CHECKLIST.md: Pre-build verification checklist
  - DEVICE_CONTROL_GUIDE.md: Application usage guide
  - README_BUILD.md: Build summary
  - FINAL_SUMMARY.md: Final overview

- **Build Scripts**:
  - build-apk.bat: Windows batch script for APK building
  - build-apk.sh: Linux/Mac bash script
  - COMMANDS.sh: Copy-paste command reference

- **Configuration Updates**:
  - app.json: Android configuration with permissions
  - eas.json: EAS build configuration
  - tsconfig.json: TypeScript configuration

### Improved (Firmware)
- **Compilation Optimization**:
  - Changed optimization from `-Os` to `-Oz` (aggressive size reduction)
  - Added function/data sections: `-ffunction-sections -fdata-sections`
  - Removed exceptions and RTTI: `-fno-exceptions -fno-rtti`
  - Added link-time GC and stripping: `-Wl,--gc-sections -Wl,--strip-all`
  - Result: Freed 68 KB of flash memory

- **MQTT Code**:
  - Complete rewrite of vqtt.cpp (~400 lines)
  - Synchronous PubSubClient API (simpler than async)
  - Better error messages and logging
  - Device ID format: `w_sys_XXXX_YYYYYYYY` (WiFi MAC + random)
  - Proper WiFi/MQTT state synchronization

- **Engineer Menu**:
  - Added TLS Enable/Disable toggle
  - Added TLS Port editing
  - Integrated with existing MQTT settings
  - Clear MQTT now removes all TLS settings

### Fixed (Firmware)
- **Critical MQTT Bug**: Fixed null pointer crash when WiFi not connected during initVQTT()
  - Now initializes default client unconditionally
  - Only connects when WiFi transitions to connected state
- **LTO Incompatibility**: Removed Link-Time Optimization due to linker conflicts
  - Switched to manual section stripping instead
  - Maintains similar memory savings without errors

### Tested
- ✅ Plain MQTT: Connected to m1.wqpt.ru:19160 with credentials REDACTED_MQTT_USER/REDACTED_MQTT_PASS
- ✅ TLS MQTT: Connected to m1.wqpt.ru:19161 (TLS verified working)
- ✅ Telemetry publishing: JSON format with all metrics (uptime, IP, tank levels, relay states)
- ✅ Command subscription: MQTT→device relay control working
- ✅ Web API: /api/mqtt endpoint functional
- ✅ Automatic reconnection: Exponential backoff verified
- ✅ WiFi state changes: Proper MQTT initialization on reconnection

### Device Status
- IP Address: 192.168.3.13 (discovered)
- Flash Usage: 94.6% (1,240,256 bytes) - safe operating range
- RAM Usage: 16.3% (53,540 bytes)  
- Free Heap: ~172 KB during operation
- MQTT Status: Both plain and TLS modes operational

### Cleanup
- Removed AsyncMqttClient library dependency
- Deleted old MQTT implementation references
- Cleaned up compilation flags (removed failed LTO)

---

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
