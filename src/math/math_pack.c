#include <simde/x86/avx2.h>
#include "math_backend.h"

void math_vpacksswb(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res = simde_mm256_packs_epi16(a, b);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpackuswb(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res = simde_mm256_packus_epi16(a, b);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpackssdw(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res = simde_mm256_packs_epi32(a, b);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpackusdw(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res = simde_mm256_packus_epi32(a, b);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}
