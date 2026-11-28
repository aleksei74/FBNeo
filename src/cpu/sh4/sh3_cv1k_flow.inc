// Stage 2.5: callback-free one-cycle stack transfers and control flow.
// The baseline instruction handlers remain authoritative on guard failure.
template<bool Store, UINT32 Sh3HotState::*Reg, bool Regions=false>
static inline __attribute__((always_inline)) bool Cv1kTryStack(UINT16 opcode)
{
	const unsigned n=Rn;
	const UINT32 ea=m_r[n]-(Store?4:0);
	UINT8 *p=Cv1kDirectPointer<4,Store,Regions>(ea);
	if(!p)return false;
	m_ea=ea;
	if(Store){
		Cv1kV2NoteWrite(ea,4);
		UINT32 v=sh3Hot.*Reg;
#ifdef LSB_FIRST
		v=(v<<16)|(v>>16);
#endif
		m_r[n]=ea;memcpy(p,&v,4);
	}else{
		UINT32 v;memcpy(&v,p,4);
#ifdef LSB_FIRST
		v=(v<<16)|(v>>16);
#endif
		sh3Hot.*Reg=v;m_r[n]+=4;
	}
	return true;
}

template<bool Regions=false>
static inline bool Cv1kTryFlowDT(UINT16 opcode,UINT32 nextPC)
{
#if BUSY_LOOP_HACKS
	// DT's original lookahead uses the DATA read map, not fetch.
	// Preserve MMIO callbacks and the exact existing busy-loop burn.
	const UINT8 *p=Cv1kDirectPointer<2,false,Regions>(nextPC&AM);
	if(!p){CV1K_STAT(dtHandler);return false;}
	UINT16 next;memcpy(&next,p,2);
	if(next==0x8bfd){CV1K_STAT(dtBusy);return false;}
#endif
	--m_r[Rn];m_sr=(m_sr&~T)|UINT32(!m_r[Rn]);
	return true;
}

// Only audited single-cycle, callback-free handlers can execute inside
// a fused branch. Direct accesses can reject without changing state.
template<void (*Op)(UINT16)>
static bool Cv1kPureSlot(UINT16 opcode,UINT32)
{
	Op(opcode);return true;
}
template<bool Store,UINT32 Sh3HotState::*Reg,bool Regions>
static bool Cv1kStackSlot(UINT16 opcode,UINT32)
{
	return Cv1kTryStack<Store,Reg,Regions>(opcode);
}
typedef bool (*Cv1kSlotFn)(UINT16,UINT32);
template<bool Regions>
static const Cv1kSlotFn *Cv1kFlowSlots()
{
static const Cv1kSlotFn slots[]={
#define SH3_OPCODE(name) nullptr,
#define SH3_PURE_OPCODE(name) &Cv1kPureSlot<&name>,
#define SH3_FLOW_OPCODE(name) &Cv1kPureSlot<&name>,
#define SH3_MEMORY_OPCODE(name,width,store,form) &Cv1kTryDirectMove<width,store,form,Regions>,
#define SH3_STACK_OPCODE(name,store,reg) &Cv1kStackSlot<store,&Sh3HotState::reg,Regions>,
#include "sh3_decode_ops.inc"
#undef SH3_STACK_OPCODE
#undef SH3_MEMORY_OPCODE
#undef SH3_FLOW_OPCODE
#undef SH3_PURE_OPCODE
#undef SH3_OPCODE
};
static_assert(sizeof(slots)/sizeof(slots[0])==SH3_ID_COUNT,"slot decoder order");
return slots;
}

enum Cv1kBranchKind {
	CV1K_BRANCH_BF,CV1K_BRANCH_BFS,CV1K_BRANCH_BRA,CV1K_BRANCH_BRAF,
	CV1K_BRANCH_BSR,CV1K_BRANCH_BSRF,CV1K_BRANCH_BT,CV1K_BRANCH_BTS,
	CV1K_BRANCH_JMP,CV1K_BRANCH_JSR,CV1K_BRANCH_RTS
};
struct Cv1kFlowResult {UINT32 target,ppc;INT32 cost;bool slot;};

