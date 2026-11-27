// Interpreter-only fusion of observed SH3 data-copy kernels.
// Match opcodes and register relationships, never game or guest PC.
// A cache holds only a decoded *shape*: positive entries must match
// live instruction bytes on every use, and all data mappings are live.
struct Cv1kDataPlan {
	UINT32 pc;
	UINT16 words[12];
	UINT8 kind,length,width,src,dst,value,counter,bits,flag,temp;
	UINT8 cooldown;
};
static Cv1kDataPlan cv1kDataPlans[1024];
static unsigned Cv1kDataN(UINT16 op){return (op>>8)&15;}
static unsigned Cv1kDataM(UINT16 op){return (op>>4)&15;}
static bool Cv1kDataDistinct(const UINT8 *regs,unsigned count)
{
	unsigned seen=0;
	for(unsigned i=0;i<count;i++){
		const unsigned bit=1u<<regs[i];
		if(seen&bit)return false;
		seen|=bit;
	}
	return true;
}
static bool Cv1kDataForwardExit(UINT16 op,unsigned index,unsigned length,unsigned prefix)
{
	return (op>>8)==prefix && INT8(op)>0 &&
		INT32(index*2+4)+INT32(INT8(op))*2>=INT32(length*2);
}
static bool Cv1kDataDecode(Cv1kDataPlan &p,const UINT16 *w,unsigned avail,unsigned expected)
{
	// Observed literal-data loop: test a signed flag byte, copy one
	// literal byte, decrement the output count, shift the flag and
	// decrement the bit count. The two forward exits run normally.
	// CMP/PZ flag; BF exit; MOV.B @src+,v; MOV.B v,@dst; ADD #1,dst;
	// DT count; BT/S exit; MOV flag,tmp; ADD tmp,tmp; DT bits;
	// BF/S head; EXTS.B tmp,flag.
	if((expected==0 || expected==1) && avail>=12 &&
		(w[0]&0xf0ff)==0x4011 && Cv1kDataForwardExit(w[1],1,12,0x8b) &&
		(w[2]&0xf00f)==0x6004 && (w[3]&0xf00f)==0x2000 &&
		(w[5]&0xf0ff)==0x4010 && Cv1kDataForwardExit(w[6],6,12,0x8d) &&
		(w[7]&0xf00f)==0x6003 && (w[9]&0xf0ff)==0x4010 && w[10]==0x8ff4){
		p.flag=Cv1kDataN(w[0]);p.src=Cv1kDataM(w[2]);p.value=Cv1kDataN(w[2]);
		p.dst=Cv1kDataN(w[3]);p.counter=Cv1kDataN(w[5]);p.temp=Cv1kDataN(w[7]);
		p.bits=Cv1kDataN(w[9]);
		const UINT8 regs[]={p.flag,p.src,p.value,p.dst,p.counter,p.temp,p.bits};
		if(Cv1kDataM(w[3])!=p.value || w[4]!=(0x7001|(p.dst<<8)) ||
			Cv1kDataM(w[7])!=p.flag || w[8]!=(0x300c|(p.temp<<8)|(p.temp<<4)) ||
			w[11]!=(0x600e|(p.flag<<8)|(p.temp<<4)) || !Cv1kDataDistinct(regs,7))return false;
		p.kind=1;p.length=12;p.width=1;
		memcpy(p.words,w,24);return true;
	}
	// Common counted copy, with normal BF or BF/S and the address
	// increment scheduled in its delay slot. Width may be B/W/L.
	if(avail<5 || (w[0]&0xf000)!=0x6000 || (w[0]&15)<4 || (w[0]&15)>6)return false;
	const unsigned size=w[0]&15, width=1u<<(size-4);
	const bool delayed=w[3]==0x8ffb && (w[2]&0xf0ff)==0x4010;
	const unsigned dt=delayed?2:3,br=delayed?3:4,add=delayed?4:2;
	const unsigned kind=delayed?3:2;
	if((expected && expected!=kind) || (w[1]&0xf00f)!=0x2000+size-4 ||
		(w[dt]&0xf0ff)!=0x4010 || w[br]!=(delayed?0x8ffb:0x8bfa))return false;
	p.src=Cv1kDataM(w[0]);p.value=Cv1kDataN(w[0]);p.dst=Cv1kDataN(w[1]);p.counter=Cv1kDataN(w[dt]);
	const UINT8 regs[]={p.src,p.value,p.dst,p.counter};
	if(Cv1kDataM(w[1])!=p.value || w[add]!=(0x7000|(p.dst<<8)|width) ||
		!Cv1kDataDistinct(regs,4))return false;
	p.kind=kind;p.length=5;p.width=width;
	memcpy(p.words,w,10);return true;
}

