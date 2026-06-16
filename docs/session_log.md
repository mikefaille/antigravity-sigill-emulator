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
* Command: `readelf -S ~/.local/bin/agy`
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
export LD_PRELOAD=~/sigill_emulator.so
exec ~/.local/bin/agy.real "$@"
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


---

## 🌟 Advanced Engineering: Performance Tuning & Concurrency (Commits 1d79599 to 6c80c7e)

Below is the chronological log of engineering sprints, including precise commit hashes and architectural explanations of changes made to the codebase.

### 1. JIT Dynamic Code Patching & Compiler flag optimizations
*   **Commit Hash**: `1d79599`
*   **Engineering Changes**:
    *   Optimized compilation flags inside the [Makefile](../Makefile) to use `-march=native`, `-O3`, and Link Time Optimization (`-flto`) for the production library target `sigill_emulator.so`.
    *   Designed and implemented the **Option B (Experimental)** Dynamic Code Patching (Trap-and-Patch) engine.
    *   To overcome x86-64 32-bit relative jump limit constraints (`+/- 2GB`), implemented a **Trampoline Island Allocator** (`allocate_trampoline_island`) that scans and dynamically allocates executable pages via `mmap` near the call site.
    *   Wrote 16-byte register-preserving JIT jumps:
        ```assembly
        push %rax
        movabs $target, %rax
        xchg %rax, (%rsp)
        ret
        ```
    *   Overwrote trapped instruction bytes at `RIP` using atomic signal traps and relative `call` instructions targeting the island stub, bypassing hardware interrupts on subsequent runs.
