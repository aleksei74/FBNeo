// Stage C-2 frontend. Compact write watches and guarded static native
// links. Only link stubs are patched; retired descriptors stay pinned.
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
	UINT16 watch,link,region,global;
	UINT8 regionMember;
	uint64_t regionTried;
	uint64_t generation,opaqueEpoch,sliceEpoch;
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
	UINT64 verifyChecks,verifySkipped;
	UINT64 linkGroups,linkBodies,linkEdges,linkPatched,linkBuilt,linkStops;
	UINT64 residentGroups,residentBlocks,residentOps,residentBuilt,residentReject,residentInvalid,residentChecks;
	UINT64 globalGroups,globalBlocks,globalOps,globalBuilt,globalPatched,globalChecks,globalInvalid;
	UINT64 globalFence,globalCold,globalBudget,globalGuard,globalEager,globalSkipped,globalFull,globalDT,globalCooldown,globalMasked,globalReenabled;
};
static Cv1kV2Set *cv1kV2Fast;
static Cv1kV2Entry *cv1kV2Table;
static Cv1kV2Learn cv1kV2Learn[Cv1kV2LearnEntries];
static Sh3V2::CodeCache cv1kV2Code;
static Sh3V2::LinkDesc *cv1kV2Links;
static unsigned cv1kV2LinkCount;
static bool cv1kV2LinkEnabled;
// Fixed-size immutable regions; generated code has no references to
// movable directory entries. Regions share the existing 8 MiB cache.
static constexpr unsigned Cv1kC3MaxMembers=12,Cv1kC3MaxRegions=512;
struct Cv1kC3Member {
	const UINT16 *source;
	UINT32 pc,offset;
	UINT16 ops[Sh3Native::MaxOps];
	UINT16 watch;
	UINT8 count,body;
	bool trusted,hasPrediction;
	UINT32 prediction;
	uint64_t generation;
};
struct Cv1kC3Region {
	uint64_t mapVersion,opaque,slice,buildEpoch;
	unsigned count;
	bool active;
	Cv1kC3Member members[Cv1kC3MaxMembers];
};
static Cv1kC3Region *cv1kC3Regions;
static unsigned cv1kC3Count;
static bool cv1kC3Enabled;
// C4 pinned descriptors, one native body per promoted guest entry.
// The generated entry gate checks both an exact validation token and
// the live backing-page generation, even for an old incoming edge.
struct Cv1kC4Edge {UINT32 pc,at,linked;};
struct Cv1kC4Block {
	Sh3V2::GlobalStamp stamp;
	uint64_t mapVersion;
	const UINT16 *source;
	UINT32 pc,offset;
	UINT16 ops[Sh3Native::MaxOps];
	UINT8 count,body,watch,edges;
	bool active,trusted;
	UINT32 trialGroups,trialOps;
	uint64_t retrySlice;
	Cv1kC4Edge edge[3];
};
struct Cv1kC4Target {UINT32 pc,pad;const void *entry;};
static_assert(sizeof(Cv1kC4Target)==16 && offsetof(Cv1kC4Target,entry)==8,"C4 indirect map ABI");
static constexpr unsigned Cv1kC4MaxBlocks=Cv1kV2Entries;
static constexpr unsigned Cv1kC4MapSize=8192;
static Cv1kC4Block *cv1kC4Blocks;
static Cv1kC4Target *cv1kC4Map;
static unsigned cv1kC4Count,cv1kC4Unknown,cv1kC4EagerBudget,cv1kC4MinOps;
static bool cv1kC4Enabled,cv1kC4FrameOption,cv1kC4InFrame,cv1kC4LastInFrame,cv1kV2RepeatOnly;
static uint64_t cv1kC4Token,cv1kC4UnknownToken,cv1kC4LastFence,cv1kC4LastOpaque,cv1kC4LastMap,cv1kC4FrameEpoch;
static UINT16 cv1kC4CooldownQueue[Cv1kC4MaxBlocks];
static unsigned cv1kC4CooldownRead,cv1kC4CooldownWrite;
static inline uint64_t Cv1kC4BlockToken(const Cv1kC4Block &d){return d.trusted?cv1kC4Token:cv1kC4UnknownToken;}
static inline unsigned Cv1kC4MapIndex(UINT32 pc){return (pc>>1)&(Cv1kC4MapSize-1);}
static void Cv1kC4ResetCode(){
	cv1kC4Count=cv1kC4Unknown=cv1kC4EagerBudget=0;
	cv1kC4CooldownRead=cv1kC4CooldownWrite=0;++cv1kC4UnknownToken;
	if(cv1kC4Map)memset(cv1kC4Map,0,sizeof(Cv1kC4Target)*Cv1kC4MapSize);
	++cv1kC4Token;
	cv1kC4LastFence=~uint64_t(0);
}
static void Cv1kC4FrameScope(bool on){
	if(!cv1kC4Enabled||!cv1kC4FrameOption)return;
	cv1kC4InFrame=on;++cv1kC4FrameEpoch;
}
static inline void Cv1kC4RefreshToken(){
	const uint64_t fence=cv1kC4InFrame?cv1kC4FrameEpoch:cv1kV2SliceEpoch;
	// Unknown blocks get a fresh token on each outer invocation.
	// They cannot force all registered RAM/ROM blocks to revalidate.
	++cv1kC4UnknownToken;
	if(fence!=cv1kC4LastFence || cv1kC4InFrame!=cv1kC4LastInFrame ||
		cv1kV2OpaqueEpoch!=cv1kC4LastOpaque || cv1kV2MapVersion!=cv1kC4LastMap){
		++cv1kC4Token;cv1kC4LastFence=fence;cv1kC4LastInFrame=cv1kC4InFrame;
		cv1kC4LastOpaque=cv1kV2OpaqueEpoch;cv1kC4LastMap=cv1kV2MapVersion;
	}
}
static void Cv1kC4Disable(Cv1kC4Block &d);

