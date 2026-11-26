// SPDX-License-Identifier: BSD-3-Clause
// Exact integer colour arithmetic for the Y2 software rasterizer.
#ifndef FBNEO_Y2_PIXEL_OPS_H
#define FBNEO_Y2_PIXEL_OPS_H
#include <stdint.h>
#if defined(__aarch64__) && !defined(__AARCH64EB__) && !defined(FBNEO_Y2_NO_NEON)
#include <arm_neon.h>
#define Y2_PIXEL_NEON 1
#else
#define Y2_PIXEL_NEON 0
#endif

namespace Y2PixelOps {
inline uint32_t Factor(unsigned which,uint32_t src,uint32_t dst,unsigned shift) {
	switch(which){
		case 0:return 0;case 1:return 255;case 2:return (dst>>shift)&255;
		case 3:return 255-((dst>>shift)&255);case 4:return src>>24;
		case 5:return 255-(src>>24);case 6:return dst>>24;default:return 255-(dst>>24);
	}
}
inline uint32_t BlendFactorsScalar(uint32_t src,uint32_t dst,unsigned sf,unsigned df,bool sub){
	uint32_t c=0xff000000;
	for(unsigned shift=0;shift<24;shift+=8){
		int sv=int(((src>>shift)&255)*Factor(sf,src,dst,shift));
		int dv=int(((dst>>shift)&255)*Factor(df,src,dst,shift));
		int v=((sub?dv-sv:dv+sv)+127)/255;
		c|=uint32_t(v<0?0:v>255?255:v)<<shift;
	}
	return c;
}
inline uint32_t ModulateScalar(uint32_t a, uint32_t b)
{
	uint32_t c = 0;
	for (unsigned shift = 0; shift < 32; shift += 8)
		c |= (((a >> shift) & 255) * ((b >> shift) & 255) / 255) << shift;
	return c;
}

inline uint32_t BlendScalar(uint32_t src, uint32_t dst, unsigned sf, unsigned df, bool subtract)
{
	uint32_t c = 0xff000000;
	for (unsigned shift = 0; shift < 24; shift += 8) {
		const int sv = int(((src >> shift) & 255) * sf);
		const int dv = int(((dst >> shift) & 255) * df);
		int v = ((subtract ? dv - sv : dv + sv) + 127) / 255;
		if (v < 0) v = 0;
		if (v > 255) v = 255;
		c |= uint32_t(v) << shift;
	}
	return c;
}

#if Y2_PIXEL_NEON
inline uint32_t Divide255(uint16x8_t v)
{
	// Exact floor(v/255) for 0..65025. Clamp before using this identity.
	v = vaddq_u16(vaddq_u16(v, vdupq_n_u16(1)), vshrq_n_u16(v, 8));
	return vget_lane_u32(vreinterpret_u32_u8(vshrn_n_u16(v, 8)), 0);
}
inline uint32_t Modulate(uint32_t a, uint32_t b)
{
	return Divide255(vmull_u8(vcreate_u8(a), vcreate_u8(b)));
}
inline uint8x8_t FactorVec(unsigned f,uint32_t src,uint32_t dst) {
	if(f==2)return vcreate_u8(dst);
	if(f==3)return vmvn_u8(vcreate_u8(dst));
	return vdup_n_u8(uint8_t(Factor(f,src,dst,0)));
}
inline uint32_t BlendFactors(uint32_t src,uint32_t dst,unsigned sf,unsigned df,bool subtract) {
	const uint16x8_t s=vmull_u8(vcreate_u8(src),FactorVec(sf,src,dst));
	const uint16x8_t d=vmull_u8(vcreate_u8(dst),FactorVec(df,src,dst));
	const uint16x8_t v=subtract?vqsubq_u16(d,s):vqaddq_u16(d,s);
	return 0xff000000u|(Divide255(vminq_u16(vqaddq_u16(v,vdupq_n_u16(127)),vdupq_n_u16(65025)))&0xffffffu);
}
inline uint32_t BlendAlpha(uint32_t src, uint32_t dst, unsigned sf, unsigned df, bool subtract)
{
	const uint16x8_t s = vmull_u8(vcreate_u8(src), vdup_n_u8(uint8_t(sf)));
	const uint16x8_t d = vmull_u8(vcreate_u8(dst), vdup_n_u8(uint8_t(df)));
	// Saturation before division is equivalent to the scalar final clamp.
	const uint16x8_t v = subtract ? vqsubq_u16(d, s) : vqaddq_u16(d, s);
	const uint16x8_t rounded = vminq_u16(vqaddq_u16(v, vdupq_n_u16(127)), vdupq_n_u16(65025));
	return 0xff000000u | (Divide255(rounded) & 0xffffffu);
}
#else
inline uint32_t BlendFactors(uint32_t src,uint32_t dst,unsigned sf,unsigned df,bool subtract) {
	return BlendFactorsScalar(src,dst,sf,df,subtract);
}
inline uint32_t Modulate(uint32_t a, uint32_t b) { return ModulateScalar(a, b); }
inline uint32_t BlendAlpha(uint32_t src, uint32_t dst, unsigned sf, unsigned df, bool subtract)
{
	return BlendScalar(src, dst, sf, df, subtract);
}
#endif
}
#endif
