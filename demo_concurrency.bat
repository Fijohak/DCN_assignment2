@echo off
REM ============================================================
REM Concurrency Demo Script
REM Opens multiple TCP connections to the server simultaneously
REM to demonstrate multi-threaded connection handling.
REM ============================================================
setlocal enabledelayedexpansion

set SERVER=127.0.0.1
set PORT=54000
set COUNT=5

echo ============================================
echo  Course Timetable - Concurrency Demo
echo ============================================
echo.
echo  Server: %SERVER%:%PORT%
echo  Connections: %COUNT%
echo.
echo  Opening %COUNT% simultaneous connections...
echo  Watch the Concurrency Monitor in the web UI!
echo.

REM Launch multiple connections using PowerShell
for /L %%i in (1,1,%COUNT%) do (
    echo  [Connection %%i] Connecting...
    start /B /MIN powershell -Command ^
        "$tcp = New-Object System.Net.Sockets.TcpClient; " ^
        "$tcp.Connect('%SERVER%', %PORT%); " ^
        "$stream = $tcp.GetStream(); " ^
        "$reader = New-Object System.IO.StreamReader($stream); " ^
        "$welcome = $reader.ReadLine(); " ^
        "Write-Host '  [Connection %%i] Connected: ' $welcome; " ^
        "Start-Sleep -Seconds 30; " ^
        "$tcp.Close(); " ^
        "Write-Host '  [Connection %%i] Disconnected'"
)

echo.
echo  All %COUNT% connections established!
echo  They will stay open for 30 seconds.
echo  Check the web UI at http://localhost:8080
echo  to see Active Connections = %COUNT%.
echo.
echo  Press any key to close all connections early...
pause >nul

echo.
echo  Closing all connections...
taskkill /f /im powershell.exe /fi "WINDOWTITLE eq *Connection*" >nul 2>&1
echo  Done.
echo.
echo ============================================
