PoC backend for WaterSystem (ESP32 → MQTT → Node.js → Mobile)

Remote web control is included and served by backend at /remote/

Quick start (requires Docker):

1. Start services:
   docker-compose up -d

2. Check logs:
   docker-compose logs -f backend

3. REST API is available at http://localhost:3000
   - GET /api/health
   - GET /api/devices
   - GET /api/status/:deviceId
   - POST /api/command/:deviceId
   - GET /remote/ (new remote web panel)
   - GET /api/remote/config
   - GET /api/remote/latest (protected by token by default)
   - POST /api/remote/command (protected by token by default)

Remote panel quick setup:

1. Configure backend env variables:
   - MQTT_BROKER=mqtt://... or mqtts://...
   - MQTT_TOPIC_BASE=watersystem
   - REMOTE_REQUIRE_TOKEN=true
   - REMOTE_ADMIN_TOKEN=<strong random token>
2. Start backend and open http://<server-host>:3000/remote/
3. Enter REMOTE_ADMIN_TOKEN in the panel and send ON/OFF commands.

MQTT topic contract for current firmware:

- Telemetry: watersystem/telemetry
- State: watersystem/state
- Presence: watersystem/status
- Commands: watersystem/cmd/<target>
- Supported targets: pump1, pump2, aeration, ozone, filter, backwash

Notes:
- MQTT broker is available on tcp://localhost:1883 (in compose -> mosquitto)
- DB is Postgres (service `postgres`). Tables are created automatically on startup.
- Push notifications use Expo (optional): register tokens via POST /api/push/register
