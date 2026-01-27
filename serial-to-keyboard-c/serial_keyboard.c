/*
 * serial_keyboard.c
 * Cross-Platform CW Hotline to Keyboard converter
 * Works on macOS (Native CoreGraphics) and Windows (Native API)
 * 
 * Compile macOS: make
 * Compile Windows: gcc -o serial_keyboard.exe serial_keyboard.c -luser32
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
    #include <errno.h>  // Added for error checking
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
#define MIN_PULSE_LENGTH 30      // Filter out noise < 30ms
#define CHARACTER_TIMEOUT_MS 1500 // Flush pending char after 1.5s of inactivity
#define WORD_GAP_TIMEOUT_MS 500   // Add space after 0.5s of silence (if char pending)

// Global Config
static char dotChar = 'z';
static char dashChar = 'x';
static int dotTiming = -1;
static int dashTiming = -1;
static int debugMode = 0;
static int quietMode = 0;
static int verboseMode = 0;  // Show raw serial data and timing info
static int keyboardMode = 0; // Full keyboard mode - type decoded characters
static int lowercaseMode = 0; // Output lowercase instead of uppercase (default)

// Platform Specific Key Codes
#ifdef __APPLE__
    static CGEventRef dotDown = NULL, dotUp = NULL;
    static CGEventRef dashDown = NULL, dashUp = NULL;
#endif

// ============================================================
// MORSE CODE DECODER - Binary Tree Implementation
// ============================================================

/*
 * Binary tree structure for Morse decoding:
 * - Start at root (index 0)
 * - Dit (.) = go to left child = index * 2 + 1
 * - Dah (-) = go to right child = index * 2 + 2
 * - When pause detected, output character at current index
 * 
 * Tree layout (first 63 nodes, depth 5):
 *                        [0] ROOT
 *                       /        \
 *                    [1]E        [2]T
 *                   /    \      /    \
 *                [3]I   [4]A  [5]N  [6]M
 *               / \    / \   / \   / \
 *             [7]S...  and so on
 */

