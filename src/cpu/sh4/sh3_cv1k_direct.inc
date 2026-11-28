// Direct mapped one-cycle MOV instructions. A rejected access must not
// change registers, EA, memory or cycles: the original handler will run.
// WaitState is disabled in this core; never use this path with wait states.
enum Cv1kMoveForm {
	CV1K_REG,CV1K_PRE,CV1K_POST,CV1K_INDEX,CV1K_GBR,
	CV1K_DISP4_R0,CV1K_DISP4,CV1K_LITERAL
};

// Resolve a live, aligned, callback-free access. The registered base
// shortcuts are revoked by all map APIs and idle-handler replacement.
// This is a cold exception to the original map lookup, not a
// direct-region check on every ordinary load. Live maps are authoritative.
static __attribute__((noinline)) UINT8 *Cv1kTryIdleBatchPointer(UINT32 a,UINT8 *page)
{
	if((a>>SH3_SHIFT)==0xc00 && Cv1kIsIdlePage(page)){
		if(cv1kIdleBatchEnabled && a!=cv1kIdleAddress){
			CV1K_STAT(idlePage);
			return cv1kIdlePage+(a&SH3_PAGEM);
		}
		CV1K_STAT(idleReject);
	}else CV1K_STAT(handler);
	return nullptr;
}
template<unsigned Width, bool Store, bool Regions=false>
static inline __attribute__((always_inline)) UINT8 *Cv1kDirectPointer(UINT32 ea)
{
	if(ea>=0xe0000000u){CV1K_STAT(p4);return nullptr;}
	if(ea&(Width-1)){CV1K_STAT(unaligned);return nullptr;}
	UINT32 a=ea&AM;
	const UINT32 off=a-0xc000000u;
	if(Regions && off<0x1000000u && cv1kFastRamBase){
		if(!Store && Width==4 && off<0x10000u && cv1kFastIdleAddress!=~UINT32(0) &&
			(!cv1kIdleBatchEnabled || a==cv1kFastIdleAddress)){
			CV1K_STAT(idleReject);return nullptr;
		}
		CV1K_STAT(ram);
		if(!Store && Width==4 && off<0x10000u &&
			cv1kFastIdleAddress!=~UINT32(0))CV1K_STAT(idlePage);
		UINT32 pos=off&cv1kFastRamMask;
#ifdef LSB_FIRST
		if(Width==1)pos^=1;
#endif
		return cv1kFastRamBase+pos;
	}
	if(Regions && !Store && a<0x400000u && cv1kFastRomBase){
		CV1K_STAT(rom);
#ifdef LSB_FIRST
		if(Width==1)a^=1;
#endif
		return cv1kFastRomBase+a;
	}
	UINT8 *page=Store?MemMapW[a>>SH3_SHIFT]:
		(Width==4?MemMapRL[a>>SH3_SHIFT]:MemMapR[a>>SH3_SHIFT]);
	if((uintptr_t)page<SH3_MAXHANDLER){
		if(!Store && Width==4)return Cv1kTryIdleBatchPointer(a,page);
		CV1K_STAT(handler);return nullptr;
	}
	CV1K_STAT(mapped);
#ifdef LSB_FIRST
	if(Width==1)a^=1;
#endif
	return page+(a&SH3_PAGEM);
}

template<unsigned Width, bool Store, Cv1kMoveForm Form, bool Regions=false>
static inline __attribute__((always_inline)) bool Cv1kTryDirectMove(UINT16 opcode,UINT32 nextPC)
{
	const unsigned base=Form==CV1K_DISP4_R0?Rm:Store?Rn:Rm;
	const unsigned valueReg=(Form==CV1K_GBR || Form==CV1K_DISP4_R0)?0:Store?Rm:Rn;
	UINT32 ea;
	if(Form==CV1K_LITERAL)
		ea=(Width==4?((nextPC+2)&~3u):nextPC+2)+(opcode&255)*Width;
	else if(Form==CV1K_GBR) ea=m_gbr+(opcode&255)*Width;
	else {
		ea=m_r[base];
		if(Form==CV1K_PRE)ea-=Width;
		if(Form==CV1K_INDEX)ea+=m_r[0];
		if(Form==CV1K_DISP4 || Form==CV1K_DISP4_R0)ea+=(opcode&15)*Width;
	}
	UINT8 *p=Cv1kDirectPointer<Width,Store,Regions>(ea);
	if(!p)return false;
	if(Store){
		Cv1kV2NoteWrite(ea,Width);
		UINT32 v=m_r[valueReg]; // predecrement aliases use the old value
		if(Form==CV1K_PRE)m_r[base]=ea;
		else m_ea=ea;
		if(Width==1)*p=UINT8(v);
		else if(Width==2){const UINT16 w=UINT16(v);memcpy(p,&w,2);}
		else{
#ifdef LSB_FIRST
			v=(v<<16)|(v>>16);
#endif
			memcpy(p,&v,4);
		}
	}else{
		UINT32 v;
		if(Width==1)v=UINT32(INT32(INT8(*p)));
		else if(Width==2){UINT16 w;memcpy(&w,p,2);v=UINT32(INT32(INT16(w)));}
		else{
			memcpy(&v,p,4);
#ifdef LSB_FIRST
			v=(v<<16)|(v>>16);
#endif
		}
		// The original predecrement/postincrement handlers leave EA alone.
		if(Form!=CV1K_POST)m_ea=ea;
		m_r[valueReg]=v;
		if(Form==CV1K_POST && base!=valueReg)m_r[base]+=Width;
	}
	return true;
}

// Delay slots retain the existing published target PC, PPC, delay clear,
// extra cycles and IRQ checks. Only opcode selection is changed.
static inline void Cv1kDispatchSlot(UINT16 opcode)
{
	static void (*const handlers[])(UINT16)={
#define SH3_OPCODE(name) &name,
#include "sh3_decode_ops.inc"
#undef SH3_OPCODE
	};
	handlers[sh3OpIds[opcode]](opcode);
}
