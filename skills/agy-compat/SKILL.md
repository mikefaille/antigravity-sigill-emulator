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
Use the Python auto-patcher to patch any compatible Go binary. Run it once after each binary update:
```bash
python3 ~/patch_agy.py /path/to/binary
```
Exit codes: `0` = success (patched or already patched), `1` = incompatible binary. Patch output (binary sha256, size, mtime, matched signature, patch site) is logged to stdout for diagnostics.

**Signature variants tried in order:**
1. `primary-aes-bit18` — full prologue + `MOV EAX,[RIP+x]` + `TEST EAX,0x40000` + `JE` (stable across all known builds May 2026–present)
2. `generic-feature-check` — same prologue, wildcards the 4-byte TEST mask (survives future Go cpu feature bit reassignment)
3. `no-push-rax` — shorter frame variant without `push rax`, jmp_offset=10
4. `test-al-byte-check` — `TEST AL,imm8` compact form (pclmulqdq-style byte checks)

**Epilogue variants tried in order:** `add rsp,8/16/24` + `pop rbx; pop rbp; ret`, `pop rbx; pop rbp; ret`, `pop rbp; ret`.

**Already-patched detection:** Before scanning for unpatched signatures, the patcher looks for its own `JMP+NOP` fingerprint in the expected position. If found, it exits `0` without re-reading or re-patching.

#### 2. Automatic Update Integration (`agy` wrapper)
The `~/.local/bin/agy` wrapper script automatically re-applies Track A whenever `agy.real` is replaced by a self-update:
```bash
REAL=~/.local/bin/agy.real
MARKER=~/.local/bin/.agy.real.patched

if [ ! -f "$MARKER" ] || [ "$REAL" -nt "$MARKER" ]; then
    python3 ~/patch_agy.py "$REAL" >/tmp/agy_patch.log 2>&1 && touch "$MARKER"
fi

LD_PRELOAD=~/sigill_emulator.so exec "$REAL" "$@"
```
The marker file `~/.local/bin/.agy.real.patched` tracks the last-patched mtime. When `agy.real` is newer (self-update detected), Track A re-runs automatically before launch. Patch log: `/tmp/agy_patch.log`.

#### 3. Manual Method: Static Patching via Rizin
If manually locating check offsets:
```bash
# Non-interactive search and relative JMP patch
rizin -w -c "s <offset_of_validation_check>; wx e9b300000090; q" /path/to/binary
```
*(The patch `e9b300000090` writes `jmp <return_block_offset>` plus one `nop` padding).*

### Track B: Shared Library preloader
For mid-execution instruction exceptions, inject the emulation library. 

**Note on Stack Alignment:** Experimental JIT Mode is fully hardened against non-aligned stack frames (such as the Go runtime's non-standard 8-byte alignment) because the C helper callback uses `__attribute__((force_align_arg_pointer))` to dynamically realign the stack pointer to 16 bytes.

```bash
# 1. Compile all targets
make

# 2. Install targets atomically (prevents linker crashes on running instances)
make install

# 3. Preload the dynamic library (Safe Mode — default, SELinux/AppArmor compliant)
LD_PRELOAD=~/sigill_emulator_v2.so /path/to/binary

# 4. Preload in Experimental JIT Mode (10x faster; requires mprotect / writable code pages)
EMU_MODE=experimental LD_PRELOAD=~/sigill_emulator_v2.so /path/to/binary
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

---

## 🟣 5. JVM Targets (Bitwig Studio / OpenJDK)

### 5.1 The JVM Raw Syscall Bypass Problem
The JVM (OpenJDK 25 / Azul Zulu 25) calls `syscall(SYS_rt_sigaction)` **directly** — not the libc `sigaction()` wrapper — to install its own `crash_handler` for SIGILL. This silently replaces our emulator's handler even with `LD_PRELOAD`. The crash log will show:
```
SIGILL: crash_handler in libjvm.so, mask=..., flags=SA_RESTART|SA_SIGINFO, unblocked
```
even though `sigill_emulator.so` is visible in the memory map.

**Fix**: A background `pthread` reassert watchdog in `sigill_emulator.c` that polls the kernel SIGILL disposition via raw `syscall(SYS_rt_sigaction, SIGILL, NULL, &current, sizeof(sigset_t))` every 50 ms for 10 s and reinstalls our handler when displaced. See `src/sigill_emulator.c` for the full implementation. Requires `-lpthread` in `LDLIBS`.

### 5.2 Suppress JIT AVX Generation
```bash
JDK_JAVA_OPTIONS="-XX:UseAVX=0"   # integer flag, no + prefix
```
Stops the JIT compiler from emitting AVX in hotspot-compiled Java code. Does not affect native `.so` libraries (those still need SIGILL emulation). Injection via env var is required because Bitwig's launcher is a compiled C++ ELF binary, not a shell script — no `.vmoptions` file exists.

**Effect on Bitwig Studio 6.0**: Advances startup from ~16.5 s crash to ~28 s before the next native crash.

### 5.3 Compatibility Launcher
```bash
# ~/.local/bin/bitwig-compat  (installed and on PATH)
exec env \
    JDK_JAVA_OPTIONS="-XX:UseAVX=0" \
    LD_PRELOAD="${HOME}/sigill_emulator.so${LD_PRELOAD:+:$LD_PRELOAD}" \
    /usr/bin/bitwig-studio "$@"
```

### 5.4 Debug Workflow
```bash
# Run with opcode trace to find next unhandled instruction
timeout 60 env JDK_JAVA_OPTIONS="-XX:UseAVX=0" \
  LD_PRELOAD=~/sigill_emulator.so DEBUG_EMU=1 \
  bitwig-studio 2>&1 | tee /tmp/bitwig_test.log

# Last [DEBUG_AVX_DECODE] line before crash = next opcode to implement
tail -30 ~/.BitwigStudio/log/BitwigStudio.log | grep -E "Op=|SIGILL|fatal"

# Decode that offset in the native .so
objdump -d /opt/bitwig-studio/lib/bitwig-studio/libbitwig-jni.so \
  | awk '/<HEX_OFFSET>:/{found=1} found{print; if(NR>20)exit}'
```
