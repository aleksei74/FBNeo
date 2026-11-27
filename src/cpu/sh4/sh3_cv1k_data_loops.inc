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
static bool Cv1kTryDataLoop(unsigned expected=0)
{
	if(m_delay || m_test_irq || m_cpu_off || m_sh4_icount<13 ||
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
		avail>=p.length && !memcmp(w,p.words,p.length*2)){
		++cv1kDataStats.cached;
	}else{
		Cv1kDataPlan next={};next.pc=pc;
		if(!Cv1kDataDecode(next,w,avail,expected)){
			next.kind=255;next.cooldown=63;p=next;return false;
		}
		p=next;
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
