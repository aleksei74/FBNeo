/***************************************************************************

 h8_16.c: Hitachi H8/3xx series 16/32-bit microcontroller emulator

 Original by The_Author & DynaChicken for the ZiNc emulator.
 MAME changes by R. Belmont, Luca Elia, and Tomasz Slanina.

 TS 20060412 Added exts.l, sub.l, divxs.w (buggy), jsr @reg, rotxl.l reg, mov.l @(adr, reg), reg
 LE 20070903 Added divxu.b  shal.l  extu.w  dec.l #Imm,Rd  subx.b
 LE 20080202 Separated 3002/3044/3007, Added or.l  shal.l  rotl.l  not.l  neg.l  exts.w
             sub/or/xor.l #Imm:32,ERd  bset/bnot/bclr.b Rn,@ERd  bst/bist.b #Imm:3,@ERd  bnot.b #Imm:3,@ERd
 LE 20090128 Added mov.l ers,@aa:16;  bild #xx:3,rd;  eepmov.b;  bnot #xx:3,@aa:8

***************************************************************************/

#include "burnint.h"
#include "h83002.h"
#include "h83002_int.h"

#define IFLAG  0x80
#define UIFLAG 0x40
#define HFLAG  0x20
#define UFLAG  0x10
#define NFLAG  0x08
#define ZFLAG  0x04
#define VFLAG  0x02
#define CFLAG  0x01
#define INLINE static inline
#define U64(value) value##ULL

static h83xx_state h8_state;
static h83002_read8_handler h8_read;
static h83002_write8_handler h8_write;

static const UINT8 h8_timer16_base[5] = { 0x64, 0x6e, 0x78, 0x82, 0x92 };

static INT32 h8_timer16_channel(UINT8 offset)
{
	for (INT32 channel = 0; channel < 5; channel++) {
		if (offset >= h8_timer16_base[channel] && offset <= h8_timer16_base[channel] + 9) {
			return channel;
		}
	}

	return -1;
}

static UINT8 h8_mem_read8_handler(UINT32 address)
{
	address &= 0xffffff;
	if (address >= 0xfffd10 && address <= 0xffff0f) {
		return h8_state.internal_ram[address - 0xfffd10];
	}
	if (address >= 0xffff10) {
		UINT8 offset = address & 0xff;
		INT32 channel = h8_timer16_channel(offset);

		if (channel >= 0) {
			UINT8 reg = offset - h8_timer16_base[channel];
			if (reg == 2) return h8_state.per_regs[offset] | 0xf8;
			if (reg == 3) return 0xf8 | (h8_state.timer16_isr[channel] & 7);
		}

		switch (address & 0xff) {
			case 0xb4:
			case 0xbc:
				return h8_state.per_regs[address & 0xff] | 0xc4;
			case 0xe8:
				return 0x80;
			case 0xc7:
			case 0xcb:
			case 0xce:
			case 0xcf:
			case 0xd2:
			case 0xd3:
			case 0xd6:
				return h8_read ? h8_read(address) : 0xff;
		}
		return h8_state.per_regs[address & 0xff];
	}
	return h8_read ? h8_read(address) : 0xff;
}

static void h8_mem_write8_handler(UINT32 address, UINT8 data)
{
	address &= 0xffffff;
	if (address >= 0xfffd10 && address <= 0xffff0f) {
		h8_state.internal_ram[address - 0xfffd10] = data;
		return;
	}
	if (address >= 0xffff10) {
		UINT8 offset = address & 0xff;
		INT32 channel = h8_timer16_channel(offset);

		if (channel >= 0) {
			UINT8 reg = offset - h8_timer16_base[channel];
			if (reg == 2) data |= 0xf8;
			if (reg == 3) {
				h8_state.timer16_isr[channel] &= data & 7;
				h8_state.per_regs[offset] = 0xf8 | h8_state.timer16_isr[channel];
				if (h8_write) h8_write(address, data);
				return;
			}
		}

		h8_state.per_regs[offset] = data;
		if (h8_write) h8_write(address, data);
		return;
	}
	if (h8_write) h8_write(address, data);
}

#define H8_SP	(7)

#define h8_mem_read8(x) h8_mem_read8_handler(x)
#define h8_mem_read16(z, x) ((h8_mem_read8_handler(x) << 8) | h8_mem_read8_handler((x) + 1))
#define h8_mem_write8(x, y) h8_mem_write8_handler(x, y)
#define h8_mem_write16(z, x, y) do { h8_mem_write8_handler(x, (y) >> 8); h8_mem_write8_handler((x) + 1, y); } while (0)
#define h8_readop16(x, y) h8_mem_read16(x, y)

