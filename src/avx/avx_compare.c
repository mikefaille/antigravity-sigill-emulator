#include "avx_emulator.h"

static int eval_cmp_pred(int pred, double a, double b, int is_nan_a, int is_nan_b) {
    int unordered = (is_nan_a || is_nan_b);
    switch (pred & 31) {
        case 0:  case 16: return !unordered && (a == b); // EQ_OQ / EQ_OS
        case 1:  case 17: return !unordered && (a < b);  // LT_OS / LT_OQ
        case 2:  case 18: return !unordered && (a <= b); // LE_OS / LE_OQ
        case 3:  case 19: return unordered;              // UNORD_Q / UNORD_S
        case 4:  case 20: return unordered || (a != b);  // NEQ_UQ / NEQ_US
        case 5:  case 21: return unordered || (a >= b);  // NLT_US / NLT_UQ
        case 6:  case 22: return unordered || (a > b);   // NLE_US / NLE_UQ
        case 7:  case 23: return !unordered;             // ORD_Q / ORD_S
        case 8:  case 24: return unordered || (a == b);  // EQ_UQ / EQ_US
        case 9:  case 25: return !unordered && (a >= b); // NGE_US / NGE_UQ
        case 10: case 26: return !unordered && (a > b);  // NGT_US / NGT_UQ
        case 11: case 27: return 0;                      // FALSE_OQ / FALSE_OS
        case 12: case 28: return !unordered && (a != b); // NEQ_OQ / NEQ_OS
        case 13: case 29: return unordered || (a < b);   // GE_OS / GE_OQ
        case 14: case 30: return unordered || (a <= b);  // GT_OS / GT_OQ
        case 15: case 31: return 1;                      // TRUE_UQ / TRUE_US
    }
    return 0;
}

