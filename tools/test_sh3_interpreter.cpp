// Standalone differential tests of the production SH3 interpreter, no JIT.
#define SH3_INTERPRETER_TEST 1
#include "../src/cpu/sh4/sh4.cpp"
#ifdef fprintf
#undef fprintf
#endif
#include <chrono>
static INT32 __cdecl NoScan(BurnArea *) { return 0; }
INT32 (__cdecl *BurnAcb)(BurnArea *) = NoScan;
void CpuCheatRegister(INT32, cpu_core_config *) {}
#ifndef bprintf
static INT32 __cdecl NoLog(INT32, TCHAR *, ...) { return 0; }
INT32 (__cdecl *bprintf)(INT32, TCHAR *, ...) = NoLog;
#endif
static void Check(bool b,const char *s) { if(!b){fprintf(stderr,"FAIL: %s\n",s);exit(1);} }
static UINT16 code[65536],replacement[65536],data[32770];
static UINT64 trace;
static unsigned hits,scenario;
static void Observe(unsigned event,UINT32 address=0,UINT32 value=0) {
    const UINT32 state[]={event,address,value,m_pc,m_ppc,m_ea,m_delay,m_sr,
        UINT32(m_sh4_icount),UINT32(Sh3TotalCycles()),Sh3GetPC(-1)};
    for(auto v:state)trace=(trace^v)*1099511628211ull;
    hits++;
}
static void Remap() { Sh3MapMemory((UINT8*)replacement,0,0x1ffff,MAP_FETCHOP); }
static UINT32 __fastcall ReadTest(UINT32 a) {
    Observe(1,a);
    if(scenario==1) Remap();
    if(scenario==2) {Remap();Sh3SetIRQLine(2,CPU_IRQSTATUS_HOLD);}
    if(scenario==3) Sh3BurnCycles(19);
    return 0x12345678;
}
static void __fastcall WriteTest(UINT32 a,UINT32 v) {Observe(2,a,v);Remap();}
static UINT16 __fastcall ReadWordTest(UINT32 a) {Observe(3,a);return 0x5678;}
static void TimerTest(int param) {
    Observe(10+param);
    if(param==0) {
        // Restart and change prescaler inside callback, while ticks remain.
        m_timer[0].timer_prescaler=3;
        m_timer[0].start(4,1,1,0);
        Remap();
        Sh3SetIRQLine(2,CPU_IRQSTATUS_HOLD);
    }
}
struct State {
    UINT32 r[16],bank[2][8],pc,ppc,ea,sr,delay,spc,ssr,mach,macl;
    int total,left,spent;
    UINT64 trace;
    unsigned hits;
    UINT32 timers[3][5];
    UINT16 memory[32770];
};
static void Setup(unsigned mode,unsigned test) {
    scenario=test;trace=0;hits=0;
    setenv("FBNEO_CV1K_JIT","0",1);
    setenv("FBNEO_CV1K_DISPATCH",mode?"1":"0",1);
    setenv("FBNEO_CV1K_COMPACT","1",1);
    setenv("FBNEO_CV1K_BATCH","1",1);
    setenv("FBNEO_CV1K_FETCH_PAGE",mode>=2?"1":"0",1);
    setenv("FBNEO_CV1K_DISPATCH8",mode==3 || mode==4?"1":"0",1);
    setenv("FBNEO_CV1K_FETCH_REUSE",mode>=4?"1":"0",1);
    Sh3MapInit();Sh3SetCv1kNative(1);
    Check(!cv1kNativeEnabled && cv1kFetchPageEnabled==(mode>=2) &&
        cv1kDispatch8Enabled==(mode==3 || mode==4) && cv1kSlowFetchReuseEnabled==(mode>=4),"policy");
    for(unsigned i=0;i<65536;i++){code[i]=0x7001;replacement[i]=0x7203;}
    memset(data,0,sizeof(data));
    Sh3MapMemory((UINT8*)code,0,0x1ffff,MAP_RAM);
    Sh3MapMemory((UINT8*)data,0xc000000,0xc00ffff,MAP_RAM);
    Sh3MapReadLongHandler(1,0xd000000,0xd00ffff);
    Sh3SetReadLongHandler(1,ReadTest);
    // Pure instructions may change R15 before the trailing load. Provide
    // the default unmapped-memory callback just as a real core init does.
    Sh3SetReadLongHandler(0,ReadTest);
    Sh3SetReadWordHandler(1,ReadWordTest);
    Sh3MapHandler(1,0xd000000,0xd00ffff,MAP_WRITE);
    Sh3SetWriteLongHandler(1,WriteTest);
    m_cpu_type=CPU_TYPE_SH3;m_pc=0x80000000;m_vbr=0x80000000;
    m_ppc=m_ea=m_sr=m_delay=m_spc=m_ssr=m_mach=m_macl=0;
    m_test_irq=m_pending_irq=m_cpu_off=0;sh3_total_cycles=m_sh4_icount=0;
    memset(m_r,0,sizeof(m_r));memset(m_rbnk,0,sizeof(m_rbnk));
    memset(m_irq_line_state,0,sizeof(m_irq_line_state));
    memset(m_exception_requesting,0,sizeof(m_exception_requesting));
    memset(m_exception_priority,0,sizeof(m_exception_priority));
    m_exception_priority[SH4_INTC_IRL0+2-SH4_IRL0]=15<<8;
    m_r[15]=0x8d000000;m_r[14]=0x8c000000;
    m_ratio=1;
    for(auto &t:m_timer){t={};t.timer_prescaler=64;}
    for(auto &t:m_dma_timer)t={};
    cave_blitter_delay={};
}
// Other standalone harnesses can opt into the same dispatcher without
// changing the meaning of their loop/poll A/B modes.
static unsigned TestFetchMode() {
    const char *s=getenv("FBNEO_CV1K_TEST_FETCH_MODE");
    return s && s[0]>='2' && s[0]<='5' && !s[1] ? unsigned(s[0]-'0') : 2;
}
static State Save(int spent) {
    State s={};memcpy(s.r,m_r,sizeof(m_r));memcpy(s.bank,m_rbnk,sizeof(m_rbnk));
    s.pc=m_pc;s.ppc=m_ppc;s.ea=m_ea;s.sr=m_sr;s.delay=m_delay;s.spc=m_spc;s.ssr=m_ssr;
    s.mach=m_mach;s.macl=m_macl;s.total=sh3_total_cycles;s.left=m_sh4_icount;s.spent=spent;
    s.trace=trace;s.hits=hits;memcpy(s.memory,data,sizeof(data));
    for(unsigned i=0;i<3;i++) {
        const auto &t=m_timer[i];
        s.timers[i][0]=t.running;s.timers[i][1]=t.time_current;s.timers[i][2]=t.time_trig;
        s.timers[i][3]=t.timer_prescaler;s.timers[i][4]=t.prescale_counter;
    }
    return s;
}
static State Run(unsigned mode,unsigned test,unsigned seed) {
    Setup(mode,test);
    const unsigned prefix=seed%31;
    switch(test) {
    case 0: // page edge; live rewrite between slices
        m_pc=0x8000ffe0;break;
    case 1: case 2: case 3: // MMIO remap, IRQ, extra cycles
        code[prefix]=0x61f2;break;
    case 4: // slow write remaps instruction page
        code[prefix]=0x2f02;break;
    case 5: // alias SMC: store replaces the very next instruction
        Sh3MapMemory((UINT8*)code,0xc000000,0xc00ffff,MAP_WRITE);
        m_r[14]=0xac000000+(prefix+1)*2;m_r[1]=0xe77f;
        code[prefix]=0x2e11;break;
    case 6: // delay slot MMIO remaps before target fetch
        scenario=1;code[prefix]=0xa004;code[prefix+1]=0x61f2;break;
    case 7: // delayed branch crosses page, preserved PC/PPC at callback
        scenario=2;m_pc=0x8000fffe;code[32767]=0xa004;code[32768]=0x61f2;break;
    case 8: // event callback restarts timer, changes map, requests IRQ
        m_timer[0].timer_prescaler=1;m_timer[0].timer_exec=TimerTest;
        m_timer[0].start(5,0,1,0);
        m_dma_timer[0].timer_exec=TimerTest;m_dma_timer[0].start(7,2,1,0);
        cave_blitter_delay.timer_exec=TimerTest;cave_blitter_delay.start(9,3,1,0);break;
    case 9: // exact normal timer mode must remain untouched
        m_timer[0].timer_prescaler=1;m_timer[0].timer_exec=TimerTest;
        m_timer[0].start(5,0,1,0);break;
    case 10: // direct load/store widths and pre/post increment, separate long map
        { const UINT16 ops[]={0x2e00,0x2e11,0x2e22,0x63e0,0x64e1,0x65e2,
            0x66e4,0x67e5,0x68e6,0x2e04,0x2e15,0x2e26,0x59e0,0x1e93};
          memcpy(code+prefix,ops,sizeof(ops));m_r[0]=0xa5;m_r[1]=0x7654;m_r[2]=0x12345678;
          m_r[14]+=0x100;Sh3MapReadLongHandler(1,0xc000000,0xc00ffff); }
        break;
    case 11: // pending IRQ on entry
        Sh3SetIRQLine(2,CPU_IRQSTATUS_HOLD);break;
    case 12: // CPU-off bypass, no timer work
        m_cpu_off=1;break;
    case 13: // page-masked guest address wrap
        Sh3MapMemory((UINT8*)code,0x1fff0000,0x1fffffff,MAP_RAM);
        m_pc=0x9fffffe0;break;
    }
    int spent=0;
    for(unsigned i=0;i<7;i++) {
        if(test==0 && i==2)code[(m_pc&AM)>>1]=0xe77f;
        // Include zero budget: the legacy do/while still executes one op.
        const int budget=(i==0 && seed==0)?0:1+(seed*7+i*11)%47;
        spent+=test==9?Sh3Run_normal(budget):Sh3Run_timerhack(budget);
    }
    if(mode && test!=9 && test!=12)Check(cv1kBatchActive,"batch did not execute");
    if(mode>=2 && test!=9 && test!=12)Check(cv1kFetchPageActive,"fetch-page path did not execute");
    if((mode==3 || mode==4) && test!=9 && test!=12)Check(cv1kDispatch8Active,"dense path did not execute");
    return Save(spent);
}
static void TestPure() {
    bool pure[SH3_ID_COUNT]={};
#define SH3_OPCODE(name)
#define SH3_PURE_OPCODE(name) pure[SH3_ID_##name]=true;
#include "../src/cpu/sh4/sh3_decode_ops.inc"
#undef SH3_PURE_OPCODE
#undef SH3_OPCODE
    unsigned count=0;
    Setup(2,0);
    for(unsigned op=0;op<65536;op++)if(pure[sh3OpIds[op]]) {
        State expected={};
        for(unsigned mode=0;mode<6;mode++) {
            // A varied register state plus a trailing MMIO callback checks
            // both pure-op results and publication before the next handler.
            cv1kFlatEnabled=cv1kBatchEnabled=mode!=0;
            cv1kFetchPageEnabled=mode>=2;
            cv1kDispatch8Enabled=mode==3 || mode==4;
            cv1kSlowFetchReuseEnabled=mode>=4;
            m_pc=0x80000000;m_ppc=m_ea=m_delay=0;
            cv1kFetchPageActive=false;cv1kDispatch8Active=false;
            m_test_irq=m_pending_irq=m_cpu_off=0;
            sh3_total_cycles=m_sh4_icount=0;trace=0;hits=0;
            m_spc=m_ssr=0;
            memset(m_rbnk,0,sizeof(m_rbnk));
            for(auto &t:m_timer){t={};t.timer_prescaler=64;}
            for(auto &t:m_dma_timer)t={};
            cave_blitter_delay={};
            memset(m_r,0,sizeof(m_r));m_r[15]=0x8d000000;
            for(unsigned r=0;r<14;r++)m_r[r]=0xdeadbeef*(r+1)+op;
            m_sr=op&0x301;m_mach=op;m_macl=~op;
            m_gbr=0x8d000000;
            code[0]=code[1]=UINT16(op);code[2]=0xc600;
            State got=Save(Sh3Run_timerhack(3));
            if(mode>=2)Check(cv1kFetchPageActive,"pure fetch-page path did not execute");
            if(mode==3 || mode==4)Check(cv1kDispatch8Active,"pure dense path did not execute");
            if(!mode)expected=got;else if(memcmp(&got,&expected,sizeof(got))){
                fprintf(stderr,"pure opcode=%04x mode=%u pc=%08x/%08x ppc=%08x/%08x ea=%08x/%08x sr=%08x/%08x trace=%llx/%llx hits=%u/%u\n",op,mode,got.pc,expected.pc,got.ppc,expected.ppc,got.ea,expected.ea,got.sr,expected.sr,(unsigned long long)got.trace,(unsigned long long)expected.trace,got.hits,expected.hits);
                const unsigned char *a=(const unsigned char*)&got,*b=(const unsigned char*)&expected;
                for(size_t i=0;i<sizeof(got);i++)if(a[i]!=b[i]){fprintf(stderr,"first diff offset %zu %02x/%02x\n",i,a[i],b[i]);break;}
                Check(false,"pure encoding/state publication");
            }
        }
        count++;
    }
    printf("SH3 pure encodings and callback publication: %u x 6 modes PASS\n",count);
}
// Independent 64-bit signed oracle, including aliased source/destination.
static void TestOverflow() {
    const UINT32 values[]={0,1,0x7fffffff,0x80000000,0xffffffff,0x40000000,0xc0000000};
    unsigned count=0;
    for(UINT32 a:values)for(UINT32 b:values)for(unsigned alias=0;alias<2;alias++)
    for(unsigned sub=0;sub<2;sub++)for(unsigned t=0;t<2;t++) {
        m_r[1]=a;m_r[2]=b;m_sr=0x700000f0|t;
        const UINT16 op=UINT16((1<<8)|((alias?1:2)<<4));
        const INT64 x=INT32(a),y=INT32(alias?a:b), result=sub?x-y:x+y;
        if(sub)SUBV(op);else ADDV(op);
        Check(m_r[1]==UINT32(result),"overflow arithmetic result");
        Check(m_sr==(0x700000f0u|unsigned(result>INT64(0x7fffffff)||result< -INT64(0x80000000))),"overflow arithmetic SR");
        count++;
    }
    printf("SH3 ADDV/SUBV independent signed oracle: %u PASS\n",count);
}
static void TestHostArithmetic() {
    UINT32 rng=0x234f18ad;
    auto next=[&](){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;};
    const UINT32 edge[]={0,1,0x7fffffff,0x80000000,0xffffffff,0x01010101,0x80808080,0xfefefefe};
    unsigned checks=0;
    for(unsigned c=0;c<20000;c++){
        const UINT32 a=c<64?edge[c/8]:next();
        const UINT32 b=c<64?edge[c%8]:next();
        const bool alias=c&1;
        const UINT32 y=alias?a:b,t=(c>>1)&1,sr=0x700000f0|t;
        const UINT16 op=UINT16((1<<8)|((alias?1:2)<<4));
        auto reset=[&](){m_r[1]=a;m_r[2]=b;m_sr=sr;m_sh4_icount=7;sh3_total_cycles=123;};
        reset();ADDC(op);
        const UINT64 sum=UINT64(a)+y+t;
        Check(m_r[1]==UINT32(sum) && m_sr==((sr&~T)|UINT32(sum>>32)) && m_sh4_icount==7 && sh3_total_cycles==123,"ADDC arithmetic/carry");
        reset();SUBC(op);
        const UINT64 sub=UINT64(y)+t;
        Check(m_r[1]==UINT32(UINT64(a)-sub) && m_sr==((sr&~T)|UINT32(UINT64(a)<sub)) && m_sh4_icount==7 && sh3_total_cycles==123,"SUBC arithmetic/borrow");
        reset();CMPSTR(op);
        bool equal=false;for(unsigned i=0;i<4;i++)equal|=((a>>(i*8))&255)==((y>>(i*8))&255);
        Check(m_r[1]==a && m_sr==((sr&~T)|unsigned(equal)) && m_sh4_icount==7 && sh3_total_cycles==123,"CMPSTR byte equality");
        for(unsigned sign=0;sign<2;sign++){
            reset();if(sign)DMULS(op);else DMULU(op);
            const UINT64 product=sign?UINT64(INT64(INT32(a))*INT64(INT32(y))):UINT64(a)*y;
            Check(UINT32(m_mach)==UINT32(product>>32) && UINT32(m_macl)==UINT32(product) && m_sr==sr &&
                m_r[1]==a && m_sh4_icount==6 && sh3_total_cycles==124,"DMUL result/cycles");
        }
        checks+=5;
    }
    printf("SH3 host arithmetic: %u carry, borrow, byte compare and signed/unsigned multiply checks PASS\n",checks);
}
static void TestDense() {
    for(unsigned mode=4;mode<=5;mode++) {
        Setup(mode,0);
        Check(Sh3DispatchKeysValid(),"all 65536 dispatch keys");
        // Read/write guest RAM, including a live instruction modified by
        // an ordinary store without changing the fetch-map epoch.
        code[0]=0x2121; // MOV.W R2,@R1
        code[1]=0x7001;
        code[2]=0x6342; // MOV.L @R4,R3
        m_r[1]=0x80000002;m_r[2]=0x7007;m_r[4]=0x8c000000;
        Sh3Run_timerhack(3);
        Check(m_r[0]==7 && code[1]==0x7007,"dense live code after direct store");
        Check(cv1kSlowFetchReuseHits>0,"sequential fetch reuse");
    }
    printf("Dense opcode keys (65536), live self-modification and sequential reuse (both tables) PASS\n");
}
static void Benchmark() {
    for(unsigned mixed=0;mixed<2;mixed++)for(unsigned mode=2;mode<6;mode++) {
        Setup(mode,0);
        if(mixed) {
            const UINT16 ops[]={0x6212,0x7201,0x2122,0x7001};
            for(unsigned i=0;i<65536;i++)code[i]=ops[i&3];
            m_r[1]=0x8c000000;
        }
        // Same guest work, synthetic host timings only. Warm up the
        // computed-goto table outside the measured region.
        m_pc=0x80000000;Sh3Run_timerhack(4);
        m_r[0]=m_r[2]=0;memset(data,0,sizeof(data));
        const auto start=std::chrono::steady_clock::now();
        for(unsigned i=0;i<1500;i++){m_pc=0x80000000;Sh3Run_timerhack(20000);}
        const auto end=std::chrono::steady_clock::now();
        printf("%s mode=%u: %.3f ms, checksum=%u/%u, reused=%llu\n",mixed?"mixed":"pure",mode,
            std::chrono::duration<double,std::milli>(end-start).count(),m_r[0],m_r[2],
            (unsigned long long)cv1kSlowFetchReuseHits);
    }
}
int main(int argc,char **) {
    if(argc>1){Benchmark();return 0;}
    for(unsigned test=0;test<14;test++)for(unsigned seed=0;seed<32;seed++) {
        const State ref=Run(0,test,seed);
        for(unsigned mode=1;mode<6;mode++) {
            const State got=Run(mode,test,seed);
            if(memcmp(&ref,&got,sizeof(got))) {
                fprintf(stderr,"scenario=%u seed=%u mode=%u\n",test,seed,mode);
                Check(false,"interpreter state/memory/callback trace/timers");
            }
        }
    }
    puts("SH3 remap/SMC/page/wrap/MMIO/IRQ/delay/timer differential: 448 x 6 modes PASS");
    TestDense();TestPure();TestOverflow();TestHostArithmetic();return 0;
}
