@echo off
setlocal
cd /d "%~dp0"

echo ========================================================
echo   Installing Cursor_Vansh for Windows
echo ========================================================
echo.

if not exist "bin\Cursor_Vansh.exe" (
    echo [ERROR] bin\Cursor_Vansh.exe not found!
    echo Please run build.bat first.
    pause
    exit /b 1
)

"bin\Cursor_Vansh.exe" --install
