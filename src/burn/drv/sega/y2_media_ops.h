// SPDX-License-Identifier: BSD-3-Clause
#ifndef FBNEO_Y2_MEDIA_OPS_H
#define FBNEO_Y2_MEDIA_OPS_H
#include <stdint.h>
#include <string.h>
#include "y2_pixel_ops.h"

namespace Y2Media {
inline bool Overlap(uint32_t a, uint32_t b, uint64_t bytes) {
	return uint64_t(a) < uint64_t(b)+bytes && uint64_t(b) < uint64_t(a)+bytes;
}

// Same frame layout, distinct 16x16 macroblock, no reference aliases with
// the entire output frame. All other combinations retain serial job order.
// Width/height are multiples of 16, so adjacent macroblocks write disjoint
// luma and chroma byte ranges, including the interleaved chroma rows.
class MvcGroup {
	uint32_t width_=0,height_=0,out_=0;
	uint64_t seen_[64]={};
public:
	template<class Job>
	bool Append(const Job &j, uint32_t ramSize) {
		const unsigned type=j.cmd[0]>>28;
		const unsigned mx=(j.cmd[0]>>16)&63,my=(j.cmd[0]>>8)&63;
		if (type>3 || !j.width || !j.height || j.width>1024 || j.height>1024 ||
			(j.width&15) || (j.height&15) || mx*16>=j.width || my*16>=j.height) return false;
		const uint64_t bytes=uint64_t(j.width)*j.height*3/2;
		if (uint64_t(j.out)+bytes>ramSize) return false;
		if (width_ && (width_!=j.width || height_!=j.height || out_!=j.out)) return false;
		for(unsigned i=0;i<2;i++) {
			if (!(type==3 || type==i+1)) continue;
			const uint32_t ref=j.ref[i]&0x0fffffe0;
			if (uint64_t(ref)+bytes>ramSize || Overlap(ref,j.out,bytes)) return false;
		}
		const uint64_t bit=uint64_t(1)<<mx;
		if(seen_[my]&bit)return false;
		seen_[my]|=bit;
		width_=j.width;height_=j.height;out_=j.out;
		return true;
	}
};

// BT.601 integer formula used by the original CFT_Yge emulation.
inline uint32_t YuvPixel(unsigned y,unsigned u,unsigned v) {
	const int yy=int(y)-16, uu=int(u)-128,vv=int(v)-128;
	const int r=(298*yy+409*vv+128)>>8;
	const int g=(298*yy-100*uu-208*vv+128)>>8;
	const int b=(298*yy+516*uu+128)>>8;
	const auto clip=[](int x)->uint32_t{return x<0?0:x>255?255:x;};
	return 0xff000000u|(clip(r)<<16)|(clip(g)<<8)|clip(b);
}
#if Y2_PIXEL_NEON
inline uint8x8_t YuvChannel(int16x8_t y,int16x8_t u,int16x8_t v,int uc,int vc) {
	int32x4_t lo=vmull_n_s16(vget_low_s16(y),298);
	int32x4_t hi=vmull_n_s16(vget_high_s16(y),298);
	lo=vmlal_n_s16(lo,vget_low_s16(u),uc);hi=vmlal_n_s16(hi,vget_high_s16(u),uc);
	lo=vmlal_n_s16(lo,vget_low_s16(v),vc);hi=vmlal_n_s16(hi,vget_high_s16(v),vc);
	lo=vshrq_n_s32(vaddq_s32(lo,vdupq_n_s32(128)),8);
	hi=vshrq_n_s32(vaddq_s32(hi,vdupq_n_s32(128)),8);
	return vqmovn_u16(vcombine_u16(vqmovun_s32(lo),vqmovun_s32(hi)));
}
inline uint8x8_t Duplicate4(const uint8_t *p) {
	// Exactly four bytes, no speculative read past the chroma row.
	uint32_t x;memcpy(&x,p,4);
	const uint8x8_t q=vreinterpret_u8_u64(vcreate_u64(x));
	return vzip_u8(q,q).val[0];
}
#endif
inline void Yuv8(const uint8_t *y,const uint8_t *u,const uint8_t *v,uint32_t *argb) {
#if Y2_PIXEL_NEON
	const int16x8_t yy=vreinterpretq_s16_u16(vsubl_u8(vld1_u8(y),vdup_n_u8(16)));
	const int16x8_t uu=vreinterpretq_s16_u16(vsubl_u8(Duplicate4(u),vdup_n_u8(128)));
	const int16x8_t vv=vreinterpretq_s16_u16(vsubl_u8(Duplicate4(v),vdup_n_u8(128)));
	uint8x8x4_t bgra;
	bgra.val[0]=YuvChannel(yy,uu,vv,516,0);
	bgra.val[1]=YuvChannel(yy,uu,vv,-100,-208);
	bgra.val[2]=YuvChannel(yy,uu,vv,0,409);
	bgra.val[3]=vdup_n_u8(255);
	vst4_u8(reinterpret_cast<uint8_t*>(argb),bgra);
#else
	for(unsigned i=0;i<8;i++)argb[i]=YuvPixel(y[i],u[i/2],v[i/2]);
#endif
}
inline uint16_t PackYuv(uint32_t c,unsigned mode) {
	switch(mode) {
		case 0:return 0x8000|((c>>9)&0x7c00)|((c>>6)&0x3e0)|((c>>3)&31);
		case 1:return ((c>>8)&0xf800)|((c>>5)&0x7e0)|((c>>3)&31);
		default:return 0xf000|((c>>12)&0xf00)|((c>>8)&0xf0)|((c>>4)&15);
	}
}
inline void PackYuv8(const uint32_t *argb,uint16_t *out,unsigned mode) {
#if Y2_PIXEL_NEON
	const uint8x8x4_t c=vld4_u8(reinterpret_cast<const uint8_t*>(argb));
	uint16x8_t b=vmovl_u8(c.val[0]),g=vmovl_u8(c.val[1]),r=vmovl_u8(c.val[2]),p;
	if(mode==0)p=vorrq_u16(vdupq_n_u16(0x8000),vorrq_u16(vshlq_n_u16(vshrq_n_u16(r,3),10),vorrq_u16(vshlq_n_u16(vshrq_n_u16(g,3),5),vshrq_n_u16(b,3))));
	else if(mode==1)p=vorrq_u16(vshlq_n_u16(vshrq_n_u16(r,3),11),vorrq_u16(vshlq_n_u16(vshrq_n_u16(g,2),5),vshrq_n_u16(b,3)));
	else p=vorrq_u16(vdupq_n_u16(0xf000),vorrq_u16(vshlq_n_u16(vshrq_n_u16(r,4),8),vorrq_u16(vshlq_n_u16(vshrq_n_u16(g,4),4),vshrq_n_u16(b,4))));
	vst1q_u16(out,p);
#else
	for(unsigned i=0;i<8;i++)out[i]=PackYuv(argb[i],mode);
#endif
}

inline void PredictScalar(const uint8_t *src,unsigned width,unsigned hx,unsigned hy,int32_t *pred) {
	for(unsigned y=0;y<8;y++,src+=width) {
		const uint8_t *s1=src+hy*width;
		for(unsigned x=0;x<8;x++)pred[y*8+x]=(src[x]+src[x+hx]+s1[x]+s1[x+hx]+2)/4;
	}
}
inline void Predict(const uint8_t *src,unsigned width,unsigned hx,unsigned hy,int32_t *pred) {
#if Y2_PIXEL_NEON
	for(unsigned y=0;y<8;y++,src+=width,pred+=8) {
		uint8x8_t a=vld1_u8(src), p=a;
		if(hx&&hy) {
			const uint8_t *s1=src+width;
			const uint16x8_t sum=vaddq_u16(vaddl_u8(a,vld1_u8(src+1)),vaddl_u8(vld1_u8(s1),vld1_u8(s1+1)));
			p=vrshrn_n_u16(sum,2);
		} else if(hx) p=vrhadd_u8(a,vld1_u8(src+1));
		else if(hy) p=vrhadd_u8(a,vld1_u8(src+width));
		const uint16x8_t v=vmovl_u8(p);
		vst1q_s32(pred,vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(v))));
		vst1q_s32(pred+4,vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(v))));
	}
