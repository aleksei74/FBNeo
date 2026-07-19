#include "psx.h"
#include "psx_gte.h"

#define PSX_CAUSE	13
#define PSX_STATUS	12
#define PSX_EPC		14
#define PSX_TAR		6
#define PSX_BADVADDR	8

#define PSX_EXC_ADEL	4
#define PSX_EXC_ADES	5
#define PSX_EXC_DBE	7
#define PSX_EXC_RI	10
#define PSX_EXC_CPU	11
#define PSX_EXC_OVF	12

#define SR_ISC		0x00010000
#define SR_BEV		0x00400000
#define SR_KUC		0x00000002
#define SR_CU0		0x10000000
#define SR_CU1		0x20000000
#define SR_CU2		0x40000000
#define SR_CU3		0x80000000

#define CAUSE_EXC	0x0000007c
#define CAUSE_CE	0x30000000
#define CAUSE_BT	0x40000000
#define CAUSE_BD	0x80000000

#define PSX_DELAY_PC	32
#define PSX_DELAY_NOTPC	33

#define BIU_LOCK	0x00000001
#define BIU_INV		0x00000002
#define BIU_TAG		0x00000004
#define BIU_RAM		0x00000008
#define BIU_DS		0x00000080
#define BIU_IS1		0x00000800

#define ICACHE_ENTRIES	0x400
#define DCACHE_ENTRIES	0x100
#define TAG_MATCH_MASK	(0 - (ICACHE_ENTRIES * 4))
#define TAG_MATCH	0x10
#define TAG_VALID	0x0f

static const UINT32 psx_cp0_write_mask[32] = {
	0x00000000, 0x00000000, 0x00000000, 0xffffffff,
	0x00000000, 0xffffffff, 0x00000000, 0xff80f03f,
	0x00000000, 0xffffffff, 0x00000000, 0xffffffff,
	0xf04fff3f, 0x00000300, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000
};

struct psx_core_state {
	UINT32 pc;
	UINT32 next_pc;
	UINT32 r[32];
	UINT32 cp0[32];
	UINT32 hi;
	UINT32 lo;
	UINT32 delay_reg;
	UINT32 delay_value;
	UINT32 commit_reg;
	UINT32 commit_value;
	INT32 new_delay;
	UINT32 biu;
	UINT32 icache_tag[ICACHE_ENTRIES / 4];
	UINT32 icache[ICACHE_ENTRIES];
	UINT32 dcache[DCACHE_ENTRIES];
	INT32 exception_taken;
	INT32 bus_error;
	INT32 cycles;
	INT32 irq_state[8];
	INT32 irq_delay;
	psx_core_handlers handlers;
};

static psx_core_state psx;
static gte psx_gte;
static UINT32 psx_current_pc;
static UINT32 psx_current_op;
static psx_instruction_hook psx_hook;
static INT32 psx_external_interrupt_gte_completion;

static inline INT32 sext16(UINT32 data)
{
	return (INT16)(data & 0xffff);
}

static inline INT32 branch_offset(UINT32 op)
{
	return (INT32)((INT16)(op & 0xffff)) * 4;
}

static inline UINT32 op_rs(UINT32 op) { return (op >> 21) & 31; }
static inline UINT32 op_rt(UINT32 op) { return (op >> 16) & 31; }
static inline UINT32 op_rd(UINT32 op) { return (op >> 11) & 31; }
static inline UINT32 op_sa(UINT32 op) { return (op >>  6) & 31; }
static inline UINT32 op_imm(UINT32 op) { return op & 0xffff; }
static inline UINT32 op_target(UINT32 op) { return op & 0x03ffffff; }

static void take_exception(INT32 code);

static inline UINT32 cache_read_word(UINT32 address)
{
	UINT32 data = 0;

	if (psx.biu & BIU_TAG) {
		if (psx.biu & BIU_IS1) {
			UINT32 tag = psx.icache_tag[(address / 16) % (ICACHE_ENTRIES / 4)];
			data |= tag & TAG_VALID;

			if (((tag ^ address) & TAG_MATCH_MASK) == 0) {
				data |= TAG_MATCH;
			}
		}
	} else if (psx.biu & (BIU_LOCK | BIU_INV)) {
	} else {
		if ((psx.biu & BIU_IS1) == BIU_IS1) {
			data |= psx.icache[(address / 4) % ICACHE_ENTRIES];
		}

		if ((psx.biu & BIU_DS) == BIU_DS) {
			data |= psx.dcache[(address / 4) % DCACHE_ENTRIES];
		}
	}

	return data;
}

static inline void cache_write_word(UINT32 address, UINT32 data)
{
	if (psx.biu & BIU_TAG) {
		if (psx.biu & BIU_IS1) {
			psx.icache_tag[(address / 16) % (ICACHE_ENTRIES / 4)] = (data & TAG_VALID) | (address & TAG_MATCH_MASK);
		}
	} else if (psx.biu & (BIU_LOCK | BIU_INV)) {
		if (psx.biu & BIU_IS1) {
			psx.icache_tag[(address / 16) % (ICACHE_ENTRIES / 4)] = address & TAG_MATCH_MASK;
		}
	} else {
		if (psx.biu & BIU_IS1) {
			psx.icache[(address / 4) % ICACHE_ENTRIES] = data;
		}

		if (psx.biu & BIU_DS) {
			psx.dcache[(address / 4) % DCACHE_ENTRIES] = data;
		}
	}

}

