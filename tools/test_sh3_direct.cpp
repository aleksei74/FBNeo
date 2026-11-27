// Differential coverage of the actual batched mapped-memory and slot paths.
#define main PreviousInterpreterMain
#include "test_sh3_interpreter.cpp"
#undef main

static UINT8 __fastcall ByteRead(UINT32 a){Observe(4,a);return 0x83;}
static void __fastcall ByteWrite(UINT32 a,UINT8 v){Observe(5,a,v);Remap();}
static void __fastcall WordWrite(UINT32 a,UINT16 v){Observe(6,a,v);Remap();}
static UINT32 __fastcall IdleRead(UINT32 a){Observe(7,a);Sh3BurnCycles(11);return 0x89abcdef;}

struct DirectFixture {UINT16 op; unsigned width;bool store;Cv1kMoveForm form;};
static const DirectFixture fixtures[]={
	{0x2340,1,true,CV1K_REG},{0x2341,2,true,CV1K_REG},{0x2342,4,true,CV1K_REG},
	{0x6340,1,false,CV1K_REG},{0x6341,2,false,CV1K_REG},{0x6342,4,false,CV1K_REG},
	{0x2344,1,true,CV1K_PRE},{0x2345,2,true,CV1K_PRE},{0x2346,4,true,CV1K_PRE},
	{0x6344,1,false,CV1K_POST},{0x6345,2,false,CV1K_POST},{0x6346,4,false,CV1K_POST},
	{0x0344,1,true,CV1K_INDEX},{0x0345,2,true,CV1K_INDEX},{0x0346,4,true,CV1K_INDEX},
	{0x034c,1,false,CV1K_INDEX},{0x034d,2,false,CV1K_INDEX},{0x034e,4,false,CV1K_INDEX},
	{0xc011,1,true,CV1K_GBR},{0xc111,2,true,CV1K_GBR},{0xc211,4,true,CV1K_GBR},
	{0xc411,1,false,CV1K_GBR},{0xc511,2,false,CV1K_GBR},{0xc611,4,false,CV1K_GBR},
	{0x8043,1,true,CV1K_DISP4_R0},{0x8143,2,true,CV1K_DISP4_R0},{0x1343,4,true,CV1K_DISP4},
	{0x8443,1,false,CV1K_DISP4_R0},{0x8543,2,false,CV1K_DISP4_R0},{0x5343,4,false,CV1K_DISP4},
	{0x9311,2,false,CV1K_LITERAL},{0xd311,4,false,CV1K_LITERAL}
};