#else
	PredictScalar(src,width,hx,hy,pred);
#endif
}
inline void Average(int32_t *a,const int32_t *b) {
#if Y2_PIXEL_NEON
	for(unsigned i=0;i<64;i+=4)vst1q_s32(a+i,vrhaddq_s32(vld1q_s32(a+i),vld1q_s32(b+i)));
#else
	for(unsigned i=0;i<64;i++)a[i]=(a[i]+b[i]+1)/2;
#endif
}
inline void StoreScalar(uint8_t *dst,const int32_t *res,const int32_t *pred,bool predicted) {
	for(unsigned x=0;x<8;x++){
		int32_t v=res[x]+(predicted?pred[x]:0);
		dst[x]=v<0?0:v>255?255:uint8_t(v);
	}
}
inline void Store(uint8_t *dst,const int32_t *res,const int32_t *pred,bool predicted) {
#if Y2_PIXEL_NEON
	int32x4_t lo=vld1q_s32(res),hi=vld1q_s32(res+4);
	if(predicted){lo=vaddq_s32(lo,vld1q_s32(pred));hi=vaddq_s32(hi,vld1q_s32(pred+4));}
	vst1_u8(dst,vqmovn_u16(vcombine_u16(vqmovun_s32(lo),vqmovun_s32(hi))));
#else
	StoreScalar(dst,res,pred,predicted);
#endif
}
}
#endif
