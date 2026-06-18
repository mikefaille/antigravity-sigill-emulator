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
sde64 -skx -emit-illegal-insts 0 -- ~/.local/bin/agy
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

### Vectorization & Cryptographic Acceleration via SIMDe
Our emulator now integrates **SIMD Everywhere (SIMDe)** as its performance-critical mathematical and cryptographic execution backend. SIMDe translates 256-bit AVX/AVX2 vector instructions and hardware `AES-NI` routines (such as `aesdec`, `aesenc`, and `aesimc`) directly to the legacy CPU's physical `SSE4.x` instruction pipeline. By leveraging compiler-assisted vector translation instead of slow scalar C lookup tables, emulated paths execute with maximum hardware efficiency.

---

## 🚀 Beginner Quickstart (3-Step Guide)

If you are new to computer science or systems programming, follow these 3 simple steps to get a target binary (like `agy`) running on your older CPU:

### Step 1: Install Compiler Prerequisites
You need a C compiler (`gcc`) and compilation utilities (`make`) to build the project. 

*Note: The project uses **SIMD Everywhere (SIMDe)** under the hood. For your convenience, **SIMDe is already vendored inside this repository (`src/simde/`)**, so no separate installation is required to compile or run the toolkit!*

However, if you are a developer looking to install SIMDe system-wide for other projects, you can do so easily:
*   **Debian/Ubuntu**:
    ```bash
    sudo apt-get update
    sudo apt-get install libsimde-dev build-essential gcc
    ```
*   **RedHat/Fedora**:
    ```bash
    sudo dnf install simde-devel gcc make
    ```
*   **Arch Linux**:
    ```bash
    sudo pacman -S simde gcc make
    ```

### Step 2: Build and Install the Emulator
Compile the compatibility library and install it to your user directory. This automatically builds three dynamic library variants:
*   **`sigill_emulator.so`** (Native): Optimized dynamically for your host CPU.
*   **`sigill_emulator_v1.so`** (x86-64 LEGACY, circa 2003): Compatible with the original 64-bit instruction set specifications.
*   **`sigill_emulator_v2.so`** (x86-64-v2, circa 2009): Targets architectures with SSE3, SSSE3, SSE4.1, SSE4.2, and POPCNT support (like Intel Nehalem/Westmere).

```bash
# Build all library targets
make

# Install all target libraries (defaults to ~/):
make install
```

### Step 3: Patch the Binary (Track A — One-time Static Fix)
Before running, apply the static CPU validation bypass. This removes the startup gate that kills the process immediately on old CPUs:
```bash
python3 ~/patch_agy.py /path/to/binary.real
```
Exit `0` means success (or already patched). The original binary is backed up as `binary.real.bak` on the first run.

### Step 4: Run Your Program (Track B — Runtime Emulation)
Execute your target program preloaded with one of the compatibility libraries:

#### 1. Safe Mode (Default)
Pure trap-and-emulate via `SIGILL` signal handling. Memory pages remain strictly read-only — 100% compliant with SELinux/AppArmor and W^X policies:
```bash
LD_PRELOAD=~/sigill_emulator.so /path/to/binary.real
```

#### 2. Experimental Mode (10x Faster, Requires Writable Code Pages)
Rewrites instruction pages in RAM at runtime using JIT Trampoline Islands. Eliminates the kernel context switch after the first trap, yielding a **10x+ speedup** (~110 ns vs ~1,650 ns). Requires `mprotect` — not compatible with strict SELinux `execmem` policies:
```bash
EMU_MODE=experimental LD_PRELOAD=~/sigill_emulator.so /path/to/binary.real
```

#### 3. Interactive Help Menu
Print the built-in help guide linking back to the GitHub project:
```bash
# Run with --help flag
LD_PRELOAD=~/sigill_emulator.so ./your_program --help

# Or trigger using the EMU_HELP environment variable:
EMU_HELP=1 LD_PRELOAD=~/sigill_emulator.so ./your_program
```

