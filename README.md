# CW Hotline to Keyboard Converter

This tool reads Morse code signals from a Ham Radio Solutions "CW Hotline" device via USB serial and decodes them into text. It can also simulate keyboard presses, allowing you to use your Morse paddle to type anywhere!

## Features

*   **Morse Decoder**: Built-in binary tree decoder translates Morse to text (`. _ _ .` → `P`)
*   **Full Keyboard Mode**: Type into any application (emails, text editors, etc.) using Morse code!
*   **Cross-Platform**: Works natively on **macOS** and **Windows**
*   **Auto-Learning**: Automatically detects your speed (WPM)
*   **Web Trainer Support**: Compatible with [Keyer's Journey](https://www.keyersjourney.com/) (defualt mode)

## Download

👉 **[Download Latest Release](https://github.com/FtlC-ian/CW-Hotline-to-Keyboard/releases/latest)**

- **Windows**: `serial_keyboard.exe`
- **macOS**: `serial_keyboard`

## Usage

Connect your CW Hotline via USB and run the tool in your terminal.

### 1. Default Mode (for Web Trainers)
Decodes text to the console but sends `Z` (dot) and `X` (dash) keystrokes. Perfect for online Morse games like **Keyer's Journey**.

```bash
./serial_keyboard
```

### 2. Full Keyboard Mode (Type with Morse!)
Types the actual decoded characters into your computer. Open a text editor and start keying!

```bash
./serial_keyboard -k
```

*(Add `--lowercase` or `-l` if you prefer lowercase letters)*

### Options

| Flag | Description |
|------|-------------|
| `-k`, `--keyboard` | **Full Keyboard Mode**: Type decoded text |
| `-l`, `--lowercase` | Output lowercase letters (default is UPPERCASE standard Morse) |
| `-q` | **Quiet**: No console output (just typing) |
| `-v` | **Verbose**: Show raw timing data (useful for debugging) |
| `-p <port>` | Specify serial port (e.g. `COM3` or `/dev/tty...`) |
| `-b <baud>` | Specify baud rate (default: 115200) |

## Device Configuration

Configure your CW Hotline hardware settings directly from the tool:

```bash
# Set WPM Speed (7 = Straight Key mode)
./serial_keyboard --wpm 20

# Turn internal speaker on/off
./serial_keyboard --speaker-off
./serial_keyboard --speaker-on
```

## Building from Sourcce

<details>
<summary>Click to expand build instructions</summary>

### macOS
1. `cd serial-to-keyboard-c`
2. `make`

### Windows
1. Install MinGW (GCC)
2. `cd serial-to-keyboard-c`
3. Double-click `build_win32.bat` (or run `gcc -o serial_keyboard.exe serial_keyboard.c -luser32`)

</details>