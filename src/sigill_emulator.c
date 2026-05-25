#define _GNU_SOURCE
#include <signal.h>
#include <ucontext.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

static struct sigaction old_sa;
static struct sigaction old_sa_trap;
static int debug_emu = 0;

// AES Tables and S-Box definitions
// The Rijndael S-box (Substitution-box) is a non-linear byte substitution table used in AES.
// It is designed to resist linear and differential cryptanalysis by mapping each byte to its
// multiplicative inverse in GF(2^8), followed by an affine transformation.
// See: https://en.wikipedia.org/wiki/Rijndael_S-box
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
 * AES uses the finite field GF(2^8) with the irreducible polynomial:
 *   x^8 + x^4 + x^3 + x + 1 (represented in hexadecimal as 0x1B, with the x^8 bit omitted).
 * See: https://en.wikipedia.org/wiki/Finite_field_arithmetic
 * 
 * In GF(2^8), addition is represented by a simple bitwise XOR.
 * Multiplication by 2 (gmul2) shifts the byte left by 1 bit. If the high bit of x was set
 * before shifting (indicating overflow of x^7), we reduce the result by XORing with 0x1B.
 * 
 * We make gmul2 branchless:
 * 1. (int8_t)x >> 7: A signed 8-bit arithmetic shift right replicates the MSB across all 8 bits.
 *    - If MSB is 1: results in 0xFF (all 1s).
 *    - If MSB is 0: results in 0x00 (all 0s).
 * 2. (((int8_t)x >> 7) & 0x1B): Masks the irreducible polynomial to 0x1B or 0x00.
 * 3. XORing this mask with (x << 1) yields the branchless modulo multiplication.
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

/*
 * Performs the AES MixColumns transformation on the state.
 * MixColumns multiplies each column of the state matrix by a fixed polynomial.
 * We optimize this mathematically by extracting the common XOR sum (temp = s0 ^ s1 ^ s2 ^ s3)
 * which reduces the number of Galois Field doublings (gmul2) from 8 down to 4 per column.
 */
static void mix_columns(uint8_t *state) {
    for (int i = 0; i < 4; i++) {
        uint8_t s0 = state[0 + i*4];
        uint8_t s1 = state[1 + i*4];
        uint8_t s2 = state[2 + i*4];
        uint8_t s3 = state[3 + i*4];
        
        uint8_t temp = s0 ^ s1 ^ s2 ^ s3;
        
        state[0 + i*4] = gmul2(s0 ^ s1) ^ temp ^ s0;
        state[1 + i*4] = gmul2(s1 ^ s2) ^ temp ^ s1;
        state[2 + i*4] = gmul2(s2 ^ s3) ^ temp ^ s2;
        state[3 + i*4] = gmul2(s3 ^ s0) ^ temp ^ s3;
    }
}

/*
 * Performs the AES InvMixColumns transformation on the state.
 * InvMixColumns is the inverse matrix multiplication of MixColumns.
 * We optimize this mathematically using the Daemen/Rijmen algebraic relation trick:
 *   InvMixColumns(S) = MixColumns(S) XOR gmul8(s0 ^ s1 ^ s2 ^ s3) XOR gmul4(s_even/odd)
 * This collapses the multiplication of large coefficients (9, 11, 13, 14) and reduces
 * the number of gmul2 operations per column from 48 down to only 11.
 */
static void inv_mix_columns(uint8_t *state) {
    for (int i = 0; i < 4; i++) {
        uint8_t s0 = state[0 + i*4];
        uint8_t s1 = state[1 + i*4];
        uint8_t s2 = state[2 + i*4];
        uint8_t s3 = state[3 + i*4];
        
        uint8_t temp = s0 ^ s1 ^ s2 ^ s3;
        uint8_t h8 = gmul8(temp);
        uint8_t h4_02 = gmul4(s0 ^ s2);
        uint8_t h4_13 = gmul4(s1 ^ s3);
        
        state[0 + i*4] = gmul2(s0 ^ s1) ^ temp ^ s0 ^ h8 ^ h4_02;
        state[1 + i*4] = gmul2(s1 ^ s2) ^ temp ^ s1 ^ h8 ^ h4_13;
        state[2 + i*4] = gmul2(s2 ^ s3) ^ temp ^ s2 ^ h8 ^ h4_02;
        state[3 + i*4] = gmul2(s3 ^ s0) ^ temp ^ s3 ^ h8 ^ h4_13;
    }
}

