// B-3 shadow write barrier. Watch host backing pages, not guest aliases.
// All V2 executions STILL validate live bytes. This is instrumentation
// and a prerequisite for future links, not authorization to omit validation.
#if SH3_NATIVE_V2
struct Cv1kV2WatchPage { uintptr_t base; uint64_t generation; };
static constexpr unsigned Cv1kV2WatchLimit=8192;
static Cv1kV2WatchPage *cv1kV2WatchPages;
static uint64_t **cv1kV2WriteWatch;
static unsigned cv1kV2WatchCount;
static uint64_t cv1kV2OpaqueEpoch;
static bool Cv1kV2RangesOverlap(uintptr_t a,size_t n,uintptr_t b,size_t m){
	return a<=b?b-a<n:a-b<m;
}
static void Cv1kV2BindWritePage(Cv1kV2WatchPage &p){
	for(unsigned i=0;i<SH3_PAGE_COUNT;i++){
		const uintptr_t w=(uintptr_t)MemMapW[i];
		if(w<SH3_MAXHANDLER || !Cv1kV2RangesOverlap(w,SH3_PAGE_SIZE,p.base,SH3_PAGE_SIZE))continue;
		uint64_t *&entry=cv1kV2WriteWatch[i];
		// Partial overlapping/ambiguous maps conservatively dirty all.
		if(w!=p.base || (entry && entry!=&p.generation))entry=&cv1kV2OpaqueEpoch;
		else entry=&p.generation;
	}
}
static void Cv1kV2WriteMapChanged(){
	if(!cv1kV2WriteWatch)return;
	++cv1kV2OpaqueEpoch;
	memset(cv1kV2WriteWatch,0,sizeof(*cv1kV2WriteWatch)*SH3_PAGE_COUNT);
	for(unsigned i=0;i<cv1kV2WatchCount;i++)Cv1kV2BindWritePage(cv1kV2WatchPages[i]);
}
static unsigned Cv1kV2WatchCode(const void *base){
	if(!cv1kV2WriteWatch)return 0;
	const uintptr_t a=(uintptr_t)base;
	for(unsigned i=0;i<cv1kV2WatchCount;i++)if(cv1kV2WatchPages[i].base==a)return i+1;
	if(cv1kV2WatchCount==Cv1kV2WatchLimit)return 0;
	auto &p=cv1kV2WatchPages[cv1kV2WatchCount++];
	p={a,0};Cv1kV2BindWritePage(p);return cv1kV2WatchCount;
}
static inline __attribute__((always_inline)) void Cv1kV2NoteWrite(UINT32 a,unsigned n){
	if(!cv1kV2WriteWatch)return;
	if(a<0xe0000000)a&=AM;
	if(n>SH3_PAGE_SIZE-(a&SH3_PAGEM)){++cv1kV2OpaqueEpoch;return;}
	uint64_t *p=cv1kV2WriteWatch[a>>SH3_SHIFT];
	if(p)++*p;
}
static inline void Cv1kV2NoteOpaqueWrite(){
	if(cv1kV2WriteWatch)++cv1kV2OpaqueEpoch;
}
static void Cv1kV2WatchClear(){
	cv1kV2WatchCount=0;++cv1kV2OpaqueEpoch;
	if(cv1kV2WriteWatch)memset(cv1kV2WriteWatch,0,sizeof(*cv1kV2WriteWatch)*SH3_PAGE_COUNT);
}
static void Cv1kV2WatchReset(){
	delete[] cv1kV2WriteWatch;cv1kV2WriteWatch=NULL;
	delete[] cv1kV2WatchPages;cv1kV2WatchPages=NULL;
	cv1kV2WatchCount=0;cv1kV2OpaqueEpoch=0;
}
static void Cv1kV2WatchEnable(){
	if(!Cv1kPerformanceOption(getenv("FBNEO_CV1K_V2_WRITE_AUDIT"),true))return;
	cv1kV2WatchPages=new(std::nothrow) Cv1kV2WatchPage[Cv1kV2WatchLimit]();
	cv1kV2WriteWatch=new(std::nothrow) uint64_t*[SH3_PAGE_COUNT]();
	if(!cv1kV2WatchPages || !cv1kV2WriteWatch)Cv1kV2WatchReset();
}
#else
static inline void Cv1kV2WriteMapChanged(){}
static inline void Cv1kV2NoteWrite(UINT32,unsigned){}
static inline void Cv1kV2NoteOpaqueWrite(){}
#endif
