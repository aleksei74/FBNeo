// Direct mapped one-cycle MOV instructions. A rejected access must not
// change registers, EA, memory or cycles: the original handler will run.
// WaitState is disabled in this core; never use this path with wait states.
enum Cv1kMoveForm {
	CV1K_REG,CV1K_PRE,CV1K_POST,CV1K_INDEX,CV1K_GBR,
	CV1K_DISP4_R0,CV1K_DISP4,CV1K_LITERAL
};

template<unsigned Width, bool Store, Cv1kMoveForm Form>
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
	// Preserve P4/internal/cache accesses, unaligned accesses and every
	// MMIO or long-only idle handler by taking the normal slow path.
	if(ea>=0xe0000000u || (ea&(Width-1)))return false;
	UINT32 a=ea&AM;
	UINT8 *page=Store?MemMapW[a>>SH3_SHIFT]:
		(Width==4?MemMapRL[a>>SH3_SHIFT]:MemMapR[a>>SH3_SHIFT]);
	if((uintptr_t)page<SH3_MAXHANDLER)return false;
#ifdef LSB_FIRST
	if(Width==1)a^=1;
#endif
	UINT8 *p=page+(a&SH3_PAGEM);
	if(Store){
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
