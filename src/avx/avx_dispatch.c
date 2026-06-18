#include "avx_emulator.h"
#include <unistd.h>

static void safe_write_hex(uint64_t val, char *buf, int *idx) {
    char hex_chars[] = "0123456789abcdef";
    for (int i = 15; i >= 0; i--) {
        buf[(*idx)++] = hex_chars[(val >> (i * 4)) & 0xf];
    }
}

static void safe_write_byte_hex(uint8_t val, char *buf, int *idx) {
    char hex_chars[] = "0123456789abcdef";
    buf[(*idx)++] = hex_chars[(val >> 4) & 0xf];
    buf[(*idx)++] = hex_chars[val & 0xf];
}

static void safe_write_dec(uint32_t val, char *buf, int *idx) {
    char temp[10];
    int t_idx = 0;
    if (val == 0) {
        buf[(*idx)++] = '0';
        return;
    }
    while (val > 0) {
        temp[t_idx++] = '0' + (val % 10);
        val /= 10;
    }
    for (int i = t_idx - 1; i >= 0; i--) {
        buf[(*idx)++] = temp[i];
    }
}

int emulate_avx_instruction(ucontext_t *uc, uint8_t *rip, int sig, siginfo_t *si, void *ctx_void, int debug_emu) {
    (void)sig;
    (void)si;
    (void)ctx_void;
    
    struct vex_instruction inst;
    if (!avx_decode(uc, rip, &inst)) {
        return 0; // Not a VEX instruction
    }
    
    if (debug_emu) {
        char buf[200];
        int len = 0;
        memcpy(buf + len, "[DEBUG_AVX_DECODE] RIP=0x", 25); len += 25;
        safe_write_hex((uintptr_t)rip, buf, &len);
        memcpy(buf + len, " Op=0x", 6); len += 6;
        safe_write_byte_hex(inst.op, buf, &len);
        memcpy(buf + len, " L=", 4); len += 4;
        safe_write_dec(inst.vex_L, buf, &len);
        memcpy(buf + len, " pp=", 4); len += 4;
        safe_write_dec(inst.vex_pp, buf, &len);
        memcpy(buf + len, " m=", 3); len += 3;
        safe_write_dec(inst.vex_m, buf, &len);
        buf[len++] = '\n';
        ssize_t rc = write(2, buf, len);
        (void)rc;
    }

    int handled = 0;
    
    handled = avx_emulate_move(uc, &inst);
    if (!handled) handled = avx_emulate_pack(uc, &inst);
    if (!handled) handled = avx_emulate_permute(uc, &inst);
    if (!handled) handled = avx_emulate_integer(uc, &inst);
    if (!handled) handled = avx_emulate_float(uc, &inst);
    if (!handled) handled = avx_emulate_convert(uc, &inst);
    if (!handled) handled = avx_emulate_compare(uc, &inst);
    
    if (handled) {
        if (debug_emu) {
            char buf[200];
            int len = 0;
            
            memcpy(buf + len, "[DEBUG_AVX] RIP=0x", 18); len += 18;
            safe_write_hex((uintptr_t)rip, buf, &len);
            
            memcpy(buf + len, " Op=0x", 6); len += 6;
            safe_write_byte_hex(inst.op, buf, &len);
            
            memcpy(buf + len, " L=", 4); len += 4;
            safe_write_dec(inst.vex_L, buf, &len);
            
            memcpy(buf + len, " pp=", 4); len += 4;
            safe_write_dec(inst.vex_pp, buf, &len);
            
            memcpy(buf + len, " m=", 3); len += 3;
            safe_write_dec(inst.vex_m, buf, &len);
            
            memcpy(buf + len, " reg=", 5); len += 5;
            safe_write_dec(inst.reg_idx, buf, &len);
            
            memcpy(buf + len, " rm=", 4); len += 4;
            safe_write_dec(inst.rm_idx, buf, &len);
            
            memcpy(buf + len, " bytes=", 7); len += 7;
            safe_write_dec(inst.bytes_consumed, buf, &len);
            
            buf[len++] = '\n';
            ssize_t rc = write(2, buf, len);
            (void)rc;
        }
        uc->uc_mcontext.gregs[REG_RIP] += inst.bytes_consumed;
        return 1;
    }
    
    return 0;
}