static bool Cv1kDataRam(UINT32 a,bool write)
{
	if(a>=0xe0000000)return false;
	const UINT32 pa=a&AM;
	return (pa>=0x0c000000 && pa<0x0d000000) || (!write && pa<0x00400000);
}
// A short literal run normally has only 1..8 bytes. Avoid a
// general memmove call and preserve the forward overlap semantics.
static inline UINT32 Cv1kLiteralCopy(const Cv1kInterpRange &in,const Cv1kInterpRange &out)
{
	if(in.bytes>8)return Cv1kInterpCopy<1>(in,out);
	UINT8 v=0;
	for(UINT32 i=0;i<in.bytes;i++){
#ifdef LSB_FIRST
		v=in.page[(in.offset+i)^1];
		out.page[(out.offset+i)^1]=v;
#else
		v=in.page[in.offset+i];
		out.page[out.offset+i]=v;
#endif
	}
	return UINT32(INT32(INT8(v)));
}

static inline bool Cv1kLiteralFinish(UINT32 pc,UINT32 cycles,
	UINT32 rounds,UINT32 instructions,unsigned exitKind)
{
	EAT(INT32(cycles));
	++cv1kDataStats.hits;
	cv1kDataStats.literalRounds+=rounds;
	cv1kDataStats.bytes+=rounds;
	cv1kDataStats.ops+=instructions;
	cv1kDataStats.lastPC=pc;
	if(exitKind==1)++cv1kDataFullStats.flagExits;
	else if(exitKind==2)++cv1kDataFullStats.countExits;
	else if(exitKind==3)++cv1kDataFullStats.bitExits;
	return true;
}

