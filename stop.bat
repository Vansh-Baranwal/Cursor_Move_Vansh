@echo off
setlocal

taskkill /f /im "Cursor_Vansh.exe" >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Cursor_Vansh stopped.
) else (
    echo Cursor_Vansh was not running.
)
