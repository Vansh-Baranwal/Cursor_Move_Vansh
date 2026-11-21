@echo off
setlocal
cd /d "%~dp0"

echo ========================================================
echo   Uninstalling MacShake Cursor from Windows
echo ========================================================
echo.

if exist "bin\MacShakeCursor.exe" (
    "bin\MacShakeCursor.exe" --uninstall
) else (
    reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "MacShakeCursor" /f >nul 2>&1
    taskkill /f /im "MacShakeCursor.exe" >nul 2>&1
    echo Uninstalled from startup and stopped.
)
