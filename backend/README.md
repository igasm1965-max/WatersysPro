PoC backend for WaterSystem (ESP32 → MQTT → Node.js → Mobile)

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

Notes:
- MQTT broker is available on tcp://localhost:1883 (in compose -> mosquitto)
- DB is Postgres (service `postgres`). Tables are created automatically on startup.
- Push notifications use Expo (optional): register tokens via POST /api/push/register
