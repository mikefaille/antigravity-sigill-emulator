#include "avx_emulator.h"

int avx_emulate_move(ucontext_t *uc, struct vex_instruction *inst) {
    uint8_t op = inst->op;
    int vex_L = inst->vex_L;
    int vex_pp = inst->vex_pp;
    int vex_W = inst->vex_W;
    int mod = inst->mod;
    uint8_t reg_idx = inst->reg_idx;
    uint8_t rm_idx = inst->rm_idx;
    uint8_t v_reg = inst->v_reg;
    uint8_t *mem_addr = inst->mem_addr;
    int mem_bytes = inst->mem_bytes;
    int vex_len = inst->vex_len;

    if (op == 0x77 && vex_pp == 0) { // vzeroupper / vzeroall
        memset(ymm_upper, 0, sizeof(ymm_upper));
        return 1;
    }

    if ((op == 0x10 || op == 0x11 || op == 0x28 || op == 0x29) && (vex_pp == 0 || vex_pp == 1)) { // vmovups / vmovaps / vmovupd / vmovapd
        if (op == 0x10 || op == 0x28) { // Load
            uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            if (mod == 3) {
                uint8_t *src_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                memcpy(dest_xmm, src_xmm, 16);
                if (vex_L == 1) {
                    memcpy(ymm_upper[reg_idx], ymm_upper[rm_idx], 16);
                } else {
                    memset(ymm_upper[reg_idx], 0, 16);
                }
            } else {
                memcpy(dest_xmm, mem_addr, 16);
                if (vex_L == 1) {
                    memcpy(ymm_upper[reg_idx], mem_addr + 16, 16);
                } else {
                    memset(ymm_upper[reg_idx], 0, 16);
                }
            }
        } else { // Store
            uint8_t *src_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            if (mod == 3) {
                uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                memcpy(dest_xmm, src_xmm, 16);
                if (vex_L == 1) {
                    memcpy(ymm_upper[rm_idx], ymm_upper[reg_idx], 16);
                } else {
                    memset(ymm_upper[rm_idx], 0, 16);
                }
            } else {
                memcpy(mem_addr, src_xmm, 16);
                if (vex_L == 1) {
                    memcpy(mem_addr + 16, ymm_upper[reg_idx], 16);
                }
            }
        }
        return 1;
    }
    else if ((op == 0x10 || op == 0x11) && (vex_pp == 2 || vex_pp == 3)) { // vmovss / vmovsd
        int is_double = (vex_pp == 3);
        int element_size = is_double ? 8 : 4;
        
        if (op == 0x10) { // Load
            uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            if (mod == 3) {
                uint8_t *src_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                uint8_t *src1_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[v_reg];
                memcpy(dest_xmm, src1_xmm, 16);
                memcpy(dest_xmm, src_xmm, element_size);
                memset(ymm_upper[reg_idx], 0, 16);
            } else {
                memset(dest_xmm, 0, 16);
                memcpy(dest_xmm, mem_addr, element_size);
                memset(ymm_upper[reg_idx], 0, 16);
            }
        } else { // Store
            uint8_t *src_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            if (mod == 3) {
                uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                uint8_t *src1_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[v_reg];
                memcpy(dest_xmm, src1_xmm, 16);
                memcpy(dest_xmm, src_xmm, element_size);
                memset(ymm_upper[rm_idx], 0, 16);
            } else {
                memcpy(mem_addr, src_xmm, element_size);
            }
        }
        return 1;
    }
    else if (op == 0x6E && vex_pp == 1) { // vmovd / vmovq xmm, r/m (load)
        int element_size = (vex_W == 1) ? 8 : 4;
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memset(dest_xmm, 0, 16);
        memset(ymm_upper[reg_idx], 0, 16);
        if (mod == 3) {
            uint64_t val = avx_read_gpr(uc, rm_idx);
            memcpy(dest_xmm, &val, element_size);
        } else {
            memcpy(dest_xmm, mem_addr, element_size);
        }
        return 1;
    }
    else if (op == 0x7E && vex_pp == 1) { // vmovd / vmovq r/m, xmm (store)
        int element_size = (vex_W == 1) ? 8 : 4;
        uint8_t *src_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        if (mod == 3) {
            uint64_t val = 0;
            memcpy(&val, src_xmm, element_size);
            avx_write_gpr(uc, rm_idx, val);
        } else {
            memcpy(mem_addr, src_xmm, element_size);
        }
        return 1;
    }
    else if (op == 0xD6 && vex_pp == 1 && inst->vex_m == 1) { // vmovq xmm2/m64, xmm1 (store)
        uint8_t *src_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        if (mod == 3) {
            uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
            memset(dest_xmm, 0, 16);
            memcpy(dest_xmm, src_xmm, 8);
            memset(ymm_upper[rm_idx], 0, 16);
        } else {
            memcpy(mem_addr, src_xmm, 8);
        }
        return 1;
    }
    else if ((op == 0x12 || op == 0x16) && (vex_pp == 0 || vex_pp == 1) && inst->vex_m == 1) { // vmovlps, vmovlpd, vmovhps, vmovhpd, vmovhlps, vmovlhps load
        int is_high = (op == 0x16);
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        uint8_t *src_v_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[v_reg];
        
        uint8_t tmp[16];
        if (is_high) {
            // High quadword load
            memcpy(&tmp[0], &src_v_xmm[0], 8);
            if (mod == 3) {
                // vmovlhps xmm1, xmm2, xmm3
                uint8_t *src_rm_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                memcpy(&tmp[8], &src_rm_xmm[0], 8);
            } else {
                memcpy(&tmp[8], mem_addr, 8);
            }
        } else {
            // Low quadword load
            if (mod == 3) {
                // vmovhlps xmm1, xmm2, xmm3
                uint8_t *src_rm_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                memcpy(&tmp[0], &src_rm_xmm[8], 8);
            } else {
                memcpy(&tmp[0], mem_addr, 8);
            }
            memcpy(&tmp[8], &src_v_xmm[8], 8);
        }
        memcpy(dest_xmm, tmp, 16);
        memset(ymm_upper[reg_idx], 0, 16);
        return 1;
    }
    else if ((op == 0x13 || op == 0x17) && (vex_pp == 0 || vex_pp == 1) && inst->vex_m == 1) { // vmovlps, vmovlpd, vmovhps, vmovhpd store
        int is_high = (op == 0x17);
        uint8_t *src_reg_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        if (mod != 3) {
            if (is_high) {
                memcpy(mem_addr, src_reg_xmm + 8, 8);
            } else {
                memcpy(mem_addr, src_reg_xmm, 8);
            }
        }
        return 1;
    }
    else if (op == 0x7E && vex_pp == 2) { // vmovq xmm1, xmm2/m64 (load)
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memset(dest_xmm, 0, 16);
        memset(ymm_upper[reg_idx], 0, 16);
        if (mod == 3) {
            uint8_t *src_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
            memcpy(dest_xmm, src_xmm, 8);
        } else {
            memcpy(dest_xmm, mem_addr, 8);
        }
        return 1;
    }
    else if ((op == 0x6F || op == 0x7F) && (vex_pp == 1 || vex_pp == 2)) { // vmovdqu / vmovdqa
        if (op == 0x6F) { // Load
            uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            if (mod == 3) {
                uint8_t *src_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                memcpy(dest_xmm, src_xmm, 16);
                if (vex_L == 1) {
                    memcpy(ymm_upper[reg_idx], ymm_upper[rm_idx], 16);
                } else {
                    memset(ymm_upper[reg_idx], 0, 16);
                }
            } else {
                memcpy(dest_xmm, mem_addr, 16);
                if (vex_L == 1) {
                    memcpy(ymm_upper[reg_idx], mem_addr + 16, 16);
                } else {
                    memset(ymm_upper[reg_idx], 0, 16);
                }
            }
        } else { // Store
            uint8_t *src_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            if (mod == 3) {
                uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                memcpy(dest_xmm, src_xmm, 16);
                if (vex_L == 1) {
                    memcpy(ymm_upper[rm_idx], ymm_upper[reg_idx], 16);
                } else {
                    memset(ymm_upper[rm_idx], 0, 16);
                }
            } else {
                memcpy(mem_addr, src_xmm, 16);
                if (vex_L == 1) {
                    memcpy(mem_addr + 16, ymm_upper[reg_idx], 16);
                }
            }
        }
        return 1;
    }
    else if (op == 0x12 && vex_pp == 3) { // vmovddup
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        uint64_t val = 0;
        if (mod == 3) {
            uint8_t *src_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
            memcpy(&val, src_xmm, 8);
        } else {
            memcpy(&val, mem_addr, 8);
        }
        
        memcpy(dest_xmm, &val, 8);
        memcpy(dest_xmm + 8, &val, 8);
        
        if (vex_L == 1) {
            uint64_t val2 = 0;
            if (mod == 3) {
                memcpy(&val2, ymm_upper[rm_idx], 8);
            } else {
                memcpy(&val2, mem_addr + 16, 8);
            }
            memcpy(ymm_upper[reg_idx], &val2, 8);
            memcpy(ymm_upper[reg_idx] + 8, &val2, 8);
        } else {
            memset(ymm_upper[reg_idx], 0, 16);
        }
        inst->bytes_consumed = (mod == 3) ? (vex_len + 2) : mem_bytes;
        return 1;
    }
    else if (op == 0x17 && inst->vex_m == 3) { // vextractps r32/m32, xmm, imm8
        uint8_t *rip_ptr = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
        uint8_t imm = rip_ptr[mod == 3 ? (vex_len + 2) : mem_bytes];

        float *src = (float *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        uint32_t val;
        memcpy(&val, &src[imm & 3], 4);

        if (mod == 3) {
            avx_write_gpr(uc, rm_idx, val);
        } else {
            memcpy(mem_addr, &val, 4);
        }

        inst->bytes_consumed = (mod == 3) ? (vex_len + 3) : (mem_bytes + 1);
        return 1;
    }

    return 0;
}
