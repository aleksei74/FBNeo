// C4: actual generated global B/BR entries, Tier2 and short exits.
#define SH3_V2_C4_TEST 1
#include "test_sh3_v2_c3.cpp"

static void InitC4(unsigned mode,bool frame=false){
	setenv("FBNEO_CV1K_V2_GLOBAL",mode>=4?"1":"0",1);
	setenv("FBNEO_CV1K_V2_REPEAT_ONLY",mode==6?"1":"0",1);
	setenv("FBNEO_CV1K_V2_FRAME_FENCE",frame?"1":"0",1);
	setenv("FBNEO_CV1K_V2_GLOBAL_MIN_OPS",mode==5?"20":mode==7?"8":"0",1);
	if(mode==6){
		InitC2(1);
		Check(cv1kV2RepeatOnly && !cv1kC4Enabled && !cv1kC3Enabled,"repeat-only policy");
	}else{
		InitC3(mode>=4?3:mode);
		Check(cv1kC4Enabled==(mode>=4),"global policy");
	}
}
static void Tier2Encodings(){
	InitC4(4);
	Sh3MapMemory((UINT8*)data,0,0xffff,MAP_READ|MAP_WRITE);
	const UINT32 pc=0x80000000;
	Sh3V2::CodeCache cache;
	uint64_t generation=0;
	Sh3V2::GlobalStamp stamp={1,0,&generation};
	unsigned tested=0;
	for(unsigned raw=0;raw<65536;raw++){
		const UINT16 op=raw;
		if(!Sh3V2::Tier2Candidate(op))continue;
		// Exercise every encoding except large disp/immediate families,
		// which use representative boundary and middle displacements.
		const unsigned top=op>>12,hi=op>>8,n=(op>>8)&15,m=(op>>4)&15,f=op&15;
		if(top==8 && f!=0 && f!=7 && f!=15)continue;
		if(top==12 && (op&255)!=0 && (op&255)!=127 && (op&255)!=255)continue;
		if(!(tested%128))cache.Reset();
		UINT32 words[Sh3V2::CompactEmitter::ResidentMaxWords],offset;
		Sh3V2::GlobalBuild g={};g.stamp=&stamp;g.trusted=true;
		Sh3V2::CompactEmitter emit(words);unsigned fix=0;
		Check(emit.BuildResident(&op,1,pc,1,NULL,0,NULL,fix,false,0,&g),"tier2 emit");
		Check(cache.Publish(words,emit.End()-words,offset),"tier2 publish");
		for(unsigned trial=0;trial<3;trial++){
			UINT32 r[16];for(unsigned i=0;i<16;i++)r[i]=RandB2();
			UINT32 gbr=0x8c000200,pr=RandB2(),macl=RandB2(),sr=RandB2(),ea=RandB2();
			if(Sh3V2::Tier2Memory(op)){
				for(unsigned i=0;i<16;i++)r[i]=0x8c000100+i*16;
				r[0]=0x20;
			}
			if(top==4&&(f==12||f==13)){
				const UINT32 shifts[]={0,0xffffffe0,0xfffffff9};
				r[m]=shifts[trial];
			}
			// Deterministic initial memory; compare the entire mapped data.
			for(unsigned i=0;i<32770;i++)data[i]=UINT16((i*617)^op^trial);
			std::vector<UINT16> before(data,data+32770);
			memcpy(m_r,r,sizeof(r));m_sr=sr;m_ea=ea;m_pr=pr;m_macl=macl;m_gbr=gbr;
			m_pc=m_ppc=pc+2;m_sh4_icount=100;sh3_total_cycles=0;
			execute_one(op);
			UINT32 expected[16];memcpy(expected,m_r,sizeof(expected));
			UINT32 expSR=m_sr,expEA=m_ea,expPR=m_pr,expMACL=m_macl;
			std::vector<UINT16> mem(data,data+32770);
			const unsigned cost=1+100-m_sh4_icount;
			memcpy(data,before.data(),sizeof(data));
			Sh3V2::ResidentContext c={};
			c.memory={MemMapR,MemMapRL,MemMapW,NULL,0,1,pc,NULL,0,pr,cv1kV2WriteWatch,cv1kV2Generations};
			c.remaining=100;c.ppc=pc;c.token=1;c.dynamicMap=cv1kC4Map;c.gbr=gbr;c.macl=macl;
			fbneo_sh3_v3_enter((const UINT8*)cache.ExecBase()+offset,r,&sr,&ea,&c);
			if(c.ops!=1 || c.remaining!=100-cost || memcmp(expected,r,sizeof(r)) ||
				sr!=expSR || ea!=expEA || c.memory.pr!=expPR || c.macl!=expMACL ||
				memcmp(data,mem.data(),sizeof(data))){
				fprintf(stderr,"tier2 mismatch %04x trial %u ops %u cost %u/%u EA %08x/%08x PR %08x/%08x MACL %08x/%08x\n",
					op,trial,c.ops,100-c.remaining,cost,ea,expEA,c.memory.pr,expPR,c.macl,expMACL);
				for(unsigned i=0;i<16;i++)if(expected[i]!=r[i])fprintf(stderr,"R%u %08x/%08x\n",i,r[i],expected[i]);
				exit(1);
			}
		}
		++tested;
	}
	printf("C4 Tier2 encodings: %u x 3 differential PASS\n",tested);
}
static void RealisticFixture(unsigned kind,unsigned mode){
	const UINT32 base=0x80000000;
	if(kind<7){Fixture(kind==6?0:kind,mode!=0);return;}
	if(kind==7||kind==8){
		// Non-leaf function with PR + R8 stack save/restore, nested
		// BSR or JSR, and memory access in the RTS delay slot.
		code[0]=0x7701;BranchAt(1,16,0xb000);
		code[3]=0x7401;BranchAt(4,0);
		code[16]=0x2f86;code[17]=0x4f22;
		if(kind==7)BranchAt(18,32,0xb000);
		else{code[18]=0x460b;code[19]=0x0009;}
		code[20]=0x4f26;code[21]=0x000b;code[22]=0x68f6;
		code[32]=0x031e; // mov.l @(R0,R1),R3
		code[33]=0x2132; // mov.l R3,@R1
		code[34]=0x0327; // mul.l R2,R3
		code[35]=0x031a; // sts MACL,R3
		code[36]=0x000b;code[37]=0x7301;
		code[48]=0x7305;code[49]=0x000b;code[50]=0x0009;
		m_r[15]=0x8c00f000;m_r[1]=0x8c000100;m_r[6]=base+64;m_r[0]=0x20;
		m_gbr=0x8c000200;
		if(mode)for(unsigned i:{0u,3u,16u,20u,32u,48u}){
			// C3 cannot promote an unsupported first opcode. C4 can.
			if(mode==4 || i==0||i==3||i==48)PromoteAt(base+i*2);
		}
	}else if(kind==9){
		// Realistic short-chain exit: 3 small blocks, unsupported
		// CLRMAC, then a return edge. Same guest work for all modes.
		code[0]=0x7001;code[1]=0x7213;BranchAt(2,16);
		code[16]=0x6312;code[17]=0x7301;BranchAt(18,32);
		code[32]=0x2132;code[33]=0x7401;BranchAt(34,48);
		code[48]=0x0028;BranchAt(49,0);
		m_r[1]=0x8c000100;
		if(mode)for(unsigned i:{0u,16u,32u})PromoteAt(base+i*2);
	}else if(kind==10){
		// 600 distinct hot bodies, no 512-region cap. Last edge is
		// still in BRA's signed 12-bit displacement range.
		for(unsigned i=0;i<600;i++){
			code[i*3]=0x7001;
			BranchAt(i*3+1,((i+1)%600)*3);
		}
		if(mode==4)for(unsigned i=0;i<600;i++)PromoteAt(base+i*6);
	}
	m_pc=base;m_sh4_icount=0;
}
static std::vector<Snapshot> GlobalCase(unsigned kind,unsigned mode){
	InitC4(mode);RealisticFixture(kind,mode);
	if(kind==6){
		m_timer[0].timer_prescaler=1;m_timer[0].timer_exec=TimerTest;
		m_timer[0].start(1100,0,1,0);
	}
	std::vector<Snapshot> result;
	for(unsigned i=0;i<32;i++){
		if(kind==8 && i==16)m_r[6]=0x80000060;
		result.push_back(Snap(Sh3Run_timerhack(i==0?4001:1+(i*37)%231)));
	}
	if(mode==4){
		Check(cv1kV2Stats.globalBuilt && cv1kV2Stats.globalOps,"global code executed");
		if(kind==10)Check(cv1kC4Count>=600,"over 512 pinned global bodies");
		printf("C4 case %u groups %llu blocks %llu ops %llu builds %llu cold %llu guard %llu fence %llu\n",kind,
			(unsigned long long)cv1kV2Stats.globalGroups,(unsigned long long)cv1kV2Stats.globalBlocks,
			(unsigned long long)cv1kV2Stats.globalOps,(unsigned long long)cv1kV2Stats.globalBuilt,
			(unsigned long long)cv1kV2Stats.globalCold,(unsigned long long)cv1kV2Stats.globalGuard,
			(unsigned long long)cv1kV2Stats.globalFence);
	}
	return result;
}
static void GlobalSMC(){
	InitC4(4);
	const UINT32 base=0x80000000,target=base+32;
	Sh3MapMemory((UINT8*)code,0xc800000,0xc80ffff,MAP_WRITE);
	code[0]=0x7001;code[1]=0x2131;BranchAt(2,16);
	code[16]=0x7213;BranchAt(17,0);
	m_r[1]=0x8c800020;m_r[3]=0xe77f;
	PromoteAt(base);PromoteAt(target);
	auto *d=Cv1kC4Find(base),*t=Cv1kC4Find(target);
	Check(d&&t&&d->edge[0].linked,"global direct incoming B");
	m_pc=base;m_sh4_icount=100;
	Check(Cv1kV2Try(base,false,true)==1 && m_pc==base+2 &&
		code[16]==0x7213 && m_sh4_icount==99,"global watched store exits before write");
	WW(0x8c800020,0xe77f);
	m_pc=target;m_sh4_icount=100;
	Check(!Cv1kV2Try(target,false,true) && !t->active && !d->edge[0].linked,
		"global invalidation and unlink");
	Sh3Scan(ACB_WRITE);
	Check(!cv1kC4Count&&!cv1kV2Code.Used(),"global restore clears addresses");
	puts("C4 mirrored SMC, direct unlink, restore PASS");
}
static void FrameFence(){
	InitC4(4,true);
	std::vector<UINT16> ram(0x800000/2,0x0009);
	Sh3MapMemory((UINT8*)ram.data(),0xc000000,0xc7fffff,MAP_RAM);
	Sh3MapMemory((UINT8*)ram.data(),0xc800000,0xcffffff,MAP_RAM);
	Sh3SetCv1kTrackedIO();Sh3SetCv1kFastMemory((UINT8*)ram.data(),0x800000,NULL);
	const UINT32 base=0x8c000000,target=base+32;
	ram[0]=0x7001;ram[1]=0xa00d;ram[2]=0x0009;
	ram[16]=0x7213;ram[17]=0xafed;ram[18]=0x0009;
	PromoteAt(base);PromoteAt(target);
	Sh3Cv1kFrameFence(true);
	m_pc=base;m_sh4_icount=1000;Check(Cv1kV2Try(base,false,true)>3,"frame group");
	const uint64_t token=cv1kC4Token,checks=cv1kV2Stats.globalChecks;
	++cv1kV2SliceEpoch;
	m_pc=base;m_sh4_icount=1000;Check(Cv1kV2Try(base,false,true)>3 &&
		cv1kC4Token==token && cv1kV2Stats.globalChecks==checks,"scoped frame fence skips slice check");
	WW(0x8c800020,0xe77f);
	m_pc=target;m_sh4_icount=100;
	Check(!Cv1kV2Try(target,false,true),"tracked write invalidates inside scoped frame");
	WW(0x8c800020,0x7213);PromoteAt(target);
	Sh3Cv1kFrameFence(false);
	ram[16]=0xe77f;Sh3Cv1kFrameFence(true);
	m_pc=target;m_sh4_icount=100;
	Check(!Cv1kV2Try(target,false,true),"external raw mutation invalidated at frame scope");
	Sh3Cv1kFrameFence(false);Cv1kV2Reset();Sh3MapInit();
	puts("C4 scoped frame fence and tracked/external writes PASS");
}
static std::vector<Snapshot> TrustedShort(unsigned mode,bool bench,double *elapsed=NULL){
	static std::vector<UINT16> rom(0x400000/2);
	InitC4(mode,true);
	RealisticFixture(9,0);
	std::fill(rom.begin(),rom.end(),UINT16(0x0009));
	memcpy(rom.data(),code,sizeof(code));
	Sh3MapMemory((UINT8*)rom.data(),0,0x3fffff,MAP_ROM);
	Sh3SetCv1kTrackedIO();Sh3SetCv1kFastMemory(NULL,0,(UINT8*)rom.data());
	if(mode && mode!=6)for(unsigned i:{0u,16u,32u})PromoteAt(0x80000000u+i*2);
	m_pc=0x80000000u;m_sh4_icount=0;
	std::vector<Snapshot> result;
	const unsigned slices=bench?600:48;
	const auto before=std::chrono::steady_clock::now();
	for(unsigned i=0;i<slices;i++){
		if(!(i%240))Sh3Cv1kFrameFence(true);
		const int spent=Sh3Run_timerhack(bench?8333:1021+(i*37)%231);
		if(!bench)result.push_back(Snap(spent));
		if(i%240==239)Sh3Cv1kFrameFence(false);
	}
	Sh3Cv1kFrameFence(false);
	if(elapsed)*elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
	if(bench)result.push_back(Snap(0));
	if(mode==5)Check(cv1kV2Stats.globalCooldown && cv1kV2Stats.globalMasked,"adaptive gate actually used");
	if(mode==6)Check(!cv1kC4Count && !cv1kV2Stats.globalOps,"repeat-only no global bodies");
	printf("C4 short trusted mode %u groups %llu blocks %llu checks %llu cooldown %llu skipped %llu masked %llu\n",mode,
		(unsigned long long)cv1kV2Stats.globalGroups,(unsigned long long)cv1kV2Stats.globalBlocks,
		(unsigned long long)cv1kV2Stats.globalChecks,(unsigned long long)cv1kV2Stats.globalCooldown,
		(unsigned long long)cv1kV2Stats.globalSkipped,(unsigned long long)cv1kV2Stats.globalMasked);
	return result;
}
static void RepeatOnly(){
	InitC4(6);Loop(2,0);PromoteAt(0x80000000u);
	m_pc=0x80000000u;m_sh4_icount=1000;
	Check(Cv1kV2Try(m_pc,false,true)>4 && cv1kV2Stats.selfLaps,"repeat-only self loop active");
	code[64]=0x7001;BranchAt(65,80);
	m_pc=0x80000080u;m_sh4_icount=1000;
	for(unsigned i=0;i<Cv1kV2Hot;i++)Cv1kV2Observe(m_pc);
	Check(!Cv1kV2Find(m_pc),"repeat-only rejects ordinary blocks");
	puts("C4 repeat-only control PASS");
}
static double BenchC4(unsigned kind,unsigned mode,Snapshot &state){
	InitC4(mode);RealisticFixture(kind,mode);
	for(unsigned i=0;i<8;i++)Sh3Run_timerhack(2049);
	const auto before=std::chrono::steady_clock::now();
	for(unsigned i=0;i<200;i++)Sh3Run_timerhack(25000);
	const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
	state=Snap(0);
	printf("C4 bench %u mode %u %.3f ms groups %llu blocks %llu ops %llu\n",kind,mode,ms,
		(unsigned long long)cv1kV2Stats.globalGroups,
		(unsigned long long)cv1kV2Stats.globalBlocks,(unsigned long long)cv1kV2Stats.globalOps);
	return ms;
}
int main(int argc,char **argv){
	setvbuf(stdout,NULL,_IOLBF,0);
	if(argc>1 && !strcmp(argv[1],"short")){
		double ms[5][3];Snapshot states[5];const unsigned modes[]={0,3,4,5,7};
		for(unsigned i=0;i<3;i++){
			for(unsigned j=0;j<5;j++){
				unsigned n=(i+j)%5;
				states[n]=TrustedShort(modes[n],true,&ms[n][i])[0];
				printf("C4 short trusted bench mode %u %.3f ms\n",modes[n],ms[n][i]);
			}
			for(unsigned n=1;n<5;n++)Compare(states[0],states[n],190+n,i);
		}
		for(auto &a:ms)std::sort(a,a+3);
		printf("C4 short trusted median interpreter %.3f C3 %.3f raw C4 %.3f adaptive20 C4 %.3f default8 C4 %.3f ms\n",ms[0][1],ms[1][1],ms[2][1],ms[3][1],ms[4][1]);
		return 0;
	}
	if(argc>1){
		for(unsigned k:{0u,2u,3u,7u,8u,9u}){
			double ms[3][3];Snapshot states[3];
			const unsigned modes[]={0,3,4};
			for(unsigned i=0;i<3;i++){
				for(unsigned j=0;j<3;j++){
					unsigned n=(i+j)%3;
					ms[n][i]=BenchC4(k,modes[n],states[n]);
				}
				Compare(states[0],states[1],k,i);Compare(states[0],states[2],k,i);
			}
			for(auto &a:ms)std::sort(a,a+3);
			printf("C4 median %u interpreter %.3f C3 %.3f C4 %.3f ms\n",k,ms[0][1],ms[1][1],ms[2][1]);
		}
		return 0;
	}
	Tier2Encodings();GlobalSMC();FrameFence();RepeatOnly();
	const auto shortBase=TrustedShort(0,false);
	for(unsigned mode:{4u,5u,6u,7u}){
		const auto r=TrustedShort(mode,false);
		for(unsigned i=0;i<r.size();i++)Compare(shortBase[i],r[i],195+mode,i);
	}
	puts("C4 trusted short-chain controls: 192 differential slices PASS");
	for(unsigned k=0;k<=10;k++){
		const auto a=GlobalCase(k,0),b=GlobalCase(k,4);
		for(unsigned i=0;i<a.size();i++)Compare(a[i],b[i],170+k,i);
	}
	puts("C4 global execution: 352 differential slices PASS");
	return 0;
}
