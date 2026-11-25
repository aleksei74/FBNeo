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
// EPIC12 RGB555 channels occupy the high five bits of separate bytes.
// Use four pixels at once. The reciprocal is exact for 0..31*31:
// floor(x*1058/32768) == floor(x/31). Tint may produce larger products,
// but any product above 961 clamps to 31 in the original lookup table.
#if EPIC12_NEON
inline uint8x16_t Mul5(uint8x16_t a,uint8x16_t b) {
	const uint16x8_t lo=vmull_u8(vget_low_u8(a),vget_low_u8(b));
	const uint16x8_t hi=vmull_u8(vget_high_u8(a),vget_high_u8(b));
	return vcombine_u8(vmovn_u16(vreinterpretq_u16_s16(vqdmulhq_n_s16(vreinterpretq_s16_u16(lo),1058))),
		vmovn_u16(vreinterpretq_u16_s16(vqdmulhq_n_s16(vreinterpretq_s16_u16(hi),1058))));
}
inline uint8x16_t Inv5(uint8x16_t a) {return veorq_u8(a,vdupq_n_u8(31));}
inline uint8x16_t Clamp5(uint8x16_t a) {return vminq_u8(a,vdupq_n_u8(31));}
inline uint8x16_t RedBase(uint8x16_t a) {
	// Preserve the existing clr_add_with_clr_square behavior: the base
	// red component is used for R, G and B, not just R.
	const uint8x16_t index={2,2,2,3,6,6,6,7,10,10,10,11,14,14,14,15};
	return vqtbl1q_u8(a,index);
}
template<int SM,int DM,bool Tint,bool Blend,bool Flip,bool Transparent>
inline size_t BlendRow(uint32_t *dst,const uint32_t *src,size_t n,
	unsigned sa,unsigned da,unsigned tr,unsigned tg,unsigned tb)
{
	if(n<8)return 0;
	// Preserve ordered reads/writes for overlapping source/dest ranges.
	const uintptr_t d=(uintptr_t)dst;
	const uintptr_t s=(uintptr_t)(Flip?src-(n-1):src);
	const uintptr_t bytes=n*sizeof(uint32_t);
	if(d>s?d-s<bytes:s-d<bytes)return 0;
	const size_t count=n&~size_t(3);
	const uint8x16_t salpha=vdupq_n_u8(sa),dalpha=vdupq_n_u8(da);
	const uint8x16_t tint=vreinterpretq_u8_u32(vdupq_n_u32(tb|(tg<<8)|(tr<<16)));
	const uint32x4_t flag=vdupq_n_u32(0x20000000u),rgb=vdupq_n_u32(0x00ffffffu);
	for(size_t i=0;i<count;i+=4){
		uint32x4_t pen;
		if(Flip){
			pen=vld1q_u32(src-i-3);
			pen=vrev64q_u32(pen);
			pen=vextq_u32(pen,pen,2);
		}else pen=vld1q_u32(src+i);
		uint32x4_t old=vdupq_n_u32(0);
		if(Blend||Transparent)old=vld1q_u32(dst+i);
		uint8x16_t s5=vshrq_n_u8(vreinterpretq_u8_u32(pen),3);
		if(Tint)s5=Clamp5(Mul5(s5,tint));
		uint8x16_t out=s5;
		if(Blend){
			const uint8x16_t d5=vshrq_n_u8(vreinterpretq_u8_u32(old),3);
			uint8x16_t base=s5,tail=d5;
			switch(SM){
				case 0:base=Mul5(s5,salpha);break;
				case 1:base=Mul5(s5,s5);break;
				case 2:base=Mul5(s5,d5);break;
				case 4:base=Mul5(s5,Inv5(salpha));break;
				case 5:base=Mul5(s5,Inv5(s5));break;
				case 6:base=Mul5(s5,Inv5(d5));break;
				default:break;
			}
			switch(DM){
				case 0:tail=Mul5(d5,dalpha);break;
				case 1:tail=Mul5(d5,s5);break;
				case 2:base=RedBase(base);tail=Mul5(d5,d5);break;
				case 4:tail=Mul5(d5,Inv5(dalpha));break;
				case 5:tail=Mul5(d5,Inv5(s5));break;
				case 6:tail=Mul5(d5,Inv5(d5));break;
				default:break;
			}
			out=Clamp5(vaddq_u8(base,tail));
		}
		uint32x4_t result=vorrq_u32(vandq_u32(vreinterpretq_u32_u8(vshlq_n_u8(out,3)),rgb),vandq_u32(pen,flag));
		if(Transparent)result=vbslq_u32(vtstq_u32(pen,flag),result,old);
		vst1q_u32(dst+i,result);
	}
	return count;
}
#endif

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
