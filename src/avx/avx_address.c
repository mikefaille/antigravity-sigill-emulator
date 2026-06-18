#include "avx_emulator.h"

uint8_t *resolve_vex_mem_addr(ucontext_t *uc, uint8_t *rip, int vex_len, int vex_B, int vex_X, int vex_m, int *bytes_consumed_out) {
    uint8_t *modrm_ptr = rip + vex_len + 1;
    uint8_t modrm = *modrm_ptr;
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    uint8_t *ptr = modrm_ptr + 1;
    
    uint8_t base_reg = 255;
    uint8_t index_reg = 255;
    uint8_t scale = 0;
    int32_t disp = 0;
    int is_rip_relative = 0;
    
    if (rm == 4) {
        uint8_t sib = *ptr++;
        uint8_t ss = sib >> 6;
        uint8_t index = (sib >> 3) & 7;
        uint8_t base = sib & 7;
        
        scale = ss;
        if (index != 4) {
            index_reg = index + (vex_X == 0 ? 8 : 0);
        }
        if (base == 5 && mod == 0) {
            memcpy(&disp, ptr, 4);
            ptr += 4;
        } else {
            base_reg = base + (vex_B == 0 ? 8 : 0);
        }
    } else {
        if (mod == 0 && rm == 5) {
            memcpy(&disp, ptr, 4);
            ptr += 4;
            is_rip_relative = 1;
        } else {
            base_reg = rm + (vex_B == 0 ? 8 : 0);
        }
    }
    
    if (mod == 1) {
        int8_t disp8 = (int8_t)*ptr++;
        disp += disp8;
    } else if (mod == 2) {
        int32_t disp32;
        memcpy(&disp32, ptr, 4);
        ptr += 4;
        disp += disp32;
    }
    
    *bytes_consumed_out = (ptr - rip);
    
    if (is_rip_relative) {
        int has_imm = (vex_m == 3) || (vex_m == 1 && rip[vex_len] == 0xC6);
        int total_len = *bytes_consumed_out + (has_imm ? 1 : 0);
        *bytes_consumed_out = total_len;
        return rip + total_len + disp;
    }
    
    uint64_t base_val = 0;
    if (base_reg != 255) {
        base_val = avx_read_gpr(uc, base_reg);
    }
    
    uint64_t index_val = 0;
    if (index_reg != 255) {
        index_val = avx_read_gpr(uc, index_reg);
    }
    
    int has_imm = (vex_m == 3) || (vex_m == 1 && rip[vex_len] == 0xC6);
    if (has_imm) {
        *bytes_consumed_out += 1;
    }
    
    return (uint8_t *)(base_val + (index_val << scale) + disp);
}
