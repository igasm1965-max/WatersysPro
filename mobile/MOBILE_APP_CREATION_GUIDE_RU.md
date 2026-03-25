# Полное руководство: как создать и собрать мобильное приложение WaterSysPro

Этот документ описывает полный процесс создания, настройки, тестирования и сборки Android-приложения для WaterSysPro на базе Expo + React Native.

## 1. Что вы получите в итоге

После прохождения шагов у вас будет:
- React Native приложение с экранами Login, Dashboard и Settings
- Подключение к API и WebSocket вашего устройства/сервера
- Готовая APK-сборка для установки на Android
- Понимание, как выпускать обновления и диагностировать ошибки

## 2. Текущий стек проекта

Проект уже использует:
- Expo SDK 48
- React Native 0.71
- React Navigation
- Axios для API
- socket.io-client для real-time обновлений
- EAS Build для облачной Android-сборки

## 3. Предварительные требования

Установите один раз:
- Node.js 18+
- npm
- Аккаунт Expo (https://expo.dev)

Проверьте окружение:

```bash
node -v
npm -v
```

## 4. Подготовка проекта

Перейдите в папку мобильного приложения:

```bash
cd d:\WatersysPro\WatersysPro\mobile
```

Установите зависимости:

```bash
npm install
```

Установите EAS CLI глобально:

```bash
npm install -g eas-cli
```

Проверьте вход в Expo:

```bash
eas whoami
```

Если не авторизованы:

```bash
eas login
```

## 5. Важная настройка API и WebSocket

По умолчанию в проекте используются адреса ESP32:
- API: http://192.168.3.13:80/api
- WebSocket: http://192.168.3.13:80

Проверьте и при необходимости измените:
- `src/services/api.ts`
- `src/services/websocket.ts`

Пример для локальной сети:

```ts
// api.ts
const API_URL = 'http://192.168.3.13:80/api';

// websocket.ts
this.socket = io('http://192.168.3.13:80');
```

Важно:
- Телефон и устройство/сервер должны быть в одной сети
- Для реального телефона localhost не подходит, используйте IP хоста

## 6. Проверка запуска без APK (Expo Go)

Запустите dev-режим:

```bash
npm start
```

Дальше:
1. Установите Expo Go на Android
2. Отсканируйте QR-код
3. Убедитесь, что приложение открывается
4. Проверьте, что Login и Dashboard работают

## 7. Конфигурация приложения для Android

Проверьте `app.json`:
- `expo.name`
- `expo.slug`
- `expo.android.package` (уникальный идентификатор)
- `expo.android.versionCode` (увеличивайте на каждом релизе)

Текущий package:
- `com.igas65.watersysmobile`

## 8. Сборка APK через EAS Cloud (рекомендуется)

Почему этот путь лучший:
- Не нужен локальный Android SDK/Gradle
- Меньше ошибок окружения
- Быстрее старт и проще поддержка

Сборка:

```bash
eas build -p android --profile production
```

После завершения:
1. Откройте ссылку из консоли
2. Скачайте APK
3. Передайте APK на телефон
4. Установите приложение

Полезные команды:

```bash
eas build:list
eas build:view <BUILD_ID>
```

## 9. Сборка через батник (Windows)

В проекте есть сценарий:
- `build-apk.bat`

Запуск:

```bash
.\build-apk.bat
```

Что делает сценарий:
1. Проверяет Node.js
2. Ставит зависимости (если нужно)
3. Проверяет/ставит EAS CLI
4. Проверяет авторизацию Expo
5. Предлагает меню сборки

Рекомендуемый пункт в меню:
- `1 - Cloud Build (recommended)`

## 10. Чек-лист перед сборкой APK

Пройдите перед каждой сборкой:

1. Зависимости установлены: `npm install`
2. Вход в Expo активен: `eas whoami`
3. API URL в `src/services/api.ts` корректный
4. WebSocket URL в `src/services/websocket.ts` корректный
5. `android.package` в `app.json` корректный
6. `version` и `versionCode` обновлены (если это новый релиз)
7. Приложение запускается через `npm start` без критичных ошибок
8. Тест на реальном устройстве в Expo Go пройден

## 11. Чек-лист после сборки APK

1. APK скачан
2. APK устанавливается на Android
3. Экран логина открывается
4. API отвечает (нет постоянных timeout)
5. WebSocket подключается
6. Команды управления доходят до устройства
7. UI не падает при кратковременном отсутствии сети

## 12. Типовые ошибки и решения

### Ошибка: `eas` is not recognized
Причина: EAS CLI не установлен или не в PATH.

Решение:

```bash
npm install -g eas-cli
```

Перезапустите терминал и проверьте:

```bash
eas --version
```

### Ошибка авторизации Expo
Решение:

```bash
eas logout
eas login
eas whoami
```

### Ошибка подключения к API с телефона
Причины:
- Неправильный IP
- Телефон не в той же сети
- Порт/фаервол блокируют доступ

Решение:
1. Проверьте IP сервера
2. Откройте API URL в браузере телефона
3. Разрешите порт в firewall
4. Обновите `src/services/api.ts`

### WebSocket не подключается
Проверьте:
1. URL в `src/services/websocket.ts`
2. Что endpoint реально доступен
3. Что сервер поддерживает нужный namespace/events

### Ошибка установки APK на телефон
Причины:
- Поврежденный файл
- Запрещена установка из неизвестных источников

Решение:
1. Перекачайте APK
2. Разрешите установку из неизвестных источников
3. Убедитесь, что хватает памяти на устройстве

### Build failed в EAS
Действия:
1. Откройте лог сборки в Expo Dashboard
2. Исправьте конкретную ошибку
3. Повторите сборку
4. Если ошибка в зависимостях, очистите модули:

```bash
rmdir /s /q node_modules
del package-lock.json
npm install
```

## 13. Рекомендации по релизам

1. Перед релизом создавайте отдельную ветку
2. Фиксируйте изменения версий в commit
3. Делайте smoke-тесты на 2-3 устройствах
4. Не коммитьте секреты и реальные токены
5. Ведите changelog мобильной части

## 14. Минимальный маршрут от нуля до APK

Если нужно очень коротко:

1. `cd d:\WatersysPro\WatersysPro\mobile`
2. `npm install`
3. `npm install -g eas-cli`
4. `eas login`
5. Настроить URL в `src/services/api.ts` и `src/services/websocket.ts`
6. `npm start` и тест в Expo Go
7. `eas build -p android --profile production`
8. Скачать APK и установить на устройство

## 15. Полезные ссылки

- Документация Expo: https://docs.expo.dev/
- EAS Build: https://docs.expo.dev/build/introduction/
- Dashboard сборок: https://dashboard.expo.dev/

---

Если хотите, следующим шагом можно добавить этот документ в навигацию:
- `mobile/README.md`
- `DOCUMENTATION_INDEX.md`
чтобы гайд был сразу виден из главной документации.
