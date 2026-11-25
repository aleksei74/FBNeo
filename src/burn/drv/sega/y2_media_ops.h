// SPDX-License-Identifier: BSD-3-Clause
#ifndef FBNEO_Y2_MEDIA_OPS_H
#define FBNEO_Y2_MEDIA_OPS_H
#include <stdint.h>
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
