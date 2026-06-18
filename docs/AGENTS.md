# AI Agent Integration Guide (`AGENTS.md`)

This guide is optimized for agentic AI coding assistants (e.g., Antigravity, OpenCode agents) to load, parse, and automate CPU compatibility patching and signal emulation workflows.

---

## 🤖 1. System Prompt Context Injection
Copy the following XML block directly into your context window or system prompt when initialized in this workspace:

```xml
<context_sigill_emulator>
You are operating in a workspace that features the compatibility toolkit `sigill_emulator`.
- **Purpose**: Bypasses missing CPU instructions (AES-NI, PCLMULQDQ, AVX, AVX2, FMA) on legacy host processors.
- **Target Hardware**: Intel Xeon E5520 (Nehalem, LGA1366). Has SSE4.2/POPCNT. Lacks AVX, AVX2, FMA, AES-NI, PCLMULQDQ, F16C.
- **Components**:
  - `src/sigill_emulator.c`: Signal hook preloader. Contains JVM-handler reassert watchdog thread (polls kernel every 50ms, reinstalls our handler when JVM displaces it via raw `syscall(SYS_rt_sigaction)`).
  - `src/avx/`: VEX decoder, YMM state management, per-instruction handlers.
  - `src/math/`: SIMDe-backed math backend (portably lowered to SSE4.2).
  - `benchmark/benchmark.c`: Test orchestrator.
  - `Makefile`: Build system. LDLIBS includes `-lpthread` for watchdog thread.
  - `scripts/patch_agy.py` (or `~/patch_agy.py`): Multi-signature static patcher (Track A).
  - `~/.local/bin/agy`: Wrapper — auto-applies Track A patch on binary update, then launches with LD_PRELOAD (Track B).
  - `~/.local/bin/bitwig-compat`: JVM-specific launcher. Injects `JDK_JAVA_OPTIONS=-XX:UseAVX=0` + `LD_PRELOAD=sigill_emulator.so`.
- **Emulation Modes**:
  - **Option A (Safe Mode - DEFAULT)**: Software trap-and-emulate via SIGILL. Static memory (~56 KB), SELinux/AppArmor compliant.
  - **Option B (Experimental Mode - `EMU_MODE=experimental`)**: JIT dynamic code patching using Trampoline Islands. 10x-15x faster but violates W^X.
- **Targets**:
  - `v1` (`sigill_emulator_v1.so`): x86-64 Legacy (circa 2003).
  - `v2` (`sigill_emulator_v2.so`): x86-64-v2 (circa 2009, SSE4.2/POPCNT).
  - `Native` (`sigill_emulator.so`): Native host auto-optimized. Installed at `~/sigill_emulator.so`.
- **JVM Signal Bypass**: JVM calls `syscall(SYS_rt_sigaction)` directly, bypassing the libc `sigaction()` interposer. The reassert watchdog thread detects and fixes this every 50ms for 10s after load.
- **Patcher signature variants** (tried in order): `primary-aes-bit18`, `generic-feature-check`, `no-push-rax`, `test-al-byte-check`. Exit 0 on success or already-patched; exit 1 on incompatible binary.
</context_sigill_emulator>
```

---

## 🧠 2. AI Decision Matrix
When diagnosing a crash or optimizing compilation, follow this decision tree to choose the optimal mode and target:

```text
                        [SIGILL Crash Detected]
                                   |
                     +-------------+-------------+
                     |                           |
        [CPUID / Early Gate Check]       [Mid-loop execution]
                     |                           |
                     v                           v
        Static Patching (Track A)          Select Emulator
        python3 patch_agy.py <bin>               |
        (auto-runs via agy wrapper               |
         on each binary update)                  |
                                                 v
                                    [Assess Security Environment]
                                                 |
                       +-------------------------+-------------------------+
                       |                                                   |
              [Hardened/Sandboxed]                                [Open/Privileged]
           (SELinux/AppArmor Enforced)                          (Normal VM or Bare-Metal)
                       |                                                   |
                       v                                                   v
               Option A (Safe Mode)                             Option B (Experimental Mode)
             - Trap-and-emulate math                          - JIT Trampoline Islands
             - Statically allocated cache                     - Bypasses kernel traps
             - Zero mmap/mprotect calls                       - 10x-15x performance speedup
                       |                                                   |
                       +-------------------------+-------------------------+
                                                 |
                                                 v
                                     [Select Hardware Target]
                                                 |
                       +-------------------------+-------------------------+
                       |                         |                         |
                [Pre-2009 CPU]            [Post-2009 CPU]           [Current Host Compile]
                 (e.g., v1)              (e.g., v2/SSE4.2)             (e.g., Native)
                       |                         |                         |
                       v                         v                         v
               sigill_emulator_v1.so     sigill_emulator_v2.so     sigill_emulator.so
```

---

## 🛠️ 3. Non-Interactive Command Schemas
AI agents must use non-interactive commands to analyze, compile, and patch without blocking.

