---
name: avx-emu-compatibility
description: Guide and architectural instructions for compiling, building, and running AVX/AVX2/FMA/F16C emulation on legacy Intel LGA1366 / Xeon Nehalem architectures. Use when building custom SIGILL translators or integrating SIMDe execution backends for unmodifiable vector binaries.
---

# AVX Emulation and Compatibility on Legacy Intel LGA1366

This skill provides verified architectural guidance, safety rules, and execution playbooks for emulating AVX, AVX2, FMA, and F16C instruction sets on legacy Xeon Nehalem/Westmere (LGA1366) CPUs that physically lack AVX registers and opcodes.

---

## 1. Core Architectural Separation

A performant and robust compatibility emulator must split the runtime into two distinct, isolated layers:

```text
SIGILL Signal Handler (ld.so)
       ↓
AVX Architectural Layer (src/avx/)
  - VEX Prefix / ModRM / SIB Decoder
  - Register State (YMM Upper halves)
  - Context & RIP advancement
       ↓
SIMDe Mathematical Layer (src/math/)
  - Portably lowered to Nehalem SSE4.2
  - Explicit saturating arithmetic
```

1. **AVX Architectural Layer**: Responsible for system state, instruction stream decoding, reading/writing `ucontext_t` registers, managing thread-local virtual YMM upper halves, and managing EFLAGS and RIP.
2. **SIMDe Mathematical Layer**: Uses the SIMD Everywhere (SIMDe) portable header-only library as the math execution backend. SIMDe executes 256-bit operations and portably lowers them to highly optimized, native Nehalem SSE4.x instructions.

---

## 2. Critical Async-Signal-Safety & Thread-Local Rules

A SIGILL signal handler (especially when preloaded on complex host runtimes like the Java Virtual Machine) runs under strict execution contexts. To prevent deadlocks, recursive crashes, and JVM signals desynchronization:

### 2.1 symbol Pre-Resolution & Validation
1. **NO `dlsym` inside the handler**: Interposing functions like `sigaction` must pre-resolve their original symbol targets (e.g. `real_sigaction`) inside the shared library's constructor `__attribute__((constructor))` phase.
2. **Validate `real_sigaction` early**: Resolve the symbol at the very beginning of the constructor and fail/exit explicitly if unavailable:
   ```c
   real_sigaction = (orig_sigaction_t)dlsym(RTLD_NEXT, "sigaction");
   if (!real_sigaction) {
       static const char message[] = "sigill-emulator: cannot resolve sigaction\n";
       write(STDERR_FILENO, message, sizeof(message) - 1);
       _exit(127);
   }
   ```
3. **NO memory allocations**: `malloc` and `free` take global locks and are not async-signal-safe.
4. **NO `fprintf` or `printf`**: Logging must use stack-allocated, bounded string buffers and the direct, non-blocking `write(2)` system call.

### 2.2 Thread-Local Storage (TLS) Models
1. **Force `initial-exec` TLS**: Because this library is loaded dynamically through `LD_PRELOAD`, ordinary TLS accesses can fallback to dynamic paths involving `__tls_get_addr`. If the first access occurs inside the signal handler, it can cause loader deadlocks. Always decorate thread-local state with `__attribute__((tls_model("initial-exec")))`:
   ```c
   static __thread __attribute__((tls_model("initial-exec"))) volatile sig_atomic_t in_sigill_handler;
   __thread uint8_t ymm_upper[16][16] __attribute__((tls_model("initial-exec")));
   ```

---

## 3. Signal Chaining, Virtualization & Safe Fallbacks

The JVM dynamically overrides registered signal handlers during startup. To keep our `SIGILL` emulator alive, the interposer must intercept `sigaction` calls with extreme safety:

### 3.1 Special Disposition Checks
Check the special handlers and signal mask before forwarding:
```c
if (previous.sa_handler == SIG_IGN) {
    return;
}
if (previous.sa_handler == SIG_DFL) {
    // Restore default handler disposition
    real_sigaction(sig, &previous, NULL);

    // Unblock the signal in thread signal mask to allow it to be delivered immediately
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, sig);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    // Re-raise the exact signal on the faulting thread using tgkill
    syscall(SYS_tgkill, getpid(), (pid_t)syscall(SYS_gettid), sig);
    _exit(128 + sig);
}
if (previous.sa_flags & SA_SIGINFO) {
    previous.sa_sigaction(sig, si, ctx_void);
} else {
    previous.sa_handler(sig);
}
```

### 3.2 Thread-Correct Fallback Delivery
Never use process-wide `kill(getpid(), SIGILL)` for thread-default handling as it may target an arbitrary thread. Instead, restore the disposition, unblock the signal in the thread signal mask, and deliver directly to the faulting thread using `SYS_tgkill` (syscall).

