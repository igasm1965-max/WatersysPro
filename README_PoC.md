PoC (ESP32 ↔ MQTT ↔ Node.js ↔ Mobile)

What I created for you:
- `backend/` — Express + MQTT bridge + Socket.IO + Postgres
- `mobile/` — Expo React Native skeleton (Dashboard + Settings)
- `esp32_poc/esp32_mqtt_example.ino` — Arduino-style example publishing telemetry
- `docker-compose.yml` — mosquitto + postgres + backend

Quick start (recommended):
1) Start infrastructure with Docker:
   docker-compose up -d

2) Backend (if you want local instead of docker):
   cd backend
   npm install
   npm run start

3) Mobile:
   cd mobile
   npm install
   expo start
   (open Expo Go on your phone and scan QR; edit `mobile/src/services/api.ts` to point to your backend IP)

4) ESP32:
   - Edit `esp32_poc/esp32_mqtt_example.ino`: set `WIFI_SSID`, `WIFI_PASS`, and the broker IP (your dev machine IP)
   - Upload sketch with Arduino IDE or PlatformIO

Test flows:
- Use MQTT Explorer or mosquitto_sub to monitor topics `device/esp32_001/telemetry`
- Send command: curl -X POST http://localhost:3000/api/command/esp32_001 -H "Content-Type: application/json" -d '{"command":"start_flush"}'

Next steps I can do for you:
- Wire up authentication/JWT
- Add detailed DB migrations and example queries
- Polish the mobile UI and onboarding

Tell me which next step to implement and I will continue.