#include <simde/x86/avx.h>
#include <simde/x86/avx2.h>
#include <string.h>
#include "math_backend.h"

/* Pinned SIMDe Version: 0.8.2
 * Upstream Release Tag: v0.8.2
 */

void math_vandpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
        simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
        simde__m256d res = simde_mm256_and_pd(a, b);
        simde_mm256_storeu_pd((double *)dst, res);
    } else {
        simde__m128d a = simde_mm_loadu_pd((const double *)src1);
        simde__m128d b = simde_mm_loadu_pd((const double *)src2);
        simde__m128d res = simde_mm_and_pd(a, b);
        simde_mm_storeu_pd((double *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vandnpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
        simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
        simde__m256d res = simde_mm256_andnot_pd(a, b);
        simde_mm256_storeu_pd((double *)dst, res);
    } else {
        simde__m128d a = simde_mm_loadu_pd((const double *)src1);
        simde__m128d b = simde_mm_loadu_pd((const double *)src2);
        simde__m128d res = simde_mm_andnot_pd(a, b);
        simde_mm_storeu_pd((double *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vorpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
        simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
        simde__m256d res = simde_mm256_or_pd(a, b);
        simde_mm256_storeu_pd((double *)dst, res);
    } else {
        simde__m128d a = simde_mm_loadu_pd((const double *)src1);
        simde__m128d b = simde_mm_loadu_pd((const double *)src2);
        simde__m128d res = simde_mm_or_pd(a, b);
        simde_mm_storeu_pd((double *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vxorpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
        simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
        simde__m256d res = simde_mm256_xor_pd(a, b);
        simde_mm256_storeu_pd((double *)dst, res);
    } else {
        simde__m128d a = simde_mm_loadu_pd((const double *)src1);
        simde__m128d b = simde_mm_loadu_pd((const double *)src2);
        simde__m128d res = simde_mm_xor_pd(a, b);
        simde_mm_storeu_pd((double *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vmovshdup(vec256 *dst, const vec256 *src, int vex_L) {
    if (vex_L) {
        simde__m256 a = simde_mm256_loadu_ps((const float *)src);
        simde__m256 res = simde_mm256_movehdup_ps(a);
        simde_mm256_storeu_ps((float *)dst, res);
    } else {
        simde__m128 a = simde_mm_loadu_ps((const float *)src);
        simde__m128 res = simde_mm_movehdup_ps(a);
        simde_mm_storeu_ps((float *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vmovsldup(vec256 *dst, const vec256 *src, int vex_L) {
    if (vex_L) {
        simde__m256 a = simde_mm256_loadu_ps((const float *)src);
        simde__m256 res = simde_mm256_moveldup_ps(a);
        simde_mm256_storeu_ps((float *)dst, res);
    } else {
        simde__m128 a = simde_mm_loadu_ps((const float *)src);
        simde__m128 res = simde_mm_moveldup_ps(a);
        simde_mm_storeu_ps((float *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vunpckhps(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256 a = simde_mm256_loadu_ps((const float *)src1);
        simde__m256 b = simde_mm256_loadu_ps((const float *)src2);
        simde__m256 res = simde_mm256_unpackhi_ps(a, b);
        simde_mm256_storeu_ps((float *)dst, res);
    } else {
        simde__m128 a = simde_mm_loadu_ps((const float *)src1);
        simde__m128 b = simde_mm_loadu_ps((const float *)src2);
        simde__m128 res = simde_mm_unpackhi_ps(a, b);
        simde_mm_storeu_ps((float *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vunpcklps(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256 a = simde_mm256_loadu_ps((const float *)src1);
        simde__m256 b = simde_mm256_loadu_ps((const float *)src2);
        simde__m256 res = simde_mm256_unpacklo_ps(a, b);
        simde_mm256_storeu_ps((float *)dst, res);
    } else {
        simde__m128 a = simde_mm_loadu_ps((const float *)src1);
        simde__m128 b = simde_mm_loadu_ps((const float *)src2);
        simde__m128 res = simde_mm_unpacklo_ps(a, b);
        simde_mm_storeu_ps((float *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vunpckhpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
        simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
        simde__m256d res = simde_mm256_unpackhi_pd(a, b);
        simde_mm256_storeu_pd((double *)dst, res);
    } else {
        simde__m128d a = simde_mm_loadu_pd((const double *)src1);
        simde__m128d b = simde_mm_loadu_pd((const double *)src2);
        simde__m128d res = simde_mm_unpackhi_pd(a, b);
        simde_mm_storeu_pd((double *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vunpcklpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
        simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
        simde__m256d res = simde_mm256_unpacklo_pd(a, b);
        simde_mm256_storeu_pd((double *)dst, res);
    } else {
        simde__m128d a = simde_mm_loadu_pd((const double *)src1);
        simde__m128d b = simde_mm_loadu_pd((const double *)src2);
        simde__m128d res = simde_mm_unpacklo_pd(a, b);
        simde_mm_storeu_pd((double *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vaddsubps(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256 a = simde_mm256_loadu_ps((const float *)src1);
        simde__m256 b = simde_mm256_loadu_ps((const float *)src2);
        simde__m256 res = simde_mm256_addsub_ps(a, b);
        simde_mm256_storeu_ps((float *)dst, res);
    } else {
        simde__m128 a = simde_mm_loadu_ps((const float *)src1);
        simde__m128 b = simde_mm_loadu_ps((const float *)src2);
        simde__m128 res = simde_mm_addsub_ps(a, b);
        simde_mm_storeu_ps((float *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vaddsubpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
        simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
        simde__m256d res = simde_mm256_addsub_pd(a, b);
        simde_mm256_storeu_pd((double *)dst, res);
    } else {
        simde__m128d a = simde_mm_loadu_pd((const double *)src1);
        simde__m128d b = simde_mm_loadu_pd((const double *)src2);
        simde__m128d res = simde_mm_addsub_pd(a, b);
        simde_mm_storeu_pd((double *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}