template<bool Regions=false>
static inline bool Cv1kTryFlowBranch(Cv1kBranchKind kind,UINT16 opcode,UINT32 nextPC,
	INT32 available,const UINT16 *sequential,Cv1kFlowResult &out)
{
	if(available<=0){CV1K_STAT(branchReject[0]);return false;}
	UINT32 target;
	INT32 cost=2;
	bool slot=true,setEA=true,setPR=false;
	switch(kind){
	case CV1K_BRANCH_BF:
	case CV1K_BRANCH_BT:
		// The original post-BF data/counted-loop probes must still run.
		if(kind==CV1K_BRANCH_BF &&
			(cv1kInterpLoopsEnabled || (opcode==0x8bfa && cv1kDataLoopsEnabled))){CV1K_STAT(branchReject[1]);return false;}
		cost=3;slot=false;
		target=nextPC+2+INT32(INT8(opcode&255))*2;break;
	case CV1K_BRANCH_BFS:
	case CV1K_BRANCH_BTS:
		target=nextPC+2+INT32(INT8(opcode&255))*2;break;
	case CV1K_BRANCH_BRA:
	case CV1K_BRANCH_BSR:{
		const INT32 disp=INT32(UINT32(opcode&0xfff)<<20)>>20;
#if BUSY_LOOP_HACKS
		if(kind==CV1K_BRANCH_BRA && disp==-2){CV1K_STAT(branchReject[2]);return false;}
#endif
		target=nextPC+2+disp*2;setPR=kind==CV1K_BRANCH_BSR;break;
	}
	case CV1K_BRANCH_BRAF:
	case CV1K_BRANCH_BSRF:
		target=nextPC+2+m_r[Rn];setEA=false;setPR=kind==CV1K_BRANCH_BSRF;break;
	case CV1K_BRANCH_JMP:
	case CV1K_BRANCH_JSR:
		target=m_r[Rn];setPR=kind==CV1K_BRANCH_JSR;
		cost=setPR?2:1;break;
	case CV1K_BRANCH_RTS:target=m_pr;break;
	default:return false;
	}
	if(target&1){CV1K_STAT(branchReject[3]);return false;}
	if(slot){
		// A slice ending after the branch must retain the pending slot.
		// A zero slot address is also the original delay sentinel.
		if(available<=cost){CV1K_STAT(branchReject[0]);return false;}
		if(!nextPC || (nextPC&1)){CV1K_STAT(branchReject[3]);return false;}
		UINT16 op;
		if(sequential && (nextPC&SH3_PAGEM))op=*sequential;
		else {
			const UINT32 a=nextPC&AM;
			const UINT8 *page=MemMapF[a>>SH3_SHIFT];
			if((uintptr_t)page<SH3_MAXHANDLER){CV1K_STAT(branchReject[4]);return false;}
			memcpy(&op,page+(a&SH3_PAGEM),2);
		}
		const UINT32 distance=nextPC-target;
		const bool addSmall=(op&0xf000)==0x7000 &&
			((op&255)==1 || (op&255)==2 || (op&255)==4);
		// Conservatively leave every potentially matching probe edge
		// to the existing dispatcher, even at a smaller cycle budget.
		if(cv1kDataLoopsEnabled &&
			((distance==22 && (op&0xf00f)==0x600e) || (distance==8 && addSmall))){CV1K_STAT(branchReject[1]);return false;}
		if(cv1kInterpLoopsEnabled &&
			(op==0x0009 || (op&0xf000)==0x7000) &&
			(distance==4 || distance==6 || distance==8)){CV1K_STAT(branchReject[1]);return false;}
		// Common register-only slots neither read nor write the
		// branch's EA or PR. Avoid a second dispatch and rollback.
		if((op&0xf000)==0x7000)ADDI(op);
		else if(op==0x0009){}
		else if((op&0xf000)==0xe000)MOVI(op);
		else if((op&0xf00f)==0x6003)MOV(op);
		else if((op&0xf00f)==0x600e)EXTSB(op);
		else {
			Cv1kSlotFn fn=Cv1kFlowSlots<Regions>()[sh3OpIds[op]];
			if(!fn){CV1K_STAT(branchReject[5]);return false;}
			const UINT32 oldEA=m_ea,oldPR=m_pr;
			if(setEA)m_ea=target;
			if(setPR)m_pr=nextPC+2;
			// PC-relative MOVs use the branch target. A rejected
			// direct access has not changed any guest state.
			if(!fn(op,target)){m_ea=oldEA;m_pr=oldPR;CV1K_STAT(branchReject[6]);return false;}
			CV1K_STAT(fused[sh3OpIds[op]]);CV1K_STAT(indirect[sh3OpIds[op]]);
			++cost;
			out={target,target,cost,true};
			return true;
		}
		CV1K_STAT(fused[sh3OpIds[op]]);
		if(setEA)m_ea=target;
		if(setPR)m_pr=nextPC+2;
		++cost;
	}else{
		if(setEA)m_ea=target;
		if(setPR)m_pr=nextPC+2;
	}
	out={target,slot?target:nextPC,cost,slot};
	return true;
}
