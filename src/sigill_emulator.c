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

/*
 * Galois Field GF(2^8) arithmetic helpers.
 * In GF(2^8), addition is represented by bitwise XOR.
 * Multiplication by 2 (gmul2) shifts the byte left by 1 bit. If the high bit was set
 * (representing a value >= 128), we reduce it by XORing with the AES polynomial 0x1B.
 * We make this operation branchless by using an arithmetic shift right by 7 on a signed 8-bit integer
 * to create a bitmask (0xFF if high bit is set, 0x00 if not) and bitwise-ANDing it with 0x1B.
 */
static inline uint8_t gmul2(uint8_t x) {
    return (x << 1) ^ (((int8_t)x >> 7) & 0x1B);
}

/* Multiplying by 3 is (x * 2) XOR x */
static inline uint8_t gmul3(uint8_t x) {
    return gmul2(x) ^ x;
}

/* Multiplying by 4 is gmul2(gmul2(x)) */
static inline uint8_t gmul4(uint8_t x) {
    return gmul2(gmul2(x));
}

/* Multiplying by 8 is gmul2(gmul4(x)) */
static inline uint8_t gmul8(uint8_t x) {
    return gmul2(gmul4(x));
}

/* Multiplying by 9 is (x * 8) XOR x */
static inline uint8_t gmul9(uint8_t x) {
    return gmul8(x) ^ x;
}

/* Multiplying by 11 is (x * 8) XOR (x * 2) XOR x */
static inline uint8_t gmul11(uint8_t x) {
    return gmul8(x) ^ gmul2(x) ^ x;
}

/* Multiplying by 13 is (x * 8) XOR (x * 4) XOR x */
static inline uint8_t gmul13(uint8_t x) {
    return gmul8(x) ^ gmul4(x) ^ x;
}

