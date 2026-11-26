// Actual EPIC12 driver differential test, using the original scalar path.
#include "../src/burn/devices/epic12.cpp"
#if !EPIC12_NEON
#error AArch64 NEON required
#endif
#ifdef fprintf
#undef fprintf
#endif
#include <vector>
static INT32 TestLog(INT32,TCHAR*,...){return 0;}
INT32 (__cdecl *bprintf)(INT32,TCHAR*,...)=TestLog;
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
#if THREADY == THREADY_PTHREAD
static bool ParallelTest(){
	const size_t pixels=0x2000*128;
	std::vector<uint32_t> base(pixels),serial(pixels),parallel(pixels);
	for(size_t i=0;i<pixels;i++) base[i]=Epic12Fast::Expand(uint16_t(Random()));
	UINT16 commands[4096]={};
	m_ram16_copy=commands;
	m_main_rammask=sizeof(commands)-1;
	m_main_ramsize=sizeof(commands);
	m_gfx_addr_shadowcopy=0;
	m_gfx_clip_x_shadowcopy=64;
	m_gfx_clip_y_shadowcopy=16;
	m_clip_margin_shadowcopy=0;
	blitHelper.init(gfx_helper_cb);
	if(!blitHelper.is_threaded())return false;
	bool pass=true;
	for(unsigned c=0;c<24&&pass;c++){
		unsigned n=0;
		auto put=[&](UINT16 value){commands[n++]=value;};
		for(unsigned k=0;k<20;k++){
			if(k%5==0){put(0xc000);put((k/5+c)&1);}
			const UINT16 mode=(k%4==0?0:0x200)|((k+c)&7)|(((k*3+c)&7)<<4)
				|((k&1)?0x100:0)|((k&2)?0x400:0)|((k&4)?0x800:0);
			put(0x1000|mode);put(Random());put(1024+(k%3)*384);put(0);
			put(48+(k%4)*16);put((k%3)*8);
			put(319);put(95);put(k%2?0x70:0x80);put(k%2?0x9080:0x8080);
		}
		put(0);
		serial=base;parallel=base;
		m_bitmaps=serial.data();epic12_device_blit_delay=0;gfx_exec();
		const UINT64 delay=epic12_device_blit_delay;
		m_bitmaps=parallel.data();epic12_device_blit_delay=0;
		if(!gfx_exec_parallel()||delay!=epic12_device_blit_delay||serial!=parallel){
			fprintf(stderr,"EPIC12 parallel mismatch/rejection case=%u\n",c);pass=false;
		}
		int split=0;
		// Source/destination dependency (first sprite now reads the output area).
		const UINT16 saved=commands[4];commands[4]=64;
		if(gfx_split_plan(split))pass=false;
		commands[4]=saved;
		// Source wrapping / uploads must never be reordered.
		commands[4]=8190;if(gfx_split_plan(split))pass=false;commands[4]=saved;
		const UINT16 attr=commands[2];commands[2]=0x2000;
		if(gfx_split_plan(split))pass=false;
		commands[2]=attr;
	}
	blitHelper.exit();
	if(pass)printf("EPIC12 real dual-worker command lists: 24 cases PASS; dependency/wrap/upload fallback PASS\n");
	return pass;
}
#endif
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
		for(unsigned i=0;i<sizeof(original)/sizeof(original[0]);i++)original[i]=Epic12Fast::Expand(uint16_t(c%4==1 ? Random()&0x7fff : c%4==2 ? Random()|0x8000 : Random()));
		memcpy(scalar,original,sizeof(scalar));memcpy(vectorized,original,sizeof(vectorized));
		// Non-overlapping rows, horizontal overlap and exact alias.
		int width=1+Random()%129,srcx=128+Random()%32,dstx=c%4==0?srcx+(int(c%7)-3):512;
		int srcy=1,dsty=c%4==0?1:0;
		int flipy=0,height=1;
		clr_t tint={uint8_t(Random()%64),uint8_t(Random()%64),uint8_t(Random()%64),0};
		uint8_t sa=Random()%32,da=Random()%32;
		epic12_device_blit_delay=0;epic12_blend_neon=false;m_bitmaps=scalar;
		funcs[group][mode](&clip,scalar,srcx,srcy,dstx,dsty,width,height,flipy,sa,da,&tint,&epic12_device_blit_delay);
		UINT64 delay=epic12_device_blit_delay;
		epic12_device_blit_delay=0;epic12_blend_neon=true;m_bitmaps=vectorized;
		funcs[group][mode](&clip,vectorized,srcx,srcy,dstx,dsty,width,height,flipy,sa,da,&tint,&epic12_device_blit_delay);
		if(memcmp(scalar,vectorized,sizeof(scalar)) || delay!=epic12_device_blit_delay){
			fprintf(stderr,"EPIC12 blend mismatch group=%u mode=%u case=%u width=%d\n",group,mode,c,width);return 1;
		}
		cases++;
	}
	printf("EPIC12 real scalar/NEON blitter: %u cases PASS (all 512 blend variants)\n",cases);
#if THREADY == THREADY_PTHREAD
	if(!ParallelTest())return 2;
#endif
}
