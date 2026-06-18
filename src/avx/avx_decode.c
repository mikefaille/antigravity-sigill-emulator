#include "avx_emulator.h"

int avx_decode(ucontext_t *uc, uint8_t *rip, struct vex_instruction *inst) {
    memset(inst, 0, sizeof(*inst));
    
    if (rip[0] == 0xC5) {
        inst->vex_len = 2;
        uint8_t b1 = rip[1];
        inst->vex_R = (b1 >> 7) & 1;
        inst->vex_X = 1;
        inst->vex_B = 1;
        inst->vex_vvvv = (b1 >> 3) & 15;
        inst->vex_L = (b1 >> 2) & 1;
        inst->vex_pp = b1 & 3;
        inst->vex_m = 1; // 0F map
    } else if (rip[0] == 0xC4) {
        inst->vex_len = 3;
        uint8_t b1 = rip[1];
        uint8_t b2 = rip[2];
        inst->vex_R = (b1 >> 7) & 1;
        inst->vex_X = (b1 >> 6) & 1;
        inst->vex_B = (b1 >> 5) & 1;
        inst->vex_m = b1 & 31;
        inst->vex_W = (b2 >> 7) & 1;
        inst->vex_vvvv = (b2 >> 3) & 15;
        inst->vex_L = (b2 >> 2) & 1;
        inst->vex_pp = b2 & 3;
    } else {
        return 0; // Not a VEX instruction
    }
    
    inst->op = rip[inst->vex_len];
    
    if (inst->op == 0x77) { // vzeroupper / vzeroall
        inst->modrm = 0;
        inst->mod = 3;
        inst->reg = 0;
        inst->rm = 0;
        inst->reg_idx = 0;
        inst->rm_idx = 0;
        inst->v_reg = 0;
        inst->mem_addr = NULL;
        inst->mem_bytes = 0;
        inst->bytes_consumed = inst->vex_len + 1;
        return 1;
    }
    
    inst->modrm = rip[inst->vex_len + 1];
    inst->mod = inst->modrm >> 6;
    inst->reg = (inst->modrm >> 3) & 7;
    inst->rm = inst->modrm & 7;
    
    inst->reg_idx = inst->reg + (inst->vex_R == 0 ? 8 : 0);
    inst->rm_idx = inst->rm + (inst->vex_B == 0 ? 8 : 0);
    inst->v_reg = 15 - inst->vex_vvvv;
    
    if (inst->mod != 3) {
        inst->mem_addr = resolve_vex_mem_addr(uc, rip, inst->vex_len, inst->vex_B, inst->vex_X, inst->vex_m, &inst->mem_bytes);
        inst->bytes_consumed = inst->mem_bytes;
    } else {
        inst->mem_addr = NULL;
        inst->mem_bytes = 0;
        inst->bytes_consumed = inst->vex_len + 2;
    }
    
    return 1;
}
