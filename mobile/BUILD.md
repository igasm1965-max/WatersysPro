# Сборка мобильного приложения (APK)

Руководство по сборке Android APK для WaterSys Mobile. Package ID: `com.igas65.watersysmobile`.

## Способ 1: Облачная сборка EAS Build (рекомендуется)

### Шаг 1 — Установка EAS CLI (один раз)

```bash
npm install -g eas-cli
```

### Шаг 2 — Авторизация (один раз)

```bash
eas login
```

Откроется браузер — создайте аккаунт или авторизуйтесь на expo.dev.

### Шаг 3 — Инициализация проекта (если не инициализирован)

```bash
eas build:configure
```

### Шаг 4 — Сборка APK

```bash
cd mobile
eas build -p android
```

Сборка займёт 5–15 минут. При запросе выберите тип `apk` и профиль `default`. По завершении вы получите ссылку на скачивание APK.

## Способ 2: Локальная сборка

Требуется: Java JDK 11+, Android SDK, Gradle.

```bash
cd mobile
npm install
eas build -p android --local
```

Либо через Expo напрямую:

```bash
npm run android
```

## Способ 3: Быстрое тестирование без APK (Expo Go)

```bash
cd mobile
npm install
npm start
```

Откройте приложение **Expo Go** на Android-устройстве и отсканируйте QR-код (устройство должно иметь доступ к dev-машине).

## Установка APK на устройство

1. Скачайте APK-файл со ссылки из консоли (или dashboard.expo.dev → раздел сборок).
2. Передайте на Android-устройство (USB, облако).
3. Откройте APK-файл и подтвердите установку.

⚠️ Должна быть разрешена установка из неизвестных источников: Настройки → Безопасность → Установка из неизвестных источников.

## Конфигурация в app.json

```json
{
  "expo": {
    "version": "1.0.1",
    "android": {
      "package": "com.igas65.watersysmobile",
      "permissions": ["INTERNET", "ACCESS_NETWORK_STATE"]
    }
  }
}
```

При каждой сборке новой версии обновляйте поле `version` в `app.json`.

## Конфигурация подключения к backend

По умолчанию в `src/services/api.ts` указан адрес backend. Замените его на IP вашего сервера/устройства перед сборкой.

## Частые проблемы

| Проблема | Решение |
|----------|---------|
| `Command not found: eas` | `npm install -g eas-cli` |
| `Not authenticated` | `eas logout && eas login` |
| `Cannot connect to API` | Проверьте интернет, повторите `eas build -p android` |

## Полезные команды

```bash
eas build:list          # статус всех сборок
eas build:view <ID>     # логи конкретной сборки
```

## Дополнительно

- Подробная документация Expo: https://docs.expo.dev/build/setup/
- Использование приложения: [DEVICE_CONTROL_GUIDE.md](DEVICE_CONTROL_GUIDE.md)
- Настройка push-уведомлений: [FCM_SETUP_RU.md](FCM_SETUP_RU.md)