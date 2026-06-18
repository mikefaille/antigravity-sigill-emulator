#ifndef AVX_EMULATOR_H
#define AVX_EMULATOR_H

#define _GNU_SOURCE
#include <signal.h>
#include <ucontext.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../math/math_backend.h"

// Struct for decoded VEX instruction
struct vex_instruction {
    int vex_len;
    int vex_R, vex_X, vex_B, vex_W;
    int vex_vvvv;
    int vex_L;
    int vex_pp;
    int vex_m;
    uint8_t op;
    uint8_t modrm;
    uint8_t mod, reg, rm;
    uint8_t reg_idx, rm_idx, v_reg;
    uint8_t *mem_addr;
    int mem_bytes;
    int bytes_consumed;
};

// Thread-local upper YMM register state
extern __thread uint8_t ymm_upper[16][16] __attribute__((tls_model("initial-exec")));

// Public API
int emulate_avx_instruction(ucontext_t *uc, uint8_t *rip, int sig, siginfo_t *si, void *ctx_void, int debug_emu);

// Internal Module Helper Declarations
uint64_t avx_read_gpr(const ucontext_t *uc, int reg_idx) __attribute__((visibility("hidden")));
void avx_write_gpr(ucontext_t *uc, int reg_idx, uint64_t val) __attribute__((visibility("hidden")));
uint8_t *resolve_vex_mem_addr(ucontext_t *uc, uint8_t *rip, int vex_len, int vex_B, int vex_X, int vex_m, int *bytes_consumed_out);

// Individual Instruction Category Handlers
int avx_decode(ucontext_t *uc, uint8_t *rip, struct vex_instruction *inst);
int avx_emulate_move(ucontext_t *uc, struct vex_instruction *inst);
int avx_emulate_pack(ucontext_t *uc, struct vex_instruction *inst);
int avx_emulate_permute(ucontext_t *uc, struct vex_instruction *inst);
int avx_emulate_integer(ucontext_t *uc, struct vex_instruction *inst);
int avx_emulate_float(ucontext_t *uc, struct vex_instruction *inst);
int avx_emulate_convert(ucontext_t *uc, struct vex_instruction *inst);
int avx_emulate_compare(ucontext_t *uc, struct vex_instruction *inst);

#endif // AVX_EMULATOR_H