// Full literal kernel, including exactly the original forward exits.
// Runs only from the first CMP/PZ; never from inside a delay slot.
// No speculative memory accesses or changes to timer-slice boundaries.
static bool Cv1kTryLiteralFull(const Cv1kDataPlan &p,const UINT16 *w,UINT32 pc,
	UINT32 pa,UINT8 *fetchPage)
{
	const UINT32 flags=m_r[p.flag];
	const UINT32 budget=UINT32(m_sh4_icount);
	// Signed negative flags take the first BF without touching data.
	if(INT32(flags)<0){
		if(budget<4)return false;
		m_sr&=~T;m_pc=m_ea=pc+6+INT32(INT8(p.words[1]))*2;
		m_ppc=pc+4;
		return Cv1kLiteralFinish(pc,4,0,2,1);
	}
	// Only canonical positive flag bytes, and nonwrapping counters.
	const UINT32 count=m_r[p.counter],bits=m_r[p.bits];
	if(flags>127 || !count || !bits || budget<9)return false;
	const UINT32 src=m_r[p.src],dst=m_r[p.dst];
	if(!Cv1kDataRam(src,false) || !Cv1kDataRam(dst,true))return false;
	UINT32 limit=count<bits?count:bits;
	if(limit>4096)limit=4096;
	if(flags){
		const UINT32 leading=unsigned(__builtin_clz(flags))-24;
		if(limit>leading)limit=leading;
	}
	const UINT32 so=src&SH3_PAGEM,doff=dst&SH3_PAGEM;
	if(limit>SH3_PAGE_SIZE-so)limit=SH3_PAGE_SIZE-so;
	if(limit>SH3_PAGE_SIZE-doff)limit=SH3_PAGE_SIZE-doff;
	// Each taken lap costs 13. A count exit costs 9 (BT/S +
	// its MOV slot), a bit-count exit costs 12 (BF/S not taken).
	// Account for the shorter final lap before applying the budget.
	UINT32 laps=budget/13;
	if(laps>limit)laps=limit;
	if(laps<limit){
		const UINT32 next=laps+1;
		const unsigned tail=next==count?9:next==bits?12:13;
		if(budget-laps*13>=tail)++laps;
	}
	if(!laps)return false;
	const bool countExit=laps==count;
	const bool bitExit=!countExit && laps==bits;
	UINT32 spent=laps*13-(countExit?4:bitExit?1:0);
	UINT32 ops=laps*12-(countExit?4:0);
	UINT8 *inPage=MemMapR[(src&AM)>>SH3_SHIFT];
	UINT8 *outPage=MemMapW[(dst&AM)>>SH3_SHIFT];
	if((uintptr_t)inPage<SH3_MAXHANDLER || (uintptr_t)outPage<SH3_MAXHANDLER)return false;
	Cv1kInterpRange in={inPage,so,laps},out={outPage,doff,laps};
	if(Cv1kInterpCodeOverlap(out,w,24,1))return false;
	// DT's legacy data-map read of the following branch must not
	// call MMIO or see different bytes. The normal same-page case
	// is already protected by the full live code overlap guard.
	UINT8 *readPage=MemMapR[pa>>SH3_SHIFT];
	if((uintptr_t)readPage<SH3_MAXHANDLER)return false;
	if(readPage!=fetchPage){
		const UINT16 *r=(const UINT16*)(readPage+(pa&SH3_PAGEM));
		if(r[6]!=p.words[6] || r[10]!=p.words[10] ||
			Cv1kInterpCodeOverlap(out,r+6,2,1) ||
			Cv1kInterpCodeOverlap(out,r+10,2,1))return false;
	}
	m_r[p.value]=Cv1kLiteralCopy(in,out);
	m_r[p.src]+=laps;m_r[p.dst]+=laps;m_r[p.counter]-=laps;
	const UINT32 shifted=flags?flags<<(laps-unsigned(countExit)):0;
	m_r[p.temp]=shifted;
	m_r[p.flag]=UINT32(INT32(INT8(shifted)));
	m_r[p.bits]-=laps-unsigned(countExit);
	unsigned exitKind=0;
	if(countExit){
		m_sr|=T;m_pc=m_ppc=m_ea=pc+16+INT32(INT8(p.words[6]))*2;
		exitKind=2;
	}else if(bitExit){
		m_sr|=T;m_pc=m_ppc=pc+24;m_ea=dst+laps-1;
		exitKind=3;
	}else{
		m_sr&=~T;m_ppc=m_ea=pc;
		// After the last taken loop, also absorb the next two
		// read-only instructions when they take the negative exit.
		if(INT32(m_r[p.flag])<0 && budget-spent>=4){
			m_pc=m_ea=pc+6+INT32(INT8(p.words[1]))*2;
			m_ppc=pc+4;spent+=4;ops+=2;exitKind=1;
		}
	}
	return Cv1kLiteralFinish(pc,spent,laps,ops,exitKind);
}

