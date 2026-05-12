@echo off
title Course Timetable System
cd /d "%~dp0"

echo ========================================
echo   Course Timetable System - Quick Start
echo ========================================
echo.
echo Choose client type:
echo   1. CMD Client (console)
echo   2. GUI Client (graphical)
echo.
set /p choice="Enter choice (1 or 2): "

:: Check if executables exist
if not exist timetable_server.exe (
    echo [ERROR] timetable_server.exe not found!
    echo Please compile first using:  mingw32-make  (or:  g++ ...)
    pause
    exit /b 1
)

if "%choice%"=="1" (
    if not exist timetable_client.exe (
        echo [ERROR] timetable_client.exe not found!
        echo Please compile first using:  mingw32-make  (or:  g++ ...)
        pause
        exit /b 1
    )
) else if "%choice%"=="2" (
    if not exist timetable_gui.exe (
        echo [ERROR] timetable_gui.exe not found!
        echo Please compile first using:  mingw32-make  (or:  g++ ...)
        pause
        exit /b 1
    )
) else (
    echo Invalid choice!
    pause
    exit /b 1
)

:: Start server (new window)
echo [1/2] Starting server on port 54000...
start "Timetable Server" /MIN timetable_server.exe

:: Wait for server to start
timeout /t 2 /nobreak >nul

:: Start client
if "%choice%"=="1" (
    echo [2/2] Starting CMD client...
    echo.
    echo Tip: Default accounts -
    echo   Student: student / student123
    echo   Admin:   admin   / admin123
    echo.
    echo You can also register a new account!
    echo.
    start "Timetable Client" timetable_client.exe
) else (
    echo [2/2] Starting GUI client...
    start "Timetable GUI" timetable_gui.exe
)

echo.
echo Both server and client are running!
echo Close this window when done.
echo.
pause

:: Shutdown server when user presses a key
echo Shutting down server...
taskkill /f /im timetable_server.exe >nul 2>&1
echo Server stopped. Goodbye!
timeout /t 2 /nobreak >nul