/* Multiplying by 14 is (x * 8) XOR (x * 4) XOR (x * 2) */
static inline uint8_t gmul14(uint8_t x) {
    return gmul8(x) ^ gmul4(x) ^ gmul2(x);
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
        
        state[0 + i*4] = gmul14(s0) ^ gmul11(s1) ^ gmul13(s2) ^ gmul9(s3);
        state[1 + i*4] = gmul9(s0) ^ gmul14(s1) ^ gmul11(s2) ^ gmul13(s3);
        state[2 + i*4] = gmul13(s0) ^ gmul9(s1) ^ gmul14(s2) ^ gmul11(s3);
        state[3 + i*4] = gmul11(s0) ^ gmul13(s1) ^ gmul9(s2) ^ gmul14(s3);
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

/*
 * Emulates the PCLMULQDQ instruction (Carry-Less Multiplication of Quadwords).
 * This multiplies two 64-bit values (low or high halves of dest and src XMM registers,
 * chosen by the immediate byte imm) using carry-less multiplication (XOR instead of addition).
 * We optimize this loop by shifting temp_b and exiting early when temp_b reaches 0,
 * which significantly reduces cycles for sparse inputs.
 */
static void emulate_pclmulqdq(uint8_t *dest, const uint8_t *src, uint8_t imm) {
    uint64_t a, b;
    
    // Select the lower or upper 64-bit half of the destination register
    if (imm & 1) memcpy(&a, dest + 8, 8);
    else memcpy(&a, dest, 8);
    
    // Select the lower or upper 64-bit half of the source register
    if (imm & 16) memcpy(&b, src + 8, 8);
    else memcpy(&b, src, 8);
    
    uint64_t low = 0, high = 0;
    uint64_t temp_b = b;
    
    // Perform carry-less multiplication. Since addition is XOR, we do not carry bits.
    // Loop only as long as there are set bits left in temp_b to process (early exit).
    for (int i = 0; temp_b != 0; i++) {
        if (temp_b & 1) {
            if (i == 0) {
                low ^= a;
            } else {
                low ^= (a << i);
                high ^= (a >> (64 - i));
            }
        }
        temp_b >>= 1;
    }
    
    // Write the 128-bit result back to the destination register
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

static uint64_t get_register_val(ucontext_t *uc, int reg_idx) {
    static const int reg_map[16] = {
        REG_RAX, REG_RCX, REG_RDX, REG_RBX, REG_RSP, REG_RBP, REG_RSI, REG_RDI,
        REG_R8,  REG_R9,  REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15
    };
    return uc->uc_mcontext.gregs[reg_map[reg_idx]];
}

static uint8_t *resolve_mem_addr(ucontext_t *uc, uint8_t *rip, int has_rex, uint8_t rex, uint8_t modrm, int *bytes_consumed) {
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    uint8_t *ptr = rip + 1 + has_rex + 3; // pointing to ModRM + 1 (SIB or Displacement or Imm)
    
    uint64_t base_val = 0;
    uint64_t index_val = 0;
    int scale = 0;
    int32_t disp = 0;
    
    if (mod == 0 && rm == 5) {
        // RIP-relative addressing
        int32_t disp32;
        memcpy(&disp32, ptr, 4);
        ptr += 4;
        int has_imm = (rip[1+has_rex+1] == 0x3a);
        uint8_t *rip_next = ptr + (has_imm ? 1 : 0);
        *bytes_consumed = (rip_next - rip);
        return rip_next + disp32;
    }
    
    if (rm == 4) {
        uint8_t sib = *ptr++;
        uint8_t ss = sib >> 6;
        uint8_t index = (sib >> 3) & 7;
        uint8_t base = sib & 7;
        
        uint8_t base_reg = base;
        if (has_rex && (rex & 1)) base_reg += 8;
        
        uint8_t index_reg = index;
        if (has_rex && (rex & 2)) index_reg += 8;
        
        if (base == 5 && mod == 0) {
            int32_t disp32;
            memcpy(&disp32, ptr, 4);
            ptr += 4;
            disp = disp32;
        } else {
            base_val = get_register_val(uc, base_reg);
        }
        
        if (index != 4) {
            index_val = get_register_val(uc, index_reg);
            scale = ss;
        }
    } else {
        uint8_t base_reg = rm;
        if (has_rex && (rex & 1)) base_reg += 8;
        base_val = get_register_val(uc, base_reg);
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
    
    int has_imm = (rip[1+has_rex+1] == 0x3a);
    if (has_imm) {
        ptr++;
    }
    
    *bytes_consumed = (ptr - rip);
    return (uint8_t *)(base_val + (index_val << scale) + disp);
}

/*
 * The SIGILL signal handler.
 * This is invoked by the OS kernel when an invalid opcode exception occurs.
 * We parse the instruction pointer (RIP) in the saved thread context (ucontext_t)
 * to determine if the crash was caused by an unsupported AES-NI or PCLMULQDQ instruction.
 * If so, we emulate it and advance the RIP register so the program can resume.
 */
static void handler(int sig, siginfo_t *si, void *ctx_void) {
    ucontext_t *uc = (ucontext_t *)ctx_void;
    if (!uc->uc_mcontext.fpregs) {
        _exit(133);
    }
    
    // Get the instruction pointer (RIP) representing where the crash occurred
    uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
    
    // 0x66 prefix is used by x86 to indicate an SSE (Streaming SIMD Extensions) instruction
    if (rip[0] == 0x66) {
        int has_rex = 0;
        uint8_t rex = 0;
        
        // REX prefix (0x40 to 0x4f) on x86-64 extends register access (e.g. accessing XMM8-XMM15)
        if (rip[1] >= 0x40 && rip[1] <= 0x4f) {
            has_rex = 1;
            rex = rip[1];
        }
        
        uint8_t *opcode = rip + 1 + has_rex;
        
        // Check for AES instruction opcodes: 0x0F 0x38 followed by 0xDE (aesdec), 0xDF (aesdeclast),
        // 0xDB (aesimc), 0xDC (aesenc), or 0xDD (aesenclast)
        if (opcode[0] == 0x0f && opcode[1] == 0x38 && (opcode[2] == 0xde || opcode[2] == 0xdf || opcode[2] == 0xdb || opcode[2] == 0xdc || opcode[2] == 0xdd)) {
            uint8_t op_type = opcode[2];
            uint8_t modrm = opcode[3];
            
            // Extract destination register index from the ModRM byte (bits 3-5)
            uint8_t reg_idx = (modrm >> 3) & 7;
            if (has_rex) {
                // If REX.R bit (bit 2 of REX prefix) is set, add 8 to register index
                if (rex & 4) reg_idx += 8;
            }
            
            // Get pointer to the destination XMM register in the saved CPU context
            uint8_t *dest = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            
            // If mod field of ModRM byte is 3, it's a register-to-register operation
            if ((modrm >> 6) == 3) {
                // Extract source register index from the ModRM byte (bits 0-2)
                uint8_t rm_idx = modrm & 7;
                if (has_rex && (rex & 1)) rm_idx += 8; // REX.B bit extends source register index
                
                uint8_t *src = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                
                if (debug_emu) {
                    log_emu_aes(op_type, reg_idx, rm_idx);
                }
                
                // Route to the appropriate mathematical emulator
                if (op_type == 0xde) emulate_aesdec(dest, src);
                else if (op_type == 0xdf) emulate_aesdeclast(dest, src);
                else if (op_type == 0xdb) emulate_aesimc(dest, src);
                else if (op_type == 0xdc) emulate_aesenc(dest, src);
                else if (op_type == 0xdd) emulate_aesenclast(dest, src);
                
                // Skip the executed instruction (prefix + opcode + ModRM = 5 bytes + REX)
                uc->uc_mcontext.gregs[REG_RIP] += (5 + has_rex);
                return;
            } else {
                // Register-to-memory operation: resolve the target effective memory address
                int bytes_consumed = 0;
                uint8_t *src = resolve_mem_addr(uc, rip, has_rex, rex, modrm, &bytes_consumed);
                
                if (debug_emu) {
                    log_emu_aes(op_type, reg_idx, 99); // 99 represents memory operand
                }
                
                if (op_type == 0xde) emulate_aesdec(dest, src);
                else if (op_type == 0xdf) emulate_aesdeclast(dest, src);
                else if (op_type == 0xdb) emulate_aesimc(dest, src);
                else if (op_type == 0xdc) emulate_aesenc(dest, src);
                else if (op_type == 0xdd) emulate_aesenclast(dest, src);
                
                // Skip the executed instruction by the calculated byte length
                uc->uc_mcontext.gregs[REG_RIP] += bytes_consumed;
                return;
            }
        } else if (opcode[0] == 0x0f && opcode[1] == 0x3a && opcode[2] == 0x44) {
            uint8_t modrm = opcode[3];
            
            // Extract destination register index from the ModRM byte (bits 3-5)
            uint8_t reg_idx = (modrm >> 3) & 7;
            if (has_rex) {
                // REX.R bit extends destination register index
                if (rex & 4) reg_idx += 8;
            }
            uint8_t *dest = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            
            if ((modrm >> 6) == 3) {
                uint8_t imm = opcode[4];
                uint8_t rm_idx = modrm & 7;
                if (has_rex && (rex & 1)) rm_idx += 8; // REX.B bit extends source register index
                
                uint8_t *src = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                
                if (debug_emu) {
                    log_emu_pclmul(reg_idx, rm_idx, imm);
                }
                
                emulate_pclmulqdq(dest, src, imm);
                
                // Skip instruction (prefix + opcode + ModRM + imm = 6 bytes + REX)
                uc->uc_mcontext.gregs[REG_RIP] += (6 + has_rex);
                return;
            } else {
                int bytes_consumed = 0;
                uint8_t *src = resolve_mem_addr(uc, rip, has_rex, rex, modrm, &bytes_consumed);
                uint8_t imm = rip[bytes_consumed - 1]; // Immediate byte is at the end of instruction
                
                if (debug_emu) {
                    log_emu_pclmul(reg_idx, 99, imm);
                }
                
                emulate_pclmulqdq(dest, src, imm);
                
                // Skip instruction by the calculated byte length
                uc->uc_mcontext.gregs[REG_RIP] += bytes_consumed;
                return;
            }
        }
    }
    
    // Fallback: chain execution back to the original SIGILL handler if it is not one of our target instructions
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

/*
 * Shared library constructor function.
 * This runs when the library is loaded via LD_PRELOAD, before main() of the host program.
 * We check the environment for debug flags and hook the SIGILL signal handler.
 */
__attribute__((constructor))
static void init(void) {
    // Check if emulation debug tracing is requested
    char *env_dbg = getenv("DEBUG_EMU");
    if (env_dbg && strcmp(env_dbg, "1") == 0) {
        debug_emu = 1;
    }
    
    // Register our SIGILL handler while saving the original handler (old_sa) to chain fallback calls
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handler;
    sigemptyset(&sa.sa_mask);
    
    // SA_SIGINFO provides the ucontext pointer; SA_NODEFER prevents blocking nested SIGILL signals
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGILL, &sa, &old_sa);
}