int avx_emulate_compare(ucontext_t *uc, struct vex_instruction *inst) {
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

    if ((op == 0x2E || op == 0x2F) && (vex_pp == 0 || vex_pp == 1)) { // vucomiss / vucomisd / vcomiss / vcomisd
        int is_double = (vex_pp == 1);
        uint64_t rflags = uc->uc_mcontext.gregs[REG_EFL];
        rflags &= ~((1ULL << 6) | (1ULL << 2) | (1ULL << 0) | (1ULL << 4) | (1ULL << 7) | (1ULL << 11));
        
        vec128 a, b;
        memcpy(&a, &uc->uc_mcontext.fpregs->_xmm[reg_idx], 16);
        if (mod == 3) {
            memcpy(&b, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
        } else {
            memcpy(&b, mem_addr, is_double ? 8 : 4);
        }

        uint64_t temp_flags = 0;
        if (is_double) {
            temp_flags = math_ucomisd(&a, &b);
        } else {
            temp_flags = math_ucomiss(&a, &b);
        }
        
        uint64_t mask = (1ULL << 0) | (1ULL << 2) | (1ULL << 6);
        rflags |= (temp_flags & mask);
        uc->uc_mcontext.gregs[REG_EFL] = rflags;
        return 1;
    }
    else if (op == 0x02 && vex_pp == 1 && inst->vex_m == 3) { // vpblendd
        uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        uint8_t imm = rip[mod == 3 ? (vex_len + 2) : mem_bytes];

        vec256 a, b, out;
        memcpy(&a.lo, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
        memcpy(&a.hi, ymm_upper[v_reg], 16);
        
        if (mod == 3) {
            memcpy(&b.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
            memcpy(&b.hi, ymm_upper[rm_idx], 16);
        } else {
            memcpy(&b.lo, mem_addr, 16);
            memcpy(&b.hi, mem_addr + 16, 16);
        }
        
        memset(&out, 0, sizeof(vec256));

        // Use canonical float blendps-equivalent loop
        for (int i = 0; i < 8; i++) {
            int select_src2 = (imm >> i) & 1;
            if (i < 4) {
                out.lo.u32[i] = select_src2 ? b.lo.u32[i] : a.lo.u32[i];
            } else {
                out.hi.u32[i - 4] = select_src2 ? b.hi.u32[i - 4] : a.hi.u32[i - 4];
            }
        }
        
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        inst->bytes_consumed = (mod == 3) ? (vex_len + 3) : (mem_bytes + 1);
        return 1;
    }
    else if ((op == 0x0E || op == 0x0F) && vex_pp == 1 && inst->vex_m == 2) { // vtestps / vtestpd
        vec256 src1, src2;
        memset(&src1, 0, sizeof(vec256));
        memset(&src2, 0, sizeof(vec256));

        // First operand (src1) is reg_idx
        memcpy(&src1.lo, &uc->uc_mcontext.fpregs->_xmm[reg_idx], 16);
        if (vex_L == 1) {
            memcpy(&src1.hi, ymm_upper[reg_idx], 16);
        }

        // Second operand (src2) is rm_idx or mem_addr
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

        uint64_t res_flags = math_vtest(&src1, &src2, vex_L);

        // Merge ZF and CF into context EFLAGS
        uint64_t rflags = uc->uc_mcontext.gregs[REG_EFL];
        rflags &= ~((1ULL << 6) | (1ULL << 0) | (1ULL << 2) | (1ULL << 4) | (1ULL << 7) | (1ULL << 11)); // clear ZF, CF, PF, AF, SF, OF
        rflags |= res_flags;
        uc->uc_mcontext.gregs[REG_EFL] = rflags;
        return 1;
    }
    else if (op == 0xC2) { // vcmpps, vcmppd, vcmpss, vcmpsd
        // The immediate is at the end of the instruction
        uint8_t *rip_ptr = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        uint8_t imm = rip_ptr[mod == 3 ? (vex_len + 2) : mem_bytes];
        
        int is_double = (vex_pp == 1 || vex_pp == 3);
        int is_scalar = (vex_pp == 2 || vex_pp == 3);

        if (is_scalar) {
            // Scalar compare: vcmpss or vcmpsd
            uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            uint8_t *src_v_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[v_reg];
            
            uint8_t tmp[16];
            // Copy upper elements from src_v_xmm
            memcpy(tmp, src_v_xmm, 16);
            
            if (is_double) {
                double a, b;
                memcpy(&a, src_v_xmm, 8);
                if (mod == 3) {
                    memcpy(&b, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 8);
                } else {
                    memcpy(&b, mem_addr, 8);
                }
                uint64_t mask = eval_cmp_pred(imm, a, b, a != a, b != b) ? ~0ULL : 0ULL;
                memcpy(&tmp[0], &mask, 8);
            } else {
                float a, b;
                memcpy(&a, src_v_xmm, 4);
                if (mod == 3) {
                    memcpy(&b, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 4);
                } else {
                    memcpy(&b, mem_addr, 4);
                }
                uint32_t mask = eval_cmp_pred(imm, a, b, a != a, b != b) ? ~0U : 0U;
                memcpy(&tmp[0], &mask, 4);
            }
            
            memcpy(dest_xmm, tmp, 16);
            memset(ymm_upper[reg_idx], 0, 16);
        } else {
            // Packed compare: vcmpps or vcmppd
            int num_elements = vex_L ? (is_double ? 4 : 8) : (is_double ? 2 : 4);
            uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            
            vec256 a, b, out;
            memset(&a, 0, sizeof(vec256));
            memset(&b, 0, sizeof(vec256));
            memset(&out, 0, sizeof(vec256));
            
            memcpy(&a.lo, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
            if (vex_L) memcpy(&a.hi, ymm_upper[v_reg], 16);
            
            if (mod == 3) {
                memcpy(&b.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
                if (vex_L) memcpy(&b.hi, ymm_upper[rm_idx], 16);
            } else {
                memcpy(&b.lo, mem_addr, 16);
                if (vex_L) memcpy(&b.hi, mem_addr + 16, 16);
            }
            
            if (is_double) {
                double *ap = (double *)&a;
                double *bp = (double *)&b;
                uint64_t *outp = (uint64_t *)&out;
                for (int i = 0; i < num_elements; i++) {
                    outp[i] = eval_cmp_pred(imm, ap[i], bp[i], ap[i] != ap[i], bp[i] != bp[i]) ? ~0ULL : 0ULL;
                }
            } else {
                float *ap = (float *)&a;
                float *bp = (float *)&b;
                uint32_t *outp = (uint32_t *)&out;
                for (int i = 0; i < num_elements; i++) {
                    outp[i] = eval_cmp_pred(imm, ap[i], bp[i], ap[i] != ap[i], bp[i] != bp[i]) ? ~0U : 0U;
                }
            }
            
            memcpy(dest_xmm, &out.lo, 16);
            if (vex_L) {
                memcpy(ymm_upper[reg_idx], &out.hi, 16);
            } else {
                memset(ymm_upper[reg_idx], 0, 16);
            }
        }
        inst->bytes_consumed = (mod == 3) ? (vex_len + 3) : (mem_bytes + 1);
        return 1;
    }

    return 0;
}