/*
 * Shifts the rows of the AES state.
 * In AES, the 16-byte state is treated as a 4x4 matrix stored in column-major order:
 *   [ state[0]  state[4]  state[8]   state[12] ]  <- Row 0 (no shift)
 *   [ state[1]  state[5]  state[9]   state[13] ]  <- Row 1 (shift left 1)
 *   [ state[2]  state[6]  state[10]  state[14] ]  <- Row 2 (shift left 2)
 *   [ state[3]  state[7]  state[11]  state[15] ]  <- Row 3 (shift left 3)
 * See: https://en.wikipedia.org/wiki/Advanced_Encryption_Standard#The_ShiftRows_step
 */
static void shift_rows(uint8_t *state) {
    uint8_t tmp;
    // Row 1: Shift left by 1 column
    tmp = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = tmp;
    // Row 2: Shift left by 2 columns (swap columns 0<->2 and 1<->3)
    tmp = state[2]; state[2] = state[10]; state[10] = tmp;
    tmp = state[6]; state[6] = state[14]; state[14] = tmp;
    // Row 3: Shift left by 3 columns (equivalent to shifting right by 1)
    tmp = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = state[3]; state[3] = tmp;
}

/*
 * Inverse ShiftRows.
 * Row 1 is shifted right by 1, Row 2 by 2, and Row 3 by 3 columns.
 */