static inline UINT8 read_byte(UINT32 address)
{
	if (psx.cp0[PSX_STATUS] & SR_ISC) {
		UINT8 data = cache_read_word(address) >> ((address & 3) << 3);
		return data;
	}

	psx.bus_error = 0;
	UINT8 data = psx.handlers.read_byte ? psx.handlers.read_byte(address) : 0xff;
	if (psx.bus_error) take_exception(PSX_EXC_DBE);
	return data;
}

static inline UINT16 read_half(UINT32 address)
{
	if (psx.cp0[PSX_STATUS] & SR_ISC) {
		return cache_read_word(address) >> ((address & 2) << 3);
	}

	psx.bus_error = 0;
	UINT16 data = psx.handlers.read_half ? psx.handlers.read_half(address) : 0xffff;
	if (psx.bus_error) take_exception(PSX_EXC_DBE);
	return data;
}

static inline UINT32 read_word(UINT32 address)
{
	if (psx.cp0[PSX_STATUS] & SR_ISC) {
		return cache_read_word(address);
	}

	psx.bus_error = 0;
	UINT32 data = psx.handlers.read_word ? psx.handlers.read_word(address) : 0xffffffff;
	if (psx.bus_error) take_exception(PSX_EXC_DBE);
	return data;
}

static inline UINT32 read_word_masked(UINT32 address, UINT32 mask)
{
	UINT32 aligned = address & ~3;

	if (psx.cp0[PSX_STATUS] & SR_ISC) {
		return cache_read_word(aligned);
	}

	psx.bus_error = 0;
	UINT32 data = (psx.handlers.read_word ? psx.handlers.read_word(aligned) : 0xffffffff) & mask;
	if (psx.bus_error) take_exception(PSX_EXC_DBE);
	return data;
}

static inline UINT32 fetch_word(UINT32 address)
{
	return psx.handlers.read_word ? psx.handlers.read_word(address) : 0xffffffff;
}

static inline void write_byte(UINT32 address, UINT8 data)
{
	if (psx.cp0[PSX_STATUS] & SR_ISC) {
		cache_write_word(address, data << ((address & 3) << 3));
		return;
	}

	psx.bus_error = 0;
	if (psx.handlers.write_byte) psx.handlers.write_byte(address, data);
	if (psx.bus_error) take_exception(PSX_EXC_DBE);
}

static inline void write_half(UINT32 address, UINT16 data)
{
	if (psx.cp0[PSX_STATUS] & SR_ISC) {
		cache_write_word(address, data << ((address & 2) << 3));
		return;
	}

	psx.bus_error = 0;
	if (psx.handlers.write_half) psx.handlers.write_half(address, data);
	if (psx.bus_error) take_exception(PSX_EXC_DBE);
}

static inline void write_word(UINT32 address, UINT32 data)
{
	if (psx.cp0[PSX_STATUS] & SR_ISC) {
		cache_write_word(address, data);
		return;
	}

	psx.bus_error = 0;
	if (psx.handlers.write_word) psx.handlers.write_word(address, data);
	if (psx.bus_error) take_exception(PSX_EXC_DBE);
}

static inline void write_word_masked(UINT32 address, UINT32 data, UINT32 mask)
{
	UINT32 aligned = address & ~3;

	if (psx.cp0[PSX_STATUS] & SR_ISC) {
		cache_write_word(aligned, data);
		return;
	}

	psx.bus_error = 0;

	for (INT32 i = 0; i < 4; i++) {
		UINT32 byte_mask = 0xff << (i << 3);

		if (mask & byte_mask) {
			if (psx.handlers.write_byte) {
				psx.handlers.write_byte(aligned + i, (data >> (i << 3)) & 0xff);
			}

			if (psx.bus_error) {
				take_exception(PSX_EXC_DBE);
				return;
			}
		}
	}
}

static inline void commit_delayed_reg();
static inline void advance_pc();

static inline void set_reg(UINT32 reg, UINT32 data)
{
	if (psx.exception_taken) return;

	advance_pc();

	if (reg) psx.r[reg] = data;
}

static inline void set_delayed_reg(UINT32 reg, UINT32 data)
{
	if (psx.exception_taken) return;

	if (psx.delay_reg == reg) {
		psx.delay_reg = 0;
		psx.delay_value = 0;
	}

	advance_pc();

	psx.delay_reg = reg;
	psx.delay_value = data;
	psx.new_delay = 1;
}

static inline void commit_delayed_reg()
{
	if (psx.delay_reg && psx.delay_reg < 32) {
		psx.r[psx.delay_reg] = psx.delay_value;
		psx.delay_reg = 0;
		psx.delay_value = 0;
	}
}

