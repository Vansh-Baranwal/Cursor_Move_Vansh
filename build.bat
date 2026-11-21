@echo off
setlocal

echo ========================================================
echo   Building MacShakeCursor (Optimized Standalone Binary)
echo ========================================================

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist bin mkdir bin

cl.exe /nologo /O2 /W3 /Gy /GL /DUNICODE /D_UNICODE /DNDEBUG /EHsc ^
    src\main.cpp ^
    user32.lib gdi32.lib shell32.lib advapi32.lib gdiplus.lib ole32.lib ^
    /link /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS ^
    /OUT:bin\MacShakeCursor.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo   BUILD SUCCESSFUL!
    echo   Output: bin\MacShakeCursor.exe
    echo ========================================================
    dir bin\MacShakeCursor.exe
) else (
    echo.
    echo [ERROR] Build failed with code %ERRORLEVEL%.
)