*Tip: To see active emulation traces in your console, run with `DEBUG_EMU=1`:*
```bash
DEBUG_EMU=1 LD_PRELOAD=~/sigill_emulator.so /path/to/binary.real
```

### Step 5: Configure Permanently — Wrapper Script (Recommended)
The wrapper script approach is the most robust option for `agy` because it **automatically re-patches after every self-update** — you never need to manually re-run the patcher when the Antigravity IDE upgrades its language server.

The wrapper at `~/.local/bin/agy` handles both tracks in one place:
1. Detects if `agy.real` is newer than the last-patched marker (`~/.local/bin/.agy.real.patched`)
2. If so, runs `patch_agy.py` automatically and logs output to `/tmp/agy_patch.log`
3. Sets `LD_PRELOAD` and launches `agy.real`

No further configuration is needed if you installed via this wrapper.

#### Alternative: Shell Alias (Manual Patch Required After Each Update)
If you prefer a simple alias without auto-patching, remember to re-run `patch_agy.py` manually whenever `agy` updates itself:
```bash
alias agy="LD_PRELOAD=~/sigill_emulator.so ~/.local/bin/agy.real"
```

#### Alternative: Global Shell Export (Applies to All Processes)
This automatically preloads the compatibility library for *all* processes launched by your shell. Safe to use since the emulator has negligible overhead on CPUs that don't need it:
*   **For `bash`:** Add to `~/.bashrc`:
    ```bash
    export LD_PRELOAD="~/sigill_emulator.so"
    ```
*   **For `zsh`:** Add to `~/.zshrc`:
    ```bash
    export LD_PRELOAD="~/sigill_emulator.so"
    ```

Reload your profile to apply:
```bash
source ~/.bashrc  # or ~/.zshrc
```

---

## 📘 Documentation Directory

*   [LEARNING_GUIDE.md](./docs/LEARNING_GUIDE.md): **Learning Guide**. A comprehensive, step-by-step systems programming tutorial explaining hooks, signals, register manipulations, and manual patching without AI.
*   [SKILL.md](./docs/SKILL.md): **The Operational Playbook**. Read this for a step-by-step diagnostic guide on tracking crashes, finding instruction offsets, and static/dynamic patching logic.
*   [AGENTS.md](./docs/AGENTS.md): **AI Agent Guide**. Documents how LLM coding agents (like Antigravity or OpenCode) can automatically ingest and apply the skill guide to resolve SIGILL errors.
*   [benchmark_results.md](./docs/benchmark_results.md): **Performance Statistics**. Shows the low-level signal trapping overhead (~1.63 microseconds per trap).

---

## 🛠️ Toolkit Components

*   **`src/sigill_emulator.c`**: Core emulator catching `SIGILL` signals, decoding register states, performing software AES/carry-less operations, and returning state.
*   **`scripts/patch_agy.py`**: Multi-signature static patcher (Track A) — also copied to `~/patch_agy.py` during installation. Patches any compatible Go binary; detects already-patched state; backs up the original. Generic — not tied to `agy` specifically.
*   **`~/.local/bin/agy`**: Wrapper script combining Track A (auto-patch on update) + Track B (LD_PRELOAD). Logs to `/tmp/agy_patch.log`. Uses `~/.local/bin/.agy.real.patched` as update marker.
*   **`scripts/find_bad_insns.py`**: Static analysis instruction scanner. Runs instantly with `uv run scripts/find_bad_insns.py`.
*   **`benchmark/benchmark.c`**: Trapping test utility looping 1,000,000 AESENC traps.
*   **`docs/LEARNING_GUIDE.md`**: Systems programming educational guide detailing dynamic hooking, signal trapping, and manual binary hacking.
*   **`pyproject.toml`**: Modern Python dependency definition for `uv` environment isolation.
*   **`Makefile`**: Build automator.
*   **`docs/session_log.md`**: Historical engineering notes on initial bypass static patching.
*   **`LICENSE`**: zlib Open Source License.