static inline void update_next_pc()
{
	if (psx.delay_reg == PSX_DELAY_PC) {
		psx.next_pc = psx.delay_value;
	} else {
		psx.next_pc = psx.pc + 4;
	}
}

static inline void advance_pc()
{
	if (psx.exception_taken) return;

	if (psx.delay_reg == PSX_DELAY_PC) {
		psx.pc = psx.delay_value;
		psx.delay_reg = 0;
		psx.delay_value = 0;
	} else if (psx.delay_reg == PSX_DELAY_NOTPC) {
		psx.delay_reg = 0;
		psx.delay_value = 0;
		psx.pc += 4;
	} else {
		commit_delayed_reg();
		psx.pc += 4;
	}

	update_next_pc();
}

static inline void branch(UINT32 address)
{
	advance_pc();
	psx.delay_reg = PSX_DELAY_PC;
	psx.delay_value = address;
	update_next_pc();
}

static inline void conditional_branch(UINT32 op, INT32 take_branch)
{
	advance_pc();

	if (take_branch) {
		psx.delay_reg = PSX_DELAY_PC;
		psx.delay_value = psx.pc + branch_offset(op);
	} else {
		psx.delay_reg = PSX_DELAY_NOTPC;
		psx.delay_value = 0;
	}

	update_next_pc();
}

static inline void unconditional_branch(UINT32 op)
{
	advance_pc();
	psx.delay_reg = PSX_DELAY_PC;
	psx.delay_value = (psx.pc & 0xf0000000) | (op_target(op) << 2);
	update_next_pc();
}

static inline UINT32 get_pipeline_reg(UINT32 reg)
{
	if (psx.delay_reg == reg) return psx.delay_value;

	return psx.r[reg];
}

static void take_exception(INT32 code)
{
	UINT32 cause = ((code & 31) << 2) | (((psx_current_op >> 26) & 3) << 28);
	UINT32 vector = (psx.cp0[PSX_STATUS] & SR_BEV) ? 0xbfc00180 : 0x80000080;

	if (psx.delay_reg == PSX_DELAY_PC) {
		cause |= CAUSE_BD | CAUSE_BT;
		psx.cp0[PSX_TAR] = psx.delay_value;
		psx.cp0[PSX_EPC] = psx_current_pc - 4;
	} else if (psx.delay_reg == PSX_DELAY_NOTPC) {
		cause |= CAUSE_BD;
		psx.cp0[PSX_TAR] = psx_current_pc + 4;
		psx.cp0[PSX_EPC] = psx_current_pc - 4;
	} else {
		commit_delayed_reg();
		psx.cp0[PSX_EPC] = psx_current_pc;
	}

	psx.delay_reg = 0;
	psx.delay_value = 0;
	psx.cp0[PSX_STATUS] = (psx.cp0[PSX_STATUS] & ~0x3f) | ((psx.cp0[PSX_STATUS] << 2) & 0x3f);
	psx.cp0[PSX_CAUSE] = (psx.cp0[PSX_CAUSE] & ~(CAUSE_EXC | CAUSE_BD | CAUSE_BT | CAUSE_CE)) | cause;
	psx.pc = vector;
	psx.next_pc = psx.pc + 4;
	psx.exception_taken = 1;
	psx.bus_error = 0;
}

static inline INT32 is_invalid_cp0_reg(UINT32 reg)
{
	return reg == 0 || reg == 1 || reg == 2 || reg == 4 || reg == 10;
}

static void take_interrupt()
{
	psx_current_pc = psx.pc;
	psx_current_op = fetch_word(psx.pc);
	take_exception(0);
}

static void take_external_interrupt()
{
	UINT32 op = fetch_word(psx.pc);

	// A GTE command already in the execution stage cannot be stopped by an
	// interrupt. Complete it without advancing the PC, then enter the handler.
	if (psx_external_interrupt_gte_completion &&
		(op >> 26) == 0x12 && (op & 0x02000000) && (psx.cp0[PSX_STATUS] & SR_CU2)) {
		psx_gte.docop2(psx.pc, op & 0x01ffffff);
	}

	psx.irq_delay = 0;
	take_interrupt();
}

static inline void check_interrupts()
{
	if ((psx.cp0[PSX_STATUS] & 1) && (psx.cp0[PSX_STATUS] & psx.cp0[PSX_CAUSE] & 0x0000ff00)) {
		if (psx.irq_delay) {
			psx.irq_delay = 0;
			if (psx_external_interrupt_gte_completion) {
				take_external_interrupt();
			} else {
				take_interrupt();
			}
		} else {
			psx.irq_delay = 1;
		}
	} else {
		psx.irq_delay = 0;
	}
}