// Morse tree - character at each node position (0=root, 1=E, 2=T, etc.)
// '\0' means invalid/unused position
static const char morseTree[128] = {
    '\0',  // [0] ROOT
    'E',   // [1] .
    'T',   // [2] -
    'I',   // [3] ..
    'A',   // [4] .-
    'N',   // [5] -.
    'M',   // [6] --
    'S',   // [7] ...
    'U',   // [8] ..-
    'R',   // [9] .-.
    'W',   // [10] .--
    'D',   // [11] -..
    'K',   // [12] -.-
    'G',   // [13] --.
    'O',   // [14] ---
    'H',   // [15] ....
    'V',   // [16] ...-
    'F',   // [17] ..-.
    '\0',  // [18] ..--
    'L',   // [19] .-..
    '\n',  // [20] .-.- (AA) - Newline/Enter
    'P',   // [21] .--.
    'J',   // [22] .---
    'B',   // [23] -...
    'X',   // [24] -..-
    'C',   // [25] -.-.
    'Y',   // [26] -.--
    'Z',   // [27] --..
    'Q',   // [28] --.-
    '\0',  // [29] ---.
    '\0',  // [30] ----
    '5',   // [31] .....
    '4',   // [32] ....-
    '\0',  // [33] ...-.
    '3',   // [34] ...--
    '\0',  // [35] ..-..
    '\0',  // [36] ..-.-
    '\0',  // [37] ..--. 
    '2',   // [38] ..---
    '\0',  // [39] .-...
    '\0',  // [40] .-..-
    '+',   // [41] .-.-.
    '\0',  // [42] .-.-. (dup?)
    '\0',  // [43] .-.--
    '\0',  // [44] .--..
    '\0',  // [45] .--.-
    '1',   // [46] .----
    '6',   // [47] -....
    '=',   // [48] -...-
    '/',   // [49] -..-.
    '\0',  // [50] -..--
    '\0',  // [51] -.-..
    '\0',  // [52] -.-.-
    '(',   // [53] -.--.
    '\0',  // [54] -.---
    '7',   // [55] --...
    '\0',  // [56] --..-
    '\0',  // [57] --.-.
    '\0',  // [58] --.--
    '8',   // [59] ---..
    '\0',  // [60] ---.-
    '9',   // [61] ----.
    '0',   // [62] -----
    
    // Level 6 (Indices 63-126)
    // Common Punctuation
    '.',   // [84] .-.-.-  (AAA) - Period
    ',',   // [114] --..-- (MIM) - Comma
    '?',   // [75] ..--..  (IMI) - Question Mark
    '\'',  // [92] .----.  (1, then dit?) Wait .---- is 1(46). 46->L(93)? No 1 is .----
           // J(22) .--- -> R(46: 1 .----) -> L(93: .----.) - Apostrophe
    '!',   // [106] -.-.-- (KW)  - Exclamation
    ':',   // [71] ---...  (OS)  - Colon (O->29->59(8)->L(119)? No O=14. 14->29->...)
           // O(14) --- -> 29(---.) -> 59(---..) -> (8)
           // 29(---.) -> R(60: ---.-) -> ?
           // 14(---) -> L(29) -> L(59 : 8)
           // Wait. 8 is ---..
           // : is ---...
           // 59(8) -> L(119)? No. 59*2+1 = 119.
           // So : is at 119? 
           // Let's verify.
           // 0->2(T)->6(M)->14(O)->29(---.)->59(8: ---..)->119(---...) 
           // Yes. : is at 119. ???
           // Standard : is ---...  (3 dahs, 3 dits)
           // O (3 dahs). S (3 dits). 
           // 14 (O). Left is 29 (---.). Left is 59 (---..). Left is 119 (---...).
           // Yes. 119.
    
    // Fill specific slots
    [75] = '?',
    [84] = '.',
    [93] = '\'',
    [106] = '!', // KW digraph
    [114] = ',',
    [119] = ':',
    [70] = ';', // -.-.-. (C -> R(52) -> L(105)??)
                // C(25) -.-. -> R(52: -.-.-) -> L(105: -.-.-.)
    [105] = ';',
    // [20] = '\n', // Moved to line 111
    [97] = '-',   // -....- (6 -> R(95)? No 6 is 47.)
                  // 6(47) -.... -> R(96) is -....-
                  // Let's verify index for -....-
                  // T(2)->M(6)->O(14)->CH(30: ----)->0(62: -----)->Was L(125: ----.) or R(126)?
                  // Wait. - is -....-
                  // T(2)->M(6)->G(13: --.)->Z(27: --..)->7(55: --...)->L(111: --...-)?
                  // No 7 is --...
                  // - is -....-
                  // T(2)->N(5: -.)->D(11: -..)->B(23: -...)->6(47: -....)->R(96: -....-)
                  // So 96 is Hyphen
    [96] = '-'
};

// Forward declaration for type_character (defined later in platform layer)
void type_character(char c);

// Decoder state
static int morseTreePos = 0;      // Current position in tree (0 = root)
static int elementCount = 0;      // Number of elements in current character
static char decodedBuffer[256];   // Buffer for decoded text
static int decodedPos = 0;        // Position in decoded buffer
static unsigned long lastActivityTime = 0;  // Last time we received data
static int pendingWordGap = 0;    // Flag: we've added a char but not yet a word gap

