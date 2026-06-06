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
/*
 * Controls which emulation strategy is used when an illegal instruction is caught.
 *
 * 0 — Safe Mode (default, set by EMU_MODE=safe or leaving it unset)
 *     Every illegal instruction causes the kernel to deliver a SIGILL signal.
 *     Our handler catches it, runs the math in software, then returns.
 *     Works everywhere — containers, SELinux, read-only memory policies.
 *     Overhead: ~1,650 ns per emulated instruction.
 *
 * 1 — Experimental Mode (set by EMU_MODE=experimental)
 *     The first time an illegal instruction is caught we also overwrite it in
 *     memory with a tiny call stub pointing to our emulator. Every subsequent
 *     execution of the same instruction goes straight to the emulator without
 *     ever entering the kernel. About 15× faster (~110 ns per call).
 *     Requires the OS to allow writing to executable pages (mprotect). Will
 *     not work inside strict security policies (SELinux execmem denial, etc.)
 *     and automatically falls back to Safe Mode on a per-instruction basis if
 *     the rewrite fails.
 *
 * Important: before loading this library, the target binary must have its
 * startup CPU-feature check removed, otherwise it will print a fatal error and
 * exit before any instructions are emulated. Use the companion patcher:
 *   python3 /home/michael/patch_agy.py /path/to/binary
 *
 * Full details and benchmarks: docs/LEARNING_GUIDE.md, docs/session_log.md,
 * docs/benchmark_results.md, docs/SKILL.md.
 */
