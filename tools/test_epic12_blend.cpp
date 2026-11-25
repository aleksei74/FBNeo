// Actual EPIC12 driver differential test, using the original scalar path.
#include "../src/burn/devices/epic12.cpp"
#if !EPIC12_NEON
#error AArch64 NEON required
#endif
#ifdef fprintf
#undef fprintf
#endif
static uint32_t rng=0x919f55u;
static uint32_t Random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static uint32_t original[0x2000*3], scalar[0x2000*3], vectorized[0x2000*3];
static void Tables(){
	for(unsigned y=0;y<64;y++)for(unsigned x=0;x<32;x++){
		unsigned v=x*y/31;
		epic12_device_colrtable[x][y]=epic12_device_colrtable_rev[x^31][y]=v>31?31:v;
	}
	for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++)epic12_device_colrtable_add[x][y]=x+y>31?31:x+y;
}
int main(){
	Tables();
	epic12_device_blitfunction *funcs[]={
		epic12_device_f0_ti0_tr0_blit_funcs,epic12_device_f0_ti0_tr1_blit_funcs,
		epic12_device_f0_ti1_tr0_blit_funcs,epic12_device_f0_ti1_tr1_blit_funcs,
		epic12_device_f1_ti0_tr0_blit_funcs,epic12_device_f1_ti0_tr1_blit_funcs,
		epic12_device_f1_ti1_tr0_blit_funcs,epic12_device_f1_ti1_tr1_blit_funcs};
	unsigned cases=0;
	rectangle clip;clip.set(0,0x1fff,0,2);
	for(unsigned group=0;group<8;group++)for(unsigned mode=0;mode<64;mode++)for(unsigned c=0;c<8;c++){
		for(unsigned i=0;i<sizeof(original)/sizeof(original[0]);i++)original[i]=Epic12Fast::Expand(uint16_t(Random()));
		memcpy(scalar,original,sizeof(scalar));memcpy(vectorized,original,sizeof(vectorized));
		// Non-overlapping rows, horizontal overlap and exact alias.
		int width=1+Random()%129,srcx=128+Random()%32,dstx=c%4==0?srcx+(int(c%7)-3):512;
		int srcy=1,dsty=c%4==0?1:0;
		int flipy=0,height=1;
		clr_t tint={uint8_t(Random()%64),uint8_t(Random()%64),uint8_t(Random()%64),0};
		uint8_t sa=Random()%32,da=Random()%32;
		epic12_device_blit_delay=0;epic12_blend_neon=false;m_bitmaps=scalar;
		funcs[group][mode](&clip,scalar,srcx,srcy,dstx,dsty,width,height,flipy,sa,da,&tint);
		UINT64 delay=epic12_device_blit_delay;
		epic12_device_blit_delay=0;epic12_blend_neon=true;m_bitmaps=vectorized;
		funcs[group][mode](&clip,vectorized,srcx,srcy,dstx,dsty,width,height,flipy,sa,da,&tint);
		if(memcmp(scalar,vectorized,sizeof(scalar)) || delay!=epic12_device_blit_delay){
			fprintf(stderr,"EPIC12 blend mismatch group=%u mode=%u case=%u width=%d\n",group,mode,c,width);return 1;
		}
		cases++;
	}
	printf("EPIC12 real scalar/NEON blitter: %u cases PASS (all 512 blend variants)\n",cases);
}
