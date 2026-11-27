// CV1000 short, read-only polling loops. Independent interpreter
// implementation: no generated code or guessed idle-cycle burns.
// Try once per existing timer slice, including a rotation of a loop
// when the slice starts in its body. The final guest register/flag
// state must be a fixed point of a complete, actually taken round.
struct Cv1kPollState {
	UINT32 r[16],sr,ea,ppc;
	UINT32 cycles,reads,kinds;
};
static bool Cv1kPollRead(Cv1kPollState &s,UINT32 address,unsigned width,UINT32 visiblePC,UINT32 &value)
{
	if(address>=0xe0000000 || (address&(width-1)))return false;
	const UINT32 a=address&AM;
	UINT8 *page=width==4?MemMapRL[a>>SH3_SHIFT]:MemMapR[a>>SH3_SHIFT];
	if((uintptr_t)page<SH3_MAXHANDLER){
		// Only the board's explicitly registered, side-effect-free
		// long-read equivalent and the existing direct idle mapping.
		if(width!=4)return false;
		if((uintptr_t)page==1 && cv1kIdlePage && cv1kIdleCycles &&
			(a>>SH3_SHIFT)==0xc00 && MemMapR[0xc00]==cv1kIdlePage &&
			ReadLong[1]==cv1kIdleHandler){
			memcpy(&value,cv1kIdlePage+(a&SH3_PAGEM),4);
#ifdef LSB_FIRST
			value=(value<<16)|(value>>16);
#endif
			if(a==cv1kIdleAddress && ((visiblePC&AM)==cv1kIdlePC ||
				(visiblePC&AM)==cv1kIdlePC+2)){
				if(*cv1kIdleCycles<0 || *cv1kIdleCycles>1000000)return false;
				s.cycles+=*cv1kIdleCycles;
				s.kinds|=2;
			}else s.kinds|=1;
		}else{
			INT32 extra=0;
			if(!cv1kPollReadHandler || !cv1kPollPeek ||
				ReadLong[(uintptr_t)page]!=cv1kPollReadHandler ||
				!cv1kPollPeek(a,&value,&extra) || extra<0 || extra>1000000)return false;
			s.cycles+=extra;s.kinds|=4;
		}
	}else{
		// CV1000 ROM and main RAM only. No device memory, internal
		// registers, DMA destinations with asynchronous host writers,
		// or implicit effects through a mapped callback.
		if(a>=0x00400000 && (a<0x0c000000 || a>=0x0d000000))return false;
		const UINT32 off=a&SH3_PAGEM;
		if(width==4){
			memcpy(&value,page+off,4);
#ifdef LSB_FIRST
			value=(value<<16)|(value>>16);
#endif
		}else if(width==2){
			UINT16 v;memcpy(&v,page+off,2);value=UINT32(INT32(INT16(v)));
		}else{
#ifdef LSB_FIRST
			value=UINT32(INT32(INT8(page[off^1])));
#else
			value=UINT32(INT32(INT8(page[off])));
#endif
		}
		s.kinds|=1;
	}
	++s.reads;s.ea=address;
	return true;
}

static bool Cv1kPollData(Cv1kPollState &s,UINT16 op,UINT32 visiblePC)
{
	const unsigned n=(op>>8)&15,m=(op>>4)&15;
	const unsigned high=op>>12,low=op&15;
	UINT32 &a=s.r[n];const UINT32 b=s.r[m];
	bool t=false;
	++s.cycles;s.ppc=visiblePC;
	// No writes, postincrement, system registers, timers, exceptions,
	// control flow, or instructions with hidden architectural effects.
	if(high==6 && low<=2){
		UINT32 value;
		if(!Cv1kPollRead(s,b,1u<<low,visiblePC,value))return false;
		a=value;return true;
	}
	if(high==5){
		UINT32 value;
		if(!Cv1kPollRead(s,b+low*4,4,visiblePC,value))return false;
		a=value;return true;
	}
	if((op&0xff00)==0xc600){
		UINT32 value;
		if(!Cv1kPollRead(s,m_gbr+(op&255)*4,4,visiblePC,value))return false;
		s.r[0]=value;return true;
	}
	if(high==6 && low==3){a=b;return true;}
	if(high==14){a=UINT32(INT32(INT8(op)));return true;}
	if(high==7){a+=UINT32(INT32(INT8(op)));return true;}
	if(high==3){
		switch(low){
			case 0:t=a==b;break;
			case 2:t=a>=b;break;
			case 3:t=INT32(a)>=INT32(b);break;
			case 6:t=a>b;break;
			case 7:t=INT32(a)>INT32(b);break;
			case 8:a-=b;return true;
			case 12:a+=b;return true;
			default:return false;
		}
	}else if(high==2){
		switch(low){
			case 8:t=(a&b)==0;break;
			case 9:a&=b;return true;
			case 10:a^=b;return true;
			case 11:a|=b;return true;
			default:return false;
		}
	}else if((op&0xff00)==0x8800)t=s.r[0]==UINT32(INT32(INT8(op)));
	else if((op&0xff00)==0xc800)t=(s.r[0]&(op&255))==0;
	else if((op&0xff00)==0xc900){s.r[0]&=op&255;return true;}
	else if((op&0xff00)==0xca00){s.r[0]^=op&255;return true;}
	else if((op&0xff00)==0xcb00){s.r[0]|=op&255;return true;}
	else if((op&0xf0ff)==0x4011)t=INT32(a)>=0;
	else if((op&0xf0ff)==0x4015)t=INT32(a)>0;
	else if(op==0x0009)return true;
	else return false;
	s.sr=(s.sr&~T)|UINT32(t);
	return true;
}

