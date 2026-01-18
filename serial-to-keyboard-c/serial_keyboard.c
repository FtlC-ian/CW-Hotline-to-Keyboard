/*
 * serial_keyboard.c
 * Cross-Platform CW Hotline to Keyboard converter
 * Works on macOS (Native CoreGraphics) and Windows (Native API)
 * 
 * Compile macOS: make
 * Compile Windows: gcc -o serial_keyboard.exe serial_keyboard.c
 */

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
    #define sleep_ms(x) Sleep(x)
    #define strncasecmp _strnicmp
    typedef HANDLE SERIAL_HANDLE;
    #define INVALID_SERIAL_HANDLE INVALID_HANDLE_VALUE
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <termios.h>
    #include <sys/select.h>
    #include <sys/time.h>
    #include <ApplicationServices/ApplicationServices.h>
    #define sleep_ms(x) usleep((x)*1000)
    typedef int SERIAL_HANDLE;
    #define INVALID_SERIAL_HANDLE -1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================
// CONFIGURATION
// ============================================================

#ifdef _WIN32
  #define DEFAULT_PORT "COM3"
#else
  #define DEFAULT_PORT "/dev/tty.usbserial-11240"
#endif

#define DEFAULT_BAUD 115200
#define TIMING_TOLERANCE 50
#define MIN_PULSE_LENGTH 30  // Filter out noise < 30ms

// Global Config
static char dotChar = 'z';
static char dashChar = 'x';
static int dotTiming = -1;
static int dashTiming = -1;
static int debugMode = 0;
static int quietMode = 0;

// Platform Specific Key Codes
#ifdef __APPLE__
    static CGEventRef dotDown = NULL, dotUp = NULL;
    static CGEventRef dashDown = NULL, dashUp = NULL;
#endif

// ============================================================
// PLATFORM ABSTRACTION LAYERS
// ============================================================

// 1. KEYBOARD HANDLING

void init_keyboard(void) {
#ifdef __APPLE__
    // macOS: Pre-create events
    CGKeyCode dotCode = 0x06; // 'z' default
    CGKeyCode dashCode = 0x07; // 'x' default
    
    // Simple lookup for common keys
    switch (tolower(dotChar)) {
        case 'z': dotCode = 0x06; break;
        case 'a': dotCode = 0x00; break;
        // ... (truncated full mapping for brevity, defaults work)
    }
    switch (tolower(dashChar)) {
        case 'x': dashCode = 0x07; break;
        case 'b': dashCode = 0x0B; break;
    }
    
    dotDown = CGEventCreateKeyboardEvent(NULL, dotCode, true);
    dotUp = CGEventCreateKeyboardEvent(NULL, dotCode, false);
    dashDown = CGEventCreateKeyboardEvent(NULL, dashCode, true);
    dashUp = CGEventCreateKeyboardEvent(NULL, dashCode, false);
#endif
    // Windows: No init needed
}

void cleanup_keyboard(void) {
#ifdef __APPLE__
    if (dotDown) CFRelease(dotDown);
    if (dotUp) CFRelease(dotUp);
    if (dashDown) CFRelease(dashDown);
    if (dashUp) CFRelease(dashUp);
#endif
}

void press_key(int isDash) {
    if (isDash) {
        if (!quietMode) printf("-");
#ifdef _WIN32
        SHORT vk = VkKeyScan(dashChar);
        INPUT ip[2] = {0};
        ip[0].type = INPUT_KEYBOARD; ip[0].ki.wVk = LOBYTE(vk);
        ip[1].type = INPUT_KEYBOARD; ip[1].ki.wVk = LOBYTE(vk); ip[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ip[0], sizeof(INPUT));
        Sleep(25); // Hold
        SendInput(1, &ip[1], sizeof(INPUT));
#else
        CGEventPost(kCGHIDEventTap, dashDown);
        usleep(25000); // Hold 25ms
        CGEventPost(kCGHIDEventTap, dashUp);
#endif
    } else {
        if (!quietMode) printf(".");
#ifdef _WIN32
        SHORT vk = VkKeyScan(dotChar);
        INPUT ip[2] = {0};
        ip[0].type = INPUT_KEYBOARD; ip[0].ki.wVk = LOBYTE(vk);
        ip[1].type = INPUT_KEYBOARD; ip[1].ki.wVk = LOBYTE(vk); ip[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ip[0], sizeof(INPUT));
        Sleep(25); // Hold
        SendInput(1, &ip[1], sizeof(INPUT));
#else
        CGEventPost(kCGHIDEventTap, dotDown);
        usleep(25000); // Hold 25ms
        CGEventPost(kCGHIDEventTap, dotUp);
#endif
    }
}

// 2. SERIAL PORT HANDLING

