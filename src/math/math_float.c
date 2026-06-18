#include <simde/x86/avx2.h>
#include "math_backend.h"

void math_vaddps(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256 a = simde_mm256_loadu_ps((const float *)src1);
    simde__m256 b = simde_mm256_loadu_ps((const float *)src2);
    simde__m256 res = simde_mm256_add_ps(a, b);
    simde_mm256_storeu_ps((float *)dst, res);
}

void math_vmulps(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256 a = simde_mm256_loadu_ps((const float *)src1);
    simde__m256 b = simde_mm256_loadu_ps((const float *)src2);
    simde__m256 res = simde_mm256_mul_ps(a, b);
    simde_mm256_storeu_ps((float *)dst, res);
}

void math_vsubps(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256 a = simde_mm256_loadu_ps((const float *)src1);
    simde__m256 b = simde_mm256_loadu_ps((const float *)src2);
    simde__m256 res = simde_mm256_sub_ps(a, b);
    simde_mm256_storeu_ps((float *)dst, res);
}

void math_vdivps(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256 a = simde_mm256_loadu_ps((const float *)src1);
    simde__m256 b = simde_mm256_loadu_ps((const float *)src2);
    simde__m256 res = simde_mm256_div_ps(a, b);
    simde_mm256_storeu_ps((float *)dst, res);
}

void math_vaddpd(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
    simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
    simde__m256d res = simde_mm256_add_pd(a, b);
    simde_mm256_storeu_pd((double *)dst, res);
}

void math_vmulpd(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
    simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
    simde__m256d res = simde_mm256_mul_pd(a, b);
    simde_mm256_storeu_pd((double *)dst, res);
}

void math_vsubpd(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
    simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
    simde__m256d res = simde_mm256_sub_pd(a, b);
    simde_mm256_storeu_pd((double *)dst, res);
}

void math_vdivpd(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
    simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
    simde__m256d res = simde_mm256_div_pd(a, b);
    simde_mm256_storeu_pd((double *)dst, res);
}

void math_vaddss(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    simde__m128 a = simde_mm_loadu_ps((const float *)src1);
    simde__m128 b = simde_mm_loadu_ps((const float *)src2);
    simde__m128 res = simde_mm_add_ss(a, b);
    simde_mm_storeu_ps((float *)dst, res);
}

void math_vsubss(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    simde__m128 a = simde_mm_loadu_ps((const float *)src1);
    simde__m128 b = simde_mm_loadu_ps((const float *)src2);
    simde__m128 res = simde_mm_sub_ss(a, b);
    simde_mm_storeu_ps((float *)dst, res);
}

void math_vmulss(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    simde__m128 a = simde_mm_loadu_ps((const float *)src1);
    simde__m128 b = simde_mm_loadu_ps((const float *)src2);
    simde__m128 res = simde_mm_mul_ss(a, b);
    simde_mm_storeu_ps((float *)dst, res);
}

void math_vdivss(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    simde__m128 a = simde_mm_loadu_ps((const float *)src1);
    simde__m128 b = simde_mm_loadu_ps((const float *)src2);
    simde__m128 res = simde_mm_div_ss(a, b);
    simde_mm_storeu_ps((float *)dst, res);
}

void math_vaddsd(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    simde__m128d a = simde_mm_loadu_pd((const double *)src1);
    simde__m128d b = simde_mm_loadu_pd((const double *)src2);
    simde__m128d res = simde_mm_add_sd(a, b);
    simde_mm_storeu_pd((double *)dst, res);
}

void math_vsubsd(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    simde__m128d a = simde_mm_loadu_pd((const double *)src1);
    simde__m128d b = simde_mm_loadu_pd((const double *)src2);
    simde__m128d res = simde_mm_sub_sd(a, b);
    simde_mm_storeu_pd((double *)dst, res);
}

void math_vmulsd(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    simde__m128d a = simde_mm_loadu_pd((const double *)src1);
    simde__m128d b = simde_mm_loadu_pd((const double *)src2);
    simde__m128d res = simde_mm_mul_sd(a, b);
    simde_mm_storeu_pd((double *)dst, res);
}

void math_vdivsd(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    simde__m128d a = simde_mm_loadu_pd((const double *)src1);
    simde__m128d b = simde_mm_loadu_pd((const double *)src2);
    simde__m128d res = simde_mm_div_sd(a, b);
    simde_mm_storeu_pd((double *)dst, res);
}

