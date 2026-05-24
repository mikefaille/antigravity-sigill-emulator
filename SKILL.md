---
name: agy-compat
description: Use when analyzing, patching, or debugging compiled Go/C++ binaries (like agy) that crash with SIGILL (illegal instruction) or hardware validation errors (like aes, pclmul, avx) on legacy processors.
---

# Go Binary CPU Instruction Compatibility Skill

This document is the operational playbook for diagnosing, patching, and running compiled Go/C++ binaries on CPUs that lack modern instruction sets (such as `AES-NI`, `PCLMULQDQ`, `AVX`, or `AVX2`).

---

## 🟢 Novice Level: Problem Classification and Setup

### 1. How Compatibility Problems Manifest
When a compiled binary executes unsupported hardware instructions on a legacy CPU, it will halt in one of two ways:

*   **Type 1: Early Validation Failures (Fast Fail)**
    The program halts during initialization before the application starts, printing a validation error to stderr:
    `FATAL ERROR: This binary was compiled with aes enabled, but this feature is not available on this processor (go/sigill-fail-fast).`
*   **Type 2: Raw SIGILL (Illegal Instruction) Exceptions**
    The program starts successfully, but halts in the middle of executing work (e.g. during map hash checks) with:
    `Illegal instruction (core dumped)`

### 2. Quick Setup: The Signal Emulator
If you just want to run the binary, we use the **LD_PRELOAD emulator**:
1.  Navigate to the toolkit folder: `~/src/agy-compat-toolkit`
2.  Compile the library:
    ```bash
    make
    ```
3.  Install it to your home directory:
    ```bash
    make install
    ```
4.  Run your program wrapped with the library:
    ```bash
    LD_PRELOAD=/home/michael/sigill_emulator.so /path/to/binary.real "$@"
    ```

---

## 🟡 Developer Level: Diagnostics and Verification

For debugging crash sites and verifying target binaries, follow this workflow:

### Step 1: Trace the Crash site in GDB
Run the binary under GDB in batch mode to capture the program counter (`RIP`) and backtrace at the crash site:
```bash
gdb -batch -ex "run" -ex "bt" /path/to/binary
```

### Step 2: Calculate the relative file offset
To modify or inspect the binary, find the physical file offset of the instruction:
1. Run `info proc mappings` inside GDB.
2. Locate the base load address of the executable's `r-xp` (executable) mapping.
3. Subtract the base address from the crash instruction address:
   `file_offset = crash_address - base_load_address`

### Step 3: Inspect Instruction Bytes using Capstone
Run a quick Python check (using `uv run` for zero-install environment isolation) to disassemble the bytes at that offset:
```bash
uv run python3 -c '
from capstone import *
with open("/path/to/binary", "rb") as f:
    f.seek(<file_offset>)
    code = f.read(32)
md = Cs(CS_ARCH_X86, CS_MODE_64)
for insn in md.disasm(code, <crash_address>):
    print(f"0x{insn.address:x}: {insn.mnemonic:10} {insn.op_str} bytes={insn.bytes.hex()}")
'
```

---

## 🔴 Expert Level: Surgical Patching and Signal Hooks

This section details static binary modifications and signal chaining mechanics for advanced engineers.

### 1. Static Patching with Rizin (Bypassing Validation Gates)
For initialization checks that prevent the binary from starting (Type 1 checks), we apply a static jump patch. If the binary is updated, compile address shuffles will shift the patch location. Use these methods to apply the patch:

*   **Method A: Symbol Lookup (Non-Stripped Binaries)**
    Open the binary in `rizin` and find the CPU initialization check function:
    ```bash
    rizin /path/to/binary
    > is ~cpu.Initialize
    ```
    Seek directly to that function symbol and write the relative JMP bypass patch.
*   **Method B: String XREF Lookup (Stripped Binaries)**
    If symbols are missing, find the error message string reference to locate the validation block:
    ```bash
    # Open binary in write mode
    rizin -w /path/to/binary

    # Search for the validation error string
    > / FATAL ERROR: This binary was compiled with aes enabled

    # List cross-references (XREFs) to the string's virtual address (e.g. 0x08123450)
    > axt 0x08123450

    # Seek to the instruction referencing it (the check block caller)
    > s <address_of_caller>

    # Write a relative JMP (E9 B3 00 00 00) + NOP (90) to bypass validation
    > wx e9b300000090
    > q
    ```

### 2. Signal Chaining Fallback Mechanics
Inside `sigill_emulator.c`, we hook `SIGILL` using `sigaction`. If an instruction is encountered that our emulator does not handle, we delegate it back to the original signal handler:
1.  **SA_SIGINFO handler**: If `old_sa.sa_flags & SA_SIGINFO` is true, we call `old_sa.sa_sigaction(sig, si, ctx_void)`.
2.  **Legacy handler**: If a standard handler was registered, we call `old_sa.sa_handler(sig)`.
3.  **SIG_DFL (Default)**: If no handler was registered, we temporarily restore the default handler and call `kill(getpid(), SIGILL)` to allow the operating system to terminate the program and write a standard core dump.

---

## 🔵 Upgraded Level: Dynamic Addressing-Mode Decoding

### 1. Memory Operand Support
During code review, we discovered a RIP-relative memory operand instruction:
```text
0x741591c: pclmulqdq xmm0, xmmword ptr [rip - 0x29ce8a6], 0x10
```
Because the initial emulator only supported register-to-register operands, we upgraded `sigill_emulator.c` to include a full, robust **x86_64 addressing-mode decoder** (`resolve_mem_addr`). It parses ModRM, SIB byte scales, indices, base registers, and 8-bit/32-bit displacements to dynamically calculate effective memory addresses. 

This enables the emulator to seamlessly handle both register-to-register and register-to-memory SSE instructions at runtime, ensuring that no `pclmulqdq` instruction with memory operands triggers a crash.

