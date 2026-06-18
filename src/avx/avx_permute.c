#include "avx_emulator.h"

int avx_emulate_permute(ucontext_t *uc, struct vex_instruction *inst) {
    uint8_t op = inst->op;
    int vex_pp = inst->vex_pp;
    int mod = inst->mod;
    uint8_t reg_idx = inst->reg_idx;
    uint8_t rm_idx = inst->rm_idx;
    uint8_t v_reg = inst->v_reg;
    uint8_t *mem_addr = inst->mem_addr;
    int vex_len = inst->vex_len;

    if (op == 0x00 && vex_pp == 1 && inst->vex_m == 3) { // vpermq
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
            memcpy(&src.hi, ymm_upper[rm_idx], 16);
        } else {
            memcpy(&src.lo, mem_addr, 16);
            memcpy(&src.hi, mem_addr + 16, 16);
        }
        
        math_vpermq(&out, &src, imm_val);
        
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        memcpy(ymm_upper[reg_idx], &out.hi, 16);
        
        inst->bytes_consumed = expected_len;
        return 1;
    }
    else if ((op == 0x19 || op == 0x39) && vex_pp == 1 && inst->vex_m == 3) { // vextractf128 / vextracti128
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

        vec256 src;
        memcpy(&src.lo, &uc->uc_mcontext.fpregs->_xmm[reg_idx], 16);
        memcpy(&src.hi, ymm_upper[reg_idx], 16);

        vec128 out;
        memset(&out, 0, sizeof(vec128));

        math_vextract128(&out, &src, imm_val);
        
        if (mod == 3) {
            uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
            memcpy(dest_xmm, &out, 16);
            memset(ymm_upper[rm_idx], 0, 16);
        } else {
            memcpy(mem_addr, &out, 16);
        }
        inst->bytes_consumed = expected_len;
        return 1;
    }
    else if ((op == 0x18 || op == 0x38) && vex_pp == 1 && inst->vex_m == 3) { // vinsertf128 / vinserti128
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

        vec256 src1, out;
        memcpy(&src1.lo, &uc->uc_mcontext.fpregs->_xmm[v_reg], 16);
        memcpy(&src1.hi, ymm_upper[v_reg], 16);
        
        vec128 src2;
        if (mod == 3) {
            memcpy(&src2, &uc->uc_mcontext.fpregs->_xmm[rm_idx], 16);
        } else {
            memcpy(&src2, mem_addr, 16);
        }
        
        memset(&out, 0, sizeof(vec256));

        math_vinsert128(&out, &src1, &src2, imm_val);
        
        uint8_t *dest_xmm = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
        memcpy(dest_xmm, &out.lo, 16);
        memcpy(ymm_upper[reg_idx], &out.hi, 16);
        
        inst->bytes_consumed = expected_len;
        return 1;
    }

    return 0;
}
