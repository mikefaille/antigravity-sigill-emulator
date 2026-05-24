# CPU Instruction Compatibility Toolkit

This toolkit enables high-performance binaries compiled for modern instruction sets (like `AES-NI`, `PCLMULQDQ`, and `AVX/AVX2` vector operations) to run on legacy processors (such as older Intel Xeon Westmere/Nehalem CPUs) without crashing.

It achieves this by combining **static binary patching** (for cold validation gates) with a **dynamic in-process signal emulator** (for hot loops) preloaded via `LD_PRELOAD`.

---

## 🚀 Beginner Quickstart (3-Step Guide)

If you are new to computer science or systems programming, follow these 3 simple steps to get a target binary (like `agy`) running on your older CPU:

### Step 1: Install Compiler Prerequisites
You need a C compiler (`gcc`) and compilation utilities (`make`) to build the project:
```bash
sudo apt-get update
sudo apt-get install build-essential gcc
```

### Step 2: Build and Install the Emulator
Compile the compatibility library and install it to your user directory:
```bash
# Build the project
make

# Install globally to /home/michael/sigill_emulator.so
make install
```

### Step 3: Run Your Program
Execute your target program (e.g. `agy.real`) preloaded with the compatibility library:
```bash
LD_PRELOAD=/home/michael/sigill_emulator.so /path/to/binary.real --help
```
*Tip: To see active emulation traces in your console, run with `DEBUG_EMU=1`:*
```bash
DEBUG_EMU=1 LD_PRELOAD=/home/michael/sigill_emulator.so /path/to/binary.real --help
```

---

## 📘 Documentation Directory

*   [LEARNING_GUIDE.md](file:///home/michael/src/agy-compat-toolkit/LEARNING_GUIDE.md): **Learning Guide**. A comprehensive, step-by-step systems programming tutorial explaining hooks, signals, register manipulations, and manual patching without AI.
*   [SKILL.md](file:///home/michael/src/agy-compat-toolkit/SKILL.md): **The Operational Playbook**. Read this for a step-by-step diagnostic guide on tracking crashes, finding instruction offsets, and static/dynamic patching logic.
*   [AGENTS.md](file:///home/michael/src/agy-compat-toolkit/AGENTS.md): **AI Agent Guide**. Documents how LLM coding agents (like Antigravity or OpenCode) can automatically ingest and apply the skill guide to resolve SIGILL errors.
*   [benchmark_results.md](file:///home/michael/src/agy-compat-toolkit/benchmark_results.md): **Performance Statistics**. Shows the low-level signal trapping overhead (~1.63 microseconds per trap).

---

## 🛠️ Toolkit Components

*   **`sigill_emulator.c`**: Core emulator catching `SIGILL` signals, decoding register states, performing software AES/carry-less operations, and returning state.
*   **`find_bad_insns.py`**: Static analysis instruction scanner. Runs instantly with `uv run find_bad_insns.py`.
*   **`benchmark.c`**: Trapping test utility looping 1,000,000 AESENC traps.
*   **`LEARNING_GUIDE.md`**: Systems programming educational guide detailing dynamic hooking, signal trapping, and manual binary hacking.
*   **`pyproject.toml`**: Modern Python dependency definition for `uv` environment isolation.
*   **`Makefile`**: Build automator.
*   **`session_log.md`**: Historical engineering notes on initial bypass static patching.
*   **`LICENSE`**: zlib Open Source License.
