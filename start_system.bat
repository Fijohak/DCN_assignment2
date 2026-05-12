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
echo [1/2] Starting server on port 54000...
echo       Web interface: http://localhost:8080
echo.

:: Start the server in the background (using full path for portability)
start /B /MIN "%SCRIPT_DIR%timetable_server.exe"

:: Wait for server to initialize
timeout /t 2 /nobreak >nul

echo [2/2] Opening web interface...
echo.
start http://localhost:8080

echo ============================================
echo   Server is running!
echo   - TCP Port: 54000 (CMD Client)
echo   - Web UI:   http://localhost:8080
echo   - Default Admin: admin / admin123
echo ============================================
echo.
echo Press any key to stop the server...
pause >nul

:: Kill the server
taskkill /f /im timetable_server.exe >nul 2>&1

echo Server stopped.
timeout /t 2 /nobreak >nul
