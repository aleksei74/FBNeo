#include "psx_intf.h"
#include "psx/psx.h"

#define PSX_ADDRESS_BITS	32
#define PSX_PAGE_SHIFT		12
#define PSX_PAGE_SIZE		(1 << PSX_PAGE_SHIFT)
#define PSX_PAGE_MASK		(PSX_PAGE_SIZE - 1)
#define PSX_PAGE_COUNT		(1 << (PSX_ADDRESS_BITS - PSX_PAGE_SHIFT))
#define PSX_PAGE_WRITE		PSX_PAGE_COUNT
#define PSX_MAX_HANDLER		16
#define PSX_PAGE(address)	(((address) >> PSX_PAGE_SHIFT) & (PSX_PAGE_COUNT - 1))

struct psx_memory_map {
	UINT8 *map[PSX_PAGE_COUNT * 2];
	pPsxReadByteHandler read_byte[PSX_MAX_HANDLER];
	pPsxReadHalfHandler read_half[PSX_MAX_HANDLER];
	pPsxReadWordHandler read_word[PSX_MAX_HANDLER];
	pPsxWriteByteHandler write_byte[PSX_MAX_HANDLER];
	pPsxWriteHalfHandler write_half[PSX_MAX_HANDLER];
	pPsxWriteWordHandler write_word[PSX_MAX_HANDLER];
};

static psx_memory_map *PsxMap;

static UINT8 default_read_byte(UINT32) { return 0xff; }
static UINT16 default_read_half(UINT32) { return 0xffff; }
static UINT32 default_read_word(UINT32) { return 0xffffffff; }
static void default_write_byte(UINT32, UINT8) { }
static void default_write_half(UINT32, UINT16) { }
static void default_write_word(UINT32, UINT32) { }

static inline UINT32 translate_address(UINT32 address)
{
	if ((address & 0xe0000000) == 0x80000000 || (address & 0xe0000000) == 0xa0000000) {
		return address & 0x1fffffff;
	}

	return address;
}

static void reset_map()
{
	memset(PsxMap->map, 0, sizeof(PsxMap->map));

	for (INT32 i = 0; i < PSX_MAX_HANDLER; i++) {
		PsxMap->read_byte[i] = default_read_byte;
		PsxMap->read_half[i] = default_read_half;
		PsxMap->read_word[i] = default_read_word;
		PsxMap->write_byte[i] = default_write_byte;
		PsxMap->write_half[i] = default_write_half;
		PsxMap->write_word[i] = default_write_word;
	}
}

static inline UINT8 read_byte(UINT32 address)
{
	UINT32 mapped = translate_address(address);
	UINT8 *ptr = PsxMap->map[PSX_PAGE(mapped)];
	UINT8 data;

	if ((uintptr_t)ptr < PSX_MAX_HANDLER) {
		return PsxMap->read_byte[(uintptr_t)ptr](mapped);
	}

	data = ptr[mapped & PSX_PAGE_MASK];

	return data;
}

static inline UINT16 read_half(UINT32 address)
{
	UINT32 mapped = translate_address(address);
	UINT8 *ptr = PsxMap->map[PSX_PAGE(mapped)];

	if ((uintptr_t)ptr < PSX_MAX_HANDLER) {
		return PsxMap->read_half[(uintptr_t)ptr](mapped);
	}

	ptr += mapped & PSX_PAGE_MASK;
	UINT16 data = ptr[0] | (ptr[1] << 8);

	return data;
}

static inline UINT32 read_word(UINT32 address)
{
	UINT32 mapped = translate_address(address);
	UINT8 *ptr = PsxMap->map[PSX_PAGE(mapped)];

	if ((uintptr_t)ptr < PSX_MAX_HANDLER) {
		return PsxMap->read_word[(uintptr_t)ptr](mapped);
	}

	ptr += mapped & PSX_PAGE_MASK;
	UINT32 data = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);

	return data;
}

static inline void write_byte(UINT32 address, UINT8 data)
{
	UINT32 mapped = translate_address(address);
	UINT8 *ptr = PsxMap->map[PSX_PAGE_WRITE + PSX_PAGE(mapped)];

	if ((uintptr_t)ptr < PSX_MAX_HANDLER) {
		PsxMap->write_byte[(uintptr_t)ptr](mapped, data);
		return;
	}

	ptr[mapped & PSX_PAGE_MASK] = data;
}

static inline void write_half(UINT32 address, UINT16 data)
{
	UINT32 mapped = translate_address(address);
	UINT8 *ptr = PsxMap->map[PSX_PAGE_WRITE + PSX_PAGE(mapped)];

	if ((uintptr_t)ptr < PSX_MAX_HANDLER) {
		PsxMap->write_half[(uintptr_t)ptr](mapped, data);
		return;
	}

	ptr += mapped & PSX_PAGE_MASK;
	ptr[0] = data & 0xff;
	ptr[1] = data >> 8;
}

static inline void write_word(UINT32 address, UINT32 data)
{
	UINT32 mapped = translate_address(address);
	UINT8 *ptr = PsxMap->map[PSX_PAGE_WRITE + PSX_PAGE(mapped)];

	if ((uintptr_t)ptr < PSX_MAX_HANDLER) {
		PsxMap->write_word[(uintptr_t)ptr](mapped, data);
		return;
	}

	ptr += mapped & PSX_PAGE_MASK;
	ptr[0] = data & 0xff;
	ptr[1] = (data >> 8) & 0xff;
	ptr[2] = (data >> 16) & 0xff;
	ptr[3] = data >> 24;
}