static void execute_special(UINT32 op)
{
	switch (op & 0x3f)
	{
		case 0x00: set_reg(op_rd(op), psx.r[op_rt(op)] << op_sa(op)); break;
		case 0x04: set_reg(op_rd(op), psx.r[op_rt(op)] << (psx.r[op_rs(op)] & 31)); break;
		case 0x02: set_reg(op_rd(op), psx.r[op_rt(op)] >> op_sa(op)); break;
		case 0x06: set_reg(op_rd(op), psx.r[op_rt(op)] >> (psx.r[op_rs(op)] & 31)); break;
		case 0x03: set_reg(op_rd(op), (INT32)psx.r[op_rt(op)] >> op_sa(op)); break;
		case 0x07: set_reg(op_rd(op), (INT32)psx.r[op_rt(op)] >> (psx.r[op_rs(op)] & 31)); break;
		case 0x08: branch(psx.r[op_rs(op)]); break;
		case 0x09: {
			UINT32 target = psx.r[op_rs(op)];
			UINT32 rd = op_rd(op);
			branch(target);
			UINT32 link = psx.pc + 4;
			if (rd) psx.r[rd] = link;
			break;
		}
		case 0x0c: take_exception(8); break;
		case 0x0d: take_exception(9); break;
		case 0x10: set_reg(op_rd(op), psx.hi); break;
		case 0x11: psx.hi = psx.r[op_rs(op)]; advance_pc(); break;
		case 0x12: set_reg(op_rd(op), psx.lo); break;
		case 0x13: psx.lo = psx.r[op_rs(op)]; advance_pc(); break;
		case 0x18: { INT64 r = (INT64)(INT32)psx.r[op_rs(op)] * (INT64)(INT32)psx.r[op_rt(op)]; psx.lo = (UINT32)r; psx.hi = (UINT32)(r >> 32); advance_pc(); break; }
		case 0x19: { UINT64 r = (UINT64)psx.r[op_rs(op)] * (UINT64)psx.r[op_rt(op)]; psx.lo = (UINT32)r; psx.hi = (UINT32)(r >> 32); advance_pc(); break; }
		case 0x1a:
			if (psx.r[op_rt(op)] == 0) {
				psx.lo = (psx.r[op_rs(op)] & 0x80000000) ? 1 : 0xffffffff;
				psx.hi = psx.r[op_rs(op)];
			} else if (psx.r[op_rs(op)] == 0x80000000 && psx.r[op_rt(op)] == 0xffffffff) {
				psx.lo = 0x80000000;
				psx.hi = 0;
			} else {
				psx.lo = (INT32)psx.r[op_rs(op)] / (INT32)psx.r[op_rt(op)];
				psx.hi = (INT32)psx.r[op_rs(op)] % (INT32)psx.r[op_rt(op)];
			}
			advance_pc();
			break;
		case 0x1b:
			if (psx.r[op_rt(op)] == 0) {
				psx.lo = 0xffffffff;
				psx.hi = psx.r[op_rs(op)];
			} else {
				psx.lo = psx.r[op_rs(op)] / psx.r[op_rt(op)];
				psx.hi = psx.r[op_rs(op)] % psx.r[op_rt(op)];
			}
			advance_pc();
			break;
		case 0x20: {
			UINT32 rs = psx.r[op_rs(op)];
			UINT32 rt = psx.r[op_rt(op)];
			UINT32 result = rs + rt;
			if ((INT32)(~(rs ^ rt) & (rs ^ result)) < 0) {
				take_exception(PSX_EXC_OVF);
			} else {
				set_reg(op_rd(op), result);
			}
			break;
		}
		case 0x21: set_reg(op_rd(op), psx.r[op_rs(op)] + psx.r[op_rt(op)]); break;
		case 0x22: {
			UINT32 rs = psx.r[op_rs(op)];
			UINT32 rt = psx.r[op_rt(op)];
			UINT32 result = rs - rt;
			if ((INT32)((rs ^ rt) & (rs ^ result)) < 0) {
				take_exception(PSX_EXC_OVF);
			} else {
				set_reg(op_rd(op), result);
			}
			break;
		}
		case 0x23: set_reg(op_rd(op), psx.r[op_rs(op)] - psx.r[op_rt(op)]); break;
		case 0x24: set_reg(op_rd(op), psx.r[op_rs(op)] & psx.r[op_rt(op)]); break;
		case 0x25: set_reg(op_rd(op), psx.r[op_rs(op)] | psx.r[op_rt(op)]); break;
		case 0x26: set_reg(op_rd(op), psx.r[op_rs(op)] ^ psx.r[op_rt(op)]); break;
		case 0x27: set_reg(op_rd(op), ~(psx.r[op_rs(op)] | psx.r[op_rt(op)])); break;
		case 0x2a: set_reg(op_rd(op), (INT32)psx.r[op_rs(op)] < (INT32)psx.r[op_rt(op)]); break;
		case 0x2b: set_reg(op_rd(op), psx.r[op_rs(op)] < psx.r[op_rt(op)]); break;
		default: take_exception(PSX_EXC_RI); break;
	}
}

