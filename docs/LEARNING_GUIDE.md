# Under the Hood: A Systems Programming Learning Guide

Welcome, fellow developer! This guide is designed to teach you the fundamental concepts of low-level [systems programming](https://en.wikipedia.org/wiki/Systems_programming) behind this toolkit. 

To ensure this guide is fully accessible, **we assume zero prior knowledge** of CPU internals or operating system architectures. We will define every key concept from scratch, and we provide [Wikipedia](https://www.wikipedia.org/) links for complementary reading.

---

## 🗺️ High-Level Architecture Overview
A computer program is ultimately a sequence of instructions stored in a binary file. When executed, the [central processing unit (CPU)](https://en.wikipedia.org/wiki/Central_processing_unit) decodes and runs these instructions one by one.

If a binary is compiled to use modern instructions (like [AES-NI](https://en.wikipedia.org/wiki/AES_instruction_set) for encryption or [PCLMULQDQ](https://en.wikipedia.org/wiki/CLMUL_instruction_set) for carry-less multiplication) but is run on a legacy CPU that physically lacks those circuits, the CPU does not know what to do. It halts and crashes the program.

Our toolkit bypasses this hardware limitation using two core techniques:
1.  **Dynamic Hooking (`LD_PRELOAD`)**: Injecting our custom [shared library](https://en.wikipedia.org/wiki/Shared_library) into the target program's memory at startup.
2.  **Hardware Exception Trapping (`SIGILL`)**: Intercepting the CPU's crash signal, decoding the unsupported instructions, executing their mathematical logic in software on the fly, and resuming the program cleanly.

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

## 🧠 Core Concept 2: Hardware Exceptions & POSIX Signals

CPUs read instructions in the form of binary byte sequences called [opcodes](https://en.wikipedia.org/wiki/Opcode). If the CPU reads an opcode that does not exist in its instruction set architecture (ISA), it cannot proceed.

### The Trap-and-Emulate Sequence:
1.  **Hardware Exception**: The CPU halts the running thread and triggers an [illegal instruction exception](https://en.wikipedia.org/wiki/Illegal_instruction) (a type of hardware [interrupt](https://en.wikipedia.org/wiki/Interrupt)).
2.  **Privilege Transition**: The CPU switches from [User Mode to Kernel Mode](https://en.wikipedia.org/wiki/User_space_and_kernel_space) (giving the OS full control over hardware) and invokes the operating system's [interrupt handler](https://en.wikipedia.org/wiki/Interrupt_handler).
3.  **Signal Delivery**: The Linux kernel translates this hardware interrupt into a user-space [POSIX signal](https://en.wikipedia.org/wiki/Signal_(IPC)) called `SIGILL` (Signal Illegal Instruction).
4.  **Signal Handler Redirect**: Normally, `SIGILL` terminates the program ("Illegal instruction (core dumped)"). However, since our preloaded library registered a custom [signal handler](https://en.wikipedia.org/wiki/Signal_(IPC)#Handling_signals) using the `sigaction` system call, the kernel switches the thread back to User Mode and executes our handler function.

---

## 🧠 Core Concept 3: Thread Context Manipulation (`ucontext_t`)

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

## 🧠 Core Concept 4: Finite Field Math & Branchless Optimization

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
Run the following command in your terminal:
```bash
gcc -shared -fPIC -O2 -Wall -o sigill_emulator.so src/sigill_emulator.c
```

### Compiler Flags Explained:
*   **`-shared`**: Tells the compiler to produce a shared object (`.so`) library instead of a standard executable binary (which expects a `main` function).
*   **`-fPIC`**: Generates **Position Independent Code**. Shared libraries are loaded into arbitrary address locations in a process's memory space at runtime. `-fPIC` ensures the machine instructions use relative offsets (RIP-relative addressing) instead of absolute memory locations.
*   **`-O2`**: Activates Level 2 optimization, which is essential to minimize signal-trapping latency.
*   **`-Wall`**: Enables all compiler warning messages to ensure code safety.

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
    Identify the address of the conditional check (e.g. `0x0074d110`) and the exit or success path return instruction address (e.g. `0x0074d1c9`).

5.  **How to Calculate the Offset**:
    If you want to write raw hex bytes, you need to calculate the relative offset:
    `Offset = TargetAddress - (SourceAddress + InstructionLength)`
    *   Target Address: `0x0074d1c9`
    *   Source Address (Start of Jump): `0x0074d110`
    *   Instruction Length of `jmp rel32`: `5` bytes
    *   Calculation: `0x0074d1c9 - (0x0074d110 + 5) = 0xb4` (in hexadecimal).
    *   This translates to opcode `E9 B4 00 00 00`.

6.  **Write the Jump Patch (Option A: Write Raw Hexadecimal)**:
    Use the `wx` command to write the hexadecimal values of `jmp +0xb4` followed by `90` (NOP) to overwrite any leftover instruction bytes:
    ```text
    > wx e9b400000090
    ```

7.  **Write the Jump Patch (Option B: Write Assembly Directly - Recommended)**:
    Instead of calculating offsets manually, Rizin has a built-in assembler. You can write the assembly instruction directly using the `wa` (Write Assembly) command:
    ```text
    > wa jmp 0x0074d1c9
    ```
    To overwrite the remaining bytes of the original instruction with NOPs, use:
    ```text
    > wa nop
    ```

8.  **Verify & Exit**:
    Disassemble the code again to ensure the instruction has been replaced and points to the correct destination:
    ```text
    > pd 5
    > q
    ```

---

## 📝 Conceptual Review Questions
1. Why does changing `dest[i] ^= key[i]` to use a `tmp` buffer prevent register corruption when `dest == key`?
2. What happens if a signal handler calls a function that isn't "async-signal-safe" (like `printf`), and how did we resolve this in `sigill_emulator.c`?
3. If you compile a native binary on ARM64, does it use x86 AES-NI instructions? Why or why not?
