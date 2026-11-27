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
static uint8_t *mapR[65536],*mapL[65536],*mapW[65536];
static void InitRegs(uint32_t *regs) {
	for(unsigned i=0;i<16;i++)regs[i]=Random();
}
static void TestEmitter() {
	using namespace Sh3Native;
	Cache cache;
	for(unsigned i=0;i<32768;i++)ram[i]=uint16_t(Random());
	const unsigned pages[]={0xc00u,0xec00u,0xfc00u};
	for(unsigned page : pages) {
		mapR[page]=mapL[page]=mapW[page]=(uint8_t*)ram;
		MemMapR[page]=MemMapRL[page]=MemMapW[page]=(uint8_t*)ram;
	}
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
		const unsigned top=op>>12,f=op&15,m=(op>>4)&15,n=(op>>8)&15;
		const bool store=top==1||(top==2&&(f<=2||(f>=4&&f<=6)));
		const bool pre=top==2&&f>=4&&f<=6;
		const bool memory=store||top==5||(top==6&&(f<=2||(f>=4&&f<=6)));
		const unsigned addr=store?n:m;
		const uint32_t aliases[]={0x0c000000u,0x8c000000u,0xac000000u,0xcc000000u,0xec000000u,0xfc000000u};
		if(memory)regs[addr]=(aliases[tested%6]|(Random()&0xff00))+(pre?(1u<<(f&3)):0);
		uint16_t beforeRam[32768],expectedRam[32768];
		if(store)memcpy(beforeRam,ram,sizeof(ram));
		memcpy(m_r,regs,sizeof(regs));m_sr=sr;m_ea=ea;
		m_sh4_icount=10;sh3_total_cycles=0;
		execute_one(uint16_t(op));
		Require(m_sh4_icount==10 && sh3_total_cycles==0,"non-unit-cycle instruction accepted");
		if(store){memcpy(expectedRam,ram,sizeof(ram));memcpy(ram,beforeRam,sizeof(ram));}
		unsigned done=cache.Run(offset,regs,&sr,&ea,mapR,mapL,mapW);
		if(store)Require(!memcmp(ram,expectedRam,sizeof(ram)),"native store memory mismatch");
		if(done!=1 || memcmp(regs,m_r,sizeof(regs)) || sr!=m_sr || ea!=m_ea) {
			fprintf(stderr,"opcode %04x failed done=%u SR=%08x/%08x EA=%08x/%08x\n",op,done,sr,m_sr,ea,m_ea);
			exit(2);
		}
		if(memory) {
			InitRegs(regs);regs[addr]=0x8d000100u;
			uint32_t before[16];memcpy(before,regs,sizeof(regs));uint32_t oldsr=sr,oldea=ea;
			Require(cache.Run(offset,regs,&sr,&ea,mapR,mapL,mapW)==0 &&
				!memcmp(before,regs,sizeof(regs)) && sr==oldsr && ea==oldea,"unmapped memory guard");
			// Long-only handler must override direct byte/word map.
			if(!store && (top==5 || (f&3)==2)) {
				regs[m]=0x8c000000;memcpy(before,regs,sizeof(regs));
				mapL[0xc00]=(uint8_t*)1;
				Require(cache.Run(offset,regs,&sr,&ea,mapR,mapL,mapW)==0 && !memcmp(before,regs,sizeof(regs)),"long handler bypassed");
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
		const unsigned miss=c%(MaxOps+1);
		for(unsigned i=0;i<MaxOps;i++){
			uint16_t op;
			if(i==miss){op=0x60f2;mapL[0xc00]=(uint8_t*)1;}
			else {
				do {op=uint16_t(Random());}while(!Candidate(op)||((op>>12)==1)||
					((op>>12)==2&&((op&15)<=2||((op&15)>=4&&(op&15)<=6)))||((op>>12)==5)||
					((op>>12)==6 && ((op&15)<=2||((op&15)>=4&&(op&15)<=6)))||
					((op>>8)&15)==15);
			}
			Require(e.Emit(op,i),"mixed emit");
			if(i<miss){execute_one(op);expected++;}
		}
		e.Finish(MaxOps);
		Require(cache.Publish(words,e.End()-words,offset),"mixed publish");
		unsigned done=cache.Run(offset,regs,&sr,&ea,mapR,mapL,mapW);
		if(done!=expected || memcmp(regs,m_r,sizeof(regs)) || sr!=m_sr || ea!=m_ea) {
			fprintf(stderr,"mixed %u done=%u expected=%u SR=%08x/%08x\n",c,done,expected,sr,m_sr);exit(3);
		}
		mapL[0xc00]=(uint8_t*)ram;
	}
	puts("SH3 mixed spans and dirty state at guard exits: 4096 PASS");
}

// Exercise both edges of the protected HOST interval. A guest write mapping
// can alias a different fetch page, including a long store straddling its start.
static void TestStoreGuards() {
	using namespace Sh3Native;
	Cache cache;
	const uint16_t stores[]={0x2f00,0x2f01,0x2f02,0x2f04,0x2f05,0x2f06,0x1f03};
	unsigned tested=0;
	for(uint16_t op:stores) {
		uint32_t words[MaxWords],offset;
		Emitter e(words);
		for(unsigned i=0;i<16;i++)Require(e.Emit(0x7001|((i%14)<<8),i),"store prefix");
		Require(e.Emit(op,16),"guard store emit");e.Finish(17);
		Require(cache.Publish(words,e.End()-words,offset),"guard store publish");
		const unsigned bytes=(op>>12)==1?4:1u<<(op&3);
		const bool pre=(op>>12)==2&&(op&15)>=4;
		for(unsigned dest=90;dest<240;dest++) {
			for(unsigned i=0;i<32768;i++)ram[i]=uint16_t(i*173+dest);
			uint16_t before[32768],expected[32768];memcpy(before,ram,sizeof(ram));
			uint32_t regs[16],sr=Random(),ea=Random();InitRegs(regs);
			regs[15]=0xac000000u+dest+(pre?bytes:0)-((op>>12)==1?12:0);
			memcpy(m_r,regs,sizeof(regs));m_sr=sr;m_ea=ea;
			for(unsigned i=0;i<16;i++)execute_one(0x7001|((i%14)<<8));
			const unsigned host=bytes==1?dest^1:dest;
			const bool guard=(dest&(bytes-1)) || (host<230 && host+bytes>102);
			if(!guard)execute_one(op);
			memcpy(expected,ram,sizeof(ram));memcpy(ram,before,sizeof(ram));
			unsigned done=cache.Run(offset,regs,&sr,&ea,mapR,mapL,mapW,(uint8_t*)ram+102,128);
			Require(done==(guard?16u:17u) && !memcmp(regs,m_r,sizeof(regs)) && sr==m_sr && ea==m_ea &&
				!memcmp(expected,ram,sizeof(ram)),"store alignment/code overlap guard or prefix state");
			tested++;
		}
	}
	printf("SH3 store alias/overlap/alignment guards and dirty prefix: %u PASS\n",tested);
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
	uint32_t callbackTrace;
};
static unsigned handlerHits;
static uint32_t callbackTrace;
static UINT32 __fastcall ReadLongTest(UINT32 a) {
	handlerHits++;
	callbackTrace=callbackTrace*33+Sh3GetPC(-1)+UINT32(Sh3TotalCycles())*17+UINT32(m_sh4_icount);
	return 0x12345678u ^ a;
}
static Snapshot RunCore(unsigned mode, bool patch, bool handler)
{
	Sh3MapInit();
	setenv("FBNEO_CV1K_JIT",mode==2?"1":mode==5?"2":"0",1);
	setenv("FBNEO_CV1K_DISPATCH",(mode==1||mode>=3)?"1":"0",1);
	setenv("FBNEO_CV1K_COMPACT",mode>=3?"1":"0",1);
	setenv("FBNEO_CV1K_BATCH",mode>=4?"1":"0",1);
	Sh3SetCv1kNative(1);
	Require(cv1kNativeEnabled==(mode==2||mode==5) && cv1kFlatEnabled==(mode==1||mode>=3) && cv1kCompactEnabled==(mode>=3) && cv1kBatchEnabled==(mode>=4) && cv1kHybridEnabled==(mode==5),"wrong CPU execution policy");
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
	handlerHits=0;callbackTrace=0;
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
	out.cycles=cycles;out.total=sh3_total_cycles;out.hits=handlerHits;out.callbackTrace=callbackTrace;
	if(mode==3) Require(cv1kCompactActive,"compact dispatch did not execute");
	if(mode>=4) Require(cv1kBatchActive,"batched dispatch did not execute");
	if(mode==2 || (mode==5 && !handler)){
		Require(cv1kNativeOps>0 && cv1kNativeBuilt>0,"native CPU execution path unused");
		printf("Core native ops %llu, spans %llu, built %llu\n",
			(unsigned long long)cv1kNativeOps,(unsigned long long)cv1kNativeSpans,(unsigned long long)cv1kNativeBuilt);
	}
	Sh3SetCv1kNative(0);
	return out;
}
static uint16_t storeData[32768];
static void StoreTrace(UINT32 a,UINT32 v) {
	handlerHits++;
	callbackTrace=callbackTrace*33+Sh3GetPC(-1)+UINT32(Sh3TotalCycles())*17+UINT32(m_sh4_icount)+a+v;
}
static void __fastcall StoreByteTest(UINT32 a,UINT8 v) {
	StoreTrace(a,v);((UINT8*)storeData)[(a&0xffff)^1]=v;
}
static void __fastcall StoreWordTest(UINT32 a,UINT16 v) {
	StoreTrace(a,v);storeData[(a&0xffff)>>1]=v;
}
static void __fastcall StoreLongTest(UINT32 a,UINT32 v) {
	StoreTrace(a,v);UINT32 swapped=(v<<16)|(v>>16);memcpy((UINT8*)storeData+(a&0xffff),&swapped,4);
}
struct StoreResult {
	Snapshot state;
	UINT16 code[164],data[32768];
	int timerTime, prescale;
};
static StoreResult RunStoreProgram(bool hybrid,unsigned scenario) {
	setenv("FBNEO_CV1K_JIT",hybrid?"2":"0",1);
	setenv("FBNEO_CV1K_DISPATCH",hybrid?"1":"0",1);
	setenv("FBNEO_CV1K_COMPACT","1",1);setenv("FBNEO_CV1K_BATCH","1",1);
	Sh3MapInit();Sh3SetCv1kNative(1);
	static UINT16 code[32768];memset(code,0,sizeof(code));memset(storeData,0,sizeof(storeData));
	const UINT16 pattern[]={0x7001,0x2f02,0x61f2,0x321c,0x2f04,0x63f4,0x0018,0x6453};
	for(unsigned i=0;i<160;i++)code[i]=pattern[i%8];
	code[22]=0x2dc1;code[40]=0x7001;
	code[160]=0xa000|((-162)&0xfff);code[161]=0x7401; // branch with nontrivial delay
	Sh3MapMemory((UINT8*)code,0,0xffff,MAP_RAM);
	Sh3MapMemory((UINT8*)code,0x10000,0x1ffff,MAP_RAM); // distinct guest alias
	Sh3MapMemory((UINT8*)storeData,0xc000000,0xc00ffff,MAP_RAM);
	Sh3SetWriteByteHandler(1,StoreByteTest);Sh3SetWriteWordHandler(1,StoreWordTest);Sh3SetWriteLongHandler(1,StoreLongTest);
	m_pc=0x80000000;m_ppc=m_ea=m_delay=m_sr=0;
	m_test_irq=m_pending_irq=m_cpu_off=0;sh3_total_cycles=0;
	memset(m_r,0,sizeof(m_r));m_r[15]=0x8c000100;m_r[13]=0xac000200;m_r[12]=0x7007;
	m_ratio=1;handlerHits=0;callbackTrace=0;
	for(unsigned i=0;i<3;i++){m_timer[i]={};m_timer[i].timer_prescaler=64;}
	m_timer[0].running=1;m_timer[0].time_trig=100000;
	for(unsigned i=0;i<4;i++)m_dma_timer[i]={};cave_blitter_delay={};
	int cycles=0;
	for(unsigned i=0;i<96;i++) {
		if(i==32) {
			if(hybrid)Require(cv1kNativeOps>cv1kNativeSpans*24 && cv1kNativeBuilt>0,"long store spans not executing");
			if(scenario==1)Sh3MapHandler(1,0xc000000,0xc00ffff,MAP_WRITE);
			if(scenario==2)m_r[13]=0xa0010050; // rewrite upcoming opcode through alias
			if(scenario==3)m_r[13]=0xa0010000; // rewrite already executed opcode
			if(scenario==4)code[40]=0x7009; // direct DMA/save-style RAM replacement
			if(scenario==5){cheat_write(0xa0010050,0x70);cheat_write(0xa0010051,0x09);}
			if(scenario==6)Sh3MapMemory((UINT8*)storeData,0x10000,0x1ffff,MAP_WRITE);
		}
		if(i==64 && scenario==1)Sh3MapMemory((UINT8*)storeData,0xc000000,0xc00ffff,MAP_WRITE);
		// Include budgets below/at/above the cap, mid-block starts and delay slots.
		cycles+=Sh3Run_timerhack(i<32?521:1+((i*37)%257));
	}
	StoreResult out={};Snapshot &v=out.state;
	memcpy(v.r,m_r,sizeof(m_r));v.sr=m_sr;v.pc=m_pc;v.ppc=m_ppc;v.ea=m_ea;v.delay=m_delay;
	v.cycles=cycles;v.total=sh3_total_cycles;v.hits=handlerHits;v.callbackTrace=callbackTrace;
	memcpy(out.code,code,sizeof(out.code));memcpy(out.data,storeData,sizeof(storeData));
	out.timerTime=m_timer[0].time_current;out.prescale=m_timer[0].prescale_counter;
	if(scenario==1)Require(handlerHits>0,"store handlers unused");
	if(scenario==2)Require(code[40]==0x7007,"future SMC unused");
	if(scenario==3)Require(code[0]==0x7007,"past SMC unused");
	Sh3SetCv1kNative(0);return out;
}
static void TestStorePrograms() {
	for(unsigned scenario=0;scenario<7;scenario++) {
		StoreResult a=RunStoreProgram(false,scenario),b=RunStoreProgram(true,scenario);
		Require(!memcmp(&a,&b,sizeof(a)),"long store span run-loop, SMC, handler trace or timer mismatch");
	}
	puts("SH3 long store spans: 7 switch/hybrid RAM, alias SMC, external rewrite, cheat, remap, MMIO and timer comparisons PASS");
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
		Snapshot a=RunCore(0,p,h),b=RunCore(1,p,h),c=RunCore(2,p,h),d=RunCore(3,p,h),e=RunCore(4,p,h),f=RunCore(5,p,h);
		Require(!memcmp(&a,&b,sizeof(a)),"flat CPU run loop/state/cycle mismatch");
		Require(!memcmp(&a,&c,sizeof(a)),"JIT CPU run loop/state/cycle mismatch");
		Require(!memcmp(&a,&d,sizeof(a)),"compact CPU run loop/state/cycle mismatch");
		Require(!memcmp(&a,&e,sizeof(a)),"batched CPU run loop/state/cycle mismatch");
		Require(!memcmp(&a,&f,sizeof(a)),"hybrid CPU run loop/state/cycle mismatch");
		if(h)Require(b.hits>0 && c.hits>0 && d.hits>0 && e.hits>0 && f.hits>0,"read handler not called");
	}
	puts("SH3 switch/flat/compact/batch/JIT/hybrid run-loop, code rewrite, handler and cycle comparisons PASS");
}
static Snapshot RunDispatchProgram(unsigned mode,unsigned seed)
{
	setenv("FBNEO_CV1K_JIT","0",1);
	setenv("FBNEO_CV1K_DISPATCH",mode?"1":"0",1);
	setenv("FBNEO_CV1K_COMPACT",mode>=2?"1":"0",1);
	setenv("FBNEO_CV1K_BATCH",mode>=3?"1":"0",1);
	Sh3MapInit();Sh3SetCv1kNative(1);
	static UINT16 code[32768],data[32768];
	memset(code,0,sizeof(code));memset(data,0,sizeof(data));
	// Mix comparison, taken/untaken branches, delay slots, direct
	// memory writes/reads and a changing program loop. R15 is RAM.
	const UINT16 program[]={0xe001,0xe105,0x301c,0x2f02,0x62f2,
		0x3210,0x8901,0x72ff,0x72fe,0x4028,0x2029,0x4019,
		0x71ff,0x2118,0x8df1,0xe105,0xafeF,0x0009};
	memcpy(code,program,sizeof(program));
	Sh3MapMemory((UINT8*)code,0,0xffff,MAP_RAM);
	Sh3MapMemory((UINT8*)data,0xc000000,0xc00ffff,MAP_RAM);
	m_pc=0x80000000;m_ppc=m_ea=m_delay=m_sr=0;
	m_test_irq=m_pending_irq=m_cpu_off=0;sh3_total_cycles=0;
	memset(m_r,0,sizeof(m_r));m_r[15]=0x8c000000u;m_r[0]=seed;
	m_ratio=1.0;
	for(unsigned i=0;i<3;i++){m_timer[i]={};m_timer[i].timer_prescaler=64;}
	for(unsigned i=0;i<4;i++)m_dma_timer[i]={};
	cave_blitter_delay={};
	int cycles=0;
	for(unsigned i=0;i<64;i++)cycles+=Sh3Run_timerhack(13+(seed+i)%31);
	Snapshot out={};
	memcpy(out.r,m_r,sizeof(m_r));out.sr=m_sr;out.pc=m_pc;out.ppc=m_ppc;out.ea=m_ea;out.delay=m_delay;
	out.cycles=cycles;out.total=sh3_total_cycles;
	out.hits=uint32_t(data[0])|(uint32_t(data[1])<<16);
	Sh3SetCv1kNative(0);return out;
}
static void TestFlat(){
	for(unsigned i=0;i<64;i++){
		Snapshot a=RunDispatchProgram(0,i),b=RunDispatchProgram(1,i),c=RunDispatchProgram(2,i),d=RunDispatchProgram(3,i);
		Require(!memcmp(&a,&b,sizeof(a)),"flat dispatch branch or store state differs");
		Require(!memcmp(&a,&c,sizeof(a)),"compact dispatch branch or store state differs");
		Require(!memcmp(&a,&d,sizeof(a)),"batched dispatch branch or store state differs");
	}
	puts("SH3 flat, compact and batched dispatcher mixed branch/store programs: 64 each PASS");
}
static void TestDtLoop(){
#if BUSY_LOOP_HACKS
	Sh3MapInit();
	static UINT16 code[32768]={};
	code[0]=0x8bfd;
	Sh3MapMemory((UINT8*)code,0,0xffff,MAP_RAM);
	for(unsigned i=0;i<20000;i++){
		UINT32 r=(i%4==0)?Random():Random()%2000;
		INT32 cyc=INT32(Random()%20001)-10;
		UINT32 sr=Random();
		UINT32 expected=r-1;
		INT32 left=cyc,spent=0;
		UINT32 expectedSr=(sr&~T)|(expected==0?T:0);
		while(expected>1 && left>4){expected--;left-=4;spent+=4;}
		m_r[0]=r;m_sr=sr;m_ppc=0;m_sh4_icount=cyc;sh3_total_cycles=0;
		DT(0x4010);
		Require(m_r[0]==expected && m_sr==expectedSr &&
			m_sh4_icount==left && sh3_total_cycles==spent,"DT busy loop cycle mismatch");
	}
	puts("SH3 DT/BF busy-loop batching: 20000 PASS");
#endif
}