static void execute_regimm(UINT32 op)
{
	INT32 rs = (INT32)psx.r[op_rs(op)];
	INT32 link = (op_rt(op) == 0x10 || op_rt(op) == 0x11);

	switch (op_rt(op))
	{
		case 0x00: conditional_branch(op, rs < 0); break; // BLTZ
		case 0x01: conditional_branch(op, rs >= 0); break; // BGEZ
		case 0x10: conditional_branch(op, rs < 0); break; // BLTZAL
		case 0x11: conditional_branch(op, rs >= 0); break; // BGEZAL
		default: advance_pc(); break;
	}

	if (link) {
		psx.r[31] = psx.pc + 4;
	}
}

static void execute_cop0(UINT32 op)
{
	if ((psx.cp0[PSX_STATUS] & SR_KUC) && (psx.cp0[PSX_STATUS] & SR_CU0) == 0) {
		take_exception(PSX_EXC_CPU);
		return;
	}

	switch ((op >> 21) & 31)
	{
		case 0x00:
			if (is_invalid_cp0_reg(op_rd(op))) {
				take_exception(PSX_EXC_RI);
			} else if (op_rd(op) < 16) {
				set_delayed_reg(op_rt(op), psx.cp0[op_rd(op)]);
			} else {
				advance_pc();
			}
			break;
		case 0x04: {
			UINT32 reg = op_rd(op);
			if (is_invalid_cp0_reg(reg)) {
				take_exception(PSX_EXC_RI);
			} else if (reg < 16) {
				UINT32 old = psx.cp0[reg];
				UINT32 data = (old & ~psx_cp0_write_mask[reg]) | (psx.r[op_rt(op)] & psx_cp0_write_mask[reg]);
				advance_pc();
				psx.cp0[reg] = data;
			} else {
				advance_pc();
			}
			break;
		}
		case 0x08:
		case 0x09:
		case 0x0c:
		case 0x0d: {
			INT32 condition = (op >> 16) & 1;
			conditional_branch(op, condition == 0);
			break;
		}
		case 0x10:
			if ((op & 0x3f) == 0x10) {
				advance_pc();
				psx.cp0[PSX_STATUS] = (psx.cp0[PSX_STATUS] & ~0x0f) | ((psx.cp0[PSX_STATUS] >> 2) & 0x0f);
			} else {
				take_exception(PSX_EXC_RI);
			}
			break;
		default: advance_pc(); break;
	}
}

static void execute_cop2(UINT32 op, UINT32 pc)
{
	if ((psx.cp0[PSX_STATUS] & SR_CU2) == 0) {
		take_exception(PSX_EXC_CPU);
		return;
	}

	switch ((op >> 21) & 31)
	{
		case 0x00: set_delayed_reg(op_rt(op), psx_gte.getcp2dr(pc, op_rd(op))); break; // MFC2
		case 0x02: set_delayed_reg(op_rt(op), psx_gte.getcp2cr(pc, op_rd(op))); break; // CFC2
		case 0x04: psx_gte.setcp2dr(pc, op_rd(op), psx.r[op_rt(op)]); advance_pc(); break; // MTC2
		case 0x06: psx_gte.setcp2cr(pc, op_rd(op), psx.r[op_rt(op)]); advance_pc(); break; // CTC2
		default:
			if ((op >> 25) & 1) {
				psx_gte.docop2(pc, op & 0x01ffffff);
			}
			advance_pc();
			break;
	}
}

static void execute_cop_opcode(UINT32 op, UINT32 current_pc, UINT32 status_mask)
{
	if ((psx.cp0[PSX_STATUS] & status_mask) == 0) {
		take_exception(PSX_EXC_CPU);
		return;
	}

	switch ((op >> 21) & 31)
	{
		case 0x00: // MFC
		case 0x02: // CFC
			set_delayed_reg(op_rt(op), fetch_word(current_pc + 4));
			break;

		case 0x04: // MTC
		case 0x06: // CTC
			advance_pc();
			break;

		case 0x08: // BC
		case 0x09: {
			INT32 condition = (op >> 16) & 1;
			conditional_branch(op, condition == 0);
			break;
		}

		default:
			advance_pc();
			break;
	}
}

static UINT32 load_lwl(UINT32 address, UINT32 old)
{
	UINT32 aligned = address & ~3;

	switch (address & 3)
	{
		case 0: return (old & 0x00ffffff) | (read_word_masked(aligned, 0x000000ff) << 24);
		case 1: return (old & 0x0000ffff) | (read_word_masked(aligned, 0x0000ffff) << 16);
		case 2: return (old & 0x000000ff) | (read_word_masked(aligned, 0x00ffffff) << 8);
		case 3: return read_word(aligned);
	}

	return old;
}

static UINT32 load_lwr(UINT32 address, UINT32 old)
{
	UINT32 aligned = address & ~3;

	switch (address & 3)
	{
		case 0: return read_word(aligned);
		case 1: return (old & 0xff000000) | (read_word_masked(aligned, 0xffffff00) >> 8);
		case 2: return (old & 0xffff0000) | (read_word_masked(aligned, 0xffff0000) >> 16);
		case 3: return (old & 0xffffff00) | (read_word_masked(aligned, 0xff000000) >> 24);
	}

	return old;
}

