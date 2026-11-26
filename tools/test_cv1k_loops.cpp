// CPU-only JIT=3 regression. Reuse (and run) the existing native suite.
#define main Cv1kPreviousMain
#include "test_cv1k_native.cpp"
#undef main

static UINT16 loopCode[32768], loopData[32768];
static void LoopState(unsigned sr=0) {
	m_cpu_type=CPU_TYPE_SH3;
	m_pc=0x80000000;m_ppc=0x12345678;m_ea=0x87654321;m_delay=0;m_sr=sr;
	m_test_irq=m_pending_irq=m_cpu_off=0;sh3_total_cycles=0;
	memset(m_r,0,sizeof(m_r));m_r[0]=0x8c000100;m_r[1]=9;m_r[2]=0x12345678;
	m_ratio=1;handlerHits=callbackTrace=0;
	for(unsigned i=0;i<3;i++){m_timer[i]={};m_timer[i].timer_prescaler=64;}
	for(unsigned i=0;i<4;i++)m_dma_timer[i]={};cave_blitter_delay={};
}
static Snapshot LoopSnapshot(int cycles) {
	Snapshot s={};memcpy(s.r,m_r,sizeof(m_r));s.sr=m_sr;s.pc=m_pc;s.ppc=m_ppc;
	s.ea=m_ea;s.delay=m_delay;s.cycles=cycles;s.total=sh3_total_cycles;
	s.hits=handlerHits;s.callbackTrace=callbackTrace;return s;
}
static void LoopSetup(const UINT16 *ops,unsigned count,bool jit=true) {
	setenv("FBNEO_CV1K_JIT",jit?"3":"0",1);
	setenv("FBNEO_CV1K_DISPATCH","1",1);setenv("FBNEO_CV1K_COMPACT","1",1);
	setenv("FBNEO_CV1K_BATCH","1",1);
	Sh3MapInit();Sh3SetCv1kNative(1);
	for(auto &op:loopCode)op=0x0009;
	memcpy(loopCode,ops,count*2);memset(loopData,0,sizeof(loopData));
	Sh3MapMemory((UINT8*)loopCode,0,0xffff,MAP_RAM);
	Sh3MapMemory((UINT8*)loopData,0xc000000,0xc00ffff,MAP_RAM);
	LoopState();
	if(jit) {
		// Build without running: all fixtures cost >=5 per taken lap.
		for(unsigned i=0;i<Cv1kHot;i++){m_pc=0x80000000;m_sh4_icount=8;Cv1kTryNative(loopCode[0]);}
		Require(cv1kNativeBuilt==1,"closed-loop fixture did not compile");
		Cv1kSet &set=cv1kSets[Cv1kHash(0)];bool found=false;
		for(auto &b:set.ways)if(b.valid && b.pc==0)found=b.loop;
		Require(found,"fixture compiled only a straight span");
	}
	LoopState();
}
static void CompareNativeExit(unsigned budget,const char *label) {
	UINT16 beforeCode[32768],beforeData[32768],afterData[32768];
	memcpy(beforeCode,loopCode,sizeof(loopCode));memcpy(beforeData,loopData,sizeof(loopData));
	Snapshot before=LoopSnapshot(0);
	m_sh4_icount=budget;
	Require(Cv1kTryNative(loopCode[0]),label);
	unsigned spent=budget-m_sh4_icount;
	Require(spent<=budget,"native loop exceeded budget");
	Snapshot actual=LoopSnapshot(spent);
	memcpy(afterData,loopData,sizeof(loopData));
	Require(!memcmp(loopCode,beforeCode,sizeof(loopCode)),"native modified validated code");
	memcpy(loopData,beforeData,sizeof(loopData));
	memcpy(m_r,before.r,sizeof(m_r));m_sr=before.sr;m_pc=before.pc;m_ppc=before.ppc;
	m_ea=before.ea;m_delay=before.delay;sh3_total_cycles=before.total;
	int used=Sh3Run_timerhack_switch<false>(spent);
	Snapshot expected=LoopSnapshot(used);
	if(memcmp(&actual,&expected,sizeof(actual))) {
		fprintf(stderr,"%s budget=%u spent=%u PC=%08x/%08x PPC=%08x/%08x EA=%08x/%08x cycles=%d/%d\n",
			label,budget,spent,actual.pc,expected.pc,actual.ppc,expected.ppc,actual.ea,expected.ea,actual.total,expected.total);
		Require(false,"native loop exit differs from interpreter");
	}
	Require(!memcmp(loopData,afterData,sizeof(loopData)),"native loop RAM differs");
}
static void TestLoopBranches() {
	unsigned cases=0;
	for(unsigned branch : {0x8900u,0x8b00u,0x8d00u,0x8f00u,0xa000u}) {
		for(unsigned t=0;t<2;t++)for(unsigned slot : {0x0009u,0x0008u,0x0018u,0x7401u}) {
			UINT16 p[]={0x7301,0x0009,UINT16(branch|((branch==0xa000)?0xffc:0xfc)),UINT16(slot)};
			LoopSetup(p,4);
			for(unsigned budget : {10u,11u,12u,17u,31u,128u,1001u}) {
				LoopState(t);CompareNativeExit(budget,"translated branch/delay");cases++;
			}
			Require(cv1kLoopHits==7 && cv1kLoopOps>0,"translated branches unused");
		}
	}
	// The comparison changes T in the body, then the slot changes R3.
	UINT16 p[]={0x7301,0x3310,0x8ffc,0x7301};
	LoopSetup(p,4);CompareNativeExit(1000,"conditional terminating loop");
	Require(m_r[3]==10 && m_pc==0x80000008,"conditional loop never took both edges");
	printf("SH3 translated BT/BF/BT.S/BF.S/BRA, taken/fallthrough, delay T: %u PASS\n",cases+1);
}
static void TestLoopMemory() {
	UINT16 p[]={0x6202,0x7201,0x2022,0x7301,0x3310,0x8bf9};
	LoopSetup(p,6);CompareNativeExit(1024,"RAM load/store loop");
	Require(m_r[3]==9 && loopData[0x81]==9,"RAM loop did not finish");
	// Guard after dirty arithmetic, including every guest alias of code.
	for(UINT32 alias : {0u,0x80000000u,0xa0000000u,0xc0000000u,0xe0000000u,0xf0000000u}) {
		UINT16 smc[]={0x7301,0x0009,0x2021,0x8bfb};
		LoopSetup(smc,4);
		// High P4 addresses are not masked by writes; map those explicitly.
		if(alias>=0xe0000000)Sh3MapMemory((UINT8*)loopCode,alias,alias+0xffff,MAP_RAM);
		m_r[0]=alias+6;CompareNativeExit(128,"alias SMC dirty exit");
		Require(m_r[3]==1 && m_pc==0x80000004,"SMC store was executed natively");
	}
	// A postincrement read crosses into a handler on the second lap.
	for(unsigned delayed=0;delayed<2;delayed++) {
		UINT16 cross[]={0x6206,0x7301,UINT16(delayed?0x8ffc:0x8bfc),0x7401};
		LoopSetup(cross,4);m_r[0]=0x8c00fffc;
		CompareNativeExit(128,"guard at first instruction after linked edge");
		Require(m_r[0]==0x8c010000 && m_r[3]==1,"read guard failed on second lap");
	}
	for(unsigned fail=0;fail<3;fail++) {
		LoopSetup(p,6);
		if(fail==0)Sh3MapHandler(1,0xc000000,0xc00ffff,MAP_WRITE);
		if(fail==1)Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);
		if(fail==2)m_r[0]++;
		m_sh4_icount=128;
		bool ran=Cv1kTryNative(p[0]);
		Require(ran==(fail==0),"memory guard progress wrong");
		Require(m_pc==(fail==0?0x80000004u:0x80000000u),"memory handler/alignment guard wrong PC");
	}
	puts("SH3 linked RAM loops, alias SMC, dirty/first-op guard, MMIO and alignment: PASS");
}
struct LoopRunResult {Snapshot state;UINT16 data[256];int time,prescale;};
static LoopRunResult RunLoopSlices(bool jit,unsigned variant,unsigned budget) {
	UINT16 p[]={0x6202,0x7201,0x2022,0x7301,0x8ffa,0x7401};
	LoopSetup(p,6,jit);
	m_timer[0].running=1;m_timer[0].time_trig=100000;
	int cycles=0;
	for(unsigned slice=0;slice<40;slice++) {
		if(slice==12) {
			if(variant==1)loopCode[1]=0x7203; // external/DMA-style direct write
			if(variant==2)cheat_write(0x80000002,0x72),cheat_write(0x80000003,0x05);
			if(variant==3) { // remap code to a different live page
				static UINT16 remapped[32768];memcpy(remapped,loopCode,sizeof(loopCode));remapped[1]=0x7207;
				Sh3MapMemory((UINT8*)remapped,0,0xffff,MAP_RAM);
			}
			if(variant==4) { // savestate-style restore of code/data/registers
				loopCode[1]=0x7209;m_r[3]=17;loopData[0x81]=33;
			}
		}
		cycles+=jit?Sh3Run_timerhack(budget+slice%3):Sh3Run_timerhack_switch<false>(budget+slice%3);
	}
	LoopRunResult r={};r.state=LoopSnapshot(cycles);memcpy(r.data,loopData,sizeof(r.data));
	r.time=m_timer[0].time_current;r.prescale=m_timer[0].prescale_counter;
	if(jit && budget>=16)Require(cv1kLoopOps>0,"slice tests did not chain");
	return r;
}
static void TestLoopSlices() {
	for(unsigned variant=0;variant<5;variant++)for(unsigned budget=1;budget<=40;budget++) {
		LoopRunResult a=RunLoopSlices(false,variant,budget),b=RunLoopSlices(true,variant,budget);
		Require(!memcmp(&a,&b,sizeof(a)),"loop slice budget/timer/live-code differential");
	}
	UINT16 p[]={0x7301,0x0009,0x8bfc};LoopSetup(p,3);
	m_test_irq=1;m_sh4_icount=100;
	Require(!Cv1kTryNative(p[0]) && !cv1kLoopHits,"pending IRQ entered native loop");
	m_test_irq=0;m_delay=4;
	Require(!Cv1kTryNative(p[0]),"pending delay entered native loop");
	puts("SH3 linked slice budgets 1..42, timers, external/cheat/remap/restore writes, pending IRQ/delay: 200 PASS");
}
int main() {
	Cv1kPreviousMain();TestLoopBranches();TestLoopMemory();TestLoopSlices();
	Sh3SetCv1kNative(0);return 0;
}
