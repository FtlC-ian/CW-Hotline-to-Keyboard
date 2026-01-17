# Building for Win32

This document explains how to build the CW Hotline to Keyboard tool for Windows.

## Prerequisites

You need a C compiler for Windows. Choose one of the following options:

### Option 1: MinGW-w64 (Recommended)
1. Download from [MinGW-w64](https://www.mingw-w64.org/downloads/)
2. Or install via [MSYS2](https://www.msys2.org/):
   ```bash
   pacman -S mingw-w64-x86_64-gcc
   ```
3. Add to PATH: `C:\msys64\mingw64\bin` (or wherever installed)

### Option 2: Microsoft Visual Studio
- Install "Desktop development with C++" workload
- Use the Developer Command Prompt

### Option 3: Chocolatey Package Manager
```powershell
choco install mingw
```

## Building

### Using MinGW/GCC:

Navigate to the project directory and run:

```bash
cd serial-to-keyboard-c
make -f Makefile.win32
```

Or compile directly:

```bash
gcc -Wall -O2 -D_WIN32 -o serial_keyboard.exe serial_keyboard.c -luser32
```

### Using Visual Studio Developer Command Prompt:

```cmd
cl /O2 /D_WIN32 serial_keyboard.c user32.lib
```

## Running

After building, run the executable:

```cmd
serial_keyboard.exe
```

### Common Options:

```cmd
# Use specific COM port
serial_keyboard.exe -p COM3

# Change keys (dot=a, dash=b)
serial_keyboard.exe -d a -a b

# Quiet mode for maximum speed
serial_keyboard.exe -q

# Configure device speaker
serial_keyboard.exe --speaker-off
serial_keyboard.exe --speaker-on

# Set device WPM
serial_keyboard.exe --wpm 25

# Interactive config mode
serial_keyboard.exe --config
```

## Troubleshooting

### "Command not found" or "gcc is not recognized"
- Make sure gcc is in your PATH
- Try running from the directory where gcc.exe is located
- Use full path to gcc: `C:\mingw\bin\gcc.exe ...`

### COM Port Issues
- Check Device Manager to find the correct COM port (e.g., COM3, COM4)
- Make sure no other program is using the port
- You may need to close Arduino IDE or other serial monitors

### Permission Issues
- Run as Administrator if you get access denied errors
- Windows may require elevated privileges for keyboard simulation

## Default Settings

- **Port**: COM3 (change with `-p`)
- **Baud Rate**: 115200 (change with `-b`)
- **Dot Key**: Z (change with `-d`)
- **Dash Key**: X (change with `-a`)

## Notes

The Windows build uses native Windows APIs:
- `windows.h` for serial port communication (CreateFile, ReadFile, etc.)
- `SendInput` API for keyboard simulation
- No external dependencies required
