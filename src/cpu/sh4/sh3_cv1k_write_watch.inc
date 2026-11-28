// C-2: compact alias-aware write generations. P4 and ambiguous
// mappings use the opaque epoch. Only registered CV1000 backing pages
// may skip comparisons, and a new CPU slice always revalidates them.
#if SH3_NATIVE_V2
static constexpr unsigned Cv1kV2WatchLimit=254,Cv1kV2Opaque=255;
static constexpr unsigned Cv1kV2WatchMapSize=(AM>>SH3_SHIFT)+1;
static uintptr_t cv1kV2WatchPages[Cv1kV2WatchLimit];
static uint64_t cv1kV2Generations[256];
static uint64_t &cv1kV2OpaqueEpoch=cv1kV2Generations[Cv1kV2Opaque];
static UINT8 *cv1kV2WriteWatch;
static unsigned cv1kV2WatchCount;
static uint64_t cv1kV2MapVersion,cv1kV2SliceEpoch;
static UINT8 cv1kV2TrackedHandlers;
static UINT8 *cv1kV2TrustedRam,*cv1kV2TrustedRom;
static UINT32 cv1kV2TrustedRamSize;
static bool cv1kV2VerifyAlways;
static bool Cv1kV2RangesOverlap(uintptr_t a,size_t n,uintptr_t b,size_t m){
	return a<=b?b-a<n:a-b<m;
}
static void Cv1kV2BindWritePage(unsigned id){
	const uintptr_t base=cv1kV2WatchPages[id-1];
	for(unsigned i=0;i<Cv1kV2WatchMapSize;i++){
		const uintptr_t w=(uintptr_t)MemMapW[i];
		if(w<SH3_MAXHANDLER || !Cv1kV2RangesOverlap(w,SH3_PAGE_SIZE,base,SH3_PAGE_SIZE))continue;
		UINT8 &entry=cv1kV2WriteWatch[i];
		entry=(w!=base || (entry && entry!=id))?Cv1kV2Opaque:id;
	}
}
static void Cv1kV2RevokeTrust(){
	if(cv1kV2TrustedRam || cv1kV2TrustedRom)++cv1kV2MapVersion;
	cv1kV2TrustedRam=cv1kV2TrustedRom=NULL;cv1kV2TrustedRamSize=0;
}
static void Cv1kV2WriteMapChanged(){
	++cv1kV2MapVersion;Cv1kV2RevokeTrust();
	if(!cv1kV2WriteWatch)return;
	++cv1kV2OpaqueEpoch;
	memset(cv1kV2WriteWatch,0,Cv1kV2WatchMapSize);
	for(unsigned i=1;i<=cv1kV2WatchCount;i++)Cv1kV2BindWritePage(i);
}
static unsigned Cv1kV2WatchCode(const void *base){
	if(!cv1kV2WriteWatch)return 0;
	const uintptr_t a=(uintptr_t)base;
	for(unsigned i=0;i<cv1kV2WatchCount;i++)if(cv1kV2WatchPages[i]==a)return i+1;
	if(cv1kV2WatchCount==Cv1kV2WatchLimit)return 0;
	cv1kV2WatchPages[cv1kV2WatchCount++]=a;
	cv1kV2Generations[cv1kV2WatchCount]=0;
	Cv1kV2BindWritePage(cv1kV2WatchCount);return cv1kV2WatchCount;
}
static inline __attribute__((always_inline)) void Cv1kV2NoteWrite(UINT32 a,unsigned n){
	if(!cv1kV2WriteWatch)return;
	if(a>=0xe0000000 || n>SH3_PAGE_SIZE-(a&SH3_PAGEM)){++cv1kV2OpaqueEpoch;return;}
	const unsigned id=cv1kV2WriteWatch[(a&AM)>>SH3_SHIFT];
	if(id)++cv1kV2Generations[id];
}
static inline void Cv1kV2NoteOpaqueWrite(){
	if(cv1kV2WriteWatch)++cv1kV2OpaqueEpoch;
}
static inline void Cv1kV2NoteHandlerWrite(unsigned id){
	if(!(cv1kV2TrackedHandlers&(1u<<id)))Cv1kV2NoteOpaqueWrite();
}
static void Cv1kV2SetTracked(unsigned i,bool tracked){
	if(i>=SH3_MAXHANDLER)return;
	cv1kV2TrackedHandlers=(cv1kV2TrackedHandlers&~(1u<<i))|(unsigned(tracked)<<i);
	Cv1kV2NoteOpaqueWrite();
}
static void Cv1kV2WatchClear(){
	cv1kV2WatchCount=0;++cv1kV2OpaqueEpoch;++cv1kV2MapVersion;++cv1kV2SliceEpoch;
	if(cv1kV2WriteWatch)memset(cv1kV2WriteWatch,0,Cv1kV2WatchMapSize);
}
static void Cv1kV2WatchReset(){
	delete[] cv1kV2WriteWatch;cv1kV2WriteWatch=NULL;
	cv1kV2WatchCount=0;memset(cv1kV2Generations,0,sizeof(cv1kV2Generations));
	++cv1kV2MapVersion;++cv1kV2SliceEpoch;cv1kV2VerifyAlways=true;
}
static void Cv1kV2WatchEnable(){
	cv1kV2VerifyAlways=Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_VERIFY_ALWAYS"),false);
	if(!Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_WRITE_AUDIT"),true))return;
	cv1kV2WriteWatch=new(std::nothrow) UINT8[Cv1kV2WatchMapSize]();
}
static bool Cv1kV2TrustedCode(UINT32 pc,const UINT8 *base){
	if(cv1kV2VerifyAlways || !cv1kV2WriteWatch || pc>=0xe0000000)return false;
	const UINT32 a=(pc&AM)&~SH3_PAGEM;
	if(cv1kV2TrustedRom && a<0x400000 && base==cv1kV2TrustedRom+a)return true;
	return cv1kV2TrustedRam && a>=0xc000000 && a<0xd000000 &&
		base==cv1kV2TrustedRam+((a-0xc000000)&(cv1kV2TrustedRamSize-1));
}
static void Cv1kV2TrustMemory(UINT8 *ram,UINT32 size,UINT8 *rom){
	Cv1kV2RevokeTrust();
	if(!(cv1kV2TrackedHandlers&1u))return; // explicit board contract
	bool ramOK=ram && (size==0x800000 || size==0x1000000);
	for(UINT32 off=0;ramOK && off<0x1000000;off+=SH3_PAGE_SIZE){
		const unsigned idx=(0xc000000+off)>>SH3_SHIFT;
		UINT8 *p=ram+(off&(size-1));
		if(MemMapR[idx]!=p || MemMapW[idx]!=p || MemMapF[idx]!=p)ramOK=false;
	}
	bool romOK=rom!=NULL;
	for(UINT32 off=0;romOK && off<0x400000;off+=SH3_PAGE_SIZE)
		if(MemMapF[off>>SH3_SHIFT]!=rom+off)romOK=false;
	if(ramOK){cv1kV2TrustedRam=ram;cv1kV2TrustedRamSize=size;}
	if(romOK)cv1kV2TrustedRom=rom;
	if(cv1kV2WriteWatch)bprintf(PRINT_UI,_T("CV1000 SH3 V2 verification backing: RAM %s, ROM %s\n"),
		ramOK && !cv1kV2VerifyAlways?_T("registered"):_T("always compare"),
		romOK && !cv1kV2VerifyAlways?_T("registered"):_T("always compare"));
}
#else
static inline void Cv1kV2WriteMapChanged(){}
static inline void Cv1kV2NoteWrite(UINT32,unsigned){}
static inline void Cv1kV2NoteOpaqueWrite(){}
static inline void Cv1kV2NoteHandlerWrite(unsigned){}
static inline void Cv1kV2SetTracked(unsigned,bool){}
static inline void Cv1kV2TrustMemory(UINT8*,UINT32,UINT8*){}
static inline void Cv1kV2RevokeTrust(){}
#endif
