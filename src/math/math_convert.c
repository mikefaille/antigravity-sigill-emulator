#include <simde/x86/avx2.h>
#include "math_backend.h"

void math_vcvtsi2sd(vec128 *dst, const vec128 *src1, uint64_t val, int vex_W) {
    memcpy(dst, src1, sizeof(vec128));
    if (vex_W) {
        dst->f64[0] = (double)((int64_t)val);
    } else {
        dst->f64[0] = (double)((int32_t)val);
    }
}

void math_vcvtsi2ss(vec128 *dst, const vec128 *src1, uint64_t val, int vex_W) {
    memcpy(dst, src1, sizeof(vec128));
    if (vex_W) {
        dst->f32[0] = (float)((int64_t)val);
    } else {
        dst->f32[0] = (float)((int32_t)val);
    }
}

uint64_t math_vcvttsd2si(const vec128 *src, int vex_W) {
    if (vex_W) {
        return (int64_t)(src->f64[0]);
    } else {
        return (uint32_t)((int32_t)(src->f64[0]));
    }
}

uint64_t math_vcvttss2si(const vec128 *src, int vex_W) {
    if (vex_W) {
        return (int64_t)(src->f32[0]);
    } else {
        return (uint32_t)((int32_t)(src->f32[0]));
    }
}

void math_vcvttps2dq(vec256 *dst, const vec256 *src, int vex_L) {
    if (vex_L) {
        simde__m256 a = simde_mm256_loadu_ps((const float *)src);
        simde__m256i res = simde_mm256_cvttps_epi32(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128 a = simde_mm_loadu_ps((const float *)src);
        simde__m128i res = simde_mm_cvttps_epi32(a);
        simde_mm_storeu_si128((simde__m128i *)dst, res);
    }
}

void math_vcvtps2dq(vec256 *dst, const vec256 *src, int vex_L) {
    if (vex_L) {
        simde__m256 a = simde_mm256_loadu_ps((const float *)src);
        simde__m256i res = simde_mm256_cvtps_epi32(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128 a = simde_mm_loadu_ps((const float *)src);
        simde__m128i res = simde_mm_cvtps_epi32(a);
        simde_mm_storeu_si128((simde__m128i *)dst, res);
    }
}

void math_vcvtdq2ps(vec256 *dst, const vec256 *src, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
        simde__m256 res = simde_mm256_cvtepi32_ps(a);
        simde_mm256_storeu_ps((float *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
        simde__m128 res = simde_mm_cvtepi32_ps(a);
        simde_mm_storeu_ps((float *)dst, res);
    }
}