static inline void take_address_exception(INT32 code, UINT32 address)
{
	psx.cp0[PSX_BADVADDR] = address;
	take_exception(code);
}

static void store_swl(UINT32 address, UINT32 data)
{
	UINT32 aligned = address & ~3;

	switch (address & 3)
	{
		case 0: write_word_masked(aligned, data >> 24, 0x000000ff); break;
		case 1: write_word_masked(aligned, data >> 16, 0x0000ffff); break;
		case 2: write_word_masked(aligned, data >> 8, 0x00ffffff); break;
		case 3: write_word(aligned, data); break;
	}

	advance_pc();
}

static void store_swr(UINT32 address, UINT32 data)
{
	UINT32 aligned = address & ~3;

	switch (address & 3)
	{
		case 0: write_word(aligned, data); break;
		case 1: write_word_masked(address, data << 8, 0xffffff00); break;
		case 2: write_word_masked(address, data << 16, 0xffff0000); break;
		case 3: write_word_masked(address, data << 24, 0xff000000); break;
	}

	advance_pc();
}

static void store_swc0(UINT32 op, UINT32 current_pc)
{
	UINT32 address = psx.r[op_rs(op)] + sext16(op);
	UINT32 read_address;

	if ((psx.cp0[PSX_STATUS] & SR_CU0) == 0) {
		take_exception(PSX_EXC_CPU);
		return;
	}

	if (address & 3) {
		take_address_exception(PSX_EXC_ADES, address);
		return;
	}

	if (psx.delay_reg == PSX_DELAY_PC) {
		switch (psx.delay_value & 0x0c)
		{
			case 0x0c:
				read_address = psx.delay_value;
				break;

			default:
				read_address = psx.delay_value + 0x04;
				break;
		}
	} else {
		switch (current_pc & 0x0c)
		{
			case 0x00:
			case 0x0c:
				read_address = current_pc + 0x08;
				break;

			default:
				read_address = current_pc | 0x0c;
				break;
		}
	}

	write_word(address, fetch_word(read_address));
	advance_pc();
}

static void load_lwc2(UINT32 op, UINT32 current_pc)
{
	UINT32 address = psx.r[op_rs(op)] + sext16(op);

	if ((psx.cp0[PSX_STATUS] & SR_CU2) == 0) {
		take_exception(PSX_EXC_CPU);
		return;
	}

	if (address & 3) {
		take_address_exception(PSX_EXC_ADEL, address);
		return;
	}

	psx_gte.setcp2dr(current_pc, op_rt(op), read_word(address));
	if (psx.bus_error == 0) {
		advance_pc();
	}
}

static void store_swc2(UINT32 op, UINT32 current_pc)
{
	UINT32 address = psx.r[op_rs(op)] + sext16(op);

	if ((psx.cp0[PSX_STATUS] & SR_CU2) == 0) {
		take_exception(PSX_EXC_CPU);
		return;
	}

	if (address & 3) {
		take_address_exception(PSX_EXC_ADES, address);
		return;
	}

	UINT32 data = psx_gte.getcp2dr(current_pc, op_rt(op));
	write_word(address, data);
	if (psx.bus_error == 0) {
		advance_pc();
	}
}

static void load_lwc_disabled(UINT32 status_mask, UINT32 op)
{
	if ((psx.cp0[PSX_STATUS] & status_mask) == 0) {
		take_exception(PSX_EXC_CPU);
		return;
	}

	UINT32 address = psx.r[op_rs(op)] + sext16(op);
	if (address & 3) {
		take_address_exception(PSX_EXC_ADEL, address);
		return;
	}

	read_word(address);
	if (psx.bus_error == 0) {
		advance_pc();
	}
}

static void store_swc_disabled(UINT32 status_mask, UINT32 op, UINT32 current_pc)
{
	if ((psx.cp0[PSX_STATUS] & status_mask) == 0) {
		take_exception(PSX_EXC_CPU);
		return;
	}

	UINT32 address = psx.r[op_rs(op)] + sext16(op);
	if (address & 3) {
		take_address_exception(PSX_EXC_ADES, address);
		return;
	}

	write_word(address, fetch_word(current_pc + 4));
	if (psx.bus_error == 0) {
		advance_pc();
	}
}

