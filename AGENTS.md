# AI Agent Integration Guide (`AGENTS.md`)

This guide explains how agentic AI coding assistants (like Antigravity or OpenCode agents) can load, understand, and apply the binary compatibility tools in this repository.

---

## 1. What is the Compatibility Skill?
The [SKILL.md](file:///home/michael/src/agy-compat-toolkit/SKILL.md) file is a structured, agentic instruction playbook. It contains the classification criteria, disassembly workflows, static patching procedures, and signal emulation instructions required to execute binaries compiled for modern instruction sets (AES-NI, AVX, PCLMULQDQ) on legacy host processors.

---

## 2. Ingesting the Playbook
When an agent encounters a program crash with an `Illegal instruction` (`SIGILL`) or an early runtime validation failure (e.g., `FATAL ERROR: This binary was compiled with aes enabled...`), the agent should:

1. **Load the Skill**: Read the contents of [SKILL.md](file:///home/michael/src/agy-compat-toolkit/SKILL.md).
2. **Determine the Track**:
   * **Track A (Static Patching)**: If the binary aborts during initialization via a CPUID-check before execution reaches the program hotpaths, the agent must statically patch the check instructions using `rizin`.
   * **Track B (Signal Emulation)**: If the binary fails mid-execution during performance-critical cryptographic or hashing loops, the agent must compile and install the `sigill_emulator.so` preloader.

---

## 3. Automated Agent Execution Workflow

### A. Diagnosing the Crash Address
The agent should execute the target program under GDB in batch mode to pinpoint the instruction pointer (`RIP`) and disassemble the faulting instruction:
```bash
gdb -batch -ex "run" -ex "bt" /path/to/binary
```

### B. Patching with Rizin
If the crash is in the initialization CPUID gate, the agent should seek the function using symbol lookup or string cross-references (`axt`) to bypass the check:
```bash
# Example script injected by agent:
rizin -w /path/to/binary
> s <patch_offset>
> wx e9b300000090
> q
```

### C. Deploying the Emulator Wrapper
If dynamic emulation is needed, the agent must build the preloader:
```bash
make && make install
```
Then, write a wrapper script that sets `LD_PRELOAD=/home/michael/sigill_emulator.so` and executes the target binary.