*   **Code Reference**: [src/sigill_emulator.c](../src/sigill_emulator.c#L449-L506) (`allocate_trampoline_island` and `resolve_mem_addr_fast_trampoline`), [src/sigill_emulator.c](../src/sigill_emulator.c#L636-L690) (`patch_code`).

### 2. Multi-Architecture Dynamic Builds, Safety Switches & Interactive Help
*   **Commit Hash**: `2f74386`
*   **Engineering Changes**:
    *   Configured distinct target builds inside the `Makefile` to output:
        *   `sigill_emulator_v1.so` targeting basic legacy `x86-64` (circa 2003) via `-march=x86-64`.
        *   `sigill_emulator_v2.so` targeting `x86-64-v2` (circa 2009) via `-march=x86-64-v2` for processors featuring SSE4.1/4.2.
    *   Implemented a runtime mode toggle via environment variables: defaults to **Safe Mode (Option A)**, switching to **Experimental Mode (Option B)** only when `EMU_MODE=experimental` or `EMU_EXPERIMENTAL=1` is supplied.
    *   Wrote an argument and environment parser (`check_help`) that inspects `/proc/self/cmdline` and `EMU_HELP`/`HELP` env variables to display a comprehensive CLI usage manual before exiting cleanly with status `0`.
*   **Code Reference**: [src/sigill_emulator.c](../src/sigill_emulator.c#L1031-L1086) (`check_help`).

### 3. Documentation Restructuring & Relative Link Portability
*   **Commit Hash**: `b2ecc44`
*   **Engineering Changes**:
    *   Moved documentation resources (`LEARNING_GUIDE.md`, `SKILL.md`, `AGENTS.md`, `benchmark_results.md`, `session_log.md`) to a unified `docs/` subdirectory.
    *   Cleaned all absolute local references (`file:///home/<user>/...` paths) in the documentation and replaced them with portable relative links to allow the repo to be cloned and read across any engineering workspace.

### 4. Compiler Performance Profiling
*   **Commit Hash**: `d0c9cc5`
*   **Engineering Changes**:
    *   Updated the benchmark results file to include a comparative latency budget table testing the impact of `-O2` vs `-O3 -flto -march=native`.
    *   Proven that Option A is entirely bound by the kernel's hardware interrupt context switch cost (~1,300 ns), whereas Option B's user-space overhead is reduced by **35%** (latency dropped from `174 ns` to `111 ns` per instruction) when compiled with optimized flags.

### 5. Memory Operand Parsing Cache (Option A)
*   **Commit Hash**: `6c80c7e`
*   **Engineering Changes**:
    *   Refactored the parser for memory displacement calculations inside Safe Mode.
    *   Previously, the ModRM and SIB bytes had to be decoded and evaluated on every signal trap, which involved slow conditional testing.
    *   Upgraded the signal handler to pre-parse addressing mode elements (RIP relative state, base/index register mappings, scale, and displacements) on the first cache miss (via `parse_mem_operand`).
    *   Cached these pre-parsed properties directly in the direct-mapped `cache_entry` table.
    *   On cache hits, `resolve_mem_addr_fast` resolves the memory address inline using cached values, saving **~100 ns** of CPU execution time per trap.
*   **Code Reference**: [src/sigill_emulator.c](../src/sigill_emulator.c#L327-L443) (`cache_entry` structure, `parse_mem_operand`, and `resolve_mem_addr_fast`).

---

## 🌟 Multi-Target Automated Orchestrator Benchmarking

*   **Engineering Changes**:
    *   Re-engineered the test utility [benchmark/benchmark.c](../benchmark/benchmark.c).
    *   Previously, running benchmarks required manual preloading of individual targets.
    *   Upgraded the benchmark to act as a **Parent Orchestrator** which automatically forks, sets appropriate preloads and modes in the environment, runs child subprocesses of itself, and parses their timing results.
    *   Runs 7 test configurations including an un-preloaded Baseline (confirming the expected `SIGILL` crash), and both Safe (Option A) and Experimental (Option B) modes for `v1`, `v2`, and `Native` builds.
    *   Presents a beautifully formatted ASCII comparison table compiling elapsed time, traps per second, and avg latency directly to the developer at run time.
*   **Code Reference**: [benchmark/benchmark.c](../benchmark/benchmark.c)

---

## 🌟 Security Analysis: SELinux & AppArmor Compliance

*   **Context**: Explored options to make Option B (Experimental JIT Mode) run under strict security configurations (W^X-enforced sandboxes).
*   **Engineering Verification**:
    1.  **`/proc/self/mem` writing**: Verified that writing directly to memory through `/proc/self/mem` successfully patches read-only executable instructions without calling `mprotect` or triggering MMU-level page table transitions.
    2.  **`memfd_create` Dual-Mapping**: Verified that JIT trampoline execution is possible by dual-mapping an anonymous shared memory descriptor once as writable (`PROT_READ | PROT_WRITE`) and once as executable (`PROT_READ | PROT_EXEC`).
*   **Hardening Realities**:
    *   **AppArmor** profiles in production (Docker, Kubernetes, systemd) explicitly block process memory writing (`deny @{PROC}/[0-9]*/mem rwklx,`), which instantly fails `/proc/self/mem` writes with `EACCES`.
    *   **SELinux** policies block execution of anonymous memory mapped with `PROT_EXEC` (the `execmem` boolean set to `false`), preventing dual-mapped mappings from executing unless they are backed by a labeled binary on disk.
*   **Design Decision**:
    *   Confirmed that Option A (Safe Mode) remains the **only secure, architecturally compliant path** for strict sandboxes since it uses standard library files loaded by `ld.so` and relies purely on kernel-delivered `SIGILL` signals without runtime code modification.

---

## 🌟 Fallback Architecture: Graceful Degradation

*   **Hierarchy Design**:
    *   Implemented a unidirectional fallback pipeline ($\text{Option B} \rightarrow \text{Option A}$). 
    *   If Option B fails (e.g., due to memory constraints, permission denials, or allocator failures), it aborts patching at that specific instruction location and drops back to Option A (pure software emulation).
    *   Prevents cyclic loops and ensures failsafe execution on hardened hosts.
*   **Granularity**:
    *   Dynamic code patching operates at per-instruction resolution. A patching failure at a specific site does not affect other successfully patched sites.
*   **Resource Immunity**:
    *   Option A acts as a bulletproof floor because it relies on static memory allocation (~56 KB) and zero runtime dynamic allocations. Cache line contentions are handled via direct eviction rather than out-of-memory errors, rendering Safe Mode immune to system resource limits.

---

## 🌟 Resilient Upgrades & Stack Alignment Hardening (Commit v1.0.19)

Below is the log of the latest engineering sprint focusing on binary upgrade resilience and low-level JIT execution hardening.

### 1. Robust Signature-Based Auto-Patcher (`patch_agy.py`)
*   **Engineering Changes**:
    *   Discovered that newer versions of `agy` (such as the 177M version 1.0.4) compiled with modern compilers bypass simple struct-based checks and instead inline fail-fast checks into runtime and package initialize routines.
    *   Designed and implemented a state-of-the-art **Byte-Signature Matcher** inside `scripts/patch_agy.py` (copied to `~/patch_agy.py` on install).
    *   The script uses a byte regular expression to dynamically locate the prologue of Go's `cpu.Initialize` function:
        `\x55\x48\x89\xe5\x53\x50\xe8.{4}\x8b\x05.{4}\xa9\x00\x00\x04\x00\x0f\x84`
    *   Once found, it dynamically scans forward to locate the function's epilogue (`\x48\x83\xc4\x08\x5b\x5d\xc3`) and computes the relative target address.
    *   Automatically writes the relative JMP instruction (`\xE9 [32-bit offset] \x90`) at the capability register-load instruction, ensuring 100% resilience against compiler address offsets or binary upgrades.

### 2. JIT Trampoline Stack Alignment Hardening (Option B)
*   **Engineering Changes**:
    *   Identified and diagnosed a Segmentation Fault (Exit Code 139) occurring during Option B (Experimental JIT Mode) hot-loop execution.
    *   By executing under GDB with `handle SIGILL pass noprint nostop`, caught a General Protection Fault (`SIGSEGV`) inside `emulate_aesdec` caused by an unaligned `movaps %xmm0, 0x30(%rsp)` instruction.
    *   **The Root Cause**: Go's runtime executes with non-standard 8-byte stack alignment instead of the 16-byte alignment mandated by the System V ABI. When the JIT trampoline stub was called, the stack alignment mismatch carried over into our compiled C library.
    *   **The Surgical Solution**: Hardened the C callback function `trampoline_c_helper` inside `src/sigill_emulator.c` using the GCC frame-alignment attribute:
        `__attribute__((force_align_arg_pointer))`
    *   This forces the GCC/Clang compiler to dynamically align the stack pointer `RSP` to a 16-byte boundary (`and $-16, %rsp`) in the prologue of the helper callback, ensuring all emulated vector operations are perfectly aligned and execute with absolute stability.

---

## 🌟 Auto-Update Integration & Patcher Hardening (Commit v1.0.20)

This sprint addressed a gap identified after `agy` self-updated on June 4, 2026: the new binary silently replaced `agy.real` with an unpatched version, requiring manual re-patching.

### 1. Multi-Signature Patcher (`patch_agy.py`)
*   **Problem**: The single primary signature `\x55\x48\x89\xe5\x53\x50\xe8.{4}\x8b\x05.{4}\xa9\x00\x00\x04\x00\x0f\x84` works for all known builds but provides no resilience if the Go compiler changes the prologue structure or cpu feature bit layout in a future version.
*   **Solution**:
    *   Added 4 signature variants tried in priority order: `primary-aes-bit18` (original), `generic-feature-check` (wildcards TEST mask — survives future Go bit reassignment), `no-push-rax` (shorter frame variant), `test-al-byte-check` (compact `TEST AL,imm8` form for pclmulqdq-style checks).
    *   Added 5 epilogue variants: `add rsp,8/16/24` + pop/ret combinations. The function epilogue has been `4883c4085b5dc3` across all 4 tested versions (May 2026–Jun 2026) but the patcher no longer hard-depends on it.
    *   Added explicit **already-patched detection** before signature scanning: searches for `\xe9.{4}\x90` (our own JMP+NOP fingerprint) in the expected position. Exits `0` cleanly without re-reading the binary on subsequent launches.
    *   Added binary fingerprinting in log output (sha256, size, mtime) for future diagnostics.
    *   Exit codes: `0` = patched or already patched, `1` = incompatible binary. This allows callers to distinguish genuine failure from success.
    *   **Generic**: the patcher works on any compatible Go binary — not tied to `agy` specifically.
*   **Cross-version pattern analysis** (confirming signature stability):
    | Version | Sig offset | jmp_site | Epilogue distance |
    |---|---|---|---|
    | May 22 | `0x74B9180` | `+11` | `0xB3` (179 bytes) |
    | May 24 | `0x74D0F00` | `+11` | `0xB3` |
    | Jun 2  | `0x75E9D20` | `+11` | `0xB3` |
    | Jun 4  | `0x766A3C0` | `+11` | `0xB3` |

### 2. Auto-Update Wrapper (`~/.local/bin/agy`)
*   **Problem**: When `agy` self-updates, it replaces `agy.real` in place. The wrapper only set `LD_PRELOAD`; it had no mechanism to re-apply Track A after an update.
*   **Solution**: Updated the wrapper to detect updates via a marker file (`~/.local/bin/.agy.real.patched`). On each launch, it compares the `agy.real` mtime against the marker. If `agy.real` is newer (self-update detected), it runs `patch_agy.py` automatically and refreshes the marker. Patch stdout is redirected to `/tmp/agy_patch.log`.
*   **Idempotent**: if the binary is already patched (patcher exits 0 via already-patched fast-path), the marker is still touched — no wasted work on subsequent launches.
*   Both tracks now run automatically: Track A on update detection, Track B (LD_PRELOAD) on every launch.