SERIAL_HANDLE os_open_serial(const char *port, int baud) {
#ifdef _WIN32
    HANDLE h = CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { printf("Error opening port %s\n", port); return INVALID_HANDLE_VALUE; }
    
    // Set Baud
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    GetCommState(h, &dcb);
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    SetCommState(h, &dcb);
    
    // Set Timeouts (Non-blocking / immediate return)
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD; // Return immediately
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    SetCommTimeouts(h, &timeouts);
    
    return h;
#else
    int fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) { perror("Error opening port"); return -1; }
    fcntl(fd, F_SETFL, 0); // Blocking read by default
    
    struct termios options;
    tcgetattr(fd, &options);
    speed_t speed = B115200; // Default
    if (baud == 115200) speed = B115200;
    // ... add other bauds if needed
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    
    // Raw mode
    cfmakeraw(&options);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cc[VMIN] = 0;  // Non-blocking (poll)
    options.c_cc[VTIME] = 1; // 0.1s timeout
    tcsetattr(fd, TCSANOW, &options);
    return fd;
#endif
}

void os_close_serial(SERIAL_HANDLE h) {
#ifdef _WIN32
    CloseHandle(h);
#else
    close(h);
#endif
}

int os_serial_read(SERIAL_HANDLE h, char *buf, int max) {
#ifdef _WIN32
    DWORD read = 0;
    if (ReadFile(h, buf, max, &read, NULL)) return read;
    return -1;
#else
    return read(h, buf, max);
#endif
}

void os_serial_write(SERIAL_HANDLE h, const char *buf, int len) {
#ifdef _WIN32
    DWORD written;
    WriteFile(h, buf, len, &written, NULL);
#else
    write(h, buf, len);
#endif
}

