# WaterSysPro - Water Treatment System v1.1.0

IoT water treatment controller with ESP32 firmware, Node.js backend, and React Native mobile app.

![Version](https://img.shields.io/badge/version-1.1.0-blue)
![Build](https://img.shields.io/badge/build-passing-brightgreen)
![License](https://img.shields.io/badge/license-MIT-green)

---

## 🎯 System Overview

**WaterSysPro** is a complete water treatment management system featuring:

### 📱 **Hardware (ESP32)**
- Real-time monitoring via dual ultrasonic level sensors
- 7-relay control (pump1, pump2, aeration, ozone, filter, backwash, emergency)
- MQTT telemetry to external broker (plain + TLS support)
- Web-based configuration interface
- Event logging to microSD card or SPIFFS
- LCD display with rotary encoder control

### 🌐 **Backend (Node.js)**
- RESTful API for firmware management
- Real-time WebSocket updates
- User authentication with JWT
- Equipment monitoring and history
- Docker deployment ready

### 📱 **Mobile App (React Native)**
- Android APK ready-to-build
- Real-time device dashboard
- Remote relay control
- Historical data visualization
- Push notifications
- User authentication

---

## 🚀 Quick Start

### 1. Firmware (ESP32)

```bash
cd d:\WatersysPro
platformio run           # Compile
platformio run -t upload # Flash to COM5
platformio device monitor # View serial logs (115200 baud)
```

**Device IP**: http://192.168.3.13/

### 2. Backend Server

```bash
cd d:\WatersysPro\backend
npm install
npm start
# Server runs on http://localhost:3000/api

# Or use Docker
docker-compose up -d
```

### 3. Mobile App (Android APK)

```bash
cd d:\WatersysPro\mobile
npm install -g eas-cli
eas login
eas build -p android    # Cloud build (recommended)
# Get APK link in console in 5-15 minutes
```

Or use the ready-made batch script (Windows):
```bash
d:\WatersysPro\mobile\build-apk.bat
```

---

## 📋 Features

### Firmware Features
- ✅ **Dual Ultrasonic Sensors** - Real‑time tank level monitoring
- ✅ **Configurable Timers** - Flexible scheduling for all cycles
- ✅ **MQTT Telemetry** - Plain + TLS/SSL encryption support
- ✅ **Event Logging** - Human-readable text logs with filtering
- ✅ **Secure Web UI** - Admin token + password protection
- ✅ **SD Card Support** - Full SD management from web interface
- ✅ **State Machine FSM** - 5-state water treatment cycles
- ✅ **Engineer Menu** - PIN-locked advanced configuration

### MQTT Capabilities
- Connection to external brokers (tested: m1.wqpt.ru)
- Both plain (port 19160) and TLS (port 19161) modes
- Automatic TLS certificate verification skip for self-signed certs
- Exponential backoff reconnection (5s → 60s)
- Telemetry publishing every 10 seconds (configurable)
- Command subscription for remote control
- Device auto-discovery via client ID

### Mobile App Features
- 📊 Real-time device monitoring
- 🎮 Remote relay control
- 📈 Historical data charts
- 🔔 Push notifications
- 🔐 User authentication
- 📱 Navigation with tabs

---

## 📁 Project Structure

```
d:\WatersysPro/
├── src/                          # ESP32 firmware source
│   ├── main.cpp                  # Main device loop
│   ├── vqtt.cpp                  # MQTT client (PubSubClient v2.8)
│   ├── web_server.cpp            # REST API endpoints
│   ├── state_machine.cpp         # FSM implementation
│   ├── sensors.cpp               # Ultrasonic sensor reading
│   ├── relay_control.cpp         # GPIO relay switching
│   └── event_logging.cpp         # Text log management
│
├── include/                      # Header files
│   ├── config.h                  # MQTT & system constants
│   ├── structures.h              # Data structures
│   └── state_machine.h           # FSM definitions
│
├── backend/                      # Node.js server
│   ├── src/
│   │   ├── server.js             # Express app
│   │   └── db.js                 # Database interface
│   ├── package.json              # Dependencies
│   └── Dockerfile                # Docker image
│
├── mobile/                       # React Native app
│   ├── src/
│   │   ├── screens/              # UI screens (Login, Dashboard, Settings)
│   │   └── services/             # API & WebSocket services
│   ├── app.json                  # App configuration
│   ├── eas.json                  # EAS build configuration
│   └── build-apk.bat            # Windows build script
│
├── mosquitto/                    # MQTT broker (Docker)
│   ├── config/
│   │   └── mosquitto.conf
│   └── data/                     # Persistence
│
├── docs/                         # Documentation
│   ├── SD_INTEGRATION.md         # SD card guide
│   └── REFRACTOR_AND_SMOKE_SUMMARY.md
│
├── platformio.ini               # Build configuration
├── docker-compose.yml           # Full stack deployment
├── VERSION                       # Version tracking
└── CHANGELOG.md                 # Change history
```

---

## 🔧 Configuration

### MQTT Settings (Web UI: /api/mqtt)

**Via Web Interface:**
```json
{
  "enabled": true,
  "broker": "m1.wqpt.ru",
  "port": 19160,
  "tls_port": 19161,
  "secure": true,
  "user": "u_4980GX",
  "pass": "zXhkBnSi",
  "topic_base": "watersystem",
  "interval": 10,
  "insecure": false
}
```

**Via Engineer Menu:**
- Menu > MQTT Setup > TLS Enable/Disable
- Menu > MQTT Setup > TLS Port
- Menu > MQTT Setup > Broker (hostname)
- Menu > MQTT Setup > Port (plain MQTT)

### Compilation Flags (platformio.ini)

```ini
build_flags =
    -Oz                                # Aggressive size optimization
    -ffunction-sections               # Function-level GC
    -fdata-sections                   # Data-level GC
    -fno-exceptions                   # Remove C++ exceptions
    -fno-rtti                         # Remove RTTI
    -Wl,--gc-sections                 # Link-time GC
    -Wl,--strip-all                   # Strip symbols
```

**Result**: Flash reduced from 99.8% → 94.6% (68 KB freed)

---

## 📊 Memory Status

**Device**: ESP32 Dev Module (240MHz CPU, 320KB RAM, 4MB Flash)

| Metric | Value | Status |
|--------|-------|--------|
| Flash Used | 1,240,256 bytes (94.6%) | ✅ Safe |
| RAM Used | 53,540 bytes (16.3%) | ✅ Healthy |
| Free Heap | ~172 KB | ✅ Good |

---

## 🔐 MQTT Security

### Tested Configurations

1. **Plain MQTT** (Port 19160)
   ```
   Broker: m1.wqpt.ru:19160
   User: u_4980GX
   Pass: zXhkBnSi
   Status: ✅ Connected & Publishing
   ```

2. **TLS MQTT** (Port 19161)
   ```
   Broker: m1.wqpt.ru:19161
   TLS: Enabled with setInsecure()
   User: u_4980GX
   Pass: zXhkBnSi
   Status: ✅ Connected & Publishing
   ```

### Certificate Handling
- Uses `NetworkClientSecure` for TLS
- `setInsecure()` enables self-signed certificate acceptance
- Future: support certificate pinning

---

## 📚 Documentation

### Firmware
- [SD Card Integration Guide](docs/SD_INTEGRATION.md)
- [Refactoring Summary](docs/REFRACTOR_AND_SMOKE_SUMMARY.md)
- [Release Notes](CHANGELOG.md)

### Mobile App
- [Quick Start Build](mobile/START_HERE.md)
- [Full Build Guide](mobile/BUILD_APK_GUIDE.md)
- [Pre-Build Checklist](mobile/CHECKLIST.md)
- [Device Control Guide](mobile/DEVICE_CONTROL_GUIDE.md)

### Backend
- [README](backend/README.md)

---

## 🛠️ Development

### Adding New MQTT Topics

**File**: `src/vqtt.cpp`
```cpp
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    // Topic parsing logic
    String t(topic);
    String p((char*)payload, length);
    
    if (cmd == "my_command") {
        // Handle command
    }
}
```

### Adding Web API Endpoints

**File**: `src/web_server.cpp`
```cpp
server.on("/api/my_endpoint", HTTP_GET, handler_name);
```

### Creating Mobile App Screens

**File**: `mobile/src/screens/MyScreen.tsx`
```typescript
import React from 'react';
export default function MyScreen() {
  return (/* JSX */);
}
```

---

## 🧪 Testing

### Firmware Tests
```bash
# Compile without upload
platformio run -e esp32dev

# Check for errors
platformio check -e esp32dev

# Full build with verbose output
pio run -v
```

### MQTT Testing
```bash
# Test plain connection
mosquitto_pub -h m1.wqpt.ru -p 19160 -u u_4980GX -P zXhkBnSi -t test/topic -m "hello"

# Test TLS connection
mosquitto_pub -h m1.wqpt.ru -p 19161 -u u_4980GX -P zXhkBnSi --insecure -t test/topic -m "hello"
```

### Mobile App Testing
```bash
# Test in Expo Go
npm start

# Scan QR code with Expo Go app on phone
```

---

## 🚢 Deployment

### Docker Stack (includes MQTT broker)

```bash
cd d:\WatersysPro
docker-compose up -d
```

This starts:
- Backend API on http://localhost:3000
- MQTT broker on localhost:1883
- Mosquitto persistence in `mosquitto/data/`

### Manual Deployment

1. **Build Firmware**:
   ```bash
   cd src
   platformio run
   ```

2. **Start Backend**:
   ```bash
   cd backend
   npm install && npm start
   ```

3. **Deploy Mobile App**:
   ```bash
   cd mobile
   eas build -p android
   # Install APK on Android device
   ```

---

## 📈 Version History

| Version | Date | Changes |
|---------|------|---------|
| **1.1.0** | 2026-03-13 | MQTT TLS support, Mobile app ready, Memory optimization |
| **1.0.0** | 2026-03-12 | Initial release with SD integration, Event logging |

See [CHANGELOG.md](CHANGELOG.md) for detailed version history.

---

## 🤝 Contributing

1. Create a feature branch: `git checkout -b feature/my-feature`
2. Commit changes: `git commit -m "Description"`
3. Push to branch: `git push origin feature/my-feature`
4. Submit pull request

---

## 📞 Support

- **Issues**: Check existing documentation files
- **Firmware**: Edit `src/` and `include/` files, then rebuild
- **Mobile App**: See `mobile/START_HERE.md` for quick start
- **Backend**: See `backend/README.md`

---

## 📄 License

MIT License - See LICENSE file for details

---

**Device IP**: http://192.168.3.13/
**MQTT Broker**: m1.wqpt.ru (ports 19160=plain, 19161=TLS)
**Mobile App**: Ready to build with `eas build -p android`

Last Updated: March 13, 2026
```

* the default serial port is `COM5`; override by passing a port name or
  setting the `SPIFFS_OFFSET` environment variable if your partition table
  differs from the stock `esp32dev` layout.
* requirements:
  * `mklittlefs` and `esptool.py` on your PATH (PlatformIO installations
    already include both).  The script will also look under
    `~/.platformio/packages` if they're not on PATH.
  * `pyserial` installed in the Python environment used to run the script
    (`pip install pyserial`).  This is only needed for the mini terminal that
    opens automatically after flashing.

After the flash finishes the script drops you into a serial monitor so you
can watch the log messages as the device mounts the card and moves the
files.

A GitHub Actions workflow (`.github/workflows/check_libs.yml`) runs on every
push and pull request.  It executes `scripts/analyze_libs.py`, which verifies
that every library declared in `lib_deps` is actually referenced by the
source.  The job fails if unused dependencies are detected, helping keep the
flash footprint under control.



* **SD-only firmware only:**
The project is configured to build without any SPIFFS image; all web UI and
static assets are expected to reside on a mounted SD card.  The single
`esp32dev` environment in `platformio.ini` disables SPIFFS (`board_build.filesystem = none`) and compiles with `-DUSE_SPIFFS=0`.