### A. Capstone Disassembly Extraction
To inspect instruction bytes at a specific crash offset programmatically:
```bash
uv run python3 -c '
from capstone import *
import sys
with open("test_binary", "rb") as f:
    f.seek(file_offset)
    code = f.read(16)
md = Cs(CS_ARCH_X86, CS_MODE_64)
for insn in md.disasm(code, crash_address):
    print(f"OPCODE_DATA: address=0x{insn.address:x} bytes={insn.bytes.hex()} mnemonic={insn.mnemonic}")
'
```

### B. Static Patcher (Preferred over Rizin)
Run the multi-signature auto-patcher — it handles already-patched detection, epilogue search, and backup automatically:
```bash
python3 ~/patch_agy.py /path/to/binary
# exit 0 = patched or already patched; exit 1 = incompatible
# check /tmp/agy_patch.log for sha256/offset/bytes detail
```

### C. Rizin Manual Jump Patching
Use only when `patch_agy.py` fails and you have a known offset:
```bash
# Apply relative JMP (E9 B3 00 00 00) + NOP (90) to validation offset in write-mode (-w)
rizin -w -c "s <validation_offset>; wx e9b300000090; q" /path/to/binary
```

---

## 🧪 4. Automated Verification Loop
After making changes to the math emulation or patching code, agents must execute the automated test loop to guarantee compatibility:

```bash
# 1. Compile all targets and the benchmark program
make clean && make && make benchmark

# 2. Run the automated parent benchmark orchestrator
./run_benchmark
```

### Parsing Benchmark Output:
*   **Verify Baseline**: The `Baseline (No Emulator)` line must print `Crashed (SIGILL as expected)`. If it exits with `0`, the host hardware natively supports the instructions (emulation is redundant).
*   **Verify Emulator Performance**: 
    *   Timings for `v1`, `v2`, and `Native` under `Safe` and `Experimental` modes must be reported without crashes.
    *   Timings for `Experimental` modes must be $\ge 10\times$ faster than `Safe` modes (typically ~120 ns vs ~1,700 ns).
    *   If any line prints `Crashed` or `Exited with code X`, the agent must inspect the signal handlers and decoding logic for missing instruction coverage.

---

## 🦴 5. JVM Application Targets (e.g. Bitwig Studio)

### A. JVM Signal Bypass (Critical Discovery — 2026-06-17)
The JVM (OpenJDK 25 / Azul Zulu 25) calls `syscall(SYS_rt_sigaction)` **directly**, bypassing libc `sigaction()`, to install its own SIGILL `crash_handler`. Evidence in JVM crash log:
```
SIGILL: crash_handler in libjvm.so, flags=SA_RESTART|SA_SIGINFO, unblocked
```
Fix: the **reassert watchdog thread** in `src/sigill_emulator.c` polls the kernel every 50ms for 10s and reinstalls our handler via raw syscall when displaced. Requires `-lpthread` in `LDLIBS`.

### B. JIT AVX Suppression
```bash
JDK_JAVA_OPTIONS="-XX:UseAVX=0"  # integer flag, no +/- prefix
```
Bitwig's launcher is a compiled ELF binary (not a shell script), so `.vmoptions` files don't work. `JDK_JAVA_OPTIONS` is the only injection vector. Confirms active with: `NOTE: Picked up JDK_JAVA_OPTIONS: -XX:UseAVX=0` in log.

### C. Compatibility Launcher (installed)
```bash
# ~/.local/bin/bitwig-compat  — on PATH
exec env \
    JDK_JAVA_OPTIONS="-XX:UseAVX=0" \
    LD_PRELOAD="${HOME}/sigill_emulator.so${LD_PRELOAD:+:$LD_PRELOAD}" \
    /usr/bin/bitwig-studio "$@"
```

### D. Iterative Next-Instruction Debug Loop
```bash
# Run with opcode trace:
timeout 60 env JDK_JAVA_OPTIONS="-XX:UseAVX=0" \
  LD_PRELOAD=~/sigill_emulator.so DEBUG_EMU=1 \
  bitwig-studio 2>&1 | tee /tmp/bitwig_test.log

# Find last unhandled opcode (last DECODE line before crash):
tail -30 ~/.BitwigStudio/log/BitwigStudio.log | grep -E "Op=|SIGILL|fatal"

# Disassemble the crash offset:
objdump -d /opt/bitwig-studio/lib/bitwig-studio/libbitwig-jni.so \
  | awk '/<HEX_OFFSET>:/{p=1} p{print; if(++n>20)exit}'

# Implement, rebuild, retest:
make clean && make && make install
```

### E. Bitwig Studio 6.0 Known Crash Sequence (libbitwig-jni.so, Skia/getFontExtents)
| Offset | Instruction | Op | Status |
|---|---|---|---|
| `+0x3c7119` | `vinsertps $0x10, 0xc(%rdi), %xmm0, %xmm1` | 0x21 map=3 pp=1 | ✅ |
| `+0x3c7135` | `vpsubd %xmm2, %xmm0, %xmm2` | 0xFA map=1 pp=1 | ⚠️ xmm form |
| `+0x3c7139` | `vblendvps %xmm1, %xmm2, %xmm1, %xmm2` | 0x4A map=3 pp=1 | ✅ |

