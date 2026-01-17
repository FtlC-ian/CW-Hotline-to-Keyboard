/*
 * debug_serial.c - Raw serial debug tool
 * Shows every byte received in hex and ASCII
 * 
 * Compile: clang -o debug_serial debug_serial.c
 * Run: ./debug_serial
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

#define SERIAL_PORT "/dev/tty.usbserial-11240"

int main(int argc, char *argv[]) {
    const char *port = (argc > 1) ? argv[1] : SERIAL_PORT;
    
    printf("🔌 Debug Serial Reader\n");
    printf("   Port: %s\n\n", port);
    
    // Try opening with different flags
    printf("Attempting to open port...\n");
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        printf("❌ Failed to open: %s (errno: %d)\n", strerror(errno), errno);
        return 1;
    }
    printf("✅ Port opened (fd=%d)\n", fd);
    
    // Get current settings
    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        printf("❌ tcgetattr failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    printf("✅ Got terminal attributes\n");
    
    // Configure: 9600 8N1
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);
    
    options.c_cflag |= (CLOCAL | CREAD);  // Enable receiver, ignore modem control
    options.c_cflag &= ~PARENB;           // No parity
    options.c_cflag &= ~CSTOPB;           // 1 stop bit
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;               // 8 data bits
    
    // Raw input mode (no line processing)
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);  // No software flow control
    options.c_iflag &= ~(INLCR | ICRNL);         // Don't translate CR/LF
    options.c_oflag &= ~OPOST;                    // Raw output
    
    // Read returns after 1 byte or 1 second timeout
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;  // 1 second timeout (in tenths of seconds)
    
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        printf("❌ tcsetattr failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    printf("✅ Configured: 9600 8N1, raw mode\n");
    
    // Flush any pending data
    tcflush(fd, TCIOFLUSH);
    printf("✅ Flushed buffers\n\n");
    
    printf("🎧 Listening for data... (Ctrl+C to quit)\n");
    printf("   Each byte shown as [HEX] 'CHAR'\n\n");
    
    unsigned char buffer[256];
    int totalBytes = 0;
    int lineBytes = 0;
    
    while (1) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                unsigned char c = buffer[i];
                totalBytes++;
                lineBytes++;
                
                // Print hex value
                printf("[%02X]", c);
                
                // Print ASCII if printable
                if (c >= 32 && c < 127) {
                    printf("'%c' ", c);
                } else if (c == '\n') {
                    printf("\\n ");
                } else if (c == '\r') {
                    printf("\\r ");
                } else {
                    printf("    ");
                }
                
                // Newline every 8 bytes or on actual newline
                if (c == '\n' || c == '\r' || lineBytes >= 8) {
                    printf("\n");
                    lineBytes = 0;
                }
            }
            fflush(stdout);
        } else if (n == 0) {
            // Timeout - just print a dot every second to show we're alive
            printf(".");
            fflush(stdout);
        } else {
            printf("\n❌ Read error: %s\n", strerror(errno));
        }
    }
    
    close(fd);
    return 0;
}
