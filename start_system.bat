@echo off
title Course Timetable System
cd /d d:\Share\资料\y2s2\Net\As_2

echo ========================================
echo   Course Timetable System - Quick Start
echo ========================================
echo.
echo Choose client type:
echo   1. CMD Client (console)
echo   2. GUI Client (graphical)
echo.
set /p choice="Enter choice (1 or 2): "

:: 检查可执行文件是否存在
if not exist timetable_server.exe (
    echo [ERROR] timetable_server.exe not found!
    echo Please compile first: g++ -std=c++11 -Wall -Wextra server/server.cpp -o timetable_server.exe -lws2_32
    pause
    exit /b 1
)

if "%choice%"=="1" (
    if not exist timetable_client.exe (
        echo [ERROR] timetable_client.exe not found!
        echo Please compile first: g++ -std=c++11 -Wall -Wextra client/client.cpp -o timetable_client.exe -lws2_32
        pause
        exit /b 1
    )
) else if "%choice%"=="2" (
    if not exist timetable_gui.exe (
        echo [ERROR] timetable_gui.exe not found!
        echo Please compile first: g++ -std=c++11 -Wall client/gui_client.cpp -o timetable_gui.exe -lws2_32 -lgdi32 -lcomctl32 -mwindows
        pause
        exit /b 1
    )
) else (
    echo Invalid choice!
    pause
    exit /b 1
)

:: 启动服务器（新窗口）
echo [1/2] Starting server on port 54000...
start "Timetable Server" /MIN timetable_server.exe

:: 等待服务器启动
timeout /t 2 /nobreak >nul

:: 启动客户端
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

:: 关闭服务器（当用户按任意键后）
echo Shutting down server...
taskkill /f /im timetable_server.exe >nul 2>&1
echo Server stopped. Goodbye!
timeout /t 2 /nobreak >nul
