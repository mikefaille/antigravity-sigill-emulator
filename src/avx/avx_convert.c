#include "avx_emulator.h"

int avx_emulate_convert(ucontext_t *uc, struct vex_instruction *inst) {
    uint8_t op = inst->op;
    int vex_pp = inst->vex_pp;
    int vex_W = inst->vex_W;
    int vex_L = inst->vex_L;
    int vex_len = inst->vex_len;
    int mod = inst->mod;
    uint8_t reg_idx = inst->reg_idx;
    uint8_t rm_idx = inst->rm_idx;
    uint8_t v_reg = inst->v_reg;
    uint8_t *mem_addr = inst->mem_addr;

    if (op == 0x5A && (vex_pp == 2 || vex_pp == 3)) { // vcvtss2sd / vcvtsd2ss
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        uint8_t *src_xmm;
        uint8_t tmp_mem[8];
        if (mod == 3) {
            src_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
        } else {
            memcpy(tmp_mem, mem_addr, 8);
            src_xmm = tmp_mem;
        }
        
        vec128 a, b;
        memcpy(&a, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
        memcpy(&b, src_xmm, 16);

        // Pre-buffer results for aliasing safety
        vec128 out;
        memset(&out, 0, sizeof(vec128));

        if (vex_pp == 2) { // float to double
            uint32_t b_val32 = 0;
            memcpy(&b_val32, &b, sizeof(uint32_t));
            math_vcvtsi2sd(&out, &a, b_val32, 0); // Convert low 32-bit float to double
        } else { // double to float
            uint64_t b_val64 = 0;
            memcpy(&b_val64, &b, sizeof(uint64_t));
            math_vcvtsi2ss(&out, &a, b_val64, 1); // Convert low 64-bit double to float
        }
        
        memcpy(dest_xmm, &out, 16);
        memset(ymm_upper[reg_idx], 0, 16);
        return 1;
    }
    else if (op == 0x2A && (vex_pp == 2 || vex_pp == 3)) { // vcvtsi2ss / vcvtsi2sd
        int is_double = (vex_pp == 3);
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        uint8_t *src1_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[v_reg];
        
        uint64_t val = 0;
        if (mod == 3) {
            val = avx_read_gpr(uc, rm_idx);
        } else {
            if (vex_W == 1) {
                memcpy(&val, mem_addr, 8);
            } else {
                uint32_t val32;
                memcpy(&val32, mem_addr, 4);
                val = val32;
            }
        }
        
        vec128 a, out;
        memcpy(&a, src1_xmm, 16);
        memset(&out, 0, sizeof(vec128));

        if (is_double) {
            math_vcvtsi2sd(&out, &a, val, vex_W);
        } else {
            math_vcvtsi2ss(&out, &a, val, vex_W);
        }
        
        memcpy(dest_xmm, &out, 16);
        memset(ymm_upper[reg_idx], 0, 16);
        return 1;
    }
    else if (op == 0x2C && (vex_pp == 2 || vex_pp == 3)) { // vcvttss2si / vcvttsd2si
        int is_double = (vex_pp == 3);
        
        vec128 src;
        memset(&src, 0, sizeof(vec128));
        if (mod == 3) {
            memcpy(&src, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
        } else {
            memcpy(&src, mem_addr, is_double ? 8 : 4);
        }
        
        uint64_t res = 0;
        if (is_double) {
            res = math_vcvttsd2si(&src, vex_W);
        } else {
            res = math_vcvttss2si(&src, vex_W);
        }
        
        avx_write_gpr(uc, reg_idx, res);
        return 1;
    }
    else if (op == 0x5B && (vex_pp == 0 || vex_pp == 1 || vex_pp == 2)) { // vcvtdq2ps / vcvtps2dq / vcvttps2dq
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        
        vec256 src, out;
        memset(&src, 0, sizeof(vec256));
        memset(&out, 0, sizeof(vec256));

        if (mod == 3) {
            memcpy(&src.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            if (vex_L) memcpy(&src.hi, ymm_upper[rm_idx], 16);
        } else {
            memcpy(&src.lo, mem_addr, 16);
            if (vex_L) memcpy(&src.hi, mem_addr + 16, 16);
        }

        if (vex_pp == 0) {
            math_vcvtdq2ps(&out, &src, vex_L);
        } else if (vex_pp == 1) {
            math_vcvtps2dq(&out, &src, vex_L);
        } else {
            math_vcvttps2dq(&out, &src, vex_L);
        }

        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }

    return 0;
}