INT32 PsxInit()
{
	PsxMap = (psx_memory_map*)BurnMalloc(sizeof(psx_memory_map));
	if (PsxMap == NULL) return 1;

	reset_map();

	psx_core_handlers handlers;
	handlers.read_byte = read_byte;
	handlers.read_half = read_half;
	handlers.read_word = read_word;
	handlers.write_byte = write_byte;
	handlers.write_half = write_half;
	handlers.write_word = write_word;

	psx_core_init(&handlers);

	return 0;
}

void PsxExit()
{
	psx_core_exit();
	BurnFree(PsxMap);
}

void PsxReset()
{
	psx_core_reset();
}

INT32 PsxRun(INT32 cycles)
{
	return psx_core_run(cycles);
}

INT32 PsxGetRemainingCycles()
{
	return psx_core_get_remaining_cycles();
}

UINT32 PsxGetPC()
{
	return psx_core_get_pc();
}

UINT32 PsxGetNextPC()
{
	return psx_core_get_next_pc();
}

UINT32 PsxGetOpcode()
{
	return psx_core_get_opcode();
}

UINT32 PsxGetReg(INT32 reg)
{
	return psx_core_get_reg(reg);
}

void PsxSetReg(INT32 reg, UINT32 data)
{
	psx_core_set_reg(reg, data);
}

UINT32 PsxGetCP0(INT32 reg)
{
	return psx_core_get_cp0(reg);
}

UINT32 PsxGetBIU()
{
	return psx_core_get_biu();
}

UINT8 *PsxGetDCache()
{
	return psx_core_get_dcache();
}

void PsxSetBIU(UINT32 data)
{
	psx_core_set_biu(data);
}

void PsxSetInstructionHook(pPsxInstructionHook hook)
{
	psx_core_set_instruction_hook(hook);
}

void PsxSetExternalInterruptGteCompletion(INT32 enable)
{
	psx_core_set_external_interrupt_gte_completion(enable);
}

void PsxBusError()
{
	psx_core_bus_error();
}

void PsxSetIRQLine(INT32 line, INT32 state)
{
	psx_core_set_irq_line(line, state);
}

void PsxScan(INT32 nAction)
{
	psx_core_scan(nAction);
}

INT32 PsxMapMemory(UINT8 *memory, UINT32 start, UINT32 end, INT32 type)
{
	UINT32 first = PSX_PAGE(start);
	UINT32 last = PSX_PAGE(end);

	for (UINT32 page = first; page <= last; page++) {
		UINT8 *ptr = memory + ((page - first) << PSX_PAGE_SHIFT);

		if (type & MAP_READ) {
			PsxMap->map[page] = ptr;
		}

		if (type & MAP_WRITE) {
			PsxMap->map[PSX_PAGE_WRITE + page] = ptr;
		}
	}

	return 0;
}

INT32 PsxMapHandler(uintptr_t handler, UINT32 start, UINT32 end, INT32 type)
{
	if (handler >= PSX_MAX_HANDLER) return 1;

	UINT32 first = PSX_PAGE(start);
	UINT32 last = PSX_PAGE(end);

	for (UINT32 page = first; page <= last; page++) {
		if (type & MAP_READ) {
			PsxMap->map[page] = (UINT8*)handler;
		}

		if (type & MAP_WRITE) {
			PsxMap->map[PSX_PAGE_WRITE + page] = (UINT8*)handler;
		}
	}

	return 0;
}

INT32 PsxSetReadByteHandler(INT32 i, pPsxReadByteHandler handler)
{
	if (i < 0 || i >= PSX_MAX_HANDLER) return 1;
	PsxMap->read_byte[i] = handler ? handler : default_read_byte;
	return 0;
}

INT32 PsxSetReadHalfHandler(INT32 i, pPsxReadHalfHandler handler)
{
	if (i < 0 || i >= PSX_MAX_HANDLER) return 1;
	PsxMap->read_half[i] = handler ? handler : default_read_half;
	return 0;
}

INT32 PsxSetReadWordHandler(INT32 i, pPsxReadWordHandler handler)
{
	if (i < 0 || i >= PSX_MAX_HANDLER) return 1;
	PsxMap->read_word[i] = handler ? handler : default_read_word;
	return 0;
}

INT32 PsxSetWriteByteHandler(INT32 i, pPsxWriteByteHandler handler)
{
	if (i < 0 || i >= PSX_MAX_HANDLER) return 1;
	PsxMap->write_byte[i] = handler ? handler : default_write_byte;
	return 0;
}

INT32 PsxSetWriteHalfHandler(INT32 i, pPsxWriteHalfHandler handler)
{
	if (i < 0 || i >= PSX_MAX_HANDLER) return 1;
	PsxMap->write_half[i] = handler ? handler : default_write_half;
	return 0;
}

INT32 PsxSetWriteWordHandler(INT32 i, pPsxWriteWordHandler handler)
{
	if (i < 0 || i >= PSX_MAX_HANDLER) return 1;
	PsxMap->write_word[i] = handler ? handler : default_write_word;
	return 0;
}
