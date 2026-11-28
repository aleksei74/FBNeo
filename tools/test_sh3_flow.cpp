// Differential tests for stage 2.5 against the actual stage 2 dispatcher.
#define main PriorInterpreterMain
#include "test_sh3_interpreter.cpp"
#undef main
#include <algorithm>

struct FlowState {
	State core;
	UINT32 special[7],irq[3];
	UINT64 codeHash,replacementHash;
};
static UINT64 Hash(const UINT16 *p,unsigned size){
	UINT64 h=0;
	for(unsigned i=0;i<size;i++)h=h*1099511628211ull+p[i];
	return h;
}
static FlowState Snapshot(int spent,bool hashes=true){
	FlowState s={};
	s.core=Save(spent);
	const UINT32 special[]={m_pr,m_gbr,m_vbr,m_dbr,m_sgr,m_fpul,m_fpscr};
	memcpy(s.special,special,sizeof(s.special));
	s.irq[0]=m_test_irq;s.irq[1]=m_pending_irq;s.irq[2]=m_cpu_off;
	if(hashes){s.codeHash=Hash(code,65536);s.replacementHash=Hash(replacement,65536);}
	return s;
}
static void Equal(const FlowState &a,const FlowState &b,const char *where,
	unsigned op,unsigned kind,unsigned budget)
{
	if(!memcmp(&a,&b,sizeof(a)))return;
	fprintf(stderr,"%s opcode=%04x kind=%u budget=%u PC=%08x/%08x PPC=%08x/%08x delay=%08x/%08x EA=%08x/%08x PR=%08x/%08x cycles=%d/%d trace=%llx/%llx\n",
		where,op,kind,budget,a.core.pc,b.core.pc,a.core.ppc,b.core.ppc,a.core.delay,b.core.delay,
		a.core.ea,b.core.ea,a.special[0],b.special[0],a.core.spent,b.core.spent,
		(unsigned long long)a.core.trace,(unsigned long long)b.core.trace);
	const UINT8 *p=(const UINT8*)&a,*q=(const UINT8*)&b;
	for(size_t i=0;i<sizeof(a);i++)if(p[i]!=q[i]){
		fprintf(stderr,"first diff %zu %02x/%02x\n",i,p[i],q[i]);break;
	}
	exit(1);
}
static void __fastcall FlowWriteWord(UINT32 a,UINT16 v){Observe(6,a,v);Remap();}
static UINT16 __fastcall NextRead(UINT32 a){
	Observe(3,a);return 0x8bfd;
}

static void InitFlow(unsigned mode,unsigned scenario=0){
	Setup(mode,scenario);
	Sh3SetWriteWordHandler(1,FlowWriteWord);
	m_sr=0;m_ea=0x12345678;m_pr=0x80000080;
	m_mach=0x89abcdef;m_macl=0x76543210;
	m_gbr=0x8c000400;m_dbr=0x13579bdf;m_sgr=0x2468ace0;
	m_fpul=0xabcdef01;m_fpscr=0x00123456;
	for(unsigned r=0;r<15;r++)m_r[r]=0x8c000100+r*0x10;
	m_r[15]=0x8d000000;
	for(unsigned i=0;i<32770;i++)data[i]=UINT16(0x91ab+i*0x9e37);
}

