// Stage 2.6: register real CV1000-shaped mappings; compare live execution
// with the old page-table path, including the exact idle speedhack.
#define main PreviousInterpreterMain
#include "test_sh3_interpreter.cpp"
#undef main
#include <algorithm>

static UINT16 ram[0x1000000/2],rom[0x400000/2],other[0x10000/2];
static INT32 idleBurn=11;
static constexpr UINT32 idleAddress=0xc002310, idlePC=4;
static UINT32 __fastcall IdleCallback(UINT32 a){
	if(a==idleAddress && (Sh3GetPC(-1)==idlePC || Sh3GetPC(-1)==idlePC+2))
		Sh3BurnCycles(idleBurn);
	UINT32 v;memcpy(&v,(UINT8*)ram+(a&0xfffffc),4);
#ifdef LSB_FIRST
	v=(v<<16)|(v>>16);
#endif
	return v;
}
static UINT32 __fastcall NewHandler(UINT32 a){Observe(11,a);return 0x76543210;}
static void InitMem(bool fast,unsigned size,bool idle=true,int idleBatch=-1){
	setenv("FBNEO_CV1K_FAST_RAM",fast?"1":"0",1);
	setenv("FBNEO_CV1K_IDLE_BATCH",(idleBatch<0?fast:idleBatch!=0)?"1":"0",1);
	setenv("FBNEO_CV1K_IDLE_DIRECT","1",1);
	Setup(9,0);
	// The generic fixture mapped code writable; MAP_ROM does not clear
	// an old write mapping. Match the actual CV1000 initialization.
	Sh3MapHandler(0,0,0x3fffff,MAP_WRITE);
	Sh3MapMemory((UINT8*)rom,0,0x3fffff,MAP_ROM);
	Sh3MapMemory((UINT8*)ram,0xc000000,0xc000000+size-1,MAP_RAM);
	if(size==0x800000)Sh3MapMemory((UINT8*)ram,0xc800000,0xcffffff,MAP_RAM);
	if(idle){
		Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);
		Sh3SetReadLongHandler(1,IdleCallback);
		Sh3SetCv1kIdleRead((UINT8*)ram,idleAddress,idlePC,&idleBurn,IdleCallback);
	}
	Sh3SetCv1kFastMemory((UINT8*)ram,size,(UINT8*)rom);
	Check(bool(cv1kFastRamBase)==fast && bool(cv1kFastRomBase)==fast,"validated memory policy");
	Check(!fast || cv1kFastRamMask==size-1,"registered mirror mask");
	memset(rom,0,0x10000);
	for(unsigned i=0;i<0x10000/2;i++)rom[i]=0x7001;
	m_pc=0x80000000;m_pr=0x80000020;m_gbr=0x8c002310;
}
static void StoreLong(UINT16 *p,UINT32 val){
#ifdef LSB_FIRST
	val=(val<<16)|(val>>16);
#endif
	memcpy(p,&val,4);
}
struct Result {State s;UINT32 pr,mem[8];};
static Result Run(bool fast,unsigned size,unsigned kind,unsigned budget,int idleBatch=-1){
	InitMem(fast,size,true,idleBatch);
	const UINT32 addr[]={0x8c000100,0xac00230c,0x8c002310,0xac002314,
		0x8c800100,0x8cfffffc,0x8c000101,0x8d000100,0x80000300,0x8c010000};
	UINT32 a=addr[kind];
	// Populate the exact mapped data, including alias/mirror boundaries.
	for(UINT32 off:{0x100u,0x230cu,0x2310u,0x2314u,0x800100u,0xfffffcu,0x10000u})
		StoreLong(ram+((off&(size-1))/2),0x91abcdef^off);
	StoreLong(rom+0x300/2,0x87654321);
	rom[0]=0x7a01;rom[1]=0x6312;rom[2]=0x4b26; // MOV.L then LDS.L @R11+,PR
	rom[3]=0xa004;rom[4]=0x6412; // fused long load in delay slot
	rom[9]=0x7c01;rom[10]=0x6252; // trailing MMIO callback
	m_r[1]=a;m_r[11]=a;m_r[5]=0x8d000000;
	const int spent=Sh3Run_timerhack(budget);
	Check(cv1kFlowActive,"actual stage 2.6 dispatcher");
	Result r={};r.s=Save(spent);r.pr=m_pr;
	r.mem[0]=RL(a);r.mem[1]=m_r[1];r.mem[2]=m_r[11];r.mem[3]=m_r[3];r.mem[4]=m_r[4];
	return r;
}
static void ExactIdle(){
	for(unsigned fast=0;fast<2;fast++){
		InitMem(fast,0x800000);
		StoreLong(ram+(0x2310/2),0x1234abcd);
		rom[0]=0x7a01;rom[1]=0x6312;rom[2]=0x0009;
		m_r[1]=0x8c002310;
		Check(Sh3Run_timerhack(2)==13,"exact idle burn");
		Check(m_r[3]==0x1234abcd,"exact idle value");
		InitMem(fast,0x800000);
		rom[0]=0x7a01;rom[1]=0x6312;
		m_r[1]=0x8c802310; // mirror is not the idle handler
		Check(Sh3Run_timerhack(2)==2,"mirror does not burn");
		Check(m_r[3]==0x1234abcd,"mirror value");
	}
}
static void Pointers(){
	for(unsigned size:{0x800000u,0x1000000u}){
		InitMem(true,size);
		for(UINT32 off=0;off<0x1000000;off+=0x10000){
			for(UINT32 edge:{0u,2u,4u,0xfffcu,0xfffeu,0xffffu}){
				const UINT32 o=off+edge,a=0xac000000+o;
				UINT8 *p=(UINT8*)ram+(o&(size-1));
#ifdef LSB_FIRST
				UINT8 *byte=(UINT8*)ram+((o&(size-1))^1);
#else
				UINT8 *byte=p;
#endif
				Check(Cv1kDirectPointer<1,false,true>(a)==byte && Cv1kDirectPointer<1,true,true>(a)==byte,"byte mapping");
				Check(Cv1kDirectPointer<2,false,true>(a)==((o&1)?nullptr:p),"word mapping");
				Check(Cv1kDirectPointer<4,false,true>(a)==((o&3)?nullptr:p),"long mapping");
				Check(Cv1kDirectPointer<4,true,true>(a)==((o&3)?nullptr:p),"store mapping");
			}
		}
		Check(!Cv1kDirectPointer<4,false,true>(0x8c002310),"idle exception");
		Check(Cv1kDirectPointer<4,true,true>(0x8c002310)==(UINT8*)ram+0x2310,"idle write allowed");
		Check(Cv1kDirectPointer<4,false,true>(0x8c00230c)==(UINT8*)ram+0x230c,"idle page neighbor");
		Check(Cv1kDirectPointer<4,false,true>(0x8c002314)==(UINT8*)ram+0x2314,"idle page neighbor 2");
		Check(!Cv1kDirectPointer<4,false,true>(0xec000100),"P4 not masked");
		Check(!Cv1kDirectPointer<4,true,true>(0x80000004),"ROM write not allowed");
		Check(Cv1kDirectPointer<4,false,true>(0xa03ffffc)==(UINT8*)rom+0x3ffffc,"ROM end");
		Sh3MapMemory((UINT8*)other,0xc000000,0xc00ffff,MAP_READ);
		Check(!cv1kFastRamBase && !cv1kFastRomBase,"read remap revokes direct regions");
		Check(Cv1kDirectPointer<4,false,true>(0x8c002310)==(UINT8*)other+0x2310,"remap uses live map");
		Sh3SetCv1kFastMemory((UINT8*)ram,size,(UINT8*)rom);
		Check(!cv1kFastRamBase && cv1kFastRomBase,"bad RAM region rejected independently");
		InitMem(true,size);
		Sh3MapHandler(1,0xc010000,0xc01ffff,MAP_WRITE);
		Check(!cv1kFastRamBase && !Cv1kDirectPointer<4,true,true>(0x8c010004),"write handler revokes");
		InitMem(true,size);
		Sh3SetReadLongHandler(1,NewHandler);
		Check(!cv1kFastRamBase && !Cv1kDirectPointer<4,false,true>(0x8c002314),"idle handler replacement revokes");
		InitMem(true,size);
		Sh3MapReadLongHandler(1,0xc010000,0xc01ffff);
		Check(!cv1kFastRamBase && !Cv1kDirectPointer<4,false,true>(0x8c010004),"long map revokes");
		InitMem(true,size);
		Sh3MapMemory((UINT8*)other,0,0xffff,MAP_FETCHOP);
		Check(!cv1kFastRomBase && !cv1kFastRamBase,"fetch remap revokes");
		InitMem(true,size,false);
		Check(Cv1kDirectPointer<4,false,true>(0x8c002310)==(UINT8*)ram+0x2310,"no idle handler");
		Sh3Exit();Check(!cv1kFastRamBase && !cv1kFastRomBase,"exit revokes");
	}
	puts("CV1000 direct region validation: Type B/C mirrors, Type D, all 256 RAM pages, ROM, handlers and remaps PASS");
}
static void Execution(){
	unsigned cases=0;
	for(unsigned size:{0x800000u,0x1000000u})for(unsigned kind=0;kind<10;kind++)
	for(unsigned budget=1;budget<=13;budget++){
		const auto a=Run(false,size,kind,budget),b=Run(true,size,kind,budget);
		if(memcmp(&a,&b,sizeof(a))){
			fprintf(stderr,"fast memory mismatch size=%x kind=%u budget=%u pc=%x/%x cycles=%d/%d\n",
				size,kind,budget,a.s.pc,b.s.pc,a.s.spent,b.s.spent);exit(1);
		}
		const auto c=Run(false,size,kind,budget,1);
		if(memcmp(&a,&c,sizeof(a))){
			fprintf(stderr,"idle-only mismatch size=%x kind=%u budget=%u\n",size,kind,budget);exit(1);
		}
		cases++;
	}
	ExactIdle();
	printf("CV1000 full-region and idle-only MOV/stack/slot: %u scenarios x 2 paths PASS\n",cases);
	InitMem(false,0x800000,true,1);
	Check(!cv1kFastRamBase && !cv1kFastRomBase && cv1kIdleBatchEnabled,"independent idle-only policy");
	Check(Cv1kDirectPointer<4,false>(0x8c002314)==(UINT8*)ram+0x2314,"idle-only neighbor");
	Check(!Cv1kDirectPointer<4,false>(0x8c002310),"idle-only exact address stays slow");
	Sh3SetReadLongHandler(1,NewHandler);
	Check(!Cv1kDirectPointer<4,false>(0x8c002314),"idle-only replacement not bypassed");
	puts("CV1000 independent idle-only policy and live-handler guard PASS");
}
static double Bench(bool fast,unsigned kind){
	InitMem(fast,0x800000);
	// Same instructions, different direct read region (first idle page,
	// ordinary RAM or ROM). All state and data values are live.
	const UINT16 ops[]={0x6312,0x7301,0x2232,0x6412,0x2430,0xaff9,0x7a01};
	memcpy(rom,ops,sizeof(ops));
	m_r[1]=kind==0?0x8c002314:kind==1?0x8c100100:0x80000300;
	m_r[2]=0x8c100200;m_r[3]=0;
	// Avoid writes via loaded R4: use stable R2 for the second store.
	rom[4]=0x2230;
	StoreLong(kind==2?rom+0x300/2:ram+((m_r[1]&0x7fffff)/2),0x1234abcd);
	Sh3Run_timerhack(20000);
	auto t=std::chrono::steady_clock::now();
	for(unsigned i=0;i<400;i++)Sh3Run_timerhack(25000);
	const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
	printf("fastmem bench %u %d %.3f ms checksum %08x/%08x/%08x\n",kind,fast,ms,m_r[3],m_r[4],m_r[10]);return ms;
}
int main(int argc,char**){
	setvbuf(stdout,NULL,_IOLBF,0);
	for(auto key:{"FBNEO_CV1K_POLL","FBNEO_CV1K_TIMER_FAST","FBNEO_CV1K_DATA_LOOPS","FBNEO_CV1K_DATA_FULL","FBNEO_CV1K_INTERP_LOOPS"})setenv(key,"0",1);
	if(argc>1){for(unsigned kind=0;kind<3;kind++){
		double a[3],b[3];
		for(unsigned i=0;i<3;i++){if(i&1){b[i]=Bench(true,kind);a[i]=Bench(false,kind);}else{a[i]=Bench(false,kind);b[i]=Bench(true,kind);}}
		std::sort(a,a+3);std::sort(b,b+3);printf("fastmem median %u %.3f -> %.3f ms\n",kind,a[1],b[1]);
	}return 0;}
	Pointers();Execution();
#if defined(SH3_CV1K_OPSTAT) && SH3_CV1K_OPSTAT
	InitMem(true,0x800000);
	rom[0]=0x6312;rom[1]=0xa001;rom[2]=0x7a01;rom[4]=0x0009;
	m_r[1]=0x8c002314;
	Sh3Run_timerhack(4);
	Check(cv1kOpStat.ops[SH3_ID_MOVLL]==1 && cv1kOpStat.ops[SH3_ID_BRA]==1 &&
		cv1kOpStat.fused[SH3_ID_ADDI]==1 && cv1kOpStat.idlePage==1,"diagnostic counts");
	for(unsigned i=0;i<120;i++)Sh3Cv1kOpStatsFrame();
	Check(cv1kOpStat.ops[SH3_ID_MOVLL]==0 && cv1kOpStatFrame==120,"diagnostic window reset");
	puts("CV1000 diagnostic counters and window reset PASS");
#endif
	return 0;
}
