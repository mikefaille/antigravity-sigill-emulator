---
name: agy-compat
description: Use when analyzing, patching, or debugging compiled Go/C++ binaries (like agy) that crash with SIGILL (illegal instruction) or hardware validation errors (like aes, pclmul, avx) on legacy processors.
---

# Go Binary CPU Instruction Compatibility Skill

This document is the operational playbook for diagnosing, patching, and running compiled Go/C++ binaries on CPUs that lack modern instruction sets (such as `AES-NI`, `PCLMULQDQ`, `AVX`, or `AVX2`).

---

## 🟢 1. Problem Classification & Target Selection

### A. Crash Types
1.  **Early Validation Failures (Fast Fail)**:
    *   Symptom: Binary prints validation warning and terminates immediately on start:
        `FATAL ERROR: This binary was compiled with aes enabled, but this feature is not available on this processor (go/sigill-fail-fast).`
    *   Action: **Static Patching** (Track A).
2.  **Raw SIGILL Exceptions**:
    *   Symptom: Binary executes initially, but crashes during cryptographically heavy execution loops:
        `Illegal instruction (core dumped)`
    *   Action: **Signal Emulation Preload** (Track B).

### B. Library Target Selection
Choose the correct precompiled shared object based on the virtualized or physical processor architecture:
*   `sigill_emulator_v1.so` (`x86-64` generic): Target for pre-2009 processors (Nehalem precursor / Core 2).
*   `sigill_emulator_v2.so` (`x86-64-v2` standard): Target for Nehalem, Westmere, or guest VMs without CPU pass-through (requires SSE3, SSSE3, SSE4.1, SSE4.2, POPCNT).
*   `sigill_emulator.so` (`native` host): Autodetects and optimizes code generation for the compiler host.

---

## 🟡 2. Diagnostic & Analysis Commands (Structured)

### Step 1: Extract Crash Address (`RIP`)
Run the binary inside GDB in non-interactive batch mode to dump the crash context:
```bash
gdb -batch -ex "run" -ex "bt" --args /path/to/binary [args...]
```

### Step 2: Compute Target File Offset
Identify the mapping base load address to compute the physical offset inside the executable file:
1. Run `info proc mappings` in GDB.
2. Locate the first address matching the executable (`r-xp`) mapping of the binary.
3. Compute offset:
   $$\text{File Offset} = \text{Crash Address (RIP)} - \text{Base Load Address}$$

### Step 3: Extract Instruction Opcode Bytes
Run a Capstone disassembler check in Python via `uv` to read the bytes at the calculated file offset:
```bash
uv run python3 -c '
from capstone import *
with open("/path/to/binary", "rb") as f:
    f.seek(<computed_file_offset>)
    code = f.read(16)
md = Cs(CS_ARCH_X86, CS_MODE_64)
for insn in md.disasm(code, <crash_address>):
    print(f"DISASM: 0x{insn.address:x}  {insn.bytes.hex()}  {insn.mnemonic} {insn.op_str}")
'
```

---

## 🔴 3. Patching & Preload Execution

### Track A: Static Patching (Resilient Auto-Patcher & Rizin)
For binaries failing initialization CPU validation tests, patch the conditional branch checks.

#### 1. Recommended Method: Automated Signature-Based Patcher
To automate patching and ensure resilience against binary upgrades, use the Python byte-signature auto-patcher:
```bash
python3 /home/michael/patch_agy.py /path/to/binary
```
*(This automatically scans for Go's `cpu.Initialize` pattern `55 48 89 e5 53 50 e8 [4 bytes] 8b 05 [4 bytes] a9 00 00 04 00 0f 84`, dynamically locates the nearest function epilogue, and applies the relative JMP patch cleanly. This is 100% resilient to compiler layout and address changes).*

#### 2. Manual Method: Static Patching via Rizin
If manually locating checking offsets:
```bash
# Non-interactive search and relative JMP patch
rizin -w -c "s <offset_of_validation_check>; wx e9b300000090; q" /path/to/binary
```
*(The patch `e9b300000090` writes `jmp <return_block_offset>` plus one `nop` padding).*

### Track B: Shared Library preloader
For mid-execution instruction exceptions, inject the emulation library. 

**Note on Stack Alignment:** Option B (Experimental JIT Mode) is fully hardened against non-aligned stack frames (such as the Go runtime's non-standard 8-byte alignment) because the C helper callback uses `__attribute__((force_align_arg_pointer))` to dynamically realign the stack pointer to 16 bytes.

```bash
# 1. Compile all targets
make

# 2. Install targets atomically (prevents linker crashes on running instances)
make install

# 3. Preload the dynamic library (Safe Mode Option A is default)
LD_PRELOAD=/home/michael/sigill_emulator_v2.so /path/to/binary

# 4. Preload in JIT Experimental Mode Option B (if security policies permit)
EMU_MODE=experimental LD_PRELOAD=/home/michael/sigill_emulator_v2.so /path/to/binary
```

---

## 🔵 4. Verification & Testing

Verify that all emulated targets are functioning correctly using the Comparative Test Suite:

```bash
# Compile and run orchestrator benchmark
make clean && make && make benchmark
./run_benchmark
```

### Performance Expectations:
*   **Safe Mode (Option A)**: Traps per second $\approx 600,000$, Average latency $\approx 1,600 \text{ ns}$.
*   **Experimental Mode (Option B)**: Traps per second $\approx 8,000,000$, Average latency $\approx 120 \text{ ns}$.
*   **Failsafe Fallback Check**: If `mprotect` or JIT page allocation fails under strict SELinux/AppArmor, the library must automatically degrade to Option A on a per-instruction basis.
