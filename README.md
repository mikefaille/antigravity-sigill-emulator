# Go Binary CPU Instruction Compatibility Toolkit

This repository contains tools developed to enable executing high-performance compiled Go binaries (such as `agy`) targeting modern vector/cryptographic instruction sets on legacy host processors (such as Intel Xeon Westmere processors lacking AES-NI or AVX/AVX2).

## Toolkit Contents

1.  **`sigill_emulator.c`**: A lightweight, high-performance in-process instruction emulator. It hooks the `SIGILL` signal via `LD_PRELOAD`, decodes instructions at the crash site, emulates them in software (with self-aliasing fixes, `AESIMC` support, and async-signal-safe logging), and advances `RIP` to resume execution.
2.  **`find_bad_insns.py`**: An optimized static analysis script built on top of `capstone` and `pyelftools` to scan `.text` segments for unsupported opcodes.
3.  **`benchmark.c`**: A performance verification program executing 1,000,000 AESENC loops to benchmark signal trapping latency.
4.  **`benchmark_results.md`**: Performance report logging the trapping latency (~1.63 microseconds per instruction).
5.  **`SKILL.md`**: Operational playbook guide for diagnosing, patching, and running binaries.
6.  **`pyproject.toml`**: Modern Python package configuration specifying tool dependencies.
7.  **`Makefile`**: Automation script to compile, install, and run benchmarks.

---

## Getting Started

### 1. Prerequisites

Ensure compiler tools are installed:
```bash
sudo apt-get install build-essential gcc
```

#### Python Environment Setup with `uv` (Recommended)
This project supports `uv` (a fast Python package manager) to run scripts without global dependency pollution:
```bash
# Run the scanner script instantly using uv
uv run find_bad_insns.py <path_to_binary>
```

#### Traditional Pip Setup
Alternatively, install packages globally or in a virtualenv:
```bash
python3 -m pip install --user --break-system-packages capstone pyelftools
```

### 2. Scanning for Unsupported Instructions

Scan any binary (e.g. `agy.real`) to audit unsupported instructions:
```bash
uv run find_bad_insns.py /home/michael/.local/bin/agy.real > bad_instructions.txt
```

### 3. Compiling and Installing the Signal Emulator

To build the dynamic emulator:
```bash
make
```

To install the dynamic emulator so it's globally accessible in `/home/michael/sigill_emulator.so`:
```bash
make install
```

### 4. Running Benchmarks
To compile and run the 1,000,000 iteration micro-benchmark:
```bash
make benchmark
LD_PRELOAD=/home/michael/sigill_emulator.so ./benchmark
```

### 5. Running the Target Binary with Emulator
To execute the binary transparently:
```bash
LD_PRELOAD=/home/michael/sigill_emulator.so /home/michael/.local/bin/agy.real --help
```
To enable detailed emulation tracing (printing every emulated instruction, source/destination registers to stderr):
```bash
DEBUG_EMU=1 LD_PRELOAD=/home/michael/sigill_emulator.so /home/michael/.local/bin/agy.real --help
```

---

## Static Patching & Binary Analysis with Rizin

For advanced inspection and static modification of target binaries, we recommend using [Rizin](https://rizin.re) (a fast, free, open-source reverse engineering framework).

### 1. Disassembling Opcode Bytes
To inspect instruction bytes at a specific virtual address:
```bash
rizin -c "pd 10 @ <address>" /home/michael/.local/bin/agy.real
```

### 2. Applying Surgical Patches
Instead of writing custom scripts, you can apply static patches directly in write-mode (`-w` flag) inside Rizin:
```bash
# Open binary in write mode
rizin -w /home/michael/.local/bin/agy.real

# Seek to target offset and write bytes (e.g. relative jump)
> s 0x74D0F0B
> wx E9B300000090
> q
```
