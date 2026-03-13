#!/usr/bin/env bash
# Скрипт для сборки APK через EAS Build

echo "=========================================="
echo "WatersysMobile - APK Build Script"
echo "=========================================="
echo ""

# Проверка EAS CLI
if ! command -v eas &> /dev/null; then
    echo "❌ EAS CLI не установлена"
    echo "Установка EAS CLI..."
    npm install -g eas-cli
fi

# Проверка авторизации
echo "📱 Проверка авторизации Expo..."
if ! eas whoami &> /dev/null; then
    echo "⚠️ Требуется авторизация"
    echo "Запуск: eas login"
    eas login
fi

# Выбор типа сборки
echo ""
echo "Выберите тип сборки:"
echo "1) Облачная сборка (рекомендуется)"
echo "2) Локальная сборка (требует Android SDK)"
echo "3) Internal testing (для Expo Go)"
read -p "Введите номер (1-3): " BUILD_TYPE

case $BUILD_TYPE in
    1)
        echo "🔨 Запуск облачной сборки APK..."
        eas build -p android
        ;;
    2)
        echo "🔨 Запуск локальной сборки APK..."
        eas build -p android --local
        ;;
    3)
        echo "📱 Запуск в режиме тестирования (Expo Go)..."
        npm start
        ;;
    *)
        echo "❌ Неверный выбор"
        exit 1
        ;;
esac

echo ""
echo "✅ Сборка завершена!"