static bool Cv1kTryPoll()
{
	if(m_delay || m_test_irq || m_cpu_off || m_sh4_icount<24 ||
		(m_pc&1) || m_pc>=0xe0000000)return false;
	const UINT32 entry=m_pc,pa=entry&AM,offset=pa&SH3_PAGEM;
	UINT8 *page=MemMapF[pa>>SH3_SHIFT];
	if((uintptr_t)page<SH3_MAXHANDLER)return false;
	// Limit scans to eight instructions on one live fetch page. An
	// unsupported shape is retried occasionally, not on every branch.
	const unsigned slot=(entry>>1)&1023;
	auto &reject=cv1kPollReject[slot];
	if(reject.pc==entry && reject.cooldown){--reject.cooldown;return false;}
	++cv1kPollStats.attempts;
	UINT32 branch=entry,head=entry;UINT16 br=0;
	bool delayed=false,unconditional=false;
	for(unsigned i=0;i<8;i++,branch+=2){
		if((branch&~SH3_PAGEM)!=(entry&~SH3_PAGEM))break;
		UINT16 op;memcpy(&op,page+((branch&AM)&SH3_PAGEM),2);
		const unsigned top=op>>8;
		if(top==0x89 || top==0x8b || top==0x8d || top==0x8f){
			delayed=top==0x8d || top==0x8f;
			head=branch+4+INT32(INT8(op))*2;br=op;break;
		}
		if((op&0xf000)==0xa000){
			const INT32 disp=(op&0x800)?INT32(op&0xfff)-4096:INT32(op&0xfff);
			if(disp==-2)break; // existing BRA-self special case
			delayed=unconditional=true;head=branch+4+disp*2;br=op;break;
		}
		// Cheap structural filter; the actual simulation below is
		// stricter and will reject any unsupported opcode.
		if((op>>12)==1 || (op>>12)==4 || (op>>12)==0xb ||
			((op>>12)==2 && (op&15)<8) || (op>>12)==0xf)break;
	}
	const UINT32 end=branch+(delayed?4:2);
	if(!br || head>entry || entry>branch || head>=branch ||
		end-head>16 || (head&~SH3_PAGEM)!=(entry&~SH3_PAGEM) ||
		((end-1)&~SH3_PAGEM)!=(entry&~SH3_PAGEM)){
		reject={entry,63};return false;
	}
	Cv1kPollState state={};memcpy(state.r,m_r,sizeof(m_r));
	state.sr=m_sr;state.ea=m_ea;state.ppc=m_ppc;
	UINT32 pc=entry;
	// Starting mid-loop is fine: follow the real taken edge and end
	// at precisely the same next instruction, not at a guessed head.
	unsigned executed=0;
	do{
		if(++executed>8)return false;
		if(pc==branch){
			const bool take=unconditional || ((br&0x0200)?!(state.sr&T):!!(state.sr&T));
			if(!take)return false;
			state.cycles+=delayed?2:3;
			state.ea=head;state.ppc=branch+2;
			if(delayed){
				UINT16 op;memcpy(&op,page+((branch+2)&SH3_PAGEM),2);
				if(++executed>8 || !Cv1kPollData(state,op,head))return false;
			}
			pc=head;
		}else{
			if(pc<head || pc>=branch)return false;
			UINT16 op;memcpy(&op,page+(pc&SH3_PAGEM),2);
			if(!Cv1kPollData(state,op,pc+2))return false;
			pc+=2;
		}
	}while(pc!=entry);
	// Same full register/SR state + unchanged readable memory +
	// no possible event until this original slice boundary. Nothing
	// in the verified round can make a future iteration differ.
	if(!state.reads || state.sr!=m_sr || memcmp(state.r,m_r,sizeof(m_r)) ||
		!state.cycles)return false;
	const UINT32 laps=UINT32(m_sh4_icount)/state.cycles;
	if(laps<2)return false;
	m_ppc=state.ppc;m_ea=state.ea;
	const UINT32 consumed=laps*state.cycles;
	EAT(INT32(consumed));
	++cv1kPollStats.hits;cv1kPollStats.rounds+=laps;
	cv1kPollStats.cycles+=consumed;cv1kPollStats.ops+=UINT64(executed)*laps;
	if(state.kinds&2)++cv1kPollStats.idleHits;
	else if(state.kinds&4)++cv1kPollStats.deviceHits;
	else ++cv1kPollStats.ramHits;
	cv1kPollStats.lastPC=entry;
	reject={};
	return true;
}
