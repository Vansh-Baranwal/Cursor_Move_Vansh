@echo off
setlocal
cd /d "%~dp0"

echo ========================================================
echo   Uninstalling Cursor_Vansh from Windows
echo ========================================================
echo.

if exist "bin\Cursor_Vansh.exe" (
    "bin\Cursor_Vansh.exe" --uninstall
) else (
    reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "Cursor_Vansh" /f >nul 2>&1
    taskkill /f /im "Cursor_Vansh.exe" >nul 2>&1
    echo Uninstalled from startup and stopped.
)
