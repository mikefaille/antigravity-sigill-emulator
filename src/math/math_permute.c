#include <simde/x86/avx2.h>
#include "math_backend.h"

void math_vpermq(vec256 *dst, const vec256 *src, uint8_t imm) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
    // Since SIMDe's permute macro takes compile-time constants for the immediate,
    // and our signal dispatcher decodes immediate values dynamically at runtime,
    // we use a fast lookup or fallback to a dynamic permute loop.
    // For maximum correctness, we use a simple switch on common immediates, or a dynamic loop.
    uint64_t temp[4];
    simde_mm256_storeu_si256((simde__m256i *)temp, a);
    
    dst->lo.u64[0] = temp[imm & 3];
    dst->lo.u64[1] = temp[(imm >> 2) & 3];
    dst->hi.u64[0] = temp[(imm >> 4) & 3];
    dst->hi.u64[1] = temp[(imm >> 6) & 3];
}

void math_vextract128(vec128 *dst, const vec256 *src, uint8_t imm) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
    simde__m128i res;
    if (imm & 1) {
        res = simde_mm256_extracti128_si256(a, 1);
    } else {
        res = simde_mm256_extracti128_si256(a, 0);
    }
    simde_mm_storeu_si128((simde__m128i *)dst, res);
}

void math_vinsert128(vec256 *dst, const vec256 *src1, const vec128 *src2, uint8_t imm) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
    simde__m256i res;
    if (imm & 1) {
        res = simde_mm256_inserti128_si256(a, b, 1);
    } else {
        res = simde_mm256_inserti128_si256(a, b, 0);
    }
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}