static State MoveRun(unsigned mode,DirectFixture f,unsigned kind,unsigned alias,bool slot,unsigned budget){
	Setup(mode,0);
	// Keep special-device callbacks separate for byte/word/long.
	ReadByte[0]=ReadByte[1]=ByteRead;
	ReadWord[0]=ReadWord[1]=ReadWordTest;
	WriteByte[0]=WriteByte[1]=ByteWrite;
	WriteWord[0]=WriteWord[1]=WordWrite;
	WriteLong[0]=WriteLong[1]=WriteTest;
	Sh3MapHandler(1,0xd000000,0xd00ffff,MAP_READ|MAP_WRITE);
	Sh3MapMemory((UINT8*)data,0xf0000000,0xf000ffff,MAP_RAM);
	Sh3MapMemory((UINT8*)data,0xc800000,0xc80ffff,MAP_RAM);
	// Use valid 2-byte aligned loads including the page's last longword.
	UINT32 ea=kind==0?0x8c000120:kind==1?0xac800124:kind==2?0x8d000120:
		kind==3?0x8c000121:kind==4?0xf0000120:kind==5?0x8c00fffc:0x8c000120;
	if(kind==6)Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);
	if(kind==6)ReadLong[1]=IdleRead;
	for(unsigned i=0;i<32770;i++)data[i]=UINT16(i*0x9e37+0x8abc);
	for(unsigned i=0;i<16;i++)m_r[i]=0x11223380u+i*0x1010101;
	m_ea=0x76543210;m_sr=1;m_pr=0x80000020;m_gbr=ea;
	UINT16 op=f.op;
	if(alias && (f.form==CV1K_REG || f.form==CV1K_PRE || f.form==CV1K_POST ||
		f.form==CV1K_INDEX || f.form==CV1K_DISP4))op=UINT16((op&0xf0ff)|((op&0xf0)<<4));
	const unsigned n=(op>>8)&15,m=(op>>4)&15;
	unsigned base=f.form==CV1K_DISP4_R0?m:f.store?n:m;
	if(f.form==CV1K_GBR)m_gbr=ea-(op&255)*f.width;
	else if(f.form!=CV1K_LITERAL){
		if(f.form==CV1K_INDEX)m_r[0]=0x10;
		m_r[base]=ea+(f.form==CV1K_PRE?f.width:0)-
			(f.form==CV1K_INDEX?m_r[0]:0)-
			((f.form==CV1K_DISP4 || f.form==CV1K_DISP4_R0)?(op&15)*f.width:0);
	}
	// A pure op before MOV checks deferred publication on a later
	// callback. Delay slots deliberately use the target-relative PC.
	code[0]=0x7a01;
	code[1]=slot?0xa006:op;
	code[2]=slot?op:0x7b01;
	code[9]=0x7b01;
	// Follow direct memory with a mapped-device load, to check PC,
	// PPC, EA, registers and exact cycle count at the callback.
	code[3]=code[10]=0x62f2;m_r[15]=0x8d000000;
	if(f.form==CV1K_LITERAL){
		unsigned index=((slot?0x80000012u:0x80000004u)+2)&AM;
		if(f.width==4)index&=~3u;
		index+=(op&255)*f.width;
		code[index/2]=0x91ab;code[index/2+1]=0xcdef;
	}
	int spent=Sh3Run_timerhack(budget);
	spent+=Sh3Run_timerhack(7);
	if(mode==8 && !slot && kind==0)Check(cv1kDirectMoveHits>0,"direct fixture did not accelerate");
	if(mode==8 && slot)Check(cv1kSlotDispatchHits>0,"slot fixture did not accelerate");
	return Save(spent);
}

