// Stage B-2: independent literal/DT handler checks, exact branch
// publication and end-to-end slice comparisons on generated AArch64 code.
#define SH3_V2_B2_TEST 1
#include "test_sh3_v2.cpp"

static void InitB2(bool on){
	setenv("FBNEO_CV1K_V2_COMPACT","1",1);
	setenv("FBNEO_CV1K_V2_BLOCKS","1",1);
	Init(on);
	Check(!on||cv1kV2Blocks,"B2 policy");
}
static UINT32 randomState=0x971245b3u;
static UINT32 RandB2(){
	randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;
	return randomState;
}
static void LiteralEncodings(){
	InitB2(false);
	// DATA map differs from FETCH map; literals must read DATA, not
	// a compile-time constant or the validated instruction span.
	Sh3MapMemory((UINT8*)replacement,0,0x1ffff,MAP_READ);
	for(unsigned i=0;i<65536;i++)replacement[i]=UINT16(i*617u^0x9173u);
	Sh3V2::CodeCache cache;
	unsigned tested=0;
	for(unsigned op=0;op<65536;op++){
		if(!Sh3V2::PcRelative(op) && !Sh3V2::IsDT(op))continue;
		if(!(tested%256))cache.Reset();
		const UINT32 pcs[]={0x80000002u,0xa000fffcu,0x00000000u,0xc000fffeu};
		const UINT32 pc=pcs[(tested/2)%4];
		const UINT16 inst=UINT16(op);
		UINT32 words[Sh3Native::MaxWords],offset;
		Sh3V2::CompactEmitter emit(words);
		Check(emit.Build(&inst,1,pc,~0u,true),"B2 literal/DT build");
		Check(cache.Publish(words,emit.End()-words,offset),"B2 literal publish");
		for(unsigned pass=0;pass<2;pass++){
			UINT32 r[16],sr=RandB2(),ea=RandB2();
			for(auto &v:r)v=RandB2();
			if(Sh3V2::IsDT(op)){
				r[(op>>8)&15]=pass;
				replacement[((pc+2)&AM)/2]=0x0009;
			}else if((op>>12)==9||(op>>12)==13){
				const UINT32 a=((op>>12)==9?pc+4:(pc+4)&~3u)+
					(op&255)*((op>>12)==9?2:4);
				replacement[(a&AM)/2]=UINT16(RandB2());
				if((op>>12)==13)replacement[(a&AM)/2+1]=UINT16(RandB2());
			}
			memcpy(m_r,r,sizeof(r));m_sr=sr;m_ea=ea;m_pc=m_ppc=pc+2;
			m_sh4_icount=100;sh3_total_cycles=0;
			execute_one(inst);
			Check(m_sh4_icount==100 && !sh3_total_cycles,"B2 one-cycle handler");
			const Sh3Native::Memory memory={MemMapR,MemMapRL,MemMapW,NULL,0,0,pc,NULL,0,0};
			const unsigned done=cache.RunCompact(offset,r,&sr,&ea,memory,emit.Wide());
			if(done!=1 || memcmp(r,m_r,sizeof(r))||sr!=m_sr||ea!=m_ea){
				fprintf(stderr,"B2 literal/DT mismatch op=%04x pc=%08x done=%u sr=%08x/%08x ea=%08x/%08x\n",
					op,pc,done,sr,m_sr,ea,m_ea);exit(1);
			}
			++tested;
		}
	}
	printf("B2 live literal/MOVA/DT encoding cases: %u PASS\n",tested);
}
static void Promote(){
	for(unsigned i=0;i<Cv1kV2Hot;i++)Cv1kV2Observe(m_pc);
	Check(Cv1kV2Find(m_pc)!=NULL,"B2 compiled entry");
}
static Snapshot BranchCase(bool on,UINT16 op,UINT16 slot,unsigned body,unsigned t,bool odd,bool shortBudget){
	InitB2(on);
	const UINT32 pc=0x80000100;
	const unsigned index=(pc&AM)/2;
	m_pc=pc;m_sr=t?T:0;m_ea=0x12345678;m_pr=0x80000400;
	m_r[3]=0x80000300+(odd?1:0)-unsigned(body>3);
	// A wide fixture exercises both the trampoline and leaf exits.
	for(unsigned i=0;i<body;i++)code[index+i]=UINT16(0x7001|((i%11)<<8));
	// A dynamic target changed by the delay slot must be captured
	// before that slot, not reconstructed from the modified GPR.
	code[index+body]=op;
	if(Sh3V2::Delayed(op))code[index+body+1]=slot;
	const bool taken=!Sh3V2::Conditional(op)||bool(t)!=bool(op&0x0200);
	const bool guarded=odd && Sh3V2::Dynamic(op) && op!=0x000b;
	const unsigned count=body+1+unsigned(taken&&Sh3V2::Delayed(op));
	const unsigned expected=guarded?body:count;
	const unsigned spent=guarded?body:count+(taken?Sh3V2::ExtraCycles(op):0);
	const unsigned maxCost=body+1+unsigned(Sh3V2::Delayed(op))+Sh3V2::ExtraCycles(op);
	if(on){
		m_sh4_icount=1000;sh3_end_run=0;Promote();
		const auto *entry=Cv1kV2Find(pc);
		Check(cv1kV2Table[entry->id-1].body==body,"terminal compiled");
		if(shortBudget){
			m_sh4_icount=maxCost-1;
			const auto before=Snap(0);
			Check(!Cv1kV2Try(pc),"terminal maximum budget guard");
			Compare(before,Snap(0),98,0);
			return Snap(0);
		}
		const unsigned done=Cv1kV2Try(pc);
		Check(done==expected && m_sh4_icount==1000-int(spent) &&
			sh3_total_cycles==int(spent),"terminal instruction/cycle accounting");
		Check(cv1kV2Stats.terminalCalls==unsigned(!guarded),"terminal exit marker");
		Check(cv1kV2Stats.guards==unsigned(guarded),"odd target guard");
		sh4_run_timers(spent);m_sh4_icount=0;
		return Snap(spent);
	}
	return Snap(Sh3Run_timerhack(spent));
}
static void Branches(){
	const UINT16 ops[]={0x8905,0x8b05,0x8d05,0x8f05,0xa010,0xb010,0x432b,0x430b,0x000b,
		0x89f0,0x8bf0,0x8df0,0x8ff0,0xaff0,0xbff0};
	const UINT16 slots[]={0x0009,0x7302,0x0008,0x0018,0x6433,0x633e,0xe380,0x4330 /* unsupported */};
	unsigned tested=0;
	for(auto op:ops)for(unsigned t=0;t<2;t++)for(unsigned body:{0u,1u,4u,11u}){
		if(!body && !Sh3V2::Delayed(op))continue;
		for(unsigned i=0;i<7;i++){
			const UINT16 slot=slots[i];
			const auto a=BranchCase(false,op,slot,body,t,false,false),
				b=BranchCase(true,op,slot,body,t,false,false);
			Compare(a,b,op,body*16+i*2+t);++tested;
		}
		if(body && Sh3V2::Dynamic(op)&&op!=0x000b){
			const auto a=BranchCase(false,op,0x7302,body,t,true,false),
				b=BranchCase(true,op,0x7302,body,t,true,false);
			Compare(a,b,op,88);++tested;
		}
		if(t==0)BranchCase(true,op,0x0009,body,t,false,true);
	}
	InitB2(true);code[0]=0x432b;code[1]=0x7302;
	m_r[3]=0x80000301;m_sh4_icount=1000;sh3_end_run=0;Promote();
	const auto before=Snap(0);
	Check(!Cv1kV2Try(m_pc)&&cv1kV2Stats.guards==1,"zero-prefix odd branch guard");
	Compare(before,Snap(0),97,0);
	printf("B2 terminal/slot differential cases: %u PASS\n",tested);
}
static void DTGuards(){
	for(unsigned mode=0;mode<3;mode++){
		InitB2(true);
		code[0]=0x7001;code[1]=0x4210;code[2]=0x8902;
		memcpy(replacement,code,sizeof(code));
		if(mode==0)replacement[2]=0x8bfd; // only DATA sees busy pattern
		Sh3MapMemory((UINT8*)replacement,0,0x1ffff,MAP_READ);
		if(mode==1)Sh3MapHandler(1,0,0xffff,MAP_READ);
		m_r[2]=100;m_sh4_icount=1000;sh3_end_run=0;Promote();
		const unsigned done=Cv1kV2Try(m_pc);
		if(mode<2)Check(done==1 && m_r[2]==100 && m_pc==0x80000002 &&
			m_sh4_icount==999 && cv1kV2Stats.guards==1 && hits==0,"DT busy/MMIO guard before decrement");
		else Check(done==3 && m_r[2]==99 && !(m_sr&T),"DT nonbusy DATA map");
	}
	InitB2(true);m_sh4_icount=1000;
	// No terminal folding over a PC-relative delay slot, busy self
	// branch or the existing data-kernel probe edges.
	const UINT16 a[]={0x7001,0xa004,0xd100},b[]={0x7001,0xaffe,0x0009},
		c[]={0x7001,0x8bfa},d[]={0x7001,0x8ffb,0x7101};
	cv1kDataLoopsEnabled=true;
	Check(!Cv1kV2Terminal(a,1,3,m_pc)&&!Cv1kV2Terminal(b,1,3,m_pc)&&
		!Cv1kV2Terminal(c,1,2,m_pc)&&!Cv1kV2Terminal(d,1,3,m_pc),"special edges/slots preserved");
	puts("B2 DT data-map guards and special branch exclusions PASS");
}
static INT32 idleBurnB2=11;
static constexpr UINT32 idleAddrB2=0x0c002310u;
static UINT32 __fastcall IdleB2(UINT32 a){
	if(a==idleAddrB2)Sh3BurnCycles(idleBurnB2);
	UINT32 v;memcpy(&v,(UINT8*)data+(a&0xfffc),4);
	return (v<<16)|(v>>16);
}
static void IdleSetup(){
	Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);
	Sh3SetReadLongHandler(1,IdleB2);
	Sh3SetCv1kIdleRead((UINT8*)data,idleAddrB2,0,&idleBurnB2,IdleB2);
	cv1kIdleBatchEnabled=true;
	data[0x230c/2]=0x1234;data[0x230e/2]=0x9876;
	data[0x2310/2]=0x5678;data[0x2312/2]=0xabcd;
}
static Snapshot IdleCase(bool on,unsigned which){
	InitB2(on);IdleSetup();
	code[0]=0x7001;code[1]=0x6312;code[2]=0xa004;code[3]=0x0009;
	m_r[1]=(which==0?0x8c00230cu:which==1?0xac002310u:0x8c802310u);
	if(which==2)Sh3MapMemory((UINT8*)data,0xc800000,0xc80ffff,MAP_RAM);
	if(which>=3){
		// PC-relative long literal in the first RAM code page.
		m_pc=0x8c0022fcu;
		data[0x22fc/2]=0x7001;data[0x22fe/2]=(which==4?0xd304:0xd303);
		data[0x2300/2]=0xa004;data[0x2302/2]=0x0009;
	}
	if(which==5){Sh3SetReadLongHandler(1,ReadTest);scenario=0;}
	const unsigned budget=5;
	if(on){m_sh4_icount=1000;sh3_end_run=0;Promote();}
	const auto result=Snap(Sh3Run_timerhack(budget));
	if(on){
		Check(cv1kV2Stats.hits>0,"idle compiled entry reached");
		const bool reject=which==1||which==4||which==5;
		Check(cv1kV2Stats.guards==unsigned(reject),"idle exact/replacement guard");
		Check(cv1kV2Stats.terminalCalls==unsigned(!reject),"idle rescue terminal");
	}
	return result;
}
static void Idle(){
	for(unsigned i=0;i<6;i++){
		const auto a=IdleCase(false,i),b=IdleCase(true,i);
		Compare(a,b,99,i);
	}
	puts("B2 idle neighbor/exact/mirror/literal/replacement PASS");
}
static std::vector<Snapshot> BasicLoop(bool on,bool kernels){
	InitB2(on);
	cv1kDataLoopsEnabled=kernels;cv1kDataFullEnabled=kernels;cv1kPollEnabled=kernels;
	const UINT16 ops[]={0xd10f,0x6312,0x7301,0x2132,0x4210,0x8bf9,0xe220,0xaff7,0x0009};
	memcpy(code,ops,sizeof(ops));code[32]=0x8c00;code[33]=0x0100;
	m_r[2]=32;data[0x100/2]=0x1234;data[0x102/2]=0xabcd;
	std::vector<Snapshot> states;
	for(unsigned i=0;i<32;i++){
		// Change the literal pool without changing the compiled span.
		if(i==18)code[33]=0x0200;
		const int budget=3073+(i%9);
		states.push_back(Snap(Sh3Run_timerhack(budget)));
	}
	if(on){
		Check(cv1kV2Stats.terminalCalls>0 && cv1kV2Stats.extendedBuilt>0 &&
			cv1kV2Stats.cycles>=cv1kV2Stats.ops,"native B2 loop actually executed");
		printf("B2 loop: terminal exits %llu ops %llu cycles %llu built %llu guards %llu\n",
			(unsigned long long)cv1kV2Stats.terminalCalls,(unsigned long long)cv1kV2Stats.ops,
			(unsigned long long)cv1kV2Stats.cycles,(unsigned long long)cv1kV2Stats.built,
			(unsigned long long)cv1kV2Stats.guards);
	}
	return states;
}
static double BenchB2(unsigned mode,unsigned kind,Snapshot &state){
	InitB2(mode!=0);cv1kV2Blocks=mode==2;
	if(kind==0){
		const UINT16 ops[]={0xd10f,0x6312,0x7301,0x2132,0x4210,0x8bf9,0xe220,0xaff7,0x0009};
		memcpy(code,ops,sizeof(ops));code[32]=0x8c00;code[33]=0x0100;m_r[2]=32;
	}else Loop(2,0);
	for(unsigned i=0;i<12;i++)Sh3Run_timerhack(2049);
	const auto before=std::chrono::steady_clock::now();
	for(unsigned i=0;i<200;i++)Sh3Run_timerhack(25000);
	const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
	state=Snap(0);
	printf("B2 bench %u mode %u %.3f ms native ops %llu cycles %llu terminal %llu built %llu\n",
		kind,mode,ms,(unsigned long long)cv1kV2Stats.ops,(unsigned long long)cv1kV2Stats.cycles,
		(unsigned long long)cv1kV2Stats.terminalCalls,(unsigned long long)cv1kV2Stats.built);
	return ms;
}
static void BenchmarkB2(){
	for(unsigned kind=0;kind<2;kind++){
		double ms[3][3];Snapshot states[3];
		for(unsigned i=0;i<3;i++){
			for(unsigned j=0;j<3;j++){
				const unsigned mode=(i+j)%3;
				ms[mode][i]=BenchB2(mode,kind,states[mode]);
			}
			Compare(states[0],states[1],kind,i);Compare(states[0],states[2],kind,i);
		}
		for(auto &a:ms)std::sort(a,a+3);
		printf("B2 median %u control %.3f B1 %.3f B2 %.3f ms\n",kind,ms[0][1],ms[1][1],ms[2][1]);
	}
}
#ifndef SH3_V2_C1_TEST
int main(int argc,char **){
	setvbuf(stdout,NULL,_IOLBF,0);
	if(argc>1){BenchmarkB2();return 0;}
	LiteralEncodings();Branches();DTGuards();Idle();
	for(unsigned k=0;k<2;k++){
		const auto a=BasicLoop(false,k),b=BasicLoop(true,k);
		for(unsigned i=0;i<a.size();i++)Compare(a[i],b[i],100+k,i);
	}
	for(unsigned i=0;i<7;i++){
		const auto a=Scenarios(false,i),b=Scenarios(true,i);
		for(unsigned j=0;j<a.size();j++)Compare(a[j],b[j],i,j);
	}
	puts("B2 integrated slices: 64 basic loop + 168 existing scenarios PASS");
	return 0;
}
#endif
