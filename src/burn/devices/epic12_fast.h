#ifndef FBNEO_EPIC12_FAST_H
#define FBNEO_EPIC12_FAST_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#if defined(__aarch64__) && !defined(__AARCH64EB__) && !defined(FBNEO_EPIC12_NO_NEON)
#include <arm_neon.h>
#define EPIC12_NEON 1
#else
#define EPIC12_NEON 0
#endif
namespace Epic12Fast {
inline uint32_t Expand(uint16_t p) {
	return ((uint32_t(p)&0x8000)<<14)|((uint32_t(p)&0x7c00)<<9)|
		((uint32_t(p)&0x03e0)<<6)|((uint32_t(p)&0x001f)<<3);
}
#if EPIC12_NEON
inline uint32x4_t Expand4(uint16x4_t p) {
	const uint32x4_t v = vmovl_u16(p);
	return vorrq_u32(vorrq_u32(vshlq_n_u32(vandq_u32(v,vdupq_n_u32(0x8000)),14),
		vshlq_n_u32(vandq_u32(v,vdupq_n_u32(0x7c00)),9)),
		vorrq_u32(vshlq_n_u32(vandq_u32(v,vdupq_n_u32(0x03e0)),6),
			vshlq_n_u32(vandq_u32(v,vdupq_n_u32(0x001f)),3)));
}
#endif
inline void Upload(uint32_t *dst,const uint16_t *src,size_t n) {
#if EPIC12_NEON
	for (;n>=8;n-=8,src+=8,dst+=8) {
		uint16x8_t p=vld1q_u16(src);
		vst1q_u32(dst,Expand4(vget_low_u16(p)));
		vst1q_u32(dst+4,Expand4(vget_high_u16(p)));
	}
#endif
	for (;n;--n) *dst++=Expand(*src++);
}
inline void Copy(uint32_t *dst,const uint32_t *src,size_t n,bool transparent) {
	// Compare addresses, not relational pointers into different objects.
	const uintptr_t d=(uintptr_t)dst,s=(uintptr_t)src,bytes=n*sizeof(uint32_t);
	if (d==s) return;
	const bool overlap=d>s ? d-s<bytes : s-d<bytes;
	if (!overlap) {
		if (!transparent) { memcpy(dst,src,bytes); return; }
#if EPIC12_NEON
		for (;n>=4;n-=4,src+=4,dst+=4) {
			uint32x4_t p=vld1q_u32(src),q=vld1q_u32(dst);
			uint32x4_t mask=vtstq_u32(p,vdupq_n_u32(0x20000000));
			vst1q_u32(dst,vbslq_u32(mask,p,q));
		}
#endif
	}
	for (;n;--n,++src,++dst) if (!transparent || (*src&0x20000000)) *dst=*src;
}
}
#endif
