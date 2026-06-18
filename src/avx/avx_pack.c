#include "avx_emulator.h"

int avx_emulate_pack(ucontext_t *uc, struct vex_instruction *inst) {
    uint8_t op = inst->op;
    int vex_L = inst->vex_L;
    int vex_pp = inst->vex_pp;
    int mod = inst->mod;
    uint8_t reg_idx = inst->reg_idx;
    uint8_t rm_idx = inst->rm_idx;
    uint8_t v_reg = inst->v_reg;
    uint8_t *mem_addr = inst->mem_addr;
    int mem_bytes = inst->mem_bytes;
    int vex_len = inst->vex_len;

    if ((op == 0x63 || op == 0x67 || op == 0x6B) && vex_pp == 1 && inst->vex_m == 1) { // vpacksswb / vpackuswb / vpackssdw
        uint8_t *src1_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[v_reg];
        uint8_t *src1_ymm_u = ymm_upper[v_reg];
        uint8_t *src2_xmm;
        uint8_t *src2_ymm_u;
        uint8_t tmp_mem[32] = {0};
        
        if (mod == 3) {
            src2_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
            src2_ymm_u = ymm_upper[rm_idx];
        } else {
            memcpy(tmp_mem, mem_addr, 16);
            if (vex_L == 1) {
                memcpy(tmp_mem + 16, mem_addr + 16, 16);
            }
            src2_xmm = tmp_mem;
            src2_ymm_u = tmp_mem + 16;
        }

        // Pre-buffer / snapshot to local canonical types for complete aliasing safety
        vec256 a, b, out;
        memcpy(&a.lo, src1_xmm, 16);
        memcpy(&a.hi, src1_ymm_u, 16);
        memcpy(&b.lo, src2_xmm, 16);
        memcpy(&b.hi, src2_ymm_u, 16);
        memset(&out, 0, sizeof(vec256));

        // Invoke pure math operations
        if (op == 0x63) {
            math_vpacksswb(&out, &a, &b);
        } else if (op == 0x67) {
            math_vpackuswb(&out, &a, &b);
        } else if (op == 0x6B) {
            math_vpackssdw(&out, &a, &b);
        }

        // Commit outcomes with architectural constraints (VEX.128 clears upper half)
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if (op == 0x2B && vex_pp == 1 && inst->vex_m == 2) { // vpackusdw
        uint8_t *src1_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[v_reg];
        uint8_t *src1_ymm_u = ymm_upper[v_reg];
        uint8_t *src2_xmm;
        uint8_t tmp_mem[32] = {0};
        
        if (mod == 3) {
            src2_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
        } else {
            memcpy(tmp_mem, mem_addr, 16);
            if (vex_L == 1) {
                memcpy(tmp_mem + 16, mem_addr + 16, 16);
            }
            src2_xmm = tmp_mem;
        }

        vec256 a, b, out;
        memcpy(&a.lo, src1_xmm, 16);
        memcpy(&a.hi, src1_ymm_u, 16);
        memcpy(&b.lo, src2_xmm, 16);
        if (mod == 3) {
            memcpy(&b.hi, ymm_upper[rm_idx], 16);
        } else {
            memcpy(&b.hi, tmp_mem + 16, 16);
        }
        memset(&out, 0, sizeof(vec256));

        math_vpackusdw(&out, &a, &b);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        inst->bytes_consumed = (mod == 3) ? (vex_len + 2) : mem_bytes;
        return 1;
    }

    return 0;
}
