// Stage A-fix frontend. Read-only compact hot lookup, sparse promotion,
// immutable compiled blocks. Live fetch/byte validation is mandatory:
// there is not yet a complete CPU/DMA/cheat/state-restore write barrier.
#if SH3_NATIVE_V2
static constexpr unsigned Cv1kV2Sets=2048,Cv1kV2Ways=4;
static constexpr unsigned Cv1kV2Entries=Cv1kV2Sets*Cv1kV2Ways;
static constexpr unsigned Cv1kV2LearnEntries=1024,Cv1kV2Hot=3,Cv1kV2Min=12;
// A bounded, time-stratified sample in each CPU slice replaces the
// global increment on every backward miss. The physical-page mask
// rejects absent PC tags without accessing the 64 KiB directory.
static constexpr unsigned Cv1kV2PageCount=(AM>>SH3_SHIFT)+1;
static UINT16 cv1kV2PageTags[Cv1kV2PageCount];
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
};
static Cv1kV2Set *cv1kV2Fast;
static Cv1kV2Entry *cv1kV2Table;
static Cv1kV2Learn cv1kV2Learn[Cv1kV2LearnEntries];
static Sh3V2::CodeCache cv1kV2Code;
static Cv1kV2Stats cv1kV2Stats;
static bool cv1kV2Full,cv1kV2LookupOnly,cv1kV2Compact;
static UINT32 cv1kV2SampleClock,cv1kV2Random;
static INT32 cv1kV2SampleAt=-1;
static void Cv1kV2InvalidateAll(){
	if(!cv1kV2Fast)return;
	memset(cv1kV2Fast,0,sizeof(Cv1kV2Set)*Cv1kV2Sets);
	memset(cv1kV2PageTags,0,sizeof(cv1kV2PageTags));
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
	cv1kV2LookupOnly=false;cv1kV2Compact=false;cv1kV2SampleClock=0;
	cv1kV2Random=0x957fea31u;cv1kV2SampleAt=-1;
	memset(cv1kV2PageTags,0,sizeof(cv1kV2PageTags));
}
static bool Cv1kV2Enable(){
	cv1kV2Fast=new(std::nothrow) Cv1kV2Set[Cv1kV2Sets]();
	cv1kV2Table=new(std::nothrow) Cv1kV2Entry[Cv1kV2Entries]();
	if(!cv1kV2Fast || !cv1kV2Table){Cv1kV2Reset();return false;}
	const char *lookup=getenv("FBNEO_CV1K_V2_LOOKUP_ONLY");
	cv1kV2LookupOnly=lookup && !strcmp(lookup,"1");
	cv1kV2Compact=Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_COMPACT"),true);
	return true;
}
static inline __attribute__((always_inline)) unsigned Cv1kV2Hash(UINT32 pc){
	const UINT32 h=pc>>1;
	return (h^(h>>11)^(h>>22))&(Cv1kV2Sets-1);
}
static inline __attribute__((always_inline)) UINT16 Cv1kV2Tag(UINT32 pc){
	return UINT16(1u<<(((pc>>1)^(pc>>5)^(pc>>9))&15u));
}
static inline __attribute__((always_inline)) Cv1kV2Slot *Cv1kV2Find(UINT32 pc){
	if(!(cv1kV2PageTags[(pc&AM)>>SH3_SHIFT]&Cv1kV2Tag(pc)))return NULL;
	Cv1kV2Slot *w=cv1kV2Fast[Cv1kV2Hash(pc)].way;
	// No writes, no opcode decode, no live fetch-map walk on an ordinary
	// miss. Empty entries can occur between occupied ways after remap.
	for(unsigned i=0;i<Cv1kV2Ways;i++)
		if(w[i].pc==pc && w[i].id)return w+i;
	return NULL;
}
static bool Cv1kV2Candidate(UINT16 op){
	// Preserve the existing full-literal CMP/PZ dispatch hook.
	return (op&0xf0ff)!=0x4011 && Sh3V2::Candidate(op);
}
static inline bool Cv1kV2Allowed(){
	return m_cpu_type==CPU_TYPE_SH3 && !m_delay && !m_test_irq &&
		!m_cpu_off && !sh3_end_run && m_sh4_icount>=INT32(Cv1kV2Min) && !(m_pc&1);
}
static const UINT16 *Cv1kV2Source(UINT32 pc,unsigned &available){
	const UINT32 physical=pc&AM,inpage=physical&SH3_PAGEM;
	const UINT8 *page=MemMapF[physical>>SH3_SHIFT];
	available=(SH3_PAGE_SIZE-inpage)/2;
	if(available>Sh3Native::MaxOps)available=Sh3Native::MaxOps;
	if((uintptr_t)page<SH3_MAXHANDLER || available<Cv1kV2Min)return NULL;
	return reinterpret_cast<const UINT16*>(page+inpage);
}
static void Cv1kV2Reject(Cv1kV2Learn &learn){
	learn.rejected=1;++cv1kV2Stats.rejected;++cv1kNativeRejected;
}
// The large emission scratch and all compile work belong to a cold
// function; neither the compiled-hit nor the inline miss path needs it.
static __attribute__((noinline,cold)) void Cv1kV2Compile(
	UINT32 pc,const UINT16 *code,unsigned available,Cv1kV2Learn &learn)
{
	unsigned n=0;
	while(n<available && Cv1kV2Candidate(code[n]))++n;
	if(n<Cv1kV2Min){Cv1kV2Reject(learn);return;}
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
	if(cv1kV2Compact){
		Sh3V2::CompactEmitter emit(words);
		if(emit.Build(code,n)){
			wordCount=emit.End()-words;kind=emit.Wide()?2:1;
		}
	}
	if(!kind){
		// Wide footprints or unsupported compact forms use the audited
		// V1 emitter. Keep the previous emitter for same-binary A/B.
		Sh3Native::Emitter emit(words);
		for(unsigned i=0;i<n;i++)if(!emit.Emit(code[i],i)){
			Cv1kV2Reject(learn);return;
		}
		emit.Finish(n);wordCount=emit.End()-words;
	}
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
	memcpy(b.ops,code,n*sizeof(UINT16));
	ways[way]={pc,id};
	cv1kV2PageTags[(pc&AM)>>SH3_SHIFT]|=Cv1kV2Tag(pc);
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
static __attribute__((noinline)) unsigned Cv1kV2Run(Cv1kV2Slot &slot){
	if(!Cv1kV2Allowed())return 0;
	++cv1kV2Stats.ready;++cv1kV2Stats.probes;
	Cv1kV2Entry &b=cv1kV2Table[slot.id-1];
	unsigned available;
	const UINT32 pc=m_pc;
	const UINT16 *code=Cv1kV2Source(pc,available);
	if(!code || b.source!=code || b.count>available ||
		memcmp(b.ops,code,b.count*sizeof(UINT16))){
		// Live code/remap invalidation, never a hash collision. Retire
		// the immutable code; its storage is not reused mid-execution.
		slot.id=0;++cv1kV2Stats.changed;++cv1kV2Stats.retired;
		return 0;
	}
	if(b.count>m_sh4_icount)return 0;
	if(cv1kV2LookupOnly){++cv1kV2Stats.skipped;return 0;}
	const Sh3Native::Memory memory={MemMapR,MemMapRL,MemMapW,code,
		size_t(b.count)*sizeof(UINT16),0,pc};
	const unsigned done=b.kind?
		cv1kV2Code.RunCompact(b.offset,m_r,&m_sr,&m_ea,memory,b.kind==2):
		cv1kV2Code.Run(b.offset,m_r,&m_sr,&m_ea,memory);
	if(b.kind){++cv1kV2Stats.compactCalls;cv1kV2Stats.compactOps+=done;}
	++cv1kV2Stats.hits;++cv1kNativeSpans;
	if(done!=b.count)++cv1kV2Stats.guards;
	if(done<Cv1kV2Min){
		if(++b.shortExits>=8){
			// Keep both the metadata and native storage pinned. Repeated
			// guard failures must not trigger endless recompilation.
			slot.id|=Cv1kV2Blocked;++cv1kV2Stats.disabled;
		}
	}else if(b.shortExits)b.shortExits=0;
	if(!done)return 0;
	m_pc=m_ppc=pc+done*2;EAT(done);
	cv1kV2Stats.ops+=done;cv1kNativeOps+=done;return done;
}
static inline __attribute__((always_inline)) unsigned Cv1kV2Try(UINT32 pc,bool backward=false){
	// Only selected in a dispatcher with an allocated directory.
	// Ordinary misses do not enter a C function or touch large metadata.
	Cv1kV2Slot *s=Cv1kV2Find(pc);
	if(s)return s->id&Cv1kV2Blocked?0:Cv1kV2Run(*s);
	if(backward && cv1kV2SampleAt>=m_sh4_icount && m_sh4_icount>=INT32(Cv1kV2Min) && !cv1kV2Full){
		cv1kV2SampleAt=-1;
		Cv1kV2Observe(pc);
	}
	return 0;
}
static inline __attribute__((always_inline)) void Cv1kV2SliceSample(){
	// One time-stratified backward-miss sample per slice, plus a
	// low-rate entry sample. No RMW on ordinary backward misses.
	cv1kV2SampleAt=-1;
	if(cv1kV2Full || m_sh4_icount<INT32(Cv1kV2Min))return;
	UINT32 r=cv1kV2Random;r^=r<<13;r^=r>>17;r^=r<<5;cv1kV2Random=r;
	cv1kV2SampleAt=INT32((UINT64(r)*UINT32(m_sh4_icount))>>32);
	if(!(++cv1kV2SampleClock&15u))Cv1kV2Observe(m_pc);
}
#else
static void Cv1kV2InvalidateAll(){}
static inline unsigned Cv1kV2Try(UINT32,bool=false){return 0;}
static inline void Cv1kV2SliceSample(){}
#endif
