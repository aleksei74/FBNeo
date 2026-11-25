// Standalone test of the real interpreter and experimental native emitter.
#include "../src/cpu/sh4/sh4.cpp"
#if !SH3_NATIVE
#error Linux AArch64 required
#endif
#ifdef fprintf
#undef fprintf
#endif
static INT32 __cdecl NoScan(BurnArea *) { return 0; }
INT32 (__cdecl *BurnAcb)(BurnArea *) = NoScan;
void CpuCheatRegister(INT32, cpu_core_config *) {}
#ifndef bprintf
static INT32 __cdecl NoLog(INT32, TCHAR *, ...) { return 0; }
INT32 (__cdecl *bprintf)(INT32, TCHAR *, ...) = NoLog;
#endif

static void Require(bool b,const char *s) { if(!b){fprintf(stderr,"FAIL: %s\n",s);exit(1);} }
static uint32_t rng=0xc711e1u;
static uint32_t Random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static uint16_t ram[32768];
static uint8_t *mapR[65536],*mapL[65536];
static void InitRegs(uint32_t *regs) {
	for(unsigned i=0;i<16;i++)regs[i]=Random();
}
static void TestEmitter() {
	using namespace Sh3Native;
	Cache cache;
	for(unsigned i=0;i<32768;i++)ram[i]=uint16_t(Random());
	mapR[0xc00]=mapL[0xc00]=(uint8_t*)ram;
	MemMapR[0xc00]=MemMapRL[0xc00]=(uint8_t*)ram;
	unsigned tested=0;
	for(unsigned op=0;op<65536;op++) {
		if(!Candidate(uint16_t(op)))continue;
		uint32_t words[MaxWords],offset;
		Emitter e(words);
		Require(e.Emit(uint16_t(op),0),"candidate rejected");
		e.Finish(1);
		if(!(tested%512))cache.Reset();
		Require(cache.Publish(words,e.End()-words,offset),"publish");
		uint32_t regs[16],sr=Random(),ea=Random();
		InitRegs(regs);
		const unsigned top=op>>12,f=op&15,m=(op>>4)&15;
		const bool memory=top==5||(top==6&&(f<=2||(f>=4&&f<=6)));
		if(memory)regs[m]=0x8c000000u|(Random()&0xff00);
		memcpy(m_r,regs,sizeof(regs));m_sr=sr;m_ea=ea;
		m_sh4_icount=10;sh3_total_cycles=0;
		execute_one(uint16_t(op));
		Require(m_sh4_icount==10 && sh3_total_cycles==0,"non-unit-cycle instruction accepted");
		unsigned done=cache.Run(offset,regs,&sr,&ea,mapR,mapL);
		if(done!=1 || memcmp(regs,m_r,sizeof(regs)) || sr!=m_sr || ea!=m_ea) {
			fprintf(stderr,"opcode %04x failed done=%u SR=%08x/%08x EA=%08x/%08x\n",op,done,sr,m_sr,ea,m_ea);
			exit(2);
		}
		if(memory) {
			InitRegs(regs);regs[m]=0x8d000000u;
			uint32_t before[16];memcpy(before,regs,sizeof(regs));uint32_t oldsr=sr,oldea=ea;
			Require(cache.Run(offset,regs,&sr,&ea,mapR,mapL)==0 &&
				!memcmp(before,regs,sizeof(regs)) && sr==oldsr && ea==oldea,"unmapped memory guard");
			// Long-only handler must override direct byte/word map.
			if(top==5 || (f&3)==2) {
				regs[m]=0x8c000000;memcpy(before,regs,sizeof(regs));
				mapL[0xc00]=(uint8_t*)1;
				Require(cache.Run(offset,regs,&sr,&ea,mapR,mapL)==0 && !memcmp(before,regs,sizeof(regs)),"long handler bypassed");
				mapL[0xc00]=(uint8_t*)ram;
			}
		}
		tested++;
	}
	printf("SH3 single instruction differential: %u encodings PASS\n",tested);
	// Multi-op blocks exercise eviction, aliases, T-bit state and guard exits.
	for(unsigned c=0;c<4096;c++) {
		if(!(c%128))cache.Reset();
		uint32_t words[MaxWords],offset,regs[16],sr=Random(),ea=Random();
		InitRegs(regs);regs[15]=0x8c000000u;
		memcpy(m_r,regs,sizeof(regs));m_sr=sr;m_ea=ea;
		Emitter e(words);
		unsigned expected=0;
		const unsigned miss=c%25;
		for(unsigned i=0;i<MaxOps;i++){
			uint16_t op;
			if(i==miss){op=0x60f2;mapL[0xc00]=(uint8_t*)1;}
			else {
				do {op=uint16_t(Random());}while(!Candidate(op)||((op>>12)==5)||
					((op>>12)==6 && ((op&15)<=2||((op&15)>=4&&(op&15)<=6)))||
					((op>>8)&15)==15);
			}
			Require(e.Emit(op,i),"mixed emit");
			if(i<miss){execute_one(op);expected++;}
		}
		e.Finish(MaxOps);
		Require(cache.Publish(words,e.End()-words,offset),"mixed publish");
		unsigned done=cache.Run(offset,regs,&sr,&ea,mapR,mapL);
		if(done!=expected || memcmp(regs,m_r,sizeof(regs)) || sr!=m_sr || ea!=m_ea) {
			fprintf(stderr,"mixed %u done=%u expected=%u SR=%08x/%08x\n",c,done,expected,sr,m_sr);exit(3);
		}
		mapL[0xc00]=(uint8_t*)ram;
	}
	puts("SH3 mixed spans and dirty state at guard exits: 4096 PASS");
}