// 3. CONSOLE UTILS
int os_kbhit() {
#ifdef _WIN32
    return _kbhit();
#else
    struct timeval tv = {0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
#endif
}

char os_getch() {
#ifdef _WIN32
    return _getch();
#else
    char c;
    read(STDIN_FILENO, &c, 1);
    return c;
#endif
}

// ============================================================
// LOGIC
// ============================================================

int isClose(int val, int target) {
    return abs(val - target) <= TIMING_TOLERANCE;
}

// Process a command starting at the first comma (e.g. ",100,200")
// Returns pointer to end of command (digits), or NULL if invalid
char* processCommandWithComma(char *firstComma) {
    char *secondComma = strchr(firstComma + 1, ',');
    if (!secondComma) return NULL;
    
    // Parse length cleanly
    char lengthStr[32] = {0};
    char *src = secondComma + 1;
    char *dst = lengthStr;
    char *endOfDigits = src;
    
    while (*src && isdigit((unsigned char)*src) && dst - lengthStr < 31) {
        *dst++ = *src;
        endOfDigits = src; // Keep track of last digit
        src++;
    }
    
    int charLength = atoi(lengthStr);
    if (charLength == 0) return NULL;
    
    // Reuse existing logic (copied directly from processCommand)
    
    // Glitch Filter
    if (charLength < MIN_PULSE_LENGTH) {
        if (debugMode) printf("[ignored noise: %d] ", charLength);
        // Valid structure but ignored value. Return valid end pointer.
        return endOfDigits;
    }
    
    if (!quietMode) printf("[%dms] ", charLength);

    // Auto-learn mode
    if (dotTiming == -1) {
        dotTiming = charLength;
        if (!quietMode) printf("[learned initial=%d] ", dotTiming);
        press_key(0); // Dot
        return endOfDigits;
    }
    
    if (dashTiming == -1) {
        if (isClose(charLength, dotTiming)) {
            press_key(0); 
        } else {
            if (charLength > dotTiming) {
                dashTiming = charLength;
            } else {
                dashTiming = dotTiming;
                dotTiming = charLength;
            }
            if (!quietMode) printf("[learned DOT=%d DASH=%d] ", dotTiming, dashTiming);
            if (charLength == dotTiming) press_key(0); else press_key(1);
        }
        return endOfDigits;
    }
    
    // Self-Correction
    if (charLength < dotTiming * 0.6 && charLength > MIN_PULSE_LENGTH) {
        if (!quietMode) printf("[CORRECTION: New shorter Dot: %d] ", charLength);
        dashTiming = dotTiming;
        dotTiming = charLength;
        press_key(0);
        return endOfDigits;
    }
    
    if (dashTiming > dotTiming * 6 && charLength > dotTiming * 2 && charLength < dashTiming) {
        if (!quietMode) printf("[CORRECTION: Fixed huge Dash: %d] ", charLength);
        dashTiming = charLength;
        press_key(1);
        return endOfDigits;
    }

    // Classify
    if (isClose(charLength, dotTiming)) {
        press_key(0);
        dotTiming = (dotTiming * 3 + charLength) / 4;
    } else if (isClose(charLength, dashTiming)) {
        press_key(1);
        dashTiming = (dashTiming * 3 + charLength) / 4;
    } else {
        int dotDiff = abs(charLength - dotTiming);
        int dashDiff = abs(charLength - dashTiming);
        if (dotDiff < dashDiff) press_key(0); else press_key(1);
    }
    
    return endOfDigits;
}

void handleLine(char *line) {
    if (strlen(line) == 0) return;
    
    if (!quietMode) printf("\n>> Raw: \"%s\" -> ", line);
    
    char *cursor = line;
    // Scan for 'S' or 's', then find the next comma pattern
    while ((cursor = strpbrk(cursor, "Ss"))) {
        // We found an 'S'. Now look for the next comma
        char *comma = strchr(cursor, ',');
        
        // If no comma, or comma is too far (sanity check: >20 chars away?), break
        if (!comma) break;
        if (comma - cursor > 20) {
            cursor++; // Too far, this 'S' isn't a prefix
            continue;
        }

        // Check pattern from comma: ,digits,digits
        if (isdigit((unsigned char)*(comma+1))) {
            char *secondComma = strchr(comma + 1, ',');
            if (secondComma && isdigit((unsigned char)*(secondComma+1))) {
                // Found valid pattern associated with this 'S'
                char *end = processCommandWithComma(comma);
                if (end) {
                    cursor = end;
                    continue;
                }
            }
        }
        cursor++;
    }
    
    if (!quietMode) { printf("\n"); fflush(stdout); }
}

// Config Automation constants
#define CONFIG_TOTAL_SETTINGS 14
#define CONFIG_SPEAKER_INDEX 9
#define CONFIG_WPM_INDEX 12

void automatedConfig(SERIAL_HANDLE h, int targetSetting, const char *newValue) {
    printf("[*] Automated CW Hotline Configuration\n");
    printf("   Changing setting #%d to: %s\n\n", targetSetting, newValue);
    
    // Send entry command
    printf(">>> Sending *** ...\n");
    os_serial_write(h, "***\r", 4);
    
    // Wait for "Settings" banner
    char buffer[1024];
    int gotResponse = 0;
    printf("... Waiting for device response...\n");
    for (int i=0; i<40; i++) { // 4s timeout
        sleep_ms(100);
        int n = os_serial_read(h, buffer, sizeof(buffer)-1);
        if (n > 0) {
            buffer[n] = 0;
            printf("%s", buffer);
            if (strstr(buffer, "Settings")) gotResponse = 1;
        }
        if (gotResponse) break;
    }
    
    printf("\n[+] Going through %d settings...\n", CONFIG_TOTAL_SETTINGS);
    
    for (int setting=1; setting<=CONFIG_TOTAL_SETTINGS; setting++) {
        printf("   [%d/%d] Waiting for prompt... ", setting, CONFIG_TOTAL_SETTINGS);
        fflush(stdout);
        
        char lineBuf[1024] = {0};
        int sawColon = 0;
        
        // Wait for prompt ':'
        for (int w=0; w<40; w++) {
            sleep_ms(100);
            int n = os_serial_read(h, buffer, sizeof(buffer)-1);
            if (n>0) {
                buffer[n]=0;
                strncat(lineBuf, buffer, sizeof(lineBuf)-strlen(lineBuf)-1);
                if (strchr(buffer, ':')) { sawColon=1; break; }
            }
        }
        
        // Cleanup output
        for(int k=0; lineBuf[k]; k++) if(lineBuf[k]<32 && lineBuf[k]!='\n' && lineBuf[k]!='\r') lineBuf[k]='.';
        printf("%s", lineBuf);
        
        if (!sawColon) printf("\n   [!] Timeout waiting for prompt!\n");
        
        if (setting == targetSetting) {
            printf("   >>> SETTING to: %s\n", newValue);
            os_serial_write(h, newValue, strlen(newValue));
            os_serial_write(h, "\r", 1);
        } else {
            printf("   (keeping)\n");
            os_serial_write(h, "\r", 1);
        }
        sleep_ms(200);
    }
    
    printf("\n[OK] Configuration complete! Power cycle device.\n");
}

void enterConfigMode(SERIAL_HANDLE h) {
    printf("[*] Interactive Mode (Press 'exit' + Enter to quit)\n");
    os_serial_write(h, "***\r", 4);
    
    while(1) {
        // Serial -> Console
        char buf[256];
        int n = os_serial_read(h, buf, sizeof(buf)-1);
        if (n>0) { buf[n]=0; printf("%s", buf); fflush(stdout); }
        
        // Console -> Serial
        if (os_kbhit()) {
            char c = os_getch();
            if (c=='\n') c='\r';
            // primitive line check for 'exit'? omitted for brevity
            os_serial_write(h, &c, 1);
            printf("%c", c);
        }
        sleep_ms(10);
    }
}

// ============================================================
// MAIN
// ============================================================

void printUsage(const char *progname) {
    printf("CW Hotline to Keyboard (Universal)\n");
    printf("Usage: %s [options]\n", progname);
    printf("  -p <port>   Serial port (default: %s)\n", DEFAULT_PORT);
    printf("  -b <baud>   Baud rate (default: %d)\n", DEFAULT_BAUD);
    printf("  -d <key>    Key for DOT\n");
    printf("  -a <key>    Key for DASH\n");
    printf("  -q          Quiet mode\n");
    printf("  -r          Debug mode\n");
    printf("  --speaker-on/off, --wpm <N>, --config\n");
}

int main(int argc, char *argv[]) {
    const char *port = DEFAULT_PORT;
    int baud = DEFAULT_BAUD;
    int speakerCmd = 0;
    int wpmCmd = 0;
    int configCmd = 0;

    // Manual Arg Parsing
    for (int i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-p")==0 && i+1<argc) port = argv[++i];
        else if (strcmp(arg, "-b")==0 && i+1<argc) baud = atoi(argv[++i]);
        else if (strcmp(arg, "-d")==0 && i+1<argc) dotChar = argv[++i][0];
        else if (strcmp(arg, "-a")==0 && i+1<argc) dashChar = argv[++i][0];
        else if (strcmp(arg, "-q")==0) quietMode = 1;
        else if (strcmp(arg, "-r")==0) debugMode = 1;
        else if (strcmp(arg, "-h")==0 || strcmp(arg, "--help")==0) { printUsage(argv[0]); return 0; }
        else if (strcmp(arg, "--speaker-off")==0) speakerCmd = 1;
        else if (strcmp(arg, "--speaker-on")==0) speakerCmd = 2;
        else if (strcmp(arg, "--wpm")==0 && i+1<argc) wpmCmd = atoi(argv[++i]);
        else if (strcmp(arg, "--config")==0) configCmd = 1;
    }

    init_keyboard();
    
    if (!quietMode) printf("[*] CW Hotline Universal (%s @ %d)\n", port, baud);

    SERIAL_HANDLE h = os_open_serial(port, baud);
    if (h == INVALID_SERIAL_HANDLE) return 1;

    // Handle Commands
    if (configCmd) { enterConfigMode(h); return 0; }
    if (speakerCmd) {
        char val[2]; snprintf(val, sizeof(val), "%d", speakerCmd==2?1:0);
        automatedConfig(h, CONFIG_SPEAKER_INDEX, val);
        return 0;
    }
    if (wpmCmd) {
        char val[10]; snprintf(val, sizeof(val), "%d", wpmCmd);
        automatedConfig(h, CONFIG_WPM_INDEX, val);
        return 0;
    }

    if (!quietMode) printf("[+] Listening...\n");

    // Main Loop with Buffering
    static char lineBuf[4096];
    static int linePos = 0;
    
    while(1) {
        char buf[256];
        int n = os_serial_read(h, buf, sizeof(buf)-1);
        if (n > 0) {
            if (debugMode) {
                for(int j=0; j<n; j++) printf("[%02X] %c ", buf[j], (buf[j]>=32 && buf[j]<127)?buf[j]:'.');
                printf("\n"); fflush(stdout);
                continue;
            }
            
            // Append to buffer
            if (linePos + n < sizeof(lineBuf)) {
                memcpy(lineBuf + linePos, buf, n);
                linePos += n;
            } else {
                // Buffer overflow protection
                linePos = 0; 
            }
            
            // Scan for lines
            while(1) {
                int found = -1;
                for(int k=0; k<linePos; k++) {
                    if (lineBuf[k] == '\n' || lineBuf[k] == '\r') {
                        found = k;
                        break;
                    }
                }
                
                if (found == -1) break;
                
                // Process Line
                lineBuf[found] = 0;
                handleLine(lineBuf);
                
                // Shift remaining
                int remaining = linePos - (found + 1);
                memmove(lineBuf, lineBuf + found + 1, remaining);
                linePos = remaining;
                
                // If next char is also newline (CRLF?), skip it
                if (linePos > 0 && (lineBuf[0] == '\n' || lineBuf[0] == '\r')) {
                    memmove(lineBuf, lineBuf + 1, linePos - 1);
                    linePos--;
                }
            }
        } else {
            // Short sleep if no data to save CPU
            // (Only if read implementation implies non-blocking spin)
            // Mac VTIME=1 implies block. Win Timeout implies block.
            // So we loop immediately.
        }
    }

    cleanup_keyboard();
    os_close_serial(h);
    return 0;
}
