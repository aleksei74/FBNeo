// C-1: use real emitted AArch64 loops, not a mock of their result ABI.
#define SH3_V2_C1_TEST 1
#include "test_sh3_v2_b2.cpp"

static void C1Init(bool on,bool chain=true){
	setenv("FBNEO_CV1K_V2_WRITE_AUDIT","1",1);
	setenv("FBNEO_CV1K_V2_CHAIN",chain?"1":"0",1);
	setenv("FBNEO_CV1K_V2_SELF_LOOP",chain?"1":"0",1);
	InitB2(on);
}
static void PromoteAt(UINT32 pc){
	m_pc=pc;m_sh4_icount=1000;sh3_end_run=0;Promote();
}
static std::vector<Snapshot> RepeatCase(bool on,unsigned test){
	C1Init(on);
	const UINT32 base=test==6?0x80000100:0x80000000;
	switch(test){
	case 0: Loop(2,0);break;
	case 1:
		code[0]=0x7001;code[1]=0x4210;code[2]=0x8bfc;
		code[3]=0x7301;code[4]=0xaffd;code[5]=0x0009;m_r[2]=37;break;
	case 2:
		code[0]=0x7001;code[1]=0x4210;code[2]=0x8ffc;code[3]=0x7301;
		code[4]=0x7401;code[5]=0xaffd;code[6]=0x0009;m_r[2]=37;break;
	case 3:
		code[0]=0x6316;code[1]=0x7001;code[2]=0xaffc;code[3]=0x0009;
		m_r[1]=0x8c00fff0;scenario=3;break;
	case 4:
		code[0]=0x6316;code[1]=0x8bfd;m_r[1]=0x8c00fff0;scenario=3;break;
	case 5:
		code[0]=0x7001;code[1]=0xa00d;code[2]=0x0009;
		code[16]=0x7213;code[17]=0xafed;code[18]=0x0009;break;
	case 6:
		// Several successful native store laps precede a write through
		// the mirror into the executing span. The old code must exit.
		code[128]=0x2131;code[129]=0x7102;code[130]=0xaffc;code[131]=0x0009;
		Sh3MapMemory((UINT8*)code,0xc800000,0xc80ffff,MAP_WRITE);
		m_pc=base;m_r[1]=0x8c8000f8;m_r[3]=0x7001;break;
	}
	if(on){
		PromoteAt(base);
		const auto *slot=Cv1kV2Find(base);
		Check(cv1kV2Table[slot->id-1].repeat==unsigned(test!=5),"C1 self loop eligibility");
		if(test==5)PromoteAt(base+32);
		m_pc=base;
	}
	m_sh4_icount=0;
	std::vector<Snapshot> states;
	for(unsigned i=0;i<24;i++){
		const unsigned budget=i==0?1001:1+(i*31)%167;
		states.push_back(Snap(Sh3Run_timerhack(budget)));
	}
	if(on){
		if(test==5)Check(cv1kV2Stats.chainJoined>0,"C1 distinct blocks joined");
		else Check(cv1kV2Stats.selfLaps>0,"C1 actual native self repetition");
		if(test==3||test==4)Check(cv1kV2Stats.guards>0 && hits>0,"C1 guard after completed laps");
		if(test==6)Check(cv1kV2Stats.guards>0 && cv1kV2Stats.changed>0 && !cv1kV2Stats.untracked,"C1 mirror SMC after completed laps");
		printf("C1 case %u: calls %llu joins %llu selflaps %llu guards %llu\n",test,
			(unsigned long long)cv1kV2Stats.hits,(unsigned long long)cv1kV2Stats.chainJoined,
			(unsigned long long)cv1kV2Stats.selfLaps,(unsigned long long)cv1kV2Stats.guards);
	}
	return states;
}
static void ShadowWatch(){
	for(unsigned test=0;test<8;test++){
		C1Init(true);Loop(20,0);
		const UINT32 base=0x80000000,alias=0x8c800000;
		Sh3MapMemory((UINT8*)code,0xc800000,0xc80ffff,MAP_WRITE);
		PromoteAt(base);
		Check(cv1kV2WriteWatch[0] && cv1kV2WriteWatch[0]==cv1kV2WriteWatch[0xc80],"host backing alias watch");
		const auto before=*cv1kV2WriteWatch[0];
		switch(test){
		case 0: WB(alias+14,0xe7);break;
		case 1: cheat_write(alias+15,0x7f);break;
		case 2: WW(alias+14,0xe77f);break;
		case 3: WL(alias+12,0xe77fe77f);break;
		case 4:
			m_r[1]=alias+14;m_r[3]=0xe77f;
			Check(Cv1kTryDirectMove<2,true,CV1K_REG,false>(0x2131,base+2),"direct mapped word write");break;
		case 5:
			m_r[1]=alias+16;m_pr=0xe77fe77f;
			Check(Cv1kTryStack<true,&Sh3HotState::pr,false>(0x4122),"direct stack store");break;
		case 6:
			// Execute a generated store to ANOTHER compiled backing page.
			code[32768]=0x2131;code[32769]=0xa004;code[32770]=0x0009;
			m_r[1]=alias+14;m_r[3]=0xe77f;
			PromoteAt(base+0x10000);
			Check(Cv1kV2Try(m_pc)==3,"native writer executed");break;
		case 7: code[7]=0xe77f;break; // intentionally opaque host mutation
		}
		Check((*cv1kV2WriteWatch[0]!=before)==(test!=7),"write generation observed");
		m_pc=base;m_sh4_icount=1000;
		const auto snap=Snap(0);
		Check(!Cv1kV2Try(base) && !Cv1kV2Find(base) && cv1kV2Stats.changed==1,"modified block retired before execution");
		Check(cv1kV2Stats.untracked==unsigned(test==7),"untracked mutation cross-check");
		Compare(snap,Snap(0),120,test);
	}
	C1Init(true);Loop(20,0);PromoteAt(0x80000000);
	Sh3Scan(ACB_WRITE);
	Check(!Cv1kV2Find(0x80000000) && cv1kV2WatchCount==0 && !cv1kV2Code.Used(),"state restore invalidates code and watches");
	puts("B3 write watch: byte/cheat/word/long/direct/stack/native/opaque + mirror + restore PASS");
}
static double C1Bench(unsigned mode,unsigned kind,Snapshot &state){
	C1Init(mode!=0,mode==2);
	if(kind==0)Loop(2,0);
	else{
		const UINT16 ops[]={0xd10f,0x6312,0x7301,0x2132,0x4210,0x8bf9,0xe220,0xaff7,0x0009};
		memcpy(code,ops,sizeof(ops));code[32]=0x8c00;code[33]=0x0100;m_r[2]=32;
	}
	for(unsigned i=0;i<12;i++)Sh3Run_timerhack(2049);
	const auto before=std::chrono::steady_clock::now();
	for(unsigned i=0;i<200;i++)Sh3Run_timerhack(25000);
	const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
	state=Snap(0);
	printf("C1 bench %u mode %u %.3f ms calls %llu native cycles %llu selflaps %llu joins %llu\n",kind,mode,ms,
		(unsigned long long)cv1kV2Stats.hits,(unsigned long long)cv1kV2Stats.cycles,
		(unsigned long long)cv1kV2Stats.selfLaps,(unsigned long long)cv1kV2Stats.chainJoined);
	return ms;
}
int main(int argc,char **){
	setvbuf(stdout,NULL,_IOLBF,0);
	if(argc>1){
		for(unsigned kind=0;kind<2;kind++){
			double ms[3][3];Snapshot states[3];
			for(unsigned i=0;i<3;i++){
				for(unsigned j=0;j<3;j++){const unsigned mode=(i+j)%3;ms[mode][i]=C1Bench(mode,kind,states[mode]);}
				Compare(states[0],states[1],kind,i);Compare(states[0],states[2],kind,i);
			}
			for(auto &a:ms)std::sort(a,a+3);
			printf("C1 median %u control %.3f unchained %.3f C1 %.3f ms\n",kind,ms[0][1],ms[1][1],ms[2][1]);
		}
		return 0;
	}
	ShadowWatch();
	for(unsigned k=0;k<7;k++){
		const auto a=RepeatCase(false,k),b=RepeatCase(true,k);
		for(unsigned i=0;i<a.size();i++)Compare(a[i],b[i],121+k,i);
	}
	puts("C1 repeat/chain: 168 differential slices PASS");
	return 0;
}
