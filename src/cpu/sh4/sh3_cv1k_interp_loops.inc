// CV1000 interpreter-only fusion of a few common counted loops.
// No generated code, persistent decoded cache, skipped guest time,
// device callbacks, or speculative writes. Called only at a validated backwards
// branch/slot boundary in the timer-sliced interpreter.
struct Cv1kInterpRange {
	UINT8 *page;
	UINT32 offset;
	UINT32 bytes;
};
static bool Cv1kInterpDirectRange(UINT32 a,UINT32 bytes,unsigned width,bool write,Cv1kInterpRange &r)
{
	// P4 accesses and unaligned/cross-page accesses retain the original
	// handler path (including the CV1000 long-only idle page).
	if(!bytes || a>=0xe0000000 || UINT64(a)+bytes>0xe0000000ULL ||
		(a&(width-1)))return false;
	const UINT32 physical=a&AM,offset=physical&SH3_PAGEM;
	if(bytes>SH3_PAGE_SIZE-offset)return false;
	UINT8 *page=write?MemMapW[physical>>SH3_SHIFT]:
		width==4?MemMapRL[physical>>SH3_SHIFT]:MemMapR[physical>>SH3_SHIFT];
	if((uintptr_t)page<SH3_MAXHANDLER)return false;
	r={page,offset,bytes};
	return true;
}
static bool Cv1kInterpOverlap(uintptr_t a,size_t n,uintptr_t b,size_t m)
{
	return a<=b?b-a<n:a-b<m;
}
static bool Cv1kInterpCodeOverlap(const Cv1kInterpRange &r,const void *code,unsigned bytes,unsigned width)
{
	UINT32 lo=r.offset,hi=r.offset+r.bytes;
#ifdef LSB_FIRST
	// Guest byte accesses swap adjacent bytes in host RAM.
	if(width==1){lo&=~UINT32(1);hi=(hi+1)&~UINT32(1);}
#else
	(void)width;
#endif
	return Cv1kInterpOverlap((uintptr_t)(r.page+lo),hi-lo,(uintptr_t)code,bytes);
}
template<unsigned Width> static UINT32 Cv1kInterpLoad(const Cv1kInterpRange &r,UINT32 at)
{
	const UINT32 off=r.offset+at;
	if(Width==1){
#ifdef LSB_FIRST
		return UINT32(INT32(INT8(r.page[off^1])));
#else
		return UINT32(INT32(INT8(r.page[off])));
#endif
	}
	if(Width==2){
		UINT16 v;memcpy(&v,r.page+off,2);return UINT32(INT32(INT16(v)));
	}
	UINT32 v;memcpy(&v,r.page+off,4);
#ifdef LSB_FIRST
	v=(v<<16)|(v>>16);
#endif
	return v;
}
template<unsigned Width> static void Cv1kInterpStore(const Cv1kInterpRange &r,UINT32 at,UINT32 v)
{
	const UINT32 off=r.offset+at;
	if(Width==1){
#ifdef LSB_FIRST
		r.page[off^1]=UINT8(v);
#else
		r.page[off]=UINT8(v);
#endif
	}else if(Width==2){
		const UINT16 word=UINT16(v);memcpy(r.page+off,&word,2);
	}else{
#ifdef LSB_FIRST
		v=(v<<16)|(v>>16);
#endif
		memcpy(r.page+off,&v,4);
	}
}
template<unsigned Width> static UINT32 Cv1kInterpCopy(const Cv1kInterpRange &src,const Cv1kInterpRange &dst)
{
	const uintptr_t s=(uintptr_t)(src.page+src.offset),d=(uintptr_t)(dst.page+dst.offset);
	// Forward copies are NOT generally memmove: overlapping writes
	// to a higher address can feed subsequent reads. Retain that order.
	// Even-aligned complete guest-byte pairs share the same host layout.
	bool raw=Width!=1 || !((src.offset|dst.offset|src.bytes)&1);
	if(raw && (d<=s || !Cv1kInterpOverlap(d,dst.bytes,s,src.bytes))){
		const UINT32 last=Cv1kInterpLoad<Width>(src,src.bytes-Width);
		memmove(dst.page+dst.offset,src.page+src.offset,src.bytes);
		return last;
	}
	UINT32 v=0;
	for(UINT32 at=0;at<src.bytes;at+=Width){
		v=Cv1kInterpLoad<Width>(src,at);
		Cv1kInterpStore<Width>(dst,at,v);
	}
	return v;
}
template<unsigned Width> static void Cv1kInterpFill(const Cv1kInterpRange &dst,UINT32 value)
{
	if(Width==1){
		UINT32 lo=dst.offset,hi=lo+dst.bytes;
#ifdef LSB_FIRST
		if(lo&1){dst.page[lo^1]=UINT8(value);++lo;}
		if(hi&1){--hi;dst.page[hi^1]=UINT8(value);}
#endif
		memset(dst.page+lo,UINT8(value),hi-lo);
	}else{
		for(UINT32 at=0;at<dst.bytes;at+=Width)Cv1kInterpStore<Width>(dst,at,value);
	}
}

