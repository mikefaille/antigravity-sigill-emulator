#include <simde/x86/avx2.h>
#include "math_backend.h"

void math_vpxor(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res = simde_mm256_xor_si256(a, b);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpand(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res = simde_mm256_and_si256(a, b);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpor(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res = simde_mm256_or_si256(a, b);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpandn(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res = simde_mm256_andnot_si256(a, b);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpbroadcastb(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_broadcastb_epi8(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_broadcastb_epi8(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpbroadcastw(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_broadcastw_epi16(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_broadcastw_epi16(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpbroadcastd(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_broadcastd_epi32(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_broadcastd_epi32(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpbroadcastq(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_broadcastq_epi64(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_broadcastq_epi64(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpsllq(vec256 *dst, const vec256 *src, uint8_t imm) {
    // Dynamic immediate values can be handled by non-const versions or passing to intrinsic
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
    simde__m256i res = simde_mm256_slli_epi64(a, imm);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpsrlq(vec256 *dst, const vec256 *src, uint8_t imm) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
    simde__m256i res = simde_mm256_srli_epi64(a, imm);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpcmpeqq(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res = simde_mm256_cmpeq_epi64(a, b);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpshufd(vec256 *dst, const vec256 *src, uint8_t imm, int vex_L) {
    uint32_t temp_lo[4];
    uint32_t temp_hi[4];
    memcpy(temp_lo, &src->lo, 16);
    memcpy(temp_hi, &src->hi, 16);
    
    vec256 out;
    memset(&out, 0, sizeof(vec256));
    
    for (int i = 0; i < 4; i++) {
        out.lo.u32[i] = temp_lo[(imm >> (i * 2)) & 3];
    }
    if (vex_L) {
        for (int i = 0; i < 4; i++) {
            out.hi.u32[i] = temp_hi[(imm >> (i * 2)) & 3];
        }
    } else {
        memset(&out.hi, 0, sizeof(vec128));
    }
    memcpy(dst, &out, sizeof(vec256));
}

void math_vpshufb(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res;
    if (vex_L) {
        res = simde_mm256_shuffle_epi8(a, b);
    } else {
        simde__m128i al = simde_mm256_castsi256_si128(a);
        simde__m128i bl = simde_mm256_castsi256_si128(b);
        simde__m128i rl = simde_mm_shuffle_epi8(al, bl);
        res = simde_mm256_castsi128_si256(rl);
    }
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpunpcklqdq(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res = simde_mm256_unpacklo_epi64(a, b);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpunpckhqdq(vec256 *dst, const vec256 *src1, const vec256 *src2) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
    simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
    simde__m256i res = simde_mm256_unpackhi_epi64(a, b);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpsllw(vec256 *dst, const vec256 *src, uint8_t imm) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
    simde__m256i res = simde_mm256_slli_epi16(a, imm);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpsrlw(vec256 *dst, const vec256 *src, uint8_t imm) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
    simde__m256i res = simde_mm256_srli_epi16(a, imm);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpsraw(vec256 *dst, const vec256 *src, uint8_t imm) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
    simde__m256i res = simde_mm256_srai_epi16(a, imm);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpslld(vec256 *dst, const vec256 *src, uint8_t imm) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
    simde__m256i res = simde_mm256_slli_epi32(a, imm);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpsrld(vec256 *dst, const vec256 *src, uint8_t imm) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
    simde__m256i res = simde_mm256_srli_epi32(a, imm);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

void math_vpsrad(vec256 *dst, const vec256 *src, uint8_t imm) {
    simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
    simde__m256i res = simde_mm256_srai_epi32(a, imm);
    simde_mm256_storeu_si256((simde__m256i *)dst, res);
}

uint32_t math_vpmovmskb(const vec256 *src, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src);
        return (uint32_t)simde_mm256_movemask_epi8(a);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
        return (uint32_t)simde_mm_movemask_epi8(a);
    }
}

void math_vpmovzxbw(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_cvtepu8_epi16(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_cvtepu8_epi16(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmullw(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_mullo_epi16(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_mullo_epi16(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmulld(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_mullo_epi32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_mullo_epi32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmuldq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_mul_epi32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_mul_epi32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmuludq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_mul_epu32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_mul_epu32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpinsrb(vec128 *dst, const vec128 *src1, uint32_t val, uint8_t imm) {
    vec128 out;
    memcpy(&out, src1, 16);
    out.u8[imm & 15] = (uint8_t)val;
    memcpy(dst, &out, 16);
}

void math_vpinsrd(vec128 *dst, const vec128 *src1, uint32_t val, uint8_t imm) {
    vec128 out;
    memcpy(&out, src1, 16);
    out.u32[imm & 3] = val;
    memcpy(dst, &out, 16);
}

void math_vpinsrq(vec128 *dst, const vec128 *src1, uint64_t val, uint8_t imm) {
    vec128 out;
    memcpy(&out, src1, 16);
    out.u64[imm & 1] = val;
    memcpy(dst, &out, 16);
}

void math_vpaddb(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_add_epi8(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_add_epi8(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpaddw(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_add_epi16(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_add_epi16(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpaddd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_add_epi32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_add_epi32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpaddq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_add_epi64(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_add_epi64(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpcmpeqb(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_cmpeq_epi8(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_cmpeq_epi8(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpcmpeqw(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_cmpeq_epi16(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_cmpeq_epi16(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpcmpeqd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_cmpeq_epi32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_cmpeq_epi32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpcmpgtb(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_cmpgt_epi8(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_cmpgt_epi8(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpcmpgtw(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_cmpgt_epi16(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_cmpgt_epi16(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpcmpgtd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_cmpgt_epi32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_cmpgt_epi32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpcmpgtq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_cmpgt_epi64(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_cmpgt_epi64(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpsllvd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_sllv_epi32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_sllv_epi32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpsllvq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_sllv_epi64(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_sllv_epi64(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpsrlvd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_srlv_epi32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_srlv_epi32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpsrlvq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_srlv_epi64(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_srlv_epi64(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpminsd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_min_epi32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_min_epi32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpminud(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_min_epu32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_min_epu32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmaxsd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_max_epi32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_max_epi32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmaxud(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L) {
    if (vex_L) {
        simde__m256i a = simde_mm256_loadu_si256((const simde__m256i *)src1);
        simde__m256i b = simde_mm256_loadu_si256((const simde__m256i *)src2);
        simde__m256i res = simde_mm256_max_epu32(a, b);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src1);
        simde__m128i b = simde_mm_loadu_si128((const simde__m128i *)src2);
        simde__m128i res = simde_mm_max_epu32(a, b);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vmovq_store(vec128 *dst, const vec128 *src) {
    memset(dst, 0, 16);
    memcpy(dst, src, 8);
}

void math_vpmovsxwq(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_cvtepi16_epi64(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_cvtepi16_epi64(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmovsxwd(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_cvtepi16_epi32(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_cvtepi16_epi32(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmovzxwd(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_cvtepu16_epi32(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_cvtepu16_epi32(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmovsxdq(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_cvtepi32_epi64(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_cvtepi32_epi64(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmovzxdq(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_cvtepu32_epi64(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_cvtepu32_epi64(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmovsxbd(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_cvtepi8_epi32(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_cvtepi8_epi32(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmovzxbd(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_cvtepu8_epi32(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_cvtepu8_epi32(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmovsxbq(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_cvtepi8_epi64(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_cvtepi8_epi64(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmovzxbq(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_cvtepu8_epi64(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_cvtepu8_epi64(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

void math_vpmovzxwq(vec256 *dst, const vec256 *src, int vex_L) {
    simde__m128i a = simde_mm_loadu_si128((const simde__m128i *)src);
    if (vex_L) {
        simde__m256i res = simde_mm256_cvtepu16_epi64(a);
        simde_mm256_storeu_si256((simde__m256i *)dst, res);
    } else {
        simde__m128i res = simde_mm_cvtepu16_epi64(a);
        simde_mm_storeu_si128((simde__m128i *)&dst->lo, res);
        memset(&dst->hi, 0, sizeof(vec128));
    }
}

uint64_t math_vtest(const vec256 *src1, const vec256 *src2, int vex_L) {
    int bytes = vex_L ? 32 : 16;
    int temp1_zero = 1;
    int temp2_zero = 1;
    
    const uint8_t *s1 = (const uint8_t *)src1;
    const uint8_t *s2 = (const uint8_t *)src2;
    
    for (int i = 0; i < bytes; i++) {
        if ((s1[i] & s2[i]) != 0) {
            temp1_zero = 0;
        }
        if (((~s1[i]) & s2[i]) != 0) {
            temp2_zero = 0;
        }
    }
    
    uint64_t rflags = 0;
    if (temp1_zero) rflags |= (1ULL << 6);
    if (temp2_zero) rflags |= (1ULL << 0);
    return rflags;
}
