# Настройка FCM для push-уведомлений (Android)

Пошаговая инструкция, как включить push-уведомления в мобильном приложении WatersysPro
на Android через **Firebase Cloud Messaging (FCM)** и EAS Build.

> Файлы `mobile/google-services.json` и `mobile/*firebase-adminsdk*.json` исключены
> из репозитория (см. `.codeassistantignore`) и содержат секреты — их добавляют вручную.

---

## Общая картина

```
Backend → Expo Push API → FCM (Google) → ОС Android показывает уведомление
                                  ↑
              1. google-services.json   (описывает приложение в Firebase)
              2. FCM Service Account     (авторизация отправки через EAS)
```

Нужны **два разных файла**:

| Что | Зачем | Откуда |
|-----|-------|--------|
| `google-services.json` | Привязывает приложение к Firebase (вшивается в APK при сборке) | Firebase Console → «Настройки проекта» |
| JSON ключ сервисного аккаунта FCM (`firebase-adminsdk-*.json`) | Авторизация Expo Push API для отправки на это приложение | Firebase Console → «Сервисные аккаунты» → «Создать закрытый ключ» |

---

## Часть 1. Создание / выбор проекта в Firebase

1. Откройте [Firebase Console](https://console.firebase.google.com).
2. Нажмите **«Создать проект»** (или выберите уже существующий).
   - Можно использовать любой подходящий проект, например общий для WatersysPro.
3. Дождитесь создания проекта, затем перейдите в его **настройки** (шестерёнка
   слева внизу → **«Настройки проекта»**).

> Совет: если используется тот же проект Firebase, что и для других сервисов —
   это нормально, можно не создавать новый.

---

## Часть 2. Регистрация Android-приложения в Firebase

4. В **«Настройках проекта»** откройте вкладку **«Ваши приложения»**
   (или «Общие» → блок «Приложения»).
5. Нажмите **«Добавить приложение»** → выберите платформу **Android**.
6. В поле **«Имя пакета Android»** укажите точное значение package приложения —
   оно задано в [`mobile/app.json`](app.json):

   ```
   com.igas65.watersysmobile
   ```

   ⚠️ Имя пакета должно **полностью совпадать** — иначе push не привяжется к приложению.

7. Поля «Прозвище приложения» и «Отладочный подписывающий сертификат (SHA-1)»
   можно оставить пустыми (или заполнить позже — для базовых push они не требуются).
8. Нажмите **«Зарегистрировать приложение»**.

---

## Часть 3. Скачивание google-services.json

9. После регистрации Firebase предложит скачать файл **`google-services.json`**.
10. Сохраните его **в папку `mobile/`** проекта, рядом с `app.json`:

    ```
    mobile/
    ├── app.json
    ├── google-services.json   ← положить сюда
    └── ...
    ```

11. Убедитесь, что файл на месте. Его содержимое выглядит примерно так:
    ```json
    {
      "project_info": { "project_number": "...", "project_id": "..." },
      "client": [ { "client_info": { "android_client_info": { "package_name": "com.igas65.watersysmobile" } } } ]
    }
    ```

### 3.1. Привязка файла в конфиге Expo (уже сделано)

Чтобы Expo-prebuild скопировал файл в собранный Android-проект, в
[`mobile/app.json`](app.json) в блоке `android` прописан путь:

```json
"android": {
  "package": "com.igas65.watersysmobile",
  "googleServicesFile": "./google-services.json",
  ...
}
```

Это уже добавлено в проект. Если файла ещё нет — создайте его, иначе сборка упадёт
с ошибкой о недостающем `google-services.json`.

---

## Часть 4. Создание ключа сервисного аккаунта FCM (для EAS)

Это **отдельный** ключ от `google-services.json`. Он нужен EAS/Expo, чтобы отправлять
push от имени вашего проекта.

12. В Firebase Console откройте **«Настройки проекта»** → вкладка **«Сервисные аккаунты»**.
13. Нажмите **«Создать сервисный аккаунт»** (или «Управление сервисными аккаунтами»).
14. В появившемся диалоге нажмите **«Создать закрытый ключ»**.
    - Скачается JSON-файл вида `watersyspro-firebase-adminsdk-xxxxx.json`.
15. **Сохраните его в безопасном месте** (например, в `mobile/` или вне репозитория).
    Он содержит секретные данные — никому не передавайте.

---

## Часть 5. Загрузка FCM credentials в EAS

16. Убедитесь, что вы авторизованы в Expo/EAS:
    ```bash
    cd mobile
    eas login
    ```
17. Проверьте, что проект связан с EAS (projectId уже задан в `app.json`, но для
    надёжности):
    ```bash
    eas init
    ```
18. Выполните интерактивную настройку credentials:
    ```bash
    eas credentials
    ```
    Или сразу для Android:
    ```bash
    eas credentials --platform android
    ```
19. В меню выберите **«Push Notifications (FCM V1)»** → вариант **«Manage Push Notifications»**
    (или «Set up»).
20. Выберите **«Provide existing...»** (предоставить готовый файл) и укажите путь
    к скачанному на шаге 14 JSON-файлу сервисного аккаунта
    (`firebase-adminsdk-*.json`).

    Либо, если сервисный аккаунт уже есть в Firebase-проекте, EAS может сгенерировать
    ключ автоматически (вариант «Create a new one»).

21. Подтвердите выбор. EAS сохранит credentials в своём облаке и привяжет их
    к этому приложению (`com.igas65.watersysmobile`).

---

## Часть 6. Сборка APK / development build

> ⚠️ Push-уведомления **не работают в Expo Go**. Нужен нативный билд
> (development build или standalone APK/AAB).

### Вариант A — Development build (для тестирования)
```bash
cd mobile
eas build -p android --profile development
```
Установите полученный `.apk`/`.aab` на устройство.

### Вариант B — Preview (standalone APK)
```bash
eas build -p android --profile preview
```

### Вариант C — Production (для магазина)
```bash
eas build -p android --profile production
```

> Если профиля нет в `eas.json`, создайте его (пример ниже). Для быстрого теста
> push подойдёт `preview` или `development`.

Пример фрагмента [`mobile/eas.json`](eas.json):
```json
{
  "build": {
    "preview": { "distribution": "internal" },
    "production": {}
  }
}
```

---

## Часть 7. Проверка, что push работает

22. Установите собранный билд на телефон.
23. Откройте приложение → **Настройки** → убедитесь, что в поле «Адрес сервера»
    указан доступный с телефона адрес backend.
24. Нажмите **«Зарегистрировать push token»**.
    - При успехе появится сообщение «Push token зарегистрирован.»
    - Если приходит ошибка про FCM — значит `google-services.json` не попал в сборку
      или FCM-credentials не загружены в EAS (повторите Часть 3 и Часть 5, пересоберите).
25. На backend выполните тестовую отправку:
    ```bash
    curl -X POST http://<server>:3000/api/notify/test
    ```
    Или вызовите аварию на устройстве. Уведомление должно прийти в шторку телефона
    **даже при закрытом приложении** (доставкой управляет ОС через FCM).

26. Диагностика при ошибках:
    - **`Default FirebaseApp is not initialized` / `fcm-credentials`** — нет/неверный
      `google-services.json` или не загружены credentials в EAS. Пересоберите билд.
    - **`SERVICE_NOT_AVAILABLE`** — Google Play services временно недоступны или VPN /
      Private DNS мешает. Повторите через 1–2 минуты.
    - **Push не приходит при закрытом приложении** — проверьте, что разрешение
      `POST_NOTIFICATIONS` выдано приложению (системные настройки) и что APK собран
      с FCM (не через Expo Go).

---

## Часто задаваемые вопросы

**Можно ли использовать один Firebase-проект для нескольких приложений?**
Да. Каждое приложение регистрируется отдельно (Часть 2), у каждого свой `google-services.json`.

**Нужно ли настраивать FCM на iOS?**
Нет, для iOS используется APNs через Apple Developer account (отдельная настройка в EAS).

**Что если у меня уже есть google-services.json, но push не работает?**
Проверьте, что `package_name` внутри файла равен `com.igas65.watersysmobile`, и что
файл лежит в `mobile/` (путь `./google-services.json` в `app.json`).

**Почему файлы помечены как секретные?**
`google-services.json` и ключ сервисного аккаунта содержат учётные данные Firebase.
Они исключены из репозитория через `.codeassistantignore`, поэтому добавляются вручную
на каждой машине, где происходит сборка.

---

## Итоговый чек-лист

- [ ] В Firebase создан проект.
- [ ] Android-приложение `com.igas65.watersysmobile` зарегистрировано.
- [ ] `mobile/google-services.json` на месте, `package_name` совпадает.
- [ ] В `mobile/app.json` задан `googleServicesFile: "./google-services.json"`.
- [ ] Создан ключ сервисного аккаунта FCM (`firebase-adminsdk-*.json`).
- [ ] `eas credentials --platform android` → загружены FCM V1 credentials.
- [ ] Выполнен `eas build -p android --profile preview` (или `development`).
- [ ] В приложении нажат «Зарегистрировать push token».
- [ ] Тест `/api/notify/test` доставил push при закрытом приложении.