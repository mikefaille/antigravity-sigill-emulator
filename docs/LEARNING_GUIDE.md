# Under the Hood: A Systems Programming Learning Guide

Welcome, fellow developer! This guide is designed to teach you the fundamental concepts of low-level [systems programming](https://en.wikipedia.org/wiki/Systems_programming) behind this toolkit. 

To ensure this guide is fully accessible, **we assume zero prior knowledge** of CPU internals or operating system architectures. We will define every key concept from scratch, and we provide [Wikipedia](https://www.wikipedia.org/) links for complementary reading.

---

## 🗺️ High-Level Architecture Overview
A computer program is ultimately a sequence of instructions stored in a binary file. When executed, the [central processing unit (CPU)](https://en.wikipedia.org/wiki/Central_processing_unit) decodes and runs these instructions one by one.

If a binary is compiled to use modern instructions (like [AES-NI](https://en.wikipedia.org/wiki/AES_instruction_set) for encryption or [PCLMULQDQ](https://en.wikipedia.org/wiki/CLMUL_instruction_set) for carry-less multiplication) but is run on a legacy CPU that physically lacks those circuits, the program crashes in two distinct ways — and we need a different fix for each one.

**Crash type 1 — Startup gate:** Go binaries check for required CPU features before doing any real work. If the feature is missing they print a fatal error and exit immediately, before any of the AES instructions are even reached.

**Crash type 2 — Mid-execution illegal instruction:** If the startup gate is bypassed, the program runs fine until it executes an actual AES or PCLMULQDQ instruction on the unsupported CPU. The CPU raises a hardware exception and the OS kills the process.

Our toolkit solves both problems with two complementary tracks:

```text
BEFORE LAUNCH — Track A: Static Patcher (patch_agy.py)
+-------------------------------------------------------------+
|  patch_agy.py reads the binary file and scans for the       |
|  Go CPUID gate (a byte signature in the cpu.Initialize       |
|  function). It overwrites the conditional branch with an     |
|  unconditional JMP past the check, so the gate is never      |
|  evaluated. The fix survives reboots — it lives on disk.     |
|                                                              |
|  The agy wrapper re-runs this automatically every time       |
|  the binary is replaced by a self-update.                    |
+-------------------------------------------------------------+
                         |
                         v (binary now starts without aborting)

AT RUNTIME — Track B: Signal Emulator (sigill_emulator.so)
+-------------------------------------------------------------+
|  1. agy wrapper sets LD_PRELOAD and launches the binary      |
|  2. OS loader injects sigill_emulator.so before main()       |
|  3. Constructor registers our SIGILL signal handler          |
+-------------------------------------------------------------+
                         |
                         v
+-------------------------------------------------------------+
|  4. CPU hits an AES instruction it cannot execute            |
|  5. CPU raises a hardware illegal-instruction exception      |
|  6. Kernel delivers SIGILL to our registered handler         |
+-------------------------------------------------------------+
                         |
                         v
+-------------------------------------------------------------+
|  7. Handler reads the opcode bytes at RIP                    |
|  8. Emulates the AES math in software                        |
|  9. Writes the result into the saved register context        |
| 10. Advances RIP past the instruction                        |
| 11. Returns — CPU resumes the program at the next line       |
+-------------------------------------------------------------+
```

---

## 🧠 Core Concept 1: Static Binary Patching (Track A)

Before we can even inject our runtime emulator, we have to deal with the startup gate. Go's runtime calls a function called `cpu.Initialize` very early in the program's life. That function checks whether the CPU supports the features the binary was compiled for, and if not, prints a fatal message and calls `exit()` — before `main()` even runs.

### What does the gate look like in machine code?

At the machine-code level, the check compiles down to something like this sequence of bytes:

```text
call  cpu.Initialize        ; ask the CPU what it supports
mov   eax, [cpu_features]   ; load the result into register EAX
test  eax, 0x40000          ; check if bit 18 (AES) is set
je    <epilogue>            ; if NOT set → jump to the exit path (crash)
```

The bytes for `mov eax, [cpu_features]` look like `8B 05 ?? ?? ?? ??` in the binary file (6 bytes). That is exactly what we overwrite.

### The patch: turn a conditional check into an unconditional skip

We replace those 6 bytes with a 5-byte unconditional `JMP` instruction plus one padding `NOP` byte:

```text
E9 [4-byte relative offset] 90
```

The `E9` opcode means "jump to this relative address". We point it directly at the function's normal return code — the epilogue. The CPU never evaluates the feature check at all; it jumps straight to the exit of `cpu.Initialize` as if everything was fine.

### How does patch_agy.py find the right bytes?

The patcher uses a **byte-signature** — a pattern of known bytes with wildcards for the parts that change between binary versions:

```
55 48 89 E5 53 50     <- function prologue: push rbp, set up frame, push registers
E8 ?? ?? ?? ??        <- call cpu.Initialize (target address changes each build)
8B 05 ?? ?? ?? ??     <- MOV EAX,[cpu_features]  ← we overwrite this
A9 00 00 04 00        <- TEST EAX, 0x40000
0F 84                 <- JE (jump-if-equal)
```

The `??` bytes are wildcards. The patcher also scans forward for the function's epilogue (`add rsp,8; pop rbx; pop rbp; ret`) to compute the exact relative offset for the jump. This means the patch works even when the binary is recompiled and all the addresses change — only the surrounding byte pattern needs to stay the same, and it has been stable across every observed build.

### How does the patcher know if the binary was already patched?

After writing the patch, the 6 bytes at that position look like `E9 [offset] 90` instead of `8B 05 ...`. On the next run the patcher scans for this JMP fingerprint before trying to patch again. If it finds it, it exits immediately without touching the file, making the operation safe to run repeatedly.

### Why does the wrapper re-run the patcher automatically?

When the Antigravity IDE updates itself, it downloads a fresh binary and replaces the old one on disk. The new binary has the original CPUID gate again. The `agy` wrapper script detects this using a marker file — it compares the modification time of `agy.real` against a `.agy.real.patched` timestamp file. If the binary is newer than the marker, the patcher runs before launch, then the marker is refreshed. This whole cycle is transparent: the user just types `agy` and it works, regardless of when the IDE last updated.

---

## 🧠 Core Concept 2: Dynamic Library Loading (`LD_PRELOAD`)


Before a program runs, it must be loaded into the computer's [random-access memory (RAM)](https://en.wikipedia.org/wiki/Random-access_memory). 

### Statically vs. Dynamically Linked Programs
*   **Static Linking**: All dependency code is bundled directly into the single executable file.
*   **Dynamic Linking**: The executable contains references to external files called **shared libraries** (e.g. `.so` files on Linux, `.dll` on Windows). 

When a dynamically linked program starts, the [operating system loader](https://en.wikipedia.org/wiki/Loader_(computing)) runs a special helper program called the [dynamic linker](https://en.wikipedia.org/wiki/Dynamic_linker) (on Linux, usually `ld-linux.so`). This linker searches for the required shared libraries, loads them into memory, and resolves the function addresses so the program can call them.

### What is `LD_PRELOAD`?
The dynamic linker supports an override mechanism called [preloading](https://en.wikipedia.org/wiki/Dynamic_linker#Dynamic_linking_in_Unix-like_systems). By setting the `LD_PRELOAD` [environment variable](https://en.wikipedia.org/wiki/Environment_variable) to point to our library (`sigill_emulator.so`), we tell the dynamic linker: 
*"Load our library first, before any standard system library, and override any matching function symbols."*

In [sigill_emulator.c](../src/sigill_emulator.c), we define a constructor:
```c
__attribute__((constructor))
static void init(void) {
    // This code runs when the shared library is loaded
    // BEFORE the main() function of the program executes!
    ...
}
```
The `__attribute__((constructor))` is a compiler directive. It registers the function in the Executable and Linkable Format ([ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)) binary's `.init_array` section. When the OS loads the library, it executes all functions in `.init_array` first.

---

## 🧠 Core Concept 3: Hardware Exceptions & POSIX Signals

CPUs read instructions in the form of binary byte sequences called [opcodes](https://en.wikipedia.org/wiki/Opcode). If the CPU reads an opcode that does not exist in its instruction set architecture (ISA), it cannot proceed.

### The Trap-and-Emulate Sequence:
1.  **Hardware Exception**: The CPU halts the running thread and triggers an [illegal instruction exception](https://en.wikipedia.org/wiki/Illegal_instruction) (a type of hardware [interrupt](https://en.wikipedia.org/wiki/Interrupt)).
2.  **Privilege Transition**: The CPU switches from [User Mode to Kernel Mode](https://en.wikipedia.org/wiki/User_space_and_kernel_space) (giving the OS full control over hardware) and invokes the operating system's [interrupt handler](https://en.wikipedia.org/wiki/Interrupt_handler).
3.  **Signal Delivery**: The Linux kernel translates this hardware interrupt into a user-space [POSIX signal](https://en.wikipedia.org/wiki/Signal_(IPC)) called `SIGILL` (Signal Illegal Instruction).
4.  **Signal Handler Redirect**: Normally, `SIGILL` terminates the program ("Illegal instruction (core dumped)"). However, since our preloaded library registered a custom [signal handler](https://en.wikipedia.org/wiki/Signal_(IPC)#Handling_signals) using the `sigaction` system call, the kernel switches the thread back to User Mode and executes our handler function.

---

## 🧠 Core Concept 4: Thread Context Manipulation (`ucontext_t`)

A CPU uses ultra-fast internal storage units called [processor registers](https://en.wikipedia.org/wiki/Processor_register) to hold temporary values, memory addresses, and execution states. 

When a signal interrupts a thread, the operating system must save the values of all registers (a [register context](https://en.wikipedia.org/wiki/Context_(computing))) so that execution can resume later as if nothing happened. The OS passes this snapshot to our handler as the third argument:
```c
static void handler(int sig, siginfo_t *si, void *ctx_void)
```
*   `ctx_void`: A pointer to a structure (`ucontext_t`) representing the saved CPU registers.

### 1. Reading the Instruction Pointer (`RIP`)
The most important register is the [instruction pointer / program counter (RIP)](https://en.wikipedia.org/wiki/Program_counter). It contains the memory address of the instruction currently executing.
```c
uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
```
By inspecting the bytes starting at the memory address pointed to by `rip` (`rip[0]`, `rip[1]`, etc.), our code can look at the raw [instruction bytes](https://en.wikipedia.org/wiki/X86_instruction_listings) to determine exactly which unsupported instruction failed (e.g. `0x0F 0x38 0xDC` for `AESENC`).

### 2. Modifying Registers & Skipping the Instruction
To emulate the instruction, we:
1.  Parse the [ModR/M byte](https://en.wikipedia.org/wiki/ModR/M) (a byte following the opcode that specifies which registers or memory locations are the operands).
2.  Perform the corresponding math in software.
3.  Write the resulting value back into the saved register context (like the 128-bit vector [XMM registers](https://en.wikipedia.org/wiki/Streaming_SIMD_Extensions) used for SSE/AES calculations):
    ```c
    uint8_t *dest = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
    ```
4.  **Crucial Step**: If we return now, the CPU will try to execute the exact same instruction at `RIP` again, leading to an infinite crash loop. We must manually advance `RIP` in the saved context record past the instruction length:
    ```c
    uc->uc_mcontext.gregs[REG_RIP] += bytes_consumed;
    ```
When our handler returns, the operating system restores the modified context, and the CPU resumes executing the program smoothly at the next instruction!

### 3. Upgraded Concept: Register-to-Memory Address Decoding
Not all instructions operate in register-to-register mode (`mod == 3`). In more advanced cases, instructions load and process round keys directly from read-only data segments (`.rodata`) in memory, e.g. using RIP-relative addressing:
```text
pclmulqdq xmm0, xmmword ptr [rip - 0x29ce8a6], 0x10
```
To support these memory operands, the emulator parses the **ModRM byte**, optional **SIB byte**, and **disp8/disp32 displacements** to calculate the operand's effective memory address:
1.  **RIP-Relative:** `Effective Address = RIP_next + disp32`
2.  **Base + Index + Scale + Disp:** `Effective Address = base_val + (index_val << scale) + disp`

We fetch the corresponding register values from `uc->uc_mcontext.gregs` and retrieve the memory bytes at the computed address directly, allowing flawless emulation of memory-operand instructions.

---

## 🧠 Core Concept 5: Finite Field Math & Branchless Optimization

The Advanced Encryption Standard (AES) relies on mathematical operations in a [finite field / Galois Field](https://en.wikipedia.org/wiki/Finite_field) (specifically $\text{GF}(2^8)$). In this mathematical system, addition is represented by bitwise [exclusive OR (XOR)](https://en.wikipedia.org/wiki/Exclusive_or) operations, and multiplication is defined modulo an irreducible polynomial ($x^8 + x^4 + x^3 + x + 1$, or hex `0x1B`).

### The Traditional Approach (Looping Multiplication)
Traditionally, multiplying two numbers in $\text{GF}(2^8)$ requires a loop that iterates 8 times (once for each bit), checking conditions and performing shifts:
```c
static inline uint8_t gmul(uint8_t x, uint8_t y) {
    uint8_t p = 0;
    for (int i = 0; i < 8; i++) {
        if (y & 1) p ^= x;
        uint8_t carry = x & 0x80;
        x <<= 1;
        if (carry) x ^= 0x1B;
        y >>= 1;
    }
    return p;
}
```
While simple, this loop introduces multiple **conditional branches**. In modern pipelined CPUs, branch mispredictions cause CPU stalls, adding latency. During deep decryption operations (which execute `inv_mix_columns` calling multiplication 64 times per block), this introduces massive CPU overhead.

### The Branchless Optimization
Because the decryption steps multiply solely by fixed constants ($9$, $11$, $13$, and $14$), we can pre-factor the shifts and make the operations entirely **branchless**:

1.  **Branchless doubling (`gmul2`)**:
    We perform the check for the carry bit using arithmetic bit shifts instead of a branch. If the high bit of `x` is set, `(int8_t)x >> 7` evaluates to `0xFF` (all bits 1), and `0xFF & 0x1B` yields `0x1B`. If clear, it evaluates to `0x00`:
    ```c
    static inline uint8_t gmul2(uint8_t x) {
        return (x << 1) ^ (((int8_t)x >> 7) & 0x1B);
    }
    ```
2.  **Power-of-Two double chains**:
    We define multiplying by 4 and 8 as cascading inline calls to `gmul2`:
    ```c
    static inline uint8_t gmul4(uint8_t x) { return gmul2(gmul2(x)); }
    static inline uint8_t gmul8(uint8_t x) { return gmul2(gmul4(x)); }
    ```
3.  **Constant reconstruction**:
    Since multiplication distributes over XOR addition in finite fields, we reconstruct multiplication by the constants 9, 11, 13, and 14 using simple XOR operations on our power-of-two helper functions:
    *   $\text{gmul9}(x) = (x \cdot 8) \oplus x$
    *   $\text{gmul11}(x) = (x \cdot 8) \oplus (x \cdot 2) \oplus x$
    *   $\text{gmul13}(x) = (x \cdot 8) \oplus (x \cdot 4) \oplus x$
    *   $\text{gmul14}(x) = (x \cdot 8) \oplus (x \cdot 4) \oplus (x \cdot 2)$

This optimization removes all loop iterations and conditional branches from our hot math paths, reducing CPU cycle consumption.

When our handler returns, the OS restores the context, and the CPU resumes executing the program at the updated `RIP` address.

---

## 🛠️ Step-by-Step: Compiling the Emulator Shared Library

Before modifying the emulator, it is crucial to understand how we compile a raw C source file into a dynamic library that can be injected via `LD_PRELOAD`.

### The Compilation Command
To optimize for different hardware specifications, compile targeting the appropriate architecture:

*   **Native Optimization (For the Host CPU):**
    ```bash
    gcc -shared -fPIC -O3 -march=native -flto -Wall -o sigill_emulator.so src/sigill_emulator.c
    ```
*   **v1 Build (x86-64 Legacy, circa 2003):**
    ```bash
    gcc -shared -fPIC -O3 -march=x86-64 -flto -Wall -o sigill_emulator_v1.so src/sigill_emulator.c
    ```
*   **v2 Build (x86-64-v2, circa 2009):**
    ```bash
    gcc -shared -fPIC -O3 -march=x86-64-v2 -flto -Wall -o sigill_emulator_v2.so src/sigill_emulator.c
    ```

### Compiler Flags Explained:
*   **`-shared`**: Tells the compiler to produce a shared object (`.so`) library instead of a standard executable binary (which expects a `main` function).
*   **`-fPIC`**: Generates **Position Independent Code**. Shared libraries are loaded into arbitrary address locations in a process's memory space at runtime. `-fPIC` ensures the machine instructions use relative offsets (RIP-relative addressing) instead of absolute memory locations.
*   **`-O3`**: Activates Level 3 optimization. It enables loop unrolling, vectorization (using supported SSE vector extensions), and more aggressive code optimizations compared to `-O2` to minimize instruction translation latencies.
*   **`-march=native|x86-64|x86-64-v2`**: Selects the target CPU instruction set. `x86-64` targets the base 2003 AMD64 specification. `x86-64-v2` adds support for SSE3, SSSE3, SSE4.1, SSE4.2, and POPCNT, which aligns with Xeon Nehalem/Westmere architectures. `native` autodetects and targets all instruction sets supported by the machine running the compiler.
*   **`-flto`**: Activates **Link-Time Optimization**. LTO passes the compiler intermediate representation bytes so it can optimize and inline helper code (such as math functions and cache checks) across translation units at link time.
*   **`-Wall`**: Enables all compiler warning messages to ensure code safety.

### 🚨 Troubleshooting: Atomic Installation and `LD_PRELOAD` Race Conditions

If you copy the newly compiled shared library directly over the existing path where a running process (like `agy`) has preloaded it using `LD_PRELOAD`:
```bash
# DO NOT DO THIS directly on an active preload target!
cp sigill_emulator.so ~/sigill_emulator.so
```
You will encounter this linker error:
```text
ERROR: ld.so: object '~/sigill_emulator.so' from LD_PRELOAD cannot be preloaded (cannot open shared object file): ignored.
```

#### Why does this happen?
When you use `cp`, it opens the destination file and truncates it to 0 bytes, then writes the new content block-by-block. If any new subprocess (such as dynamic Go runtime threads) tries to start during this write window, the dynamic linker (`ld.so`) attempts to read the library, finds it incomplete or truncated (0 bytes), fails to load it, and prints the error message.

#### The Solution: Atomic Installation
To prevent this race condition, you must replace the shared library **atomically** using the filesystem's rename operation. Renaming changes the directory entry pointing to the file inode in a single, atomic operation, ensuring that any process attempting to load it will either get the complete old version or the complete new version:
```bash
# Correct atomic sequence:
cp sigill_emulator.so ~/sigill_emulator.so.tmp
mv ~/sigill_emulator.so.tmp ~/sigill_emulator.so
```
This is fully automated in the project's [Makefile](../Makefile) under the `install` target.

---

## 🧠 Advanced Concept: Lock-Free Metadata Caching (`Seqlock`)

When running highly multi-threaded processes (like the Go runtime which spawns dozens of concurrent OS threads), multiple threads can trigger `SIGILL` traps simultaneously. To maximize performance, we must avoid re-decoding instruction bytes (prefixes, REX, ModRM, SIB) on every single trap.

However, since signal handlers are invoked asynchronously on arbitrary threads, standard thread synchronization mechanisms like mutexes (`pthread_mutex_t`) **are not async-signal-safe** and can cause deadlocks if a signal interrupts a thread that already holds the lock.

### The Lock-Free Direct-Mapped Cache
To solve this, we implement a direct-mapped cache table indexed by a hash of the instruction pointer:
```c
#define CACHE_SIZE 1024
static struct cache_entry rip_cache[CACHE_SIZE];

static inline uint32_t get_cache_index(uint8_t *rip) {
    return ((uintptr_t)rip >> 2) & (CACHE_SIZE - 1);
}
```

### The Sequence Lock (Seqlock) Pattern
To guarantee consistency without locks when multiple threads read/write the same cache entry, we use a **Sequence Lock (Seqlock)** pattern:
1. Each entry contains a `seq` counter initialized to `0`.
2. **Writer**: Before writing a cache entry, the writer atomically increments `seq` to an odd number. After writing the fields, the writer atomically increments `seq` again to an even number.
3. **Reader**: The reader reads the `seq` counter, reads the cached values, and then reads the `seq` counter again. 
   - If `seq` is odd (indicating a write is in progress), or if `seq` changed between the two reads (indicating a write occurred during the read), the reader discards the read data and retries.
   - Otherwise, the data is guaranteed to be consistent!

This seqlock pattern is 100% async-signal-safe, lock-free, and ensures we can safely share cache entries across all Go threads with zero contention.

## 🧠 Advanced Concept: Overcoming the x86-64 2GB relative call limit (Trampoline Islands)

To bypass the hardware context-switching overhead (~1,000 ns) of `SIGILL` traps entirely, the emulator dynamically patches the executable code at runtime. It replaces the unsupported instruction with a `call` to a user-space emulation trampoline.

However, x86-64 direct jumps and calls (`call rel32` / opcode `E8`) are restricted to a **signed 32-bit offset**, meaning the target must be within **+/- 2GB** of the calling instruction. Because dynamically preloaded libraries (`LD_PRELOAD` objects) are loaded in the high memory area (near `0x7f...`) and the main executable runs in the low memory area (near `0x55...`), the distance between them is usually **40+ Terabytes**, which overflows the 32-bit relative range.

### The Solution: Trampoline Islands (Split-Jumps)
To overcome this limitation, we implement a **Trampoline Island** architecture:

1. **Nearby Page Allocation**: During the first trap, the emulator scans the address space within +/- 1GB of the trapped `RIP` in 16MB increments and calls `mmap` to allocate an executable page (`island_page`) close to the executable. Because this page is within 2GB, relative offsets to it fit in 32 bits.
2. **Intermediate stubs**: For each patched instruction, we write a 16-byte absolute jump stub into the island page:
   ```assembly
   push %rax             ; Save original RAX
   movabs $target, %rax  ; Load 64-bit absolute address of trampoline_entry
   xchg %rax, (%rsp)     ; Swap RAX with target on stack, restoring RAX
   ret                   ; Pop target into RIP and jump
   ```
   This stub uses no registers (preserving RAX perfectly) and jumps to the final trampoline in the shared library.
3. **Atomic Overwrite**: We overwrite the original instruction with a relative `call` pointing to our 16-byte stub. To ensure multi-threaded safety during the write, we atomically write `0xCC` (INT3) to the first byte, write the relative offset and NOP padding, and then atomically replace `0xCC` with `0xE8` (call).
4. **Lock-Free SIGTRAP Spin-Retry**: Any concurrent thread executing the instruction during the write hits `0xCC` and raises `SIGTRAP`. Our registered `SIGTRAP` handler simply returns immediately, causing the CPU to retry the instruction. Once the write completes, the instruction becomes `0xE8` and the threads call the trampoline natively without traps!

This combination of split-jumps, atomic swaps, and lock-free trap retries yields a **10x overall benchmark speedup** (averaging ~170 ns per trap) and completely eliminates kernel mode context switching on all subsequent instruction executions.

---

## 🧠 Advanced Concept: W^X Compliance and Mandatory Access Control (SELinux & AppArmor)

When deploying compatibility tools in production, they are subject to operating system and kernel security policies. Dynamic code modification (JIT patching used in Option B) creates security challenges under strict policies.

### 1. Write XOR Execute (W^X) Compliance
W^X is a fundamental security policy enforced at the CPU and MMU level. It dictates that virtual memory pages must be:
- **Writable** (so programs can write data to them), OR
- **Executable** (so the CPU can execute code in them),
- But **never both simultaneously**.

#### How Experimental Mode violates raw W^X:
To patch instructions in-place, Option B changes the target code page from read-executable (`PROT_READ | PROT_EXEC`) to writable (`PROT_READ | PROT_WRITE | PROT_EXEC`), overwrites the instruction, and changes it back. This temporary combination is heavily monitored and blocked by security modules.

#### Secure W^X-Compliant Patching: `/proc/self/mem`
Instead of using `mprotect` to change memory page permissions, code can be patched by opening `/proc/self/mem` and writing directly to the offset matching the instruction's virtual address:
```c
int fd = open("/proc/self/mem", O_RDWR);
pwrite(fd, patch_bytes, size, target_address);
```
Under standard Linux, writing to `/proc/self/mem` bypasses the virtual memory table write protection. The page permissions remain `PROT_READ | PROT_EXEC` in the page table at all times, making this write fully W^X compliant since the writeable bit is never exposed in user-space mappings.

### 2. Mandatory Access Control (MAC) Constraints

While `/proc/self/mem` satisfies hardware-level W^X checks, **strict Mandatory Access Control (MAC)** profiles enforced by SELinux and AppArmor will block it.

#### SELinux constraints:
1. **`execmem` block**: Hardened SELinux profiles set `execmem` to `false` by default. This blocks a process from making any anonymous memory page executable, which neutralizes the JIT Trampoline Island pages allocated by Option B.
2. **`memfd_create` Dual-Mapping**: To bypass `execmem` for JITs, compilers use **Dual-Mapping (Alias Mapping)**. They map an anonymous in-memory file descriptor twice: once as `PROT_READ | PROT_WRITE` (to compile code) and once as `PROT_READ | PROT_EXEC` (to execute code). No page is ever writable and executable simultaneously. However, under strict SELinux rules, mapping any memory-backed file as executable is still blocked if the process lacks the `execmem` privilege.

#### AppArmor constraints:
AppArmor profiles in secure environments (like systemd units, Docker containers, or LXD sandboxes) almost universally deny all access to process memory:
```apparmor
deny @{PROC}/[0-9]*/mem rwklx,
```
This rule intercepts the `open()` call to `/proc/self/mem` and returns a `Permission Denied` (`EACCES`) error, causing any in-place patching attempt to crash.

### 3. Why Safe Mode is the Secure Standard

Because Experimental Mode's JIT compiling and memory-patching techniques are incompatible with strict MAC security, **Safe Mode remains the industry standard for hardened environments**:

*   **No runtime memory modification**: It does not allocate any JIT pages or write to `/proc/self/mem`.
*   **Static Executable Labeling**: The signal handler runs inside the static dynamic library `sigill_emulator.so` loaded by the standard operating system loader (`ld.so`) at startup. The library file is labeled on disk (e.g. as `lib_t`), allowing SELinux to approve its execution.
*   **Pure Exception Trapping**: It relies entirely on standard `SIGILL` signals delivered by the kernel, making it safe for all hardened cloud, sandboxed, and VM workloads.

---

## 🧠 Advanced Concept: Graceful Degradation & Unidirectional Fallback

To prevent security policy or resource execution limits from causing a fatal crash, the emulator implements a **unidirectional fallback and graceful degradation** architecture.

### 1. The Failsafe Hierarchy
The modes are organized hierarchically without cyclic dependency (avoiding infinite loops):

```text
       [Application starts in Option B]
                      |
                      v
            Try JIT Code Patching
                      |
            +---------+---------+
            |                   |
         Succeeds             Fails (e.g. SELinux block, out of memory)
            |                   |
            v                   v
      Direct JIT Execution    Fallback to Option A (Trap-and-Emulate)
                                |
                                v
                           Execute via Software Math
```

If the JIT engine fails at any stage (e.g. unable to allocate a JIT Trampoline Island page, unable to open `/proc/self/mem` due to AppArmor, or failed `mprotect` call), it immediately aborts patching, leaves the code intact, and delegates to the Option A software emulator.

### 2. Per-Instruction Granularity
Rather than aborting the entire process, the fallback operates on a **per-instruction basis**. If the emulator successfully patches 9 out of 10 instructions but fails to patch the 10th (e.g. due to page boundaries or memory locks), the 9 patched sites execute at direct hardware speed, while the 10th site seamlessly degrades to software emulation.

### 3. Safe Mode Immunity to Resource Exhaustion
Safe Mode acts as the absolute floor of the system because it is designed to be **immune to resource exhaustion**:
*   **No Dynamic Memory Allocations**: It never calls `malloc()`, `mmap()`, or `memfd_create()` at runtime.
*   **Static Memory Footprint**: The direct-mapped Seqlock cache table is allocated statically inside the library's data segment at load time, taking a fixed **56 KB** of RAM.
*   **Cache Eviction instead of Out of Memory**: If multiple thread targets collide in the cache index, old metadata entries are overwritten (evicted) using lock-free atomic sequence numbers. There is no failure condition or memory leak associated with the cache filling up.

---

## 💻 Hacking Exercise 1: Add a Dummy Instruction to the Emulator
Let's practice extending the emulator. We will register a dummy instruction that prints a greeting whenever executed, rather than crashing the program.

We will use the byte sequence `0x0F 0x38 0x00` (which is unassigned or triggers SIGILL on most CPUs) as our dummy instruction.

### Step 1: Modify the Signal Handler
Open [sigill_emulator.c](../src/sigill_emulator.c) and find the opcode decoding block in the `handler` function. Add a check for `0x00` under the `0x0f 0x38` prefix:

```c
// Inside handler() in sigill_emulator.c:
if (opcode[0] == 0x0f && opcode[1] == 0x38) {
    if (opcode[2] == 0x00) {
        // Our dummy instruction!
        write(2, "[HACK] Hello from your custom CPU instruction!\n", 48);
        
        // Advance RIP past our instruction (66 0f 38 00 ModRM -> 5 bytes)
        uc->uc_mcontext.gregs[REG_RIP] += (5 + has_rex);
        return;
    }
}
```

### Step 2: Create the Test Program
Create a test file named `test_dummy.c` with the following C code:
```c
#include <stdio.h>

int main() {
    printf("[*] Executing custom dummy instruction...\n");
    
    // Inline assembly injecting the byte sequence for our custom instruction
    // 0x66 = Prefix, 0x0f 0x38 0x00 = Opcode, 0xc0 = ModRM byte (register-to-register)
    __asm__ __volatile__ (
        ".byte 0x66, 0x0f, 0x38, 0x00, 0xc0"
    );
    
    printf("[*] Program execution resumed successfully!\n");
    return 0;
}
```

### Step 3: Compile and Run
1.  **Compile the test program**:
    ```bash
    gcc -o test_dummy test_dummy.c
    ```
2.  **Execute the binary** with the emulator preloaded:
    ```bash
    LD_PRELOAD=./sigill_emulator.so ./test_dummy
    ```
3.  **Verify the Output**:
    You will see the custom greeting from your signal handler printed directly to the terminal, and the test program will terminate cleanly:
    ```text
    [*] Executing custom dummy instruction...
    [HACK] Hello from your custom CPU instruction!
    [*] Program execution resumed successfully!
    ```

---

## 💻 Hacking Exercise 2: Static Binary Patching

Statically modifying a binary file on disk (binary patching) removes a check permanently without needing a runtime signal handler. This exercise teaches you how it works in two ways: first using the automated patcher, then by hand with Rizin so you understand every step.

### The Mechanics of a Jump Patch

When Go compiles the CPU feature check it produces a conditional branch. In simplified assembly it looks like:

```asm
call  cpu.Initialize        ; detect CPU features, store in global
mov   eax, [cpu_features]   ; load the result
test  eax, 0x40000          ; check bit 18 (AES)
je    <exit and crash>      ; if AES is missing → exit
```

To bypass this we replace the `mov eax, [cpu_features]` instruction (6 bytes) with an unconditional jump directly to the function's return code, so the test is never reached.

---

### Part A: Using the Automated Patcher (Recommended)

The automated patcher (`patch_agy.py`) handles all of the address arithmetic and byte-signature searching for you. It is the right tool for everyday use.

```bash
python3 ~/patch_agy.py ~/.local/bin/agy.real
```

A successful run prints the matched signature, the file offset of the patch site, the original bytes, and the patch bytes written. The original binary is saved as `agy.real.bak` before any changes are made. If the binary has already been patched, the tool detects its own JMP fingerprint and exits without touching the file.

**What to look for in the output:**
```text
[+] primary-aes-bit18: match at 0x766A3C0  jmp_site=0x766A3CB  original=8b05cf712902
[+] Epilogue '4883c4085b5dc3' at 0x766A483  (distance: 195 bytes)
[*] Patch: e9b300000090  (JMP rel=0xB3 → epilogue 0x766A483)
[+] Patch applied at 0x766A3CB
```

---

### Part B: Doing it by Hand with Rizin (Deep Dive)

This section walks through every step that the patcher does automatically. Doing it manually builds intuition for binary structure, offset arithmetic, and instruction encoding.

#### Step 1: Find the function

Open the binary in Rizin's read-only mode and search for the `cpu.Initialize` symbol:

```bash
rizin agy.real
```
```text
> is ~cpu.Initialize
```
This prints something like `0x0074d0f0  cpu.Initialize`. That is the function's base address in the binary's virtual address space.

#### Step 2: Disassemble to find the gate

Seek to the function and print 15 instructions:
```text
> s 0x0074d0f0
> pd 15
```

Look for the pattern described above — a `mov eax` loading from a global, immediately followed by a `test` and a conditional `je`. Note the address of the `mov` instruction (the patch site) and the address of the block the `je` would jump to (the epilogue / exit of the function).

#### Step 3: Calculate the relative jump offset

An x86-64 relative `jmp` (`E9`) encodes the distance from the byte *after* the instruction to the target:

```
offset = target_address − (patch_site + 5)
```

Example:
- Patch site: `0x0074d110`
- Epilogue (target): `0x0074d1c9`
- Offset: `0x0074d1c9 − (0x0074d110 + 5) = 0xB4`
- Full patch bytes: `E9 B4 00 00 00 90` (`jmp +0xB4` + one NOP to cover the 6th byte)

#### Step 4: Write the patch

Open the binary in write mode and apply the bytes:
```bash
rizin -w agy.real
```
```text
> s 0x0074d110
> wx e9b400000090
```

Alternatively, let Rizin calculate the offset for you using its assembler:
```text
> wa jmp 0x0074d1c9
> wa nop
```

#### Step 5: Verify

Disassemble the patch site to confirm the `jmp` now points to the epilogue:
```text
> pd 5
> q
```

---

## 📝 Conceptual Review Questions
1. Why does changing `dest[i] ^= key[i]` to use a `tmp` buffer prevent register corruption when `dest == key`?
2. What happens if a signal handler calls a function that isn't "async-signal-safe" (like `printf`), and how did we resolve this in `sigill_emulator.c`?
3. If you compile a native binary on ARM64, does it use x86 AES-NI instructions? Why or why not?
4. Track A patches the binary on disk. Track B intercepts illegal instructions at runtime. Why are both needed — what problem does each one solve that the other cannot?
5. The auto-patcher detects already-patched binaries by looking for its own JMP fingerprint (`E9 ?? ?? ?? ?? 90`) rather than re-running the full byte-signature scan. Why is this faster, and what would happen if a future version of the patcher used a different patch format?
6. The wrapper uses a file modification-time comparison (`agy.real` newer than `.agy.real.patched`) to decide whether to re-patch. What edge cases could cause this check to give the wrong answer, and how would you make it more robust?
