#ifndef FBNEO_PSX_H
#define FBNEO_PSX_H

#include "burnint.h"

typedef UINT8  (*psx_read_byte_handler)(UINT32 address);
typedef UINT16 (*psx_read_half_handler)(UINT32 address);
typedef UINT32 (*psx_read_word_handler)(UINT32 address);
typedef void   (*psx_write_byte_handler)(UINT32 address, UINT8 data);
typedef void   (*psx_write_half_handler)(UINT32 address, UINT16 data);
typedef void   (*psx_write_word_handler)(UINT32 address, UINT32 data);
typedef void   (*psx_instruction_hook)(UINT32 pc);

struct psx_core_handlers {
	psx_read_byte_handler read_byte;
	psx_read_half_handler read_half;
	psx_read_word_handler read_word;
	psx_write_byte_handler write_byte;
	psx_write_half_handler write_half;
	psx_write_word_handler write_word;
};

void psx_core_init(psx_core_handlers *handlers);
void psx_core_exit();
void psx_core_reset();
INT32 psx_core_run(INT32 cycles);
INT32 psx_core_get_remaining_cycles();
void psx_core_set_irq_line(INT32 line, INT32 state);
void psx_core_bus_error();
UINT32 psx_core_get_pc();
UINT32 psx_core_get_next_pc();
UINT32 psx_core_get_opcode();
UINT32 psx_core_get_reg(INT32 reg);
void psx_core_set_reg(INT32 reg, UINT32 data);
UINT32 psx_core_get_cp0(INT32 reg);
UINT32 psx_core_get_biu();
UINT8 *psx_core_get_dcache();
void psx_core_set_biu(UINT32 data);
void psx_core_set_instruction_hook(psx_instruction_hook hook);
void psx_core_set_external_interrupt_gte_completion(INT32 enable);
void psx_core_scan(INT32 nAction);

#endif