static void TestTimers() {
	for(unsigned c=0;c<100000;c++) {
		sh4_dtimer t={};
		t.running=c&1;t.timer_prescaler=1+Random()%256;t.prescale_counter=Random()%t.timer_prescaler;
		t.time_current=Random()%1000;t.time_trig=t.time_current+1+Random()%256;
		t.retrig=0;t.timer_exec=NULL;
		m_ratio=1.0;
		unsigned cycles=Random()%4096;
		sh4_dtimer ref=t;
		ref.prescale_counter+=cycles;
		while(ref.prescale_counter>=unsigned(ref.timer_prescaler)){
			ref.prescale_counter-=ref.timer_prescaler;
			if(ref.running && ++ref.time_current>=ref.time_trig)ref.running=0;
		}
		t.run_prescale(cycles);
		Require(t.running==ref.running&&t.time_current==ref.time_current&&t.prescale_counter==ref.prescale_counter,"prescaler mismatch");
	}
	puts("SH3 timer prescaler: 100000 comparisons PASS");
}

// Exercise the real execution loop, including live instruction changes,
// code-cache reuse, an IRQ-free block boundary, and long-read handler fallback.
struct Snapshot {
	uint32_t r[16], sr, pc, ppc, ea, delay;
	int cycles, total;
	unsigned hits;
};
static unsigned handlerHits;
static UINT32 __fastcall ReadLongTest(UINT32 a) {
	handlerHits++;
	return 0x12345678u ^ a;
}
static Snapshot RunCore(bool native, bool patch, bool handler)
{
	Sh3MapInit();
	Sh3SetCv1kNative(native);
	static uint16_t code[32768];
	memset(code,0,sizeof(code));
	// MOV #3,R0; ADD #5,R0; MOV R0,R1; ADD R1,R0;
	// MOV.L @R15,R2; XOR R2,R0; CMP/EQ R1,R0; NOP;
	// BRA back to offset 0; NOP delay.
	const uint16_t program[]={0xe003,0x7005,0x6103,0x301c,0x62f2,0x202a,0x3010,0x0009,0xaff6,0x0009};
	memcpy(code,program,sizeof(program));
	Sh3MapMemory((UINT8*)code,0,0xffff,MAP_RAM);
	Sh3MapMemory((UINT8*)ram,0xc000000,0xc00ffff,MAP_RAM);
	if(handler){
		Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);
		Sh3SetReadLongHandler(1,ReadLongTest);
	}
	m_pc=0x80000000;m_ppc=0;m_ea=0;m_delay=0;m_sr=0;
	m_test_irq=0;m_pending_irq=0;m_cpu_off=0;sh3_total_cycles=0;
	memset(m_r,0,sizeof(m_r));m_r[15]=0x8c000000u;
	handlerHits=0;
	m_ratio=1.0;
	for(unsigned i=0;i<3;i++){m_timer[i]={};m_timer[i].timer_prescaler=64;}
	for(unsigned i=0;i<4;i++)m_dma_timer[i]={};
	cave_blitter_delay={};
	int cycles=0;
	for(unsigned i=0;i<100;i++){
		if(i==40 && patch)code[1]=0x7009; // guest RAM changed after caching
		cycles+=Sh3Run_timerhack(37);
	}
	Snapshot out={};
	memcpy(out.r,m_r,sizeof(m_r));out.sr=m_sr;out.pc=m_pc;out.ppc=m_ppc;out.ea=m_ea;out.delay=m_delay;
	out.cycles=cycles;out.total=sh3_total_cycles;out.hits=handlerHits;
	if(native){
		Require(cv1kNativeOps>0 && cv1kNativeBuilt>0,"native CPU execution path unused");
		printf("Core native ops %llu, spans %llu, built %llu\n",
			(unsigned long long)cv1kNativeOps,(unsigned long long)cv1kNativeSpans,(unsigned long long)cv1kNativeBuilt);
	}
	Sh3SetCv1kNative(0);
	return out;
}
static UINT16 __fastcall ReadWordTest(UINT32 a) {
	return ram[(a&0xffff)>>1];
}
static void TestLongOnlyMap(){
	Sh3MapInit();
	Sh3MapMemory((UINT8*)ram,0xc000000,0xc00ffff,MAP_RAM);
	Sh3SetReadWordHandler(1,ReadWordTest);
	Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);
	for(unsigned a=0;a<64;a++){
		Require(RW(0x8c000000u+a)==ram[a>>1],"long-only map changed word semantics");
		Require(RB(0x8c000000u+a)==((UINT8*)ram)[a^1],"long-only map changed byte semantics");
	}
	puts("CV1000 long-only page, odd word and byte mapping PASS");
}
static void TestCore(){
	for(unsigned p=0;p<2;p++)for(unsigned h=0;h<2;h++){
		Snapshot a=RunCore(false,p,h),b=RunCore(true,p,h);
		Require(!memcmp(&a,&b,sizeof(a)),"CPU run loop/state/cycle mismatch");
		if(h)Require(b.hits>0,"read handler not called");
	}
	puts("SH3 run-loop, code rewrite, handler and cycle comparisons PASS");
}
int main(){TestEmitter();TestTimers();TestCore();TestLongOnlyMap();return 0;}
