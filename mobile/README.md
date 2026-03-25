Mobile (Expo) PoC

Подробное руководство по созданию и сборке приложения:
- [MOBILE_APP_CREATION_GUIDE_RU.md](MOBILE_APP_CREATION_GUIDE_RU.md)

Quick start:
1. cd mobile
2. npm install
3. expo start
4. Open Expo Go on your phone and scan the QR (ensure phone can reach your dev machine)

Important:
- Update `mobile/src/services/api.ts` and `websocket.ts` to point to your backend IP (replace 10.0.0.100).
- Register push tokens via Settings -> Register push token.
