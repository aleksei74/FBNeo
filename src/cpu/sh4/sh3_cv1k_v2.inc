// Stage A frontend. No speculative guest writes, no linked native
// branches and no page-invalidating write barrier yet. Every invocation
// validates the entire live code span and its fetch mapping. The old
// emitter exits BEFORE a failing memory instruction, including a store
// overlapping its own validated code through a physical/host alias.
#if SH3_NATIVE_V2
static constexpr unsigned Cv1kV2Entries=16384,Cv1kV2Hot=4,Cv1kV2Min=12;
struct Cv1kV2Entry {
	const UINT16 *source;
	UINT32 pc,offset;
	UINT16 first,ops[Sh3Native::MaxOps];
	UINT8 count,hot,cooldown,shortExits;
};
struct Cv1kV2Stats {
	UINT64 probes,hits,ops,built,rejected,changed,guards,full,negative;
};
static Cv1kV2Entry *cv1kV2Table;
static Sh3V2::CodeCache cv1kV2Code;
static Cv1kV2Stats cv1kV2Stats;
static bool cv1kV2Full;
static void Cv1kV2Reset(){
	delete[] cv1kV2Table;cv1kV2Table=NULL;
	cv1kV2Code.Reset();cv1kV2Stats={};cv1kV2Full=false;
}
static bool Cv1kV2Enable(){
	cv1kV2Table=new(std::nothrow) Cv1kV2Entry[Cv1kV2Entries]();
	return cv1kV2Table!=NULL;
}
static unsigned Cv1kV2Hash(UINT32 pc){
	UINT32 h=pc>>1;h^=h>>13;h*=0x9e3779b1u;h^=h>>16;
	return h&(Cv1kV2Entries-1);
}
static bool Cv1kV2Candidate(UINT16 op){
	// CMP/PZ is the existing full-literal kernel dispatch hook. Do
	// not compile over it even when the learned head changes later.
	return (op&0xf0ff)!=0x4011 && Sh3Native::Candidate(op);
}
static unsigned Cv1kV2Try(){
	if(!cv1kV2Table || cv1kV2Code.Denied() || m_cpu_type!=CPU_TYPE_SH3 ||
		m_delay || m_test_irq || m_cpu_off || sh3_end_run ||
		m_sh4_icount<INT32(Cv1kV2Min) || (m_pc&1))return 0;
	const UINT32 pc=m_pc,physical=pc&AM,inpage=physical&SH3_PAGEM;
	const UINT8 *page=MemMapF[physical>>SH3_SHIFT];
	if((uintptr_t)page<SH3_MAXHANDLER)return 0;
	unsigned available=(SH3_PAGE_SIZE-inpage)/2;
	if(available<Cv1kV2Min)return 0;
	if(available>Sh3Native::MaxOps)available=Sh3Native::MaxOps;
	const UINT16 *code=reinterpret_cast<const UINT16*>(page+inpage);
	if(!Cv1kV2Candidate(code[0]))return 0;
	++cv1kV2Stats.probes;
	Cv1kV2Entry &b=cv1kV2Table[Cv1kV2Hash(pc)];
	if(b.source!=code || b.pc!=pc || b.first!=code[0]){
		if(b.count && b.pc==pc)++cv1kV2Stats.changed;
		b={};b.source=code;b.pc=pc;b.first=code[0];b.hot=1;
		++cv1kNativeMisses;return 0;
	}
	if(b.count){
		if(b.count>available || memcmp(b.ops,code,b.count*sizeof(UINT16))){
			++cv1kV2Stats.changed;b.count=0;b.hot=1;b.cooldown=0;b.shortExits=0;return 0;
		}
	}else{
		if(cv1kV2Full)return 0;
		if(b.cooldown){--b.cooldown;++cv1kV2Stats.negative;return 0;}
		if(++b.hot<Cv1kV2Hot)return 0;
		unsigned n=0;
		while(n<available && Cv1kV2Candidate(code[n]))++n;
		if(n<Cv1kV2Min){
			b.hot=0;b.cooldown=63;++cv1kV2Stats.rejected;++cv1kNativeRejected;return 0;
		}
		UINT32 words[Sh3Native::MaxWords];
		Sh3Native::Emitter emit(words);
		for(unsigned i=0;i<n;i++)if(!emit.Emit(code[i],i)){
			b.hot=0;b.cooldown=63;++cv1kV2Stats.rejected;++cv1kNativeRejected;return 0;
		}
		emit.Finish(n);
		if(!cv1kV2Code.Publish(words,emit.End()-words,b.offset)){
			cv1kV2Full=true;++cv1kV2Stats.full;++cv1kNativeFull;
			bprintf(PRINT_UI,_T("CV1000 SH3 V2 cache: %s; interpreter fallback\n"),
				cv1kV2Code.Denied()?_T("RW/RX allocation denied"):_T("full"));
			return 0;
		}
		memcpy(b.ops,code,n*sizeof(UINT16));b.count=n;
		++cv1kV2Stats.built;++cv1kNativeBuilt;
	}
	// No partial-budget entry: the original interpreter handles all
	// tails. Timer/IRQ checks remain at the same slice/slow boundaries.
	if(b.count>m_sh4_icount)return 0;
	const Sh3Native::Memory memory={MemMapR,MemMapRL,MemMapW,code,
		size_t(b.count)*sizeof(UINT16),0,pc};
	const unsigned done=cv1kV2Code.Run(b.offset,m_r,&m_sr,&m_ea,memory);
	++cv1kV2Stats.hits;++cv1kNativeSpans;
	if(done!=b.count)++cv1kV2Stats.guards;
	if(done<Cv1kV2Min){
		if(++b.shortExits>=8){
			b.count=0;b.hot=0;b.cooldown=127;b.shortExits=0;
			++cv1kV2Stats.rejected;++cv1kNativeRejected;
		}
	}else b.shortExits=0;
	if(!done)return 0;
	// All emitted instructions are one-cycle and cannot call a device.
	// A failed guard executes no part of its guest instruction.
	m_pc=m_ppc=pc+done*2;EAT(done);
	cv1kV2Stats.ops+=done;cv1kNativeOps+=done;return done;
}
#else
static inline unsigned Cv1kV2Try(){return 0;}
#endif
