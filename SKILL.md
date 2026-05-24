---
name: agy-compat
description: Use when analyzing, patching, or debugging compiled Go/C++ binaries (like agy) that crash with SIGILL (illegal instruction) or hardware validation errors (like aes, pclmul, avx) on legacy processors.
---

# Go Binary CPU Instruction Compatibility Skill

This skill contains the compiled intelligence and actionable tools to diagnose, patch, and run compiled Go/C++ binaries (such as `agy` or related tools) that require modern processor instruction sets (e.g. `AES-NI`, `PCLMULQDQ`, `AVX`, `AVX2`) on legacy processors (such as Intel Xeon Westmere or Nehalem CPUs lacking these flags).

---

## 1. Problem Classification

When a compiled binary executes unsupported instructions on legacy hardware, it will crash in one of two ways:

1.  **Early Runtime Validation Failures (Fast Fail):**
    The binary halts during early initialization before `main` runs, printing a fatal message to stderr:
    `FATAL ERROR: This binary was compiled with aes enabled, but this feature is not available on this processor (go/sigill-fail-fast).`
2.  **Raw SIGILL (Illegal Instruction) Exceptions:**
    The binary halts in the middle of execution (e.g. inside hash tables or cryptographic blocks) with:
    `Illegal instruction (core dumped)`

---

## 2. Diagnosis Workflow

### Step 2.1: Locate the Binary and Test Execution
Find where the binary is installed (e.g., `~/.local/bin/agy`) and execute it. If it fails with `Illegal instruction` or `FATAL ERROR`, run it under GDB in batch mode to find the crash site and backtrace:

```bash
gdb -batch -ex "run" -ex "bt" /path/to/binary
```

### Step 2.2: Identify the Crash Site and File Offset
Under GDB, identify the program counter (`$pc` or `RIP`) at the crash site. Query the mapped memory addresses to calculate the physical file offset of the instruction:

1.  Run `info proc mappings` inside GDB.
2.  Locate the base load address of the executable's `r-xp` mapping.
3.  Calculate the relative file offset:
    `file_offset = crash_address - base_load_address`

### Step 2.3: Disassemble and Verify Instruction Bytes

#### Option A: Using Rizin (Recommended)
You can directly disassemble instruction bytes at a specific crash address using `rizin`:
```bash
rizin -c "pd 10 @ <crash_address>" /path/to/binary
```

#### Option B: Using Capstone (Python)
Using `uv run`, you can run the capstone disassembler instantly without globally installing packages:
```bash
uv run python3 -c '
from capstone import *
import sys
with open("/path/to/binary", "rb") as f:
    f.seek(<file_offset>)
    code = f.read(32)
md = Cs(CS_ARCH_X86, CS_MODE_64)
for insn in md.disasm(code, <crash_address>):
    print(f"0x{insn.address:x}: {insn.mnemonic:10} {insn.op_str} bytes={insn.bytes.hex()}")
'
```

---

## 3. Emulation vs. Static Patching

Depending on where the instruction resides, choose the appropriate workflow:

### Track A: Static Patching (for cold initialization checks)
If the instruction triggers during initialization (e.g. Go CPUID runtime check) and is only run once, you can statically patch the binary using `rizin` in write mode (`-w`).

#### Handling Address Shuffles in Updates (Automated Patch Finding)
When `agy` is updated, the compiler will shuffle addresses, and the hardcoded patch offset will change. Use one of these methods to find the new patch location:

1. **Symbol Lookup (Non-Stripped Binaries):**
   Open the binary in `rizin` and find the symbols:
   ```bash
   rizin /path/to/binary
   > is ~cpu.Initialize
   ```
   Seek directly to the symbol location and apply the bypass patch.

2. **String XREF Lookup (Stripped Binaries):**
   Search for the unique error string to find the validation function:
   ```bash
   # Open binary in write mode
   rizin -w /path/to/binary

   # Search for the target error string
   > / FATAL ERROR: This binary was compiled with aes enabled

   # Find cross-references (XREFs) to the string address found (e.g. 0x08123450)
   > axt 0x08123450

   # Seek to the instruction referencing it (the check block caller)
   > s <address_of_caller>

   # Apply the relative jump bypass patch at the beginning of the check block
   > wx e9b300000090
   > q
   ```

### Track B: Signal Emulation (for hot loop execution)

If the binary contains compiled instructions (such as `aesdec`, `aesenc`, or `pclmulqdq`) executed on runtime hotpaths, static patching of instructions is unsafe. Instead, use the **LD_PRELOAD signal emulation layer** located at `~/src/agy-compat-toolkit`.

### How the Signal Emulator Works
The shared library (`sigill_emulator.so`) hooks `SIGILL` signals at process initialization. When a `SIGILL` occurs, the handler:
1.  Verifies `RIP` points to a valid operand pattern (such as SSE size prefix `0x66`).
2.  Decodes any `REX` prefixes (extending register indexes to `XMM8`-`XMM15`).
3.  Decodes the `ModRM` byte (and registers).
4.  Emulates the instruction in software (supporting complete round semantics for `AESDEC`, `AESDECLAST`, `AESENC`, `AESENCLAST`, and `PCLMULQDQ`).
5.  Saves updated states back into the thread's FPU mapping (`uc->uc_mcontext.fpregs->_xmm`).
6.  Advances `RIP` and returns to resume execution transparently.

### Compilation and Setup
1.  Navigate to the project toolkit folder: `~/src/agy-compat-toolkit`
2.  Build the dynamic library:
    ```bash
    make
    ```
3.  Install it globally:
    ```bash
    make install
    ```
4.  Configure a transparent wrapper script replacing the original target:
    ```bash
    #!/bin/bash
    export LD_PRELOAD=/home/michael/sigill_emulator.so
    exec /path/to/binary.real "$@"
    ```

---

## 4. Rebuilding the Toolkit
The source code, Makefiles, bad-instruction scanner, and reference material are stored inside `/home/michael/src/agy-compat-toolkit`. Use this folder for future upgrades.

---

## 5. Alternative Implementations & Reference Libraries

### C vs. Rust Tradeoffs for LD_PRELOAD Emulators
*   **C (Current implementation):**
    *   *Pros:* Extremely small binary footprint (~20 KB), zero dependencies, direct FFI-free mapping of libc context structs (`ucontext_t`), and fast load times.
    *   *Cons:* Memory safety is manual, and decoding complex prefixes (like AVX/AVX512 VEX/EVEX) requires writing error-prone parsing code.
*   **Rust implementation:**
    *   *Pros:* Safe instruction parsing using modern crates like `iced-x86` or `zydis-rs` (disassembler/decoder crates), preventing bounds/overflow errors.
    *   *Cons:* Larger binary size (300 KB+), potential conflicts with target application memory allocators (e.g. jemalloc), and complex cross-platform definitions for libc context structures.

### Reference Libraries for AES-NI & Decoding
If scaling the emulation layer, the following libraries are recommended:
1.  **Instruction Decoding:**
    *   [Zydis](https://github.com/zyantific/zydis): Fast and lightweight x86/x86-64 decoder.
    *   [iced-x86](https://github.com/icedland/iced): High-performance Rust-based x86 instruction decoder/assembler.
2.  **Portable AES implementation:**
    *   [tiny-AES-c](https://github.com/kokke/tiny-AES-c): Ultra-lightweight standalone C implementation.
    *   [mbedTLS](https://github.com/Mbed-TLS/mbedtls): Lightweight cryptolib with portable software fallback rounds.
