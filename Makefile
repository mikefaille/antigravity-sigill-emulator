CC = gcc
CFLAGS = -shared -fPIC -O2 -Wall
TARGET = sigill_emulator.so
SRC = sigill_emulator.c
INSTALL_DIR = /home/michael

.PHONY: all clean install benchmark

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

benchmark: benchmark.c
	$(CC) -O2 -o benchmark benchmark.c

install: $(TARGET)
	rm -f $(INSTALL_DIR)/$(TARGET)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET)

clean:
	rm -f $(TARGET) benchmark
