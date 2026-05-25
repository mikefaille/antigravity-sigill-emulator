CC = gcc
CFLAGS = -shared -fPIC -O3 -march=native -flto -Wall
TARGET = sigill_emulator.so
SRC = src/sigill_emulator.c
INSTALL_DIR = /home/michael
VERSION ?= v1.0.8

RELEASE_FILES = src/sigill_emulator.c Makefile README.md docs/LEARNING_GUIDE.md docs/SKILL.md scripts/find_bad_insns.py docs/session_log.md docs/AGENTS.md pyproject.toml uv.lock LICENSE benchmark/benchmark.c docs/benchmark_results.md .gitignore

.PHONY: all clean install benchmark release

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

benchmark: benchmark/benchmark.c
	$(CC) -O2 -o run_benchmark benchmark/benchmark.c

install: $(TARGET)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET).tmp
	mv $(INSTALL_DIR)/$(TARGET).tmp $(INSTALL_DIR)/$(TARGET)

release: clean all
	tar -czf agy-compat-toolkit-$(VERSION).tar.gz $(RELEASE_FILES)
	@echo "[+] Packed release agy-compat-toolkit-$(VERSION).tar.gz"

clean:
	rm -f $(TARGET) run_benchmark