### 3.3 Atomic Signal-Action Snapshots (Ring Slots)
To avoid data races when the host thread replaces signal handlers concurrently with signal delivery, use thread-safe lock-free snapshots. Write the updated handler config to a dormant slot and atomically publish its pointer:
```c
static struct sigaction sa_slots[16];
static volatile int sa_slot_idx = 0;
static struct sigaction * volatile active_sa = NULL;

// Inside interposed sigaction():
int next_idx = (sa_slot_idx + 1) % 16;
sa_slots[next_idx] = *act;
__sync_synchronize(); // Memory fence
active_sa = &sa_slots[next_idx];
sa_slot_idx = next_idx;
```

### 3.4 siglongjmp-Safe TLS guard
Chained signal handlers (such as JRE crash reporters or custom exceptions) can perform `siglongjmp()`, never returning to our outer wrapper. Always clear the thread-local recursion guard (`in_sigill_handler = 0;`) immediately *before* calling/chaining to any external handler.

---

## 4. Instruction Mapping & expected Length matching

### 4.1 vzeroupper / vzeroall
* **Encoding**: Opcode `0x77`, `vex_pp == 0`.
* **No ModRM**: This instruction contains no ModR/M byte.
* **Length**: Must consume exactly `vex_len + 1` bytes (3 bytes for 2-byte VEX, 4 bytes for 3-byte VEX) to avoid desynchronizing the IP stream.
* **Effect**: Zeroes out the virtual thread-local YMM upper registers.

### 4.2 Failsafe Expected Length Match (The Immediate Multi-byte Guard)
Instructions with trailing immediate bytes and memory displacement operands (such as `vpermq`, `vextractf128`/`vextracti128`, `vinsertf128`/`vinserti128`, `vpermilps`, `vshufps`/`vshufpd`, and `vroundss`/`vroundsd`/`vroundps`/`vroundpd`) are highly vulnerable to size calculation bugs inside standard `resolve_vex_mem_addr` parsers. Always calculate their expected instruction length mathematically:
```c
int expected_len;
if (mod == 3) {
    expected_len = vex_len + 3; // Prefix + Opcode + ModRM + Immediate
} else {
    expected_len = vex_len + 2; // Prefix + Opcode + ModRM
    if (inst->rm == 4) expected_len += 1; // SIB byte
    if (mod == 1) expected_len += 1; // 8-bit displacement
    else if (mod == 2) expected_len += 4; // 32-bit displacement
    else if (mod == 0 && inst->rm == 5) expected_len += 4; // RIP-relative VMA
    expected_len += 1; // Trailing immediate byte
}
inst->bytes_consumed = expected_len;
```

---

## 5. Developer's Playbook: Diagnostic & Build Rules

### 5.1 Verification Script Target Constraints
Compile the entire library with explicit Nehalem-specific target constraints, disable dynamic vectorization, and configure tuning options:
```makefile
CFLAGS = -fPIC -O2 -march=nehalem -mtune=nehalem -mno-avx -mno-avx2 -mno-fma -mno-bmi -mno-bmi2 -fno-tree-vectorize -Wall -Wextra -fvisibility=hidden
LDFLAGS = -shared
LDLIBS = -ldl
```

To ensure absolutely no unsupported instruction patterns are compiled, the `verify` target must scan the disassembled binary for the following mnemonic patterns:
```makefile
verify: $(TARGET)
	@echo "🔍 Auditing $(TARGET) for forbidden CPU instructions..."
	@if objdump -d $(TARGET) | grep -E '\b(v[a-z0-9]+|andn|bextr|pdep|pext|mulx|shlx|shrx|sarx|aesenc|aesdec|aesenclast|aesdeclast|aesimc|aeskeygenassist|pclmulqdq|rdrand|rdseed|adcx|adox)\b' > /dev/null; then \
		echo "❌ ERROR: Forbidden instructions found in emulator library!"; \
		exit 1; \
	else \
		echo "✅ No configured forbidden instruction mnemonics detected."; \
	fi
```

### 5.2 Signal Chaining Test Suite Guidelines
The test suite `test_signal_chaining.c` must verify:
1. `SA_SIGINFO` handler forwarding.
2. One-argument handler forwarding.
3. `SIG_DFL` handoff.
4. `SIG_IGN` handoff.
5. `SA_RESETHAND` restoration behavior.
6. Handlers with a non-empty `sa_mask`.
7. Handlers that modify the supplied `ucontext`.
8. Handlers that perform `siglongjmp` (and verify recursion guard reset).
9. Supported VEX instructions do not chain.
10. Unsupported VEX instructions chain exactly once.
11. Concurrent `sigaction()` replacement atomic safety.
12. Comprehensive timeouts to fail instead of hanging on loops.
