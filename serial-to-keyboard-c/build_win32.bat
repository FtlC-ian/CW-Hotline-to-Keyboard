@echo off
REM Build script for Win32 using gcc (MinGW)

echo Building CW Hotline to Keyboard for Win32...
echo.

gcc -Wall -O2 -D_WIN32 -o serial_keyboard.exe serial_keyboard.c -luser32

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✓ Build successful! 
    echo Created: serial_keyboard.exe
    echo.
    echo Run with: serial_keyboard.exe
) else (
    echo.
    echo ✗ Build failed!
    echo Make sure gcc is installed and in your PATH
    echo.
    echo Install options:
    echo   1. Download MinGW-w64 from https://www.mingw-w64.org/
    echo   2. Install via MSYS2 from https://www.msys2.org/
    echo   3. Use Chocolatey: choco install mingw
)

pause
