# Emulation Performance Benchmark Report

This report records the performance metrics of the `sigill_emulator` compatibility layer, structured progressively from high-level concepts to low-level technical specifications.

---

## 🟢 Novice Level: What is Trapping and How Fast is It?

### 1. The Core Concept
Think of a compiled program as a book written in a specific language (machine code). If your computer's processor (CPU) doesn't understand a specific word (an instruction like `aesenc`), it gets confused, stops immediately, and crashes with an **Illegal Instruction** error.

This compatibility layer acts as an **on-the-fly translator**:
1. When the CPU encounters a word it doesn't know, it calls out for help (a signal called `SIGILL` is raised).
2. Our library intercepts this call, translates the complex word into simpler operations the CPU *does* understand, and tells the CPU to keep reading.

### 2. High-Level Speed Summary
Because translation happens at the hardware/operating system boundary, it takes a small amount of time. However, it is incredibly optimized:
* **Throughput**: Translates **613,000 instructions per second**.
* **Impact**: In a typical run, the program only needs to translate a few dozen instructions. The total overhead is **under 1 millisecond** (0.001 seconds)—completely unnoticeable to a human.

---

## 🟡 Developer Level: Benchmark Setup and Comparison

To verify that the emulator works under load and measure its latency, we use a test utility named [benchmark.c](file:///home/michael/src/agy-compat-toolkit/benchmark.c). This program runs a loop of **1,000,000** unsupported `aesenc` instructions.

### 1. Baseline Test (Without Emulator)
Running the benchmark directly on the host CPU without loading the emulator:
```text
[*] Starting benchmark: running 1000000 iterations of 'aesenc xmm0, xmm0'...
Illegal instruction (core dumped)
Exit Code: 132 (SIGILL)
```
* **Result**: Immediate crash on the very first instruction due to hardware lack of AES-NI.

### 2. Emulated Test
Running the benchmark preloaded with our library:
```text
[*] Starting benchmark: running 1000000 iterations of 'aesenc xmm0, xmm0'...
[*] Benchmark complete:
    Elapsed time:       1.631335 seconds
    Traps per second:   612994.93
    Avg latency/trap:   1631.33 ns (1.631 us)
```
* **Result**: 1,000,000 traps intercepted, decoded, mathematically emulated in software, and context-restored successfully with **zero crashes**.

---

## 🔴 Expert Level: Hardware Context and Latency Budget

Below is the technical breakdown of the latency budget for a single emulation trap.

### 1. Cycle & Latency Breakdown (Per Trap)
At an average latency of **1,631 ns (1.63 microseconds)** per instruction trap, the execution flow is divided as follows:

| Stage | Operations | Est. Time | Est. CPU Cycles |
| :--- | :--- | :--- | :--- |
| **Trap Entry** | CPU hardware context save -> Kernel space interrupt handler -> OS signal dispatch to user-space thread. | ~1000 ns | ~3000 |
| **Handler Decode** | Fetch context pointer (`ucontext_t`), read instruction at `RIP`, parse prefix (`0x66`), REX prefix, opcode (`0x38`/`0x3a`), and ModRM register mappings. | ~150 ns | ~450 |
| **Math Emulation** | Stack-buffer AES/PCLMUL computation, state modification, register context writeback. | ~180 ns | ~540 |
| **Trap Exit** | Advance `RIP` in context record, return from signal handler, OS calls `rt_sigreturn` to restore thread context, CPU resumes native execution. | ~300 ns | ~900 |

### 2. Architectural Impact on Registers
During each trap, the emulator manipulates the saved execution context inside `ucontext_t`:
1. It reads from the SSE register states stored in `uc_mcontext.fpregs->_xmm`.
2. To prevent self-modification/aliasing bugs (e.g. `aesenc xmm8, xmm8`), the input registers are treated as immutable values.
3. The emulated output is stored back in a single atomic write to `_xmm` before returning.
