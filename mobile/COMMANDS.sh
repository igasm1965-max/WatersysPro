#!/usr/bin/env bash
# Copy-paste commands for building APK

echo "========================================"
echo "WatersysMobile - APK Build Commands"
echo "========================================"
echo ""

# Option 1: Cloud Build (Recommended)
echo "🚀 OPTION 1: CLOUD BUILD (Recommended)"
echo "========================================"
echo "npm install -g eas-cli"
echo "eas login"
echo "cd d:\WatersysPro\mobile"
echo "eas build -p android"
echo ""
echo "⏱️  Time: 5-15 minutes"
echo "✅ No need to install Android SDK"
echo ""
echo ""

# Option 2: Local Build
echo "🔨 OPTION 2: LOCAL BUILD"
echo "========================================"
echo "npm install -g eas-cli"
echo "eas login"
echo "cd d:\WatersysPro\mobile"
echo "eas build -p android --local"
echo ""
echo "⚠️  Requires: Java JDK 11+, Android SDK"
echo ""
echo ""

# Option 3: Test in Expo Go
echo "📱 OPTION 3: TEST IN EXPO GO (No APK)"
echo "========================================"
echo "cd d:\WatersysPro\mobile"
echo "npm start"
echo ""
echo "Then scan QR code in Expo Go app"
echo ""
echo ""

# Option 4: Use Windows Batch Script
echo "🪟 OPTION 4: WINDOWS - USE BATCH SCRIPT"
echo "========================================"
echo "cd d:\WatersysPro\mobile"
echo ".\build-apk.bat"
echo ""
echo "Choose option 1 in the menu"
echo ""

