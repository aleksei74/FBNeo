#include "../src/burn/devices/epic12_fast.h"
#include <stdio.h>
#include <stdlib.h>
static uint32_t rng=0xe912c711u;
static uint32_t Random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(){
#if EPIC12_NEON
	// Every possible 5-bit alpha/channel pair and both square tables.
	for(unsigned a=0;a<32;a++){
		uint8_t lut[32];
		for(unsigned x=0;x<32;x++)lut[x]=a*x/31;
		for(unsigned x=0;x<32;x+=16){
			uint8_t index[16],out[16],sq[16],inv[16];
			for(unsigned i=0;i<16;i++)index[i]=x+i;
			const uint8x16_t ix=vld1q_u8(index);
			vst1q_u8(out,Epic12Fast::Lookup5(ix,Epic12Fast::Load5Table(lut)));
			vst1q_u8(sq,Epic12Fast::Lookup5(ix,Epic12Fast::Load5Table(Epic12Fast::Square5)));
			vst1q_u8(inv,Epic12Fast::Lookup5(ix,Epic12Fast::Load5Table(Epic12Fast::InvSquare5)));
			for(unsigned i=0;i<16;i++){
				const unsigned v=x+i;
				if(out[i]!=a*v/31 || sq[i]!=v*v/31 || inv[i]!=v*(31-v)/31)return 4;
			}
		}
	}
	puts("EPIC12 exact NEON lookup: all 1024 alpha/channel pairs and both square tables PASS");
#endif
	uint16_t src[65536];
	uint32_t dst[65536];
	for(unsigned i=0;i<65536;i++)src[i]=i;
	Epic12Fast::Upload(dst,src,65536);
	for(unsigned i=0;i<65536;i++){
		uint32_t p=i,ref=((p&0x8000)<<14)|((p&0x7c00)<<9)|((p&0x03e0)<<6)|((p&31)<<3);
		if(dst[i]!=ref)return 1;
	}
	for(unsigned c=0;c<20000;c++){
		uint32_t ref[256],out[256];
		for(unsigned i=0;i<256;i++)ref[i]=Random();
		memcpy(out,ref,sizeof(ref));
		unsigned n=Random()%129,s=Random()%(257-n),d=Random()%(257-n),t=c&1;
		for(unsigned i=0;i<n;i++)if(!t||(ref[s+i]&0x20000000))ref[d+i]=ref[s+i];
		Epic12Fast::Copy(out+d,out+s,n,t);
		if(memcmp(ref,out,sizeof(ref)))return 2;
		for(unsigned i=0;i<256;i++)ref[i]=Random();
		memcpy(out,ref,sizeof(ref));
		if(n){
			for(unsigned i=0;i<n;i++)if(!t||(ref[s+n-1-i]&0x20000000))ref[d+i]=ref[s+n-1-i];
			Epic12Fast::CopyRow<true>(out+d,out+s+n-1,n,t);
			if(memcmp(ref,out,sizeof(ref)))return 3;
		}
	}
	printf("EPIC12 upload all 65536 words and 20000 forward and reversed copy/overlap cases PASS (NEON %d)\n",EPIC12_NEON);
}
