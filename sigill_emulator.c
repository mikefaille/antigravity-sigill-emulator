#define _GNU_SOURCE
#include <signal.h>
#include <ucontext.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct sigaction old_sa;
static int debug_emu = 0;

// AES Tables and S-Box definitions
static const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t inv_sbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

static inline uint8_t gmul(uint8_t x, uint8_t y) {
    uint8_t p = 0;
    for (int i = 0; i < 8; i++) {
        if (y & 1) p ^= x;
        uint8_t carry = x & 0x80;
        x <<= 1;
        if (carry) x ^= 0x1B;
        y >>= 1;
    }
    return p;
}

static inline uint8_t gmul2(uint8_t x) {
    return (x & 0x80) ? ((x << 1) ^ 0x1B) : (x << 1);
}

static inline uint8_t gmul3(uint8_t x) {
    return gmul2(x) ^ x;
}

static void mix_columns(uint8_t *state) {
    for (int i = 0; i < 4; i++) {
        uint8_t s0 = state[0 + i*4];
        uint8_t s1 = state[1 + i*4];
        uint8_t s2 = state[2 + i*4];
        uint8_t s3 = state[3 + i*4];
        
        state[0 + i*4] = gmul2(s0) ^ gmul3(s1) ^ s2 ^ s3;
        state[1 + i*4] = s0 ^ gmul2(s1) ^ gmul3(s2) ^ s3;
        state[2 + i*4] = s0 ^ s1 ^ gmul2(s2) ^ gmul3(s3);
        state[3 + i*4] = gmul3(s0) ^ s1 ^ s2 ^ gmul2(s3);
    }
}

static void inv_mix_columns(uint8_t *state) {
    for (int i = 0; i < 4; i++) {
        uint8_t s0 = state[0 + i*4];
        uint8_t s1 = state[1 + i*4];
        uint8_t s2 = state[2 + i*4];
        uint8_t s3 = state[3 + i*4];
        
        state[0 + i*4] = gmul(s0, 0x0e) ^ gmul(s1, 0x0b) ^ gmul(s2, 0x0d) ^ gmul(s3, 0x09);
        state[1 + i*4] = gmul(s0, 0x09) ^ gmul(s1, 0x0e) ^ gmul(s2, 0x0b) ^ gmul(s3, 0x0d);
        state[2 + i*4] = gmul(s0, 0x0d) ^ gmul(s1, 0x09) ^ gmul(s2, 0x0e) ^ gmul(s3, 0x0b);
        state[3 + i*4] = gmul(s0, 0x0b) ^ gmul(s1, 0x0d) ^ gmul(s2, 0x09) ^ gmul(s3, 0x0e);
    }
}

static void shift_rows(uint8_t *state) {
    uint8_t tmp;
    tmp = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = tmp;
    tmp = state[2]; state[2] = state[10]; state[10] = tmp;
    tmp = state[6]; state[6] = state[14]; state[14] = tmp;
    tmp = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = state[3]; state[3] = tmp;
}

static void inv_shift_rows(uint8_t *state) {
    uint8_t tmp;
    tmp = state[13]; state[13] = state[9]; state[9] = state[5]; state[5] = state[1]; state[1] = tmp;
    tmp = state[2]; state[2] = state[10]; state[10] = tmp;
    tmp = state[6]; state[6] = state[14]; state[14] = tmp;
    tmp = state[3]; state[3] = state[7]; state[7] = state[11]; state[11] = state[15]; state[15] = tmp;
}

static void sub_bytes(uint8_t *state) {
    for (int i = 0; i < 16; i++) state[i] = sbox[state[i]];
}

static void inv_sub_bytes(uint8_t *state) {
    for (int i = 0; i < 16; i++) state[i] = inv_sbox[state[i]];
}

static void emulate_aesdec(uint8_t *dest, const uint8_t *key) {
    uint8_t tmp[16];
    memcpy(tmp, dest, 16);
    inv_shift_rows(tmp);
    inv_sub_bytes(tmp);
    inv_mix_columns(tmp);
    for (int i = 0; i < 16; i++) dest[i] = tmp[i] ^ key[i];
}

static void emulate_aesdeclast(uint8_t *dest, const uint8_t *key) {
    uint8_t tmp[16];
    memcpy(tmp, dest, 16);
    inv_shift_rows(tmp);
    inv_sub_bytes(tmp);
    for (int i = 0; i < 16; i++) dest[i] = tmp[i] ^ key[i];
}

static void emulate_aesenc(uint8_t *dest, const uint8_t *key) {
    uint8_t tmp[16];
    memcpy(tmp, dest, 16);
    shift_rows(tmp);
    sub_bytes(tmp);
    mix_columns(tmp);
    for (int i = 0; i < 16; i++) dest[i] = tmp[i] ^ key[i];
}

static void emulate_aesenclast(uint8_t *dest, const uint8_t *key) {
    uint8_t tmp[16];
    memcpy(tmp, dest, 16);
    shift_rows(tmp);
    sub_bytes(tmp);
    for (int i = 0; i < 16; i++) dest[i] = tmp[i] ^ key[i];
}

static void emulate_aesimc(uint8_t *dest, const uint8_t *src) {
    uint8_t tmp[16];
    memcpy(tmp, src, 16);
    inv_mix_columns(tmp);
    memcpy(dest, tmp, 16);
}

