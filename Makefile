CC = gcc
CFLAGS = -fPIC -O2 -march=nehalem -mtune=nehalem -mno-avx -mno-avx2 -mno-fma -mno-bmi -mno-bmi2 -fno-tree-vectorize -Wall -Wextra -Isrc -Isrc/avx -Isrc/math -fvisibility=hidden
LDFLAGS = -shared
LDLIBS = -ldl -lpthread

TARGET = sigill_emulator.so

SRC = src/sigill_emulator.c \
      src/avx/avx_decode.c \
      src/avx/avx_address.c \
      src/avx/avx_state.c \
      src/avx/avx_move.c \
      src/avx/avx_pack.c \
      src/avx/avx_permute.c \
      src/avx/avx_integer.c \
      src/avx/avx_float.c \
      src/avx/avx_convert.c \
      src/avx/avx_compare.c \
      src/avx/avx_dispatch.c \
      src/math/math_integer.c \
      src/math/math_pack.c \
      src/math/math_permute.c \
      src/math/math_float.c \
      src/math/math_convert.c \
      src/math/math_compare.c \
      src/math/simde_backend.c

OBJS = $(SRC:.c=.o)
DEPS = $(SRC:.c=.d)

INSTALL_DIR ?= $(HOME)
VERSION ?= v1.0.20

RELEASE_FILES = $(SRC) test_signal_chaining.c src/avx/avx_emulator.h src/math/math_backend.h Makefile README.md docs/LEARNING_GUIDE.md docs/SKILL.md scripts/find_bad_insns.py scripts/patch_agy.py docs/session_log.md docs/AGENTS.md pyproject.toml uv.lock LICENSE benchmark/benchmark.c docs/benchmark_results.md .gitignore AGENTS.md skills/agy-compat/SKILL.md skills/avx-emu-compatibility/SKILL.md

.PHONY: all clean install benchmark release verify test

all: $(TARGET) verify

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) -MT $@ -MMD -MP -MF $(@:.o=.d) $(CFLAGS) -c $< -o $@

-include $(DEPS)

verify: $(TARGET)
	@echo "🔍 Checking $(TARGET) for unsupported instruction signatures..."
	@if objdump -d $(TARGET) | grep -E '^\s+[0-9a-f]+:\s+([0-9a-f]{2}\s+)+\b(v[a-z0-9]+|andn|bextr|pdep|pext|mulx|shlx|shrx|sarx|aesenc|aesdec|aesenclast|aesdeclast|aesimc|aeskeygenassist|pclmulqdq|rdrand|rdseed|adcx|adox)\b' > /dev/null; then \
		echo "❌ ERROR: Unsupported instructions found in emulator library!"; \
		objdump -d $(TARGET) | grep -E '^\s+[0-9a-f]+:\s+([0-9a-f]{2}\s+)+\b(v[a-z0-9]+|andn|bextr|pdep|pext|mulx|shlx|shrx|sarx|aesenc|aesdec|aesenclast|aesdeclast|aesimc|aeskeygenassist|pclmulqdq|rdrand|rdseed|adcx|adox)\b'; \
		exit 1; \
	else \
		echo "✅ No configured forbidden instruction mnemonics detected."; \
	fi

test: $(TARGET) test_signal_chaining.c
	$(CC) -O2 -o run_test test_signal_chaining.c
	@echo "🧪 Running test in SAFE mode..."
	timeout 15 env LD_PRELOAD=./$(TARGET) ./run_test
	@echo "🧪 Running test in EXPERIMENTAL mode..."
	timeout 15 env EMU_MODE=experimental LD_PRELOAD=./$(TARGET) ./run_test
	@echo "✅ All tests passed successfully!"

benchmark: benchmark/benchmark.c
	$(CC) -O2 -o run_benchmark benchmark/benchmark.c

install: $(TARGET)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET).tmp
	mv $(INSTALL_DIR)/$(TARGET).tmp $(INSTALL_DIR)/$(TARGET)
	cp scripts/patch_agy.py $(INSTALL_DIR)/patch_agy.py.tmp
	mv $(INSTALL_DIR)/patch_agy.py.tmp $(INSTALL_DIR)/patch_agy.py
	chmod +x $(INSTALL_DIR)/patch_agy.py

release: clean all
	tar -czf agy-compat-toolkit-$(VERSION).tar.gz $(RELEASE_FILES)
	@echo "[+] Packed release agy-compat-toolkit-$(VERSION).tar.gz"

clean:
	rm -f $(TARGET) $(OBJS) $(DEPS) run_benchmark run_test

