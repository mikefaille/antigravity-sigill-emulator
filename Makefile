CC = gcc
CFLAGS = -shared -fPIC -O2 -Wall
TARGET = sigill_emulator.so
SRC = sigill_emulator.c
INSTALL_DIR = /home/michael
VERSION ?= v1.0.7

RELEASE_FILES = sigill_emulator.c Makefile README.md LEARNING_GUIDE.md SKILL.md find_bad_insns.py session_log.md AGENTS.md pyproject.toml uv.lock LICENSE benchmark.c benchmark_results.md .gitignore

.PHONY: all clean install benchmark release

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

benchmark: benchmark.c
	$(CC) -O2 -o benchmark benchmark.c

install: $(TARGET)
	rm -f $(INSTALL_DIR)/$(TARGET)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET)

release: clean all
	tar -czf agy-compat-toolkit-$(VERSION).tar.gz $(RELEASE_FILES)
	@echo "[+] Packed release agy-compat-toolkit-$(VERSION).tar.gz"

clean:
	rm -f $(TARGET) benchmark
