// Experimental RGB555+flag VRAM. Keep the legacy 32-bit backend as
// reference; packed arithmetic reproduces its lookup tables and ordering.
#ifndef FBNEO_EPIC12_PACKED_H
#define FBNEO_EPIC12_PACKED_H
#include "epic12_fast.h"
namespace Epic12Packed {
inline uint16_t Pack(uint32_t p) {
	return uint16_t(((p>>14)&0x8000)|((p>>9)&0x7c00)|((p>>6)&0x03e0)|((p>>3)&0x001f));
}
inline void ExpandRow(uint32_t *dst,const uint16_t *src,size_t n) {
	Epic12Fast::Upload(dst,src,n);
}
inline unsigned Mul(unsigned a,unsigned b) {
	const unsigned p=a*b/31;
	return p>31?31:p;
}
inline unsigned Term(unsigned mode,unsigned a,unsigned b,unsigned alpha,bool source) {
	// Mode 1 squares source for the source term, cross-multiplies
	// for the destination term. Mode 2 is the opposite.
	switch(mode) {
		case 0:return Mul(a,alpha);
		case 1:return Mul(a,source?a:b);
		case 2:return Mul(a,source?b:a);
		case 4:return Mul(a,31-alpha);
		case 5:return Mul(a,31-(source?a:b));
		case 6:return Mul(a,31-(source?b:a));
		default:return a;
	}
}
inline uint16_t Pixel(uint16_t p,uint16_t d,unsigned sm,unsigned dm,
	unsigned sa,unsigned da,unsigned tr,unsigned tg,unsigned tb,bool tint,bool blend) {
	unsigned sr=(p>>10)&31,sg=(p>>5)&31,sb=p&31;
	if(tint){sr=Mul(sr,tr);sg=Mul(sg,tg);sb=Mul(sb,tb);}
	if(blend){
		const unsigned dr=(d>>10)&31,dg=(d>>5)&31,db=d&31;
		const unsigned r=Term(sm,sr,dr,sa,true);
		const unsigned g=dm==2?r:Term(sm,sg,dg,sa,true);
		const unsigned b=dm==2?r:Term(sm,sb,db,sa,true);
		sr=r+Term(dm,dr,sr,da,false);
		sg=g+Term(dm,dg,sg,da,false);
		sb=b+Term(dm,db,sb,da,false);
		if(sr>31)sr=31;if(sg>31)sg=31;if(sb>31)sb=31;
	}
	return uint16_t((p&0x8000)|(sr<<10)|(sg<<5)|sb);
}
#if EPIC12_NEON
inline uint16x8_t MulV(uint16x8_t a,uint16x8_t b){
	return vreinterpretq_u16_s16(vqdmulhq_n_s16(vreinterpretq_s16_u16(vmulq_u16(a,b)),1058));
}
inline uint16x8_t Min31(uint16x8_t a){return vminq_u16(a,vdupq_n_u16(31));}
inline uint16x8_t Inv(uint16x8_t a){return veorq_u16(a,vdupq_n_u16(31));}
inline uint16x8_t TermV(unsigned mode,uint16x8_t a,uint16x8_t b,unsigned alpha,bool source){
	switch(mode){
		case 0:return MulV(a,vdupq_n_u16(alpha));
		case 1:return MulV(a,source?a:b);
		case 2:return MulV(a,source?b:a);
		case 4:return MulV(a,vdupq_n_u16(31-alpha));
		case 5:return MulV(a,Inv(source?a:b));
		case 6:return MulV(a,Inv(source?b:a));
		default:return a;
	}
}
inline uint16x8_t Reverse(uint16x8_t a){a=vrev64q_u16(a);return vextq_u16(a,a,4);}
#endif
// Runtime mode/tint/alpha are invariant for the entire row. Keep this
// shared rather than emitting another 512 full copies of the rasterizer.
template<bool Flip,bool Trans,int SM=-1,int DM=-1,int Tint=-1,int Blend=-1>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
inline void RowKernel(uint16_t *dst,const uint16_t *src,size_t n,
	unsigned sm,unsigned dm,unsigned sa,unsigned da,unsigned tr,unsigned tg,unsigned tb,
	bool tint,bool blend,bool neon=true){
	if(SM>=0)sm=SM;
	if(DM>=0)dm=DM;
	if(Tint>=0)tint=Tint;
	if(Blend>=0)blend=Blend;
	if(!n)return;
	const uintptr_t d=(uintptr_t)dst,s=(uintptr_t)(Flip?src-(n-1):src);
	const size_t bytes=n*2;
	const bool overlap=d>s?d-s<bytes:s-d<bytes;
	if(!Flip&&!tint&&!blend&&!Trans&&(!overlap||d<=s)){
		if(d!=s)memmove(dst,src,bytes);
		return;
	}
	size_t i=0;
#if EPIC12_NEON
	if(neon && (!overlap || (!Flip && (d<=s || d-s>=16)))){
		const uint16x8_t flag=vdupq_n_u16(0x8000),mask5=vdupq_n_u16(31);
		for(;i+8<=n;i+=8){
			uint16x8_t p=vld1q_u16(Flip?src-i-7:src+i);
			if(Flip)p=Reverse(p);
			const uint16x8_t mask=vtstq_u16(p,flag);
			if(Trans && vmaxvq_u16(mask)==0)continue;
			const bool mixed=Trans && vminvq_u16(mask)==0;
			uint16x8_t old=vdupq_n_u16(0);
			if(blend||mixed)old=vld1q_u16(dst+i);
			uint16x8_t result=p;
			if(tint||blend){
				uint16x8_t sr=vandq_u16(vshrq_n_u16(p,10),mask5),sg=vandq_u16(vshrq_n_u16(p,5),mask5),sb=vandq_u16(p,mask5);
				if(tint){
					sr=Min31(MulV(sr,vdupq_n_u16(tr)));
					sg=Min31(MulV(sg,vdupq_n_u16(tg)));
					sb=Min31(MulV(sb,vdupq_n_u16(tb)));
				}
				if(blend){
					const uint16x8_t dr=vandq_u16(vshrq_n_u16(old,10),mask5),dg=vandq_u16(vshrq_n_u16(old,5),mask5),db=vandq_u16(old,mask5);
					const uint16x8_t r=TermV(sm,sr,dr,sa,true);
					const uint16x8_t g=dm==2?r:TermV(sm,sg,dg,sa,true);
					const uint16x8_t b=dm==2?r:TermV(sm,sb,db,sa,true);
					sr=Min31(vaddq_u16(r,TermV(dm,dr,sr,da,false)));
					sg=Min31(vaddq_u16(g,TermV(dm,dg,sg,da,false)));
					sb=Min31(vaddq_u16(b,TermV(dm,db,sb,da,false)));
				}
				result=vorrq_u16(vandq_u16(p,flag),vorrq_u16(vshlq_n_u16(sr,10),vorrq_u16(vshlq_n_u16(sg,5),sb)));
			}
			if(mixed)result=vbslq_u16(mask,result,old);
			vst1q_u16(dst+i,result);
		}
	}
#endif
	for(;i<n;i++){
		const uint16_t p=Flip?src[-(ptrdiff_t)i]:src[i];
		if(!Trans||(p&0x8000))dst[i]=(tint||blend)?Pixel(p,dst[i],sm,dm,sa,da,tr,tg,tb,tint,blend):p;
	}
}
// Common paths bypass mode/tint branches for each eight-pixel group.
// Keep the generic kernel for uncommon blend modes and A/B tests.
static bool specialized=true;
template<bool Flip,bool Trans>
inline void Row(uint16_t *dst,const uint16_t *src,size_t n,
	unsigned sm,unsigned dm,unsigned sa,unsigned da,unsigned tr,unsigned tg,unsigned tb,
	bool tint,bool blend,bool neon=true){
#define PACKED_KERNEL(S,D,T,B) return RowKernel<Flip,Trans,S,D,T,B>(dst,src,n,sm,dm,sa,da,tr,tg,tb,tint,blend,neon)
	if(specialized){
		if(!blend){
			if(tint){PACKED_KERNEL(0,0,1,0);}
			PACKED_KERNEL(0,0,0,0);
		}
		if(!tint){
			if(sm==0 && dm==0){PACKED_KERNEL(0,0,0,1);}
			if(sm==0 && dm==3){PACKED_KERNEL(0,3,0,1);}
			if(sm==3 && dm==0){PACKED_KERNEL(3,0,0,1);}
			if(sm==3 && dm==3){PACKED_KERNEL(3,3,0,1);}
		} else if(sm==0 && dm==0){PACKED_KERNEL(0,0,1,1);}
	}
	PACKED_KERNEL(-1,-1,-1,-1);
#undef PACKED_KERNEL
}
}
#endif
