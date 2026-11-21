@echo off
setlocal
cd /d "%~dp0"

echo ========================================================
echo   Installing MacShake Cursor for Windows
echo ========================================================
echo.

if not exist "bin\MacShakeCursor.exe" (
    echo [ERROR] bin\MacShakeCursor.exe not found!
    echo Please run build.bat first.
    pause
    exit /b 1
)

"bin\MacShakeCursor.exe" --install
