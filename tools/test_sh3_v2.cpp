// Stage 3.0A: real emitted code, cache lifecycle, and differential
// execution of the new hybrid path against the stage 2.7 dispatcher.
#define main InterpreterMain
#include "test_sh3_interpreter.cpp"
#undef main
#if !SH3_NATIVE_V2
#error AArch64 Linux little-endian memfd support required
#endif
#include <vector>
#include <algorithm>
static void CacheTest(){
	const size_t page=size_t(sysconf(_SC_PAGESIZE));
	Sh3V2::CodeCache c(page);
	const UINT32 a[]={0xd503245f,0x52800f60,0xd65f03c0}; // mov w0,#123
	const UINT32 b[]={0xd503245f,0x52803900,0xd65f03c0}; // mov w0,#456
	UINT32 x=0,y=0,unused=0;
	Sh3Native::Memory mem={};
	Check(c.Publish(a,3,x) && c.Publish(b,3,y),"dual cache publish");
	Check(x==0 && y==16 && c.WriteBase()!=c.ExecBase(),"separate aliases/alignment");
	Check(c.Run(x,m_r,&m_sr,&m_ea,mem)==123 &&
		c.Run(y,m_r,&m_sr,&m_ea,mem)==456,"published code remains executable");
	std::vector<UINT32> filler((page-c.Used())/4,0xd503201f);
	Check(c.Publish(filler.data(),filler.size(),unused),"fill exact page");
	Check(!c.Publish(a,3,unused) && !c.Denied() && c.Used()==page,"bounded full cache");
	Check(c.Run(x,m_r,&m_sr,&m_ea,mem)==123,"full retains older code");
	c.Reset();
	Check(!c.WriteBase() && !c.ExecBase() && !c.Used(),"reset unmaps");
	Check(c.Publish(b,3,x) && c.Run(x,m_r,&m_sr,&m_ea,mem)==456,"new cache after reset");
	Sh3V2::CodeCache invalid(page+16);
	Check(!invalid.Publish(a,3,x) && invalid.Denied(),"unsupported mapping size falls back");
	puts("V2 RW/RX cache: publish, execute, fill, retain, reset, denied PASS");
}
static void Init(bool v2,unsigned test=0){
	// Old test suites remain explicit controls on the C-3 branch.
	if(!getenv("FBNEO_CV1K_V2_RESIDENT"))setenv("FBNEO_CV1K_V2_RESIDENT","0",1);
	setenv("FBNEO_CV1K_JIT_V2",v2?"1":"0",1);
	setenv("FBNEO_CV1K_V2_LOOKUP_ONLY","0",1);
	setenv("FBNEO_CV1K_FAST_RAM","0",1);
	setenv("FBNEO_CV1K_DATA_LOOPS","0",1);
	setenv("FBNEO_CV1K_DATA_FULL","0",1);
	setenv("FBNEO_CV1K_POLL","0",1);
	setenv("FBNEO_CV1K_INTERP_LOOPS","0",1);
	setenv("FBNEO_CV1K_TIMER_FAST","1",1);
	Setup(10,test);
	Check(cv1kV2Enabled==v2 && cv1kFlowEnabled && cv1kFetchPageEnabled &&
		!cv1kNativeEnabled && !cv1kPCRelEnabled,"V2 independent flow policy");
}
static void Loop(unsigned n,unsigned kind,unsigned start=0){
	for(unsigned i=0;i<n;i++){
		const UINT16 pureOps[]={0x7001,0x7213,0x302c,0x202a,0x3020,0x0429,0x6443,0x6409};
		const UINT16 memOps[]={0x6312,0x7301,0x2132,0x7a01};
		const UINT16 smcOps[]={0x7001,0x2131,0x7213,0x7a01};
		code[start+i]=kind==0?pureOps[i%8]:kind==1?memOps[i%4]:smcOps[i%4];
	}
	code[start+n]=0xa000|((-int(n)-2)&0xfff);
	code[start+n+1]=0x0009;
	m_pc=0x80000000+start*2;m_r[1]=0x8c000100;m_r[3]=0xe77f;
}
struct Snapshot {
	State state;
	UINT32 extra[9];
	UINT64 codeHash;
};
static Snapshot Snap(int spent){
	Snapshot r={};r.state=Save(spent);
	const UINT32 x[]={m_pr,m_gbr,m_vbr,m_dbr,m_sgr,m_fpul,m_fpscr,m_pending_irq,m_test_irq};
	memcpy(r.extra,x,sizeof(x));
	for(auto v:code)r.codeHash=r.codeHash*1099511628211ull^v;
	return r;
}
static void Compare(const Snapshot &a,const Snapshot &b,unsigned test,unsigned step){
	if(memcmp(&a,&b,sizeof(a))){
		fprintf(stderr,"V2 mismatch test=%u step=%u pc=%08x/%08x total=%d/%d trace=%llx/%llx\n",
			test,step,a.state.pc,b.state.pc,a.state.total,b.state.total,
			(unsigned long long)a.state.trace,(unsigned long long)b.state.trace);
		for(unsigned i=0;i<16;i++)if(a.state.r[i]!=b.state.r[i])
			fprintf(stderr," r%u=%08x/%08x\n",i,a.state.r[i],b.state.r[i]);
		fprintf(stderr," ppc=%08x/%08x EA=%08x/%08x SR=%08x/%08x PR=%08x/%08x delay=%08x/%08x spent=%d/%d left=%d/%d\n",
			a.state.ppc,b.state.ppc,a.state.ea,b.state.ea,a.state.sr,b.state.sr,a.extra[0],b.extra[0],
			a.state.delay,b.state.delay,a.state.spent,b.state.spent,a.state.left,b.state.left);
		const auto *ap=reinterpret_cast<const UINT8*>(&a),*bp=reinterpret_cast<const UINT8*>(&b);
		for(unsigned i=0;i<sizeof(a);i++)if(ap[i]!=bp[i]){fprintf(stderr," first differing byte %u: %02x/%02x\n",i,ap[i],bp[i]);break;}
		exit(1);
	}
}
static std::vector<Snapshot> Scenarios(bool on,unsigned test){
	Init(on);
	unsigned start=test==6?32768-24:0;
	const unsigned len=test==1?40:20;
	Loop(len,test==3?2:test==2||test==4?1:0,start);
	if(test==6)m_pc=0x8000ffd0;
	// Timer expiry and IRQ/remap remain in original slice machinery.
	if(test==5){
		m_timer[0].timer_prescaler=1;m_timer[0].timer_exec=TimerTest;
		m_timer[0].start(21000,0,1,0);
	}
	std::vector<Snapshot> states;
	for(unsigned step=0;step<24;step++){
		if(step==8){
			if(on)Check(cv1kV2Stats.ops>0,"warm native execution before mutation");
			m_pc=0x80000000+start*2;m_delay=0;
			switch(test){
			case 0: code[7]=0xe77f;break; // interior opcode changes
			case 1: memcpy(replacement,code,sizeof(code));replacement[9]=0xe77f;Remap();break;
			case 2: m_r[1]=0x8d000100;scenario=3;break; // MMIO + extra cycles
			case 3: Sh3MapMemory((UINT8*)code,0xc000000,0xc00ffff,MAP_WRITE);
				m_r[1]=0xac00000c;break; // store into its own compiled span
			case 4: Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);scenario=1;break;
			case 5: break;
			case 6: break;
			}
		}
		const int budget=step<8?3073:(step%7==0?1:30+int(step*7%127));
		states.push_back(Snap(Sh3Run_timerhack(budget)));
	}
	if(on){
		Check(cv1kV2Stats.built>0 && cv1kV2Stats.ops>0,"native execution actually occurred");
		if(test<=1)Check(cv1kV2Stats.changed>0,"live code or remap invalidated");
		if(test>=2 && test<=4)Check(cv1kV2Stats.guards>0,"callback or SMC guard hit");
		printf("V2 case %u: built %llu ops %llu changes %llu guards %llu\n",test,
			(unsigned long long)cv1kV2Stats.built,(unsigned long long)cv1kV2Stats.ops,
			(unsigned long long)cv1kV2Stats.changed,(unsigned long long)cv1kV2Stats.guards);
	}
	return states;
}
static void TestPolicy(){
	TestDefaultPolicy();
	Init(false);
	unsetenv("FBNEO_CV1K_JIT_V2");
	Sh3SetCv1kNative(1);
	Check(cv1kV2Enabled==cv1kPerformanceDefault,"dedicated branch default");

	setenv("FBNEO_CV1K_JIT_V2","1",1);
	Sh3SetCv1kNative(1);
	Check(cv1kV2Enabled && cv1kFlowEnabled && cv1kV2Table,"explicit enable");
	setenv("FBNEO_CV1K_PCREL_DISPATCH","1",1);
	Sh3SetCv1kNative(1);
	if(SH3_CV1K_PCREL)Check(!cv1kV2Enabled && !cv1kV2Table,"PCREL excluded");
	setenv("FBNEO_CV1K_PCREL_DISPATCH","0",1);
	setenv("FBNEO_CV1K_FAST_RAM","1",1);
	Sh3SetCv1kNative(1);
	Check(!cv1kV2Enabled && !cv1kV2Table,"full region excluded");
	setenv("FBNEO_CV1K_FAST_RAM","0",1);
	setenv("FBNEO_CV1K_JIT","3",1);
	Sh3SetCv1kNative(1);
	Check(!cv1kV2Enabled && cv1kNativeEnabled,"V1 exclusive");
	setenv("FBNEO_CV1K_JIT","0",1);
	Sh3SetCv1kNative(0);
	Check(!cv1kV2Enabled && !cv1kV2Table && !cv1kV2Code.Used(),"non-CV1000 isolation/reset");
	puts("V2 opt-in, dependencies, conflicting modes and reset PASS");
}
static void TestBudgetAndEdges(){
	Init(true);Loop(20,0);
	for(unsigned i=0;i<12;i++)Sh3Run_timerhack(2049);
	Check(cv1kV2Stats.ops>0,"warm block");
	UINT64 before=cv1kV2Stats.hits;
	m_pc=0x80000000;m_delay=0;m_test_irq=0;m_sh4_icount=11;
	Check(!Cv1kV2Try(m_pc) && cv1kV2Stats.hits==before,"short budget");
	m_sh4_icount=100;m_delay=2;
	Check(!Cv1kV2Try(m_pc),"pending delay");
	m_delay=0;m_test_irq=1;
	Check(!Cv1kV2Try(m_pc),"pending IRQ");
	m_test_irq=0;m_pc=0x8000fff0;
	Check(!Cv1kV2Try(m_pc),"page tail");
	m_pc=0x80000001;
	Check(!Cv1kV2Try(m_pc),"odd PC");
	Check(!Cv1kV2Candidate(0x4011),"full literal hook retained");
	Sh3Exit();
	Check(!cv1kV2Table && !cv1kV2Code.Used(),"exit releases V2 cache");
	puts("V2 budget, IRQ, delay, page edge, literal hook and exit PASS");
}
// Exercise the formerly catastrophic workload: no eligible blocks and
// a mixture of short blocks with a small number of compilable loops.
static void TestFrontend(){
	Init(true);
	m_sh4_icount=1000;sh3_end_run=0;
	const UINT32 base=0x80000000;
	unsigned positions[5]={};
	unsigned found=1;
	for(unsigned pos=64;pos<32768-64 && found<5;pos++)
		if(Cv1kV2Hash(base+pos*2)==Cv1kV2Hash(base))positions[found++]=pos;
	Check(found==5,"collision fixture");
	for(unsigned i=0;i<5;i++){
		const unsigned pos=positions[i];
		for(unsigned j=0;j<20;j++)code[pos+j]=0x7001;
		code[pos+20]=0xa000;code[pos+21]=0x0009;
	}
	for(unsigned i=0;i<4;i++){
		m_pc=base+positions[i]*2;
		for(unsigned j=0;j<3;j++)Cv1kV2Observe(m_pc);
		Check(Cv1kV2Find(m_pc)!=NULL,"four-way promotion");
	}
	const auto bytes=cv1kV2Code.Used();
	Check(cv1kV2Stats.built==4,"four distinct blocks");
	m_pc=base+positions[4]*2;
	for(unsigned j=0;j<10000;j++)Cv1kV2Observe(m_pc);
	Check(cv1kV2Stats.setFull==1 && cv1kV2Stats.rejected==1 &&
		cv1kV2Stats.negative>=9997,"saturated set rejected once");
	Check(cv1kV2Code.Used()==bytes && cv1kV2Stats.built==4,"collision no code leak");
	for(unsigned i=0;i<4;i++){
		m_pc=base+positions[i]*2;m_sh4_icount=1000;
		Check(Cv1kV2Try(m_pc)==20,"all four compiled blocks preserved");
	}
	// A full cache must not sample/allocate/evict on a miss.
	cv1kV2Full=true;
	const auto clock=cv1kV2SampleClock;
	const auto probes=cv1kV2Stats.probes;
	m_pc=base+positions[4]*2;
	for(unsigned j=0;j<10000;j++){
		Check(!Cv1kV2Try(m_pc,true),"full cache miss");
		Cv1kV2SliceSample();
	}
	Check(cv1kV2Stats.probes==probes && cv1kV2SampleClock==clock,"full cache no miss writes");
	m_pc=base;m_sh4_icount=1000;
	Check(Cv1kV2Try(m_pc)==20,"full cache preserves hit");
	code[7]=0xe77f;m_pc=base;m_sh4_icount=1000;
	Check(!Cv1kV2Try(m_pc) && !Cv1kV2Find(base) &&
		cv1kV2Stats.retired==1,"full cache still validates live code");
	for(unsigned i=1;i<4;i++){
		m_pc=base+positions[i]*2;m_sh4_icount=1000;
		Check(Cv1kV2Try(m_pc)==20,"packed set preserves surviving ids");
	}

	Init(true);Loop(2,0);
	m_sh4_icount=1000;m_pc=base;sh3_end_run=0;
	for(unsigned j=0;j<10000;j++)Cv1kV2Observe(m_pc);
	Check(cv1kV2Stats.built==0 && cv1kV2Stats.rejected==1 &&
		cv1kV2Stats.negative==9997,"negative site no repeated scan");
	const UINT32 key=m_pc>>1;
	auto &learn=cv1kV2Learn[(key^(key>>10)^(key>>20))&(Cv1kV2LearnEntries-1)];
	const Cv1kV2Learn saved=learn;
	Cv1kV2Observe(m_pc);
	Check(!memcmp(&learn,&saved,sizeof(saved)),"negative lookup read-only");
	// Remap epoch change permits a previously rejected site to be learned.
	for(unsigned j=0;j<20;j++)replacement[j]=0x7001;
	replacement[20]=0xa000;replacement[21]=0x0009;
	Remap();
	for(unsigned j=0;j<3;j++)Cv1kV2Observe(m_pc);
	Check(Cv1kV2Find(m_pc)!=NULL,"negative remap re-learn");

	Init(true);Loop(20,0);
	m_sh4_icount=1000;sh3_end_run=0;
	for(unsigned j=0;j<3;j++)Cv1kV2Observe(m_pc);
	Check(Cv1kV2Find(m_pc)!=NULL,"lookup-only fixture");
	cv1kV2LookupOnly=true;
	const auto a=Snap(0);
	Check(!Cv1kV2Try(m_pc) && cv1kV2Stats.skipped==1 &&
		!cv1kV2Stats.hits && !cv1kV2Stats.ops,"lookup-only suppresses execution");
	const auto b=Snap(0);Compare(a,b,99,0);
	const auto resetCount=cv1kV2Stats.resets;
	Sh3Scan(ACB_WRITE);
	Check(!Cv1kV2Find(base) && !cv1kV2Code.Used() &&
		cv1kV2Stats.resets==resetCount+1 && cv1kV2Enabled,"state restore clears cache");
	cv1kV2LookupOnly=false;m_pc=base;m_sh4_icount=1000;
	for(unsigned j=0;j<3;j++)Cv1kV2Observe(m_pc);
	Check(Cv1kV2Find(base)!=NULL,"recompile after restore");
	Sh3Reset();
	Check(!Cv1kV2Find(base) && !cv1kV2Code.Used() && cv1kV2Enabled,"CPU reset clears cache");
	puts("V2 frontend: four-way pinning, collision/full, negative, remap and lookup-only PASS");
}
// Check both emitter selections and the bounded sampling/page filter.
static void TestCompactPolicy(){
	Init(true);Loop(20,0);
	m_sh4_icount=1000;sh3_end_run=0;
	const UINT32 base=m_pc;
	// Explicitly select the new emitter for this fixture, even when
	// the remaining suite is run with the legacy emitter override.
	cv1kV2Compact=true;
	for(unsigned i=0;i<3;i++)Cv1kV2Observe(base);
	auto *slot=Cv1kV2Find(base);
	Check(slot && cv1kV2Table[slot->id-1].kind!=0,"compact published");
	Check(cv1kV2PageTags[(base&AM)>>SH3_SHIFT]!=0,"page filter publication");
	Check(!Cv1kV2Find(base+0x10000),"unregistered page filter");
	const auto before=cv1kV2Stats.samples;
	m_pc=base+0x10000;m_sh4_icount=1000;
	Cv1kV2SliceSample();
	m_sh4_icount=100;
	for(unsigned i=0;i<10000;i++)Cv1kV2Try(m_pc,true);
	Check(cv1kV2Stats.samples<=before+2,"bounded per-slice samples");
	// A 14-register span cannot fit the 13 compact bindings and must
	// be emitted through the existing V1 fallback, with no guest change.
	Init(true);cv1kV2Compact=true;m_sh4_icount=1000;sh3_end_run=0;
	for(unsigned i=0;i<14;i++)code[i]=0x7001|(i<<8);
	code[14]=0xa000;code[15]=0x0009;
	for(unsigned i=0;i<3;i++)Cv1kV2Observe(m_pc);
	slot=Cv1kV2Find(m_pc);
	Check(slot && !cv1kV2Table[slot->id-1].kind && cv1kV2Stats.legacyBuilt==1,"wide footprint fallback");
	Check(Cv1kV2Try(m_pc)==14,"fallback executed");
	for(unsigned i=0;i<14;i++)Check(m_r[i]==1,"fallback state");
	puts("V2 compact selection, page filter, bounded sampling and wide fallback PASS");
}
static double Bench(unsigned mode,unsigned kind,Snapshot &result){
	setenv("FBNEO_CV1K_V2_COMPACT",mode==2?"1":"0",1);
	Init(mode!=0);
	Loop(kind==0?40:kind==1?32:2,kind==1?1:0);
	for(unsigned j=0;j<12;j++)Sh3Run_timerhack(2049);
	auto t=std::chrono::steady_clock::now();
	for(unsigned i=0;i<200;i++)Sh3Run_timerhack(25000);
	double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
	printf("V2 bench %u %u %.3f ms state %08x/%08x/%08x/%08x cycles %d ops %llu calls %llu\n",
		kind,mode,ms,m_r[0],m_r[2],m_r[3],m_r[10],sh3_total_cycles,
		(unsigned long long)cv1kV2Stats.ops,(unsigned long long)cv1kV2Stats.hits);
	result=Snap(0);return ms;
}
#ifndef SH3_V2_B2_TEST
int main(int argc,char **){
	setvbuf(stdout,NULL,_IOLBF,0);
	// Preserve the B-1 fixtures and benchmark as the narrow-scope
	// regression control. The B-2 suite exercises expanded blocks.
	setenv("FBNEO_CV1K_V2_BLOCKS","0",1);
	if(argc>1){
		for(unsigned kind=0;kind<3;kind++){
			double ms[3][3]={};
			Snapshot states[3];
			for(unsigned i=0;i<3;i++){
				for(unsigned j=0;j<3;j++){
					const unsigned mode=(i+j)%3;
					ms[mode][i]=Bench(mode,kind,states[mode]);
				}
				Compare(states[0],states[1],kind,i);Compare(states[0],states[2],kind,i);
			}
			for(auto &a:ms)std::sort(a,a+3);
			printf("V2 median %u control %.3f legacy %.3f compact %.3f ms\n",kind,ms[0][1],ms[1][1],ms[2][1]);
		}
		return 0;
	}
	CacheTest();TestPolicy();
	for(unsigned i=0;i<7;i++){
		const auto a=Scenarios(false,i),b=Scenarios(true,i);
		for(unsigned j=0;j<a.size();j++)Compare(a[j],b[j],i,j);
	}
	puts("V2 live interpreter differential: 7 x 24 slices PASS");
	TestBudgetAndEdges();TestFrontend();TestCompactPolicy();return 0;
}
#endif
