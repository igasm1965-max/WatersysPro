# 📋 Чек-лист перед сборкой APK

## 🔍 Проверка конфигурации

- [x] **npm зависимости установлены**
  ```bash
  npm list --depth=0
  ```

- [x] **app.json** - конфигурация готова
  - Package ID настроен: `com.igas65.watersysmobile`
  - Версия указана: `1.0.0`
  - Разрешения добавлены для Android

- [x] **TypeScript** - синтаксис корректен
  ```bash
  npx tsc --noEmit
  ```

- [x] **Expo** версия совместима с SDK 48

---

## 📱 Проверка приложения

### Перед сборкой протестируйте:

1. **Запустить приложение в Expo Go**
   ```bash
   npm start
   ```
   Отсканируйте QR-код на телефоне с установленным Expo Go

2. **Проверить экраны:**
   - ✅ LoginScreen - вход в приложение
   - ✅ DashboardScreen - основной экран управления
   - ✅ SettingsScreen - экран настроек

3. **Проверить подключение:**
   - ✅ API подключение работает
   - ✅ WebSocket соединение устанавливается
   - ✅ Push-уведомления регистрируются

4. **Проверить навигацию:**
   - ✅ Переходы между экранами работают
   - ✅ Bottom Tab Navigation отображается правильно
   - ✅ Иконки загружены из Ionicons

---

## 🔐 Безопасность перед сборкой

- [ ] Проверить что API URL указан правильно
- [ ] Убедиться что токены не захардкодены
- [ ] Проверить HTTPS вместо HTTP где возможно
- [ ] Убедиться что sensitive data не логируется в console

---

## 📦 Сборка APK

### Для облачной сборки:
```bash
# 1. Один раз - установить EAS
npm install -g eas-cli

# 2. Один раз - авторизация
eas login

# 3. Сборка (может повторяться):
eas build -p android
```

### Для локальной сборки требуется:
- [ ] Java JDK 11+ установлена
  ```bash
  java -version
  ```
- [ ] Android SDK установлен
- [ ] ANDROID_HOME переменная окружения установлена

---

## ✅ После сборки APK

1. **Скачайте APK файл** со ссылки из консоли
2. **Протестируйте на реальном Android устройстве**:
   - Поставьте галочку у "Неизвестные источники"
   - Откройте APK
   - Проверьте все функции
3. **Если все работает:** готово к публикации/распространению

---

## 🚀 Версионирование для обновлений

При создании новой версии:
```json
// app.json
{
  "expo": {
    "version": "1.0.1",      ← Увеличить версию
    "appVersion": "1.0.1"    
  },
  "android": {
    "versionCode": 2         ← Увеличить код версии
  }
}
```

**Правило:** versionCode всегда должна быть больше чем в предыдущем релизе

---

## 📚 Полезные ссылки

- [Expo Documentation](https://docs.expo.dev/)
- [EAS Build Guide](https://docs.expo.dev/build/setup/)
- [Android App Configuration](https://docs.expo.dev/versions/latest/config/app/)
- [Google Play Console Upload](https://developer.android.com/studio/publish)

---

## 🆘 Помощь при проблемах

```bash
# Очистить кэш
rm -r node_modules package-lock.json
npm install

# Проверить синтаксис TypeScript
npx tsc

# Запустить в режиме отладки
eas build -p android --local

# Просмотреть логи с деталями
eas build:list --json

# Выйти и заново авторизоваться
eas logout
eas login
```