static void execute_one()
{
	UINT32 current_pc = psx.pc;
	if (psx_hook) psx_hook(current_pc);
	UINT32 op = fetch_word(current_pc);
	psx_current_pc = current_pc;
	psx_current_op = op;
	psx.exception_taken = 0;

	psx.bus_error = 0;
	psx.new_delay = 0;

	switch (op >> 26)
	{
		case 0x00: execute_special(op); break;
		case 0x01: execute_regimm(op); break;
		case 0x02: unconditional_branch(op); break;
		case 0x03: {
			unconditional_branch(op);
			psx.r[31] = psx.pc + 4;
			break;
		}
		case 0x04: conditional_branch(op, psx.r[op_rs(op)] == psx.r[op_rt(op)]); break;
		case 0x05: conditional_branch(op, psx.r[op_rs(op)] != psx.r[op_rt(op)]); break;
		case 0x06: conditional_branch(op, (INT32)psx.r[op_rs(op)] < 0 || psx.r[op_rs(op)] == psx.r[op_rt(op)]); break;
		case 0x07: conditional_branch(op, (INT32)psx.r[op_rs(op)] >= 0 && psx.r[op_rs(op)] != psx.r[op_rt(op)]); break;
		case 0x08: {
			UINT32 rs = psx.r[op_rs(op)];
			UINT32 imm = sext16(op);
			UINT32 result = rs + imm;
			if ((INT32)(~(rs ^ imm) & (rs ^ result)) < 0) {
				take_exception(PSX_EXC_OVF);
			} else {
				set_reg(op_rt(op), result);
			}
			break;
		}
		case 0x09: set_reg(op_rt(op), psx.r[op_rs(op)] + sext16(op)); break;
		case 0x0a: set_reg(op_rt(op), (INT32)psx.r[op_rs(op)] < (INT32)sext16(op)); break;
		case 0x0b: set_reg(op_rt(op), psx.r[op_rs(op)] < (UINT32)sext16(op)); break;
		case 0x0c: set_reg(op_rt(op), psx.r[op_rs(op)] & op_imm(op)); break;
		case 0x0d: set_reg(op_rt(op), psx.r[op_rs(op)] | op_imm(op)); break;
		case 0x0e: set_reg(op_rt(op), psx.r[op_rs(op)] ^ op_imm(op)); break;
		case 0x0f: set_reg(op_rt(op), op_imm(op) << 16); break;
		case 0x10: execute_cop0(op); break;
		case 0x11: execute_cop_opcode(op, current_pc, SR_CU1); break;
		case 0x12: execute_cop2(op, current_pc); break;
		case 0x13: execute_cop_opcode(op, current_pc, SR_CU3); break;
		case 0x22: set_delayed_reg(op_rt(op), load_lwl(psx.r[op_rs(op)] + sext16(op), get_pipeline_reg(op_rt(op)))); break;
		case 0x20: set_delayed_reg(op_rt(op), (INT8)read_byte(psx.r[op_rs(op)] + sext16(op))); break;
		case 0x21: {
			UINT32 address = psx.r[op_rs(op)] + sext16(op);
			if (address & 1) {
				take_address_exception(PSX_EXC_ADEL, address);
			} else {
				set_delayed_reg(op_rt(op), (INT16)read_half(address));
			}
			break;
		}
		case 0x23: {
			UINT32 address = psx.r[op_rs(op)] + sext16(op);
			if (address & 3) {
				take_address_exception(PSX_EXC_ADEL, address);
			} else {
				UINT32 data = read_word(address);
				set_delayed_reg(op_rt(op), data);
			}
			break;
		}
		case 0x26: set_delayed_reg(op_rt(op), load_lwr(psx.r[op_rs(op)] + sext16(op), get_pipeline_reg(op_rt(op)))); break;
		case 0x24: set_delayed_reg(op_rt(op), read_byte(psx.r[op_rs(op)] + sext16(op))); break;
		case 0x25: {
			UINT32 address = psx.r[op_rs(op)] + sext16(op);
			if (address & 1) {
				take_address_exception(PSX_EXC_ADEL, address);
			} else {
				set_delayed_reg(op_rt(op), read_half(address));
			}
			break;
		}
		case 0x2a: store_swl(psx.r[op_rs(op)] + sext16(op), psx.r[op_rt(op)]); break;
		case 0x28: {
			UINT32 address = psx.r[op_rs(op)] + sext16(op);
			UINT32 shift = (address & 3) << 3;
			write_word_masked(address, (psx.r[op_rt(op)] & 0xff) << shift, 0xff << shift);
			advance_pc();
			break;
		}
		case 0x29: {
			UINT32 address = psx.r[op_rs(op)] + sext16(op);
			if (address & 1) {
				take_address_exception(PSX_EXC_ADES, address);
				break;
			}
			UINT32 shift = (address & 2) << 3;
			write_word_masked(address, (psx.r[op_rt(op)] & 0xffff) << shift, 0xffff << shift);
			advance_pc();
			break;
		}
		case 0x2b: {
			UINT32 address = psx.r[op_rs(op)] + sext16(op);
			if (address & 3) {
				take_address_exception(PSX_EXC_ADES, address);
			} else {
				write_word(address, psx.r[op_rt(op)]);
				advance_pc();
			}
			break;
		}
		case 0x2e: store_swr(psx.r[op_rs(op)] + sext16(op), psx.r[op_rt(op)]); break;
		case 0x30: load_lwc_disabled(SR_CU0, op); break;
		case 0x31: load_lwc_disabled(SR_CU1, op); break;
		case 0x32: load_lwc2(op, current_pc); break;
		case 0x33: load_lwc_disabled(SR_CU3, op); break;
		case 0x38: store_swc0(op, current_pc); break;
		case 0x39: store_swc_disabled(SR_CU1, op, current_pc); break;
		case 0x3a: store_swc2(op, current_pc); break;
		case 0x3b: store_swc_disabled(SR_CU3, op, current_pc); break;
		default: take_exception(PSX_EXC_RI); break;
	}

	if (psx.exception_taken) {
		psx.r[0] = 0;
		psx.cycles--;
		return;
	}

	psx.r[0] = 0;
	check_interrupts();
	psx.cycles--;
}

