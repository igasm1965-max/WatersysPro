# WaterSysPro - Water Treatment Control System

WaterSysPro is a full-stack water treatment automation project built around ESP32 firmware, a Node.js backend, and a React Native mobile app.

![Version](https://img.shields.io/badge/version-1.1.0-blue)
![Status](https://img.shields.io/badge/status-active-brightgreen)
![License](https://img.shields.io/badge/license-MIT-green)

## Project Scope

WaterSysPro combines three layers:

### 1. ESP32 Firmware
- Controls 7 relays (pumps, aeration, ozone, filtration, backwash, emergency)
- Implements an FSM for water treatment operating cycles
- Reads dual ultrasonic level sensors
- Exposes a web interface for configuration and diagnostics
- Publishes telemetry and receives control commands via MQTT
- Stores human-readable event logs
- Serves static UI assets from SD card

### 2. Backend (Node.js)
- REST API for integration and service workflows
- WebSocket channel for real-time updates
- Docker-ready local deployment flow

### 3. Mobile App (React Native)
- Login, dashboard, and settings screens
- API and WebSocket clients
- Android build scripts and EAS cloud build support

## High-Level Data Flow

1. ESP32 collects sensor values and actuator state.
2. FSM decides transitions between operating modes.
3. Device publishes telemetry to MQTT and exposes state over web/API endpoints.
4. Backend and mobile app display status/history and can send control actions.

## Repository Layout

- src/ - ESP32 firmware source code
- include/ - firmware headers
- backend/ - Node.js service and Docker files
- mobile/ - React Native application
- docs/ - technical docs, summaries, and trackers
- mosquitto/ - local broker config/data for containerized runs
- platformio.ini - PlatformIO build/upload configuration
- docker-compose.yml - stack startup for backend and infrastructure

## Requirements

### Firmware Environment
- PlatformIO (CLI or VS Code extension)
- ESP32 Dev Module board
- USB connection (default upload port: COM5)

### Backend Environment
- Node.js 18+
- npm
- Docker + Docker Compose (optional)

### Mobile Environment
- Node.js 18+
- npm
- EAS CLI (for Android cloud builds)

## Quick Start

### 1. Build Firmware

```bash
platformio run
```

### 2. Upload Firmware to ESP32

```bash
platformio run -t upload
```

Use explicit port when needed:

```bash
platformio run -t upload -e esp32dev --upload-port COM5
```

### 3. Open Serial Monitor

```bash
platformio device monitor
```

## Backend Run Modes

### Option A: Local Node.js

```bash
cd backend
npm install
npm start
```

### Option B: Docker Compose

```bash
docker-compose up -d
```

## Mobile App Setup

```bash
cd mobile
npm install
```

Android cloud build:

```bash
eas build -p android
```

Windows helper script:

```bash
mobile\build-apk.bat
```

## Firmware Configuration Notes

Primary settings are in platformio.ini:
- platform: espressif32
- board: esp32dev
- framework: arduino
- monitor speed: 115200
- default upload port: COM5
- SPIFFS disabled, SD-first runtime model

## MQTT and Remote Control

WaterSysPro supports:
- External MQTT broker connectivity
- Plain and TLS modes
- Periodic telemetry publishing
- Command subscriptions for remote control operations

Security recommendation:
- Keep real MQTT credentials outside source control
- Avoid publishing live credentials in documentation/screenshots

## Web Interface and API

On-device web interface supports:
- Live status review
- Parameter configuration
- Service operations and maintenance actions

The backend exposes API endpoints for external clients and integrations.

## Typical Operating Workflows

### First Bring-Up

1. Build and upload firmware.
2. Connect to the device web UI.
3. Validate sensors and relay switching in manual checks.
4. Configure network and MQTT parameters.
5. Confirm telemetry and logs are being generated.

### Diagnostics

1. Inspect boot/runtime output in serial monitor.
2. Verify ESP32 web interface availability.
3. Check MQTT broker connection and topic traffic.
4. Compare device logs with backend-side observations.

## Development and Maintenance

### Useful VS Code Tasks

- PlatformIO: Build
- PlatformIO: Upload
- PlatformIO: Upload (COM5)
- PlatformIO: Device List
- PlatformIO: Upload FS (COM5)

### Change Management Guidelines

1. Prefer small and focused commits.
2. Validate firmware and backend behavior before commit.
3. Test FSM and sensor logic changes on hardware.
4. Avoid mixing broad formatting updates with functional changes.

## Documentation Index

Key documents in this repository:
- README_RU.md - detailed Russian overview
- CHANGELOG.md and CHANGELOG_RU.md - release history
- RELEASE_NOTES_v1.1.0.md and RELEASE_NOTES_v1.1.0_RU.md - release details
- DOCUMENTATION_INDEX.md - documentation navigation map
- docs/SD_INTEGRATION.md - SD integration details
- docs/PRODUCTION_HARDENING_TRACKER.md - production hardening tasks

## Current State

- Active development repository.
- Main stack: ESP32 + Node.js + React Native.
- Production rollouts should include hardening and secret-review steps.

## License

MIT. See LICENSE for details.
