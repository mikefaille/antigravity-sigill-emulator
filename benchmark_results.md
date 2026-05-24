# Emulation Performance Benchmark Report

This document records the throughput and latency metrics for the dynamic `SIGILL` emulation layer.

## System Environment
* **Host Processor**: Intel Xeon (Nehalem/Westmere Architecture, lacking hardware AES-NI / PCLMULQDQ)
* **OS**: Linux (x86_64)

## Benchmark Methodology
We developed a micro-benchmark utility ([benchmark.c](file:///home/michael/src/agy-compat-toolkit/benchmark.c)) that executes a loop of **1,000,000 iterations** of raw `aesenc xmm0, xmm0` instructions.

1. **Unassisted Run (Baseline)**: Executed without the preloaded emulator.
2. **Emulated Run**: Executed with `LD_PRELOAD=/home/michael/sigill_emulator.so` enabled.

---

## Results

### 1. Unassisted Run (Baseline)
```text
[*] Starting benchmark: running 1000000 iterations of 'aesenc xmm0, xmm0'...
Illegal instruction (core dumped)
Exit Code: 132 (SIGILL)
```
* **Result**: Immediate crash on the first loop iteration, verifying the host CPU lacks hardware support for the instruction.

### 2. Emulated Run
```text
[*] Starting benchmark: running 1000000 iterations of 'aesenc xmm0, xmm0'...
[*] Benchmark complete:
    Elapsed time:       1.631335 seconds
    Traps per second:   612994.93
    Avg latency/trap:   1631.33 ns (1.631 us)
```
* **Result**: 1,000,000 traps intercepted, decoded, mathematically emulated, and context-restored successfully with **zero crashes**.

---

## Analysis & Conclusions
* **Throughput**: The emulator processes **~613,000 instruction traps per second**.
* **Latency**: Each dynamic trap (including context switch to kernel, signal generation, handler execution, context decoding, arithmetic round emulation, and returning to user mode) takes only **~1.63 microseconds**.
* **Impact on `agy`**: For typical execution where `agy` processes several hundred map hash functions, the cumulative emulation overhead is under **1 millisecond**, making it completely imperceptible during native operations.