static bool Cv1kTryDataLoop(unsigned expected=0)
{
	if(m_delay || m_test_irq || m_cpu_off || m_sh4_icount<(cv1kDataFullEnabled && expected==1?4:13) ||
		(m_pc&1) || !Cv1kDataRam(m_pc,false))return false;
	const UINT32 pc=m_pc,pa=pc&AM,off=pa&SH3_PAGEM;
	UINT8 *page=MemMapF[pa>>SH3_SHIFT];
	if((uintptr_t)page<SH3_MAXHANDLER)return false;
	const UINT16 *w=(const UINT16*)(page+off);
	const unsigned avail=(SH3_PAGE_SIZE-off)/2;
	// Slice-entry path uses just a cheap first-op filter. At a known
	// branch shape the same decoder is used. Negative cache entries
	// are hints only and cannot cause stale guest instruction execution.
	if(!expected && ((*w&0xf0ff)!=0x4011) &&
		((*w&0xf000)!=0x6000 || (*w&15)<4 || (*w&15)>6))return false;
	++cv1kDataStats.probes;
	auto &p=cv1kDataPlans[((pc>>1)^(pc>>13))&1023];
	if(p.pc==pc && p.kind==255 && p.cooldown){--p.cooldown;return false;}
	if(p.pc==pc && p.kind && p.kind<4 && (!expected || p.kind==expected) &&
		avail>=p.length && (p.kind==1?!memcmp(w,p.words,24):!memcmp(w,p.words,10))){
		++cv1kDataStats.cached;
	}else{
		Cv1kDataPlan next={};next.pc=pc;
		if(!Cv1kDataDecode(next,w,avail,expected)){
			next.kind=255;next.cooldown=63;p=next;return false;
		}
		p=next;
	}
	if(p.kind==1 && cv1kDataFullEnabled){
		cv1kDataHeadPC=pc;
		return Cv1kTryLiteralFull(p,w,pc,pa,page);
	}
	const bool literal=p.kind==1,delayed=p.kind!=2;
	const unsigned width=p.width,cost=literal?13:delayed?6:7;
	const UINT32 count=m_r[p.counter];
	if(count<=1)return false;
	UINT32 laps=UINT32(m_sh4_icount)/cost;
	if(laps>count-1)laps=count-1;
	if(laps>4096)laps=4096;
	UINT32 flags=0;
	if(literal){
		flags=m_r[p.flag];
		const UINT32 bits=m_r[p.bits];
		// Keep all forward exits and the terminal bit-count iteration
		// in the interpreter. All fused rounds start with a nonnegative
		// canonical sign-extended byte and leave both counters nonzero.
		if(flags>127 || bits<=1)return false;
		if(laps>bits-1)laps=bits-1;
		if(flags){
			const unsigned leading=unsigned(__builtin_clz(flags))-24;
			if(laps>leading)laps=leading;
		}
	}
	const UINT32 src=m_r[p.src],dst=m_r[p.dst];
	if(!Cv1kDataRam(src,false) || !Cv1kDataRam(dst,true) ||
		((src|dst)&(width-1)))return false;
	const UINT32 srclim=(SH3_PAGE_SIZE-(src&SH3_PAGEM))/width;
	const UINT32 dstlim=(SH3_PAGE_SIZE-(dst&SH3_PAGEM))/width;
	if(laps>srclim)laps=srclim;
	if(laps>dstlim)laps=dstlim;
	if(laps<(literal?1u:2u))return false;
	// DT really fetches its following word from the data map. Never
	// erase a side effect or accidentally suppress its DT/BF hack.
	UINT8 *readPage=MemMapR[pa>>SH3_SHIFT];
	if((uintptr_t)readPage<SH3_MAXHANDLER)return false;
	const UINT16 *readWords=(const UINT16*)(readPage+off);
	const unsigned branch=literal?10:delayed?3:4;
	if(readWords[branch]!=p.words[branch] ||
		(literal && readWords[6]!=p.words[6]))return false;
	const UINT32 bytes=laps*width;
	Cv1kInterpRange in,out;
	if(!Cv1kInterpDirectRange(src,bytes,width,false,in) ||
		!Cv1kInterpDirectRange(dst,bytes,width,true,out) ||
		Cv1kInterpCodeOverlap(out,w,p.length*2,width) ||
		Cv1kInterpCodeOverlap(out,readWords+branch,2,width) ||
		(literal && Cv1kInterpCodeOverlap(out,readWords+6,2,width)))return false;
	// Do not turn forward overlapping decompression copies into
	// memmove. This helper retains sequential byte/word/long order
	// and the guest's endian layout, including physical aliases.
	m_r[p.value]=width==1?Cv1kInterpCopy<1>(in,out):
		width==2?Cv1kInterpCopy<2>(in,out):Cv1kInterpCopy<4>(in,out);
	m_r[p.src]+=bytes;m_r[p.dst]+=bytes;m_r[p.counter]-=laps;
	if(literal){
		const UINT32 shifted=flags?flags<<laps:0;
		m_r[p.temp]=shifted;
		m_r[p.flag]=UINT32(INT32(INT8(shifted)));
		m_r[p.bits]-=laps;
		cv1kDataStats.literalRounds+=laps;
	}else cv1kDataStats.copyRounds+=laps;
	// The final executed branch is taken, and its delay slot
	// (when present) does not alter T. Preserve exact guest cycles.
	m_sr&=~T;m_ea=pc;m_ppc=delayed?pc:pc+p.length*2;
	EAT(INT32(laps*cost));
	++cv1kDataStats.hits;cv1kDataStats.bytes+=bytes;
	cv1kDataStats.ops+=UINT64(laps)*p.length;
	cv1kDataStats.lastPC=pc;
	return true;
}