// Cross-platform millisecond timer
static unsigned long getCurrentTimeMs(void) {
#ifdef _WIN32
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

// Flush decoded buffer to output
static void flushDecoded(void) {
    if (decodedPos > 0) {
        decodedBuffer[decodedPos] = '\0';
        if (!quietMode) {
            printf("%s", decodedBuffer);
            fflush(stdout);
        }
        decodedPos = 0;
    }
}

// Add character to decoded buffer (and optionally type it)
static void addDecodedChar(char c) {
    // Apply case conversion (default: uppercase, Morse standard)
    if (c >= 'A' && c <= 'Z' && lowercaseMode) {
        c = c - 'A' + 'a';  // Convert to lowercase
    }
    // Note: Morse tree already stores uppercase, so no conversion needed for uppercase mode
    
    // In keyboard mode, actually type the character
    if (keyboardMode) {
        type_character(c);
    }
    
    if (decodedPos < sizeof(decodedBuffer) - 1) {
        decodedBuffer[decodedPos++] = c;
    }
    // Auto-flush every 64 chars or on space/newline
    if (decodedPos >= 64 || c == ' ' || c == '\n') {
        flushDecoded();
    }
}

// Complete current character and output it
static void completeCharacter(void) {
    if (elementCount > 0 && morseTreePos < 128) {
        char c = morseTree[morseTreePos];
        if (c != '\0') {
            addDecodedChar(c);
            pendingWordGap = 1;  // We output a char, might need word gap later
            if (verboseMode) {
                // Visualize special chars
                if (c == '\n') printf(" [=ENTER] ");
                else printf(" [=%c] ", c);
            }
        } else if (verboseMode) {
            printf(" [?] ");  // Unknown sequence
        }
    }
    // Reset for next character
    morseTreePos = 0;
    elementCount = 0;
}

// Check for timeout and flush pending character
static void checkTimeout(void) {
    if (lastActivityTime == 0) return;  // No activity yet
    
    unsigned long now = getCurrentTimeMs();
    unsigned long elapsed = now - lastActivityTime;
    
    // If we have a pending character and enough time has passed, complete it
    if (elementCount > 0 && elapsed > CHARACTER_TIMEOUT_MS) {
        if (verboseMode) printf(" [timeout] ");
        completeCharacter();
        flushDecoded();
    }
    
    // If we completed a char but haven't added word gap yet, add space after shorter timeout
    if (pendingWordGap && elapsed > WORD_GAP_TIMEOUT_MS && elementCount == 0) {
        // Don't add space - word gaps come naturally from pause detection
        // Just reset the flag
        pendingWordGap = 0;
    }
}

// Add a dit to the current sequence
static void addDit(void) {
    if (morseTreePos < 63) {  // Allow moving to children of nodes < 63 (up to index 126)
        morseTreePos = morseTreePos * 2 + 1;  // Left child
        elementCount++;
    }
}

// Add a dah to the current sequence
static void addDah(void) {
    if (morseTreePos < 63) {  // Allow moving to children of nodes < 63 (up to index 126)
        morseTreePos = morseTreePos * 2 + 2;  // Right child
        elementCount++;
    }
}

// ============================================================
// PLATFORM ABSTRACTION LAYERS
// ============================================================

// macOS key code lookup table
#ifdef __APPLE__
static CGKeyCode charToKeyCode(char c) {
    // Letters
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';  // Lowercase
    switch (c) {
        case 'a': return 0x00; case 'b': return 0x0B; case 'c': return 0x08;
        case 'd': return 0x02; case 'e': return 0x0E; case 'f': return 0x03;
        case 'g': return 0x05; case 'h': return 0x04; case 'i': return 0x22;
        case 'j': return 0x26; case 'k': return 0x28; case 'l': return 0x25;
        case 'm': return 0x2E; case 'n': return 0x2D; case 'o': return 0x1F;
        case 'p': return 0x23; case 'q': return 0x0C; case 'r': return 0x0F;
        case 's': return 0x01; case 't': return 0x11; case 'u': return 0x20;
        case 'v': return 0x09; case 'w': return 0x0D; case 'x': return 0x07;
        case 'y': return 0x10; case 'z': return 0x06;
        // Numbers
        case '0': return 0x1D; case '1': return 0x12; case '2': return 0x13;
        case '3': return 0x14; case '4': return 0x15; case '5': return 0x17;
        case '6': return 0x16; case '7': return 0x1A; case '8': return 0x1C;
        case '9': return 0x19;
        // Punctuation
        case ' ': return 0x31;  // Space
        case '.': return 0x2F;  // Period
        case ',': return 0x2B;  // Comma
        case '/': return 0x2C;  // Slash
        case '=': return 0x18;  // Equals
        case '+': return 0x18;  // Plus (same key, needs shift)
        case '(': return 0x19;  // 9 with shift
        case '-': return 0x1B;  // Minus
        case '?': return 0x2C;  // Slash with shift
        case '\n': return 0x24; // Return/Enter
        default: return 0xFF;   // Invalid
    }
}
#endif

// Type a single character (for full keyboard mode)
void type_character(char c) {
    if (c == '\0') return;
    
#ifdef _WIN32
    // Windows: Use SendInput with virtual key
    SHORT vk = VkKeyScan(c);
    if (vk == -1) return;  // Character not found
    
    INPUT ip[4] = {0};
    int inputCount = 0;
    
    // Check if shift is needed (high byte of VkKeyScan result)
    int needsShift = (HIBYTE(vk) & 1);
    
    if (needsShift) {
        ip[inputCount].type = INPUT_KEYBOARD;
        ip[inputCount].ki.wVk = VK_SHIFT;
        inputCount++;
    }
    
    ip[inputCount].type = INPUT_KEYBOARD;
    ip[inputCount].ki.wVk = LOBYTE(vk);
    inputCount++;
    
    ip[inputCount].type = INPUT_KEYBOARD;
    ip[inputCount].ki.wVk = LOBYTE(vk);
    ip[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
    inputCount++;
    
    if (needsShift) {
        ip[inputCount].type = INPUT_KEYBOARD;
        ip[inputCount].ki.wVk = VK_SHIFT;
        ip[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
        inputCount++;
    }
    
    SendInput(inputCount, ip, sizeof(INPUT));
#else
    // macOS: Use CGEvent
    CGKeyCode keyCode = charToKeyCode(c);
    if (keyCode == 0xFF) return;  // Invalid character
    
    // Check if shift is needed for uppercase or special chars
    int needsShift = (c >= 'A' && c <= 'Z') || c == '?' || c == '+' || c == '(';
    
    if (needsShift) {
        CGEventRef shiftDown = CGEventCreateKeyboardEvent(NULL, 0x38, true);  // Shift
        CGEventPost(kCGHIDEventTap, shiftDown);
        CFRelease(shiftDown);
    }
    
    CGEventRef keyDown = CGEventCreateKeyboardEvent(NULL, keyCode, true);
    CGEventRef keyUp = CGEventCreateKeyboardEvent(NULL, keyCode, false);
    CGEventPost(kCGHIDEventTap, keyDown);
    usleep(10000);  // 10ms hold
    CGEventPost(kCGHIDEventTap, keyUp);
    CFRelease(keyDown);
    CFRelease(keyUp);
    
    if (needsShift) {
        CGEventRef shiftUp = CGEventCreateKeyboardEvent(NULL, 0x38, false);
        CGEventPost(kCGHIDEventTap, shiftUp);
        CFRelease(shiftUp);
    }
#endif
    
    // Small delay between characters to ensure they're processed
    sleep_ms(30);
}

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
        case 's': dotCode = 0x01; break;
        case 'd': dotCode = 0x02; break;
        case 'f': dotCode = 0x03; break;
        case 'j': dotCode = 0x26; break;
        case 'k': dotCode = 0x28; break;
        case 'l': dotCode = 0x25; break;
        // Punctuation maps (common)
        case '.': dotCode = 0x2F; break;
        case ',': dotCode = 0x2B; break;
        case '\'': dotCode = 0x27; break;
        case ';': dotCode = 0x29; break;
    }
    switch (tolower(dashChar)) {
        case 'x': dashCode = 0x07; break;
        case 'a': dashCode = 0x00; break;
        case 's': dashCode = 0x01; break;
        case 'd': dashCode = 0x02; break;
        case 'f': dashCode = 0x03; break;
        case 'j': dashCode = 0x26; break;
        case 'k': dashCode = 0x28; break;
        case 'l': dashCode = 0x25; break;
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
    // In keyboard mode, we don't send Z/X keys - only decoded characters
    if (keyboardMode) {
        if (verboseMode) printf(isDash ? "-" : ".");
        return;
    }
    
    if (isDash) {
        if (verboseMode) printf("-");
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
        if (verboseMode) printf(".");
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
    DWORD bytesRead = 0;
    if (ReadFile(h, buf, max, &bytesRead, NULL)) return bytesRead;
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
int os_kbhit(void) {
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

char os_getch(void) {
#ifdef _WIN32
    return _getch();
#else
    char c;
    read(STDIN_FILENO, &c, 1);
    return c;
#endif
}

// ============================================================
// MORSE PROCESSING LOGIC
// ============================================================

int isClose(int val, int target) {
    return abs(val - target) <= TIMING_TOLERANCE;
}

// Process a command starting at the first comma (e.g. ",100,200")
// Returns pointer to end of command (digits), or NULL if invalid
char* processCommandWithComma(char *firstComma) {
    // Parse pause time (between first and second comma)
    char pauseStr[32] = {0};
    char *p = firstComma + 1;
    char *d = pauseStr;
    while (*p && isdigit((unsigned char)*p) && d - pauseStr < 31) {
        *d++ = *p++;
    }
    int pauseTime = atoi(pauseStr);
    
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
    
    // Glitch Filter
    if (charLength < MIN_PULSE_LENGTH) {
        if (debugMode) printf("[noise:%d] ", charLength);
        return endOfDigits;
    }
    
    if (verboseMode) printf("[p=%d l=%d] ", pauseTime, charLength);

    // Auto-learn mode
    if (dotTiming == -1) {
        dotTiming = charLength;
        if (verboseMode) printf("[learned dit=%d] ", dotTiming);
        addDit();
        press_key(0); // Dot
        return endOfDigits;
    }
    
    // Check for character/word boundary based on pause time
    // Character gap = 3 dit units, Word gap = 7 dit units
    // We use 2.5x and 6x as thresholds (with some tolerance)
    if (dotTiming > 0 && pauseTime > dotTiming * 2.5) {
        // End of character detected - decode what we have
        completeCharacter();
        
        // Check for word gap (7 dit units, use 6x threshold)
        if (pauseTime > dotTiming * 6) {
            addDecodedChar(' ');
            if (verboseMode) printf(" ");
        }
    }
    
    if (dashTiming == -1) {
        if (isClose(charLength, dotTiming)) {
            addDit();
            press_key(0); 
        } else {
            if (charLength > dotTiming) {
                dashTiming = charLength;
            } else {
                dashTiming = dotTiming;
                dotTiming = charLength;
            }
            if (verboseMode) printf("[learned dit=%d dah=%d] ", dotTiming, dashTiming);
            if (charLength == dotTiming) {
                addDit();
                press_key(0);
            } else {
                addDah();
                press_key(1);
            }
        }
        return endOfDigits;
    }
    
    // Self-Correction
    if (charLength < dotTiming * 0.6 && charLength > MIN_PULSE_LENGTH) {
        if (verboseMode) printf("[CORRECTION: dit=%d] ", charLength);
        dashTiming = dotTiming;
        dotTiming = charLength;
        addDit();
        press_key(0);
        return endOfDigits;
    }
    
    if (dashTiming > dotTiming * 6 && charLength > dotTiming * 2 && charLength < dashTiming) {
        if (verboseMode) printf("[CORRECTION: dah=%d] ", charLength);
        dashTiming = charLength;
        addDah();
        press_key(1);
        return endOfDigits;
    }

    // Classify
    if (isClose(charLength, dotTiming)) {
        addDit();
        press_key(0);
        dotTiming = (dotTiming * 3 + charLength) / 4;
    } else if (isClose(charLength, dashTiming)) {
        addDah();
        press_key(1);
        dashTiming = (dashTiming * 3 + charLength) / 4;
    } else {
        int dotDiff = abs(charLength - dotTiming);
        int dashDiff = abs(charLength - dashTiming);
        if (dotDiff < dashDiff) {
            addDit();
            press_key(0);
        } else {
            addDah();
            press_key(1);
        }
    }
    
    return endOfDigits;
}

void handleLine(char *line) {
    if (strlen(line) == 0) return;
    
    if (verboseMode) printf("\n>> %s -> ", line);
    
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
    
    if (verboseMode) { printf("\n"); fflush(stdout); }
}

// Config Automation constants
#define CONFIG_TOTAL_SETTINGS 14
#define CONFIG_SPEAKER_INDEX 9
#define CONFIG_WPM_INDEX 12

void automatedConfig(SERIAL_HANDLE h, int targetSetting, const char *newValue) {
    printf("[*] Automated CW Hotline Configuration\n");
    printf("    Changing setting #%d to: %s\n\n", targetSetting, newValue);
    
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
        printf("    [%d/%d] Waiting for prompt... ", setting, CONFIG_TOTAL_SETTINGS);
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
        
        if (!sawColon) printf("\n    [!] Timeout waiting for prompt!\n");
        
        if (setting == targetSetting) {
            printf("    >>> SETTING to: %s\n", newValue);
            os_serial_write(h, newValue, strlen(newValue));
            os_serial_write(h, "\r", 1);
        } else {
            printf("    (keeping)\n");
            os_serial_write(h, "\r", 1);
        }
        sleep_ms(200);
    }
    
    printf("\n[OK] Configuration complete! Power cycle device.\n");
}

void enterConfigMode(SERIAL_HANDLE h) {
    printf("[*] Interactive Mode (Press Ctrl+C to quit)\n");
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
    printf("Decodes Morse code from CW Hotline device and simulates keyboard input.\n\n");
    printf("Usage: %s [options]\n\n", progname);
    printf("Modes:\n");
    printf("  (default)       Simulates Z/X keys for web trainers, shows decoded text\n");
    printf("  -k, --keyboard  Full Keyboard Mode - types decoded characters!\n");
    printf("  -q              Quiet mode (no console output)\n");
    printf("  -v              Verbose mode (show raw data and timing info)\n");
    printf("  -r              Raw debug mode (show hex bytes)\n\n");
    printf("Options:\n");
    printf("  -p <port>   Serial port (default: %s)\n", DEFAULT_PORT);
    printf("  -b <baud>   Baud rate (default: %d)\n", DEFAULT_BAUD);
    printf("  -d <key>    Key for DOT in default mode (default: z)\n");
    printf("  -a <key>    Key for DASH in default mode (default: x)\n");
    printf("  --lowercase Output lowercase instead of UPPERCASE (default)\n");
    printf("  -h          Show this help\n\n");
    printf("Device Config:\n");
    printf("  --speaker-on/off   Toggle internal speaker\n");
    printf("  --wpm <N>          Set keyer speed (7=straight key, 8-50)\n");
    printf("  --config           Enter interactive config mode\n\n");
    printf("Examples:\n");
    printf("  %s                    # For web trainers (outputs Z/X keys)\n", progname);
    printf("  %s -k                 # TYPE WITH MORSE! (Full Keyboard Mode)\n", progname);
    printf("  %s -q                 # Silent operation\n", progname);
    printf("  %s -v                 # Debug timing issues\n", progname);
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
        else if (strcmp(arg, "-v")==0) verboseMode = 1;
        else if (strcmp(arg, "-r")==0) debugMode = 1;
        else if (strcmp(arg, "-h")==0 || strcmp(arg, "--help")==0) { printUsage(argv[0]); return 0; }
        else if (strcmp(arg, "--speaker-off")==0) speakerCmd = 1;
        else if (strcmp(arg, "--speaker-on")==0) speakerCmd = 2;
        else if (strcmp(arg, "--wpm")==0 && i+1<argc) wpmCmd = atoi(argv[++i]);
        else if (strcmp(arg, "-k")==0 || strcmp(arg, "--keyboard")==0) keyboardMode = 1;
        else if (strcmp(arg, "--lowercase")==0 || strcmp(arg, "-l")==0) lowercaseMode = 1;
        else if (strcmp(arg, "--config")==0) configCmd = 1;
    }

    init_keyboard();
    
    if (!quietMode) {
        printf("[*] CW Hotline to Keyboard\n");
        printf("    Port: %s @ %d baud\n", port, baud);
        if (keyboardMode) printf("    Mode: FULL KEYBOARD (typing decoded chars)\n");
        else printf("    Mode: Web Trainer (Z/X keys)\n");
        if (verboseMode) printf("    Verbose: ON (showing timing data)\n");
        printf("\n");
    }

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

    if (!quietMode) printf("Listening... (decoded text will appear below)\n\n");

    // Main Loop with Buffering
    static char lineBuf[4096];
    static int linePos = 0;
    
    while(1) {
        char buf[256];
        int n = os_serial_read(h, buf, sizeof(buf)-1);
        if (n > 0) {
            lastActivityTime = getCurrentTimeMs();  // Update activity timestamp
            if (debugMode) {
                for(int j=0; j<n; j++) printf("[%02X]%c ", buf[j], (buf[j]>=32 && buf[j]<127)?buf[j]:'.');
                printf("\n"); fflush(stdout);
                continue;
            }
            
            // Append to buffer
            if (linePos + n < (int)sizeof(lineBuf)) {
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
        } else if (n < 0) {
            // Error occurred
            #ifdef _WIN32
            // Windows: ReadFile returned FALSE
            printf("\n[!] Serial port error or device disconnected.\n");
            break;
            #else
            // macOS/Linux: check errno
            // If non-blocking and no data, it returns -1 with EAGAIN/EWOULDBLOCK.
            // If device disconnected, it returns -1 with EIO, ENXIO, or EBADF.
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                printf("\n[!] Device disconnected (errno=%d: %s).\n", errno, strerror(errno));
                break;
            }
            // If just EAGAIN, treat as timeout (no data)
            checkTimeout();
            #endif
        } else {
            // n == 0 (Timeout / No Data)
            checkTimeout();
        }
    }

    // Flush any remaining decoded text
    flushDecoded();
    
    cleanup_keyboard();
    os_close_serial(h);
    return 0;
}