void math_vbroadcastss(vec256 *dst, const vec128 *src, int vex_L) {
    simde__m128 a = simde_mm_loadu_ps((const float *)src);
    if (vex_L) {
        simde__m256 res = simde_mm256_broadcast_ss((const float *)&a);
        simde_mm256_storeu_ps((float *)dst, res);
    } else {
        simde__m128 res = simde_mm_shuffle_ps(a, a, 0); // Replicate low 32-bit element
        simde_mm_storeu_ps((float *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vbroadcastsd(vec256 *dst, const vec128 *src, int vex_L) {
    simde__m128d a = simde_mm_loadu_pd((const double *)src);
    if (vex_L) {
        simde__m256d res = simde_mm256_broadcast_sd((const double *)&a);
        simde_mm256_storeu_pd((double *)dst, res);
    } else {
        simde__m128d res = simde_mm_movedup_pd(a);
        simde_mm_storeu_pd((double *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vmovddup(vec256 *dst, const vec256 *src, int vex_L) {
    if (vex_L) {
        simde__m256d a = simde_mm256_loadu_pd((const double *)src);
        simde__m256d res = simde_mm256_movedup_pd(a);
        simde_mm256_storeu_pd((double *)dst, res);
    } else {
        simde__m128d a = simde_mm_loadu_pd((const double *)src);
        simde__m128d res = simde_mm_movedup_pd(a);
        simde_mm_storeu_pd((double *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpermilps(vec256 *dst, const vec256 *src, uint8_t imm, int vex_L) {
    float temp_lo[4];
    float temp_hi[4];
    memcpy(temp_lo, &src->lo, 16);
    memcpy(temp_hi, &src->hi, 16);
    
    vec256 out;
    memset(&out, 0, sizeof(vec256));
    
    for (int i = 0; i < 4; i++) {
        out.lo.f32[i] = temp_lo[(imm >> (i * 2)) & 3];
    }
    if (vex_L) {
        for (int i = 0; i < 4; i++) {
            out.hi.f32[i] = temp_hi[(imm >> (i * 2)) & 3];
        }
    } else {
        memset(&out.hi, 0, sizeof(vec128));
    }
    memcpy(dst, &out, sizeof(vec256));
}

void math_vshufps(vec256 *dst, const vec256 *src1, const vec256 *src2, uint8_t imm, int vex_L) {
    vec256 out;
    memset(&out, 0, sizeof(vec256));
    
    float s1_lo[4];
    float s2_lo[4];
    memcpy(s1_lo, &src1->lo, 16);
    memcpy(s2_lo, &src2->lo, 16);
    
    out.lo.f32[0] = s1_lo[imm & 3];
    out.lo.f32[1] = s1_lo[(imm >> 2) & 3];
    out.lo.f32[2] = s2_lo[(imm >> 4) & 3];
    out.lo.f32[3] = s2_lo[(imm >> 6) & 3];
    
    if (vex_L) {
        float s1_hi[4];
        float s2_hi[4];
        memcpy(s1_hi, &src1->hi, 16);
        memcpy(s2_hi, &src2->hi, 16);
        
        out.hi.f32[0] = s1_hi[imm & 3];
        out.hi.f32[1] = s1_hi[(imm >> 2) & 3];
        out.hi.f32[2] = s2_hi[(imm >> 4) & 3];
        out.hi.f32[3] = s2_hi[(imm >> 6) & 3];
    } else {
        memset(&out.hi, 0, sizeof(vec128));
    }
    memcpy(dst, &out, sizeof(vec256));
}

void math_vshufpd(vec256 *dst, const vec256 *src1, const vec256 *src2, uint8_t imm, int vex_L) {
    vec256 out;
    memset(&out, 0, sizeof(vec256));
    
    double s1_lo[2];
    double s2_lo[2];
    memcpy(s1_lo, &src1->lo, 16);
    memcpy(s2_lo, &src2->lo, 16);
    
    out.lo.f64[0] = s1_lo[imm & 1];
    out.lo.f64[1] = s2_lo[(imm >> 1) & 1];
    
    if (vex_L) {
        double s1_hi[2];
        double s2_hi[2];
        memcpy(s1_hi, &src1->hi, 16);
        memcpy(s2_hi, &src2->hi, 16);
        
        out.hi.f64[0] = s1_hi[(imm >> 2) & 1];
        out.hi.f64[1] = s2_hi[(imm >> 3) & 1];
    } else {
        memset(&out.hi, 0, sizeof(vec128));
    }
    memcpy(dst, &out, sizeof(vec256));
}

void math_vminss(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    vec128 out;
    memcpy(&out, src1, 16);
    float a = src1->f32[0];
    float b = src2->f32[0];
    out.f32[0] = (a < b) ? a : b;
    memcpy(dst, &out, 16);
}

void math_vmaxss(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    vec128 out;
    memcpy(&out, src1, 16);
    float a = src1->f32[0];
    float f = src2->f32[0];
    out.f32[0] = (a > f) ? a : f;
    memcpy(dst, &out, 16);
}

void math_vminsd(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    vec128 out;
    memcpy(&out, src1, 16);
    double a = src1->f64[0];
    double b = src2->f64[0];
    out.f64[0] = (a < b) ? a : b;
    memcpy(dst, &out, 16);
}

void math_vmaxsd(vec128 *dst, const vec128 *src1, const vec128 *src2) {
    vec128 out;
    memcpy(&out, src1, 16);
    double a = src1->f64[0];
    double f = src2->f64[0];
    out.f64[0] = (a > f) ? a : f;
    memcpy(dst, &out, 16);
}

void math_vminps(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256 a = simde_mm256_loadu_ps((const float *)src1);
    simde__m256 b = simde_mm256_loadu_ps((const float *)src2);
    simde__m256 res = simde_mm256_min_ps(a, b);
    simde_mm256_storeu_ps((float *)dst, res);
}

void math_vmaxps(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256 a = simde_mm256_loadu_ps((const float *)src1);
    simde__m256 b = simde_mm256_loadu_ps((const float *)src2);
    simde__m256 res = simde_mm256_max_ps(a, b);
    simde_mm256_storeu_ps((float *)dst, res);
}

void math_vminpd(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
    simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
    simde__m256d res = simde_mm256_min_pd(a, b);
    simde_mm256_storeu_pd((double *)dst, res);
}

void math_vmaxpd(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
    simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
    simde__m256d res = simde_mm256_max_pd(a, b);
    simde_mm256_storeu_pd((double *)dst, res);
}

void math_vroundss(vec128 *dst, const vec128 *src1, const vec128 *src2, uint8_t imm) {
    simde__m128 a = simde_mm_loadu_ps((const float *)src1);
    simde__m128 b = simde_mm_loadu_ps((const float *)src2);
    simde__m128 res;
    switch (imm & 7) {
        case 0: res = simde_mm_round_ss(a, b, 0); break;
        case 1: res = simde_mm_round_ss(a, b, 1); break;
        case 2: res = simde_mm_round_ss(a, b, 2); break;
        case 3: res = simde_mm_round_ss(a, b, 3); break;
        case 4: res = simde_mm_round_ss(a, b, 4); break;
        case 5: res = simde_mm_round_ss(a, b, 5); break;
        case 6: res = simde_mm_round_ss(a, b, 6); break;
        case 7: res = simde_mm_round_ss(a, b, 7); break;
        default: res = a; break;
    }
    simde_mm_storeu_ps((float *)dst, res);
}

void math_vroundsd(vec128 *dst, const vec128 *src1, const vec128 *src2, uint8_t imm) {
    simde__m128d a = simde_mm_loadu_pd((const double *)src1);
    simde__m128d b = simde_mm_loadu_pd((const double *)src2);
    simde__m128d res;
    switch (imm & 7) {
        case 0: res = simde_mm_round_sd(a, b, 0); break;
        case 1: res = simde_mm_round_sd(a, b, 1); break;
        case 2: res = simde_mm_round_sd(a, b, 2); break;
        case 3: res = simde_mm_round_sd(a, b, 3); break;
        case 4: res = simde_mm_round_sd(a, b, 4); break;
        case 5: res = simde_mm_round_sd(a, b, 5); break;
        case 6: res = simde_mm_round_sd(a, b, 6); break;
        case 7: res = simde_mm_round_sd(a, b, 7); break;
        default: res = a; break;
    }
    simde_mm_storeu_pd((double *)dst, res);
}

void math_vroundps(vec256 *dst, const vec256 *src, uint8_t imm, int vex_L) {
    if (vex_L) {
        simde__m256 a = simde_mm256_loadu_ps((const float *)src);
        simde__m256 res;
        switch (imm & 7) {
            case 0: res = simde_mm256_round_ps(a, 0); break;
            case 1: res = simde_mm256_round_ps(a, 1); break;
            case 2: res = simde_mm256_round_ps(a, 2); break;
            case 3: res = simde_mm256_round_ps(a, 3); break;
            case 4: res = simde_mm256_round_ps(a, 4); break;
            case 5: res = simde_mm256_round_ps(a, 5); break;
            case 6: res = simde_mm256_round_ps(a, 6); break;
            case 7: res = simde_mm256_round_ps(a, 7); break;
            default: res = a; break;
        }
        simde_mm256_storeu_ps((float *)dst, res);
    } else {
        simde__m128 a = simde_mm_loadu_ps((const float *)src);
        simde__m128 res;
        switch (imm & 7) {
            case 0: res = simde_mm_round_ps(a, 0); break;
            case 1: res = simde_mm_round_ps(a, 1); break;
            case 2: res = simde_mm_round_ps(a, 2); break;
            case 3: res = simde_mm_round_ps(a, 3); break;
            case 4: res = simde_mm_round_ps(a, 4); break;
            case 5: res = simde_mm_round_ps(a, 5); break;
            case 6: res = simde_mm_round_ps(a, 6); break;
            case 7: res = simde_mm_round_ps(a, 7); break;
            default: res = a; break;
        }
        simde_mm_storeu_ps((float *)dst, res);
    }
}

void math_vroundpd(vec256 *dst, const vec256 *src, uint8_t imm, int vex_L) {
    if (vex_L) {
        simde__m256d a = simde_mm256_loadu_pd((const double *)src);
        simde__m256d res;
        switch (imm & 7) {
            case 0: res = simde_mm256_round_pd(a, 0); break;
            case 1: res = simde_mm256_round_pd(a, 1); break;
            case 2: res = simde_mm256_round_pd(a, 2); break;
            case 3: res = simde_mm256_round_pd(a, 3); break;
            case 4: res = simde_mm256_round_pd(a, 4); break;
            case 5: res = simde_mm256_round_pd(a, 5); break;
            case 6: res = simde_mm256_round_pd(a, 6); break;
            case 7: res = simde_mm256_round_pd(a, 7); break;
            default: res = a; break;
        }
        simde_mm256_storeu_pd((double *)dst, res);
    } else {
        simde__m128d a = simde_mm_loadu_pd((const double *)src);
        simde__m128d res;
        switch (imm & 7) {
            case 0: res = simde_mm_round_pd(a, 0); break;
            case 1: res = simde_mm_round_pd(a, 1); break;
            case 2: res = simde_mm_round_pd(a, 2); break;
            case 3: res = simde_mm_round_pd(a, 3); break;
            case 4: res = simde_mm_round_pd(a, 4); break;
            case 5: res = simde_mm_round_pd(a, 5); break;
            case 6: res = simde_mm_round_pd(a, 6); break;
            case 7: res = simde_mm_round_pd(a, 7); break;
            default: res = a; break;
        }
        simde_mm_storeu_pd((double *)dst, res);
    }
}

#define CMP_CASE_PS_256(val) \
    case val: res = simde_mm256_cmp_ps(a, b, val); break;

#define CMP_CASE_PS_128(val) \
    case val: res = simde_mm_cmp_ps(a, b, val); break;

void math_vcmpps(vec256 *dst, const vec256 *src1, const vec256 *src2, uint8_t imm, int vex_L) {
    uint8_t pred = imm & 31;
    if (vex_L) {
        simde__m256 a = simde_mm256_loadu_ps((const float *)src1);
        simde__m256 b = simde_mm256_loadu_ps((const float *)src2);
        simde__m256 res;
        switch (pred) {
            CMP_CASE_PS_256(0)  CMP_CASE_PS_256(1)  CMP_CASE_PS_256(2)  CMP_CASE_PS_256(3)
            CMP_CASE_PS_256(4)  CMP_CASE_PS_256(5)  CMP_CASE_PS_256(6)  CMP_CASE_PS_256(7)
            CMP_CASE_PS_256(8)  CMP_CASE_PS_256(9)  CMP_CASE_PS_256(10) CMP_CASE_PS_256(11)
            CMP_CASE_PS_256(12) CMP_CASE_PS_256(13) CMP_CASE_PS_256(14) CMP_CASE_PS_256(15)
            CMP_CASE_PS_256(16) CMP_CASE_PS_256(17) CMP_CASE_PS_256(18) CMP_CASE_PS_256(19)
            CMP_CASE_PS_256(20) CMP_CASE_PS_256(21) CMP_CASE_PS_256(22) CMP_CASE_PS_256(23)
            CMP_CASE_PS_256(24) CMP_CASE_PS_256(25) CMP_CASE_PS_256(26) CMP_CASE_PS_256(27)
            CMP_CASE_PS_256(28) CMP_CASE_PS_256(29) CMP_CASE_PS_256(30) CMP_CASE_PS_256(31)
            default: res = simde_mm256_setzero_ps(); break;
        }
        simde_mm256_storeu_ps((float *)dst, res);
    } else {
        simde__m128 a = simde_mm_loadu_ps((const float *)src1);
        simde__m128 b = simde_mm_loadu_ps((const float *)src2);
        simde__m128 res;
        switch (pred) {
            CMP_CASE_PS_128(0)  CMP_CASE_PS_128(1)  CMP_CASE_PS_128(2)  CMP_CASE_PS_128(3)
            CMP_CASE_PS_128(4)  CMP_CASE_PS_128(5)  CMP_CASE_PS_128(6)  CMP_CASE_PS_128(7)
            CMP_CASE_PS_128(8)  CMP_CASE_PS_128(9)  CMP_CASE_PS_128(10) CMP_CASE_PS_128(11)
            CMP_CASE_PS_128(12) CMP_CASE_PS_128(13) CMP_CASE_PS_128(14) CMP_CASE_PS_128(15)
            CMP_CASE_PS_128(16) CMP_CASE_PS_128(17) CMP_CASE_PS_128(18) CMP_CASE_PS_128(19)
            CMP_CASE_PS_128(20) CMP_CASE_PS_128(21) CMP_CASE_PS_128(22) CMP_CASE_PS_128(23)
            CMP_CASE_PS_128(24) CMP_CASE_PS_128(25) CMP_CASE_PS_128(26) CMP_CASE_PS_128(27)
            CMP_CASE_PS_128(28) CMP_CASE_PS_128(29) CMP_CASE_PS_128(30) CMP_CASE_PS_128(31)
            default: res = simde_mm_setzero_ps(); break;
        }
        simde_mm_storeu_ps((float *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

#define CMP_CASE_PD_256(val) \
    case val: res = simde_mm256_cmp_pd(a, b, val); break;

#define CMP_CASE_PD_128(val) \
    case val: res = simde_mm_cmp_pd(a, b, val); break;

void math_vcmppd(vec256 *dst, const vec256 *src1, const vec256 *src2, uint8_t imm, int vex_L) {
    uint8_t pred = imm & 31;
    if (vex_L) {
        simde__m256d a = simde_mm256_loadu_pd((const double *)src1);
        simde__m256d b = simde_mm256_loadu_pd((const double *)src2);
        simde__m256d res;
        switch (pred) {
            CMP_CASE_PD_256(0)  CMP_CASE_PD_256(1)  CMP_CASE_PD_256(2)  CMP_CASE_PD_256(3)
            CMP_CASE_PD_256(4)  CMP_CASE_PD_256(5)  CMP_CASE_PD_256(6)  CMP_CASE_PD_256(7)
            CMP_CASE_PD_256(8)  CMP_CASE_PD_256(9)  CMP_CASE_PD_256(10) CMP_CASE_PD_256(11)
            CMP_CASE_PD_256(12) CMP_CASE_PD_256(13) CMP_CASE_PD_256(14) CMP_CASE_PD_256(15)
            CMP_CASE_PD_256(16) CMP_CASE_PD_256(17) CMP_CASE_PD_256(18) CMP_CASE_PD_256(19)
            CMP_CASE_PD_256(20) CMP_CASE_PD_256(21) CMP_CASE_PD_256(22) CMP_CASE_PD_256(23)
            CMP_CASE_PD_256(24) CMP_CASE_PD_256(25) CMP_CASE_PD_256(26) CMP_CASE_PD_256(27)
            CMP_CASE_PD_256(28) CMP_CASE_PD_256(29) CMP_CASE_PD_256(30) CMP_CASE_PD_256(31)
            default: res = simde_mm256_setzero_pd(); break;
        }
        simde_mm256_storeu_pd((double *)dst, res);
    } else {
        simde__m128d a = simde_mm_loadu_pd((const double *)src1);
        simde__m128d b = simde_mm_loadu_pd((const double *)src2);
        simde__m128d res;
        switch (pred) {
            CMP_CASE_PD_128(0)  CMP_CASE_PD_128(1)  CMP_CASE_PD_128(2)  CMP_CASE_PD_128(3)
            CMP_CASE_PD_128(4)  CMP_CASE_PD_128(5)  CMP_CASE_PD_128(6)  CMP_CASE_PD_128(7)
            CMP_CASE_PD_128(8)  CMP_CASE_PD_128(9)  CMP_CASE_PD_128(10) CMP_CASE_PD_128(11)
            CMP_CASE_PD_128(12) CMP_CASE_PD_128(13) CMP_CASE_PD_128(14) CMP_CASE_PD_128(15)
            CMP_CASE_PD_128(16) CMP_CASE_PD_128(17) CMP_CASE_PD_128(18) CMP_CASE_PD_128(19)
            CMP_CASE_PD_128(20) CMP_CASE_PD_128(21) CMP_CASE_PD_128(22) CMP_CASE_PD_128(23)
            CMP_CASE_PD_128(24) CMP_CASE_PD_128(25) CMP_CASE_PD_128(26) CMP_CASE_PD_128(27)
            CMP_CASE_PD_128(28) CMP_CASE_PD_128(29) CMP_CASE_PD_128(30) CMP_CASE_PD_128(31)
            default: res = simde_mm_setzero_pd(); break;
        }
        simde_mm_storeu_pd((double *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

#define CMP_CASE_SS(val) \
    case val: res = simde_mm_cmp_ss(a, b, val); break;

void math_vcmpss(vec128 *dst, const vec128 *src1, const vec128 *src2, uint8_t imm) {
    uint8_t pred = imm & 31;
    simde__m128 a = simde_mm_loadu_ps((const float *)src1);
    simde__m128 b = simde_mm_loadu_ps((const float *)src2);
    simde__m128 res;
    switch (pred) {
        CMP_CASE_SS(0)  CMP_CASE_SS(1)  CMP_CASE_SS(2)  CMP_CASE_SS(3)
        CMP_CASE_SS(4)  CMP_CASE_SS(5)  CMP_CASE_SS(6)  CMP_CASE_SS(7)
        CMP_CASE_SS(8)  CMP_CASE_SS(9)  CMP_CASE_SS(10) CMP_CASE_SS(11)
        CMP_CASE_SS(12) CMP_CASE_SS(13) CMP_CASE_SS(14) CMP_CASE_SS(15)
        CMP_CASE_SS(16) CMP_CASE_SS(17) CMP_CASE_SS(18) CMP_CASE_SS(19)
        CMP_CASE_SS(20) CMP_CASE_SS(21) CMP_CASE_SS(22) CMP_CASE_SS(23)
        CMP_CASE_SS(24) CMP_CASE_SS(25) CMP_CASE_SS(26) CMP_CASE_SS(27)
        CMP_CASE_SS(28) CMP_CASE_SS(29) CMP_CASE_SS(30) CMP_CASE_SS(31)
        default: res = simde_mm_setzero_ps(); break;
    }
    simde_mm_storeu_ps((float *)dst, res);
}

#define CMP_CASE_SD(val) \
    case val: res = simde_mm_cmp_sd(a, b, val); break;

void math_vcmpsd(vec128 *dst, const vec128 *src1, const vec128 *src2, uint8_t imm) {
    uint8_t pred = imm & 31;
    simde__m128d a = simde_mm_loadu_pd((const double *)src1);
    simde__m128d b = simde_mm_loadu_pd((const double *)src2);
    simde__m128d res;
    switch (pred) {
        CMP_CASE_SD(0)  CMP_CASE_SD(1)  CMP_CASE_SD(2)  CMP_CASE_SD(3)
        CMP_CASE_SD(4)  CMP_CASE_SD(5)  CMP_CASE_SD(6)  CMP_CASE_SD(7)
        CMP_CASE_SD(8)  CMP_CASE_SD(9)  CMP_CASE_SD(10) CMP_CASE_SD(11)
        CMP_CASE_SD(12) CMP_CASE_SD(13) CMP_CASE_SD(14) CMP_CASE_SD(15)
        CMP_CASE_SD(16) CMP_CASE_SD(17) CMP_CASE_SD(18) CMP_CASE_SD(19)
        CMP_CASE_SD(20) CMP_CASE_SD(21) CMP_CASE_SD(22) CMP_CASE_SD(23)
        CMP_CASE_SD(24) CMP_CASE_SD(25) CMP_CASE_SD(26) CMP_CASE_SD(27)
        CMP_CASE_SD(28) CMP_CASE_SD(29) CMP_CASE_SD(30) CMP_CASE_SD(31)
        default: res = simde_mm_setzero_pd(); break;
    }
    simde_mm_storeu_pd((double *)dst, res);
}
