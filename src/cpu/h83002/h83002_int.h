#pragma once

typedef struct h83xx_state h83xx_state;
struct h83xx_state
{
	UINT32 h8err;
	UINT32 regs[8];
	UINT32 pc;
	UINT32 ppc;
	UINT32 h8_IRQrequestH;
	UINT32 h8_IRQrequestL;
	INT32 cyccnt;
	UINT8 ccr;
	UINT8 h8nflag;
	UINT8 h8vflag;
	UINT8 h8cflag;
	UINT8 h8zflag;
	UINT8 h8iflag;
	UINT8 h8hflag;
	UINT8 h8uflag;
	UINT8 h8uiflag;
	UINT8 incheckirqs;
	UINT8 mode_8bit;
	UINT8 per_regs[0x100];
	UINT8 internal_ram[0x200];
	UINT32 timer16_cycles[5];
	UINT8 timer16_isr[5];
};
