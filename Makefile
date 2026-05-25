CC = gcc
CFLAGS_NATIVE = -shared -fPIC -O3 -march=native -flto -Wall
CFLAGS_V1 = -shared -fPIC -O3 -march=x86-64 -flto -Wall
CFLAGS_V2 = -shared -fPIC -O3 -march=x86-64-v2 -flto -Wall

TARGET = sigill_emulator.so
TARGET_V1 = sigill_emulator_v1.so
TARGET_V2 = sigill_emulator_v2.so

SRC = src/sigill_emulator.c
INSTALL_DIR = /home/michael
VERSION ?= v1.0.18

RELEASE_FILES = src/sigill_emulator.c Makefile README.md docs/LEARNING_GUIDE.md docs/SKILL.md scripts/find_bad_insns.py docs/session_log.md docs/AGENTS.md pyproject.toml uv.lock LICENSE benchmark/benchmark.c docs/benchmark_results.md .gitignore AGENTS.md skills/agy-compat/SKILL.md

.PHONY: all clean install benchmark release

all: $(TARGET) $(TARGET_V1) $(TARGET_V2)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS_NATIVE) -o $(TARGET) $(SRC)

$(TARGET_V1): $(SRC)
	$(CC) $(CFLAGS_V1) -o $(TARGET_V1) $(SRC)

$(TARGET_V2): $(SRC)
	$(CC) $(CFLAGS_V2) -o $(TARGET_V2) $(SRC)

benchmark: benchmark/benchmark.c
	$(CC) -O2 -o run_benchmark benchmark/benchmark.c

install: $(TARGET) $(TARGET_V1) $(TARGET_V2)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET).tmp
	mv $(INSTALL_DIR)/$(TARGET).tmp $(INSTALL_DIR)/$(TARGET)
	cp $(TARGET_V1) $(INSTALL_DIR)/$(TARGET_V1).tmp
	mv $(INSTALL_DIR)/$(TARGET_V1).tmp $(INSTALL_DIR)/$(TARGET_V1)
	cp $(TARGET_V2) $(INSTALL_DIR)/$(TARGET_V2).tmp
	mv $(INSTALL_DIR)/$(TARGET_V2).tmp $(INSTALL_DIR)/$(TARGET_V2)

release: clean all
	tar -czf agy-compat-toolkit-$(VERSION).tar.gz $(RELEASE_FILES)
	@echo "[+] Packed release agy-compat-toolkit-$(VERSION).tar.gz"

clean:
	rm -f $(TARGET) $(TARGET_V1) $(TARGET_V2) run_benchmark
