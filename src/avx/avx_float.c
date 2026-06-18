#include "avx_emulator.h"

int avx_emulate_float(ucontext_t *uc, struct vex_instruction *inst) {
    uint8_t op = inst->op;
    int vex_L = inst->vex_L;
    int vex_pp = inst->vex_pp;
    int mod = inst->mod;
    uint8_t reg_idx = inst->reg_idx;
    uint8_t rm_idx = inst->rm_idx;
    uint8_t v_reg = inst->v_reg;
    uint8_t *mem_addr = inst->mem_addr;
    int vex_len = inst->vex_len;

    if ((op == 0x54 || op == 0x55 || op == 0x56 || op == 0x57) && (vex_pp == 0 || vex_pp == 1)) { // vandps / vandnps / vorps / vxorps
        float src2_f[8];
        memset(src2_f, 0, sizeof(src2_f));
        if (mod == 3) {
            memcpy(&src2_f[0], &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            memcpy(&src2_f[4], ymm_upper[rm_idx], 16);
        } else {
            memcpy(&src2_f[0], mem_addr, vex_L ? 32 : 16);
        }

        float src1_f[8];
        memcpy(&src1_f[0], &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
        memcpy(&src1_f[4], ymm_upper[v_reg], 16);
        
        vec256 a, b, out;
        memcpy(&a.lo, &src1_f[0], 16);
        memcpy(&a.hi, &src1_f[4], 16);
        memcpy(&b.lo, &src2_f[0], 16);
        memcpy(&b.hi, &src2_f[4], 16);
        memset(&out, 0, sizeof(vec256));

        if (op == 0x57) {
            if (vex_pp == 1) math_vxorpd(&out, &a, &b, vex_L);
            else math_vpxor(&out, &a, &b);
        }
        else if (op == 0x54) {
            if (vex_pp == 1) math_vandpd(&out, &a, &b, vex_L);
            else math_vpand(&out, &a, &b);
        }
        else if (op == 0x56) {
            if (vex_pp == 1) math_vorpd(&out, &a, &b, vex_L);
            else math_vpor(&out, &a, &b);
        }
        else if (op == 0x55) {
            if (vex_pp == 1) math_vandnpd(&out, &a, &b, vex_L);
            else math_vpandn(&out, &a, &b);
        }

        float *dest_f = (float *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_f, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }

    if ((op == 0x58 || op == 0x59 || op == 0x5C || op == 0x5E || op == 0x5D || op == 0x5F) && vex_pp == 0) { // vaddps / vmulps / vsubps / vdivps / vminps / vmaxps
        float src2_f[8];
        memset(src2_f, 0, sizeof(src2_f));
        if (mod == 3) {
            memcpy(&src2_f[0], &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            memcpy(&src2_f[4], ymm_upper[rm_idx], 16);
        } else {
            memcpy(&src2_f[0], mem_addr, vex_L ? 32 : 16);
        }

        float src1_f[8];
        memcpy(&src1_f[0], &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
        memcpy(&src1_f[4], ymm_upper[v_reg], 16);
        
        vec256 a, b, out;
        memcpy(&a.lo, &src1_f[0], 16);
        memcpy(&a.hi, &src1_f[4], 16);
        memcpy(&b.lo, &src2_f[0], 16);
        memcpy(&b.hi, &src2_f[4], 16);
        memset(&out, 0, sizeof(vec256));

        if (op == 0x58) math_vaddps(&out, &a, &b);
        else if (op == 0x59) math_vmulps(&out, &a, &b);
        else if (op == 0x5C) math_vsubps(&out, &a, &b);
        else if (op == 0x5E) math_vdivps(&out, &a, &b);
        else if (op == 0x5D) math_vminps(&out, &a, &b);
        else if (op == 0x5F) math_vmaxps(&out, &a, &b);

        float *dest_f = (float *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_f, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x58 || op == 0x59 || op == 0x5C || op == 0x5E || op == 0x5D || op == 0x5F) && vex_pp == 1) { // vaddpd / vmulpd / vsubpd / vdivpd / vminpd / vmaxpd
        double src2_d[4];
        memset(src2_d, 0, sizeof(src2_d));
        if (mod == 3) {
            memcpy(&src2_d[0], &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            memcpy(&src2_d[2], ymm_upper[rm_idx], 16);
        } else {
            memcpy(&src2_d[0], mem_addr, vex_L ? 32 : 16);
        }

        double src1_d[4];
        memcpy(&src1_d[0], &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
        memcpy(&src1_d[2], ymm_upper[v_reg], 16);
        
        vec256 a, b, out;
        memcpy(&a.lo, &src1_d[0], 16);
        memcpy(&a.hi, &src1_d[2], 16);
        memcpy(&b.lo, &src2_d[0], 16);
        memcpy(&b.hi, &src2_d[2], 16);
        memset(&out, 0, sizeof(vec256));

        if (op == 0x58) math_vaddpd(&out, &a, &b);
        else if (op == 0x59) math_vmulpd(&out, &a, &b);
        else if (op == 0x5C) math_vsubpd(&out, &a, &b);
        else if (op == 0x5E) math_vdivpd(&out, &a, &b);
        else if (op == 0x5D) math_vminpd(&out, &a, &b);
        else if (op == 0x5F) math_vmaxpd(&out, &a, &b);

        double *dest_d = (double *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_d, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x58 || op == 0x59 || op == 0x5C || op == 0x5E || op == 0x5D || op == 0x5F) && (vex_pp == 2 || vex_pp == 3)) { // vaddss/d, vsubss/d, vmulss/d, vdivss/d, vminss/d, vmaxss/d
        int is_double = (vex_pp == 3);
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        uint8_t *src1_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[v_reg];
        
        vec128 a, b, out;
        memcpy(&a, src1_xmm, 16);
        if (mod == 3) {
            memcpy(&b, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
        } else {
            memcpy(&b, mem_addr, is_double ? 8 : 4);
        }
        memset(&out, 0, sizeof(vec128));

        if (is_double) {
            if (op == 0x58) math_vaddsd(&out, &a, &b);
            else if (op == 0x59) math_vmulsd(&out, &a, &b);
            else if (op == 0x5C) math_vsubsd(&out, &a, &b);
            else if (op == 0x5E) math_vdivsd(&out, &a, &b);
            else if (op == 0x5D) math_vminsd(&out, &a, &b);
            else if (op == 0x5F) math_vmaxsd(&out, &a, &b);
        } else {
            if (op == 0x58) math_vaddss(&out, &a, &b);
            else if (op == 0x59) math_vmulss(&out, &a, &b);
            else if (op == 0x5C) math_vsubss(&out, &a, &b);
            else if (op == 0x5E) math_vdivss(&out, &a, &b);
            else if (op == 0x5D) math_vminss(&out, &a, &b);
            else if (op == 0x5F) math_vmaxss(&out, &a, &b);
        }
        
        memcpy(dest_xmm, &out, 16);
        memset(ymm_upper[reg_idx], 0, 16);
        return 1;
    }
    else if (op == 0xD0 && (vex_pp == 1 || vex_pp == 3) && inst->vex_m == 1) { // vaddsubpd / vaddsubps
        vec256 src1, src2, out;
        memset(&src1, 0, sizeof(vec256));
        memset(&src2, 0, sizeof(vec256));
        memset(&out, 0, sizeof(vec256));

        memcpy(&src1.lo, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
        memcpy(&src1.hi, ymm_upper[v_reg], 16);

        if (mod == 3) {
            memcpy(&src2.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            memcpy(&src2.hi, ymm_upper[rm_idx], 16);
        } else {
            memcpy(&src2.lo, mem_addr, 16);
            if (vex_L == 1) {
                memcpy(&src2.hi, mem_addr + 16, 16);
            }
        }

        if (vex_pp == 1) {
            math_vaddsubpd(&out, &src1, &src2, vex_L);
        } else {
            math_vaddsubps(&out, &src1, &src2, vex_L);
        }

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x12 || op == 0x16) && vex_pp == 2 && inst->vex_m == 1) { // vmovsldup / vmovshdup
        vec256 src, out;
        memset(&src, 0, sizeof(vec256));
        memset(&out, 0, sizeof(vec256));

        if (mod == 3) {
            memcpy(&src.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            memcpy(&src.hi, ymm_upper[rm_idx], 16);
        } else {
            memcpy(&src.lo, mem_addr, 16);
            if (vex_L == 1) {
                memcpy(&src.hi, mem_addr + 16, 16);
            }
        }

        if (op == 0x12) {
            math_vmovsldup(&out, &src, vex_L);
        } else {
            math_vmovshdup(&out, &src, vex_L);
        }

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x14 || op == 0x15) && vex_pp == 0 && inst->vex_m == 1) { // vunpcklps / vunpckhps
        vec256 src1, src2, out;
        memset(&src1, 0, sizeof(vec256));
        memset(&src2, 0, sizeof(vec256));
        memset(&out, 0, sizeof(vec256));

        memcpy(&src1.lo, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
        memcpy(&src1.hi, ymm_upper[v_reg], 16);

        if (mod == 3) {
            memcpy(&src2.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            memcpy(&src2.hi, ymm_upper[rm_idx], 16);
        } else {
            memcpy(&src2.lo, mem_addr, 16);
            if (vex_L == 1) {
                memcpy(&src2.hi, mem_addr + 16, 16);
            }
        }

        if (op == 0x14) {
            math_vunpcklps(&out, &src1, &src2, vex_L);
        } else {
            math_vunpckhps(&out, &src1, &src2, vex_L);
        }

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x12 || op == 0x13) && vex_pp == 1 && inst->vex_m == 1) { // vunpcklpd / vunpckhpd
        vec256 src1, src2, out;
        memset(&src1, 0, sizeof(vec256));
        memset(&src2, 0, sizeof(vec256));
        memset(&out, 0, sizeof(vec256));

        memcpy(&src1.lo, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
        memcpy(&src1.hi, ymm_upper[v_reg], 16);

        if (mod == 3) {
            memcpy(&src2.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            memcpy(&src2.hi, ymm_upper[rm_idx], 16);
        } else {
            memcpy(&src2.lo, mem_addr, 16);
            if (vex_L == 1) {
                memcpy(&src2.hi, mem_addr + 16, 16);
            }
        }

        if (op == 0x12) {
            math_vunpcklpd(&out, &src1, &src2, vex_L);
        } else {
            math_vunpckhpd(&out, &src1, &src2, vex_L);
        }

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x18 || op == 0x19) && vex_pp == 1 && inst->vex_m == 2) { // vbroadcastss / vbroadcastsd
        int is_double = (op == 0x19);
        vec128 src;
        memset(&src, 0, sizeof(vec128));
        
        if (mod == 3) {
            memcpy(&src, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
        } else {
            memcpy(&src, mem_addr, is_double ? 8 : 4);
        }
        
        vec256 out;
        memset(&out, 0, sizeof(vec256));

        if (is_double) {
            math_vbroadcastsd(&out, &src, vex_L);
        } else {
            math_vbroadcastss(&out, &src, vex_L);
        }
        
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if (op == 0x04 && vex_pp == 1 && inst->vex_m == 3) { // vpermilps ymm/xmm, ymm/xmm/m256, imm
        uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        int expected_len;
        if (mod == 3) {
            expected_len = vex_len + 3;
        } else {
            expected_len = vex_len + 2; // prefix + opcode + ModRM
            if (inst->rm == 4) expected_len += 1; // SIB
            if (mod == 1) expected_len += 1;
            else if (mod == 2) expected_len += 4;
            else if (mod == 0 && inst->rm == 5) expected_len += 4;
            expected_len += 1; // imm
        }
        uint8_t imm_val = rip[expected_len - 1];

        vec256 src, out;
        memset(&src, 0, sizeof(vec256));
        memset(&out, 0, sizeof(vec256));

        if (mod == 3) {
            memcpy(&src.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            if (vex_L == 1) {
                memcpy(&src.hi, ymm_upper[rm_idx], 16);
            }
        } else {
            memcpy(&src.lo, mem_addr, 16);
            if (vex_L == 1) {
                memcpy(&src.hi, mem_addr + 16, 16);
            }
        }

        math_vpermilps(&out, &src, imm_val, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        inst->bytes_consumed = expected_len;
        return 1;
    }
    else if (op == 0xC6 && (vex_pp == 0 || vex_pp == 1) && inst->vex_m == 1) { // vshufps / vshufpd ymm/xmm, ymm/xmm, ymm/xmm/m256, imm
        uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        int expected_len;
        if (mod == 3) {
            expected_len = vex_len + 3;
        } else {
            expected_len = vex_len + 2; // prefix + opcode + ModRM
            if (inst->rm == 4) expected_len += 1; // SIB
            if (mod == 1) expected_len += 1;
            else if (mod == 2) expected_len += 4;
            else if (mod == 0 && inst->rm == 5) expected_len += 4;
            expected_len += 1; // imm
        }
        uint8_t imm_val = rip[expected_len - 1];

        vec256 src1, src2, out;
        memset(&src1, 0, sizeof(vec256));
        memset(&src2, 0, sizeof(vec256));
        memset(&out, 0, sizeof(vec256));

        memcpy(&src1.lo, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
        memcpy(&src1.hi, ymm_upper[v_reg], 16);

        if (mod == 3) {
            memcpy(&src2.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            if (vex_L == 1) {
                memcpy(&src2.hi, ymm_upper[rm_idx], 16);
            }
        } else {
            memcpy(&src2.lo, mem_addr, 16);
            if (vex_L == 1) {
                memcpy(&src2.hi, mem_addr + 16, 16);
            }
        }

        if (vex_pp == 0) {
            math_vshufps(&out, &src1, &src2, imm_val, vex_L);
        } else {
            math_vshufpd(&out, &src1, &src2, imm_val, vex_L);
        }

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        inst->bytes_consumed = expected_len;
        return 1;
    }
    else if ((op == 0x08 || op == 0x09) && vex_pp == 1 && inst->vex_m == 3) { // vroundps / vroundpd
        uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        int expected_len;
        if (mod == 3) {
            expected_len = vex_len + 3;
        } else {
            expected_len = vex_len + 2; // prefix + opcode + ModRM
            if (inst->rm == 4) expected_len += 1; // SIB
            if (mod == 1) expected_len += 1;
            else if (mod == 2) expected_len += 4;
            else if (mod == 0 && inst->rm == 5) expected_len += 4;
            expected_len += 1; // imm
        }
        uint8_t imm_val = rip[expected_len - 1];

        vec256 src, out;
        memset(&src, 0, sizeof(vec256));
        memset(&out, 0, sizeof(vec256));

        if (mod == 3) {
            memcpy(&src.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            if (vex_L == 1) {
                memcpy(&src.hi, ymm_upper[rm_idx], 16);
            }
        } else {
            memcpy(&src.lo, mem_addr, 16);
            if (vex_L == 1) {
                memcpy(&src.hi, mem_addr + 16, 16);
            }
        }

        if (op == 0x08) {
            math_vroundps(&out, &src, imm_val, vex_L);
        } else {
            math_vroundpd(&out, &src, imm_val, vex_L);
        }

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        inst->bytes_consumed = expected_len;
        return 1;
    }
    else if ((op == 0x0A || op == 0x0B) && vex_pp == 1 && inst->vex_m == 3) { // vroundss / vroundsd
        uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        int expected_len;
        if (mod == 3) {
            expected_len = vex_len + 3;
        } else {
            expected_len = vex_len + 2; // prefix + opcode + ModRM
            if (inst->rm == 4) expected_len += 1; // SIB
            if (mod == 1) expected_len += 1;
            else if (mod == 2) expected_len += 4;
            else if (mod == 0 && inst->rm == 5) expected_len += 4;
            expected_len += 1; // imm
        }
        uint8_t imm_val = rip[expected_len - 1];

        vec128 src1, src2, out;
        memset(&src1, 0, sizeof(vec128));
        memset(&src2, 0, sizeof(vec128));
        memset(&out, 0, sizeof(vec128));

        memcpy(&src1, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);

        if (mod == 3) {
            memcpy(&src2, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
        } else {
            memcpy(&src2, mem_addr, (op == 0x0B) ? 8 : 4);
        }

        if (op == 0x0A) {
            math_vroundss(&out, &src1, &src2, imm_val);
        } else {
            math_vroundsd(&out, &src1, &src2, imm_val);
        }

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out, 16);
        memset(ymm_upper[reg_idx], 0, 16);
        inst->bytes_consumed = expected_len;
        return 1;
    }

    if (op == 0x0A || op == 0x0B) {
        char buf[200];
        int len = 0;
        memcpy(buf + len, "[DEBUG_AVX_FLOAT_ROUND] op=0x", 29); len += 29;
        // write byte hex
        char hex_chars[] = "0123456789abcdef";
        buf[len++] = hex_chars[op >> 4];
        buf[len++] = hex_chars[op & 0xf];
        memcpy(buf + len, " pp=", 4); len += 4;
        buf[len++] = '0' + vex_pp;
        memcpy(buf + len, " m=", 3); len += 3;
        buf[len++] = '0' + inst->vex_m;
        buf[len++] = '\n';
        ssize_t rc = write(2, buf, len);
        (void)rc;
    }

    // VINSERTPS — VEX.NDS.LIG.66.0F3A.W0 21 /r ib
    // dst[count_d] = src2[count_s]; lanes in zmask are zeroed.
    // op=0x21, map=3 (0F3A), pp=1 (66)
    if (op == 0x21 && vex_pp == 1 && inst->vex_m == 3) {
        uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        // Calculate instruction length (always xmm-only, L=0)
        int expected_len = vex_len + 2; // prefix + opcode + ModRM
        if (mod != 3) {
            if (inst->rm == 4) expected_len += 1; // SIB
            if (mod == 1) expected_len += 1;
            else if (mod == 2) expected_len += 4;
            else if (mod == 0 && inst->rm == 5) expected_len += 4; // RIP-relative
        }
        expected_len += 1; // trailing imm8
        uint8_t imm8 = rip[expected_len - 1];

        uint8_t count_s = (imm8 >> 6) & 0x3; // source element index
        uint8_t count_d = (imm8 >> 4) & 0x3; // destination element index
        uint8_t zmask   =  imm8        & 0xf; // zero mask

        // Load dst from src1 (v_reg / vvvv field)
        float dst[4];
        memcpy(dst, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);

        // Extract the scalar from src2
        float src2_elem;
        if (mod == 3) {
            float tmp[4];
            memcpy(tmp, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            src2_elem = tmp[count_s];
        } else {
            // Memory operand: only one float is selected by count_s from the 32-bit mem location
            // (Intel spec: mem operand is m32, always selects element [0])
            memcpy(&src2_elem, mem_addr, 4);
            (void)count_s; // count_s ignored for memory operand
        }

        // Insert and apply zmask
        dst[count_d] = src2_elem;
        for (int i = 0; i < 4; i++) {
            if (zmask & (1 << i)) dst[i] = 0.0f;
        }

        // Write to destination (reg_idx), zero upper YMM half (L=0)
        memcpy(&uc->uc_mcontext.fpregs->_xmm[reg_idx], dst, 16);
        memset(ymm_upper[reg_idx], 0, 16);

        inst->bytes_consumed = expected_len;
        return 1;
    }

    // VBLENDVPS — VEX.NDS.LIG.66.0F3A.W0 4A /r /is4
    // dst[i] = (mask[i] < 0) ? src2[i] : src1[i]   (sign bit of mask selects)
    // op=0x4A, map=3 (0F3A), pp=1 (66)
    if (op == 0x4A && vex_pp == 1 && inst->vex_m == 3) {
        uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        int expected_len = vex_len + 2;
        if (mod != 3) {
            if (inst->rm == 4) expected_len += 1;
            if (mod == 1) expected_len += 1;
            else if (mod == 2) expected_len += 4;
            else if (mod == 0 && inst->rm == 5) expected_len += 4;
        }
        expected_len += 1; // is4 byte (encodes mask register in bits [7:4])
        uint8_t is4 = rip[expected_len - 1];
        uint8_t mask_reg = (is4 >> 4) & 0xf;

        int lanes = (vex_L == 1) ? 8 : 4;

        float src1[8], src2[8], mask_f[8], out[8];
        memcpy(&src1[0], &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
        memcpy(&src1[4], ymm_upper[v_reg], 16);

        if (mod == 3) {
            memcpy(&src2[0], &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            memcpy(&src2[4], ymm_upper[rm_idx], 16);
        } else {
            memcpy(&src2[0], mem_addr, lanes * 4);
        }

        memcpy(&mask_f[0], &uc->uc_mcontext.fpregs->_xmm[mask_reg], 16);
        memcpy(&mask_f[4], ymm_upper[mask_reg], 16);

        for (int i = 0; i < lanes; i++) {
            uint32_t m;
            memcpy(&m, &mask_f[i], 4);
            out[i] = (m & 0x80000000u) ? src2[i] : src1[i];
        }

        memcpy(&uc->uc_mcontext.fpregs->_xmm[reg_idx], &out[0], 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out[4], 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }

        inst->bytes_consumed = expected_len;
        return 1;
    }

    else if (op == 0xC2 && inst->vex_m == 1) { // vcmpps / vcmppd / vcmpss / vcmpsd
        uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        int expected_len;
        if (mod == 3) {
            expected_len = vex_len + 3;
        } else {
            expected_len = vex_len + 2; // prefix + opcode + ModRM
            if (inst->rm == 4) expected_len += 1; // SIB
            if (mod == 1) expected_len += 1;
            else if (mod == 2) expected_len += 4;
            else if (mod == 0 && inst->rm == 5) expected_len += 4;
            expected_len += 1; // imm
        }
        uint8_t imm_val = rip[expected_len - 1];

        if (vex_pp == 0 || vex_pp == 1) { // vcmpps / vcmppd (packed)
            vec256 src1, src2, out;
            memset(&src1, 0, sizeof(vec256));
            memset(&src2, 0, sizeof(vec256));
            memset(&out, 0, sizeof(vec256));

            memcpy(&src1.lo, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
            memcpy(&src1.hi, ymm_upper[v_reg], 16);

            if (mod == 3) {
                memcpy(&src2.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
                memcpy(&src2.hi, ymm_upper[rm_idx], 16);
            } else {
                memcpy(&src2.lo, mem_addr, 16);
                if (vex_L == 1) {
                    memcpy(&src2.hi, mem_addr + 16, 16);
                }
            }

            if (vex_pp == 0) {
                math_vcmpps(&out, &src1, &src2, imm_val, vex_L);
            } else {
                math_vcmppd(&out, &src1, &src2, imm_val, vex_L);
            }

            uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            memcpy(dest_xmm, &out.lo, 16);
            if (vex_L == 1) {
                memcpy(ymm_upper[reg_idx], &out.hi, 16);
            } else {
                memset(ymm_upper[reg_idx], 0, 16);
            }
        } else { // vcmpss / vcmpsd (scalar)
            vec128 src1, src2, out;
            memset(&src1, 0, sizeof(vec128));
            memset(&src2, 0, sizeof(vec128));
            memset(&out, 0, sizeof(vec128));

            memcpy(&src1, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);

            if (mod == 3) {
                memcpy(&src2, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            } else {
                memcpy(&src2, mem_addr, (vex_pp == 3) ? 8 : 4);
            }

            if (vex_pp == 2) {
                math_vcmpss(&out, &src1, &src2, imm_val);
            } else {
                math_vcmpsd(&out, &src1, &src2, imm_val);
            }

            uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            memcpy(dest_xmm, &out, 16);
            memset(ymm_upper[reg_idx], 0, 16);
        }

        inst->bytes_consumed = expected_len;
        return 1;
    }

    return 0;
}

