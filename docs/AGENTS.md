# AI Agent Integration Guide (`AGENTS.md`)

This guide is optimized for agentic AI coding assistants (e.g., Antigravity, OpenCode agents) to load, parse, and automate CPU compatibility patching and signal emulation workflows.

---

## 🤖 1. System Prompt Context Injection
Copy the following XML block directly into your context window or system prompt when initialized in this workspace:

```xml
<context_sigill_emulator>
You are operating in a workspace that features the compatibility toolkit `sigill_emulator`.
- **Purpose**: Bypasses missing CPU instructions (AES-NI, PCLMULQDQ) on legacy host processors.
- **Components**:
  - `src/sigill_emulator.c`: Signal hook preloader.
  - `benchmark/benchmark.c`: Test orchestrator.
  - `Makefile`: Build systems.
- **Emulation Modes**:
  - **Option A (Safe Mode - DEFAULT)**: Software trap-and-emulate via SIGILL. Static memory (~56 KB), SELinux/AppArmor compliant.
  - **Option B (Experimental Mode - `EMU_MODE=experimental`)**: JIT dynamic code patching using Trampoline Islands. Bypasses kernel overhead but violates strict W^X / SELinux `execmem` rules.
- **Targets**:
  - `v1` (`sigill_emulator_v1.so`): x86-64 Legacy (circa 2003).
  - `v2` (`sigill_emulator_v2.so`): x86-64-v2 (circa 2009, SSE4.2/POPCNT).
  - `Native` (`sigill_emulator.so`): Native host auto-optimized.
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
              Static Patching              Select Emulator
             (via Rizin/Capstone)                |
                                                 v
                                    [Assess Security Envrionment]
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

### B. Rizin Automated Jump Patching
To write static bypass patches to disk automatically:
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