// timing macros
// note: we assume a system 12 - type setup where external access is 3+1 states
// timing will be off somewhat for other configurations.
#define H8_IFETCH_TIMING(x)	h8->cyccnt -= (x) * 4;
#define H8_BRANCH_TIMING(x)	h8->cyccnt -= (x) * 4;
#define H8_STACK_TIMING(x)	h8->cyccnt -= (x) * 4;
#define H8_BYTE_TIMING(x, adr)	if (address24 >= 0xffff10) h8->cyccnt -= (x) * 3; else h8->cyccnt -= (x) * 4;
#define H8_WORD_TIMING(x, adr)	if (address24 >= 0xffff10) h8->cyccnt -= (x) * 3; else h8->cyccnt -= (x) * 4;
#define H8_IOP_TIMING(x)	h8->cyccnt -= (x);
#define logerror(...) do { } while (0)
#define fatalerror(...) do { h8->h8err = 1; } while (0)

INLINE UINT32 h8_mem_read32(h83xx_state *h8, UINT32 address)
{
	UINT32 result = h8_mem_read16(h8, address) << 16;
	return result | h8_mem_read16(h8, address + 2);
}

INLINE void h8_mem_write32(h83xx_state *h8, UINT32 address, UINT32 data)
{
	h8_mem_write16(h8, address, data >> 16);
	h8_mem_write16(h8, address + 2, data);
}

static void h8_check_irqs(h83xx_state *h8);

/* implementation */

void h8_3002_InterruptRequest(h83xx_state *h8, UINT8 source, UINT8 state)
{
	// don't allow clear on external interrupts
	if ((source <= 17) && (state == 0)) return;

	if (state)
	{
		if (source>31)
		{
			h8->h8_IRQrequestH |= (1<<(source-32));
		}
		else
		{
			h8->h8_IRQrequestL |= (1<<source);
		}
	}
	else
	{
		if (source>31)
		{
			h8->h8_IRQrequestH &= ~(1<<(source-32));
		}
		else
		{
			h8->h8_IRQrequestL &= ~(1<<source);
		}
	}
}


static UINT8 h8_get_ccr(h83xx_state *h8)
{
	h8->ccr = 0;
	if(h8->h8nflag)h8->ccr |= NFLAG;
	if(h8->h8zflag)h8->ccr |= ZFLAG;
	if(h8->h8vflag)h8->ccr |= VFLAG;
	if(h8->h8cflag)h8->ccr |= CFLAG;
	if(h8->h8uflag)h8->ccr |= UFLAG;
	if(h8->h8hflag)h8->ccr |= HFLAG;
	if(h8->h8uiflag)h8->ccr |= UIFLAG;
	if(h8->h8iflag)h8->ccr |= IFLAG;
	return h8->ccr;
}

static char *h8_get_ccr_str(h83xx_state *h8)
{
	static char res[8];

	memset(res, 0, 8);
	if(h8->h8iflag) strcat(res, "I"); else strcat(res, "i");
	if(h8->h8uiflag)strcat(res, "U"); else strcat(res, "u");
	if(h8->h8hflag) strcat(res, "H"); else strcat(res, "h");
	if(h8->h8uflag) strcat(res, "U"); else strcat(res, "u");
	if(h8->h8nflag) strcat(res, "N"); else strcat(res, "n");
	if(h8->h8zflag) strcat(res, "Z"); else strcat(res, "z");
	if(h8->h8vflag) strcat(res, "V"); else strcat(res, "v");
	if(h8->h8cflag) strcat(res, "C"); else strcat(res, "c");

	return res;
}

static void h8_set_ccr(h83xx_state *h8, UINT8 data)
{
	h8->ccr = data;

	h8->h8nflag = 0;
	h8->h8zflag = 0;
	h8->h8vflag = 0;
	h8->h8cflag = 0;
	h8->h8hflag = 0;
	h8->h8iflag = 0;
	h8->h8uflag = 0;
	h8->h8uiflag = 0;

	if(h8->ccr & NFLAG) h8->h8nflag = 1;
	if(h8->ccr & ZFLAG) h8->h8zflag = 1;
	if(h8->ccr & VFLAG) h8->h8vflag = 1;
	if(h8->ccr & CFLAG) h8->h8cflag = 1;
	if(h8->ccr & HFLAG) h8->h8hflag = 1;
	if(h8->ccr & UFLAG) h8->h8uflag = 1;
	if(h8->ccr & UIFLAG) h8->h8uiflag = 1;
	if(h8->ccr & IFLAG) h8->h8iflag = 1;

	if (!h8->incheckirqs) h8_check_irqs(h8);
}

static INT16 h8_getreg16(h83xx_state *h8, UINT8 reg)
{
	if(reg > 7)
	{
		return h8->regs[reg-8]>>16;
	}
	else
	{
		return h8->regs[reg];
	}
}

