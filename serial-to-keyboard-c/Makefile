# Makefile for serial_keyboard

CC = clang
CFLAGS = -Wall -O2
FRAMEWORKS = -framework CoreGraphics -framework ApplicationServices

TARGET = serial_keyboard
SRC = serial_keyboard.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(FRAMEWORKS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