static void emulate_pclmulqdq(uint8_t *dest, const uint8_t *src, uint8_t imm) {
    uint64_t a, b;
    if (imm & 1) memcpy(&a, dest + 8, 8);
    else memcpy(&a, dest, 8);
    
    if (imm & 16) memcpy(&b, src + 8, 8);
    else memcpy(&b, src, 8);
    
    uint64_t low = 0, high = 0;
    for (int i = 0; i < 64; i++) {
        if ((b >> i) & 1) {
            if (i == 0) {
                low ^= a;
            } else {
                low ^= (a << i);
                high ^= (a >> (64 - i));
            }
        }
    }
    memcpy(dest, &low, 8);
    memcpy(dest + 8, &high, 8);
}

static void safe_print_hex(char **p, uint8_t val) {
    const char *hex = "0123456789abcdef";
    *(*p)++ = hex[(val >> 4) & 0xf];
    *(*p)++ = hex[val & 0xf];
}

static void safe_print_dec(char **p, uint8_t val) {
    if (val >= 10) {
        *(*p)++ = '0' + (val / 10);
        *(*p)++ = '0' + (val % 10);
    } else {
        *(*p)++ = '0' + val;
    }
}

static void log_emu_aes(uint8_t op, uint8_t dest, uint8_t src) {
    char buf[128];
    char *p = buf;
    memcpy(p, "[EMU] aes-op=", 13); p += 13;
    safe_print_hex(&p, op);
    memcpy(p, " dest=xmm", 9); p += 9;
    safe_print_dec(&p, dest);
    memcpy(p, " src=xmm", 8); p += 8;
    safe_print_dec(&p, src);
    *p++ = '\n';
    ssize_t rc = write(2, buf, p - buf);
    (void)rc;
}

static void log_emu_pclmul(uint8_t dest, uint8_t src, uint8_t imm) {
    char buf[128];
    char *p = buf;
    memcpy(p, "[EMU] pclmulqdq dest=xmm", 24); p += 24;
    safe_print_dec(&p, dest);
    memcpy(p, " src=xmm", 8); p += 8;
    safe_print_dec(&p, src);
    memcpy(p, " imm=", 5); p += 5;
    safe_print_hex(&p, imm);
    *p++ = '\n';
    ssize_t rc = write(2, buf, p - buf);
    (void)rc;
}

static void handler(int sig, siginfo_t *si, void *ctx_void) {
    ucontext_t *uc = (ucontext_t *)ctx_void;
    if (!uc->uc_mcontext.fpregs) {
        _exit(133);
    }
    
    uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
    
    if (rip[0] == 0x66) {
        int has_rex = 0;
        uint8_t rex = 0;
        if (rip[1] >= 0x40 && rip[1] <= 0x4f) {
            has_rex = 1;
            rex = rip[1];
        }
        
        uint8_t *opcode = rip + 1 + has_rex;
        
        if (opcode[0] == 0x0f && opcode[1] == 0x38 && (opcode[2] == 0xde || opcode[2] == 0xdf || opcode[2] == 0xdb || opcode[2] == 0xdc || opcode[2] == 0xdd)) {
            uint8_t op_type = opcode[2];
            uint8_t modrm = opcode[3];
            
            if ((modrm >> 6) == 3) {
                uint8_t reg_idx = (modrm >> 3) & 7;
                uint8_t rm_idx = modrm & 7;
                if (has_rex) {
                    if (rex & 4) reg_idx += 8;
                    if (rex & 1) rm_idx += 8;
                }
                
                uint8_t *dest = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
                uint8_t *src = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                
                if (debug_emu) {
                    log_emu_aes(op_type, reg_idx, rm_idx);
                }
                
                if (op_type == 0xde) emulate_aesdec(dest, src);
                else if (op_type == 0xdf) emulate_aesdeclast(dest, src);
                else if (op_type == 0xdb) emulate_aesimc(dest, src);
                else if (op_type == 0xdc) emulate_aesenc(dest, src);
                else if (op_type == 0xdd) emulate_aesenclast(dest, src);
                
                uc->uc_mcontext.gregs[REG_RIP] += (5 + has_rex);
                return;
            }
        } else if (opcode[0] == 0x0f && opcode[1] == 0x3a && opcode[2] == 0x44) {
            uint8_t modrm = opcode[3];
            uint8_t imm = opcode[4];
            
            if ((modrm >> 6) == 3) {
                uint8_t reg_idx = (modrm >> 3) & 7;
                uint8_t rm_idx = modrm & 7;
                if (has_rex) {
                    if (rex & 4) reg_idx += 8;
                    if (rex & 1) rm_idx += 8;
                }
                
                uint8_t *dest = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
                uint8_t *src = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                
                if (debug_emu) {
                    log_emu_pclmul(reg_idx, rm_idx, imm);
                }
                
                emulate_pclmulqdq(dest, src, imm);
                
                uc->uc_mcontext.gregs[REG_RIP] += (6 + has_rex);
                return;
            }
        }
    }
    
    if (old_sa.sa_flags & SA_SIGINFO) {
        if (old_sa.sa_sigaction) {
            old_sa.sa_sigaction(sig, si, ctx_void);
            return;
        }
    } else {
        if (old_sa.sa_handler == SIG_DFL) {
            sigaction(SIGILL, &old_sa, NULL);
            kill(getpid(), SIGILL);
            return;
        } else if (old_sa.sa_handler == SIG_IGN) {
            return;
        } else if (old_sa.sa_handler) {
            old_sa.sa_handler(sig);
            return;
        }
    }
    _exit(132);
}

__attribute__((constructor))
static void init(void) {
    char *env_dbg = getenv("DEBUG_EMU");
    if (env_dbg && strcmp(env_dbg, "1") == 0) {
        debug_emu = 1;
    }
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGILL, &sa, &old_sa);
}
