# Руководство по сборке APK для WatersysMobile

## Способ 1: EAS Build (Облачная сборка - РЕКОМЕНДУЕТСЯ)

### Шаг 1: Установка EAS CLI
```bash
npm install -g eas-cli
```

### Шаг 2: Авторизация
```bash
eas login
```
Будет открыта страница браузера для входа или регистрации на Expo.

### Шаг 3: Инициализация проекта (если не инициализирован)
```bash
eas build:configure
```

### Шаг 4: Сборка APK
```bash
eas build -p android --local
```

Или для облачной сборки:
```bash
eas build -p android
```

**Параметры:**
- `--local` - собирает локально (требует Android SDK)
- Без флага - облачная сборка (recommended)

### Шаг 5: Загрузка и установка
После завершения сборки:
1. Скачайте APK файл из ссылки, которую выдаст EAS
2. Передайте на Android устройство
3. Установите файл

---

## Способ 2: Локальная сборка (Android Studio + Gradle)

### Требования:
- Java JDK 11+ 
- Android SDK
- Gradle
- Android Studio (опционально)

### Шаг 1: Установка зависимостей
```bash
npm install
```

### Шаг 2: Сборка через Expo
```bash
npm run android
```

Или через EAS с локальной сборкой:
```bash
eas build -p android --local
```

---

## Способ 3: Быстрое тестирование (Expo Go)

Для быстрого тестирования БЕЗ APK:

```bash
npm start
```

Затем откройте приложение **Expo Go** на Android телефоне и отсканируйте QR-код.

---

## Конфигурация для APK

Текущие настройки в `app.json`:
```json
{
  "android": {
    "package": "com.igas65.watersysmobile"
  }
}
```

### Опции которые можно добавить:

```json
{
  "android": {
    "package": "com.igas65.watersysmobile",
    "versionCode": 1,
    "permissions": ["INTERNET", "ACCESS_NETWORK_STATE"],
    "adaptiveIcon": {
      "foregroundImage": "./assets/adaptive-icon.png",
      "backgroundColor": "#FFFFFF"
    }
  }
}
```

---

## Решение проблем

### "Command not found: eas"
```bash
npm install -g eas-cli
```

### "Java is not installed"
Установите Java JDK 11+ с https://www.oracle.com/java/technologies/downloads/

### "Android SDK not found"
Установите Android Studio и Android SDK tools

### Не получается авторизоваться в Expo
```bash
eas logout
eas login
```

---

## Проверка готовности приложения

Перед сборкой:
1. ✅ `package.json` - зависимости установлены
2. ✅ `app.json` - конфигурация для Android указана
3. ✅ `eas.json` - настройки сборки готовы
4. ✅ Код TypeScript - проверить на ошибки

Проверка ошибок:
```bash
npm run start
```

---

## Дополнительные команды

### Просмотр статус сборки
```bash
eas build:list
```

### Отмена сборки
```bash
eas build:cancel <BUILD_ID>
```

### Информация о проекте
```bash
eas whoami
```

---

## Следующие шаги

После создания APK:
1. Протестируйте на реальном устройстве
2. Если нужно распространять - загрузите на Google Play Store
3. Настройте signing certificate для Production сборок

Подробнее: https://docs.expo.dev/build/setup/