static void inv_shift_rows(uint8_t *state) {
    uint8_t tmp;
    // Row 1: Shift right by 1 column
    tmp = state[13]; state[13] = state[9]; state[9] = state[5]; state[5] = state[1]; state[1] = tmp;
    // Row 2: Shift right by 2 columns
    tmp = state[2]; state[2] = state[10]; state[10] = tmp;
    tmp = state[6]; state[6] = state[14]; state[14] = tmp;
    // Row 3: Shift right by 3 columns (equivalent to shifting left by 1)
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
 * Carry-less multiplication operates like standard binary multiplication, but addition
 * is performed without carry (using XOR). This is mathematically equivalent to 
 * polynomial multiplication over GF(2).
 * See: https://en.wikipedia.org/wiki/CLMUL_instruction_set
 * 
 * The immediate byte (imm) selects which 64-bit halves of the 128-bit source and 
 * destination XMM registers are multiplied:
 *   - imm & 1  == 1: Selects upper 64-bit of dest (else lower)
 *   - imm & 16 == 1: Selects upper 64-bit of src (else lower)
 * 
 * Optimization: The carry-less multiplication loop shifts temp_b and exits early when
 * temp_b reaches 0, avoiding 64 full loop iterations for sparse inputs.
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

/*
 * Resolves the effective memory address for instructions with memory operands.
 * In x86_64, instruction addressing is defined by ModR/M and SIB bytes:
 *   - ModR/M Byte: bits [7:6] = Mod (Addressing mode), bits [5:3] = Reg/Opcode, bits [2:0] = R/M
 *   - SIB (Scale-Index-Base) Byte: bits [7:6] = Scale (0, 1, 2, 3 -> factor 1, 2, 4, 8),
 *     bits [5:3] = Index register index, bits [2:0] = Base register index.
 * See: https://en.wikipedia.org/wiki/ModR/M
 */
static uint8_t *resolve_mem_addr(ucontext_t *uc, uint8_t *rip, int has_rex, uint8_t rex, uint8_t modrm, int *bytes_consumed) {
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    uint8_t *ptr = rip + 1 + has_rex + 3; // Pointing past: prefix(0x66) + REX + Opcode(3 bytes) to ModR/M+1
    
    uint64_t base_val = 0;
    uint64_t index_val = 0;
    int scale = 0;
    int32_t disp = 0;
    
    // Mod == 0, R/M == 5 indicates RIP-relative addressing (Base is instruction pointer + displacement)
    if (mod == 0 && rm == 5) {
        int32_t disp32;
        memcpy(&disp32, ptr, 4);
        ptr += 4;
        // PCLMULQDQ (opcode 0x3a) includes a 1-byte immediate suffix at the end of the instruction,
        // which must be accounted for to correctly calculate the next instruction's address (RIP_next).
        int has_imm = (rip[1+has_rex+1] == 0x3a);
        uint8_t *rip_next = ptr + (has_imm ? 1 : 0);
        *bytes_consumed = (rip_next - rip);
        return rip_next + disp32;
    }
    
    // R/M == 4 indicates the presence of a SIB (Scale-Index-Base) byte for complex indexing
    if (rm == 4) {
        uint8_t sib = *ptr++;
        uint8_t ss = sib >> 6;
        uint8_t index = (sib >> 3) & 7;
        uint8_t base = sib & 7;
        
        // Translate base and index registers, extending indices if REX prefix is present
        uint8_t base_reg = base;
        if (has_rex && (rex & 1)) base_reg += 8; // REX.B bit extends base register index
        
        uint8_t index_reg = index;
        if (has_rex && (rex & 2)) index_reg += 8; // REX.X bit extends index register index
        
        // Base == 5 with Mod == 0 indicates no base register, only a 32-bit displacement
        if (base == 5 && mod == 0) {
            int32_t disp32;
            memcpy(&disp32, ptr, 4);
            ptr += 4;
            disp = disp32;
        } else {
            base_val = get_register_val(uc, base_reg);
        }
        
        // Index == 4 indicates index register is omitted (ESP/RSP cannot be used as an index)
        if (index != 4) {
            index_val = get_register_val(uc, index_reg);
            scale = ss;
        }
    } else {
        // No SIB byte: Rm specifies the base register directly
        uint8_t base_reg = rm;
        if (has_rex && (rex & 1)) base_reg += 8; // REX.B bit extends base register index
        base_val = get_register_val(uc, base_reg);
    }
    
    // Handle displacements: Mod == 1 (8-bit signed displacement), Mod == 2 (32-bit signed displacement)
    if (mod == 1) {
        int8_t disp8 = (int8_t)*ptr++;
        disp += disp8;
    } else if (mod == 2) {
        int32_t disp32;
        memcpy(&disp32, ptr, 4);
        ptr += 4;
        disp += disp32;
    }
    
    // Account for PCLMULQDQ immediate byte suffix in total instruction size
    int has_imm = (rip[1+has_rex+1] == 0x3a);
    if (has_imm) {
        ptr++;
    }
    
    *bytes_consumed = (ptr - rip);
    return (uint8_t *)(base_val + (index_val << scale) + disp);
}

struct cache_entry {
    uint64_t rip;
    uint32_t op_type;
    uint8_t reg_idx;
    uint8_t rm_idx;
    uint8_t is_mem;
    uint8_t has_rex;
    uint8_t rex;
    uint8_t modrm;
    int bytes_consumed;
    uint32_t seq;
};

#define CACHE_SIZE 1024
static struct cache_entry rip_cache[CACHE_SIZE];

static inline uint32_t get_cache_index(uint8_t *rip) {
    return ((uintptr_t)rip >> 2) & (CACHE_SIZE - 1);
}

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static uint8_t *island_page = NULL;
static int island_used_bytes = 0;

static void *allocate_trampoline_island(uint8_t *rip) {
    uintptr_t start = (uintptr_t)rip;
    
    // Attempt to allocate a page within 2GB of the calling rip
    for (int offset_mb = -1024; offset_mb <= 1024; offset_mb += 16) {
        if (offset_mb == 0) continue;
        uintptr_t hint = start + ((intptr_t)offset_mb * 1024 * 1024);
        hint &= ~0xFFF;
        
        void *addr = mmap((void *)hint, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, 
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (addr != MAP_FAILED) {
            return addr;
        }
        
        addr = mmap((void *)hint, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (addr != MAP_FAILED) {
            intptr_t diff = (intptr_t)addr - (intptr_t)rip;
            if (diff >= -2147483648LL && diff <= 2147483647LL) {
                return addr;
            }
            munmap(addr, 4096);
        }
    }
    return NULL;
}

static uint64_t get_register_val_trampoline(uint64_t *gp_regs, int reg_idx, uintptr_t original_rsp) {
    static const int gp_map[16] = {
        14, 13, 12, 11, -1, 8, 10, 9,
        7,  6,  5,  4,  3,  2,  1,  0
    };
    if (reg_idx == 4) return original_rsp;
    return gp_regs[gp_map[reg_idx]];
}

static uint8_t *resolve_mem_addr_trampoline(uint64_t *gp_regs, uintptr_t original_rsp, uint8_t *rip, int has_rex, uint8_t rex, uint8_t modrm, int *bytes_consumed) {
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    uint8_t *ptr = rip + 1 + has_rex + 3;
    
    uint64_t base_val = 0;
    uint64_t index_val = 0;
    int scale = 0;
    int32_t disp = 0;
    
    if (mod == 0 && rm == 5) {
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
            base_val = get_register_val_trampoline(gp_regs, base_reg, original_rsp);
        }
        
        if (index != 4) {
            index_val = get_register_val_trampoline(gp_regs, index_reg, original_rsp);
            scale = ss;
        }
    } else {
        uint8_t base_reg = rm;
        if (has_rex && (rex & 1)) base_reg += 8;
        base_val = get_register_val_trampoline(gp_regs, base_reg, original_rsp);
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

static struct cache_entry *find_cache_entry_by_ret_addr(uintptr_t ret_addr) {
    for (int size = 5; size <= 9; size++) {
        uintptr_t rip = ret_addr - size;
        uint32_t idx = get_cache_index((uint8_t *)rip);
        struct cache_entry *entry = &rip_cache[idx];
        if (entry->rip == rip && entry->bytes_consumed == size) {
            return entry;
        }
    }
    return NULL;
}

void trampoline_entry(void);

__asm__(
".global trampoline_entry\n"
"trampoline_entry:\n"
    "push %rax\n"
    "push %rcx\n"
    "push %rdx\n"
    "push %rbx\n"
    "push %rsi\n"
    "push %rdi\n"
    "push %rbp\n"
    "push %r8\n"
    "push %r9\n"
    "push %r10\n"
    "push %r11\n"
    "push %r12\n"
    "push %r13\n"
    "push %r14\n"
    "push %r15\n"
    "sub $256, %rsp\n"
    "movdqu %xmm0, 0(%rsp)\n"
    "movdqu %xmm1, 16(%rsp)\n"
    "movdqu %xmm2, 32(%rsp)\n"
    "movdqu %xmm3, 48(%rsp)\n"
    "movdqu %xmm4, 64(%rsp)\n"
    "movdqu %xmm5, 80(%rsp)\n"
    "movdqu %xmm6, 96(%rsp)\n"
    "movdqu %xmm7, 112(%rsp)\n"
    "movdqu %xmm8, 128(%rsp)\n"
    "movdqu %xmm9, 144(%rsp)\n"
    "movdqu %xmm10, 160(%rsp)\n"
    "movdqu %xmm11, 176(%rsp)\n"
    "movdqu %xmm12, 192(%rsp)\n"
    "movdqu %xmm13, 208(%rsp)\n"
    "movdqu %xmm14, 224(%rsp)\n"
    "movdqu %xmm15, 240(%rsp)\n"
    "mov %rsp, %rsi\n"
    "lea 256(%rsp), %rdi\n"
    "mov 376(%rsp), %rdx\n"
    "call trampoline_c_helper\n"
    "movdqu 0(%rsp), %xmm0\n"
    "movdqu 16(%rsp), %xmm1\n"
    "movdqu 32(%rsp), %xmm2\n"
    "movdqu 48(%rsp), %xmm3\n"
    "movdqu 64(%rsp), %xmm4\n"
    "movdqu 80(%rsp), %xmm5\n"
    "movdqu 96(%rsp), %xmm6\n"
    "movdqu 112(%rsp), %xmm7\n"
    "movdqu 128(%rsp), %xmm8\n"
    "movdqu 144(%rsp), %xmm9\n"
    "movdqu 160(%rsp), %xmm10\n"
    "movdqu 176(%rsp), %xmm11\n"
    "movdqu 192(%rsp), %xmm12\n"
    "movdqu 208(%rsp), %xmm13\n"
    "movdqu 224(%rsp), %xmm14\n"
    "movdqu 240(%rsp), %xmm15\n"
    "add $256, %rsp\n"
    "pop %r15\n"
    "pop %r14\n"
    "pop %r13\n"
    "pop %r12\n"
    "pop %r11\n"
    "pop %r10\n"
    "pop %r9\n"
    "pop %r8\n"
    "pop %rbp\n"
    "pop %rdi\n"
    "pop %rsi\n"
    "pop %rbx\n"
    "pop %rdx\n"
    "pop %rcx\n"
    "pop %rax\n"
    "ret\n"
);

void trampoline_c_helper(uint64_t *gp_regs, uint8_t *xmm_regs, uintptr_t ret_addr) {
    struct cache_entry *entry = find_cache_entry_by_ret_addr(ret_addr);
    if (!entry) {
        ssize_t rc = write(2, "[FATAL] Trampoline could not find cache entry!\n", 47);
        (void)rc;
        _exit(135);
    }
    
    uint8_t *dest = &xmm_regs[entry->reg_idx * 16];
    uint8_t *rip = (uint8_t *)(ret_addr - entry->bytes_consumed);
    
    if (entry->op_type == 0xde || entry->op_type == 0xdf || entry->op_type == 0xdb || entry->op_type == 0xdc || entry->op_type == 0xdd) {
        uint8_t *src;
        if (entry->is_mem) {
            int dummy_bytes_consumed;
            uintptr_t original_rsp = (uintptr_t)gp_regs + 128;
            src = resolve_mem_addr_trampoline(gp_regs, original_rsp, rip, entry->has_rex, entry->rex, entry->modrm, &dummy_bytes_consumed);
        } else {
            src = &xmm_regs[entry->rm_idx * 16];
        }
        
        if (entry->op_type == 0xde) emulate_aesdec(dest, src);
        else if (entry->op_type == 0xdf) emulate_aesdeclast(dest, src);
        else if (entry->op_type == 0xdb) emulate_aesimc(dest, src);
        else if (entry->op_type == 0xdc) emulate_aesenc(dest, src);
        else if (entry->op_type == 0xdd) emulate_aesenclast(dest, src);
    } else if (entry->op_type == 0x44) {
        uint8_t *src;
        uint8_t imm;
        if (entry->is_mem) {
            int dummy_bytes_consumed;
            uintptr_t original_rsp = (uintptr_t)gp_regs + 128;
            src = resolve_mem_addr_trampoline(gp_regs, original_rsp, rip, entry->has_rex, entry->rex, entry->modrm, &dummy_bytes_consumed);
            imm = rip[entry->bytes_consumed - 1];
        } else {
            src = &xmm_regs[entry->rm_idx * 16];
            imm = rip[1 + entry->has_rex + 4];
        }
        
        emulate_pclmulqdq(dest, src, imm);
    }
}

static void patch_code(uint8_t *rip, int bytes_consumed) {
    if (!island_page) {
        island_page = allocate_trampoline_island(rip);
        if (!island_page) {
            return;
        }
    }
    
    int offset = __atomic_fetch_add(&island_used_bytes, 16, __ATOMIC_SEQ_CST);
    if (offset + 16 > 4096) {
        return;
    }
    
    uint8_t *stub = island_page + offset;
    
    // Write absolute jump stub
    stub[0] = 0x50; // push %rax
    stub[1] = 0x48; // movabs ...
    stub[2] = 0xb8;
    uintptr_t target = (uintptr_t)trampoline_entry;
    memcpy(stub + 3, &target, 8);
    stub[11] = 0x48; // xchg %rax, (%rsp)
    stub[12] = 0x87;
    stub[13] = 0x04;
    stub[14] = 0x24;
    stub[15] = 0xc3; // ret
    
    __builtin___clear_cache((char *)stub, (char *)stub + 16);
    
    uintptr_t page_start = (uintptr_t)rip & ~0xFFF;
    if (mprotect((void *)page_start, 4096 * 2, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        return;
    }
    
    int32_t call_offset = (int32_t)((uintptr_t)stub - ((uintptr_t)rip + 5));
    
    rip[0] = 0xCC; // INT3
    __builtin___clear_cache((char *)rip, (char *)rip + 1);
    
    memcpy(rip + 1, &call_offset, 4);
    
    for (int i = 5; i < bytes_consumed; i++) {
        rip[i] = 0x90; // NOP
    }
    
    rip[0] = 0xE8; // call
    
    __builtin___clear_cache((char *)rip, (char *)rip + bytes_consumed);
    
    mprotect((void *)page_start, 4096 * 2, PROT_READ | PROT_EXEC);
}

static void handler(int sig, siginfo_t *si, void *ctx_void) {
    ucontext_t *uc = (ucontext_t *)ctx_void;
    
    // Get the instruction pointer (RIP) representing where the crash occurred
    uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];
    
    if (sig == SIGTRAP) {
        if (rip[0] == 0xCC || rip[0] == 0xE8) {
            return; // It's our patching sequence, retry the instruction
        }
        
        // Fallback to original SIGTRAP handler
        if (old_sa_trap.sa_flags & SA_SIGINFO) {
            if (old_sa_trap.sa_sigaction) {
                old_sa_trap.sa_sigaction(sig, si, ctx_void);
                return;
            }
        } else {
            if (old_sa_trap.sa_handler == SIG_DFL) {
                sigaction(SIGTRAP, &old_sa_trap, NULL);
                kill(getpid(), SIGTRAP);
                return;
            } else if (old_sa_trap.sa_handler == SIG_IGN) {
                return;
            } else if (old_sa_trap.sa_handler) {
                old_sa_trap.sa_handler(sig);
                return;
            }
        }
        _exit(134);
    }
    
    if (!uc->uc_mcontext.fpregs) {
        _exit(133);
    }
    
    // Try lock-free cache lookup first
    uint32_t idx = get_cache_index(rip);
    struct cache_entry *entry = &rip_cache[idx];
    
    uint32_t seq1, seq2;
    uint64_t entry_rip;
    uint32_t cached_op_type = 0;
    uint8_t cached_reg_idx = 0, cached_rm_idx = 0, cached_is_mem = 0, cached_has_rex = 0, cached_rex = 0, cached_modrm = 0;
    int cached_bytes_consumed = 0;
    int cache_hit = 0;
    
    for (int retry = 0; retry < 3; retry++) {
        seq1 = __atomic_load_n(&entry->seq, __ATOMIC_ACQUIRE);
        if (seq1 & 1) continue; // Write in progress
        
        entry_rip = entry->rip;
        cached_op_type = entry->op_type;
        cached_reg_idx = entry->reg_idx;
        cached_rm_idx = entry->rm_idx;
        cached_is_mem = entry->is_mem;
        cached_has_rex = entry->has_rex;
        cached_rex = entry->rex;
        cached_modrm = entry->modrm;
        cached_bytes_consumed = entry->bytes_consumed;
        
        seq2 = __atomic_load_n(&entry->seq, __ATOMIC_ACQUIRE);
        if (seq1 == seq2) {
            if (entry_rip == (uint64_t)rip) {
                cache_hit = 1;
            }
            break;
        }
    }
    
    if (cache_hit) {
        uint8_t *dest = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[cached_reg_idx];
        if (cached_op_type == 0xde || cached_op_type == 0xdf || cached_op_type == 0xdb || cached_op_type == 0xdc || cached_op_type == 0xdd) {
            uint8_t *src;
            if (cached_is_mem) {
                int dummy_bytes_consumed;
                src = resolve_mem_addr(uc, rip, cached_has_rex, cached_rex, cached_modrm, &dummy_bytes_consumed);
            } else {
                src = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[cached_rm_idx];
            }
            
            if (debug_emu) {
                log_emu_aes(cached_op_type, cached_reg_idx, cached_is_mem ? 99 : cached_rm_idx);
            }
            
            if (cached_op_type == 0xde) emulate_aesdec(dest, src);
            else if (cached_op_type == 0xdf) emulate_aesdeclast(dest, src);
            else if (cached_op_type == 0xdb) emulate_aesimc(dest, src);
            else if (cached_op_type == 0xdc) emulate_aesenc(dest, src);
            else if (cached_op_type == 0xdd) emulate_aesenclast(dest, src);
            
            uc->uc_mcontext.gregs[REG_RIP] += cached_bytes_consumed;
            return;
        } else if (cached_op_type == 0x44) {
            uint8_t *src;
            uint8_t imm;
            if (cached_is_mem) {
                int dummy_bytes_consumed;
                src = resolve_mem_addr(uc, rip, cached_has_rex, cached_rex, cached_modrm, &dummy_bytes_consumed);
                imm = rip[cached_bytes_consumed - 1];
            } else {
                src = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[cached_rm_idx];
                imm = rip[1 + cached_has_rex + 4];
            }
            
            if (debug_emu) {
                log_emu_pclmul(cached_reg_idx, cached_is_mem ? 99 : cached_rm_idx, imm);
            }
            
            emulate_pclmulqdq(dest, src, imm);
            uc->uc_mcontext.gregs[REG_RIP] += cached_bytes_consumed;
            return;
        }
    }
    
    // Cache miss: parse and decode instruction, then cache the metadata
    // 0x66 operand size override prefix: tells x86 to execute SSE/AVX 128-bit instructions
    if (rip[0] == 0x66) {
        int has_rex = 0;
        uint8_t rex = 0;
        
        // REX prefix (0x40 to 0x4f): extends register addressability from XMM0-7 to XMM8-15
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
                // REX.R bit extends the Reg field of ModRM to access XMM8-15
                if (rex & 4) reg_idx += 8;
            }
            
            // Get pointer to the destination XMM register in the saved CPU context
            uint8_t *dest = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            
            // If mod field of ModRM byte is 3, it's a register-to-register operation
            if ((modrm >> 6) == 3) {
                // Extract source register index from the ModRM byte (bits 0-2)
                uint8_t rm_idx = modrm & 7;
                if (has_rex && (rex & 1)) rm_idx += 8; // REX.B bit extends the R/M field
                
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
                
                int bytes_consumed = 5 + has_rex;
                
                // Write to cache
                uint32_t seq_val = __atomic_load_n(&entry->seq, __ATOMIC_RELAXED);
                __atomic_store_n(&entry->seq, seq_val + 1, __ATOMIC_RELEASE);
                entry->rip = (uint64_t)rip;
                entry->op_type = op_type;
                entry->reg_idx = reg_idx;
                entry->rm_idx = rm_idx;
                entry->is_mem = 0;
                entry->has_rex = has_rex;
                entry->rex = rex;
                entry->modrm = modrm;
                entry->bytes_consumed = bytes_consumed;
                __atomic_store_n(&entry->seq, seq_val + 2, __ATOMIC_RELEASE);
                
                // Patch the instruction site to call the trampoline island stub
                patch_code(rip, bytes_consumed);
                
                // Skip the executed instruction (prefix + opcode + ModRM = 5 bytes + REX)
                uc->uc_mcontext.gregs[REG_RIP] += bytes_consumed;
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
                
                // Write to cache
                uint32_t seq_val = __atomic_load_n(&entry->seq, __ATOMIC_RELAXED);
                __atomic_store_n(&entry->seq, seq_val + 1, __ATOMIC_RELEASE);
                entry->rip = (uint64_t)rip;
                entry->op_type = op_type;
                entry->reg_idx = reg_idx;
                entry->rm_idx = 0;
                entry->is_mem = 1;
                entry->has_rex = has_rex;
                entry->rex = rex;
                entry->modrm = modrm;
                entry->bytes_consumed = bytes_consumed;
                __atomic_store_n(&entry->seq, seq_val + 2, __ATOMIC_RELEASE);
                
                // Patch the instruction site to call the trampoline island stub
                patch_code(rip, bytes_consumed);
                
                // Skip the executed instruction by the calculated byte length
                uc->uc_mcontext.gregs[REG_RIP] += bytes_consumed;
                return;
            }
        } else if (opcode[0] == 0x0f && opcode[1] == 0x3a && opcode[2] == 0x44) {
            uint8_t op_type = 0x44; // Custom type representation for PCLMULQDQ
            uint8_t modrm = opcode[3];
            
            // Extract destination register index from the ModRM byte (bits 3-5)
            uint8_t reg_idx = (modrm >> 3) & 7;
            if (has_rex) {
                // REX.R bit extends the Reg field to access XMM8-15
                if (rex & 4) reg_idx += 8;
            }
            uint8_t *dest = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[reg_idx];
            
            if ((modrm >> 6) == 3) {
                uint8_t imm = opcode[4];
                uint8_t rm_idx = modrm & 7;
                if (has_rex && (rex & 1)) rm_idx += 8; // REX.B bit extends the R/M field
                
                uint8_t *src = (uint8_t *)&uc->uc_mcontext.fpregs->_xmm[rm_idx];
                
                if (debug_emu) {
                    log_emu_pclmul(reg_idx, rm_idx, imm);
                }
                
                emulate_pclmulqdq(dest, src, imm);
                
                int bytes_consumed = 6 + has_rex;
                
                // Write to cache
                uint32_t seq_val = __atomic_load_n(&entry->seq, __ATOMIC_RELAXED);
                __atomic_store_n(&entry->seq, seq_val + 1, __ATOMIC_RELEASE);
                entry->rip = (uint64_t)rip;
                entry->op_type = op_type;
                entry->reg_idx = reg_idx;
                entry->rm_idx = rm_idx;
                entry->is_mem = 0;
                entry->has_rex = has_rex;
                entry->rex = rex;
                entry->modrm = modrm;
                entry->bytes_consumed = bytes_consumed;
                __atomic_store_n(&entry->seq, seq_val + 2, __ATOMIC_RELEASE);
                
                // Patch the instruction site to call the trampoline island stub
                patch_code(rip, bytes_consumed);
                
                // Skip instruction (prefix + opcode + ModRM + imm = 6 bytes + REX)
                uc->uc_mcontext.gregs[REG_RIP] += bytes_consumed;
                return;
            } else {
                int bytes_consumed = 0;
                uint8_t *src = resolve_mem_addr(uc, rip, has_rex, rex, modrm, &bytes_consumed);
                uint8_t imm = rip[bytes_consumed - 1]; // Immediate byte is at the end of instruction
                
                if (debug_emu) {
                    log_emu_pclmul(reg_idx, 99, imm);
                }
                
                emulate_pclmulqdq(dest, src, imm);
                
                // Write to cache
                uint32_t seq_val = __atomic_load_n(&entry->seq, __ATOMIC_RELAXED);
                __atomic_store_n(&entry->seq, seq_val + 1, __ATOMIC_RELEASE);
                entry->rip = (uint64_t)rip;
                entry->op_type = op_type;
                entry->reg_idx = reg_idx;
                entry->rm_idx = 0;
                entry->is_mem = 1;
                entry->has_rex = has_rex;
                entry->rex = rex;
                entry->modrm = modrm;
                entry->bytes_consumed = bytes_consumed;
                __atomic_store_n(&entry->seq, seq_val + 2, __ATOMIC_RELEASE);
                
                // Patch the instruction site to call the trampoline island stub
                patch_code(rip, bytes_consumed);
                
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
    
    // Register our SIGTRAP handler for lock-free atomic patching, preserving original handler
    struct sigaction sa_trap;
    memset(&sa_trap, 0, sizeof(sa_trap));
    sa_trap.sa_sigaction = handler;
    sigemptyset(&sa_trap.sa_mask);
    sa_trap.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGTRAP, &sa_trap, &old_sa_trap);
}
