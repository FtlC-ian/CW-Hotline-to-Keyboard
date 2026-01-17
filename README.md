# CW Hotline to Keyboard Converter

This tool reads Morse code signals from a Ham Radio Solutions "CW Hotline" device via USB serial and converts them into keyboard presses (`.` and `-` mapped to `Z` and `X` by default).

It is designed to work with web-based CW trainers like [Keyer's Journey](https://www.keyersjourney.com/).

## Features

*   **Zero Dependencies**: Written in native C for macOS (CoreGraphics/IOKit).
*   **Auto-Learning Timings**: Automatically detects your text speed (WPM) by analyzing the first few dots and dashes. No manual calibration needed.
*   **Browser Compatible**: optimised key-press duration (25ms) to ensure web apps register the events correctly at high speeds.
*   **Configurable**: Change keys, ports, and baud rates via command line.
*   **Device Configuration**: Built-in commands to configure the CW Hotline hardware (Speaker, WPM, etc.).

## Installation

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

## Usage

Connect your CW Hotline via USB and run:

```bash
./serial_keyboard
```

### Options

| Flag | Description | Default |
|------|-------------|---------|
| `-p <port>` | Serial port path | `/dev/tty.usbserial-11240` |
| `-b <baud>` | Serial baud rate | `115200` |
| `-d <key>` | Key to press for **DOT** | `z` |
| `-a <key>` | Key to press for **DASH** | `x` |
| `-q` | **Quiet Mode**: Minimal output for max speed | Off |
| `-r` | Raw debug mode (hex dump) | Off |
| `-h` | Show help message | - |

### Examples

**Best for Gaming (Low Latency):**
```bash
./serial_keyboard -q
```

**Custom Keys (e.g., A for dot, B for dash):**
```bash
./serial_keyboard -d a -a b
```

**Debug Mode (See raw serial data):**
```bash
./serial_keyboard -r
```

## CW Hotline Device Configuration

You can configure the hardware settings (Speaker, WPM, etc.) directly from this tool without a terminal emulator.

**Turn Internal Speaker On/Off:**
```bash
./serial_keyboard --speaker-off
./serial_keyboard --speaker-on
```

**Set Internal Keyer Speed:**
```bash
./serial_keyboard --wpm 20   # Set to 20 WPM
./serial_keyboard --wpm 7    # Set to Straight Key mode
```

**Note**: After changing settings, power cycle the CW Hotline device for them to take effect.