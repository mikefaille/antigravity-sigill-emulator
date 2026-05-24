# Under the Hood: A Systems Programming Learning Guide

Welcome, software engineer! This guide is designed to teach you the low-level systems programming concepts behind this toolkit. By the end of this guide, you will understand exactly how we intercept hardware instructions, how operating system signals work, and how you can manually "hack" and extend this project without needing any AI assistance.

---

## 🗺️ High-Level Architecture Overview
This toolkit bypasses hardware constraints using two core computer science techniques:

1.  **Dynamic Hooking (`LD_PRELOAD`)**: Forcing our own custom C library to load inside the target program before the program even starts.
2.  **Hardware Exception Trapping (`SIGILL`)**: Catching the processor's "I don't understand this instruction" crash signal, executing the math in software, and resuming the program.

```text
+-------------------------------------------------------------+
|                     OPERATING SYSTEM                        |
|                                                             |
|   1. Program Starts  --->  2. LD_PRELOAD Injects Emulator   |
|                                |                            |
|                                v                            |
|                       3. Sigaction Registers SIGILL Hook    |
+-------------------------------------------------------------+
                                 |
                                 v
+-------------------------------------------------------------+
|                     USER PROGRAM RUNS                       |
|                                                             |
|   4. CPU hits unsupported instruction (e.g. aesenc)         |
|   5. CPU raises Hardware Exception!                         |
|   6. Kernel catches exception -> Dispatches SIGILL signal  |
+-------------------------------------------------------------+
                                 |
                                 v
+-------------------------------------------------------------+
|                       EMULATOR HOOK                         |
|                                                             |
|   7. Reads opcode bytes at RIP                              |
|   8. Emulates AES math in software                          |
|   9. Modifies target register context (ucontext_t)          |
|  10. Advances RIP past instruction                          |
|  11. Returns -> CPU resumes program smoothly                |
+-------------------------------------------------------------+
```

---

## 🧠 Core Concept 1: Dynamic Library Loading (`LD_PRELOAD`)
When you run a program in Linux (like `./bin`), the operating system loads the executable file into memory, looks at its dependencies (like `libc.so`), and uses a program called the **dynamic linker** (`ld.so`) to load and link those shared libraries.

### The Hook: `LD_PRELOAD`
`LD_PRELOAD` is an environment variable that tells the dynamic linker: *"Load this specific library first, before any other libraries, and override any matching symbols."*