void psx_core_init(psx_core_handlers *handlers)
{
	memset(&psx, 0, sizeof(psx));
	psx_external_interrupt_gte_completion = 0;
	if (handlers) psx.handlers = *handlers;
	psx_core_reset();
}

void psx_core_exit()
{
	memset(&psx, 0, sizeof(psx));
}

void psx_core_reset()
{
	memset(psx.r, 0, sizeof(psx.r));
	memset(psx.cp0, 0, sizeof(psx.cp0));
	memset(psx_gte.m_cp2dr, 0, sizeof(psx_gte.m_cp2dr));
	memset(psx_gte.m_cp2cr, 0, sizeof(psx_gte.m_cp2cr));
	memset(psx.irq_state, 0, sizeof(psx.irq_state));
	psx.irq_delay = 0;
	psx.pc = 0xbfc00000;
	psx.next_pc = psx.pc + 4;
	psx.hi = 0;
	psx.lo = 0;
	psx.delay_reg = 0;
	psx.delay_value = 0;
	psx.commit_reg = 0;
	psx.commit_value = 0;
	psx.new_delay = 0;
	psx.biu = 0;
	psx.cp0[PSX_STATUS] = SR_BEV;
	psx.cp0[15] = 0x00000002;
	psx.cp0[7] = 0x00000000;
	psx.cp0[11] = 0xffffffff;
	psx.cp0[9] = 0xffffffff;
	memset(psx.icache_tag, 0, sizeof(psx.icache_tag));
	memset(psx.icache, 0, sizeof(psx.icache));
	memset(psx.dcache, 0, sizeof(psx.dcache));
	psx.cycles = 0;
	psx_current_pc = 0;
	psx_current_op = 0;
}

INT32 psx_core_run(INT32 cycles)
{
	psx.cycles = cycles;

	while (psx.cycles > 0) {
		execute_one();
	}

	return cycles - psx.cycles;
}

INT32 psx_core_get_remaining_cycles()
{
	return psx.cycles;
}

void psx_core_set_irq_line(INT32 line, INT32 state)
{
	if (line < 0 || line >= 8) return;

	INT32 new_state = state ? 1 : 0;
	if (psx.irq_state[line] == new_state) return;

	psx.irq_state[line] = new_state;

	if (new_state) {
		psx.cp0[PSX_CAUSE] |= 0x400 << line;
	} else {
		psx.cp0[PSX_CAUSE] &= ~(0x400 << line);
	}

	check_interrupts();
}

UINT32 psx_core_get_pc()
{
	return psx.pc;
}

UINT32 psx_core_get_next_pc()
{
	return psx.next_pc;
}

UINT32 psx_core_get_opcode()
{
	return fetch_word(psx.pc);
}

UINT32 psx_core_get_reg(INT32 reg)
{
	return (reg >= 0 && reg < 32) ? psx.r[reg] : 0;
}

void psx_core_set_reg(INT32 reg, UINT32 data)
{
	if (reg > 0 && reg < 32) psx.r[reg] = data;
}

UINT32 psx_core_get_cp0(INT32 reg)
{
	return (reg >= 0 && reg < 32) ? psx.cp0[reg] : 0;
}

UINT32 psx_core_get_biu()
{
	return psx.biu;
}

UINT8 *psx_core_get_dcache()
{
	return (UINT8*)psx.dcache;
}

void psx_core_set_biu(UINT32 data)
{
	psx.biu = data;
}

void psx_core_set_instruction_hook(psx_instruction_hook hook)
{
	psx_hook = hook;
}

void psx_core_set_external_interrupt_gte_completion(INT32 enable)
{
	psx_external_interrupt_gte_completion = enable ? 1 : 0;
}

void psx_core_bus_error()
{
	psx.bus_error = 1;
}

void psx_core_scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(psx.pc);
		SCAN_VAR(psx.next_pc);
		SCAN_VAR(psx.r);
		SCAN_VAR(psx.cp0);
		SCAN_VAR(psx_gte.m_cp2dr);
		SCAN_VAR(psx_gte.m_cp2cr);
		SCAN_VAR(psx.hi);
		SCAN_VAR(psx.lo);
		SCAN_VAR(psx.delay_reg);
		SCAN_VAR(psx.delay_value);
		SCAN_VAR(psx.commit_reg);
		SCAN_VAR(psx.commit_value);
		SCAN_VAR(psx.new_delay);
		SCAN_VAR(psx.bus_error);
		SCAN_VAR(psx.biu);
		SCAN_VAR(psx.icache_tag);
		SCAN_VAR(psx.icache);
		SCAN_VAR(psx.dcache);
		SCAN_VAR(psx.irq_state);
		SCAN_VAR(psx.irq_delay);
	}
}