// Eleven branch forms, both T values, live slots, cross-page boundaries,
// taken and untaken conditions, pending slots and callback fallback.
static FlowState BranchRun(unsigned mode,unsigned branch,unsigned position,unsigned type,
	unsigned t,unsigned budget,bool again)
{
	InitFlow(mode,type==9?1:type==10?2:type==11?3:0);
	const UINT16 branches[]={0x8b00,0x8f00,0xa000,0x0423,0xb000,0x0403,
		0x8900,0x8d00,0x442b,0x440b,0x000b};
	const UINT32 loc=position==0?0x40:position==1?0xfffe:0x80;
	UINT32 target=position==0?0x80:position==1?0x10040:0x40;
	UINT16 op=branches[branch];
	const INT32 disp=(INT32(target)-INT32(loc)-4)/2;
	if(branch==0||branch==1||branch==6||branch==7)op|=UINT8(disp);
	if(branch==2||branch==4)op|=disp&0xfff;
	if(branch==3||branch==5)m_r[4]=target-loc-4;
	if(branch==8||branch==9)m_r[4]=(position==2?0xa0000000:0x80000000)|target;
	if(branch==10)m_pr=0x80000000|target;
	m_sr=t?T:0;
	const UINT16 slots[]={0x0009,0x7501,0x6542,0x2461,0x4526,0x4522,0x9530,0xd530,
		0x062a,0x65f2,0x65f2,0x65f2,0x2461,0x6542,0x2461,0x4710,0xcb80};
	UINT16 slot=slots[type];
	// Keep R4 intact for register branches; use R5 as the data base.
	if(type==2||type==13)slot=0x6752;
	if(type==3||type==12||type==14)slot=0x2561;
	if(type==12)m_r[5]=0x8d000000; // write callback remaps fetch
	if(type==13)m_r[5]=0x8c000101; // unaligned fallback
	if(type==14){m_r[5]=0x80000000|target;m_r[6]=0x7009;} // alias SMC
	if(type==5)m_r[5]=0x8c000800;
	if(type==4)m_r[5]=0x8c000800;
	code[loc/2]=op;code[loc/2+1]=slot;
	code[target/2]=0x7201;code[target/2+1]=0x63f2;
	// A callback at the target also checks deferred PC/cycle publication.
	m_pc=0x80000000|loc;
	int spent=Sh3Run_timerhack(budget);
	if(again)spent+=Sh3Run_timerhack(5);
	return Snapshot(spent);
}
static void BranchTests(){
	unsigned cases=0;
	// Every branch/slot/position pair, with varied budgets and flags.
	for(unsigned branch=0;branch<11;branch++)for(unsigned position=0;position<3;position++)
	for(unsigned type=0;type<17;type++)for(unsigned t=0;t<2;t++){
		const unsigned budget=(branch+position+type+t)%6;
		const bool again=(type+position)&1;
		auto a=BranchRun(8,branch,position,type,t,budget,again);
		auto b=BranchRun(9,branch,position,type,t,budget,again);
		Equal(a,b,"branch",branch,position*100+type*2+t,budget);cases++;
	}
	// Exhaust every short slice boundary on an ordinary fused slot.
	for(unsigned branch=0;branch<11;branch++)for(unsigned t=0;t<2;t++)
	for(unsigned budget=0;budget<=6;budget++)for(unsigned again=0;again<2;again++){
		auto a=BranchRun(8,branch,0,1,t,budget,again);
		auto b=BranchRun(9,branch,0,1,t,budget,again);
		Equal(a,b,"branch boundary",branch,t,budget);cases++;
	}
	printf("SH3 flow branches: %u actual slice comparisons PASS\n",cases);
}
static FlowState StackRun(unsigned mode,unsigned op,unsigned kind,unsigned budget){
	InitFlow(mode,kind==2?1:0);
	UINT32 base=kind==0?0x8c000400:kind==1?0xac000400:kind==2?0x8d000400:
		kind==3?0x8c000401:kind==4?0xf0000400:kind==5?0x8c00fffc:0x8c000400;
	Sh3MapMemory((UINT8*)data,0xf0000000,0xf000ffff,MAP_RAM);
	if(kind==6)Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);
	m_r[(op>>8)&15]=base;
	code[0]=0x7101;code[1]=op;code[2]=0x7201;code[3]=0x63f2;
	int spent=Sh3Run_timerhack(budget);
	spent+=Sh3Run_timerhack(5);
	return Snapshot(spent);
}
static void StackTests(){
	unsigned cases=0;
	const UINT16 ops[]={0x4202,0x4212,0x4222,0x4206,0x4216,0x4226};
	for(auto op:ops)for(unsigned kind=0;kind<7;kind++)for(unsigned budget=0;budget<5;budget++){
		auto a=StackRun(8,op,kind,budget);
		auto b=StackRun(9,op,kind,budget);
		if(kind==0)Check(cv1kFlowStackHits>0,"stack path coverage");
		Equal(a,b,"stack",op,kind,budget);cases++;
	}
	printf("SH3 flow stack: %u slice comparisons (six PR/MACH/MACL forms) PASS\n",cases);
}
static FlowState DTRun(unsigned mode,unsigned kind,unsigned count,unsigned budget){
	InitFlow(mode);
	m_r[7]=count;
	code[0]=0x4710;code[1]=0x8bfd;code[2]=0x0009;
	if(kind==0)code[1]=0x7001;
	if(kind==1){ // Fetch and data differ; old busy loop must not read fetch.
		replacement[1]=0x7001;Sh3MapMemory((UINT8*)replacement,0,0x1ffff,MAP_READ);
	}
	if(kind==2){
		code[1]=0x7001;replacement[1]=0x8bfd;
		Sh3MapMemory((UINT8*)replacement,0,0x1ffff,MAP_READ);
	}
	if(kind==3){
		Sh3MapHandler(1,0,0xffff,MAP_READ);
		Sh3SetReadWordHandler(1,NextRead);code[1]=0x7001;
	}
	if(kind==4){ // BRA $ uses the data map, including callback.
		code[0]=0xaffe;code[1]=0x0009;
		Sh3MapHandler(1,0,0xffff,MAP_READ);Sh3SetReadWordHandler(1,NextRead);
	}
	if(kind==5){code[0]=0xaffe;code[1]=0x0009;}
	int spent=Sh3Run_timerhack(budget);
	return Snapshot(spent);
}
static void DTTests(){
	unsigned cases=0;
	for(unsigned kind=0;kind<6;kind++)for(unsigned count:{0u,1u,2u,3u,256u,0xffffffffu})
	for(unsigned budget:{0u,1u,2u,3u,4u,5u,7u,8u,17u,127u}){
		auto a=DTRun(8,kind,count,budget);
		auto b=DTRun(9,kind,count,budget);
		if(kind==0)Check(cv1kFlowDTHits>0,"DT path coverage");
		Equal(a,b,"DT/BRA lookahead",0x4710,kind,budget);cases++;
	}
	printf("SH3 DT/BRA data-map and busy-loop: %u comparisons PASS\n",cases);
}
static void PureSlots(){
	bool pure[SH3_ID_COUNT]={};
#define SH3_OPCODE(name)
#define SH3_PURE_OPCODE(name) pure[SH3_ID_##name]=true;
#define SH3_FLOW_OPCODE(name) pure[SH3_ID_##name]=true;
#include "../src/cpu/sh4/sh3_decode_ops.inc"
#undef SH3_FLOW_OPCODE
#undef SH3_PURE_OPCODE
#undef SH3_OPCODE
	InitFlow(9);
	unsigned cases=0;
	for(unsigned op=0;op<65536;op++)if(pure[sh3OpIds[op]]){
		FlowState a={};
		for(unsigned mode=0;mode<2;mode++){
			cv1kFlowEnabled=mode!=0;
			m_pc=0x80000000;m_ppc=m_ea=m_delay=m_spc=m_ssr=0;
			m_test_irq=m_pending_irq=m_cpu_off=0;
			m_pr=0x80000080;m_gbr=0x8c000400;m_vbr=0x80000000;
			m_dbr=0x12345678;m_sgr=0x87654321;m_fpul=op;m_fpscr=op&0x003fffff;
			m_sr=op&0x301;m_mach=op;m_macl=~op;
			m_sh4_icount=sh3_total_cycles=0;trace=hits=0;
			memset(m_rbnk,0,sizeof(m_rbnk));
			for(auto &timer:m_timer){timer={};timer.timer_prescaler=64;}
			for(auto &timer:m_dma_timer)timer={};
			cave_blitter_delay={};
			for(unsigned i=0;i<16;i++)m_r[i]=0xdeadbeefu*(i+1)+op;
			code[0]=0xa004;code[1]=UINT16(op);code[6]=0x0009;
			auto b=Snapshot(Sh3Run_timerhack(3),false);
			if(!mode)a=b;else Equal(a,b,"fused slot",op,0,3);
		}
		cases++;
	}
	Check(cv1kFlowSlotHits==cases,"all pure slots actually fused");
	printf("SH3 flow fused one-cycle slots: %u encodings PASS\n",cases);
}
static double Bench(unsigned mode,unsigned kind){
	InitFlow(mode);
	const UINT16 ramBranch[]={0x6312,0x7301,0x2132,0xaffb,0x7a01};
	const UINT16 stack[]={0x4f22,0xb003,0x7001,0x4f26,0xaffa,0x0009,0x000b,0x6212};
	const UINT16 dt[]={0x7201,0x4310,0x8bfc,0xe37f,0xaffa,0x0009};
	const UINT16 *ops=kind==0?ramBranch:kind==1?stack:dt;
	memcpy(code,ops,kind==0?sizeof(ramBranch):kind==1?sizeof(stack):sizeof(dt));
	m_r[1]=0x8c000100;m_r[3]=127;m_r[15]=0x8c00f000;
	Sh3Run_timerhack(20000);
	const auto begin=std::chrono::steady_clock::now();
	for(unsigned i=0;i<400;i++)Sh3Run_timerhack(25000);
	double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
	printf("flow bench %u mode %u %.3f ms checksum %08x/%08x/%08x branch %llu slot %llu stack %llu DT %llu\n",kind,mode,ms,
		m_r[0],m_r[2],m_r[3],(unsigned long long)cv1kFlowBranchHits,
		(unsigned long long)cv1kFlowSlotHits,(unsigned long long)cv1kFlowStackHits,(unsigned long long)cv1kFlowDTHits);
	return ms;
}
int main(int argc,char **){
	setvbuf(stdout,NULL,_IOLBF,0);
	for(auto key:{"FBNEO_CV1K_POLL","FBNEO_CV1K_TIMER_FAST","FBNEO_CV1K_DATA_LOOPS","FBNEO_CV1K_DATA_FULL","FBNEO_CV1K_INTERP_LOOPS"})
		setenv(key,"0",1);
	if(argc>1){
		for(unsigned kind=0;kind<3;kind++){
			double a[3],b[3];
			for(unsigned i=0;i<3;i++){
				if(i&1){b[i]=Bench(9,kind);a[i]=Bench(8,kind);}
				else{a[i]=Bench(8,kind);b[i]=Bench(9,kind);}
			}
			std::sort(a,a+3);std::sort(b,b+3);
			printf("flow median %u: %.3f -> %.3f ms\n",kind,a[1],b[1]);
		}
		return 0;
	}
	BranchTests();StackTests();DTTests();PureSlots();return 0;
}