In [sigill_emulator.c](file:///home/michael/src/agy-compat-toolkit/sigill_emulator.c), we define a constructor:
```c
__attribute__((constructor))
static void init(void) {
    // This code runs when the shared library is loaded
    // BEFORE the main() function of the program executes!
    ...
}
```
The `__attribute__((constructor))` is a GCC compiler directive. It registers the function in the ELF binary's `.init_array` section. When the OS loads the library, it executes all functions in `.init_array` first.

---

## 🧠 Core Concept 2: Hardware Exceptions & POSIX Signals
When the CPU decoder encounters a byte sequence it cannot execute (e.g., an `aesenc` instruction on an old Intel CPU lacking the AES-NI extension), the CPU hardware triggers an **invalid opcode exception** (fault).

1.  The CPU halts the execution of the thread immediately.
2.  The CPU switches from User Mode to Kernel Mode and runs the operating system's interrupt handler.
3.  The Linux kernel packages this hardware interrupt as a **POSIX signal**: `SIGILL` (Signal Illegal Instruction).
4.  The kernel looks at the process's signal table. Since our constructor called `sigaction(SIGILL, &sa, &old_sa)`, the kernel jumps back to user space to execute our `handler` function.

---

## 🧠 Core Concept 3: Thread Context Manipulation (`ucontext_t`)
When the operating system calls our signal handler, it passes three arguments:
```c
static void handler(int sig, siginfo_t *si, void *ctx_void)
```
*   `sig`: The signal number (`SIGILL` is `4`).
*   `si`: A pointer to `siginfo_t` containing metadata about the crash (like the crash address).
*   `ctx_void`: A pointer to `ucontext_t` representing the **CPU register snapshot** at the exact millisecond of the crash.

### 1. Reading the Instruction Pointer (`RIP`)
On x86-64 Linux, the instruction pointer (the program counter tracking which byte is running) is stored in the registers array:
```c
uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
```
By reading bytes from `rip[0]`, `rip[1]`, etc., we can parse the opcode. For example:
*   `0x66`: Operand size override prefix (indicates SSE instruction).
*   `0x40` to `0x4f`: REX prefix (extends register access to `XMM8`-`XMM15`).
*   `0x0F 0x38 0xDC`: The opcode bytes for `AESENC`.

### 2. Modifying Registers & Skipping the Instruction
In our emulator, we extract the register indexes from the ModRM byte, emulate the math, and write the output directly into the CPU register context:
```c
uint8_t *dest = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
// ... write emulated math bytes to dest ...
```

**Crucial Step**: If we return from the signal handler now, the CPU will try to execute the exact same instruction at `RIP` again, leading to an infinite crash loop. We must manually advance `RIP` in the context record past the instruction length:
```c
uc->uc_mcontext.gregs[REG_RIP] += (5 + has_rex); // Advance 5 bytes (or 6 if REX is present)
```
When our handler returns, the OS restores the context, and the CPU resumes executing the program at the updated `RIP` address.

---

## 💻 Hacking Exercise 1: Add a Dummy Instruction to the Emulator
Let's practice extending the emulator. We will register a dummy instruction that prints a greeting whenever executed, rather than crashing the program.

We will use the byte sequence `0x0F 0x38 0x00` (which is unassigned or triggers SIGILL on most CPUs) as our dummy instruction.

### Step 1: Modify the Signal Handler
Open [sigill_emulator.c](file:///home/michael/src/agy-compat-toolkit/sigill_emulator.c) and find the opcode decoding block in the `handler` function. Add a check for `0x00` under the `0x0f 0x38` prefix:

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

### Step 2: Test It!
Write a test program in C executing the inline assembly bytes:
```c
__asm__ __volatile__ (".byte 0x66, 0x0f, 0x38, 0x00, 0xc0"); // triggers our handler
```
Build it and run it preloaded with `sigill_emulator.so`. You will see the custom message printed to your screen instead of a crash!

---

## 💻 Hacking Exercise 2: Static Patching via Rizin

Statically modifying binaries (binary hacking) is used to bypass checks permanently without using a runtime signal handler.

### The Mechanics of a Jump Patch
When Go compiles:
```go
if cpu.X86.HasAES == false {
    panic("This binary requires AES")
}
```
It compiles into a conditional branch in assembly (e.g. `jne` / Jump if Not Equal).
If we want to bypass this check, we overwrite the conditional jump instruction with a direct relative jump (`jmp`) or `NOP`s (No-Operation, byte `0x90`).

### Step-by-Step Rizin Hacking Guide

1.  **Open Rizin in Write Mode**:
    ```bash
    rizin -w agy.real
    ```
2.  **Locate the function** (symbol names are prefixed with `sym.`):
    ```text
    > is ~cpu.Initialize
    ```
    This displays the address (e.g., `0x0074d0f0`).
3.  **Seek to the address**:
    ```text
    > s 0x0074d0f0
    ```
4.  **Disassemble the block** to find the jump gate:
    ```text
    > pd 15
    ```
5.  **Write the Jump Patch**:
    We want to jump past the check directly to the return instruction. If the return instruction is `0xb3` bytes ahead, we write:
    `E9 B3 00 00 00` (`jmp +0xb3`) followed by NOP padding (`90`) to overwrite the remaining bytes of the original instruction.
    ```text
    > wx e9b300000090
    ```
6.  **Verify & Exit**:
    Disassemble again to ensure the code now jumps straight to the exit:
    ```text
    > pd 5
    > q
    ```

---

## 📝 Conceptual Review Questions
1. Why does changing `dest[i] ^= key[i]` to use a `tmp` buffer prevent register corruption when `dest == key`?
2. What happens if a signal handler calls a function that isn't "async-signal-safe" (like `printf`), and how did we resolve this in `sigill_emulator.c`?
3. If you compile a native binary on ARM64, does it use x86 AES-NI instructions? Why or why not?
