# Go Binary CPU Instruction Compatibility Toolkit

This repository contains tools developed to enable executing high-performance compiled Go binaries (such as `agy`) targeting modern vector/cryptographic instruction sets on legacy host processors (such as Intel Xeon Westmere processors lacking AES-NI or AVX/AVX2).

## Toolkit Contents

1.  **`find_bad_insns.py`**: An optimized static analysis script built on top of `capstone` and `pyelftools`. It scans the binary `.text` segment in small chunks to prevent OOM and prints all instances of unsupported or suspect instructions (e.g. `aesdec`, `aesenc`, `pclmulqdq`, and AVX/AVX2 vector instructions) alongside their file offsets, virtual addresses, mnemonics, and raw bytes.
2.  **`sigill_emulator.c`**: A lightweight, high-performance in-process instruction emulator. It hooks the `SIGILL` signal via `LD_PRELOAD`, decodes the instruction bytes at the crash site (`RIP`), decodes standard and extended (`REX.R`/`REX.B`) register operands, emulates the instruction in software, updates the register states inside the thread context (`ucontext_t`), and transparently advances `RIP` to resume seamless native execution.
3.  **`Makefile`**: Standard build script to automate compiling and installing the emulator shared library.

---

## Getting Started

### 1. Prerequisites

Ensure Python dependencies and compilers are installed:

```bash
python3 -m pip install --user --break-system-packages capstone pyelftools
sudo apt-get install build-essential gcc
```

### 2. Scanning for Unsupported Instructions

To scan any binary (e.g., `agy`) and generate a census of unsupported instructions, use the scanner:

```bash
./find_bad_insns.py /home/michael/.local/bin/agy.real > /tmp/bad_instructions.txt
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

### 4. Running the Binary with Emulator

To execute the binary transparently through the emulator (capturing signals on the fly):

```bash
LD_PRELOAD=/home/michael/sigill_emulator.so /home/michael/.local/bin/agy.real --help
```

To enable detailed emulation tracing (printing every emulated instruction, source/destination registers, and immediates to stderr):

```bash
DEBUG_EMU=1 LD_PRELOAD=/home/michael/sigill_emulator.so /home/michael/.local/bin/agy.real --help
```

---

## Technical Details

### AES-NI Emulation Semantics

The emulator handles the following AES-NI instructions:
*   `AESDEC`: Inverse ShiftRows -> Inverse SubBytes -> Inverse MixColumns -> XOR with Round Key.
*   `AESDECLAST`: Inverse ShiftRows -> Inverse SubBytes -> XOR with Round Key.
*   `AESENC`: ShiftRows -> SubBytes -> MixColumns -> XOR with Round Key.
*   `AESENCLAST`: ShiftRows -> SubBytes -> XOR with Round Key.

### Carry-less Multiplication Semantics

*   `PCLMULQDQ`: Performs Carry-less Multiplication (polynomial multiplication over GF(2)) on selected 64-bit halves (determined by the immediate byte `imm`) of two 128-bit XMM registers, writing the 128-bit result back to the destination register.
