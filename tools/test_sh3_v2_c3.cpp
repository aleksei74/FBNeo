// C3 resident region: exercise the actual emitted B/ADR/RAS paths.
#define SH3_V2_C3_TEST 1
#include "test_sh3_v2_c2.cpp"

static void InitC3(unsigned mode){
#ifndef SH3_V2_C4_TEST
	setenv("FBNEO_CV1K_V2_GLOBAL","0",1);
#endif
	setenv("FBNEO_CV1K_V2_RESIDENT",mode==3?"1":"0",1);
	InitC2(mode);
	Check(cv1kC3Enabled==(mode==3),"C3 opt-in policy");
}
static std::vector<Snapshot> ResidentCase(unsigned kind,unsigned mode){
	InitC3(mode);Fixture(kind==6?0:kind,mode);
	if(kind==6){
		m_timer[0].timer_prescaler=1;m_timer[0].timer_exec=TimerTest;
		m_timer[0].start(1100,0,1,0);
	}
	std::vector<Snapshot> result;
	for(unsigned i=0;i<32;i++){
		const unsigned budget=i==0?1601:1+(i*37)%231;
		result.push_back(Snap(Sh3Run_timerhack(budget)));
	}
	if(mode==3){
		Check(cv1kV2Stats.residentBuilt && cv1kV2Stats.residentOps,"resident native code executed");
		if(kind!=4)Check(cv1kV2Stats.residentBlocks>cv1kV2Stats.residentGroups,
			"multiple native bodies per region");
		printf("C3 case %u groups %llu blocks %llu ops %llu invalid %llu\n",kind,
			(unsigned long long)cv1kV2Stats.residentGroups,
			(unsigned long long)cv1kV2Stats.residentBlocks,
			(unsigned long long)cv1kV2Stats.residentOps,
			(unsigned long long)cv1kV2Stats.residentInvalid);
	}
	return result;
}
static void ResidentSMC(){
	InitC3(3);
	const UINT32 base=0x80000000,target=base+32;
	Sh3MapMemory((UINT8*)code,0xc800000,0xc80ffff,MAP_WRITE);
	code[0]=0x7001;code[1]=0x2131;BranchAt(2,16);
	code[16]=0x7213;BranchAt(17,0);
	m_r[1]=0x8c800020;m_r[3]=0xe77f;
	PromoteAt(base);PromoteAt(target);
	m_pc=base;m_sh4_icount=100;
	Check(Cv1kV2Try(base,false,true)==1 && m_pc==base+2 &&
		code[16]==0x7213 && m_sh4_icount==99,"resident store guard before watched write");
	WW(0x8c800020,0xe77f);
	m_pc=base;m_sh4_icount=100;
	Check(!Cv1kV2Try(base,false,true) && cv1kV2Stats.residentInvalid &&
		!Cv1kV2Find(target),"region invalidated before modified target");
	Sh3Scan(ACB_WRITE);
	Check(!cv1kC3Count && !cv1kV2Code.Used(),"restore clears resident regions");
	puts("C3 mirrored watched-store guard, invalidation, restore PASS");
}
static void ScratchPolicy(){
	InitC3(3);
	std::vector<UINT8> ram(0x10000);
	Sh3MapMemory(ram.data(),0xf0000000u,0xf0003fffu,MAP_RAM);
	Sh3SetCv1kTrackedIO();Sh3SetCv1kCacheRAM(ram.data(),0x4000);
	Check(cv1kV2ScratchRam==(uintptr_t)ram.data(),"registered P4 cache backing");
	const uint64_t before=cv1kV2OpaqueEpoch;
	WB(0xf0000000u,0x7f);WW(0xf0003ffeu,0x1234);
	Check(cv1kV2OpaqueEpoch==before,"bounded P4 writes not opaque");
	Cv1kV2NoteWrite(0xf0003fffu,2);
	Check(cv1kV2OpaqueEpoch==before+1,"crossing actual P4 extent opaque");
	Sh3MapMemory(ram.data(),0x0,0xffff,MAP_FETCHOP);
	Check(!cv1kV2ScratchRam,"mapping revokes scratch registration");
	Sh3SetCv1kCacheRAM(ram.data(),0x4000);
	Check(Cv1kV2WatchCode(ram.data()) && cv1kV2ScratchWatched,"watched host alias");
	const uint64_t watched=cv1kV2OpaqueEpoch;
	WW(0xf0000000u,0xe77f);
	Check(cv1kV2OpaqueEpoch==watched+1,"P4 alias writes opaque");
	puts("C3 P4 scratch extent, alias watch and remap revocation PASS");
}
static std::vector<Snapshot> DynamicCase(bool call,unsigned mode){
	InitC3(mode);
	const UINT32 base=0x80000000u;
	code[0]=0x7001;code[1]=call?0x460b:0x462b;code[2]=0x7301;
	code[3]=0x7401;BranchAt(4,0);
	code[16]=0x7213;code[17]=call?0x000b:0xafed;code[18]=0x0009;
	code[32]=0x7211;code[33]=call?0x000b:0xafdd;code[34]=0x0009;
	m_r[6]=base+32;
	if(mode){
		for(unsigned i:{0u,3u,16u,32u})PromoteAt(base+i*2);
	}
	m_pc=base;m_sh4_icount=0;
	std::vector<Snapshot> result;
	for(unsigned i=0;i<48;i++){
		// Change the dynamic target twice. The old prediction must never
		// execute on mismatch; the captured target precedes the delay slot.
		if(i==16)m_r[6]=base+64;
		if(i==32)m_r[6]=base+32;
		result.push_back(Snap(Sh3Run_timerhack(i%4?211:2003)));
	}
	if(mode==3){
		Check(cv1kV2Stats.residentBuilt>=2 && cv1kV2Stats.residentOps &&
			cv1kV2Stats.residentBlocks>4*cv1kV2Stats.residentGroups,
			"predicted dynamic native edge executed");
		printf("C3 dynamic %s: groups %llu blocks %llu builds %llu\n",call?"JSR":"JMP",
			(unsigned long long)cv1kV2Stats.residentGroups,
			(unsigned long long)cv1kV2Stats.residentBlocks,
			(unsigned long long)cv1kV2Stats.residentBuilt);
	}
	return result;
}
static double ResidentBench(unsigned mode,unsigned kind,Snapshot &state){
	InitC3(mode);Fixture(kind,mode);
	for(unsigned i=0;i<8;i++)Sh3Run_timerhack(2049);
	const auto before=std::chrono::steady_clock::now();
	for(unsigned i=0;i<200;i++)Sh3Run_timerhack(25000);
	const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
	state=Snap(0);
	printf("C3 bench %u mode %u %.3f ms groups %llu blocks %llu ops %llu\n",kind,mode,ms,
		(unsigned long long)cv1kV2Stats.residentGroups,
		(unsigned long long)cv1kV2Stats.residentBlocks,
		(unsigned long long)cv1kV2Stats.residentOps);
	return ms;
}
#ifndef SH3_V2_C4_TEST
int main(int argc,char **){
	setvbuf(stdout,NULL,_IOLBF,0);
	if(argc>1){
		for(unsigned k:{0u,2u,3u}){
			double ms[3][3];Snapshot states[3];
			const unsigned modes[]={0,2,3};
			for(unsigned i=0;i<3;i++){
				for(unsigned j=0;j<3;j++){
					unsigned n=(i+j)%3;
					ms[n][i]=ResidentBench(modes[n],k,states[n]);
				}
				Compare(states[0],states[1],k,i);Compare(states[0],states[2],k,i);
			}
			for(auto &a:ms)std::sort(a,a+3);
			printf("C3 median %u interpreter %.3f C2 %.3f resident %.3f ms\n",k,ms[0][1],ms[1][1],ms[2][1]);
		}
		return 0;
	}
	ResidentSMC();ScratchPolicy();
	for(bool call:{false,true}){
		const auto a=DynamicCase(call,0),b=DynamicCase(call,3);
		for(unsigned i=0;i<a.size();i++)Compare(a[i],b[i],160+unsigned(call),i);
	}
	puts("C3 dynamic JMP/JSR: 96 differential slices PASS");
	for(unsigned k=0;k<7;k++){
		const auto a=ResidentCase(k,0),b=ResidentCase(k,3);
		for(unsigned i=0;i<a.size();i++)Compare(a[i],b[i],150+k,i);
	}
	puts("C3 direct resident regions: 224 differential slices PASS");
	return 0;
}

#endif
