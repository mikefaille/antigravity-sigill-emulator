# Compatibility Engineering Session Log

This document records the original engineering session debugging the `agy` start-up crashes and implementing the dynamic signal emulator. It is structured from high-level concepts to low-level engineering bytes.

---

## 🟢 Novice Level: Why Did It Crash and How Did We Solve It?

### 1. The Startup Failure
When running the `agy` tool, the program crashed immediately on start with the error:
```text
FATAL ERROR: This binary was compiled with aes enabled, but this feature is not available on this processor (go/sigill-fail-fast).
Illegal instruction (core dumped)
```

### 2. Why Did This Happen?
This error is caused by a double safety check inside `agy`:
1. **The Gatekeeper Check (Initialization)**: When the program starts, Go's runtime queries the CPU's flags. If it sees that the CPU is old and lacks cryptographic acceleration (like AES-NI), it writes a fatal error to the terminal and shuts down.
2. **The Actual Work (Hot Loops)**: If we bypass that gatekeeper check, the program runs, but eventually executes raw cryptographic or hashing instructions. If the CPU hits these instructions, it triggers a hardware crash (`Illegal Instruction`).

### 3. The Dual Solution
To get `agy` working without crashes, we applied a **dual solution**:
* **Static Bypass**: We modified a few bytes in the binary's gatekeeper code. This makes the program skip the initial feature check, allowing the startup code to run.
* **Dynamic Emulation**: We created a shared library that runs alongside the program. Whenever the program executes a raw cryptographic instruction, the library catches the signal, performs the math in software, and lets the program proceed.

---

## 🟡 Developer Level: Diagnostics and Code Offsets

To apply the static bypass and compile the emulator, we mapped the binary structure:

### 1. Locating the Code Section (.text)
We ran `readelf` on the binary to find where the executable machine code lies:
* Command: `readelf -S /home/michael/.local/bin/agy`
* Result: `.text` section starts at address `0x04b13000` with file offset `0x04b13000`.

### 2. Identifying the Crash Address
We executed the binary under GDB to capture the instruction address where the `SIGILL` occurs:
* Crashed at: `0x000055555c467efe`
* Instruction: `aesdec %xmm1,%xmm0`

### 3. Finding the File Offset
Since GDB loads the executable at a randomized/base address, we calculated the relative offset inside the file on disk:
* GDB Base Load Address: `0x0000555555400000`
* Offset = `0x55555c467efe - 0x555555400000 = 0x7067efe`
* disassembling bytes at file offset `0x7067efe` verified the exact instruction as: `66 0f 38 de c1` (`aesdec xmm0, xmm1`).

---

## 🔴 Expert Level: Surgical Bytes and Code Patches

Below is the low-level description of the binary patches and dynamic emulation hooks.

### 1. The Go Runtime Check Bypass (Static Patch)
The gatekeeper check function `cpu.Initialize` was located at file offset `0x74D0F00`.

* **Original Assembly Sequence**:
  * Offset `0x74D0F06`: `E8 D5 3D 09 00` (`call cpu.Initialize`)
  * Offset `0x74D0F0B`: `8B 05 FF 0F 4A 02` (`mov 0x24a0fff(%rip), %eax`) - this moves the CPUID capabilities to `%eax` for validation.
  
* **Bypass Patch**:
  We patched the instruction at `0x74D0F0B` with a relative jump (`jmp`) targeting the function epilogue (return block) at `0x74D0FC3`:
  * Epilogue location: `0x74D0FC3` (`add $0x8, %rsp; pop %rbx; pop %rbp; ret`)
  * Patch bytes written: `E9 B3 00 00 00 90`
    * `E9 B3 00 00 00` -> relative jump instruction (`jmp 0x74d0fc3`).
    * `90` -> `NOP` padding to match the original 6-byte instruction boundary.
  
This bypasses Go's CPU check validation function while allowing `cpu.Initialize` to correctly record `cpu.X86.HasAES = false`, steering the binary into code paths where we can dynamically catch the instructions.

### 2. Integration Wrapper
We saved the patched binary as `agy.real` and created a shell wrapper `agy`:
```bash
#!/bin/bash
export LD_PRELOAD=/home/michael/sigill_emulator.so
exec /home/michael/.local/bin/agy.real "$@"
```
This ensures the emulation layer is injected into the program memory space before `main` starts executing.

---

## 🔵 Upgraded Level: Dynamic Addressing-Mode Decoding

### 1. Memory Operand Support
During code review, we discovered a RIP-relative memory operand instruction:
```text
0x741591c: pclmulqdq xmm0, xmmword ptr [rip - 0x29ce8a6], 0x10
```
Because the initial emulator only supported register-to-register operands, we upgraded `sigill_emulator.c` to include a full, robust **x86_64 addressing-mode decoder** (`resolve_mem_addr`). It parses ModRM, SIB byte scales, indices, base registers, and 8-bit/32-bit displacements to dynamically calculate effective memory addresses. 

This enables the emulator to seamlessly handle both register-to-register and register-to-memory SSE instructions at runtime, ensuring that no `pclmulqdq` instruction with memory operands triggers a crash.