static void h8_setreg16(h83xx_state *h8, UINT8 reg, UINT16 data)
{
	if(reg > 7)
	{
		h8->regs[reg-8] &= 0xffff;
		h8->regs[reg-8] |= data<<16;
	}
	else
	{
		h8->regs[reg] &= 0xffff0000;
		h8->regs[reg] |= data;
	}
}

static UINT8 h8_getreg8(h83xx_state *h8, UINT8 reg)
{
	if(reg > 7)
	{
		return h8->regs[reg-8];
	}
	else
	{
		return h8->regs[reg]>>8;
	}
}

static void h8_setreg8(h83xx_state *h8, UINT8 reg, UINT8 data)
{
	if(reg > 7)
	{
		h8->regs[reg-8] &= 0xffffff00;
		h8->regs[reg-8] |= data;
	}
	else
	{
		h8->regs[reg] &= 0xffff00ff;
		h8->regs[reg] |= data<<8;
	}
}

static UINT32 h8_getreg32(h83xx_state *h8, UINT8 reg)
{
	return h8->regs[reg];
}

static void h8_setreg32(h83xx_state *h8, UINT8 reg, UINT32 data)
{
	h8->regs[reg] = data;
}

static void h8_GenException(h83xx_state *h8, UINT8 vectornr)
{
	UINT32 sp = h8_getreg32(h8, H8_SP) - 4;
	UINT32 pc = h8->pc;

	// H8/300H advanced mode stores CCR:PC as one four-byte frame.
	h8_setreg32(h8, H8_SP, sp);
	h8_mem_write16(h8, sp, (h8_get_ccr(h8) << 8) | ((pc >> 16) & 0xff));
	h8_mem_write16(h8, sp + 2, pc & 0xffff);

	// generate address from vector
	h8_set_ccr(h8, h8_get_ccr(h8) | 0x80);
	if (h8->h8uiflag == 0)
		h8_set_ccr(h8, h8_get_ccr(h8) | 0x40);
	h8->pc = h8_mem_read32(h8, vectornr * 4) & 0xffffff;

	// I couldn't find timing info for exceptions, so this is a guess (based on JSR/BSR)
	H8_IFETCH_TIMING(2);
	H8_STACK_TIMING(2);
}

static int h8_get_priority(h83xx_state *h8, UINT8 bit)
{
	int res = 0;
	switch(bit)
	{
	case 12: // IRQ0
		if (h8->per_regs[0xF8]&0x80) res = 1; break;
	case 13: // IRQ1
		if (h8->per_regs[0xF8]&0x40) res = 1; break;
	case 14: // IRQ2
	case 15: // IRQ3
		if (h8->per_regs[0xF8]&0x20) res = 1; break;
	case 16: // IRQ4
	case 17: // IRQ5
		if (h8->per_regs[0xF8]&0x10) res = 1; break;
	case 53: // SCI0 Rx
		if (!(h8->per_regs[0xB2]&0x40)) res = -2;
		else if (h8->per_regs[0xF9]&0x08) res = 1; break;
	case 54: // SCI0 Tx Empty
		if (!(h8->per_regs[0xB2]&0x80)) res = -2;
		else if (h8->per_regs[0xF9]&0x08) res = 1; break;
	case 55: // SCI0 Tx End
		if (!(h8->per_regs[0xB2]&0x04)) res = -2;
		else if (h8->per_regs[0xF9]&0x08) res = 1; break;
	case 57: // SCI1 Rx
		if (!(h8->per_regs[0xBA]&0x40)) res = -2;
		else if (h8->per_regs[0xF9]&0x04) res = 1; break;
	case 58: // SCI1 Tx Empty
		if (!(h8->per_regs[0xBA]&0x80)) res = -2;
		else if (h8->per_regs[0xF9]&0x04) res = 1; break;
	case 59: // SCI1 Tx End
		if (!(h8->per_regs[0xBA]&0x04)) res = -2;
		else if (h8->per_regs[0xF9]&0x04) res = 1; break;
	}
	return res;
}

static void h8_check_irqs(h83xx_state *h8)
{
	int lv = -1;

	h8->incheckirqs = 1;

	if (h8->h8iflag == 0)
	{
		lv = 0;
	}
	else
	{
		if ((h8->per_regs[0xF2]&0x08)/*SYSCR*/ == 0)
		{
			if (h8->h8uiflag == 0)
				lv = 1;
		}
	}

	// any interrupts wanted and can accept ?
	if(((h8->h8_IRQrequestH != 0) || (h8->h8_IRQrequestL != 0)) && (lv >= 0))
	{
		UINT8 bit, source;
		// which one ?
		for(bit = 0, source = 0xff; source == 0xff && bit < 32; bit++)
		{
			if( h8->h8_IRQrequestL & (1<<bit) )
			{
				if (h8_get_priority(h8, bit) >= lv)
				{
					// mask off
					h8->h8_IRQrequestL &= ~(1<<bit);
					source = bit;
				}
			}
		}
		// which one ?
		for(bit = 0; source == 0xff && bit < 32; bit++)
		{
			if( h8->h8_IRQrequestH & (1<<bit) )
			{
				if (h8_get_priority(h8, bit + 32) >= lv)
				{
					// mask off
					h8->h8_IRQrequestH &= ~(1<<bit);
					source = bit + 32;
				}
			}
		}

		if (source != 0xff)
			h8_GenException(h8, source);
	}

	h8->incheckirqs = 0;
}

