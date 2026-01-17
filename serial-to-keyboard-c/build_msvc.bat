@echo off
REM Build script for Win32 using Microsoft Visual C++ compiler

echo Building CW Hotline to Keyboard for Win32 (MSVC)...
echo.

cl /O2 /D_WIN32 /Fe:serial_keyboard.exe serial_keyboard.c user32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✓ Build successful!
    echo Created: serial_keyboard.exe
    echo.
    echo Run with: serial_keyboard.exe
) else (
    echo.
    echo ✗ Build failed!
    echo Make sure to run this from "Developer Command Prompt for VS"
    echo.
    echo Or install Visual Studio with C++ Desktop Development workload
)

pause
