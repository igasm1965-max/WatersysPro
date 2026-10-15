# WaterSys Mobile (React Native / Expo)

Мобильное приложение для управления системой водоочистки WaterSysPro.

## Возможности

- **DashboardScreen** — мониторинг устройства в реальном времени (MQTT live telemetry), управление реле
- **SettingsScreen** — настройки сервера, MQTT, push-уведомлений и PIN
- **LogsScreen** — просмотр логов через MQTT-канал (`watersystem/logs`, `watersystem/logs/event`)
- **PinScreen** — защита 4-значным PIN-кодом
- Push-уведомления (Expo), безопасное хранение кредов (expo-secure-store)

## Быстрый старт

```bash
cd mobile
npm install
npm start
```

Откройте **Expo Go** на устройстве и отсканируйте QR-код (устройство должно иметь доступ к dev-машине).

## Сборка APK

См. [BUILD.md](BUILD.md) — облачная сборка EAS, локальная сборка или тестирование в Expo Go.

## Конфигурация

Перед запуском убедитесь, что адрес backend в `src/services/api.ts` указывает на ваш сервер/устройство.

- Регистрация push-токена: Настройки → Register push token
- Настройка push-уведомлений Firebase: [FCM_SETUP_RU.md](FCM_SETUP_RU.md)

## Документация

- [BUILD.md](BUILD.md) - сборка APK
- [DEVICE_CONTROL_GUIDE.md](DEVICE_CONTROL_GUIDE.md) - использование приложения
- [FCM_SETUP_RU.md](FCM_SETUP_RU.md) - push-уведомления
- Скрипты сборки: `build-apk.bat` (Windows), `build-apk.sh` (Linux/Mac)
