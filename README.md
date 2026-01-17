# CW Hotline to Keyboard Converter

This tool reads Morse code signals from a Ham Radio Solutions "CW Hotline" device via USB serial and converts them into keyboard presses (`.` and `-` mapped to `Z` and `X` by default).

It is designed to work with web-based CW trainers like [Keyer's Journey](https://www.keyersjourney.com/).

## Download Pre-Built Binaries

**Don't want to build from source?** Download the latest pre-compiled executable:

👉 **[Download Latest Release](https://github.com/FtlC-ian/CW-Hotline-to-Keyboard/releases/latest)**

- **Windows**: `serial_keyboard.exe` 
- **macOS**: `serial_keyboard`

TheBuilding from Source

If you want to build from source instead of using the pre-built binaries:e [Usage](#usage) section below.

## Features

*   **Cross-Platform**: Written in native C for **macOS** (CoreGraphics) and **Windows** (Win32 API).
*   **Zero Dependencies**: No external libraries required - uses native OS APIs.
*   **Auto-Learning Timings**: Automatically detects your text speed (WPM) by analyzing the first few dots and dashes. No manual calibration needed.
*   **Browser Compatible**: optimised key-press duration (25ms) to ensure web apps register the events correctly at high speeds.
*   **Configurable**: Change keys, ports, and baud rates via command line.
*   **Device Configuration**: Built-in commands to configure the CW Hotline hardware (Speaker, WPM, etc.).

## Installation

### macOS

1.  Navigate to the C implementation directory:
    ```bash
    cd serial-to-keyboard-c
    ```
2.  Compile the tool:
    ```bash
    make
    ```
3.  **Grant Permissions**:
    *   Go to **System Settings > Privacy & Security > Accessibility**.
    *   Add your Terminal app (e.g., iTerm, Terminal) to the allowed list.
    *   *Note: Without this, the tool cannot simulate key presses.*

### Windows

#### Prerequisites & Building

**Step 1: Install MSYS2 and MinGW-w64**

Install MSYS2 using winget:
```powershell
winget install MSYS2.MSYS2
```

Then install the GCC compiler:
```powershell
C:\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm mingw-w64-x86_64-gcc"
```

Add MinGW to your PATH (for current session):
```powershell
$env:Path = "C:\msys64\mingw64\bin;" + $env:Path
```

**Step 2: Build the Project**

Navigate to the project directory and compile:
```powershell
cd serial-to-keyboard-c
gcc -Wall -O2 -D_WIN32 -o serial_keyboard.exe serial_keyboard.c -luser32
```

**Alternative: Use the build script**
```cmd
build_win32.bat
```

**Or using Make:**
```cmd
make -f Makefile.win32
```

---

**Other Installation Options:**
- **Chocolatey**: `choco install mingw`
- **Visual Studio**: Install with "Desktop development with C++" workload
- **Manual Download**: [winlibs.com](https://winlibs.com/) or [mingw-w64.org](https://www.mingw-w64.org/)

#### Running on Windows

**First, find your COM port** in Device Manager → Ports (COM & LPT)

Then run with your specific port:
```cmd
serial_keyboard.exe -p COM6
```

Or use the default (COM3):
```cmd
serial_keyboard.exe
```

**Important Windows Notes:**
- Use COM port names like `COM3`, `COM4`, `COM6`, etc. (check Device Manager)
- You may need to run as Administrator for keyboard simulation to work
- Change port with `-p COM6` (or whatever port your device is on)

For detailed troubleshooting and advanced options, see [BUILD_WIN32.md](BUILD_WIN32.md).

## Usage

Connect your CW Hotline via USB and run:

**macOS:**
```bash
./serial_keyboard
```

**Windows:**
```cmd
serial_keyboard.exe -p COM6
```

### Options

| Flag | Description | Default |
|------|-------------|---------|
| `-p <port>` | Serial port path | macOS: `/dev/tty.usbserial-11240`<br>Windows: `COM3` |
| `-b <baud>` | Serial baud rate | `115200` |
| `-d <key>` | Key to press for **DOT** | `z` |
| `-a <key>` | Key to press for **DASH** | `x` |
| `-q` | **Quiet Mode**: Minimal output for max speed | Off |
| `-r` | Raw debug mode (hex dump) | Off |
| `-h` | Show help message | - |

### Examples

**Best for Gaming (Low Latency):**
```bash
# macOS
./serial_keyboard -q

# Windows
serial_keyboard.exe -q -p COM6
```

**Custom Keys (e.g., A for dot, B for dash):**
```bash
# macOS
./serial_keyboard -d a -a b

# Windows
serial_keyboard.exe -d a -a b -p COM6
```

**Debug Mode (See raw serial data):**
```bash
# macOS
./serial_keyboard -r

# Windows
serial_keyboard.exe -r -p COM6
```

## CW Hotline Device Configuration

You can configure the hardware settings (Speaker, WPM, etc.) directly from this tool without a terminal emulator.

**Turn Internal Speaker On/Off:**
```bash
# macOS
./serial_keyboard --speaker-off
./serial_keyboard --speaker-on

# Windows
serial_keyboard.exe --speaker-off -p COM6
serial_keyboard.exe --speaker-on -p COM6
```

**Set Internal Keyer Speed:**
```bash
# macOS
./serial_keyboard --wpm 20   # Set to 20 WPM
./serial_keyboard --wpm 7    # Set to Straight Key mode

# Windows
serial_keyboard.exe --wpm 20 -p COM6
serial_keyboard.exe --wpm 7 -p COM6
```

**Note**: After changing settings, power cycle the CW Hotline device for them to take effect.

## Troubleshooting

### Windows
- **"COM port not found"**: Check Device Manager → Ports (COM & LPT) for the correct port number
- **"Access denied"**: Run Command Prompt as Administrator
- **Keys not registering**: Make sure no other program is capturing the keyboard input

### macOS
- **Keys not working**: Grant Accessibility permissions (System Settings → Privacy & Security → Accessibility)
- **Port not found**: Check `ls /dev/tty.usb*` to find your device