#ifndef MATH_BACKEND_H
#define MATH_BACKEND_H

#include <stdint.h>
#include <string.h>

typedef union {
    uint8_t u8[16];
    int8_t i8[16];
    uint16_t u16[8];
    int16_t i16[8];
    uint32_t u32[4];
    int32_t i32[4];
    uint64_t u64[2];
    int64_t i64[2];
    float f32[4];
    double f64[2];
} vec128;

typedef struct {
    vec128 lo;
    vec128 hi;
} vec256;

// Pure mathematical category routines (no ucontext_t, no signals)
void math_vpxor(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vpand(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vpor(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vpandn(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vpbroadcastb(vec256 *dst, const vec256 *src, int vex_L);
void math_vpbroadcastw(vec256 *dst, const vec256 *src, int vex_L);
void math_vpbroadcastd(vec256 *dst, const vec256 *src, int vex_L);
void math_vpbroadcastq(vec256 *dst, const vec256 *src, int vex_L);
void math_vpsllq(vec256 *dst, const vec256 *src, uint8_t imm);
void math_vpsrlq(vec256 *dst, const vec256 *src, uint8_t imm);
void math_vpsllw(vec256 *dst, const vec256 *src, uint8_t imm);
void math_vpsrlw(vec256 *dst, const vec256 *src, uint8_t imm);
void math_vpsraw(vec256 *dst, const vec256 *src, uint8_t imm);
void math_vpslld(vec256 *dst, const vec256 *src, uint8_t imm);
void math_vpsrld(vec256 *dst, const vec256 *src, uint8_t imm);
void math_vpsrad(vec256 *dst, const vec256 *src, uint8_t imm);
uint32_t math_vpmovmskb(const vec256 *src, int vex_L);
void math_vpshufd(vec256 *dst, const vec256 *src, uint8_t imm, int vex_L);
void math_vpshufb(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpcmpeqq(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vpcmpeqb(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpcmpeqw(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpcmpeqd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpcmpgtb(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpcmpgtw(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpcmpgtd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpcmpgtq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpsllvd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpsllvq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpsrlvd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpsrlvq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);

void math_vpacksswb(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vpackuswb(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vpmovzxbw(vec256 *dst, const vec256 *src, int vex_L);
void math_vpmovsxwq(vec256 *dst, const vec256 *src, int vex_L);
void math_vpmovsxwd(vec256 *dst, const vec256 *src, int vex_L);
void math_vpmovzxwd(vec256 *dst, const vec256 *src, int vex_L);
void math_vpmovsxdq(vec256 *dst, const vec256 *src, int vex_L);
void math_vpmovzxdq(vec256 *dst, const vec256 *src, int vex_L);
void math_vpmovsxbd(vec256 *dst, const vec256 *src, int vex_L);
void math_vpmovzxbd(vec256 *dst, const vec256 *src, int vex_L);
void math_vpmovsxbq(vec256 *dst, const vec256 *src, int vex_L);
void math_vpmovzxbq(vec256 *dst, const vec256 *src, int vex_L);
void math_vpmovzxwq(vec256 *dst, const vec256 *src, int vex_L);
uint64_t math_vtest(const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpmullw(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpmulld(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpmuldq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpmuludq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpaddb(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpaddw(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpaddd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpaddq(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpminsd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpminud(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpmaxsd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpmaxud(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vpinsrb(vec128 *dst, const vec128 *src1, uint32_t val, uint8_t imm);
void math_vpinsrd(vec128 *dst, const vec128 *src1, uint32_t val, uint8_t imm);
void math_vpinsrq(vec128 *dst, const vec128 *src1, uint64_t val, uint8_t imm);
void math_vpackssdw(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vpackusdw(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vpunpcklqdq(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vpunpckhqdq(vec256 *dst, const vec256 *src1, const vec256 *src2);

void math_vpermq(vec256 *dst, const vec256 *src, uint8_t imm);
void math_vpermilps(vec256 *dst, const vec256 *src, uint8_t imm, int vex_L);
void math_vextract128(vec128 *dst, const vec256 *src, uint8_t imm);
void math_vinsert128(vec256 *dst, const vec256 *src1, const vec128 *src2, uint8_t imm);

void math_vaddps(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vmulps(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vsubps(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vdivps(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vaddpd(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vmulpd(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vsubpd(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vdivpd(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vminps(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vmaxps(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vminpd(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vmaxpd(vec256 *dst, const vec256 *src1, const vec256 *src2);
void math_vmovq_store(vec128 *dst, const vec128 *src);
void math_vshufps(vec256 *dst, const vec256 *src1, const vec256 *src2, uint8_t imm, int vex_L);
void math_vshufpd(vec256 *dst, const vec256 *src1, const vec256 *src2, uint8_t imm, int vex_L);

void math_vaddss(vec128 *dst, const vec128 *src1, const vec128 *src2);
void math_vsubss(vec128 *dst, const vec128 *src1, const vec128 *src2);
void math_vmulss(vec128 *dst, const vec128 *src1, const vec128 *src2);
void math_vdivss(vec128 *dst, const vec128 *src1, const vec128 *src2);
void math_vaddsd(vec128 *dst, const vec128 *src1, const vec128 *src2);
void math_vsubsd(vec128 *dst, const vec128 *src1, const vec128 *src2);
void math_vmulsd(vec128 *dst, const vec128 *src1, const vec128 *src2);
void math_vdivsd(vec128 *dst, const vec128 *src1, const vec128 *src2);
void math_vminss(vec128 *dst, const vec128 *src1, const vec128 *src2);
void math_vmaxss(vec128 *dst, const vec128 *src1, const vec128 *src2);
void math_vminsd(vec128 *dst, const vec128 *src1, const vec128 *src2);
void math_vmaxsd(vec128 *dst, const vec128 *src1, const vec128 *src2);

void math_vbroadcastss(vec256 *dst, const vec128 *src, int vex_L);
void math_vbroadcastsd(vec256 *dst, const vec128 *src, int vex_L);

void math_vmovddup(vec256 *dst, const vec256 *src, int vex_L);
void math_vroundss(vec128 *dst, const vec128 *src1, const vec128 *src2, uint8_t imm);
void math_vroundsd(vec128 *dst, const vec128 *src1, const vec128 *src2, uint8_t imm);
void math_vroundps(vec256 *dst, const vec256 *src, uint8_t imm, int vex_L);
void math_vroundpd(vec256 *dst, const vec256 *src, uint8_t imm, int vex_L);

void math_vaddsubps(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vaddsubpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vmovshdup(vec256 *dst, const vec256 *src, int vex_L);
void math_vmovsldup(vec256 *dst, const vec256 *src, int vex_L);
void math_vunpckhps(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vunpcklps(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vunpckhpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vunpcklpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);

void math_vandpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vandnpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vorpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);
void math_vxorpd(vec256 *dst, const vec256 *src1, const vec256 *src2, int vex_L);

void math_vcmpps(vec256 *dst, const vec256 *src1, const vec256 *src2, uint8_t imm, int vex_L);
void math_vcmppd(vec256 *dst, const vec256 *src1, const vec256 *src2, uint8_t imm, int vex_L);
void math_vcmpss(vec128 *dst, const vec128 *src1, const vec128 *src2, uint8_t imm);
void math_vcmpsd(vec128 *dst, const vec128 *src1, const vec128 *src2, uint8_t imm);

uint64_t math_ucomisd(const vec128 *a, const vec128 *b);
uint64_t math_ucomiss(const vec128 *a, const vec128 *b);

void math_vcvtsi2sd(vec128 *dst, const vec128 *src1, uint64_t val, int vex_W);
void math_vcvtsi2ss(vec128 *dst, const vec128 *src1, uint64_t val, int vex_W);
uint64_t math_vcvttsd2si(const vec128 *src, int vex_W);
uint64_t math_vcvttss2si(const vec128 *src, int vex_W);
void math_vcvttps2dq(vec256 *dst, const vec256 *src, int vex_L);
void math_vcvtps2dq(vec256 *dst, const vec256 *src, int vex_L);
void math_vcvtdq2ps(vec256 *dst, const vec256 *src, int vex_L);

#endif // MATH_BACKEND_H