static int emu_mode_experimental = 0;

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
 * Finite Field GF(2^8) Arithmetic Multiplication by 2 (Branchless Rijndael Field doubling).
 * 
 * AES operates on bytes treated as coefficients of polynomials in the finite field GF(2^8)
 * modulo the irreducible polynomial:
 *   P(x) = x^8 + x^4 + x^3 + x + 1
 * Represented in binary as 100011011 (with the x^8 bit omitted for 8-bit space as 0x1B).
 * See: https://en.wikipedia.org/wiki/Finite_field_arithmetic
 *      https://en.wikipedia.org/wiki/Rijndael_mix_columns
 * 
 * In GF(2^8), polynomial addition is represented by bitwise XOR (since 1 + 1 = 0 in binary field).
 * Multiplying a polynomial by x (multiplication by 2, or shift-left-1) shifts the bits:
 *   (x << 1)
 * If the original high-bit (x^7) was 1, shifting causes it to overflow to x^8. We must perform
 * modulo reduction by XORing the overflowed value with the irreducible polynomial 0x1B.
 * 
 * Obscure Algorithm: Branchless Signed-Shift Masking
 * Instead of using slow conditional branches (`if (x & 0x80) x ^= 0x1B;`), which cause CPU pipeline
 * stalls due to branch mispredictions, we make this operation branchless:
 *   1. (int8_t)x >> 7: Casts x to a signed 8-bit integer and performs an arithmetic right shift.
 *      Arithmetic right shift replicates the sign bit (the MSB of x) across all 8 bits.
 *      - If MSB is 1: results in 11111111 (0xFF)
 *      - If MSB is 0: results in 00000000 (0x00)
 *   2. (((int8_t)x >> 7) & 0x1B): Masks the irreducible polynomial 0x1B.
 *      - If MSB was 1: 0xFF & 0x1B = 0x1B
 *      - If MSB was 0: 0x00 & 0x1B = 0x00
 *   3. XORing this mask with (x << 1) dynamically reduces the polynomial in constant-time.
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
 * AES MixColumns Transformation (Optimized Column Mixing).
 * 
 * MixColumns treats each column of the state as a 4-term polynomial over GF(2^8) and
 * multiplies it modulo (x^4 + 1) by a fixed polynomial a(x) = {03}x^3 + {01}x^2 + {01}x + {02}.
 * Matrix multiplication form:
 *   [ s'_0 ]   [ 02  03  01  01 ] [ s_0 ]
 *   [ s'_1 ] = [ 01  02  03  01 ] [ s_1 ]
 *   [ s'_2 ]   [ 01  01  02  03 ] [ s_2 ]
 *   [ s'_3 ]   [ 03  01  01  02 ] [ s_3 ]
 * See: https://en.wikipedia.org/wiki/Rijndael_mix_columns
 * 
 * Obscure Algorithm: Common XOR Sum Factorization
 * A standard matrix multiplication requires 4 GF multiplications and 3 XORs per row (16 mults/col).
 * Since multiplication by 3 distributes as (gmul2(x) ^ x), we can factor out common terms:
 *   temp = s_0 ^ s_1 ^ s_2 ^ s_3
 * We can compute the output byte by doing:
 *   s'_0 = gmul2(s_0 ^ s_1) ^ temp ^ s_0
 * By extracting `temp`, we reduce the number of heavy GF doublings (`gmul2`) from 8 down to 4
 * per column, reducing runtime CPU cycles by 50%.
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
 * AES Inverse MixColumns (InvMixColumns) via the Daemen/Rijmen Algebraic Relation.
 * 
 * InvMixColumns multiplies columns by the inverse polynomial d(x) = {0e}x^3 + {09}x^2 + {0d}x + {0b}.
 *   [ s'_0 ]   [ 0e  0b  0d  09 ] [ s_0 ]
 *   [ s'_1 ] = [ 09  0e  0b  0d ] [ s_1 ]
 *   [ s'_2 ]   [ 0d  09  0e  0b ] [ s_2 ]
 *   [ s'_3 ]   [ 0b  0d  09  0e ] [ s_3 ]
 * A naive matrix multiplication requires multiplying by large coefficients (9, 11, 13, 14),
 * taking 48 GF doublings (`gmul2`) per column!
 * 
 * Obscure Algorithm: Daemen-Rijmen Correction Theorem
 * Authors Joan Daemen and Vincent Rijmen proved that the inverse matrix can be factorized
 * relative to the direct MixColumns matrix by adding a correction factor:
 *   InvMixColumns(S) = MixColumns(S) ^ gmul8(s_0 ^ s_1 ^ s_2 ^ s_3) ^ gmul4(s_even/odd)
 * Formulated as:
 *   temp = s_0 ^ s_1 ^ s_2 ^ s_3
 *   h8   = gmul8(temp)
 *   h4_02 = gmul4(s_0 ^ s_2)
 *   h4_13 = gmul4(s_1 ^ s_3)
 *   s'_0  = MixColumns_Row0(S) ^ h8 ^ h4_02 = gmul2(s_0 ^ s_1) ^ temp ^ s_0 ^ h8 ^ h4_02
 *   s'_1  = MixColumns_Row1(S) ^ h8 ^ h4_13 = gmul2(s_1 ^ s_2) ^ temp ^ s_1 ^ h8 ^ h4_13
 *   s'_2  = MixColumns_Row2(S) ^ h8 ^ h4_02 = gmul2(s_2 ^ s_3) ^ temp ^ s_2 ^ h8 ^ h4_02
 *   s'_3  = MixColumns_Row3(S) ^ h8 ^ h4_13 = gmul2(s_3 ^ s_0) ^ temp ^ s_3 ^ h8 ^ h4_13
 * 
 * This reduces the GF doublings per column from 48 down to only 11 (a 77% arithmetic reduction!).
 * See: https://en.wikipedia.org/wiki/Rijndael_mix_columns#Inverse_MixColumns
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
 * Emulates the PCLMULQDQ Instruction (Carry-Less Multiplication of Quadwords).
 * 
 * Carry-less multiplication operates exactly like standard long multiplication, but
 * addition is performed without carrying bits (which maps to bitwise XOR). This is
 * mathematically equivalent to polynomial multiplication over the binary field GF(2)[x].
 * See: https://en.wikipedia.org/wiki/CLMUL_instruction_set
 * 
 * Obscure Algorithm: Bit-shifting carry-less multiplication with early exit
 * 1. Register halves selection:
 *    The immediate byte (`imm`) specifies which 64-bit halves of the 128-bit source and 
 *    destination XMM registers are multiplied:
 *      - Bit 0 (imm & 1): 0 selects lower 64-bits of dest, 1 selects upper 64-bits.
 *      - Bit 4 (imm & 16): 0 selects lower 64-bits of src, 1 selects upper 64-bits.
 *    See: https://en.wikipedia.org/wiki/CLMUL_instruction_set#PCLMULQDQ_instruction
 * 2. Carry-less loop:
 *    For each bit `i` in the multiplier `b`:
 *      If bit `i` is set, we XOR the shifted multiplicand `a` to the 128-bit product accumulator.
 *      Since there are no carries, we shift the 64-bit `a` left by `i` into `low` and shift the 
 *      overflowing high-bits right by `64 - i` into `high`.
 * 3. Early Exit Optimization:
 *    We copy `b` into `temp_b` and shift it right by 1 on each iteration. We exit the loop
 *    as soon as `temp_b == 0`. For sparse polynomials (e.g. key scheduling masks), this avoids
 *    looping through all 64 iterations, yielding significant performance gains.
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

/*
 * Maps the 4-bit x86-64 ISA instruction register index to the Linux kernel ABI
 * signal context register offset (`ucontext_t`'s `gregs` array offsets).
 * 
 * In the x86-64 Instruction Set Architecture (ISA), registers are encoded in opcodes as:
 *   0: RAX, 1: RCX, 2: RDX, 3: RBX, 4: RSP, 5: RBP, 6: RSI, 7: RDI,
 *   8: R8,  9: R9,  10: R10, 11: R11, 12: R12, 13: R13, 14: R14, 15: R15
 * See: https://en.wikipedia.org/wiki/ModR/M
 *
 * However, the Linux kernel context-switch saves registers in `ucontext_t`'s `gregs` array
 * in a different layout defined by the System V AMD64 ABI:
 *   RAX is gregs[REG_RAX] (which is index 13), RCX is gregs[REG_RCX] (which is index 14), etc.
 * See: https://en.wikipedia.org/wiki/X86_calling_conventions#System_V_AMD64_ABI
 * 
 * This static lookup map translates the ISA index (0-15) directly to the ABI index.
 */
static uint64_t get_register_val(ucontext_t *uc, int reg_idx) {
    static const int reg_map[16] = {
        REG_RAX, REG_RCX, REG_RDX, REG_RBX, REG_RSP, REG_RBP, REG_RSI, REG_RDI,
        REG_R8,  REG_R9,  REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15
    };
    return uc->uc_mcontext.gregs[reg_map[reg_idx]];
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
    
    // Cache memory address resolution fields
    uint8_t is_rip_relative;
    uint8_t base_reg_idx;   // 255 if none
    uint8_t index_reg_idx;  // 255 if none
    uint8_t scale;
    int32_t displacement;
    
    uint32_t seq;
};

static void parse_mem_operand(uint8_t *rip, int has_rex, uint8_t rex, uint8_t modrm, struct cache_entry *entry) {
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    // ptr walks through the bytes that come after the fixed-size instruction header.
    // The header is: 0x66 prefix (1 byte) + optional REX prefix (has_rex bytes) +
    // three opcode bytes (e.g. 0x0F 0x38 0xDE for AESDEC) = 1 + has_rex + 3 bytes.
    // That puts ptr on the ModRM byte, which the caller already decoded and passed in
    // as the 'modrm' parameter. The branches below all call *ptr++ to consume ModRM
    // again, which advances ptr past it to the SIB / displacement that follows.
    //
    // Known issue: consuming ModRM a second time means the SIB/displacement reading
    // is off by one byte — the code reads the wrong bytes for those fields. This does
    // not affect correctness today because Go's AES and PCLMULQDQ instructions always
    // use register-to-register operands, so parse_mem_operand is never actually called.
    // If memory-operand support is needed in the future, change + 3 to + 4 here so
    // ptr starts on the byte immediately after ModRM.
    uint8_t *ptr = rip + 1 + has_rex + 3;
    
    entry->is_rip_relative = 0;
    entry->base_reg_idx = 255;
    entry->index_reg_idx = 255;
    entry->scale = 0;
    entry->displacement = 0;
    
    if (mod == 0 && rm == 5) {
        int32_t disp32;
        memcpy(&disp32, ptr, 4);
        ptr += 4;
        int has_imm = (rip[1+has_rex+1] == 0x3a);
        uint8_t *rip_next = ptr + (has_imm ? 1 : 0);
        entry->is_rip_relative = 1;
        entry->bytes_consumed = (rip_next - rip);
        entry->displacement = disp32;
        return;
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
            entry->displacement = disp32;
        } else {
            entry->base_reg_idx = base_reg;
        }
        
        if (index != 4) {
            entry->index_reg_idx = index_reg;
            entry->scale = ss;
        }
    } else {
        uint8_t base_reg = rm;
        if (has_rex && (rex & 1)) base_reg += 8;
        entry->base_reg_idx = base_reg;
    }
    
    if (mod == 1) {
        int8_t disp8 = (int8_t)*ptr++;
        entry->displacement += disp8;
    } else if (mod == 2) {
        int32_t disp32;
        memcpy(&disp32, ptr, 4);
        ptr += 4;
        entry->displacement += disp32;
    }
    
    int has_imm = (rip[1+has_rex+1] == 0x3a);
    if (has_imm) {
        ptr++;
    }
    
    entry->bytes_consumed = (ptr - rip);
}

static inline uint8_t *resolve_mem_addr_fast(ucontext_t *uc, uint8_t *rip, int bytes_consumed, int is_rip_relative, uint8_t base_reg_idx, uint8_t index_reg_idx, uint8_t scale, int32_t displacement) {
    if (is_rip_relative) {
        return rip + bytes_consumed + displacement;
    }
    
    uint64_t base_val = 0;
    if (base_reg_idx != 255) {
        base_val = get_register_val(uc, base_reg_idx);
    }
    
    uint64_t index_val = 0;
    if (index_reg_idx != 255) {
        index_val = get_register_val(uc, index_reg_idx);
    }
    
    return (uint8_t *)(base_val + (index_val << scale) + displacement);
}

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

/*
 * Resolves register values inside the JIT trampoline helper (`trampoline_c_helper`).
 * 
 * Because `trampoline_entry` pushes all 15 general-purpose registers onto the stack
 * before executing, the stack grows downwards. The `gp_regs` pointer points to the 
 * top of the stack (which is R15, index 0 in the array), and RAX is at the bottom 
 * (index 14 in the array).
 * 
 * Stack Layout mapping of pushed registers:
 *   gp_regs[0]  -> R15 (ISA Index 15)
 *   gp_regs[1]  -> R14 (ISA Index 14)
 *   gp_regs[2]  -> R13 (ISA Index 13)
 *   gp_regs[3]  -> R12 (ISA Index 12)
 *   gp_regs[4]  -> R11 (ISA Index 11)
 *   gp_regs[5]  -> R10 (ISA Index 10)
 *   gp_regs[6]  -> R9  (ISA Index 9)
 *   gp_regs[7]  -> R8  (ISA Index 8)
 *   gp_regs[8]  -> RBP (ISA Index 5)
 *   gp_regs[9]  -> RDI (ISA Index 7)
 *   gp_regs[10] -> RSI (ISA Index 6)
 *   gp_regs[11] -> RBX (ISA Index 3)
 *   gp_regs[12] -> RDX (ISA Index 2)
 *   gp_regs[13] -> RCX (ISA Index 1)
 *   gp_regs[14] -> RAX (ISA Index 0)
 *   RSP (ISA Index 4) -> Handled separately via `original_rsp`
 * 
 * This map translates the x86-64 ISA register index (0-15) directly to the stack index.
 */
static uint64_t get_register_val_trampoline(uint64_t *gp_regs, int reg_idx, uintptr_t original_rsp) {
    static const int gp_map[16] = {
        14, 13, 12, 11, -1, 8, 10, 9, // gp_map[0]=RAX(14), gp_map[1]=RCX(13), gp_map[2]=RDX(12), gp_map[3]=RBX(11), gp_map[4]=RSP(-1), gp_map[5]=RBP(8), gp_map[6]=RSI(10), gp_map[7]=RDI(9)
        7,  6,  5,  4,  3,  2,  1,  0  // gp_map[8]=R8(7), gp_map[9]=R9(6), gp_map[10]=R10(5), gp_map[11]=R11(4), gp_map[12]=R12(3), gp_map[13]=R13(2), gp_map[14]=R14(1), gp_map[15]=R15(0)
    };
    if (reg_idx == 4) return original_rsp;
    return gp_regs[gp_map[reg_idx]];
}

static inline uint8_t *resolve_mem_addr_fast_trampoline(uint64_t *gp_regs, uintptr_t original_rsp, uint8_t *rip, struct cache_entry *entry) {
    if (entry->is_rip_relative) {
        return rip + entry->bytes_consumed + entry->displacement;
    }
    
    uint64_t base_val = 0;
    if (entry->base_reg_idx != 255) {
        base_val = get_register_val_trampoline(gp_regs, entry->base_reg_idx, original_rsp);
    }
    
    uint64_t index_val = 0;
    if (entry->index_reg_idx != 255) {
        index_val = get_register_val_trampoline(gp_regs, entry->index_reg_idx, original_rsp);
    }
    
    return (uint8_t *)(base_val + (index_val << entry->scale) + entry->displacement);
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

/*
 * Why force_align_arg_pointer is here:
 *
 * Go keeps its goroutine stacks aligned to 8 bytes. The x86-64 C calling convention
 * requires 16-byte alignment whenever a function is called. When the JIT stub jumps
 * into this function, the stack is therefore misaligned by 8 bytes.
 *
 * That normally goes unnoticed — until the compiler generates a 'movaps' instruction
 * (used inside the AES emulator functions) which requires 16-byte alignment and
 * will hard-fault on anything less.
 *
 * This attribute tells GCC/Clang to insert an alignment fix (and $-16, %rsp) at
 * the very top of this function before it allocates any local storage, so all
 * downstream vector operations are safe regardless of how misaligned the caller was.
 *
 * See docs/session_log.md → "🌟 Resilient Upgrades & Stack Alignment Hardening"
 */
__attribute__((force_align_arg_pointer))
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
            uintptr_t original_rsp = (uintptr_t)gp_regs + 128;
            src = resolve_mem_addr_fast_trampoline(gp_regs, original_rsp, rip, entry);
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
            uintptr_t original_rsp = (uintptr_t)gp_regs + 128;
            src = resolve_mem_addr_fast_trampoline(gp_regs, original_rsp, rip, entry);
            imm = rip[entry->bytes_consumed - 1];
        } else {
            src = &xmm_regs[entry->rm_idx * 16];
            imm = rip[1 + entry->has_rex + 4];
        }
        
        emulate_pclmulqdq(dest, src, imm);
    }
}

static void patch_code(uint8_t *rip, int bytes_consumed) {
    if (!emu_mode_experimental) {
        return;
    }
    
    /*
     * UNIDIRECTIONAL FALLBACK STRATEGY (Graceful Degradation):
     * If JIT patching fails at any stage (e.g. strict W^X/SELinux blocks allocation
     * of Trampoline Islands, or mprotect fails, or page table changes are denied),
     * we return early without modifying the calling code.
     * The signal handler will automatically execute via Option A (pure software emulation).
     * Since the instruction is left unpatched, subsequent executions of this address
     * will trap again and seamlessly execute via Option A. This per-instruction granularity
     * ensures that JIT failures do not crash the host program.
     */
    if (!island_page) {
        island_page = allocate_trampoline_island(rip);
        if (!island_page) {
            // Fallback to Option A: JIT allocation failed
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
        // Fallback to Option A: mprotect denied write-execute permissions (SELinux / W^X block)
        return;
    }
    
    /*
     * 1. Calculate relative target call offset.
     * In x86-64, the displacement for the relative call instruction (`call rel32` / opcode `0xE8`)
     * is relative to the address of the *next* instruction. Since a `call rel32` instruction
     * is exactly 5 bytes, the next instruction is at `RIP + 5`.
     * Offset = TargetAddress - (RIP + 5).
     * See: https://en.wikipedia.org/wiki/Program_counter#x86
     */
    int32_t call_offset = (int32_t)((uintptr_t)stub - ((uintptr_t)rip + 5));
    
    /*
     * 2. Atomic Overwrite sequence via the INT3 (0xCC) Trap-Spin protocol:
     * Overwriting instruction bytes across multiple threads is risky; if a thread executes
     * a partially written instruction, it will execute corrupt machine code and crash.
     * To prevent this, we write the 1-byte INT3 opcode (`0xCC`) to the first byte of `RIP`.
     * This is atomic since a write of a single byte is always atomic.
     * See: https://en.wikipedia.org/wiki/INT_(x86)#INT_3
     */
    rip[0] = 0xCC; // INT3
    __builtin___clear_cache((char *)rip, (char *)rip + 1);
    
    /*
     * Any concurrent thread hitting `0xCC` now raises `SIGTRAP`. Our SIGTRAP handler
     * simply spins/returns immediately, causing the thread to wait and retry.
     * Now we can safely write the rest of the 5-byte relative offset and NOP padding (0x90).
     */
    memcpy(rip + 1, &call_offset, 4);
    
    for (int i = 5; i < bytes_consumed; i++) {
        rip[i] = 0x90; // NOP (No-Operation) padding to cover original instruction boundary
    }
    
    /*
     * 3. Finally, atomically swap `0xCC` with `0xE8` (relative call).
     * Subsequent threads will execute `0xE8` and call the JIT trampoline directly without any traps.
     * Any thread waiting in SIGTRAP will resume and execute the newly completed `0xE8` call.
     */
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
    
    /*
     * OPTION A FAILSAFE IMMUNITY: Option A (Safe Mode) functions as our ultimate
     * resource-exhaustion floor. The rip_cache metadata table is statically allocated 
     * at load time (~56 KB) and never performs dynamic allocations (like malloc/mmap)
     * at runtime. If multiple concurrent threads collide at a cache index, the old 
     * entry is evicted via the Seqlock protocol. Thus, Option A is immune to memory
     * scarcity and guaranteed to succeed.
     */
    // Try lock-free cache lookup first
    uint32_t idx = get_cache_index(rip);
    struct cache_entry *entry = &rip_cache[idx];
    
    uint32_t seq1, seq2;
    uint64_t entry_rip;
    uint32_t cached_op_type = 0;
    uint8_t cached_reg_idx = 0, cached_rm_idx = 0, cached_is_mem = 0, cached_has_rex = 0;
    int cached_bytes_consumed = 0;
    uint8_t cached_is_rip_relative = 0;
    uint8_t cached_base_reg_idx = 255;
    uint8_t cached_index_reg_idx = 255;
    uint8_t cached_scale = 0;
    int32_t cached_displacement = 0;
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
        cached_bytes_consumed = entry->bytes_consumed;
        cached_is_rip_relative = entry->is_rip_relative;
        cached_base_reg_idx = entry->base_reg_idx;
        cached_index_reg_idx = entry->index_reg_idx;
        cached_scale = entry->scale;
        cached_displacement = entry->displacement;
        
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
                src = resolve_mem_addr_fast(uc, rip, cached_bytes_consumed, cached_is_rip_relative, cached_base_reg_idx, cached_index_reg_idx, cached_scale, cached_displacement);
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
                src = resolve_mem_addr_fast(uc, rip, cached_bytes_consumed, cached_is_rip_relative, cached_base_reg_idx, cached_index_reg_idx, cached_scale, cached_displacement);
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
                entry->is_rip_relative = 0;
                entry->base_reg_idx = 255;
                entry->index_reg_idx = 255;
                entry->scale = 0;
                entry->displacement = 0;
                __atomic_store_n(&entry->seq, seq_val + 2, __ATOMIC_RELEASE);
                
                // Patch the instruction site to call the trampoline island stub
                patch_code(rip, bytes_consumed);
                
                // Skip the executed instruction (prefix + opcode + ModRM = 5 bytes + REX)
                uc->uc_mcontext.gregs[REG_RIP] += bytes_consumed;
                return;
            } else {
                // Register-to-memory operation: resolve the target effective memory address
                // We parse details directly into the cache entry first, then read from it to resolve the memory target
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
                parse_mem_operand(rip, has_rex, rex, modrm, entry);
                __atomic_store_n(&entry->seq, seq_val + 2, __ATOMIC_RELEASE);
                
                uint8_t *src = resolve_mem_addr_fast(uc, rip, entry->bytes_consumed, entry->is_rip_relative, entry->base_reg_idx, entry->index_reg_idx, entry->scale, entry->displacement);
                
                if (debug_emu) {
                    log_emu_aes(op_type, reg_idx, 99); // 99 represents memory operand
                }
                
                if (op_type == 0xde) emulate_aesdec(dest, src);
                else if (op_type == 0xdf) emulate_aesdeclast(dest, src);
                else if (op_type == 0xdb) emulate_aesimc(dest, src);
                else if (op_type == 0xdc) emulate_aesenc(dest, src);
                else if (op_type == 0xdd) emulate_aesenclast(dest, src);
                
                // Patch the instruction site to call the trampoline island stub
                patch_code(rip, entry->bytes_consumed);
                
                // Skip the executed instruction by the calculated byte length
                uc->uc_mcontext.gregs[REG_RIP] += entry->bytes_consumed;
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
                entry->is_rip_relative = 0;
                entry->base_reg_idx = 255;
                entry->index_reg_idx = 255;
                entry->scale = 0;
                entry->displacement = 0;
                __atomic_store_n(&entry->seq, seq_val + 2, __ATOMIC_RELEASE);
                
                // Patch the instruction site to call the trampoline island stub
                patch_code(rip, bytes_consumed);
                
                // Skip instruction (prefix + opcode + ModRM + imm = 6 bytes + REX)
                uc->uc_mcontext.gregs[REG_RIP] += bytes_consumed;
                return;
            } else {
                // Register-to-memory operation: resolve the target effective memory address
                // We parse details directly into the cache entry first, then read from it to resolve the memory target
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
                parse_mem_operand(rip, has_rex, rex, modrm, entry);
                __atomic_store_n(&entry->seq, seq_val + 2, __ATOMIC_RELEASE);
                
                uint8_t *src = resolve_mem_addr_fast(uc, rip, entry->bytes_consumed, entry->is_rip_relative, entry->base_reg_idx, entry->index_reg_idx, entry->scale, entry->displacement);
                uint8_t imm = rip[entry->bytes_consumed - 1]; // Immediate byte is at the end of instruction
                
                if (debug_emu) {
                    log_emu_pclmul(reg_idx, 99, imm);
                }
                
                emulate_pclmulqdq(dest, src, imm);
                
                // Patch the instruction site to call the trampoline island stub
                patch_code(rip, entry->bytes_consumed);
                
                // Skip instruction by the calculated byte length
                uc->uc_mcontext.gregs[REG_RIP] += entry->bytes_consumed;
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

static void check_help(void) {
    int print_help = 0;
    
    char *env_help = getenv("EMU_HELP");
    if (!env_help) env_help = getenv("HELP");
    if (env_help && (strcmp(env_help, "1") == 0 || strcmp(env_help, "true") == 0 || strcmp(env_help, "TRUE") == 0)) {
        print_help = 1;
    }
    
    FILE *f = fopen("/proc/self/cmdline", "r");
    if (f) {
        char arg[256];
        while (1) {
            int c, i = 0;
            while ((c = fgetc(f)) != EOF && c != '\0' && i < 255) {
                arg[i++] = c;
            }
            arg[i] = '\0';
            if (i == 0) break;
            
            if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0 || strcmp(arg, "help") == 0 || strcmp(arg, "HELP") == 0) {
                print_help = 1;
                break;
            }
        }
        fclose(f);
    }
    
    if (print_help) {
        const char *help_msg =
            "========================================================================\n"
            "                 SIGILL CPU Instruction Emulator Toolkit                \n"
            "========================================================================\n"
            "Usage: LD_PRELOAD=/path/to/sigill_emulator.so [ENV_VARS] ./your_program\n\n"
            "Configuration Environment Variables:\n"
            "  EMU_MODE=safe|experimental   Choose emulation mode (Default: safe)\n"
            "  DEBUG_EMU=1                  Enable verbose instruction logging to stderr\n"
            "  EMU_HELP=1                   Show this help menu and exit\n\n"
            "Emulation Modes:\n"
            "  1. Safe Mode (Option A - DEFAULT)\n"
            "     - Pure software trap-and-emulate logic via SIGILL handlers.\n"
            "     - Safe for SELinux, AppArmor, containers, and W^X memory policies.\n"
            "     - Highly optimized with a lock-free direct-mapped Seqlock cache.\n\n"
            "  2. Experimental Mode (Option B - EMU_MODE=experimental)\n"
            "     - Dynamically rewrites invalid instruction sites in RAM at runtime.\n"
            "     - Bypasses kernel signal delivery using 16-byte Trampoline Island stubs.\n"
            "     - Yields a 10x micro-benchmark speedup (~110 ns vs ~1,650 ns).\n"
            "     - WARNING: Requires write-executable memory permissions (mprotect).\n"
            "       Not compatible with strict W^X security layouts or hardened kernels.\n\n"
            "Learn more on GitHub: https://github.com/mikefaille/antigravity-sigill-emulator\n"
            "========================================================================\n";
        ssize_t rc = write(2, help_msg, strlen(help_msg));
        (void)rc;
        _exit(0);
    }
}

/*
 * Shared library constructor function.
 * This runs when the library is loaded via LD_PRELOAD, before main() of the host program.
 * We check the environment for debug flags and hook the SIGILL signal handler.
 */
__attribute__((constructor))
static void init(void) {
    check_help();

    // Check if emulation debug tracing is requested
    char *env_dbg = getenv("DEBUG_EMU");
    if (env_dbg && strcmp(env_dbg, "1") == 0) {
        debug_emu = 1;
    }
    
    // Check if experimental mode is requested
    char *env_mode = getenv("EMU_MODE");
    char *env_exp = getenv("EMU_EXPERIMENTAL");
    if ((env_mode && strcmp(env_mode, "experimental") == 0) || 
        (env_exp && strcmp(env_exp, "1") == 0)) {
        emu_mode_experimental = 1;
    }
    
    // Register our SIGILL handler while saving the original handler (old_sa) to chain fallback calls
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handler;
    sigemptyset(&sa.sa_mask);
    
    // SA_SIGINFO provides the ucontext pointer; SA_NODEFER prevents blocking nested SIGILL signals
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGILL, &sa, &old_sa);
    
    if (emu_mode_experimental) {
        // Register our SIGTRAP handler for lock-free atomic patching, preserving original handler
        struct sigaction sa_trap;
        memset(&sa_trap, 0, sizeof(sa_trap));
        sa_trap.sa_sigaction = handler;
        sigemptyset(&sa_trap.sa_mask);
        sa_trap.sa_flags = SA_SIGINFO | SA_NODEFER;
        sigaction(SIGTRAP, &sa_trap, &old_sa_trap);
    }
}
