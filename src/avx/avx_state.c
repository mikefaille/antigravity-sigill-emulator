#include "avx_emulator.h"

// Define the thread-local YMM upper registers
__thread uint8_t ymm_upper[16][16] __attribute__((tls_model("initial-exec")));

// Helper to read general-purpose register values in context
uint64_t avx_read_gpr(const ucontext_t *uc, int reg_idx) {
    static const int reg_map[16] = {
        REG_RAX, REG_RCX, REG_RDX, REG_RBX, REG_RSP, REG_RBP, REG_RSI, REG_RDI,
        REG_R8,  REG_R9,  REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15
    };
    return uc->uc_mcontext.gregs[reg_map[reg_idx]];
}

// Helper to set general-purpose register values in context
void avx_write_gpr(ucontext_t *uc, int reg_idx, uint64_t val) {
    static const int reg_map[16] = {
        REG_RAX, REG_RCX, REG_RDX, REG_RBX, REG_RSP, REG_RBP, REG_RSI, REG_RDI,
        REG_R8,  REG_R9,  REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15
    };
    uc->uc_mcontext.gregs[reg_map[reg_idx]] = val;
}
