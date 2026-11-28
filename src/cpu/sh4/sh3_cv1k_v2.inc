// Stage C-1 frontend. Read-only compact hot lookup, sparse promotion,
// immutable compiled blocks. Live fetch/byte validation is mandatory:
// there is not yet a complete CPU/DMA/cheat/state-restore write barrier.
#if SH3_NATIVE_V2
static constexpr unsigned Cv1kV2Sets=2048,Cv1kV2Ways=4;
static constexpr unsigned Cv1kV2Entries=Cv1kV2Sets*Cv1kV2Ways;
static constexpr unsigned Cv1kV2LearnEntries=1024,Cv1kV2Hot=3,Cv1kV2Min=12;
// A bounded, time-stratified sample in each CPU slice replaces the
// global increment on every backward miss. Active physical-page
// bitmaps reject absent entries before accessing the 64 KiB directory.
static constexpr unsigned Cv1kV2PageCount=(AM>>SH3_SHIFT)+1;
// Exact halfword-entry bits on active physical pages. Sticky on retire
// (other virtual aliases may share a bit); reset frees all bitmaps.
static UINT8 *cv1kV2PageTags[Cv1kV2PageCount];
static void Cv1kV2ClearPages(){
	for(auto &p:cv1kV2PageTags){delete[] p;p=NULL;}
}
static constexpr UINT32 Cv1kV2Blocked=0x80000000u;
struct Cv1kV2Slot {
	UINT32 pc,id; // 0 = empty; 1-based metadata index; high bit = pinned/disabled
};
struct Cv1kV2Set { Cv1kV2Slot way[Cv1kV2Ways]; };
static_assert(sizeof(Cv1kV2Set)*Cv1kV2Sets==65536,"64 KiB hot directory");
struct Cv1kV2Entry {
	const UINT16 *source;
	UINT32 offset;
	UINT16 ops[Sh3Native::MaxOps];
	UINT8 count,shortExits,kind; // 0 legacy, 1 compact-leaf, 2 compact-wide
	UINT8 body,extra,longRead; // body<count iff a terminal branch is emitted
	UINT8 repeat,writes;
	UINT16 watch;
	uint64_t generation,opaqueEpoch;
};
struct Cv1kV2Learn {
	UINT32 pc,epoch;
	UINT16 first;
	UINT8 heat,rejected;
};
struct Cv1kV2Stats {
	UINT64 probes,hits,ops,built,rejected,changed,guards,full,negative;
	UINT64 samples,ready,setFull,retired,disabled,skipped;
	UINT64 compactBuilt,compactCalls,compactOps,compactBytes,legacyBuilt,resets;
	UINT64 terminalBuilt,terminalCalls,taken,notTaken,extendedBuilt,cycles;
	UINT64 dirtyChecks,untracked,chainGroups,chainJoined,selfLaps,selfBuilt;
};
static Cv1kV2Set *cv1kV2Fast;
static Cv1kV2Entry *cv1kV2Table;
static Cv1kV2Learn cv1kV2Learn[Cv1kV2LearnEntries];
static Sh3V2::CodeCache cv1kV2Code;
static Cv1kV2Stats cv1kV2Stats;
static bool cv1kV2Full,cv1kV2LookupOnly,cv1kV2Compact,cv1kV2Blocks,cv1kV2Chain,cv1kV2SelfLoop;
static inline unsigned Cv1kV2Minimum(){return cv1kV2Blocks?2:Cv1kV2Min;}
static UINT32 cv1kV2SampleClock,cv1kV2Random;
static INT32 cv1kV2SampleAt=-1;
static void Cv1kV2InvalidateAll(){
	if(!cv1kV2Fast)return;
	memset(cv1kV2Fast,0,sizeof(Cv1kV2Set)*Cv1kV2Sets);
	Cv1kV2ClearPages();Cv1kV2WatchClear();
	memset(cv1kV2Learn,0,sizeof(cv1kV2Learn));
	cv1kV2Code.Reset();cv1kV2Full=false;cv1kV2SampleAt=-1;
	cv1kV2SampleClock=0;cv1kV2Random=0x957fea31u;
	++cv1kV2Stats.resets;
}
static void Cv1kV2Reset(){
	delete[] cv1kV2Fast;cv1kV2Fast=NULL;
	delete[] cv1kV2Table;cv1kV2Table=NULL;
	memset(cv1kV2Learn,0,sizeof(cv1kV2Learn));
	cv1kV2Code.Reset();cv1kV2Stats={};cv1kV2Full=false;
	cv1kV2LookupOnly=false;cv1kV2Compact=false;cv1kV2Blocks=false;cv1kV2SampleClock=0;
	cv1kV2Chain=cv1kV2SelfLoop=false;
	cv1kV2Random=0x957fea31u;cv1kV2SampleAt=-1;
	Cv1kV2ClearPages();Cv1kV2WatchReset();
}
static bool Cv1kV2Enable(){
	cv1kV2Fast=new(std::nothrow) Cv1kV2Set[Cv1kV2Sets]();
	cv1kV2Table=new(std::nothrow) Cv1kV2Entry[Cv1kV2Entries]();
	if(!cv1kV2Fast || !cv1kV2Table){Cv1kV2Reset();return false;}
	const char *lookup=getenv("FBNEO_CV1K_V2_LOOKUP_ONLY");
	cv1kV2LookupOnly=lookup && !strcmp(lookup,"1");
	cv1kV2Compact=Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_COMPACT"),true);
	cv1kV2Blocks=cv1kV2Compact && Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_BLOCKS"),true);
	cv1kV2Chain=cv1kV2Blocks && Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_CHAIN"),true);
	cv1kV2SelfLoop=cv1kV2Chain && Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_SELF_LOOP"),true);
	Cv1kV2WatchEnable();
	return true;
}
static inline __attribute__((always_inline)) unsigned Cv1kV2Hash(UINT32 pc){
	const UINT32 h=pc>>1;
	return (h^(h>>11)^(h>>22))&(Cv1kV2Sets-1);
}
static inline __attribute__((always_inline)) Cv1kV2Slot *Cv1kV2Find(UINT32 pc){
	const UINT8 *bits=cv1kV2PageTags[(pc&AM)>>SH3_SHIFT];
	if(!bits || !(bits[(pc&SH3_PAGEM)>>4]&(1u<<((pc>>1)&7))))return NULL;
	Cv1kV2Slot *w=cv1kV2Fast[Cv1kV2Hash(pc)].way;
	for(unsigned i=0;i<Cv1kV2Ways;i++){
		if(!w[i].id)return NULL; // sets are packed, even after retirement
		if(w[i].pc==pc)return w+i;
	}
	return NULL;
}
static void Cv1kV2Retire(Cv1kV2Slot &slot){
	Cv1kV2Slot *ways=cv1kV2Fast[Cv1kV2Hash(slot.pc)].way;
	unsigned last=0;
	while(last+1<Cv1kV2Ways && ways[last+1].id)++last;
	if(&slot!=ways+last){
		const UINT32 id=slot.id&~Cv1kV2Blocked;
		cv1kV2Table[id-1]=cv1kV2Table[(ways[last].id&~Cv1kV2Blocked)-1];
		slot={ways[last].pc,id|(ways[last].id&Cv1kV2Blocked)};
	}
	ways[last]={};
}
static bool Cv1kV2Candidate(UINT16 op){
	// Preserve the existing full-literal CMP/PZ dispatch hook.
	return (op&0xf0ff)!=0x4011 && (cv1kV2Blocks?Sh3V2::ExtendedCandidate(op):Sh3V2::Candidate(op));
}
static inline bool Cv1kV2Allowed(){
	return m_cpu_type==CPU_TYPE_SH3 && !m_delay && !m_test_irq &&
		!m_cpu_off && !sh3_end_run && m_sh4_icount>=INT32(Cv1kV2Minimum()) && !(m_pc&1);
}
static const UINT16 *Cv1kV2Source(UINT32 pc,unsigned &available){
	const UINT32 physical=pc&AM,inpage=physical&SH3_PAGEM;
	const UINT8 *page=MemMapF[physical>>SH3_SHIFT];
	available=(SH3_PAGE_SIZE-inpage)/2;
	if(available>Sh3Native::MaxOps)available=Sh3Native::MaxOps;
	if((uintptr_t)page<SH3_MAXHANDLER || available<Cv1kV2Minimum())return NULL;
	return reinterpret_cast<const UINT16*>(page+inpage);
}
static void Cv1kV2Reject(Cv1kV2Learn &learn){
	learn.rejected=1;++cv1kV2Stats.rejected;++cv1kNativeRejected;
}
static bool Cv1kV2Terminal(const UINT16 *code,unsigned body,unsigned available,UINT32 pc){
	const UINT16 op=code[body];
	if(!cv1kV2Blocks || !Sh3V2::Terminal(op))return false;
	// Match the original Flow exclusions. Even a not-taken condition
	// is left to the interpreter when a taken edge could enter a kernel.
	if((op>>8)==0x8b && (cv1kInterpLoopsEnabled || (op==0x8bfa && cv1kDataLoopsEnabled)))return false;
	if((op>>12)==10 && (op&0xfff)==0xffe)return false; // BRA self burn
	if(!Sh3V2::Delayed(op))return true;
	if(body+1>=available || !(pc+body*2+2) || !Sh3V2::SafeSlot(code[body+1]))return false;
	const UINT16 slot=code[body+1];
	const UINT32 next=pc+body*2+2;
	const bool dynamic=Sh3V2::Dynamic(op);
	const UINT32 distance=next-Sh3V2::StaticTarget(op,next-2);
	const bool addSmall=(slot>>12)==7 && ((slot&255)==1||(slot&255)==2||(slot&255)==4);
	if(cv1kDataLoopsEnabled &&
		(((dynamic||distance==22)&&(slot&0xf00f)==0x600e)||((dynamic||distance==8)&&addSmall)))return false;
	if(cv1kInterpLoopsEnabled && (slot==0x0009||(slot>>12)==7) &&
		(dynamic||distance==4||distance==6||distance==8))return false;
	return true;
}
// The large emission scratch and all compile work belong to a cold
// function; neither the compiled-hit nor the inline miss path needs it.
static __attribute__((noinline,cold)) void Cv1kV2Compile(
	UINT32 pc,const UINT16 *code,unsigned available,Cv1kV2Learn &learn)
{
	unsigned body=0;
	while(body<available && Cv1kV2Candidate(code[body]))++body;
	bool terminal=body<available && Cv1kV2Terminal(code,body,available,pc);
	unsigned n=body+(terminal?(1+unsigned(Sh3V2::Delayed(code[body]))):0);
	// Short straight-line snippets still pay the full invocation cost.
	// Admit terminal blocks from two ops, other extended spans from four.
	if(n<(terminal?2u:cv1kV2Blocks?4u:Cv1kV2Min)){Cv1kV2Reject(learn);return;}
	const unsigned set=Cv1kV2Hash(pc);
	Cv1kV2Slot *ways=cv1kV2Fast[set].way;
	unsigned way=0;
	while(way<Cv1kV2Ways && ways[way].id)++way;
	if(way==Cv1kV2Ways){
		// Never evict a published block merely to admit a new candidate.
		++cv1kV2Stats.setFull;Cv1kV2Reject(learn);return;
	}
	UINT32 words[Sh3Native::MaxWords];
	unsigned kind=0,wordCount=0;
	bool repeat=cv1kV2SelfLoop && terminal && !Sh3V2::Dynamic(code[body]) &&
		!Sh3V2::Call(code[body]) && Sh3V2::StaticTarget(code[body],pc+body*2)==pc;
	if(cv1kV2Compact){
		Sh3V2::CompactEmitter emit(words);
		if(emit.Build(code,n,pc,body,cv1kV2Blocks,cv1kV2WriteWatch!=NULL,repeat)){
			wordCount=emit.End()-words;kind=emit.Wide()?2:1;
		}
	}
	if(!kind){
		// Wide footprints or unsupported compact forms use the audited
		// V1 emitter. Keep the previous emitter for same-binary A/B.
		n=0;
		while(n<body && Sh3Native::Candidate(code[n]))++n;
		if(n<Cv1kV2Min){Cv1kV2Reject(learn);return;}
		body=n;terminal=false;repeat=false;
		Sh3Native::Emitter emit(words);
		for(unsigned i=0;i<n;i++)if(!emit.Emit(code[i],i)){
			Cv1kV2Reject(learn);return;
		}
		emit.Finish(n);wordCount=emit.End()-words;
	}
	UINT8 *&bits=cv1kV2PageTags[(pc&AM)>>SH3_SHIFT];
	if(!bits)bits=new(std::nothrow) UINT8[SH3_PAGE_SIZE/16]();
	if(!bits){Cv1kV2Reject(learn);return;}
	UINT32 offset;
	if(!cv1kV2Code.Publish(words,wordCount,offset)){
		cv1kV2Full=true;++cv1kV2Stats.full;++cv1kNativeFull;
		bprintf(PRINT_UI,_T("CV1000 SH3 V2 cache: %s; no new promotions\n"),
			cv1kV2Code.Denied()?_T("RW/RX allocation denied"):_T("full"));
		return;
	}
	const UINT32 id=set*Cv1kV2Ways+way+1;
	Cv1kV2Entry &b=cv1kV2Table[id-1];
	b.source=code;b.offset=offset;b.count=n;b.shortExits=0;b.kind=kind;
	b.body=body;b.extra=terminal?Sh3V2::ExtraCycles(code[body]):0;b.longRead=0;
	b.repeat=repeat;b.writes=0;
	b.watch=Cv1kV2WatchCode(MemMapF[(pc&AM)>>SH3_SHIFT]);
	b.generation=b.watch?cv1kV2WatchPages[b.watch-1].generation:0;
	b.opaqueEpoch=cv1kV2OpaqueEpoch;
	bool extended=terminal;
	for(unsigned i=0;i<body;i++){
		const UINT16 op=code[i];const unsigned t=op>>12,f=op&15;
		if(t==13 || t==5 || (t==6 && (f==2||f==6)))b.longRead=1;
		if(Sh3V2::MemoryOp(op)&&(t==1||t==2))b.writes=1;
		extended|=Sh3V2::PcRelative(op)||Sh3V2::IsDT(op);
	}
	memcpy(b.ops,code,n*sizeof(UINT16));
	ways[way]={pc,id};
	bits[(pc&SH3_PAGEM)>>4]|=1u<<((pc>>1)&7);
	if(repeat)++cv1kV2Stats.selfBuilt;
	if(terminal)++cv1kV2Stats.terminalBuilt;
	if(extended)++cv1kV2Stats.extendedBuilt;
	if(kind){++cv1kV2Stats.compactBuilt;cv1kV2Stats.compactBytes+=wordCount*4;}
	else ++cv1kV2Stats.legacyBuilt;
	++cv1kV2Stats.built;++cv1kNativeBuilt;
}
static __attribute__((noinline,cold)) void Cv1kV2Observe(UINT32 pc){
	if(cv1kV2Full || !cv1kV2Fast || !Cv1kV2Allowed() || Cv1kV2Find(pc))return;
	++cv1kV2Stats.samples;++cv1kV2Stats.probes;
	unsigned available;
	const UINT16 *code=Cv1kV2Source(pc,available);
	if(!code)return;
	// Rejection is a read-only decision on later sampled visits.
	// Map epoch/first-op changes allow relearning; interior changes
	// to a negative site may delay promotion, never guest execution.
	const UINT32 key=pc>>1;
	Cv1kV2Learn &l=cv1kV2Learn[(key^(key>>10)^(key>>20))&(Cv1kV2LearnEntries-1)];
	const UINT16 first=code[0];
	if(l.pc!=pc || l.epoch!=sh3FetchMapEpoch || l.first!=first || !l.heat){
		l={pc,sh3FetchMapEpoch,first,1,0};++cv1kNativeMisses;
		return;
	}
	if(l.rejected){++cv1kV2Stats.negative;return;}
	if(++l.heat<Cv1kV2Hot)return;
	Cv1kV2Compile(pc,code,available,l);
}
static __attribute__((noinline)) unsigned Cv1kV2Run(Cv1kV2Slot &slot,bool chain=false,bool *cont=NULL){
	if(cont)*cont=false;
	if(!Cv1kV2Allowed())return 0;
	++cv1kV2Stats.ready;++cv1kV2Stats.probes;
	Cv1kV2Entry &b=cv1kV2Table[slot.id-1];
	unsigned available;
	const UINT32 pc=m_pc;
	const UINT16 *code=Cv1kV2Source(pc,available);
	const uint64_t generation=b.watch?cv1kV2WatchPages[b.watch-1].generation:0;
	const bool dirty=b.watch && (generation!=b.generation || b.opaqueEpoch!=cv1kV2OpaqueEpoch);
	if(dirty)++cv1kV2Stats.dirtyChecks;
	if(!code || b.source!=code || b.count>available ||
		memcmp(b.ops,code,b.count*sizeof(UINT16))){
		// Live code/remap invalidation, never a hash collision. Retire
		// the immutable code; its storage is not reused mid-execution.
		if(b.watch && !dirty)++cv1kV2Stats.untracked;
		Cv1kV2Retire(slot);++cv1kV2Stats.changed;++cv1kV2Stats.retired;
		return 0;
	}
	if(dirty){b.generation=generation;b.opaqueEpoch=cv1kV2OpaqueEpoch;}
	// Reserve the worst-case complete branch+slot budget; exact tails
	// and pending slots are handled by the original interpreter.
	if(unsigned(b.count)+b.extra>unsigned(m_sh4_icount))return 0;
	if(cv1kV2LookupOnly){++cv1kV2Stats.skipped;return 0;}
	const UINT8 *idle=NULL;
	if(cv1kV2Blocks && b.longRead && cv1kIdleBatchEnabled && Cv1kIsIdlePage(MemMapRL[0xc00]))idle=cv1kIdlePage;
	unsigned iterations=1;
	const unsigned lapCost=b.count+b.extra;
	if(chain && b.repeat){
		iterations=unsigned(m_sh4_icount)/lapCost;
		if(iterations>4096)iterations=4096;
	}
	const Sh3Native::Memory memory={MemMapR,MemMapRL,MemMapW,code,
		size_t(b.count)*sizeof(UINT16),iterations,pc,idle,cv1kIdleAddress,m_pr,cv1kV2WriteWatch};
	const unsigned result=b.kind?
		cv1kV2Code.RunCompact(b.offset,m_r,&m_sr,&m_ea,memory,b.kind==2):
		cv1kV2Code.Run(b.offset,m_r,&m_sr,&m_ea,memory);
	const unsigned done=result&255u,flags=result&0x300u,laps=b.repeat?result>>10:0;
	if(!b.kind && b.writes)Cv1kV2NoteOpaqueWrite();
	const unsigned total=done+laps*b.count;
	const bool terminal=b.body<b.count;
	const bool guard=terminal?!flags:done!=b.count;
	unsigned spent=done+laps*lapCost;
	UINT32 next=pc+done*2,ppc=next;
	if(laps){
		cv1kV2Stats.selfLaps+=laps;cv1kV2Stats.terminalCalls+=laps;cv1kV2Stats.taken+=laps;
		if(!done)ppc=Sh3V2::Delayed(b.ops[b.body])?pc:pc+(b.body+1)*2;
	}
	if(terminal && flags){
		++cv1kV2Stats.terminalCalls;
		const UINT16 op=b.ops[b.body];
		const UINT32 branchNext=pc+(b.body+1)*2;
		if(flags==Sh3V2::ExitTaken){
			++cv1kV2Stats.taken;spent+=b.extra;
			next=m_ea;ppc=Sh3V2::Delayed(op)?next:branchNext;
			if(Sh3V2::Call(op))m_pr=branchNext+2;
		}else ++cv1kV2Stats.notTaken;
	}
	if(b.kind){++cv1kV2Stats.compactCalls;cv1kV2Stats.compactOps+=total;}
	++cv1kV2Stats.hits;++cv1kNativeSpans;
	if(guard)++cv1kV2Stats.guards;
	if(guard && total<Cv1kV2Min){
		if(++b.shortExits>=8){
			slot.id|=Cv1kV2Blocked;++cv1kV2Stats.disabled;
		}
	}else if(b.shortExits)b.shortExits=0;
	if(!total)return 0;
	m_pc=next;m_ppc=ppc;EAT(spent);
	cv1kV2Stats.ops+=total;cv1kV2Stats.cycles+=spent;cv1kNativeOps+=total;
	if(cont)*cont=!guard;
	return total;
}
// C-1: do not force an interpreter block after a complete native exit.
// Keep guards and budget misses on the exact interpreter fallback.
static __attribute__((noinline)) unsigned Cv1kV2RunChain(Cv1kV2Slot &first){
	unsigned total=0;Cv1kV2Slot *slot=&first;
	for(unsigned i=0;i<64;i++){
		bool cont=false;const unsigned done=Cv1kV2Run(*slot,true,&cont);
		if(!done)break;
		if(!i)++cv1kV2Stats.chainGroups;else ++cv1kV2Stats.chainJoined;
		total+=done;
		if(!cont || !Cv1kV2Allowed())break;
		slot=Cv1kV2Find(m_pc);
		if(!slot || (slot->id&Cv1kV2Blocked))break;
	}
	return total;
}
static inline __attribute__((always_inline)) unsigned Cv1kV2Try(UINT32 pc,bool backward=false,bool chain=false){
	// Only selected in a dispatcher with an allocated directory.
	// Ordinary misses do not enter a C function or touch large metadata.
	Cv1kV2Slot *s=Cv1kV2Find(pc);
	if(s)return s->id&Cv1kV2Blocked?0:(chain&&cv1kV2Chain?Cv1kV2RunChain(*s):Cv1kV2Run(*s));
	if((backward || cv1kV2Blocks) && cv1kV2SampleAt>=m_sh4_icount && m_sh4_icount>=INT32(Cv1kV2Minimum()) && !cv1kV2Full){
		cv1kV2SampleAt=-1;
		Cv1kV2Observe(pc);
	}
	return 0;
}
static inline __attribute__((always_inline)) void Cv1kV2SliceSample(){
	// One time-stratified backward-miss sample per slice, plus a
	// low-rate entry sample. No RMW on ordinary backward misses.
	cv1kV2SampleAt=-1;
	if(cv1kV2Full || m_sh4_icount<INT32(Cv1kV2Minimum()))return;
	UINT32 r=cv1kV2Random;r^=r<<13;r^=r>>17;r^=r<<5;cv1kV2Random=r;
	cv1kV2SampleAt=INT32((UINT64(r)*UINT32(m_sh4_icount))>>32);
	if(!(++cv1kV2SampleClock&15u))Cv1kV2Observe(m_pc);
}
#else
static void Cv1kV2InvalidateAll(){}
static inline unsigned Cv1kV2Try(UINT32,bool=false,bool=false){return 0;}
static inline void Cv1kV2SliceSample(){}
#endif
