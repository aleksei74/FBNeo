// Exercise the actual FBNeo dispatcher and timer callbacks, no game ROM.
#define main PreviousInterpreterMain
#include "test_sh3_interpreter.cpp"
#undef main
#include <vector>
static UINT32 busyValue,changed;
static INT32 burnCycles;
static UINT32 __fastcall TestIdleRead(UINT32 a) {
	const UINT32 pc=Sh3GetPC(-1);
	if(a==0x0c000010 && (pc==cv1kIdlePC || pc==cv1kIdlePC+2))Sh3BurnCycles(burnCycles);
	UINT32 v;memcpy(&v,(UINT8*)data+(a&0xfffc),4);
#ifdef LSB_FIRST
	v=(v<<16)|(v>>16);
#endif
	return v;
}
static UINT32 __fastcall TestBusyRead(UINT32 a){
	if(a!=0x18000010){Observe(77,a);return 0;}
	if(!busyValue)Sh3BurnCycles(burnCycles);
	return busyValue;
}
static bool TestBusyPeek(UINT32 a,UINT32 *v,INT32 *extra){
	if(a!=0x18000010)return false;
	*v=busyValue;*extra=busyValue?0:burnCycles;return true;
}
static void SetLong(unsigned off,UINT32 v){
#ifdef LSB_FIRST
	v=(v<<16)|(v>>16);
#endif
	memcpy((UINT8*)data+off,&v,4);
}
static void ChangeRead(int param){
	Observe(100+param);
	changed++;
	if(param==1)busyValue=0x10;
	else SetLong(0x10,0x10);
}
struct PollResult {
	State state;
	UINT32 extra[5][5],changed,busy;
	Sh3Cv1kPollStats stats;
};
static void SaveTimer(UINT32 *s,const sh4_dtimer &t){
	s[0]=t.running;s[1]=t.time_current;s[2]=t.time_trig;s[3]=t.timer_prescaler;s[4]=t.prescale_counter;
}
static PollResult Run(bool fast,bool timerFast,unsigned kind,unsigned rotate,unsigned event,unsigned budget){
	setenv("FBNEO_CV1K_POLL",fast?"1":"0",1);
	setenv("FBNEO_CV1K_TIMER_FAST",timerFast?"1":"0",1);
	setenv("FBNEO_CV1K_INTERP_LOOPS","0",1);
	Setup(TestFetchMode(),0);
	Check(cv1kPollEnabled==fast && cv1kTimerFastEnabled==timerFast,"poll/timer policy");
	Sh3SetTimerGranularity(2);
	changed=0;busyValue=0;burnCycles=kind==5?41:0;
	// Leave deterministic code following the loop, including a
	// nonpolling store so early termination cannot pass by accident.
	const UINT16 simple[]={0x6012,0xc810,0x89fc,0xe601,0x7601,0x0009};
	const UINT16 delayed[]={0x6012,0xc810,0x8dfc,0x0009,0xe601,0x7601};
	const UINT16 restore[]={0x3028,0x3036,0x8dfc,0x6012,0xe601,0x7601};
	const UINT16 word[]={0x6011,0x88ff,0x89fc,0xe601,0x7601,0x0009};
	const UINT16 byte[]={0x6010,0x88ff,0x89fc,0xe601,0x7601,0x0009};
	const UINT16 always[]={0x6012,0xc810,0xaffc,0x0009,0xe601,0x7601};
	unsigned length=3;
	memcpy(code,simple,sizeof(simple));
	m_r[1]=0x8c000010;m_sr=T;m_r[0]=0;m_r[2]=1;m_r[3]=0;
	SetLong(0x10,0);
	switch(kind){
	case 1:memcpy(code,delayed,sizeof(delayed));length=4;break;
	case 2:memcpy(code,restore,sizeof(restore));length=4;m_r[0]=100;SetLong(0x10,100);break;
	case 3:memcpy(code,word,sizeof(word));m_r[0]=0xffffffff;data[8]=0xffff;break;
	case 4:memcpy(code,byte,sizeof(byte));m_r[0]=0xffffffff;data[8]=0xffff;break;
	case 5:Sh3MapReadLongHandler(1,0xc000000,0xc00ffff);Sh3SetReadLongHandler(1,TestIdleRead);
		Sh3SetCv1kIdleRead((UINT8*)data,0x0c000010,0,&burnCycles,TestIdleRead);break;
	case 6:case 7:
		memcpy(code,delayed,sizeof(delayed));length=4;
		m_r[1]=0x18000010;burnCycles=kind==6?0:41;
		Sh3MapReadLongHandler(2,0x18000000,0x1800ffff);
		Sh3SetReadLongHandler(2,TestBusyRead);Sh3SetCv1kPollRead(TestBusyRead,TestBusyPeek);break;
	case 8:memcpy(code,always,sizeof(always));length=4;break;
	case 9:code[0]=0x5012;m_r[1]-=8;break; // displacement load
	}
	// Set a legitimate rotated state by executing a short prefix
	// using the original path, including a pending delay-slot case.
	const bool enabled=cv1kPollEnabled;cv1kPollEnabled=false;
	for(unsigned i=0;i<rotate && i<length-1;i++)Sh3Run_timerhack(1);
	cv1kPollEnabled=enabled;
	cv1kPollStats={};
	if(event){
		// Timer, DMA and blitter callbacks occur in exactly the
		// original order after the full requested CPU slice.
		m_timer[0].timer_prescaler=1;m_timer[0].timer_exec=ChangeRead;
		m_timer[0].start(5,0,1,0);
		m_dma_timer[0].timer_exec=ChangeRead;m_dma_timer[0].start(7,2,1,0);
		cave_blitter_delay.timer_exec=ChangeRead;cave_blitter_delay.start(9,1,1,0);
	}
	int spent=0;
	for(unsigned i=0;i<4;i++){
		spent+=Sh3Run_timerhack(budget+i*3);
		if(i==1 && event==2)code[1]=0xe601; // live code write, must recheck
	}
	PollResult out={};out.state=Save(spent);
	for(unsigned i=0;i<4;i++)SaveTimer(out.extra[i],m_dma_timer[i]);
	SaveTimer(out.extra[4],cave_blitter_delay);
	out.changed=changed;out.busy=busyValue;out.stats=cv1kPollStats;
	return out;
}
static void Rejections(){
	for(unsigned c=0;c<8;c++){
		setenv("FBNEO_CV1K_POLL","1",1);Setup(TestFetchMode(),0);
		code[0]=0x6012;code[1]=0xc810;code[2]=0x89fc;code[3]=0x0009;
		m_sr=T;m_r[0]=0;m_r[1]=0x8c000010;m_sh4_icount=1000;
		switch(c){
		case 0:code[1]=0x2102;break; // write
		case 1:code[0]=0x6016;break; // postincrement
		case 2:m_r[1]=0x8c000011;break; // unaligned
		case 3:m_r[1]=0x8d000000;break; // arbitrary MMIO
		case 4:m_test_irq=1;break;
		case 5:m_delay=2;break;
		case 6:SetLong(0x10,0x10);break; // exits
		case 7:code[1]=0x7001;break; // state changes
		}
		const State a=Save(0);
		Check(!Cv1kTryPoll(),"unsafe poll accepted");
		const State b=Save(0);
		Check(!memcmp(&a,&b,sizeof(a)),"poll rejection changed state");
	}
	puts("SH3 poll: write, postincrement, alignment, MMIO, IRQ, delay, exit and changing state rejected");
}
static sh4_dtimer *active;
static UINT32 timerCalls;
static void RestartTimer(int arg){
	timerCalls=timerCalls*13+arg+1;
	active->timer_prescaler=3+(arg&7);
	active->start(5,(arg+1)&7,1,0);
}
static void TimerDifferential(){
	UINT32 rng=0x91e47b23;
	auto next=[&](){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;};
	unsigned callbackCases=0;
	for(unsigned c=0;c<24000;c++){
		sh4_dtimer a={};a.timer_prescaler=1+next()%100000;
		a.prescale_counter=next()%a.timer_prescaler;
		a.time_trig=1+next()%200;a.time_current=next()%a.time_trig;
		a.running=next()%2;a.retrig=next()%2;a.timer_param=next()%8;
		a.timer_exec=RestartTimer;
		if(c%5==0){a.timer_prescaler=1+next()%15;a.prescale_counter%=a.timer_prescaler;}
		sh4_dtimer b=a;
		const INT32 cycles=next()%201;
		m_ratio=1+next()%13;
		timerCalls=0;active=&a;a.run_prescale(cycles);const auto trace=timerCalls;
		timerCalls=0;active=&b;b.run_prescale_fast(cycles);
		Check(a.running==b.running && a.time_trig==b.time_trig &&
			a.time_current==b.time_current && a.timer_param==b.timer_param &&
			a.timer_prescaler==b.timer_prescaler && a.prescale_counter==b.prescale_counter &&
			a.retrig==b.retrig && trace==timerCalls,"prescale/event callback mismatch");
		if(trace)callbackCases++;
	}
	printf("SH3 timer fast advance: 24000 differential cases, %u expiry/restart cases PASS\n",callbackCases);
}
int main(){
	setenv("FBNEO_CV1K_DATA_LOOPS","0",1);
	setenv("FBNEO_CV1K_DATA_FULL","0",1);
	UINT64 hits=0,rounds=0,ram=0,idle=0,device=0;
	unsigned cases=0;
	const unsigned budgets[]={1,2,3,7,8,11,23,24,25,31,63,64,79,127,512,7100};
	for(unsigned kind=0;kind<10;kind++)for(unsigned r=0;r<3;r++)
	for(unsigned event=0;event<3;event++)for(auto budget:budgets){
		const auto a=Run(false,false,kind,r,event,budget);
		const auto b=Run(true,true,kind,r,event,budget);
		if(memcmp(&a.state,&b.state,sizeof(a.state)) || memcmp(a.extra,b.extra,sizeof(a.extra)) ||
			a.changed!=b.changed || a.busy!=b.busy){
			fprintf(stderr,"poll mismatch kind=%u rotation=%u event=%u budget=%u pc=%08x/%08x cycles=%d/%d hits=%llu\n",
				kind,r,event,budget,a.state.pc,b.state.pc,a.state.spent,b.state.spent,(unsigned long long)b.stats.hits);return 1;
		}
		hits+=b.stats.hits;rounds+=b.stats.rounds;
		ram+=b.stats.ramHits;idle+=b.stats.idleHits;device+=b.stats.deviceHits;cases++;
	}
	Check(hits>100 && rounds>1000 && ram && idle && device,"poll fast path coverage");
	printf("SH3 poll: %u real slice comparisons, %llu hits, %llu rounds (RAM %llu idle %llu device %llu) PASS\n",
		cases,(unsigned long long)hits,(unsigned long long)rounds,(unsigned long long)ram,(unsigned long long)idle,(unsigned long long)device);
	Rejections();TimerDifferential();
	return 0;
}
