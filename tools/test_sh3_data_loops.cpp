// Standalone differential test of production SH3 data-loop dispatch.
// Includes the literal-copy instruction shape observed in mmpork,
// but uses synthetic input only, with varying addresses and registers.
#define main PreviousInterpreterMain
#include "test_sh3_interpreter.cpp"
#undef main
#include <algorithm>
static UINT16 extraPage[32770];
static UINT32 rng;
static UINT32 Next(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static UINT64 Hash(const UINT16 *p,size_t n){
	UINT64 h=1469598103934665603ull;
	for(size_t i=0;i<n;i++)h=(h^p[i])*1099511628211ull;
	return h;
}
struct Regs{unsigned flag,src,val,dst,count,bits,tmp;};
static unsigned Program(UINT16 *p,unsigned kind,unsigned width,const Regs &r){
	if(kind==1){
		p[0]=0x4011|(r.flag<<8);p[1]=0x8b35;
		p[2]=0x6004|(r.val<<8)|(r.src<<4);p[3]=0x2000|(r.dst<<8)|(r.val<<4);
		p[4]=0x7001|(r.dst<<8);p[5]=0x4010|(r.count<<8);
		p[6]=0x8d20;p[7]=0x6003|(r.tmp<<8)|(r.flag<<4);
		p[8]=0x300c|(r.tmp<<8)|(r.tmp<<4);p[9]=0x4010|(r.bits<<8);
		p[10]=0x8ff4;p[11]=0x600e|(r.flag<<8)|(r.tmp<<4);
		return 12;
	}
	const unsigned size=width==1?0:width==2?1:2;
	p[0]=0x6004+size+(r.val<<8)+(r.src<<4);
	p[1]=0x2000+size+(r.dst<<8)+(r.val<<4);
	p[kind==3?4:2]=0x7000|(r.dst<<8)|width;
	p[kind==3?2:3]=0x4010|(r.count<<8);
	p[kind==3?3:4]=kind==3?0x8ffb:0x8bfa;
	return 5;
}
static void Configure(bool fast){
	setenv("FBNEO_CV1K_DATA_LOOPS",fast?"1":"0",1);
	setenv("FBNEO_CV1K_INTERP_LOOPS","0",1);
	setenv("FBNEO_CV1K_POLL","0",1);
	setenv("FBNEO_CV1K_TIMER_FAST","0",1);
	Setup(2,0);Sh3SetTimerGranularity(2);
	Check(cv1kDataLoopsEnabled==fast,"data-loop policy");
}
struct Result{State state;UINT64 extra,codeHash;Sh3Cv1kDataStats stats;};
static Result Run(bool fast,unsigned kind,unsigned width,unsigned scenario,unsigned seed,unsigned budget){
	Configure(fast);
	Sh3MapMemory((UINT8*)extraPage,0xc010000,0xc01ffff,MAP_RAM);
	rng=seed+0x9e3779b9u;
	for(auto &v:data)v=Next();
	for(auto &v:extraPage)v=Next();
	unsigned regs[16];for(unsigned i=0;i<16;i++)regs[i]=i;
	if(seed)for(unsigned i=15;i;i--){const unsigned j=Next()%(i+1);std::swap(regs[i],regs[j]);}
	const Regs r=seed?Regs{regs[0],regs[1],regs[2],regs[3],regs[4],regs[5],regs[6]}:
		Regs{7,5,2,3,6,4,0};
	// Actual opcode shape and arbitrary live RAM address. Cross-page
	// instruction fetch is tested separately from cross-page data.
	unsigned head=seed?0x1200+(seed%64)*2:0xaf90;
	if(scenario==7)head=0xfffc;
	Sh3MapMemory((UINT8*)code,0xc060000,0xc07ffff,MAP_RAM);
	m_pc=0x8c060000+head;
	const unsigned len=Program(code+head/2,kind,width,r);
	code[head/2+len]=0xe001;
	for(auto &v:m_r)v=Next();
	m_r[r.src]=0x8c000400;m_r[r.dst]=0x8c001000;m_r[r.count]=2+Next()%127;
	const UINT32 flag[]={0,1,3,0x40,0x7f,0xffffff80,0xffffffff,0};
	m_r[r.flag]=flag[seed%8];m_r[r.bits]=seed%7==0?1:8+(seed%3);
	m_sr=Next()&0x303;
	switch(scenario){
	case 1:m_r[r.dst]=0xac000400+width;break; // overlapping forward copy
	case 2:m_r[r.dst]=0xac000400-width;break;
	case 3:m_r[r.dst]=0xac000400;break; // physical alias
	case 4:m_r[r.src]=0x8c00fffc;m_r[r.dst]=0xac00fff8;break;
	case 5:if(width==1){m_r[r.src]++;m_r[r.dst]+=2;}break;
	case 6: // alternate data mapping for DT: must read actual next word
		memcpy(replacement,code,sizeof(code));
		replacement[head/2+(kind==1?6:kind==3?3:4)]=0x0009;
		Sh3MapMemory((UINT8*)replacement,0xc060000,0xc07ffff,MAP_READ);break;
	case 8: // real timer boundary, restart and IRQ handled normally
		m_timer[0].timer_prescaler=1;m_timer[0].timer_exec=TimerTest;
		m_timer[0].start(19,0,1,0);break;
	}
	// Force direct paths and short tails on a variety of budgets.
	int spent=0;
	for(unsigned i=0;i<3;i++){
		spent+=Sh3Run_timerhack(budget+i*3);
	}
	Result out={};out.state=Save(spent);
	out.extra=Hash(extraPage,32770);out.codeHash=Hash(code,65536);out.stats=cv1kDataStats;
	return out;
}
static void Rejections(){
	const Regs r={7,5,2,3,6,4,0};
	for(unsigned c=0;c<12;c++){
		Configure(true);
		Program(code,1,1,r);m_pc=0x80000000;
		m_r[7]=0;m_r[4]=8;m_r[6]=100;m_r[5]=0x8c000100;m_r[3]=0x8c001000;
		m_sh4_icount=1000;
		switch(c){
		case 0:Sh3MapMemory((UINT8*)code,0xc000000,0xc00ffff,MAP_WRITE);m_r[3]=0xac000000;break;
		case 1:Sh3MapHandler(1,0xc000000,0xc00ffff,MAP_WRITE);break;
		case 2:Sh3MapHandler(1,0xc000000,0xc00ffff,MAP_READ);break;
		case 3:m_r[7]=0xffffff80;break;
		case 4:m_r[4]=1;break;
		case 5:m_r[6]=1;break;
		case 6:m_test_irq=1;break;
		case 7:m_delay=2;break;
		case 8:memcpy(replacement,code,sizeof(code));replacement[6]=0x8bfd;
			Sh3MapMemory((UINT8*)replacement,0,0x1ffff,MAP_READ);break;
		case 9:memcpy(replacement,code,sizeof(code));
			Sh3MapMemory((UINT8*)replacement,0,0x1ffff,MAP_READ);
			Sh3MapMemory((UINT8*)replacement,0xc000000,0xc00ffff,MAP_WRITE);
			m_r[3]=0xac00000c;break;
		case 10:code[8]=0x300c|0x0010;break; // different scratch dependency
		case 11:code[3]=0x2520;break; // source/dst alias and wrong increment
		}
		const State a=Save(0);const UINT64 ch=Hash(code,65536),rh=Hash(replacement,65536);
		Check(!Cv1kTryDataLoop(1),"unsafe data-loop accepted");
		const State b=Save(0);
		Check(!memcmp(&a,&b,sizeof(a)) && ch==Hash(code,65536) &&
			rh==Hash(replacement,65536),"rejection changed guest state");
	}
	Configure(true);
	Program(code,1,1,r);m_pc=0x80000000;
	m_r[7]=0;m_r[4]=8;m_r[6]=100;m_r[5]=0x8c000100;m_r[3]=0x8c001000;m_sh4_icount=1000;
	Check(Cv1kTryDataLoop(1),"positive cache fixture");
	m_r[4]=8;m_r[6]=100;m_sh4_icount=1000;
	Check(Cv1kTryDataLoop(1) && cv1kDataStats.cached==1,"cache hit");
	code[8]=0x0009;
	const State a=Save(0);Check(!Cv1kTryDataLoop(1),"stale decoded pattern accepted");
	const State b=Save(0);Check(!memcmp(&a,&b,sizeof(a)),"stale pattern changed state");
	puts("SH3 data loops: 12 unsafe patterns + positive cache/live code invalidation PASS");
}
static double Bench(bool fast,unsigned kind){
	Configure(fast);
	const Regs r={7,5,2,3,6,4,0};
	Program(code,kind,1,r);
	for(unsigned i=0;i<sizeof(data)/2;i++)data[i]=i;
	const unsigned n=100000,budget=kind==1?91:7*128;
	auto begin=std::chrono::steady_clock::now();
	for(unsigned i=0;i<n;i++){
		m_pc=0x80000000;m_delay=0;m_test_irq=0;m_sr=0;
		m_r[7]=0;m_r[4]=8;m_r[6]=kind==1?8:129;
		m_r[5]=0x8c000400;m_r[3]=0x8c001000;
		Sh3Run_timerhack(budget);
	}
	auto end=std::chrono::steady_clock::now();
	if(fast)Check(cv1kDataStats.hits==n,"benchmark fast-path coverage");
	return std::chrono::duration<double,std::milli>(end-begin).count();
}
int main(){
	UINT64 literal=0,copy=0,hits=0,cached=0;unsigned cases=0;
	const unsigned budgets[]={1,2,3,5,7,11,12,13,14,23,26,39,64,129,512,1024};
	for(unsigned kind=1;kind<=3;kind++)for(unsigned width:{1u,2u,4u}){
		if(kind==1 && width!=1)continue;
		for(unsigned scenario=0;scenario<9;scenario++)for(unsigned seed=0;seed<16;seed++){
			const auto a=Run(false,kind,width,scenario,seed,budgets[seed]);
			const auto b=Run(true,kind,width,scenario,seed,budgets[seed]);
			if(memcmp(&a.state,&b.state,sizeof(a.state)) || a.extra!=b.extra || a.codeHash!=b.codeHash){
				fprintf(stderr,"data mismatch kind=%u width=%u scenario=%u seed=%u budget=%u pc=%08x/%08x cycles=%d/%d hits=%llu\n",
					kind,width,scenario,seed,budgets[seed],a.state.pc,b.state.pc,a.state.spent,b.state.spent,(unsigned long long)b.stats.hits);return 1;
			}
			literal+=b.stats.literalRounds;copy+=b.stats.copyRounds;hits+=b.stats.hits;cached+=b.stats.cached;cases++;
		}
	}
	Check(literal && copy && hits && cached,"data-loop path coverage");
	printf("SH3 data loops: %u actual dispatcher comparisons, %llu literal rounds, %llu copy rounds, %llu hits, %llu cached PASS\n",
		cases,(unsigned long long)literal,(unsigned long long)copy,(unsigned long long)hits,(unsigned long long)cached);
	Rejections();
	for(unsigned kind:{1u,2u}){
		const double off=Bench(false,kind),on=Bench(true,kind);
		printf("Synthetic %s loop benchmark (100000 slices): off %.2f ms on %.2f ms (host/QEMU only)\n",
			kind==1?"literal":"copy",off,on);
	}
	return 0;
}