static bool Cv1kTryInterpLoop(UINT16 branch,bool delayed=false)
{
	// Execute only complete TAKEN laps; the terminal lap and short
	// tails run normally. The timer slice is never overrun.
	if(m_test_irq || m_delay || (m_sr&T) || m_sh4_icount<10 || m_cpu_off)return false;
	const unsigned length=delayed?(branch==0x8ffd?3:branch==0x8ffc?4:branch==0x8ffb?5:0):
		(branch==0x8bfc?3:branch==0x8bfb?4:branch==0x8bfa?5:0);
	if(!length || (m_pc&1) || m_ppc!=(delayed?m_pc:m_pc+length*2))return false;
	const unsigned branchIndex=length-1-unsigned(delayed),dtIndex=branchIndex-1;
	const UINT32 address=m_pc&AM,off=address&SH3_PAGEM;
	if(off+length*2>SH3_PAGE_SIZE)return false;
	UINT8 *page=MemMapF[address>>SH3_SHIFT];
	if((uintptr_t)page<SH3_MAXHANDLER)return false;
	const UINT16 *ops=(const UINT16*)(page+off);
	if(ops[branchIndex]!=branch || (ops[dtIndex]&0xf0ff)!=0x4010)return false;
	// DT's existing busy-loop check reads the next word through the
	// DATA mapping, not the fetch mapping. Do not suppress a handler,
	// or a mismatched word that could trigger its DT/BF speedhack.
	const UINT32 bfaddr=address+branchIndex*2;
	UINT8 *readPage=MemMapR[bfaddr>>SH3_SHIFT];
	if((uintptr_t)readPage<SH3_MAXHANDLER)return false;
	const UINT16 *readBF=(const UINT16*)(readPage+(bfaddr&SH3_PAGEM));
	if(*readBF!=branch)return false;

	const unsigned counter=(ops[dtIndex]>>8)&15,cost=length+(delayed?1:2);
	const UINT32 count=m_r[counter];
	if(count<=2)return false;
	UINT32 laps=UINT32(m_sh4_icount)/cost;
	if(laps>count-1)laps=count-1;
	if(laps>4096)laps=4096;
	if(laps<2)return false;

	unsigned kind=0;
	const UINT16 a=ops[0];
	const UINT16 arith=delayed && length==3?ops[length-1]:a;
	const unsigned n=(a>>8)&15,m=(a>>4)&15;
	const unsigned an=(arith>>8)&15,am=(arith>>4)&15;
	// ADD #imm,Rn / ADD Rm,Rn ; DT Rc ; BF[/S] [NOP].
	// Also DT ; BF/S ; ADD #imm,Rn, a common scheduled loop.
	const bool arithShape=(!delayed && length==3) ||
		(delayed && (length==3 || (length==4 && ops[length-1]==0x0009)));
	if(arithShape && ((arith&0xf000)==0x7000 || (arith&0xf00f)==0x300c)){
		if(an==counter)return false;
		UINT32 increment;
		if((arith&0xf000)==0x7000)increment=UINT32(INT32(INT8(arith)));
		else {
			if(am==counter || am==an)return false;
			increment=m_r[am];
		}
		m_r[an]+=increment*laps;
		kind=1;
	}else{
		// MOV.B/W/L @Rs+,Rt ; MOV.B/W/L Rt,@Rd ; ADD #width,Rd ; DT ; BF
		// MOV.B/W/L Rv,@Rd ; ADD #width,Rd ; DT ; BF
		// MOV.B/W/L Rv,@-Rd ; DT ; BF
		unsigned width=0,srcReg=0,dstReg=0,valueReg=0;
		bool copy=false,reverse=false;
		if(length==5 && (a&0xf000)==0x6000 && (a&15)>=4 && (a&15)<=6){
			copy=true;width=1u<<((a&15)-4);
			srcReg=m;valueReg=n;
			const UINT16 store=ops[1];dstReg=(store>>8)&15;
			if((store&0xf00f)!=(0x2000+((a&15)-4)) ||
				((store>>4)&15)!=valueReg ||
				ops[delayed?length-1:2]!=(0x7000|(dstReg<<8)|width) ||
				srcReg==dstReg || srcReg==valueReg || srcReg==counter)return false;
		}else if(length==4 && (a&0xf000)==0x2000 && (a&15)<=2){
			width=1u<<(a&15);dstReg=n;valueReg=m;
			if(ops[delayed?length-1:1]!=(0x7000|(dstReg<<8)|width))return false;
		}else if(((!delayed && length==3) || (delayed && length==4 && ops[length-1]==0x0009))
			&& (a&0xf000)==0x2000 && (a&15)>=4 && (a&15)<=6){
			width=1u<<((a&15)-4);dstReg=n;valueReg=m;reverse=true;
		}else return false;
		if(dstReg==valueReg || dstReg==counter || valueReg==counter)return false;
		// Cap the number of complete laps at the live mapped-page edge.
		// A later iteration that would hit an MMIO page is interpreted.
		const UINT32 dst=m_r[dstReg],src=copy?m_r[srcReg]:0;
		if(dst>=0xe0000000 || (dst&(width-1)) ||
			(copy && (src>=0xe0000000 || (src&(width-1)))))return false;
		const UINT32 dstBytes=reverse?(dst&SH3_PAGEM):(SH3_PAGE_SIZE-(dst&SH3_PAGEM));
		// Predecrement at a page boundary uses the previous page.
		const UINT32 dstAvail=reverse && !dstBytes?SH3_PAGE_SIZE:dstBytes;
		if(laps>dstAvail/width)laps=dstAvail/width;
		if(copy){
			const UINT32 srcAvail=(SH3_PAGE_SIZE-(src&SH3_PAGEM))/width;
			if(laps>srcAvail)laps=srcAvail;
		}
		if(laps<2)return false;
		const UINT32 bytes=laps*width;
		if(reverse && dst<bytes)return false;
		Cv1kInterpRange out,in;
		if(!Cv1kInterpDirectRange(reverse?dst-bytes:dst,bytes,width,true,out) ||
			(copy && !Cv1kInterpDirectRange(src,bytes,width,false,in)))return false;
		// Host addresses, not guest aliases: prohibit a write to any
		// live instruction or to the separate data-mapped BF word.
		if(Cv1kInterpCodeOverlap(out,ops,length*2,width) ||
			Cv1kInterpCodeOverlap(out,readBF,2,width))return false;
		if(copy){
			m_r[valueReg]=width==1?Cv1kInterpCopy<1>(in,out):
				width==2?Cv1kInterpCopy<2>(in,out):Cv1kInterpCopy<4>(in,out);
			m_r[srcReg]+=bytes;kind=2;
		}else{
			if(width==1)Cv1kInterpFill<1>(out,m_r[valueReg]);
			else if(width==2)Cv1kInterpFill<2>(out,m_r[valueReg]);
			else Cv1kInterpFill<4>(out,m_r[valueReg]);
			kind=3;
		}
		m_r[dstReg]=reverse?dst-bytes:dst+bytes;
	}
	m_r[counter]-=laps;
	// All laps take BF (counter remains nonzero). No other flag in
	// these patterns changes. Match the last taken branch/slot state.
	m_ea=m_pc;m_ppc=delayed?m_pc:m_pc+length*2;
	EAT(INT32(laps*cost));
	cv1kInterpLoopBlocks++;
	cv1kInterpLoopLaps+=laps;
	cv1kInterpLoopOps+=laps*length;
	cv1kInterpLoopKinds[kind-1]++;
	return true;
}
