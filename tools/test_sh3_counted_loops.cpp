// Actual timer-sliced interpreter regression. No JIT required.
#define main Cv1kInterpreterPreviousMain
#include "test_sh3_interpreter.cpp"
#undef main
#include <vector>
static UINT16 extraPage[32768];
static UINT32 rng=0x98ef2321;
static UINT32 Next(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
struct LoopResult {
	State state;
	UINT64 extraHash,codeHash,blocks,laps;
};
static UINT64 Hash(const UINT16 *p,size_t n){
	UINT64 h=1469598103934665603ull;
	for(size_t i=0;i<n;i++)h=(h^p[i])*1099511628211ull;
	return h;
}
static unsigned Program(unsigned kind,unsigned w,UINT16 *p,bool delayed=false){
	if(delayed){
		switch(kind){
		case 0:p[0]=0x4310;p[1]=0x8ffd;p[2]=0x72f9;return 3;
		case 1:p[0]=0x324c;p[1]=0x4310;p[2]=0x8ffc;p[3]=0x0009;return 4;
		case 2:p[0]=0x6104+w;p[1]=0x2210+w;p[2]=0x4310;p[3]=0x8ffb;p[4]=0x7200+(1u<<w);return 5;
		case 3:p[0]=0x2210+w;p[1]=0x4310;p[2]=0x8ffc;p[3]=0x7200+(1u<<w);return 4;
		default:p[0]=0x2214+w;p[1]=0x4310;p[2]=0x8ffc;p[3]=0x0009;return 4;
		}
	}
	if(kind==0){
		p[0]=0x72f9;p[1]=0x4310;p[2]=0x8bfc;return 3;
	}
	if(kind==1){
		p[0]=0x324c;p[1]=0x4310;p[2]=0x8bfc;return 3;
	}
	if(kind==2){
		p[0]=0x6104+w;p[1]=0x2210+w;p[2]=0x7200+(1u<<w);
		p[3]=0x4310;p[4]=0x8bfa;return 5;
	}
	if(kind==3){
		p[0]=0x2210+w;p[1]=0x7200+(1u<<w);p[2]=0x4310;p[3]=0x8bfb;return 4;
	}
	p[0]=0x2214+w;p[1]=0x4310;p[2]=0x8bfc;return 3;
}
static LoopResult RunLoop(bool fast,unsigned kind,unsigned w,unsigned scenario,unsigned seed,unsigned budget,bool delayed){
	setenv("FBNEO_CV1K_INTERP_LOOPS",fast?"1":"0",1);
	Setup(fast?2:0,0);
	Check(cv1kInterpLoopsEnabled==fast,"interpreter loop policy");
	Sh3MapMemory((UINT8*)extraPage,0xc010000,0xc01ffff,MAP_RAM);
	rng=seed^0x98ef2321;
	for(auto &v:data)v=Next();
	for(auto &v:extraPage)v=Next();
	const unsigned width=1u<<w;
	const unsigned len=Program(kind,w,code,delayed);
	code[len]=0xe57f; // ordinary instruction after terminal lap
	m_r[0]=0x8c000400;m_r[1]=Next();m_r[2]=0x8c001000;
	m_r[3]=2+Next()%127;m_r[4]=Next();
	m_sr=Next()&0x301;
	switch(scenario){
	case 1: m_r[2]=0xac000400+width;break; // forward overlapping alias
	case 2: m_r[2]=0xac000400-width;break; // backward overlapping alias
	case 3: m_r[2]=0xac000400;break; // exact alias
	case 4: m_r[0]=0x8c00fffc;m_r[2]=0xac00fff8;break; // cross pages
	case 5: m_r[2]=0xac010000;break; // reverse predec from boundary
	case 6: // odd byte address, mismatched host byte lanes
		if(w==0){m_r[0]++;m_r[2]+=2;}break;
	case 7: // distinct code read map whose BF differs
		memcpy(replacement,code,sizeof(code));
		replacement[len-1-unsigned(delayed)]=0x0009;
		Sh3MapMemory((UINT8*)replacement,0,0x1ffff,MAP_READ);
		break;
	}
	// Keep the legacy unaligned/cross-page fixture in range.
	if(kind==4 && scenario==4)m_r[2]=0x8c010008;
	int spent=0;
	for(unsigned i=0;i<4;i++)spent+=Sh3Run_timerhack(i==0?budget:7+(seed+i*13)%127);
	LoopResult out={};out.state=Save(spent);
	out.extraHash=Hash(extraPage,32768);out.codeHash=Hash(code,65536);
	out.blocks=cv1kInterpLoopBlocks;out.laps=cv1kInterpLoopLaps;
	return out;
}
static void RejectCases(){
	for(unsigned c=0;c<10;c++){
		setenv("FBNEO_CV1K_INTERP_LOOPS","1",1);Setup(2,0);
		unsigned len=Program(2,2,code);
		m_r[0]=0x8c000100;m_r[1]=0x11223344;m_r[2]=0x8c001000;m_r[3]=40;
		m_pc=0x80000000;m_ppc=m_pc+len*2;m_ea=m_pc;
		m_sr&=~T;m_sh4_icount=100;
		switch(c){
		case 0:Sh3MapMemory((UINT8*)code,0xc000000,0xc00ffff,MAP_WRITE);m_r[2]=0xac000000;break;
		case 1:Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);break;
		case 2:Sh3MapHandler(1,0xc000000,0xc00ffff,MAP_WRITE);break;
		case 3:m_r[2]++;break;
		case 4:m_r[0]=0x8c00fffc;break;
		case 5:m_test_irq=1;break;
		case 6:m_delay=4;break;
		case 7:m_r[3]=2;break;
		case 8: // Only the DT data read would touch the alternate code mapping.
			memcpy(replacement,code,sizeof(code));Sh3MapMemory((UINT8*)replacement,0,0x1ffff,MAP_READ);
			Sh3MapMemory((UINT8*)replacement,0xc000000,0xc00ffff,MAP_WRITE);
			m_r[2]=0x8c000004;break;
		case 9:Sh3MapHandler(1,0,0xffff,MAP_READ);break;
		}
		const State before=Save(0);
		const UINT64 codeBefore=Hash(code,65536),otherBefore=Hash(replacement,65536);
		Check(!Cv1kTryInterpLoop(code[len-1]),"unsafe loop accepted");
		const State after=Save(0);
		Check(!memcmp(&before,&after,sizeof(before)) && codeBefore==Hash(code,65536) &&
			otherBefore==Hash(replacement,65536) && cv1kInterpLoopBlocks==0,"rejected loop mutated state");
	}
	puts("SH3 counted loops: 10 MMIO, alias, alignment, page, IRQ, delay and counter guards PASS");
}
int main(){
	setenv("FBNEO_CV1K_DATA_LOOPS","0",1);
	setenv("FBNEO_CV1K_DATA_FULL","0",1);
	setenv("FBNEO_CV1K_POLL","0",1);
	setenv("FBNEO_CV1K_TIMER_FAST","0",1);
	UINT64 laps=0,blocks=0,delayLaps=0,delayBlocks=0;
	unsigned cases=0;
	const unsigned budgets[]={0,1,2,3,4,5,7,8,9,10,12,13,17,64,256,1024};
	for(unsigned delayed=0;delayed<2;delayed++)for(unsigned kind=0;kind<5;kind++)for(unsigned w=0;w<3;w++)
	for(unsigned scenario=0;scenario<8;scenario++)for(unsigned seed=0;seed<16;seed++){
		const unsigned budget=budgets[seed];
		const auto a=RunLoop(false,kind,w,scenario,seed+kind*31,budget,delayed);
		const auto b=RunLoop(true,kind,w,scenario,seed+kind*31,budget,delayed);
		if(memcmp(&a.state,&b.state,sizeof(a.state)) || a.extraHash!=b.extraHash || a.codeHash!=b.codeHash){
			fprintf(stderr,"counted loop mismatch kind=%u width=%u scenario=%u seed=%u budget=%u pc=%08x/%08x cycles=%d/%d laps=%llu\n",kind,1u<<w,scenario,seed,budget,a.state.pc,b.state.pc,a.state.spent,b.state.spent,(unsigned long long)b.laps);
			return 1;
		}
		laps+=b.laps;blocks+=b.blocks;cases++;
		if(delayed){delayLaps+=b.laps;delayBlocks+=b.blocks;}
	}
	Check(laps>1000 && blocks>100 && delayLaps>1000 && delayBlocks>100,"counted loop fast path did not execute");
	printf("SH3 counted interpreter loops: %u actual dispatch comparisons, %llu fused laps in %llu blocks PASS\n",cases,(unsigned long long)laps,(unsigned long long)blocks);
	printf("SH3 delay-slot loop coverage: %llu fused laps in %llu blocks PASS\n",(unsigned long long)delayLaps,(unsigned long long)delayBlocks);
	RejectCases();
	return 0;
}
