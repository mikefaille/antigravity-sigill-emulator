# Antigravity CPU Compatibility Toolkit (`antigravity-sigill-emulator`)

This toolkit enables high-performance binaries compiled for modern instruction sets (like `AES-NI`, `PCLMULQDQ`, and `AVX/AVX2` vector operations) to run on legacy processors (such as older Intel Xeon Westmere/Nehalem CPUs) without crashing.

It is specifically designed as a zero-latency compatibility layer for the **Google Antigravity IDE** companion language server (`agy` or `language_server_linux_x64`). It achieves this by combining **static binary patching** (for cold validation gates) with a **dynamic in-process signal emulator** (for hot loops) preloaded via `LD_PRELOAD`.

---

## 🔍 Common Search Errors Addressed (SEO)

If you are executing `agy` or related binaries on an older CPU, legacy hardware, or default virtual machine cores (like Proxmox `kvm64`) and searching for solutions to these exact terminal errors:

```text
michael@michael-MacPro5-1:~$ agy
FATAL ERROR: This binary was compiled with aes enabled, but this feature is not available on this processor (go/sigill-fail-fast).
Illegal instruction        (core dumped) agy
```

Or any related variants:
*   `FATAL ERROR: This binary was compiled with pclmul enabled, but this feature is not available on this processor (go/sigill-fail-fast).`
*   `FATAL ERROR: This binary was compiled with avx enabled, but this feature is not available on this processor (go/sigill-fail-fast).`
*   `gdb -batch -ex "run" -ex "bt" agy` -> `Program received signal SIGILL, Illegal instruction.`

This repository provides a high-performance, dynamic in-process emulator to bypass these hardware checks and execute the binary transparently with **zero perceptible latency**.

---

## ⚡ Evolution: Intel SDE vs. Dynamic Signal Emulation

Historically, executing compiled binaries with unsupported instructions on legacy hardware required full-system emulation layers like **Intel Software Development Emulator (SDE)**:
```bash
sde64 -skx -emit-illegal-insts 0 -- /home/michael/.local/bin/agy
```
While Intel SDE successfully emulates modern instructions, it does so via **Dynamic Binary Instrumentation (DBI)**. SDE must translate and rewrite every single instruction executed by the process on the fly, introducing massive **50x to 100x CPU execution latency slowdowns**. This is particularly severe because `agy` uses intensive hardware AES-NI instructions solely for internal map hashing functions during key lookups. Full emulation of all non-cryptographic instructions is highly wasteful.

### The Breakthrough: In-Process Trap Emulation
Our dynamic emulator (`sigill_emulator.so`) leverages **POSIX Hardware Trapping (`SIGILL`)** to achieve native execution speeds with **no perceptible latency**:

| Feature | Intel SDE Emulation | Dynamic Signal Emulation (Our Solution) |
| --- | --- | --- |
| **Mechanic** | Dynamic Binary Instrumentation (interprets every CPU instruction) | Native Hardware Execution + Signal Trap Intercept |
| **Performance** | Extremely Slow (50x - 100x overhead) | **Native Speed (0% overhead on standard code paths)** |
| **Crypto Path Cost** | Constant Translation Overhead | ~1.63 microseconds per cryptographic trap (negligible) |
| **Process Footprint** | Massive emulation engine shell | Single lightweight `LD_PRELOAD` shared library |

### Why Our Solution Has No Perceptible Latency
Instead of translating the entire application, our emulator lets **99.9% of standard x86 instructions execute directly on the physical CPU** at native clock speeds. 

Only when the processor hits an unsupported instruction (e.g. `aesdec` or `pclmulqdq`) does the hardware raise an invalid opcode exception. Our `LD_PRELOAD` handler intercepts this specific signal, emulates the instruction in software, updates the register state, and returns. This localized trap-and-emulate cycle is extremely fast (~1.6 microseconds) and runs only when internal map hash calculations are performed, leaving the application completely responsive during normal operations!

---

## 🚀 Beginner Quickstart (3-Step Guide)

If you are new to computer science or systems programming, follow these 3 simple steps to get a target binary (like `agy`) running on your older CPU:

### Step 1: Install Compiler Prerequisites
You need a C compiler (`gcc`) and compilation utilities (`make`) to build the project:
```bash
sudo apt-get update
sudo apt-get install build-essential gcc
```

### Step 2: Build and Install the Emulator
Compile the compatibility library and install it to your user directory:
```bash
# Build the project
make

# Install globally to /home/michael/sigill_emulator.so
make install
```

### Step 3: Run Your Program
Execute your target program (e.g. `agy.real`) preloaded with the compatibility library:
```bash
LD_PRELOAD=/home/michael/sigill_emulator.so /path/to/binary.real --help
```
*Tip: To see active emulation traces in your console, run with `DEBUG_EMU=1`:*
```bash
DEBUG_EMU=1 LD_PRELOAD=/home/michael/sigill_emulator.so /path/to/binary.real --help
```

### Step 4: Configure LD_PRELOAD Permanently (Shell Configurations)
To avoid typing `LD_PRELOAD` before every command, you can configure it permanently in your shell profiles (for `bash`, `zsh`, or sh).

#### Option A: Shell Alias (Safe, Target-Specific)
This is the safest method because it only preloads the library when you run the specific binary. Add this to your `~/.bashrc`, `~/.zshrc`, or `~/.bash_aliases`:
```bash
alias agy="LD_PRELOAD=/home/michael/sigill_emulator.so /home/michael/.local/bin/agy.real"
```

#### Option B: Global Shell Export (Convenient, Multi-process)
This automatically preloads the compatibility library for *all* processes launched by your shell. Since the emulator has practically zero overhead for compatible standard instructions, this is safe and highly convenient.

Add this export statement to your shell configuration files:
*   **For standard shells (`sh`/`bash`):** Add to `~/.profile` or `~/.bashrc`:
    ```bash
    export LD_PRELOAD="/home/michael/sigill_emulator.so"
    ```
*   **For Z-shell (`zsh`):** Add to `~/.zshrc`:
    ```bash
    export LD_PRELOAD="/home/michael/sigill_emulator.so"
    ```

Reload your profile to apply changes:
```bash
source ~/.profile  # or source ~/.bashrc / ~/.zshrc
```

---

## 📘 Documentation Directory

*   [LEARNING_GUIDE.md](file:///home/michael/src/agy-compat-toolkit/LEARNING_GUIDE.md): **Learning Guide**. A comprehensive, step-by-step systems programming tutorial explaining hooks, signals, register manipulations, and manual patching without AI.
*   [SKILL.md](file:///home/michael/src/agy-compat-toolkit/SKILL.md): **The Operational Playbook**. Read this for a step-by-step diagnostic guide on tracking crashes, finding instruction offsets, and static/dynamic patching logic.
*   [AGENTS.md](file:///home/michael/src/agy-compat-toolkit/AGENTS.md): **AI Agent Guide**. Documents how LLM coding agents (like Antigravity or OpenCode) can automatically ingest and apply the skill guide to resolve SIGILL errors.
*   [benchmark_results.md](file:///home/michael/src/agy-compat-toolkit/benchmark_results.md): **Performance Statistics**. Shows the low-level signal trapping overhead (~1.63 microseconds per trap).

---

## 🛠️ Toolkit Components

*   **`sigill_emulator.c`**: Core emulator catching `SIGILL` signals, decoding register states, performing software AES/carry-less operations, and returning state.
*   **`find_bad_insns.py`**: Static analysis instruction scanner. Runs instantly with `uv run find_bad_insns.py`.
*   **`benchmark.c`**: Trapping test utility looping 1,000,000 AESENC traps.
*   **`LEARNING_GUIDE.md`**: Systems programming educational guide detailing dynamic hooking, signal trapping, and manual binary hacking.
*   **`pyproject.toml`**: Modern Python dependency definition for `uv` environment isolation.
*   **`Makefile`**: Build automator.
*   **`session_log.md`**: Historical engineering notes on initial bypass static patching.
*   **`LICENSE`**: zlib Open Source License.
