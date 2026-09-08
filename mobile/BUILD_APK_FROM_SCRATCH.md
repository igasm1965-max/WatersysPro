# Сборка APK с чистого листа — ИНСТРУКЦИЯ ДЛЯ CODE AGENT

## ⚠️ Проблема
Gradle кэширует старый JS Bundle. Если просто запустить `gradlew assembleRelease`,
в APK попадёт старый код, даже если TypeScript-файлы изменены.

## ✅ Единственный правильный порядок действий (всегда с нуля)

### Шаг 1 — Увеличить versionCode
**Файл:** `mobile/app.json` → `expo.android.versionCode` (увеличить на 1)
**Файл:** `mobile/android/app/build.gradle` → `defaultConfig.versionCode` (увеличить на 1)

### Шаг 2 — Очистить всё
```bash
cd /d "D:\WatersysPro\WatersysPro\mobile"

# Удалить кэш Metro
if exist .expo rmdir /s /q .expo

# Удалить старый JS Bundle
if exist android\app\src\main\assets\index.android.bundle del /f /q android\app\src\main\assets\index.android.bundle
if exist android\app\src\main\assets\index.android.bundle.meta del /f /q android\app\src\main\assets\index.android.bundle.meta

# Удалить старые ресурсы
if exist android\app\src\main\res\drawable-mdpi rmdir /s /q android\app\src\main\res\drawable-mdpi
if exist android\app\src\main\res\drawable-hdpi rmdir /s /q android\app\src\main\res\drawable-hdpi
if exist android\app\src\main\res\drawable-xhdpi rmdir /s /q android\app\src\main\res\drawable-xhdpi
if exist android\app\src\main\res\drawable-xxhdpi rmdir /s /q android\app\src\main\res\drawable-xxhdpi
if exist android\app\src\main\res\drawable-xxxhdpi rmdir /s /q android\app\src\main\res\drawable-xxxhdpi

# Удалить старые сборки Gradle
cd android
gradlew clean
cd ..
```

### Шаг 3 — Перегенерировать нативный код
```bash
npx expo prebuild --clean
```

### Шаг 4 — Пересобрать JS Bundle принудительно
```bash
npx expo export:embed ^
  --platform android ^
  --dev false ^
  --entry-file node_modules/expo/AppEntry.js ^
  --bundle-output android/app/src/main/assets/index.android.bundle ^
  --assets-dest android/app/src/main/res
```

### Шаг 5 — Собрать Release APK
```bash
cd android
gradlew assembleRelease
```

### Шаг 6 — Скопировать результат
```bash
copy /y android\app\build\outputs\apk\release\app-release.apk app.apk
```

---

## ⚡ Быстрый способ (одна команда)
```bash
cd /d "D:\WatersysPro\WatersysPro\mobile"
call build-apk.bat
```

---

## ❗ Критические правила для Code Agent
1. **НИКОГДА** не запускать `gradlew assembleDebug` — только `assembleRelease`
2. **НИКОГДА** не пропускать шаг 4 (export:embed) — без него JS Bundle старый
3. **ВСЕГДА** увеличивать `versionCode` перед сборкой
4. **ВСЕГДА** удалять `.expo/` кэш перед сборкой
5. **НИКОГДА** не использовать `npx expo run:android` — он требует подключённого устройства