// Force a tiny cache to fill, then verify that no new emission/reset
// occurs and previously published code remains callable.
static void TestCachePressure(){
	setenv("FBNEO_CV1K_JIT","1",1);
	Sh3MapInit();Sh3SetCv1kNative(1);
	cv1kCode.Configure(size_t(sysconf(_SC_PAGESIZE)),true);
	static UINT16 code[32768];
	for(unsigned i=0;i<32768;i+=16){
		for(unsigned j=0;j<8;j++)code[i+j]=0x7001;
		code[i+8]=0xafff;
	}
	Sh3MapMemory((UINT8*)code,0,0xffff,MAP_RAM);
	m_test_irq=m_delay=0;
	for(unsigned i=0;i<1024 && !cv1kCodeFull;i++){
		for(unsigned h=0;h<Cv1kHot;h++){
			m_pc=i*32;m_sh4_icount=128;
			Cv1kTryNative(code[i*16]);
		}
	}
	Require(cv1kCodeFull && cv1kNativeFull==1 && cv1kNativeBuilt>0,"tiny cache did not saturate");
	const auto built=cv1kNativeBuilt;
	const auto used=cv1kCode.Used();
	for(unsigned i=1024;i<1200;i++)for(unsigned h=0;h<Cv1kHot;h++){
		m_pc=i*32;m_sh4_icount=128;Cv1kTryNative(code[i*16]);
	}
	Require(cv1kNativeFull==1 && cv1kNativeBuilt==built && cv1kCode.Used()==used,"full cache recompiled or reset");
	m_pc=0;m_sh4_icount=128;UINT32 before=m_r[0];
	Require(Cv1kTryNative(code[0]) && m_r[0]==before+8 && m_pc==16,"cached span unavailable after saturation");
	Sh3SetCv1kNative(0);
	cv1kCode.Configure(Sh3Native::Cache::DefaultCapacity,true);
	puts("SH3 saturated cache preserves prior spans: PASS");
}
static INT32 idleBurnTest=117;
static const UINT32 idleAddrTest=0xc002310,idlePCTest=0xc1d1346;
static UINT32 __fastcall IdleReadTest(UINT32 a) {
	if (a==idleAddrTest) {
		const UINT32 pc=Sh3GetPC(-1);
		if (pc==idlePCTest || pc==idlePCTest+2) Sh3BurnCycles(idleBurnTest);
	}
	UINT32 v=*((UINT32 *)((UINT8*)ram+(a&0xfffc)));
#ifdef LSB_FIRST
	v=(v<<16)|(v>>16);
#endif
	return v;
}
static void TestIdleDirect(){
	Sh3MapInit();
	Sh3MapMemory((UINT8*)ram,0xc000000,0xc00ffff,MAP_RAM);
	Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);
	Sh3SetReadLongHandler(1,IdleReadTest);
	for (unsigned c=0;c<20000;c++) {
		const UINT32 address=(c%5==0?idleAddrTest:0xc000000+(Random()&0xffff)) | (c&1?0x80000000:0);
		const UINT32 pc=(c%3==0?idlePCTest:c%3==1?idlePCTest+2:0xc000004);
		const UINT32 delay=c%7==0?idlePCTest:0;
		UINT32 values[2];INT32 totals[2],left[2];
		for(unsigned mode=0;mode<2;mode++){
			setenv("FBNEO_CV1K_IDLE_DIRECT",mode?"1":"0",1);
			Sh3SetCv1kIdleRead((UINT8*)ram,idleAddrTest,idlePCTest,&idleBurnTest,IdleReadTest);
			m_pc=pc;m_delay=delay;sh3_total_cycles=19;m_sh4_icount=101;
			values[mode]=RL(address);totals[mode]=sh3_total_cycles;left[mode]=m_sh4_icount;
		}
		Require(values[0]==values[1]&&totals[0]==totals[1]&&left[0]==left[1],"idle read alignment, PC or timing mismatch");
	}
	Sh3SetReadLongHandler(1,ReadLongTest);
	handlerHits=0;
	Require(RL(idleAddrTest)==(0x12345678u^idleAddrTest)&&handlerHits==1,"idle path bypassed changed handler");
	Sh3MapInit();
	Require(!cv1kIdlePage,"idle page survived mapping reset");
	puts("CV1000 direct idle-page reads: 20000 value/cycle comparisons and handler invalidation PASS");
}
// Verify the audited pure-op list itself: every encoding classified as
// pure must consume exactly one cycle, not access PC, delay or pending IRQ.
static void TestPureHandlers(){
	bool pure[SH3_ID_COUNT]={};
#define SH3_OPCODE(name)
#define SH3_PURE_OPCODE(name) pure[SH3_ID_##name]=true;
#include "../src/cpu/sh4/sh3_decode_ops.inc"
#undef SH3_PURE_OPCODE
#undef SH3_OPCODE
	InitFlatDecode();
	unsigned tested=0;
	for(unsigned op=0;op<65536;op++) if(pure[sh3OpIds[op]]){
		for(unsigned sample=0;sample<4;sample++){
			InitRegs(m_r);m_sr=Random();m_pc=0x80000004;m_ppc=0x80000004;m_delay=0;
			m_ea=0x12345678;m_test_irq=0;m_cpu_off=0;m_sh4_icount=25;sh3_total_cycles=100;
			execute_one(UINT16(op));
			Require(m_pc==0x80000004 && m_ppc==m_pc && !m_delay && !m_test_irq && !m_cpu_off &&
				m_ea==0x12345678 && m_sh4_icount==25 && sh3_total_cycles==100,"pure handler modified externally visible state");
		}
		tested++;
	}
	printf("SH3 audited pure op classification: %u encodings, four states each PASS\n",tested);
}
static Snapshot RunPurePages(bool batch,unsigned seed){
	setenv("FBNEO_CV1K_JIT","0",1);
	setenv("FBNEO_CV1K_DISPATCH",batch?"1":"0",1);
	setenv("FBNEO_CV1K_COMPACT","1",1);
	setenv("FBNEO_CV1K_BATCH",batch?"1":"0",1);
	Sh3MapInit();Sh3SetCv1kNative(1);
	static UINT16 pages[65536];
	for(unsigned i=0;i<65536;i++) pages[i]=0x7000|(i%15)<<8|((i+seed)&255);
	Sh3MapMemory((UINT8*)pages,0,0x1ffff,MAP_RAM);
	m_pc=0x8000ffd0;m_ppc=m_ea=m_delay=m_sr=0;
	m_test_irq=m_pending_irq=m_cpu_off=0;sh3_total_cycles=0;
	memset(m_r,0,sizeof(m_r));
	m_ratio=1;
	for(unsigned i=0;i<3;i++){m_timer[i]={};m_timer[i].timer_prescaler=64;}
	for(unsigned i=0;i<4;i++)m_dma_timer[i]={};
	cave_blitter_delay={};
	int cycles=0;
	for(unsigned i=0;i<64;i++){
		// Cross a fetch page boundary and rewrite the next live opcode.
		if(i==10)pages[(m_pc&AM)>>1]=0xe77f;
		cycles+=Sh3Run_timerhack(1+(seed+i)%17);
	}
	Snapshot out={};memcpy(out.r,m_r,sizeof(m_r));out.sr=m_sr;out.pc=m_pc;out.ppc=m_ppc;out.ea=m_ea;out.delay=m_delay;out.cycles=cycles;out.total=sh3_total_cycles;
	if(batch)Require(cv1kBatchActive,"batch page test did not exercise new path");
	Sh3SetCv1kNative(0);return out;
}
static void TestPurePages(){
	for(unsigned i=0;i<64;i++){
		Snapshot a=RunPurePages(false,i),b=RunPurePages(true,i);
		Require(!memcmp(&a,&b,sizeof(a)),"batched live page/state mismatch");
	}
	puts("SH3 batched fetch-page crossing and live code patch: 64 PASS");
}
// A device read requests an IRQ during a branch delay slot. The
// saved return PC, banked registers and IRQ callback cycle budget must
// match the original core, not the deferred interpreter's local PC.
static UINT32 __fastcall IrqReadTest(UINT32 a){
	Sh3SetIRQLine(2,CPU_IRQSTATUS_HOLD);
	return ReadLongTest(a);
}
static Snapshot RunIrq(bool batch,bool delay,bool hybrid=false){
	setenv("FBNEO_CV1K_JIT",hybrid?"2":"0",1);
	setenv("FBNEO_CV1K_DISPATCH",batch?"1":"0",1);
	setenv("FBNEO_CV1K_COMPACT","1",1);
	setenv("FBNEO_CV1K_BATCH",batch?"1":"0",1);
	Sh3MapInit();Sh3SetCv1kNative(1);
	static UINT16 code[32768];
	for(auto &op:code)op=0x0009;
	// Pure operations, then a device load either directly or as a
	// branch delay instruction; the IRQ handler contains pure ops.
	code[0]=0xe003;code[1]=0x7005;
	code[18]=delay?0xa002:0x0009;code[19]=0x62f2;
	// Bound the direct-load block too: the JIT requires the whole
	// candidate span to fit the 35-cycle slice even when the MMIO
	// guard will exit at instruction 19.
	code[20]=0xa000;
	code[0x300]=0xe70a;code[0x301]=0x7701;
	Sh3MapMemory((UINT8*)code,0,0xffff,MAP_RAM);
	Sh3MapMemory((UINT8*)ram,0xc000000,0xc00ffff,MAP_RAM);
	Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);
	Sh3SetReadLongHandler(1,IrqReadTest);
	if(hybrid) {
		m_test_irq=m_delay=0;
		for(unsigned h=0;h<Cv1kHot;h++){m_pc=0x80000000;m_sh4_icount=128;Cv1kTryNative(code[0]);}
		Require(cv1kNativeBuilt>0,"IRQ native prefix not built");
	}
	m_cpu_type=CPU_TYPE_SH3;
	m_pc=0x80000000;m_vbr=0x80000000;m_ppc=m_ea=m_delay=m_sr=m_spc=m_ssr=0;
	m_test_irq=m_pending_irq=m_cpu_off=0;sh3_total_cycles=0;
	memset(m_irq_line_state,0,sizeof(m_irq_line_state));
	memset(m_exception_requesting,0,sizeof(m_exception_requesting));
	memset(m_exception_priority,0,sizeof(m_exception_priority));
	m_exception_priority[SH4_INTC_IRL0+2-SH4_IRL0]=15<<8;
	memset(m_rbnk,0,sizeof(m_rbnk));memset(m_r,0,sizeof(m_r));m_r[15]=0x8c000000;
	m_ratio=1;handlerHits=0;callbackTrace=0;
	for(unsigned i=0;i<3;i++){m_timer[i]={};m_timer[i].timer_prescaler=64;}
	for(unsigned i=0;i<4;i++)m_dma_timer[i]={};
	cave_blitter_delay={};
	const UINT64 beforeNative=cv1kNativeOps;
	int cycles=Sh3Run_timerhack(35);
	if(hybrid)Require(cv1kNativeOps>beforeNative,"IRQ native prefix unused");
	Require(handlerHits==1 && (m_sr&BL) && m_spc==(delay?0x8000002c:0x80000028),"IRQ was not delivered at expected PC");
	Snapshot out={};memcpy(out.r,m_r,sizeof(m_r));out.sr=m_sr;out.pc=m_pc;out.ppc=m_ppc;out.ea=m_spc;out.delay=m_delay;out.cycles=cycles;out.total=sh3_total_cycles;out.hits=handlerHits;out.callbackTrace=callbackTrace;
	Sh3SetCv1kNative(0);return out;
}
static void TestBatchIrq(){
	for(unsigned d=0;d<2;d++){
		Snapshot a=RunIrq(false,d),b=RunIrq(true,d),c=RunIrq(true,d,true);
		Require(!memcmp(&a,&b,sizeof(a)) && !memcmp(&a,&c,sizeof(a)),"batched/hybrid IRQ boundary state differs");
	}
	puts("SH3 batched/hybrid native prefix, callback IRQ and delay slot IRQ: PASS");
}
static void TestHotPC(){
	setenv("FBNEO_CV1K_HOTPCS","1",1);
	setenv("FBNEO_CV1K_JIT","0",1);
	Sh3MapInit();Sh3SetCv1kNative(1);
	static UINT16 code[32768];
	code[0]=0x7001;
	Sh3MapMemory((UINT8*)code,0,0xffff,MAP_RAM);
	m_pc=0x80000000;m_delay=m_cpu_off=0;
	m_sh4_icount=123;sh3_total_cycles=456;
	for(unsigned i=0;i<1000000 && cv1kSampleEnabled;i++)Cv1kSamplePC();
	Require(!cv1kSampleEnabled && cv1kSamples==32768 && !cv1kSampleDropped,
		"hot PC sample bound incorrect");
	unsigned found=0;
	for(const auto &e:cv1kHotPC) if(e.hits) {
		Require(e.pc==0x80000000 && e.opcode==0x7001 && e.hits==32768,"hot PC sample value incorrect");
		found++;
	}
	Require(found==1 && m_pc==0x80000000 && !m_delay && m_sh4_icount==123 &&
		sh3_total_cycles==456,"hot PC sample affected execution state");
	Sh3SetCv1kNative(0);unsetenv("FBNEO_CV1K_HOTPCS");
	puts("SH3 bounded opt-in hot PC sampling: PASS");
}
int main(){
	// Keep the historical interpreter/JIT comparison independent of the preset.
	setenv("FBNEO_CV1K_FETCH_REUSE","0",1);
	setenv("FBNEO_CV1K_DIRECT_MEM","0",1);
	setenv("FBNEO_CV1K_SLOT_DISPATCH","0",1);
	setenv("FBNEO_CV1K_DATA_LOOPS","0",1);
	setenv("FBNEO_CV1K_DATA_FULL","0",1);
	setenv("FBNEO_CV1K_POLL","0",1);
	setenv("FBNEO_CV1K_TIMER_FAST","0",1);
	TestEmitter();TestStoreGuards();TestTimers();TestCore();TestStorePrograms();TestFlat();TestPureHandlers();TestPurePages();TestBatchIrq();TestLongOnlyMap();TestIdleDirect();TestDtLoop();TestCachePressure();TestHotPC();return 0;}
