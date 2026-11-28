// C-2: execute real patched B edges in one trampoline, and verify
// exact state, dirty checks, alias invalidation, and stale-link exits.
#define SH3_V2_C2_TEST 1
#include "test_sh3_v2_c1.cpp"
// The tracked-I/O registration references the actual internal DMAC
// handlers, so link the real serial-flash implementation as well.
#include "../src/burn/devices/serflash.cpp"

static void InitC2(unsigned mode,bool always=false){
	setenv("FBNEO_CV1K_V2_LINK",mode==2?"1":"0",1);
	setenv("FBNEO_CV1K_V2_VERIFY_ALWAYS",always?"1":"0",1);
	C1Init(mode!=0,mode!=0);
	Check(mode!=2 || cv1kV2LinkEnabled,"C2 policy");
}
static void BranchAt(unsigned at,unsigned target,UINT16 kind=0xa000){
	code[at]=kind|((int(target)-int(at)-2)&0xfff);
	code[at+1]=0x0009;
}
static void Fixture(unsigned kind,unsigned mode){
	const UINT32 base=0x80000000;
	unsigned entries[3]={0,2,16},n=3;
	if(kind==0 || kind==1){
		code[0]=0x4210;code[1]=kind?0x8f0d:0x8b0d;
		code[2]=0x7301;code[3]=0xe203;BranchAt(4,0);
		code[16]=0x7001;BranchAt(17,0);
		m_r[2]=3;
	}else if(kind==2){
		// Static BSR is linked, RTS returns through the dynamic exit.
		code[0]=0x7001;BranchAt(1,16,0xb000);
		code[3]=0x7301;BranchAt(4,0);
		code[16]=0x7213;code[17]=0x000b;code[18]=0x0009;
		entries[1]=3;
	}else if(kind==3){
		// Wide register footprint + direct load/store, then a leaf.
		for(unsigned i=0;i<10;i++)code[i]=0x7001|(i<<8);
		code[10]=0x6312;code[11]=0x2132;BranchAt(12,16);
		code[16]=0x7401;BranchAt(17,0);
		m_r[1]=0x8c000100;code[1]=0x7001; // keep R1 fixed
		entries[1]=16;n=2;
	}else if(kind==4){
		// Enter a linked block that exits to a real MMIO callback.
		code[0]=0x7001;BranchAt(1,16);
		code[16]=0x6312;code[17]=0x7401;BranchAt(18,0);
		m_r[1]=0x8d000100;scenario=3;
		entries[1]=16;n=2;
	}else{
		// Cross 64 KiB fetch-page boundary in both directions.
		code[32764]=0x7001;BranchAt(32765,32770);
		code[32770]=0x7213;BranchAt(32771,32764);
		entries[0]=32764;entries[1]=32770;n=2;
	}
	if(mode){
		for(unsigned i=0;i<n;i++)PromoteAt(base+entries[i]*2);
	}
	m_pc=base+entries[0]*2;m_sh4_icount=0;
}
static std::vector<Snapshot> LinkCase(unsigned kind,unsigned mode){
	InitC2(mode);Fixture(kind==6?0:kind,mode);
	if(kind==6){
		m_timer[0].timer_prescaler=1;m_timer[0].timer_exec=TimerTest;
		m_timer[0].start(1100,0,1,0);
	}
	std::vector<Snapshot> result;
	for(unsigned i=0;i<32;i++){
		const unsigned budget=i==0?1601:1+(i*37)%231;
		result.push_back(Snap(Sh3Run_timerhack(budget)));
	}
	if(mode==2){
		Check(cv1kV2Stats.linkGroups && cv1kV2Stats.linkEdges &&
			cv1kV2Stats.linkBodies>0,"actual native B edge executed");
		if(kind==4)Check(cv1kV2Stats.guards && hits && cv1kV2Stats.disabled &&
			Cv1kV2LinkedTarget(0x80000000)->edge[0]==0,"linked MMIO guard and disable");
		else Check(cv1kV2Stats.linkBodies>cv1kV2Stats.linkGroups,"multiple bodies per trampoline");
		if(kind<2)Check(cv1kV2Stats.taken && cv1kV2Stats.notTaken,"both conditional edges");
		printf("C2 case %u groups %llu bodies %llu edges %llu guards %llu\n",kind,
			(unsigned long long)cv1kV2Stats.linkGroups,(unsigned long long)cv1kV2Stats.linkBodies,
			(unsigned long long)cv1kV2Stats.linkEdges,(unsigned long long)cv1kV2Stats.guards);
	}
	return result;
}
static void NativeSMC(){
	InitC2(2);
	const UINT32 base=0x80000000,target=base+32;
	Sh3MapMemory((UINT8*)code,0xc800000,0xc80ffff,MAP_WRITE);
	code[0]=0x2131;BranchAt(1,16);
	code[16]=0x7001;BranchAt(17,0);
	m_r[1]=0x8c800020;m_r[3]=0xe77f;
	PromoteAt(base);PromoteAt(target);
	auto *d=Cv1kV2LinkedTarget(base),*t=Cv1kV2LinkedTarget(target);
	Check(d && t && d->edge[0]==t->offset,"linked SMC fixture");
	m_pc=base;m_sh4_icount=100;
	const unsigned done=Cv1kV2Try(base,false,true);
	Check(done==3 && m_pc==target && m_sh4_icount==96 && code[16]==0xe77f,
		"native mirror store stops before stale target");
	Check(!t->active && !d->edge[0] && !Cv1kV2Find(target) &&
		cv1kV2Stats.changed==1 && !cv1kV2Stats.untracked,"retire and unlink incoming B");
	PromoteAt(target);
	Check(d->edge[0] && d->edge[0]!=t->offset,"recompile relinks a fresh pinned descriptor");
	// Map changes invalidate the version before any old source load.
	Remap();
	Sh3V2::LinkContext c={};
	c.memory={MemMapR,MemMapRL,MemMapW,d->source,size_t(d->count)*2,1,base,
		NULL,0,m_pr,cv1kV2WriteWatch,cv1kV2Generations};
	c.mapVersion=cv1kV2MapVersion;c.slice=cv1kV2SliceEpoch;c.remaining=100;c.limit=128;
	const auto before=Snap(0);
	fbneo_sh3_v2_link_enter((const UINT8*)cv1kV2Code.ExecBase()+d->offset,m_r,&m_sr,&m_ea,&c);
	Check(c.status==3 && !c.blocks && !c.ops,"old direct link rejected on remap");
	Compare(before,Snap(0),130,0);
	Sh3Scan(ACB_WRITE);
	Check(!cv1kV2LinkCount && !cv1kV2Code.Used() && !Cv1kV2Find(base),"restore destroys all link addresses");
	puts("C2 mirrored native SMC, unlink/relink, stale remap, restore PASS");
}
static void TrustedSlice(bool always){
	InitC2(2,always);
	std::vector<UINT16> ram(0x800000/2,0x0009);
	Sh3MapMemory((UINT8*)ram.data(),0xc000000,0xc7fffff,MAP_RAM);
	Sh3MapMemory((UINT8*)ram.data(),0xc800000,0xcffffff,MAP_RAM);
	Sh3SetCv1kTrackedIO();
	Sh3SetCv1kFastMemory((UINT8*)ram.data(),0x800000,NULL);
	const UINT32 base=0x8c000000,target=base+32;
	ram[0]=0x7001;ram[1]=0xa00d;ram[2]=0x0009;
	ram[16]=0x7213;ram[17]=0xafed;ram[18]=0x0009;
	PromoteAt(base);PromoteAt(target);
	auto *d=Cv1kV2LinkedTarget(base);
	Check(d && bool(d->flags&Sh3V2::LinkTrusted)==!always,"conditional verification policy");
	m_pc=base;m_sh4_icount=600;
	Check(Cv1kV2Try(base,false,true)>3,"native group");
	Check(bool(cv1kV2Stats.verifySkipped)==!always,"actual conditional comparisons");
	// A tracked mirrored write changes a linked target in the SAME
	// slice, without relying on the external-write fence.
	WW(0x8c800020,0xe77f);
	m_pc=base;m_sh4_icount=100;
	Check(Cv1kV2Try(base,false,true)==3 && m_pc==target &&
		cv1kV2Stats.changed==1 && !cv1kV2Stats.untracked,"dirty-only mirror invalidation");
	WW(0x8c800020,0x7213);
	PromoteAt(target);
	// External raw RAM changes can occur between CPU invocations.
	ram[16]=0xe77f;
	if(!always)++cv1kV2SliceEpoch;
	m_pc=base;m_sh4_icount=100;
	Check(Cv1kV2Try(base,false,true)==3 && m_pc==target &&
		cv1kV2Stats.changed==2 && !Cv1kV2Find(target),"external mutation caught at fence or verify-always");
	// A native store to the linked successor must also dirty its
	// backing page before the B edge can execute that successor.
	Sh3Scan(ACB_WRITE);
	ram[0]=0x2131;ram[16]=0x7213;
	m_r[1]=0x8c800020;m_r[3]=0xe77f;
	PromoteAt(base);PromoteAt(target);
	const auto untracked=cv1kV2Stats.untracked;
	m_pc=base;m_sh4_icount=100;
	Check(Cv1kV2Try(base,false,true)==3 && m_pc==target &&
		cv1kV2Stats.changed==3 && cv1kV2Stats.untracked==untracked,
		"native mirror store dirty-only invalidation");
	Cv1kV2Reset();Sh3MapInit();
}
static void HandlerPolicy(){
	InitC2(2);
	const auto start=cv1kV2OpaqueEpoch;
	Cv1kV2NoteHandlerWrite(1);
	Check(cv1kV2OpaqueEpoch==start+1,"unregistered handler opaque");
	Cv1kV2SetTracked(1,true);
	const auto registered=cv1kV2OpaqueEpoch;
	Cv1kV2NoteHandlerWrite(1);
	Check(cv1kV2OpaqueEpoch==registered,"registered handler nonopaque");
	Sh3SetWriteLongHandler(1,WriteTest);
	const auto changed=cv1kV2OpaqueEpoch;
	Cv1kV2NoteHandlerWrite(1);
	Check(cv1kV2OpaqueEpoch==changed+1,"handler replacement revokes policy");
	const auto p4=cv1kV2OpaqueEpoch;
	Cv1kV2NoteWrite(0xf0000000,4);
	Check(cv1kV2OpaqueEpoch==p4+1,"P4 conservative fallback");
	Check(Cv1kV2WatchMapSize==8192,"8 KiB map");
	puts("C2 watch map, MMIO registration/revocation, P4 PASS");
}
static double LinkBench(unsigned mode,unsigned kind,Snapshot &state){
	InitC2(mode);Fixture(kind,mode);
	for(unsigned i=0;i<8;i++)Sh3Run_timerhack(2049);
	const auto before=std::chrono::steady_clock::now();
	for(unsigned i=0;i<200;i++)Sh3Run_timerhack(25000);
	const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
	state=Snap(0);
	printf("C2 bench %u mode %u %.3f ms bodies %llu groups %llu edges %llu cycles %llu\n",kind,mode,ms,
		(unsigned long long)cv1kV2Stats.hits,(unsigned long long)cv1kV2Stats.linkGroups,
		(unsigned long long)cv1kV2Stats.linkEdges,(unsigned long long)cv1kV2Stats.cycles);
	return ms;
}
int main(int argc,char **){
	setvbuf(stdout,NULL,_IOLBF,0);
	if(argc>1){
		for(unsigned k: {0u,2u,3u}){
			double ms[3][3];Snapshot states[3];
			for(unsigned i=0;i<3;i++){
				for(unsigned j=0;j<3;j++){unsigned mode=(i+j)%3;ms[mode][i]=LinkBench(mode,k,states[mode]);}
				Compare(states[0],states[1],k,i);Compare(states[0],states[2],k,i);
			}
			for(auto &a:ms)std::sort(a,a+3);
			printf("C2 median %u interpreter %.3f C1 %.3f C2 %.3f ms\n",k,ms[0][1],ms[1][1],ms[2][1]);
		}
		return 0;
	}
	HandlerPolicy();NativeSMC();TrustedSlice(false);TrustedSlice(true);
	puts("C2 trusted dirty-only and VERIFY_ALWAYS external-write fences PASS");
	for(unsigned i=0;i<6;i++){
		InitC2(2);
		const auto a=IdleCase(false,i),b=IdleCase(true,i,false);
		Compare(a,b,139,i);
	}
	puts("C2 short unlinked idle fallback: 6 differential cases PASS");
	for(unsigned k=0;k<7;k++){
		const auto a=LinkCase(k,0),b=LinkCase(k,2);
		for(unsigned i=0;i<a.size();i++)Compare(a[i],b[i],140+k,i);
	}
	puts("C2 direct B linking: 224 differential slices PASS");
	return 0;
}
