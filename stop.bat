@echo off
setlocal

taskkill /f /im "MacShakeCursor.exe" >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo MacShake Cursor stopped.
) else (
    echo MacShake Cursor was not running.
)
