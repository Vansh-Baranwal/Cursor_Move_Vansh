@echo off
setlocal

echo ========================================================
echo   Building Cursor_Vansh (Optimized Standalone Binary)
echo ========================================================

where cl.exe >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    )
)

if not exist bin mkdir bin

echo Compiling Windows Resources...
rc.exe /nologo /fo res\app.res res\app.rc
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Resource compilation failed.
    exit /b %ERRORLEVEL%
)

echo Compiling C++ Source...
cl.exe /nologo /O2 /W3 /Gy /GL /DUNICODE /D_UNICODE /DNDEBUG /EHsc ^
    src\main.cpp res\app.res ^
    user32.lib gdi32.lib shell32.lib advapi32.lib gdiplus.lib ole32.lib ^
    /link /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS ^
    /OUT:bin\Cursor_Vansh.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo   BUILD SUCCESSFUL!
    echo   Output: bin\Cursor_Vansh.exe
    echo ========================================================
    dir bin\Cursor_Vansh.exe
) else (
    echo.
    echo [ERROR] Build failed with code %ERRORLEVEL%.
)
