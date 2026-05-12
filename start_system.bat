@echo off
title Course Timetable System
color 0B

:: Get the directory where this batch file is located
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

echo ============================================
echo   Course Timetable Inquiry System
echo ============================================
echo.
echo [1/3] Compiling server...
echo.

:: Kill any existing server process first
taskkill /f /im server.exe >nul 2>&1

:: Compile the server
g++ -std=c++11 -pthread -o server.exe server/main.cpp server/server.cpp server/client_handler.cpp server/protocol.cpp server/auth.cpp server/logger.cpp database/course_db.cpp database/user_db.cpp -lws2_32

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed!
    pause
    exit /b 1
)

echo [2/3] Starting server on port 54000...
echo       Web interface: http://localhost:8080
echo.

:: Start the server in the background
start /B /MIN "timetable_server" server.exe

:: Wait for server to initialize
timeout /t 2 /nobreak >nul

echo [3/3] Opening web interface...
echo.
start http://localhost:8080

echo ============================================
echo   Server is running!
echo   - TCP Port: 54000
echo   - Web UI:   http://localhost:8080
echo   - Default Admin: admin / admin123
echo   - Default Student: student / student123
echo ============================================
echo.
echo Press any key to stop the server...
pause >nul

:: Kill the server
taskkill /f /im server.exe >nul 2>&1

echo Server stopped.
timeout /t 2 /nobreak >nul