struct Cv1kC3Prediction {
	UINT32 pc,target,hits;
	uint64_t mapVersion;
};
static constexpr unsigned Cv1kC3PredCount=1024;
static Cv1kC3Prediction cv1kC3Pred[Cv1kC3PredCount];
static UINT32 cv1kC3PredSerial;
static inline Cv1kC3Prediction &Cv1kC3PredictSlot(UINT32 pc){
	return cv1kC3Pred[((pc>>1)^(pc>>11)^(pc>>21))&(Cv1kC3PredCount-1)];
}
static void Cv1kC3RecordDynamic(UINT32 pc,UINT32 target){
	if(target&1)return;
	auto &p=Cv1kC3PredictSlot(pc);
	if(p.pc!=pc || p.mapVersion!=cv1kV2MapVersion || p.target!=target || !p.hits){
		p={pc,target,1,cv1kV2MapVersion};
	}else if(p.hits<2){
		++p.hits;++cv1kC3PredSerial;
	}
}
static bool Cv1kC3Predict(UINT32 pc,UINT32 &target){
	const auto &p=Cv1kC3PredictSlot(pc);
	if(p.pc!=pc || p.hits<2 || p.mapVersion!=cv1kV2MapVersion)return false;
	target=p.target;return true;
}
static Cv1kV2Stats cv1kV2Stats;
static uint64_t Cv1kC3BuildEpoch(){
	return (cv1kV2Stats.built<<32)|cv1kC3PredSerial;
}
static bool cv1kV2Full,cv1kV2LookupOnly,cv1kV2Compact,cv1kV2Blocks,cv1kV2Chain,cv1kV2SelfLoop;
static inline unsigned Cv1kV2Minimum(){return cv1kV2Blocks?2:Cv1kV2Min;}
static UINT32 cv1kV2SampleClock,cv1kV2Random;
static INT32 cv1kV2SampleAt=-1;
static void Cv1kV2InvalidateAll(){
	if(!cv1kV2Fast)return;
	memset(cv1kV2Fast,0,sizeof(Cv1kV2Set)*Cv1kV2Sets);
	Cv1kV2ClearPages();Cv1kV2WatchClear();
	cv1kV2LinkCount=0;cv1kC3Count=0;Cv1kC4ResetCode();
	memset(cv1kC3Pred,0,sizeof(cv1kC3Pred));cv1kC3PredSerial=0;
	memset(cv1kV2Learn,0,sizeof(cv1kV2Learn));
	cv1kV2Code.Reset();cv1kV2Full=false;cv1kV2SampleAt=-1;
	cv1kV2SampleClock=0;cv1kV2Random=0x957fea31u;
	++cv1kV2Stats.resets;
}
static void Cv1kV2Reset(){
	delete[] cv1kV2Fast;cv1kV2Fast=NULL;
	delete[] cv1kV2Table;cv1kV2Table=NULL;
	delete[] cv1kV2Links;cv1kV2Links=NULL;cv1kV2LinkCount=0;cv1kV2LinkEnabled=false;
	delete[] cv1kC3Regions;cv1kC3Regions=NULL;cv1kC3Count=0;cv1kC3Enabled=false;
	delete[] cv1kC4Blocks;cv1kC4Blocks=NULL;
	delete[] cv1kC4Map;cv1kC4Map=NULL;Cv1kC4ResetCode();
	cv1kC4Enabled=cv1kC4FrameOption=cv1kC4InFrame=cv1kV2RepeatOnly=false;
	memset(cv1kC3Pred,0,sizeof(cv1kC3Pred));cv1kC3PredSerial=0;
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
	cv1kV2RepeatOnly=cv1kV2SelfLoop && Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_REPEAT_ONLY"),false);
	if(!cv1kV2RepeatOnly && cv1kV2Chain && Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_LINK"),true)){
		cv1kV2Links=new(std::nothrow) Sh3V2::LinkDesc[Cv1kV2Entries];
		cv1kV2LinkEnabled=cv1kV2Links!=NULL;
	}
	Cv1kV2WatchEnable();
	if(!cv1kV2RepeatOnly && cv1kV2Chain && cv1kV2WriteWatch &&
		Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_RESIDENT"),true)){
		if(!cv1kV2LookupOnly && Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_GLOBAL"),true)){
			cv1kC4Blocks=new(std::nothrow) Cv1kC4Block[Cv1kC4MaxBlocks]();
			cv1kC4Map=new(std::nothrow) Cv1kC4Target[Cv1kC4MapSize]();
			cv1kC4Enabled=cv1kC4Blocks && cv1kC4Map;
			cv1kC4FrameOption=cv1kC4Enabled &&
				Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_FRAME_FENCE"),true);
			cv1kC4MinOps=8;
			if(const char *minOps=getenv("FBNEO_CV1K_V2_GLOBAL_MIN_OPS")){
				char *end=NULL;
				const unsigned long n=strtoul(minOps,&end,10);
				if(end!=minOps && !*end && n<=128)cv1kC4MinOps=unsigned(n);
			}
			if(!cv1kC4Enabled){
				delete[] cv1kC4Blocks;cv1kC4Blocks=NULL;
				delete[] cv1kC4Map;cv1kC4Map=NULL;
			}
		}
		if(!cv1kC4Enabled)cv1kC3Regions=new(std::nothrow) Cv1kC3Region[Cv1kC3MaxRegions]();
		cv1kC3Enabled=cv1kC4Enabled||cv1kC3Regions!=NULL;
		if(cv1kC3Enabled){
			delete[] cv1kV2Links;cv1kV2Links=NULL;cv1kV2LinkEnabled=false;
		}
	}
	return true;
}
static inline __attribute__((always_inline)) unsigned Cv1kV2Hash(UINT32 pc){
	const UINT32 h=pc>>1;
	return (h^(h>>11)^(h>>22))&(Cv1kV2Sets-1);
}
static inline __attribute__((always_inline)) Cv1kV2Slot *Cv1kV2FindRaw(UINT32 pc){
	Cv1kV2Slot *w=cv1kV2Fast[Cv1kV2Hash(pc)].way;
	for(unsigned i=0;i<Cv1kV2Ways;i++){
		if(!w[i].id)return NULL; // sets are packed, even after retirement
		if(w[i].pc==pc)return w+i;
	}
	return NULL;
}
static inline __attribute__((always_inline)) Cv1kV2Slot *Cv1kV2Find(UINT32 pc){
	const UINT8 *bits=cv1kV2PageTags[(pc&AM)>>SH3_SHIFT];
	if(!bits || !(bits[(pc&SH3_PAGEM)>>4]&(1u<<((pc>>1)&7))))return NULL;
	return Cv1kV2FindRaw(pc);
}
static void Cv1kC4MaskRoot(Cv1kC4Block &d){
	const unsigned id=unsigned(&d-cv1kC4Blocks)+1;
	if(cv1kC4CooldownWrite-cv1kC4CooldownRead>=Cv1kC4MaxBlocks)return;
	cv1kC4CooldownQueue[cv1kC4CooldownWrite++%Cv1kC4MaxBlocks]=id;
	// A physical bit is shared by up to eight virtual aliases. Do not
	// mask an unrelated live entry or a compact self-repeat.
	for(unsigned alias=0;alias<8;alias++){
		const UINT32 pc=(d.pc&AM)|(alias<<29);
		const auto *slot=Cv1kV2FindRaw(pc);
		if(!slot || (slot->id&Cv1kV2Blocked))continue;
		const auto &b=cv1kV2Table[slot->id-1];
		if(!b.global || cv1kC4Blocks[b.global-1].retrySlice<=cv1kV2SliceEpoch)return;
	}
	UINT8 *bits=cv1kV2PageTags[(d.pc&AM)>>SH3_SHIFT];
	if(bits){
		bits[(d.pc&SH3_PAGEM)>>4]&=~(1u<<((d.pc>>1)&7));
		++cv1kV2Stats.globalMasked;
	}
}
static void Cv1kC4WakeRoots(){
	while(cv1kC4CooldownRead!=cv1kC4CooldownWrite){
		const auto &d=cv1kC4Blocks[cv1kC4CooldownQueue[cv1kC4CooldownRead%Cv1kC4MaxBlocks]-1];
		if(d.retrySlice>cv1kV2SliceEpoch)break;
		++cv1kC4CooldownRead;
		if(!d.active)continue;
		UINT8 *bits=cv1kV2PageTags[(d.pc&AM)>>SH3_SHIFT];
		if(bits){bits[(d.pc&SH3_PAGEM)>>4]|=1u<<((d.pc>>1)&7);++cv1kV2Stats.globalReenabled;}
	}
}
static Sh3V2::LinkDesc *Cv1kV2LinkedTarget(UINT32 pc){
	const Cv1kV2Slot *s=Cv1kV2Find(pc);
	if(!s || (s->id&Cv1kV2Blocked))return NULL;
	const auto &b=cv1kV2Table[s->id-1];
	if(!b.link)return NULL;
	auto &d=cv1kV2Links[b.link-1];
	return d.active && d.mapVersion==cv1kV2MapVersion?&d:NULL;
}
static void Cv1kV2PatchLink(Sh3V2::LinkDesc &d,unsigned edge,UINT32 target){
	if(d.edge[edge]==target)return;
	const unsigned at=d.offset+(edge?Sh3V2::LinkFallOffset:Sh3V2::LinkTakenOffset);
	if(cv1kV2Code.PatchBranch(at,target?target:d.offset+Sh3V2::LinkExitOffset)){
		d.edge[edge]=target;++cv1kV2Stats.linkPatched;
	}
}
static void Cv1kV2DisableLink(Cv1kV2Entry &b){
	if(!b.link)return;
	auto &d=cv1kV2Links[b.link-1];
	d.active=0; // even an obsolete incoming B cannot enter this body
	for(unsigned i=0;i<cv1kV2LinkCount;i++){
		auto &s=cv1kV2Links[i];
		if(!s.active)continue;
		for(unsigned e=0;e<2;e++)if(s.edge[e]==d.offset)Cv1kV2PatchLink(s,e,0);
	}
}
static void Cv1kV2PublishLink(Cv1kV2Entry &b,UINT32 pc){
	// Preserve the C-1 self-loop fast path. Other compact blocks can
	// link without changing their existing leaf/wide register layout.
	if(!cv1kV2LinkEnabled || !b.kind || b.repeat || cv1kV2LinkCount>=Cv1kV2Entries)return;
	auto &d=cv1kV2Links[cv1kV2LinkCount];d={};
	d.body=(const UINT8*)cv1kV2Code.ExecBase()+b.offset;
	d.source=b.source;d.mapVersion=cv1kV2MapVersion;
	d.generation=b.generation;d.opaque=b.opaqueEpoch;d.slice=b.sliceEpoch;
	d.pc=pc;d.count=b.count;d.extra=b.extra;d.watch=b.watch;
	d.flags=b.kind==2?Sh3V2::LinkWide:0;
	if(b.watch && Cv1kV2TrustedCode(pc,MemMapF[(pc&AM)>>SH3_SHIFT]))d.flags|=Sh3V2::LinkTrusted;
	d.takenPC=d.fallPC=pc+b.count*2;
	if(b.body<b.count){
		const UINT16 op=b.ops[b.body];d.flags|=Sh3V2::LinkTerminal;
		if(Sh3V2::Delayed(op))d.flags|=Sh3V2::LinkDelayed;
		if(Sh3V2::Call(op))d.flags|=Sh3V2::LinkCall;
		if(Sh3V2::Dynamic(op))d.flags|=Sh3V2::LinkDynamic;
		if(Sh3V2::Conditional(op))d.flags|=Sh3V2::LinkConditional;
		d.takenPC=Sh3V2::StaticTarget(op,pc+b.body*2);
		d.fallPC=pc+(b.body+1)*2;
	}
	memcpy(d.ops,b.ops,b.count*sizeof(UINT16));
	if(!Sh3V2::PublishLink(cv1kV2Code,d))return;
	d.active=1;b.link=++cv1kV2LinkCount;++cv1kV2Stats.linkBuilt;
	if(!(d.flags&Sh3V2::LinkDynamic)){
		if(auto *t=Cv1kV2LinkedTarget(d.takenPC))Cv1kV2PatchLink(d,0,t->offset);
	}
	if(d.flags&Sh3V2::LinkConditional)
		if(auto *t=Cv1kV2LinkedTarget(d.fallPC))Cv1kV2PatchLink(d,1,t->offset);
	for(unsigned i=0;i+1<cv1kV2LinkCount;i++){
		auto &s=cv1kV2Links[i];
		if(!s.active || s.mapVersion!=cv1kV2MapVersion)continue;
		if(!(s.flags&Sh3V2::LinkDynamic) && s.takenPC==pc)Cv1kV2PatchLink(s,0,d.offset);
		if((s.flags&Sh3V2::LinkConditional) && s.fallPC==pc)Cv1kV2PatchLink(s,1,d.offset);
	}
}
static void Cv1kV2Retire(Cv1kV2Slot &slot){
	auto &retired=cv1kV2Table[(slot.id&~Cv1kV2Blocked)-1];
	Cv1kV2DisableLink(retired);
	if(retired.global)Cv1kC4Disable(cv1kC4Blocks[retired.global-1]);
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
	if(body+1>=available || !(pc+body*2+2) ||
		!(cv1kC4Enabled?Sh3V2::GlobalSafeSlot(code[body+1]):Sh3V2::SafeSlot(code[body+1])))return false;
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
static Cv1kC4Block *Cv1kC4Find(UINT32 pc){
	const auto *slot=Cv1kV2FindRaw(pc);
	if(!slot || (slot->id&Cv1kV2Blocked))return NULL;
	const auto &b=cv1kV2Table[slot->id-1];
	if(!b.global)return NULL;
	auto &d=cv1kC4Blocks[b.global-1];
	return d.active && d.pc==pc?&d:NULL;
}
static void Cv1kC4MapTarget(const Cv1kC4Block &d){
	auto &slot=cv1kC4Map[Cv1kC4MapIndex(d.pc)];
	const void *entry=(const UINT8*)cv1kV2Code.ExecBase()+d.offset;
	if(slot.entry!=entry)slot={d.pc,0,entry};
}
static void Cv1kC4Patch(Cv1kC4Edge &edge,const Cv1kC4Block *target){
	const UINT32 id=target?unsigned(target-cv1kC4Blocks)+1:0;
	if(edge.linked==id)return;
	if(cv1kV2Code.PatchBranch(edge.at,target?target->offset:edge.at+4)){
		edge.linked=id;++cv1kV2Stats.globalPatched;
	}
}
static void Cv1kC4Disable(Cv1kC4Block &d){
	if(!d.active)return;
	d.stamp.token=0;d.active=false;
	if(!d.trusted)--cv1kC4Unknown;
	auto &indirect=cv1kC4Map[Cv1kC4MapIndex(d.pc)];
	if(indirect.entry==(const UINT8*)cv1kV2Code.ExecBase()+d.offset)indirect={};
	const unsigned id=unsigned(&d-cv1kC4Blocks)+1;
	for(unsigned i=0;i<cv1kC4Count;i++){
		auto &src=cv1kC4Blocks[i];
		if(src.active)for(unsigned j=0;j<src.edges;j++)
			if(src.edge[j].linked==id)Cv1kC4Patch(src.edge[j],NULL);
	}
}
static bool Cv1kC4Validate(Cv1kC4Block &d){
	if(!d.active)return false;
	const uint64_t gen=*d.stamp.liveGeneration;
	if(d.stamp.token==Cv1kC4BlockToken(d) && d.stamp.generation==gen)return true;
	++cv1kV2Stats.globalChecks;
	unsigned available;
	const UINT16 *source=Cv1kV2Source(d.pc,available);
	if(d.mapVersion!=cv1kV2MapVersion || !source || source!=d.source ||
		d.count>available || memcmp(source,d.ops,d.count*sizeof(UINT16))){
		const UINT32 pc=d.pc;
		++cv1kV2Stats.globalInvalid;
		auto *slot=Cv1kV2FindRaw(pc);
		if(slot && cv1kV2Table[(slot->id&~Cv1kV2Blocked)-1].global==unsigned(&d-cv1kC4Blocks)+1){
			Cv1kV2Retire(*slot);++cv1kV2Stats.changed;++cv1kV2Stats.retired;
		}else Cv1kC4Disable(d);
		return false;
	}
	d.stamp.generation=gen;d.stamp.token=Cv1kC4BlockToken(d);
	return true;
}
// Return false only when the old, proven native self-repeat path
// should compile this exact span instead of a global block.
static bool Cv1kC4Compile(UINT32 pc,const UINT16 *code,unsigned available,Cv1kV2Learn &learn){
	unsigned body=0;
	while(body<available && (code[body]&0xf0ff)!=0x4011 &&
		Sh3V2::GlobalCandidate(code[body]) &&
		Sh3V2::CompactEmitter::ResidentEligible(code,body+1,body+1))++body;
	bool terminal=body<available && Cv1kV2Terminal(code,body,available,pc);
	unsigned count=body+(terminal?1+unsigned(Sh3V2::Delayed(code[body])):0);
	if(terminal && !Sh3V2::CompactEmitter::ResidentEligible(code,count,body)){
		terminal=false;count=body;
	}
	if(terminal && cv1kV2SelfLoop && !Sh3V2::Dynamic(code[body]) &&
		!Sh3V2::Call(code[body]) && Sh3V2::StaticTarget(code[body],pc+body*2)==pc){
		bool old=true;
		for(unsigned i=0;i<body;i++)old&=Cv1kV2Candidate(code[i]);
		if(Sh3V2::Delayed(code[body]))old&=Sh3V2::SafeSlot(code[body+1]);
		if(old && count>=2)return false;
	}
	if(count<(terminal?1u:2u)){Cv1kV2Reject(learn);return true;}
	const unsigned set=Cv1kV2Hash(pc);
	auto *ways=cv1kV2Fast[set].way;
	unsigned way=0;
	while(way<Cv1kV2Ways && ways[way].id)++way;
	if(way==Cv1kV2Ways){++cv1kV2Stats.setFull;Cv1kV2Reject(learn);return true;}
	if(cv1kC4Count>=Cv1kC4MaxBlocks){++cv1kV2Stats.globalFull;Cv1kV2Reject(learn);return true;}
	const unsigned watch=Cv1kV2WatchCode(MemMapF[(pc&AM)>>SH3_SHIFT]);
	if(!watch){Cv1kV2Reject(learn);return true;}
	auto &d=cv1kC4Blocks[cv1kC4Count];d={};
	d.pc=pc;d.count=count;d.body=body;d.source=code;d.watch=watch;
	d.mapVersion=cv1kV2MapVersion;d.stamp.liveGeneration=&cv1kV2Generations[watch];
	d.trusted=Cv1kV2TrustedCode(pc,MemMapF[(pc&AM)>>SH3_SHIFT]);
	memcpy(d.ops,code,count*sizeof(UINT16));
	UINT32 words[Sh3V2::CompactEmitter::ResidentMaxWords];
	Sh3V2::GlobalBuild global={};global.stamp=&d.stamp;global.trusted=d.trusted;
	unsigned nDT=0;
	// Only an included, guarded next instruction on the very same DATA
	// and FETCH backing may remove DT's live lookahead.
	const unsigned page=(pc&AM)>>SH3_SHIFT;
	if(MemMapR[page]==MemMapF[page])
		for(unsigned i=0;i<body && i+1<count;i++)
			if(Sh3V2::IsDT(code[i]) && code[i+1]!=0x8bfd){global.fastDT[i]=true;++nDT;}
	Sh3V2::CompactEmitter emit(words);unsigned unused=0;
	if(!emit.BuildResident(code,count,pc,body,NULL,0,NULL,unused,false,0,&global)){
		Cv1kV2Reject(learn);return true;
	}
	UINT8 *&bits=cv1kV2PageTags[page];
	if(!bits)bits=new(std::nothrow) UINT8[SH3_PAGE_SIZE/16]();
	if(!bits){Cv1kV2Reject(learn);return true;}
	if(!cv1kV2Code.Publish(words,emit.End()-words,d.offset)){
		cv1kV2Full=true;++cv1kV2Stats.full;++cv1kV2Stats.globalFull;
		return true;
	}
	d.edges=global.count;
	for(unsigned i=0;i<d.edges;i++)
		d.edge[i]={global.target[i],d.offset+UINT32((global.patch[i]-words)*4),0};
	d.active=true;if(!d.trusted)++cv1kC4Unknown;
	const unsigned globalID=++cv1kC4Count;
	const UINT32 id=set*Cv1kV2Ways+way+1;
	auto &b=cv1kV2Table[id-1];b={};
	b.source=code;b.offset=d.offset;b.count=count;b.body=body;b.kind=3;b.watch=watch;
	b.global=globalID;b.extra=terminal?Sh3V2::ExtraCycles(code[body]):0;
	b.generation=cv1kV2Generations[watch];b.opaqueEpoch=cv1kV2OpaqueEpoch;b.sliceEpoch=cv1kV2SliceEpoch;
	memcpy(b.ops,code,count*sizeof(UINT16));ways[way]={pc,id};
	bits[(pc&SH3_PAGEM)>>4]|=1u<<((pc>>1)&7);
	Cv1kC4MapTarget(d);
	for(unsigned i=0;i<d.edges;i++)
		if(auto *t=Cv1kC4Find(d.edge[i].pc))Cv1kC4Patch(d.edge[i],t);
	for(unsigned i=0;i+1<cv1kC4Count;i++){
		auto &src=cv1kC4Blocks[i];
		if(src.active && src.mapVersion==cv1kV2MapVersion)
			for(unsigned j=0;j<src.edges;j++)if(src.edge[j].pc==pc)Cv1kC4Patch(src.edge[j],&d);
	}
	++cv1kV2Stats.globalBuilt;cv1kV2Stats.globalDT+=nDT;
	++cv1kV2Stats.compactBuilt;cv1kV2Stats.compactBytes+=(emit.End()-words)*4;
	++cv1kV2Stats.extendedBuilt;
	if(terminal)++cv1kV2Stats.terminalBuilt;
	++cv1kV2Stats.built;++cv1kNativeBuilt;
	return true;
}
// The large emission scratch and all compile work belong to a cold
// function; neither the compiled-hit nor the inline miss path needs it.
static __attribute__((noinline,cold)) void Cv1kV2Compile(
	UINT32 pc,const UINT16 *code,unsigned available,Cv1kV2Learn &learn)
{
	if(cv1kC4Enabled && Cv1kC4Compile(pc,code,available,learn))return;
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
	if(cv1kV2RepeatOnly && !repeat){Cv1kV2Reject(learn);return;}
	if(cv1kV2Compact){
		Sh3V2::CompactEmitter emit(words);
		if(emit.Build(code,n,pc,body,cv1kV2Blocks,cv1kV2WriteWatch!=NULL,repeat)){
			wordCount=emit.End()-words;kind=emit.Wide()?2:1;
		}
	}
	if(!kind){
		if(cv1kV2RepeatOnly){Cv1kV2Reject(learn);return;}
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
	b.repeat=repeat;b.writes=0;b.link=0;b.region=0;b.global=0;b.regionMember=0;b.regionTried=~uint64_t(0);
	b.watch=Cv1kV2WatchCode(MemMapF[(pc&AM)>>SH3_SHIFT]);
	b.generation=b.watch?cv1kV2Generations[b.watch]:0;
	b.opaqueEpoch=cv1kV2OpaqueEpoch;b.sliceEpoch=cv1kV2SliceEpoch;
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
	if(!cv1kC3Enabled)Cv1kV2PublishLink(b,pc);
	if(repeat)++cv1kV2Stats.selfBuilt;
	if(terminal)++cv1kV2Stats.terminalBuilt;
	if(extended)++cv1kV2Stats.extendedBuilt;
	if(kind){++cv1kV2Stats.compactBuilt;cv1kV2Stats.compactBytes+=wordCount*4;}
	else ++cv1kV2Stats.legacyBuilt;
	++cv1kV2Stats.built;++cv1kNativeBuilt;
}
static __attribute__((noinline,cold)) void Cv1kV2Observe(UINT32 pc){
	if(cv1kV2Full || !cv1kV2Fast || !Cv1kV2Allowed() || (cv1kC4Enabled?Cv1kV2FindRaw(pc):Cv1kV2Find(pc)))return;
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
	const uint64_t generation=b.watch?cv1kV2Generations[b.watch]:0;
	const bool dirty=b.watch && (generation!=b.generation || b.opaqueEpoch!=cv1kV2OpaqueEpoch);
	if(dirty)++cv1kV2Stats.dirtyChecks;
	const bool verify=dirty || b.sliceEpoch!=cv1kV2SliceEpoch || !b.watch ||
		!Cv1kV2TrustedCode(pc,MemMapF[(pc&AM)>>SH3_SHIFT]);
	if(verify)++cv1kV2Stats.verifyChecks;else ++cv1kV2Stats.verifySkipped;
	if(!code || b.source!=code || b.count>available ||
		(verify && memcmp(b.ops,code,b.count*sizeof(UINT16)))){
		// Live code/remap invalidation, never a hash collision. Retire
		// the immutable code; its storage is not reused mid-execution.
		if(b.watch && !dirty)++cv1kV2Stats.untracked;
		Cv1kV2Retire(slot);++cv1kV2Stats.changed;++cv1kV2Stats.retired;
		return 0;
	}
	if(verify){b.generation=generation;b.opaqueEpoch=cv1kV2OpaqueEpoch;b.sliceEpoch=cv1kV2SliceEpoch;}
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
		size_t(b.count)*sizeof(UINT16),iterations,pc,idle,cv1kIdleAddress,m_pr,cv1kV2WriteWatch,cv1kV2Generations};
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
			Cv1kV2DisableLink(b);
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
// One C++ entry into a bounded group of patched ARM64 links. No
// callbacks occur inside the group; debit the exact slice cost on exit.
static __attribute__((noinline)) unsigned Cv1kV2RunLinked(Sh3V2::LinkDesc &d){
	if(!Cv1kV2Allowed())return 0;
	const UINT8 *idle=NULL;
	if(cv1kV2Blocks && cv1kIdleBatchEnabled && Cv1kIsIdlePage(MemMapRL[0xc00]))idle=cv1kIdlePage;
	Sh3V2::LinkContext c={};
	c.memory={MemMapR,MemMapRL,MemMapW,d.source,size_t(d.count)*2,1,m_pc,
		idle,cv1kIdleAddress,m_pr,cv1kV2WriteWatch,cv1kV2Generations};
	c.mapVersion=cv1kV2MapVersion;c.slice=cv1kV2SliceEpoch;
	c.remaining=m_sh4_icount;c.ppc=m_ppc;c.limit=128;
	fbneo_sh3_v2_link_enter((const UINT8*)cv1kV2Code.ExecBase()+d.offset,m_r,&m_sr,&m_ea,&c);
	++cv1kV2Stats.linkGroups;cv1kV2Stats.linkBodies+=c.blocks;
	cv1kV2Stats.linkEdges+=c.attempts?c.attempts-1:0;
	if(c.status)++cv1kV2Stats.linkStops;
	cv1kV2Stats.ready+=c.attempts;cv1kV2Stats.probes+=c.attempts;
	cv1kV2Stats.hits+=c.blocks;cv1kNativeSpans+=c.blocks;
	cv1kV2Stats.compactCalls+=c.blocks;cv1kV2Stats.compactOps+=c.ops;
	cv1kV2Stats.verifyChecks+=c.checks;cv1kV2Stats.verifySkipped+=c.skips;
	cv1kV2Stats.dirtyChecks+=c.dirty;cv1kV2Stats.untracked+=c.untracked;
	cv1kV2Stats.terminalCalls+=c.taken+c.notTaken;
	cv1kV2Stats.taken+=c.taken;cv1kV2Stats.notTaken+=c.notTaken;
	cv1kV2Stats.guards+=c.guards;
	if(c.status==1){
		if(auto *s=Cv1kV2Find(c.memory.pc)){
			Cv1kV2Retire(*s);++cv1kV2Stats.changed;++cv1kV2Stats.retired;
		}
	}else if(c.status==2){
		if(auto *s=Cv1kV2Find(c.guardPC)){
			auto &b=cv1kV2Table[s->id-1];
			if(b.link && cv1kV2Links[b.link-1].shortExits>=8){
				s->id|=Cv1kV2Blocked;++cv1kV2Stats.disabled;Cv1kV2DisableLink(b);
			}
		}
	}
	if(!c.ops)return 0;
	const unsigned spent=unsigned(m_sh4_icount)-c.remaining;
	m_pc=c.memory.pc;m_ppc=c.ppc;m_pr=c.memory.pr;EAT(spent);
	cv1kV2Stats.ops+=c.ops;cv1kV2Stats.cycles+=spent;cv1kNativeOps+=c.ops;
	++cv1kV2Stats.chainGroups;
	const unsigned completed=c.blocks-unsigned(c.status==2 && !c.guardOps);
	if(completed)cv1kV2Stats.chainJoined+=completed-1;
	return c.ops;
}
// C-3: compile a bounded region of already learned exact blocks. Each
// internal edge goes directly to a body in the same immutable region.
// No external pointer survives the region invocation (including RAS).
static Cv1kV2Entry *Cv1kC3Candidate(UINT32 pc){
	auto *s=Cv1kV2Find(pc);
	if(!s || (s->id&Cv1kV2Blocked))return NULL;
	auto &b=cv1kV2Table[s->id-1];
	if(!b.kind || b.repeat || !b.watch ||
		!Sh3V2::CompactEmitter::ResidentEligible(b.ops,b.count,b.body))return NULL;
	unsigned available;
	const UINT16 *source=Cv1kV2Source(pc,available);
	if(!source || source!=b.source || b.count>available ||
		memcmp(source,b.ops,b.count*sizeof(UINT16)))return NULL;
	return &b;
}
static __attribute__((noinline,cold)) bool Cv1kC3Build(UINT32 pc,Cv1kV2Entry &root){
	const uint64_t epoch=Cv1kC3BuildEpoch();
	if(root.regionTried==epoch || cv1kC3Count>=Cv1kC3MaxRegions)return false;
	root.regionTried=epoch;
	unsigned available;
	const UINT16 *source=Cv1kV2Source(pc,available);
	if(!source || source!=root.source || root.count>available ||
		memcmp(source,root.ops,root.count*sizeof(UINT16))){
		if(auto *slot=Cv1kV2Find(pc)){
			Cv1kV2Retire(*slot);++cv1kV2Stats.changed;++cv1kV2Stats.retired;
		}
		return false;
	}
	Cv1kC3Region region={};
	UINT32 pcs[Cv1kC3MaxMembers];
	auto add=[&](UINT32 target){
		if(region.count==Cv1kC3MaxMembers)return;
		for(unsigned i=0;i<region.count;i++)if(pcs[i]==target)return;
		auto *b=Cv1kC3Candidate(target);
		if(!b)return;
		auto &m=region.members[region.count];
		pcs[region.count++]=target;
		m.pc=target;m.source=b->source;m.count=b->count;m.body=b->body;
		m.watch=b->watch;m.generation=cv1kV2Generations[b->watch];
		m.trusted=Cv1kV2TrustedCode(target,MemMapF[(target&AM)>>SH3_SHIFT]);
		memcpy(m.ops,b->ops,b->count*sizeof(UINT16));
	};
	add(pc);
	for(unsigned i=0;i<region.count;i++){
		auto &m=region.members[i];
		if(m.body==m.count){add(m.pc+m.count*2);continue;}
		const UINT16 op=m.ops[m.body];
		const UINT32 branchPC=m.pc+m.body*2;
		if(!Sh3V2::Dynamic(op))add(Sh3V2::StaticTarget(op,branchPC));
		else if(op!=0x000b && Cv1kC3Predict(branchPC,m.prediction)){
			m.hasPrediction=true;add(m.prediction);
		}
		if(Sh3V2::Conditional(op))add(branchPC+2);
		if(Sh3V2::Call(op))add(branchPC+4);
	}
	if(region.count<2)return false;
	// Each member has independent bounded emission scratch. The final
	// linked region is published atomically with its local B/ADR fixups.
	const unsigned capacity=region.count*Sh3V2::CompactEmitter::ResidentMaxWords;
	UINT32 *words=new(std::nothrow) UINT32[capacity];
	if(!words)return false;
	Sh3V2::ResidentFixup fix[Cv1kC3MaxMembers*4];
	unsigned nFix=0,used=0;
	bool ok=true;
	for(unsigned i=0;i<region.count;i++){
		auto &m=region.members[i];m.offset=used*4;
		Sh3V2::CompactEmitter emit(words+used);
		if(!emit.BuildResident(m.ops,m.count,m.pc,m.body,pcs,region.count,fix,nFix,m.hasPrediction,m.prediction)){
			ok=false;break;
		}
		used=emit.End()-words;
		if(used>capacity){ok=false;break;}
	}
	if(ok)for(unsigned i=0;i<nFix;i++){
		unsigned j=0;
		while(j<region.count && pcs[j]!=fix[i].target)++j;
		if(j==region.count){ok=false;break;}
		const int32_t disp=int32_t(region.members[j].offset/4)-int32_t(fix[i].at-words);
		if(fix[i].adr){
			const int32_t bytes=disp*4;
			if(bytes<-(1<<20)||bytes>=(1<<20)){ok=false;break;}
			*fix[i].at=0x10000010u|((uint32_t(bytes)&3)<<29)|
				(((uint32_t(bytes)>>2)&0x7ffffu)<<5);
		}else{
			if(disp<-(1<<25)||disp>=(1<<25)){ok=false;break;}
			*fix[i].at=0x14000000u|(uint32_t(disp)&0x3ffffffu);
		}
	}
	UINT32 offset=0;
	if(ok)ok=cv1kV2Code.Publish(words,used,offset);
	delete[] words;
	if(!ok){++cv1kV2Stats.residentReject;return false;}
	region.mapVersion=cv1kV2MapVersion;region.opaque=cv1kV2OpaqueEpoch;
	region.slice=cv1kV2SliceEpoch;region.active=true;region.buildEpoch=epoch;
	for(unsigned i=0;i<region.count;i++)region.members[i].offset+=offset;
	const unsigned id=++cv1kC3Count;
	cv1kC3Regions[id-1]=region;
	for(unsigned i=0;i<region.count;i++){
		// All members can be entered directly from the interpreter.
		// Existing regions remain immutable and independently validated.
		if(auto *b=Cv1kC3Candidate(pcs[i])){
			b->region=id;b->regionMember=i;
		}
	}
	++cv1kV2Stats.residentBuilt;
	return true;
}
static bool Cv1kC3Validate(Cv1kC3Region &r){
	if(!r.active || r.mapVersion!=cv1kV2MapVersion)return false;
	const bool fence=r.opaque!=cv1kV2OpaqueEpoch || r.slice!=cv1kV2SliceEpoch;
	for(unsigned i=0;i<r.count;i++){
		auto &m=r.members[i];
		const uint64_t generation=cv1kV2Generations[m.watch];
		// Unknown backing memory keeps exact comparisons at each group.
		// No code checks are needed between the members: no MMIO or
		// callbacks run inside the region; watched stores exit before write.
		if(!m.trusted || fence || m.generation!=generation){
			++cv1kV2Stats.residentChecks;
			unsigned available;
			const UINT16 *source=Cv1kV2Source(m.pc,available);
			if(!source||source!=m.source||m.count>available||
				memcmp(source,m.ops,m.count*sizeof(UINT16)))return false;
			m.generation=generation;
		}
	}
	r.opaque=cv1kV2OpaqueEpoch;r.slice=cv1kV2SliceEpoch;
	return true;
}
static __attribute__((noinline)) unsigned Cv1kC3Run(UINT32 pc,Cv1kV2Entry &b){
	if(!Cv1kV2Allowed())return 0;
	if(b.region && !cv1kC3Regions[b.region-1].active){b.region=0;b.regionTried=~uint64_t(0);}
	if(!b.region && !Cv1kC3Build(pc,b))return 0;
	if(!b.region)return 0;
	auto &r=cv1kC3Regions[b.region-1];
	if(!Cv1kC3Validate(r)){
		r.active=false;b.region=0;b.regionTried=~uint64_t(0);++cv1kV2Stats.residentInvalid;
		for(unsigned i=0;i<r.count;i++){
			const auto &m=r.members[i];
			auto *slot=Cv1kV2Find(m.pc);
			if(!slot || (slot->id&Cv1kV2Blocked))continue;
			const auto &entry=cv1kV2Table[slot->id-1];
			unsigned available;
			const UINT16 *source=Cv1kV2Source(m.pc,available);
			if(!source || source!=entry.source || entry.count>available ||
				memcmp(source,entry.ops,entry.count*sizeof(UINT16))){
				Cv1kV2Retire(*slot);++cv1kV2Stats.changed;++cv1kV2Stats.retired;
			}
		}
		// The next interpreter visit and ordinary learning can rebuild.
		// Do not dereference a stale native address or replay instructions.
		return 0;
	}
	const auto &member=r.members[b.regionMember];
	if(member.pc!=pc)return 0;
	const UINT8 *idle=NULL;
	if(cv1kIdleBatchEnabled && Cv1kIsIdlePage(MemMapRL[0xc00]))idle=cv1kIdlePage;
	Sh3V2::ResidentContext c={};
	c.memory={MemMapR,MemMapRL,MemMapW,NULL,0,1,pc,idle,cv1kIdleAddress,m_pr,
		cv1kV2WriteWatch,cv1kV2Generations};
	c.remaining=m_sh4_icount;c.ppc=m_ppc;
	fbneo_sh3_v3_enter((const UINT8*)cv1kV2Code.ExecBase()+member.offset,
		m_r,&m_sr,&m_ea,&c);
	++cv1kV2Stats.residentGroups;
	cv1kV2Stats.residentBlocks+=c.blocks;cv1kV2Stats.residentOps+=c.ops;
	if(c.dynamicSite){
		const UINT32 site=c.dynamicSite&~1u;
		Cv1kC3RecordDynamic(site,c.dynamicTarget);
		UINT32 predicted;
		if(r.buildEpoch!=Cv1kC3BuildEpoch() && Cv1kC3Predict(site,predicted) &&
			predicted==c.dynamicTarget && Cv1kC3Candidate(predicted)){
			// Rebuild only after new learning/compilation, outside native execution.
			r.active=false;b.region=0;b.regionTried=~uint64_t(0);
		}
	}
	if(!c.ops)return 0;
	const unsigned spent=unsigned(m_sh4_icount)-c.remaining;
	m_pc=c.memory.pc;m_ppc=c.ppc;m_pr=c.memory.pr;EAT(spent);
	cv1kV2Stats.hits+=c.blocks;cv1kV2Stats.compactCalls+=c.blocks;
	cv1kV2Stats.compactOps+=c.ops;cv1kNativeSpans+=c.blocks;
	cv1kV2Stats.ops+=c.ops;cv1kV2Stats.cycles+=spent;cv1kNativeOps+=c.ops;
	return c.ops;
}
// Compile the actually reached successor (or a bounded known static
// frontier), without waiting for another sparse sampling hit.
static __attribute__((noinline,cold)) bool Cv1kC4Eager(UINT32 pc){
	if(!cv1kC4EagerBudget || cv1kV2Full || cv1kC4Count>=Cv1kC4MaxBlocks || Cv1kV2FindRaw(pc))return false;
	unsigned available;
	const UINT16 *code=Cv1kV2Source(pc,available);
	if(!code)return false;
	const UINT32 key=pc>>1;
	auto &l=cv1kV2Learn[(key^(key>>10)^(key>>20))&(Cv1kV2LearnEntries-1)];
	if(l.pc==pc && l.epoch==sh3FetchMapEpoch && l.first==code[0] && l.rejected)return false;
	--cv1kC4EagerBudget;
	l={pc,sh3FetchMapEpoch,code[0],Cv1kV2Hot,0};
	Cv1kV2Compile(pc,code,available,l);
	if(Cv1kV2FindRaw(pc)){++cv1kV2Stats.globalEager;return true;}
	return false;
}
static __attribute__((noinline,cold)) bool Cv1kC4Warm(Cv1kC4Block &root){
	if(!Cv1kC4Validate(root))return false;
	// Validate a small reachable frontier once per fence, not all
	// blocks in the cache on every entry. Every native target still
	// has its own exact token/generation guard.
	Cv1kC4Block *queue[16]={&root};
	unsigned size=1;
	for(unsigned i=0;i<size;i++){
		auto &d=*queue[i];
		for(unsigned j=0;j<d.edges && size<16;j++){
			const UINT32 pc=d.edge[j].pc;
			auto *t=Cv1kC4Find(pc);
			if(!t && Cv1kC4Eager(pc))t=Cv1kC4Find(pc);
			if(!t || !Cv1kC4Validate(*t))continue;
			bool seen=false;
			for(unsigned k=0;k<size;k++)if(queue[k]==t){seen=true;break;}
			if(!seen)queue[size++]=t;
		}
	}
	return true;
}
static __attribute__((noinline)) unsigned Cv1kC4Run(UINT32 pc,Cv1kC4Block *root){
	if(!Cv1kV2Allowed())return 0;
	Cv1kC4RefreshToken();
	unsigned total=0,groups=0;
	auto finish=[&](){
		// Sample native entry value, not merely number of compiled ops.
		// A short/guard-heavy root cools down without unpatching incoming
		// edges: it may still be profitable inside someone else's chain.
		if(cv1kC4MinOps && groups && root->active){
			root->trialGroups+=groups;root->trialOps+=total;
			if(root->trialGroups>=32){
				if(root->trialOps<root->trialGroups*cv1kC4MinOps){
					root->retrySlice=cv1kV2SliceEpoch+480;Cv1kC4MaskRoot(*root);
					++cv1kV2Stats.globalCooldown;
				}
				root->trialGroups=root->trialOps=0;
			}
		}
		return total;
	};
	// Bounded cold retries: validation/link learning happens outside
	// generated execution. Ordinary memory/MMIO/budget exits return
	// immediately to the exact original interpreter.
	for(unsigned retry=0;retry<6;retry++){
		auto *d=retry?Cv1kC4Find(pc):root;
		if(!d){
			if(!retry || !Cv1kC4Eager(pc))break;
			d=Cv1kC4Find(pc);if(!d)break;
		}
		if(d->stamp.token!=Cv1kC4BlockToken(*d) || d->stamp.generation!=*d->stamp.liveGeneration){
			if(!Cv1kC4Warm(*d))break;
		}
		if(unsigned(m_sh4_icount)<d->count)break;
		Cv1kC4MapTarget(*d);
		const UINT8 *idle=NULL;
		if(cv1kIdleBatchEnabled && Cv1kIsIdlePage(MemMapRL[0xc00]))idle=cv1kIdlePage;
		// RAS starts with depth zero in the trampoline. No zeroing
		// or reading of uninitialized slots is needed.
		Sh3V2::ResidentContext c;
		c.memory={MemMapR,MemMapRL,MemMapW,NULL,0,1,pc,idle,cv1kIdleAddress,m_pr,
			cv1kV2WriteWatch,cv1kV2Generations};
		c.remaining=m_sh4_icount;c.ppc=m_ppc;c.token=cv1kC4Token;
		c.dynamicMap=cv1kC4Map;c.reason=0;c.gbr=m_gbr;c.macl=m_macl;c.unknownToken=cv1kC4UnknownToken;
		fbneo_sh3_v3_enter((const UINT8*)cv1kV2Code.ExecBase()+d->offset,
			m_r,&m_sr,&m_ea,&c);
		++groups;++cv1kV2Stats.globalGroups;cv1kV2Stats.globalBlocks+=c.blocks;cv1kV2Stats.globalOps+=c.ops;
		if(c.ops){
			const unsigned spent=unsigned(m_sh4_icount)-c.remaining;
			m_pc=c.memory.pc;m_ppc=c.ppc;m_pr=c.memory.pr;m_macl=c.macl;EAT(spent);
			cv1kV2Stats.hits+=c.blocks;cv1kV2Stats.compactCalls+=c.blocks;
			cv1kV2Stats.compactOps+=c.ops;cv1kNativeSpans+=c.blocks;
			cv1kV2Stats.ops+=c.ops;cv1kV2Stats.cycles+=spent;cv1kNativeOps+=c.ops;
			total+=c.ops;
		}
		switch(c.reason){
		case 1:++cv1kV2Stats.globalFence;break;
		case 2:++cv1kV2Stats.globalCold;break;
		case 3:++cv1kV2Stats.globalBudget;return finish();
		case 4:++cv1kV2Stats.globalGuard;return finish();
		default:return finish();
		}
		pc=c.memory.pc;
		if(!Cv1kV2Allowed() || m_sh4_icount<2)break;
		// Even a zero-op invalid edge still has the previous exact PC.
		if(!c.ops && m_pc!=pc)break;
	}
	return finish();
}
static inline __attribute__((always_inline)) unsigned Cv1kV2Try(UINT32 pc,bool backward=false,bool chain=false){
	// Only selected in a dispatcher with an allocated directory.
	// Ordinary misses do not enter a C function or touch large metadata.
	Cv1kV2Slot *s=Cv1kV2Find(pc);
	if(s){
		if(s->id&Cv1kV2Blocked)return 0;
		if(cv1kC4Enabled && cv1kV2Table[s->id-1].global){
			auto &d=cv1kC4Blocks[cv1kV2Table[s->id-1].global-1];
			if(d.retrySlice>cv1kV2SliceEpoch){++cv1kV2Stats.globalSkipped;return 0;}
			return Cv1kC4Run(pc,&d);
		}
		if(cv1kV2RepeatOnly){
			if(!cv1kV2Table[s->id-1].repeat)return 0;
			const unsigned done=Cv1kV2Run(*s,chain);
			if(done && chain)++cv1kV2Stats.chainGroups;
			return done;
		}
		if(chain && cv1kV2Chain){
			if(cv1kC3Enabled && !cv1kV2LookupOnly){
				auto &b=cv1kV2Table[s->id-1];
				if(!b.repeat)return cv1kC4Enabled?0:Cv1kC3Run(pc,b);
				const unsigned done=Cv1kV2Run(*s,true);
				if(done)++cv1kV2Stats.chainGroups;
				return done;
			}
			if(!cv1kV2LinkEnabled)return Cv1kV2RunChain(*s);
			const auto &b=cv1kV2Table[s->id-1];
			if(!cv1kV2LookupOnly && b.link){
				auto &d=cv1kV2Links[b.link-1];
				if(d.active && d.mapVersion==cv1kV2MapVersion && (d.edge[0]||d.edge[1]))
					return Cv1kV2RunLinked(d);
			}
			// No expensive C++ chain in the C-2 default path.
			// Tiny unlinked blocks stay in the interpreter until a link exists.
			if(!cv1kV2LookupOnly && !b.repeat && b.count<Cv1kV2Min)return 0;
			const unsigned done=Cv1kV2Run(*s,true);
			if(done)++cv1kV2Stats.chainGroups;
			return done;
		}
		return Cv1kV2Run(*s);
	}
	if((backward || cv1kV2Blocks) && cv1kV2SampleAt>=m_sh4_icount && m_sh4_icount>=INT32(Cv1kV2Minimum()) && !cv1kV2Full){
		cv1kV2SampleAt=-1;
		Cv1kV2Observe(pc);
	}
	return 0;
}
static inline __attribute__((always_inline)) void Cv1kV2SliceSample(){
	++cv1kV2SliceEpoch; // fence external RAM access between CPU slices
	if(cv1kC4Enabled){cv1kC4EagerBudget=4;Cv1kC4WakeRoots();}
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
static inline void Cv1kC4FrameScope(bool){}
static inline unsigned Cv1kV2Try(UINT32,bool=false,bool=false){return 0;}
static inline void Cv1kV2SliceSample(){}
#endif