static void Moves(){
	unsigned cases=0;
	for(auto f:fixtures)for(unsigned kind=0;kind<7;kind++)
	for(unsigned alias=0;alias<2;alias++)for(unsigned slot=0;slot<2;slot++){
		unsigned budget=1+(kind+alias+slot)%5;
		auto a=MoveRun(0,f,kind,alias,slot,budget);
		auto b=MoveRun(8,f,kind,alias,slot,budget);
		if(memcmp(&a,&b,sizeof(a))){
			fprintf(stderr,"MOVE mismatch opcode=%04x kind=%u alias=%u slot=%u budget=%u pc=%08x/%08x ea=%08x/%08x trace=%llx/%llx\n",
				f.op,kind,alias,slot,budget,a.pc,b.pc,a.ea,b.ea,(unsigned long long)a.trace,(unsigned long long)b.trace);
			exit(1);
		}
		cases++;
	}
	printf("SH3 direct MOV: %u actual slice comparisons (32 forms, alias, MMIO, unaligned, P4, page, slots) PASS\n",cases);
}
static void Branches(){
	// All pure slots plus common stateful slots, branch target, IRQ and
	// cycle publication are compared via a real delayed branch.
	bool pure[SH3_ID_COUNT]={};
#define SH3_OPCODE(name)
#define SH3_PURE_OPCODE(name) pure[SH3_ID_##name]=true;
#include "../src/cpu/sh4/sh3_decode_ops.inc"
#undef SH3_PURE_OPCODE
#undef SH3_OPCODE
	Setup(8,0);
	unsigned count=0;
	for(unsigned op=0;op<65536;op++)if(pure[sh3OpIds[op]]){
		State expected={};
		for(unsigned mode=0;mode<2;mode++){
			cv1kDirectMemEnabled=false;cv1kSlotDispatchEnabled=mode!=0;
			m_pc=0x80000000;m_delay=m_test_irq=m_pending_irq=m_cpu_off=0;
			m_ppc=m_ea=m_spc=m_ssr=0;m_sr=op&0x301;
			m_sh4_icount=sh3_total_cycles=0;trace=hits=0;
			m_mach=op;m_macl=~op;
			memset(m_rbnk,0,sizeof(m_rbnk));
			for(auto &t:m_timer){t={};t.timer_prescaler=64;}
			for(auto &t:m_dma_timer)t={};
			cave_blitter_delay={};
			for(unsigned i=0;i<16;i++)m_r[i]=0xdeadbeefu*(i+1)+op;
			code[0]=0xa004;code[1]=UINT16(op);code[6]=0x0009;
			auto got=Save(Sh3Run_timerhack(3));
			if(!mode)expected=got;else if(memcmp(&expected,&got,sizeof(got))){
				fprintf(stderr,"pure slot opcode=%04x id=%u pc=%08x/%08x sr=%08x/%08x ea=%08x/%08x total=%d/%d\n",op,unsigned(sh3OpIds[op]),expected.pc,got.pc,expected.sr,got.sr,expected.ea,got.ea,expected.total,got.total);
				const UINT8 *a=(const UINT8*)&expected,*b=(const UINT8*)&got;
				for(size_t i=0;i<sizeof(got);i++)if(a[i]!=b[i]){
					fprintf(stderr,"first difference offset=%zu %02x/%02x\n",i,a[i],b[i]);break;
				}
				Check(false,"pure slot state");
			}
		}
		count++;
	}
	printf("SH3 decoded delay slots: %u pure encodings PASS\n",count);
}
static void DirectBench(){
	// Synthetic RAM-heavy and delayed-branch loops. The test-only hit
	// counters are present, so these timings are not device FPS.
	for(unsigned kind=0;kind<2;kind++)for(unsigned mode:{5u,6u,7u,8u}){
		Setup(mode,0);
		const UINT16 mixed[]={0x6312,0x7301,0x2132,0x5411,0x2432,0x6341,0x6310,0x7301};
		for(unsigned i=0;i<65536;i++)code[i]=mixed[i&7];
		if(kind){
			code[0]=0x6312;code[1]=0x7301;code[2]=0x2132;
			code[3]=0xaffb;code[4]=0x7a01; // BRA to 0, ADD slot
		}
		m_r[1]=0x8c000100;m_r[4]=0x8c000200;
		// Use only fixed valid RAM bases; do not use R4 as a base
		// after MOV.L @(4,R1),R4 has overwritten it.
		if(!kind){const UINT16 ops[]={0x6312,0x7301,0x2132,0x5411,0x2142,0x6311,0x6310,0x7301};
			for(unsigned i=0;i<65536;i++)code[i]=ops[i&7];}
		m_pc=0x80000000;Sh3Run_timerhack(24);
		m_r[3]=m_r[10]=0;memset(data,0,sizeof(data));
		auto t=std::chrono::steady_clock::now();
		for(unsigned i=0;i<750;i++){m_pc=0x80000000;m_delay=0;Sh3Run_timerhack(20000);}
		printf("stage2 %s mode %u: %.3f ms checksum %08x/%08x moves %llu slots %llu\n",
			kind?"branch":"memory",mode,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count(),
			m_r[3],m_r[10],(unsigned long long)cv1kDirectMoveHits,(unsigned long long)cv1kSlotDispatchHits);
	}
}
int main(int argc,char **){
	for(auto key:{"FBNEO_CV1K_POLL","FBNEO_CV1K_TIMER_FAST","FBNEO_CV1K_DATA_LOOPS","FBNEO_CV1K_DATA_FULL","FBNEO_CV1K_INTERP_LOOPS"})
		setenv(key,"0",1);
	if(argc>1){DirectBench();return 0;}
	Moves();Branches();return 0;
}
