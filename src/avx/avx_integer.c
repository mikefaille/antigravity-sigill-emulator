#include "avx_emulator.h"

int avx_emulate_integer(ucontext_t *uc, struct vex_instruction *inst) {
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

    if ((op == 0xEF || op == 0xDB || op == 0xEB || op == 0xDF) && vex_pp == 1) { // vpxor / vpand / vpor / vpandn
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

        vec256 a, b, out;
        memcpy(&a.lo, src1_xmm, 16);
        memcpy(&a.hi, src1_ymm_u, 16);
        memcpy(&b.lo, src2_xmm, 16);
        memcpy(&b.hi, src2_ymm_u, 16);
        memset(&out, 0, sizeof(vec256));

        if (op == 0xEF) math_vpxor(&out, &a, &b);
        else if (op == 0xDB) math_vpand(&out, &a, &b);
        else if (op == 0xEB) math_vpor(&out, &a, &b);
        else if (op == 0xDF) math_vpandn(&out, &a, &b);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x78 || op == 0x79 || op == 0x58 || op == 0x59) && vex_pp == 1 && inst->vex_m == 2) { // vpbroadcastb/w/d/q
        vec256 src, out;
        memset(&src, 0, sizeof(vec256));
        memset(&out, 0, sizeof(vec256));

        int src_bytes = 1;
        if (op == 0x79) src_bytes = 2;
        else if (op == 0x58) src_bytes = 4;
        else if (op == 0x59) src_bytes = 8;

        if (mod == 3) {
            memcpy(&src.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
        } else {
            memcpy(&src.lo, mem_addr, src_bytes);
        }

        if (op == 0x78) math_vpbroadcastb(&out, &src, vex_L);
        else if (op == 0x79) math_vpbroadcastw(&out, &src, vex_L);
        else if (op == 0x58) math_vpbroadcastd(&out, &src, vex_L);
        else if (op == 0x59) math_vpbroadcastq(&out, &src, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if (op == 0x30 && vex_pp == 1 && inst->vex_m == 2) { // vpmovzxbw
        vec256 src, out;
        memset(&src, 0, sizeof(vec256));
        memset(&out, 0, sizeof(vec256));

        if (mod == 3) {
            memcpy(&src.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
        } else {
            memcpy(&src.lo, mem_addr, vex_L ? 16 : 8);
        }

        math_vpmovzxbw(&out, &src, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x21 || op == 0x22 || op == 0x23 || op == 0x24 || op == 0x25 ||
              op == 0x31 || op == 0x32 || op == 0x33 || op == 0x34 || op == 0x35) &&
             vex_pp == 1 && inst->vex_m == 2) { 
        // vpmovsxbd (21) / vpmovsxbq (22) / vpmovsxwd (23) / vpmovsxwq (24) / vpmovsxdq (25)
        // vpmovzxbd (31) / vpmovzxbq (32) / vpmovzxwd (33) / vpmovzxwq (34) / vpmovzxdq (35)
        vec256 src, out;
        memset(&src, 0, sizeof(vec256));
        memset(&out, 0, sizeof(vec256));

        int mem_bytes = 16;
        if (op == 0x25 || op == 0x35) { // vpmovsxdq / vpmovzxdq
            mem_bytes = vex_L ? 16 : 8;
        } else if (op == 0x21 || op == 0x31) { // vpmovsxbd / vpmovzxbd
            mem_bytes = vex_L ? 8 : 4;
        } else if (op == 0x22 || op == 0x32) { // vpmovsxbq / vpmovzxbq
            mem_bytes = vex_L ? 4 : 2;
        } else if (op == 0x24 || op == 0x34) { // vpmovsxwq / vpmovzxwq
            mem_bytes = vex_L ? 8 : 4;
        } else if (op == 0x23 || op == 0x33) { // vpmovsxwd / vpmovzxwd
            mem_bytes = vex_L ? 16 : 8;
        }

        if (mod == 3) {
            memcpy(&src.lo, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
        } else {
            memcpy(&src.lo, mem_addr, mem_bytes);
        }

        if (op == 0x21) math_vpmovsxbd(&out, &src, vex_L);
        else if (op == 0x22) math_vpmovsxbq(&out, &src, vex_L);
        else if (op == 0x23) math_vpmovsxwd(&out, &src, vex_L);
        else if (op == 0x24) math_vpmovsxwq(&out, &src, vex_L);
        else if (op == 0x25) math_vpmovsxdq(&out, &src, vex_L);
        else if (op == 0x31) math_vpmovzxbd(&out, &src, vex_L);
        else if (op == 0x32) math_vpmovzxbq(&out, &src, vex_L);
        else if (op == 0x33) math_vpmovzxwd(&out, &src, vex_L);
        else if (op == 0x34) math_vpmovzxwq(&out, &src, vex_L);
        else if (op == 0x35) math_vpmovzxdq(&out, &src, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x39 || op == 0x3B || op == 0x3D || op == 0x3F) && vex_pp == 1 && inst->vex_m == 2) { // vpminsd / vpminud / vpmaxsd / vpmaxud
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

        if (op == 0x39) math_vpminsd(&out, &src1, &src2, vex_L);
        else if (op == 0x3B) math_vpminud(&out, &src1, &src2, vex_L);
        else if (op == 0x3D) math_vpmaxsd(&out, &src1, &src2, vex_L);
        else if (op == 0x3F) math_vpmaxud(&out, &src1, &src2, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if (op == 0x40 && vex_pp == 1 && inst->vex_m == 2) { // vpmulld
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

        math_vpmulld(&out, &src1, &src2, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if (op == 0x28 && vex_pp == 1 && inst->vex_m == 2) { // vpmuldq
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

        math_vpmuldq(&out, &src1, &src2, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if (op == 0xF4 && vex_pp == 1 && inst->vex_m == 1) { // vpmuludq
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

        math_vpmuludq(&out, &src1, &src2, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x45 || op == 0x47) && vex_pp == 1 && inst->vex_m == 2) { // vpsllvd / vpsllvq / vpsrlvd / vpsrlvq
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

        if (op == 0x47) {
            if (inst->vex_W == 1) {
                math_vpsllvq(&out, &src1, &src2, vex_L);
            } else {
                math_vpsllvd(&out, &src1, &src2, vex_L);
            }
        } else {
            if (inst->vex_W == 1) {
                math_vpsrlvq(&out, &src1, &src2, vex_L);
            } else {
                math_vpsrlvd(&out, &src1, &src2, vex_L);
            }
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
    else if (op == 0xD5 && vex_pp == 1 && inst->vex_m == 1) { // vpmullw
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

        math_vpmullw(&out, &src1, &src2, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x20 || op == 0x22) && vex_pp == 1 && inst->vex_m == 3) { // vpinsrb / vpinsrd / vpinsrq
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

        vec128 src1;
        memcpy(&src1, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);

        uint64_t val = 0;
        if (mod == 3) {
            val = avx_read_gpr(uc, rm_idx);
        } else {
            int src_bytes = (op == 0x20) ? 1 : ((inst->vex_W == 1) ? 8 : 4);
            memcpy(&val, mem_addr, src_bytes);
        }

        vec128 out;
        memset(&out, 0, sizeof(vec128));

        if (op == 0x20) {
            math_vpinsrb(&out, &src1, (uint32_t)val, imm_val);
        } else {
            if (inst->vex_W == 1) {
                math_vpinsrq(&out, &src1, val, imm_val);
            } else {
                math_vpinsrd(&out, &src1, (uint32_t)val, imm_val);
            }
        }

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out, 16);
        memset(ymm_upper[reg_idx], 0, 16); // vpinsr always inserts into XMM and zeroes upper half of YMM
        inst->bytes_consumed = expected_len;
        return 1;
    }
    else if ((op == 0xFC || op == 0xFD || op == 0xFE || op == 0xD4 || op == 0xFA) && vex_pp == 1 && inst->vex_m == 1) { // vpaddb / vpaddw / vpaddd / vpaddq / vpsubd
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

        if (op == 0xFC) math_vpaddb(&out, &src1, &src2, vex_L);
        else if (op == 0xFD) math_vpaddw(&out, &src1, &src2, vex_L);
        else if (op == 0xFE) math_vpaddd(&out, &src1, &src2, vex_L);
        else if (op == 0xD4) math_vpaddq(&out, &src1, &src2, vex_L);
        else if (op == 0xFA) {
            // vpsubd: dst = src1 - src2 (32-bit integer subtract)
            for (int i = 0; i < 4; i++) {
                out.lo.i32[i] = src1.lo.i32[i] - src2.lo.i32[i];
            }
            if (vex_L == 1) {
                for (int i = 0; i < 4; i++) {
                    out.hi.i32[i] = src1.hi.i32[i] - src2.hi.i32[i];
                }
            }
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
    else if ((op == 0x74 || op == 0x75 || op == 0x76) && vex_pp == 1 && inst->vex_m == 1) { // vpcmpeqb / vpcmpeqw / vpcmpeqd
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

        if (op == 0x74) math_vpcmpeqb(&out, &src1, &src2, vex_L);
        else if (op == 0x75) math_vpcmpeqw(&out, &src1, &src2, vex_L);
        else if (op == 0x76) math_vpcmpeqd(&out, &src1, &src2, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x64 || op == 0x65 || op == 0x66) && vex_pp == 1 && inst->vex_m == 1) { // vpcmpgtb / vpcmpgtw / vpcmpgtd
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

        if (op == 0x64) math_vpcmpgtb(&out, &src1, &src2, vex_L);
        else if (op == 0x65) math_vpcmpgtw(&out, &src1, &src2, vex_L);
        else if (op == 0x66) math_vpcmpgtd(&out, &src1, &src2, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if (op == 0x37 && vex_pp == 1 && inst->vex_m == 2) { // vpcmpgtq
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

        math_vpcmpgtq(&out, &src1, &src2, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x71 || op == 0x72 || op == 0x73) && vex_pp == 1) { // Shift by immediate (vpsllw/d/q, vpsrlw/d/q, vpsraw/d)
        uint8_t modrm = inst->modrm;
        uint8_t ext_op = (modrm >> 3) & 7;
        uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        uint8_t imm = rip[mod == 3 ? (vex_len + 2) : mem_bytes];

        if (ext_op == 2 || ext_op == 4 || ext_op == 6) {
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

            if (op == 0x71) {
                if (ext_op == 2) math_vpsrlw(&out, &src, imm);
                else if (ext_op == 4) math_vpsraw(&out, &src, imm);
                else if (ext_op == 6) math_vpsllw(&out, &src, imm);
            } else if (op == 0x72) {
                if (ext_op == 2) math_vpsrld(&out, &src, imm);
                else if (ext_op == 4) math_vpsrad(&out, &src, imm);
                else if (ext_op == 6) math_vpslld(&out, &src, imm);
            } else if (op == 0x73) {
                if (ext_op == 2) math_vpsrlq(&out, &src, imm);
                else if (ext_op == 6) math_vpsllq(&out, &src, imm);
                else return 0;
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
    }
    else if (op == 0x70 && vex_pp == 1 && inst->vex_m == 1) { // vpshufd
        uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        uint8_t imm = rip[mod == 3 ? (vex_len + 2) : mem_bytes];

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

        math_vpshufd(&out, &src, imm, vex_L);

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
    else if (op == 0x00 && vex_pp == 1 && inst->vex_m == 2) { // vpshufb (map 0F38, so vex_m == 2)
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

        vec256 a, b, out;
        memcpy(&a.lo, src1_xmm, 16);
        memcpy(&a.hi, src1_ymm_u, 16);
        memcpy(&b.lo, src2_xmm, 16);
        memcpy(&b.hi, src2_ymm_u, 16);
        memset(&out, 0, sizeof(vec256));

        math_vpshufb(&out, &a, &b, vex_L);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if ((op == 0x6C || op == 0x6D) && vex_pp == 1 && inst->vex_m == 1) { // vpunpcklqdq / vpunpckhqdq
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

        vec256 a, b, out;
        memcpy(&a.lo, src1_xmm, 16);
        memcpy(&a.hi, src1_ymm_u, 16);
        memcpy(&b.lo, src2_xmm, 16);
        memcpy(&b.hi, src2_ymm_u, 16);
        memset(&out, 0, sizeof(vec256));

        if (op == 0x6C) {
            math_vpunpcklqdq(&out, &a, &b);
        } else {
            math_vpunpckhqdq(&out, &a, &b);
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
    else if (op == 0x29 && vex_pp == 1 && inst->vex_m == 2) { // vpcmpeqq
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

        vec256 a, b, out;
        memcpy(&a.lo, src1_xmm, 16);
        memcpy(&a.hi, src1_ymm_u, 16);
        memcpy(&b.lo, src2_xmm, 16);
        memcpy(&b.hi, src2_ymm_u, 16);
        memset(&out, 0, sizeof(vec256));

        math_vpcmpeqq(&out, &a, &b);

        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        if (vex_L == 1) {
            memcpy(ymm_upper[reg_idx], &out.hi, 16);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        return 1;
    }
    else if (op == 0xD7 && vex_pp == 1 && inst->vex_m == 1) { // vpmovmskb r32, ymm/xmm
        vec256 src;
        memset(&src, 0, sizeof(vec256));
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

        uint32_t res = math_vpmovmskb(&src, vex_L);

        // Write result to the general-purpose register
        avx_write_gpr(uc, reg_idx, (uint64_t)res);
        return 1;
    }

    return 0;
}