#define H8_ADDR_MASK 0xffffff
#include "h83002_ops.h"

void H83002Init(h83002_read8_handler read, h83002_write8_handler write)
{
	h8_read = read;
	h8_write = write;
	memset(&h8_state, 0, sizeof(h8_state));
}

void H83002Exit()
{
	h8_read = NULL;
	h8_write = NULL;
}

void H83002Reset()
{
	memset(&h8_state, 0, sizeof(h8_state));
	h8_state.per_regs[0x60] = 0xe0;
	for (INT32 channel = 0; channel < 5; channel++) {
		UINT8 base = h8_timer16_base[channel];
		h8_state.per_regs[base + 2] = 0xf8;
		h8_state.per_regs[base + 3] = 0xf8;
		h8_state.per_regs[base + 6] = 0xff;
		h8_state.per_regs[base + 7] = 0xff;
		h8_state.per_regs[base + 8] = 0xff;
		h8_state.per_regs[base + 9] = 0xff;
	}
	h8_state.h8iflag = 1;
	h8_state.pc = h8_mem_read32(&h8_state, 0) & 0xffffff;
}

static void h8_timer16_update(INT32 cycles)
{
	UINT8 tstr = h8_state.per_regs[0x60];

	for (INT32 channel = 0; channel < 5; channel++) {
		if ((tstr & (1 << channel)) == 0) continue;

		UINT8 base = h8_timer16_base[channel];
		UINT8 tcr = h8_state.per_regs[base];
		INT32 clock_select = tcr & 7;
		if (clock_select >= 4) continue;

		h8_state.timer16_cycles[channel] += cycles;
		UINT32 ticks = h8_state.timer16_cycles[channel] >> clock_select;
		h8_state.timer16_cycles[channel] &= (1 << clock_select) - 1;
		if (ticks == 0) continue;

		UINT32 counter = (h8_state.per_regs[base + 4] << 8) | h8_state.per_regs[base + 5];
		UINT32 tgr[2] = {
			(UINT32)((h8_state.per_regs[base + 6] << 8) | h8_state.per_regs[base + 7]),
			(UINT32)((h8_state.per_regs[base + 8] << 8) | h8_state.per_regs[base + 9])
		};
		INT32 clear_source = -1;
		if ((tcr & 0x60) == 0x20) clear_source = 0;
		if ((tcr & 0x60) == 0x40) clear_source = 1;
		UINT32 counter_cycle = clear_source >= 0 ? tgr[clear_source] + 1 : 0x10000;

		while (ticks--) {
			UINT32 next = counter + 1;
			bool overflow = next >= 0x10000;

			if (counter_cycle != 0x10000 && next >= counter_cycle) {
				next = 0;
			} else {
				next &= 0xffff;
			}

			for (INT32 compare = 0; compare < 2; compare++) {
				if (next == ((tgr[compare] + 1) & 0xffff)) {
					UINT8 flag = 1 << compare;
					h8_state.timer16_isr[channel] |= flag;
					if (h8_state.per_regs[base + 2] & flag) {
						h8_3002_InterruptRequest(&h8_state, 24 + channel * 4 + compare, 1);
					}
				}
			}

			if (overflow) {
				h8_state.timer16_isr[channel] |= 4;
				if (h8_state.per_regs[base + 2] & 4) {
					h8_3002_InterruptRequest(&h8_state, 26 + channel * 4, 1);
				}
			}

			counter = next;
		}

		h8_state.per_regs[base + 3] = 0xf8 | (h8_state.timer16_isr[channel] & 7);
		h8_state.per_regs[base + 4] = counter >> 8;
		h8_state.per_regs[base + 5] = counter;
	}
}

INT32 H83002Run(INT32 cycles)
{
	h8_state.cyccnt = cycles;
	INT32 executed = h8_execute(&h8_state, cycles);
	h8_timer16_update(executed);
	return executed;
}

void H83002SetIRQLine(INT32 line, INT32 state)
{
	if (line >= 0 && line < 6) {
		h8_3002_InterruptRequest(&h8_state, 12 + line, state != 0);
	}
}

INT32 H83002Scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(h8_state);
	}
	return 0;
}
