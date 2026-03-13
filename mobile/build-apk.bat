@echo off
REM Build APK Script for WatersysMobile - Windows Version

setlocal enabledelayedexpansion

echo.
echo =========================================
echo    WatersysMobile - APK Build Script
echo =========================================
echo.

REM Check if Node.js is installed
where node >nul 2>nul
if !errorlevel! neq 0 (
    echo Error: Node.js is not installed
    echo Please install Node.js from https://nodejs.org/
    pause
    exit /b 1
)

REM Change to mobile directory
cd /d "%~dp0"

REM Check if node_modules exists
if not exist "node_modules" (
    echo.
    echo Installing dependencies...
    call npm install
    if !errorlevel! neq 0 (
        echo Error installing dependencies
        pause
        exit /b 1
    )
)

REM Check if EAS CLI is installed globally
where eas >nul 2>nul
if !errorlevel! neq 0 (
    echo.
    echo Installing EAS CLI globally...
    call npm install -g eas-cli
    if !errorlevel! neq 0 (
        echo Error installing EAS CLI
        pause
        exit /b 1
    )
)

REM Check authentication
echo.
echo Checking Expo authentication...
eas whoami >nul 2>nul
if !errorlevel! neq 0 (
    echo.
    echo You need to log in to Expo
    call eas login
)

REM Menu
:menu
echo.
echo =========================================
echo   Build Options:
echo =========================================
echo 1 - Cloud Build (recommended)
echo 2 - Cloud Build Interactive
echo 3 - Local Build (requires Android SDK)
echo 4 - Run in Expo Go (no APK)
echo 5 - View build status
echo 6 - Exit
echo.

set /p choice="Select option (1-6): "

if "%choice%"=="1" (
    cls
    echo Building APK in cloud...
    call eas build -p android
    goto after_build
)

if "%choice%"=="2" (
    cls
    echo Interactive APK build...
    call eas build -p android --interactive
    goto after_build
)

if "%choice%"=="3" (
    cls
    echo Building APK locally (requires Android SDK)...
    call eas build -p android --local
    goto after_build
)

if "%choice%"=="4" (
    cls
    echo Starting Expo Go...
    call npm start
    goto menu
)

if "%choice%"=="5" (
    cls
    echo Fetching build status...
    call eas build:list
    goto menu
)

if "%choice%"=="6" (
    echo Exiting...
    exit /b 0
)

echo Invalid option!
goto menu

:after_build
echo.
echo =========================================
echo Build completed!
echo =========================================
echo.
pause
goto menu
