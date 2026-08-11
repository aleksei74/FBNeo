// FinalBurn Neo Tatsumi Big Fight driver
// Based on the MAME driver by Bryan McPhail and Angelo Salese

#include "burnint.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "tiles_generic.h"
#include "cxd1095.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;

static UINT8 *Drv68KROM[2];
static UINT8 *DrvZ80ROM;
static UINT8 *DrvSpriteROM[2];
static UINT8 *DrvTileROM;
static UINT8 *DrvTileCLUT;
static UINT8 *DrvSndROM;
static UINT8 *DrvSpriteGfx[2];
static UINT8 *DrvTileGfx;
static UINT8 *DrvTileOpaque;
static UINT32 *DrvPalette;

static UINT8 *Drv68KRAM[2];
static UINT8 *DrvAuxRAM;
static UINT8 *DrvVidRAM[2];
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprCtrl;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvPriRAM;

static UINT8 DrvJoy[5][8];
static UINT8 DrvInputs[5];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT8 DrvRecalc;

static UINT16 video_config[4];
static UINT16 tile_bank;
static UINT16 mixing_control;
static UINT8 soundlatch;
static UINT8 sound_nmi_pending;
static UINT8 control_word;
static UINT8 last_control;
static INT32 nExtraCycles[2];

static struct BurnInputInfo BigfightInputList[] = {
	{"P1 Coin",        BIT_DIGITAL, DrvJoy[0] + 0, "p1 coin"},
	{"P1 Start",       BIT_DIGITAL, DrvJoy[1] + 0, "p1 start"},
	{"P1 Up",          BIT_DIGITAL, DrvJoy[1] + 7, "p1 up"},
	{"P1 Down",        BIT_DIGITAL, DrvJoy[1] + 6, "p1 down"},
	{"P1 Left",        BIT_DIGITAL, DrvJoy[1] + 5, "p1 left"},
	{"P1 Right",       BIT_DIGITAL, DrvJoy[1] + 4, "p1 right"},
	{"P1 Button 1",    BIT_DIGITAL, DrvJoy[1] + 3, "p1 fire 1"},
	{"P1 Button 2",    BIT_DIGITAL, DrvJoy[1] + 2, "p1 fire 2"},
	{"P1 Button 3",    BIT_DIGITAL, DrvJoy[1] + 1, "p1 fire 3"},

	{"P2 Coin",        BIT_DIGITAL, DrvJoy[0] + 1, "p2 coin"},
	{"P2 Start",       BIT_DIGITAL, DrvJoy[2] + 0, "p2 start"},
	{"P2 Up",          BIT_DIGITAL, DrvJoy[2] + 7, "p2 up"},
	{"P2 Down",        BIT_DIGITAL, DrvJoy[2] + 6, "p2 down"},
	{"P2 Left",        BIT_DIGITAL, DrvJoy[2] + 5, "p2 left"},
	{"P2 Right",       BIT_DIGITAL, DrvJoy[2] + 4, "p2 right"},
	{"P2 Button 1",    BIT_DIGITAL, DrvJoy[2] + 3, "p2 fire 1"},
	{"P2 Button 2",    BIT_DIGITAL, DrvJoy[2] + 2, "p2 fire 2"},
	{"P2 Button 3",    BIT_DIGITAL, DrvJoy[2] + 1, "p2 fire 3"},

	{"P3 Coin",        BIT_DIGITAL, DrvJoy[0] + 2, "p3 coin"},
	{"P3 Start",       BIT_DIGITAL, DrvJoy[3] + 0, "p3 start"},
	{"P3 Up",          BIT_DIGITAL, DrvJoy[3] + 7, "p3 up"},
	{"P3 Down",        BIT_DIGITAL, DrvJoy[3] + 6, "p3 down"},
	{"P3 Left",        BIT_DIGITAL, DrvJoy[3] + 5, "p3 left"},
	{"P3 Right",       BIT_DIGITAL, DrvJoy[3] + 4, "p3 right"},
	{"P3 Button 1",    BIT_DIGITAL, DrvJoy[3] + 3, "p3 fire 1"},
	{"P3 Button 2",    BIT_DIGITAL, DrvJoy[3] + 2, "p3 fire 2"},
	{"P3 Button 3",    BIT_DIGITAL, DrvJoy[3] + 1, "p3 fire 3"},

	{"P4 Coin",        BIT_DIGITAL, DrvJoy[0] + 3, "p4 coin"},
	{"P4 Start",       BIT_DIGITAL, DrvJoy[4] + 0, "p4 start"},
	{"P4 Up",          BIT_DIGITAL, DrvJoy[4] + 7, "p4 up"},
	{"P4 Down",        BIT_DIGITAL, DrvJoy[4] + 6, "p4 down"},
	{"P4 Left",        BIT_DIGITAL, DrvJoy[4] + 5, "p4 left"},
	{"P4 Right",       BIT_DIGITAL, DrvJoy[4] + 4, "p4 right"},
	{"P4 Button 1",    BIT_DIGITAL, DrvJoy[4] + 3, "p4 fire 1"},
	{"P4 Button 2",    BIT_DIGITAL, DrvJoy[4] + 2, "p4 fire 2"},
	{"P4 Button 3",    BIT_DIGITAL, DrvJoy[4] + 1, "p4 fire 3"},

	{"Reset",          BIT_DIGITAL, &DrvReset,     "reset"},
	{"Service",        BIT_DIGITAL, DrvJoy[0] + 4, "service"},
	{"Dip A",          BIT_DIPSWITCH, DrvDips + 0, "dip"},
	{"Dip B",          BIT_DIPSWITCH, DrvDips + 1, "dip"},
	{"Dip C",          BIT_DIPSWITCH, DrvDips + 2, "dip"},
};

STDINPUTINFO(Bigfight)

static struct BurnDIPInfo BigfightDIPList[] = {
	{0x26, 0xff, 0xff, 0xff, NULL},
	{0x27, 0xff, 0xff, 0x7f, NULL},
	{0x28, 0xff, 0xff, 0x0d, NULL},

	{0,    0xfe, 0,    4,    "Lives"},
	{0x27, 0x01, 0x03, 0x00, "1"},
	{0x27, 0x01, 0x03, 0x03, "2"},
	{0x27, 0x01, 0x03, 0x02, "3"},
	{0x27, 0x01, 0x03, 0x01, "4"},

	{0,    0xfe, 0,    2,    "Continue Coin"},
	{0x27, 0x01, 0x08, 0x08, "Off"},
	{0x27, 0x01, 0x08, 0x00, "On"},

	{0,    0xfe, 0,    2,    "Extend"},
	{0x27, 0x01, 0x10, 0x10, "100000"},
	{0x27, 0x01, 0x10, 0x00, "None"},

	{0,    0xfe, 0,    4,    "Difficulty"},
	{0x27, 0x01, 0x60, 0x40, "Easy"},
	{0x27, 0x01, 0x60, 0x60, "Normal"},
	{0x27, 0x01, 0x60, 0x20, "Hard"},
	{0x27, 0x01, 0x60, 0x00, "Very Hard"},

	{0,    0xfe, 0,    2,    "Demo Sounds"},
	{0x27, 0x01, 0x80, 0x80, "Off"},
	{0x27, 0x01, 0x80, 0x00, "On"},

	{0,    0xfe, 0,    2,    "Service Mode"},
	{0x28, 0x01, 0x04, 0x04, "Off"},
	{0x28, 0x01, 0x04, 0x00, "On"},

	{0,    0xfe, 0,    2,    "Hardware Test Mode"},
	{0x28, 0x01, 0x08, 0x08, "Off"},
	{0x28, 0x01, 0x08, 0x00, "On"},
};

STDDIPINFO(Bigfight)

static UINT8 io0_read(UINT8 offset)
{
	switch (offset) {
		case 1: return DrvInputs[0];
		case 2: return DrvInputs[1];
		case 3: return DrvInputs[2];
		case 4: return DrvDips[2];
	}
	return 0xff;
}

static UINT8 io1_read(UINT8 offset)
{
	switch (offset) {
		case 0: return DrvDips[0];
		case 1: return DrvDips[1];
		case 2: return DrvInputs[3];
		case 3: return DrvInputs[4];
	}
	return 0xff;
}

static void io1_write(UINT8 offset, UINT8 data)
{
	if (offset != 4) return;

	control_word = data;
	if ((control_word ^ last_control) & 4) {
		SekSetHALT(1, (control_word & 4) ? 1 : 0);
	}
	last_control = control_word;
}

static void __fastcall bigfight_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffff8) == 0x0a2000) {
		video_config[(address >> 1) & 3] = data;
		return;
	}

	switch (address & ~1) {
		case 0x0a4000:
			tile_bank = data;
		return;

		case 0x0a6000:
			mixing_control = data;
		return;

		case 0x0b8000:
			soundlatch = data >> 8;
			sound_nmi_pending = 1;
			return;
	}

	if ((address & 0xfffff0) == 0x0b9000) {
		cxd1095_write(0, (address >> 1) & 7, data & 0xff);
		return;
	}
	if ((address & 0xfffff0) == 0x0ba000) {
		cxd1095_write(1, (address >> 1) & 7, data & 0xff);
		return;
	}
}

static void __fastcall bigfight_write_byte(UINT32 address, UINT8 data)
{
	if ((address & ~1) == 0x0b8000 && (address & 1) == 0) {
		soundlatch = data;
		sound_nmi_pending = 1;
		return;
	}

	if ((address & 0xfffff1) == 0x0b9001) {
		cxd1095_write(0, (address >> 1) & 7, data);
		return;
	}
	if ((address & 0xfffff1) == 0x0ba001) {
		cxd1095_write(1, (address >> 1) & 7, data);
		return;
	}
}

static UINT16 __fastcall bigfight_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0x0b9000) return cxd1095_read(0, (address >> 1) & 7);
	if ((address & 0xfffff0) == 0x0ba000) return cxd1095_read(1, (address >> 1) & 7);
	return 0xffff;
}

static UINT8 __fastcall bigfight_read_byte(UINT32 address)
{
	if ((address & 0xfffff1) == 0x0b9001) return cxd1095_read(0, (address >> 1) & 7);
	if ((address & 0xfffff1) == 0x0ba001) return cxd1095_read(1, (address >> 1) & 7);
	return 0xff;
}

static void __fastcall sound_write(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0xfff0: BurnYM2151SelectRegister(data); return;
		case 0xfff1: BurnYM2151WriteRegister(data); return;
		case 0xfff4: MSM6295Write(0, data); return;
	}

	if (address >= 0xff00 && address <= 0xffef) {
		DrvZ80RAM[address - 0xe000] = data;
	}
}

static UINT8 __fastcall sound_read(UINT16 address)
{
	switch (address) {
		case 0xfff0:
		case 0xfff1: {
			UINT8 status = BurnYM2151Read();
			// The timer does not advance between these back-to-back self-test
			// accesses in FBNeo's buffered YM2151 implementation.
			if (ZetGetPrevPC(-1) == 0x1c63) status = 0x80;
			return status;
		}
		case 0xfff4: return (MSM6295Read(0) ^ 0x0f) & 0x0f;
		case 0xfffc: return soundlatch;
	}

	if (address >= 0xff00 && address <= 0xffef) {
		return DrvZ80RAM[address - 0xe000];
	}

	return 0xff;
}

static void ym2151_irq_handler(INT32 state)
{
	ZetSetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 MemIndex()
{
	UINT8 *Next = AllMem;

	Drv68KROM[0]       = Next; Next += 0x200000;
	Drv68KROM[1]       = Next; Next += 0x100000;
	DrvZ80ROM          = Next; Next += 0x010000;
	DrvSpriteROM[0]    = Next; Next += 0x200000;
	DrvSpriteROM[1]    = Next; Next += 0x200000;
	DrvTileROM         = Next; Next += 0x060000;
	DrvTileCLUT        = Next; Next += 0x020000;
	MSM6295ROM         = Next;
	DrvSndROM          = Next; Next += 0x040000;
	DrvSpriteGfx[0]    = Next; Next += 0x400000;
	DrvSpriteGfx[1]    = Next; Next += 0x400000;
	DrvTileGfx         = Next; Next += 0x100000;
	DrvTileOpaque      = Next; Next += 0x100000;
	DrvPalette         = (UINT32*)Next; Next += 0x4000 * sizeof(UINT32);

	AllRam             = Next;
	Drv68KRAM[0]       = Next; Next += 0x010000;
	Drv68KRAM[1]       = Next; Next += 0x010000;
	DrvAuxRAM          = Next; Next += 0x001000;
	DrvVidRAM[0]       = Next; Next += 0x010000;
	DrvVidRAM[1]       = Next; Next += 0x010000;
	DrvSprRAM          = Next; Next += 0x004000;
	DrvSprCtrl         = Next; Next += 0x000200;
	DrvPalRAM          = Next; Next += 0x004000;
	DrvZ80RAM          = Next; Next += 0x002000;
	DrvPriRAM          = Next; Next += 320 * 240;
	RamEnd             = Next;
	MemEnd             = Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane3[3] = { 0x40000 * 8, 0x20000 * 8, 0 };
	INT32 XOffs3[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	INT32 YOffs3[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };
	INT32 Plane4[4] = { 0, 1, 2, 3 };
	INT32 XOffs4[8] = { 8, 12, 0, 4, 24, 28, 16, 20 };
	INT32 YOffs4[8] = { 0, 32, 64, 96, 128, 160, 192, 224 };

	GfxDecode(0x4000, 3, 8, 8, Plane3, XOffs3, YOffs3, 0x40, DrvTileROM, DrvTileGfx);
	GfxDecode(0x10000, 4, 8, 8, Plane4, XOffs4, YOffs4, 0x100, DrvSpriteROM[0], DrvSpriteGfx[0]);
	GfxDecode(0x10000, 4, 8, 8, Plane4, XOffs4, YOffs4, 0x100, DrvSpriteROM[1], DrvSpriteGfx[1]);

	for (INT32 code = 0; code < 0x4000; code++) {
		for (INT32 pixel = 0; pixel < 64; pixel++) {
			UINT8 raw = DrvTileGfx[(code * 64) + pixel] & 7;
			UINT8 mapped = DrvTileCLUT[(code * 8) + raw];
			DrvTileOpaque[(code * 64) + pixel] = (raw != 0 || (mapped & 7) != 0);
			DrvTileGfx[(code * 64) + pixel] = mapped;
		}
	}

	return 0;
}

static void map_68000(INT32 cpu)
{
	SekInit(cpu, 0x68000);
	SekOpen(cpu);

	if (cpu == 0) {
		SekMapMemory(Drv68KRAM[0], 0x000000, 0x00ffff, MAP_RAM);
		SekMapMemory(DrvAuxRAM,    0x03e000, 0x03efff, MAP_RAM);
		SekMapMemory(Drv68KRAM[1], 0x040000, 0x04ffff, MAP_RAM);
	} else {
		SekMapMemory(Drv68KRAM[1], 0x000000, 0x00ffff, MAP_RAM);
	}

	SekMapMemory(DrvVidRAM[1], 0x080000, 0x08ffff, MAP_RAM);
	SekMapMemory(DrvVidRAM[0], 0x090000, 0x09ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,    0x0c0000, 0x0c3fff, MAP_RAM);
	SekMapMemory(DrvSprCtrl,   0x0ca000, 0x0ca1ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,    0x0d0000, 0x0d3fff, MAP_RAM);
	SekMapMemory(Drv68KROM[1], 0x100000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KROM[0], 0x200000, 0x3fffff, MAP_ROM);

	SekSetWriteWordHandler(0, bigfight_write_word);
	SekSetWriteByteHandler(0, bigfight_write_byte);
	SekSetReadWordHandler(0, bigfight_read_word);
	SekSetReadByteHandler(0, bigfight_read_byte);
	SekClose();
}

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);
	memcpy(Drv68KRAM[0], Drv68KROM[0], 0x100);
	memcpy(Drv68KRAM[1], Drv68KROM[1], 0x100);

	SekOpen(0);
	SekReset();
	SekSetHALT(0);
	SekClose();
	SekOpen(1);
	SekReset();
	SekSetHALT(0);
	SekClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnYM2151Reset();
	MSM6295Reset(0);
	cxd1095Reset();

	memset(video_config, 0, sizeof(video_config));
	tile_bank = 0;
	mixing_control = 0;
	soundlatch = 0;
	sound_nmi_pending = 0;
	control_word = last_control = 0;
	nExtraCycles[0] = nExtraCycles[1] = 0;

	HiscoreReset();
	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(6250000.0 / (400.0 * 272.0));
	BurnAllocMemIndex();

	if (BurnLoadRom(Drv68KROM[0] + 1, 0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[0] + 0, 1, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[1] + 1, 2, 2)) return 1;
	if (BurnLoadRom(Drv68KROM[1] + 0, 3, 2)) return 1;
	if (BurnLoadRom(DrvZ80ROM,          4, 1)) return 1;

	if (BurnLoadRom(DrvSpriteROM[0] + 0, 5, 4)) return 1;
	if (BurnLoadRom(DrvSpriteROM[0] + 1, 6, 4)) return 1;
	if (BurnLoadRom(DrvSpriteROM[0] + 2, 7, 4)) return 1;
	if (BurnLoadRom(DrvSpriteROM[0] + 3, 8, 4)) return 1;
	if (BurnLoadRom(DrvSpriteROM[1] + 0, 9, 4)) return 1;
	if (BurnLoadRom(DrvSpriteROM[1] + 1,10, 4)) return 1;
	if (BurnLoadRom(DrvSpriteROM[1] + 2,11, 4)) return 1;
	if (BurnLoadRom(DrvSpriteROM[1] + 3,12, 4)) return 1;

	if (BurnLoadRom(DrvTileCLUT,        13, 1)) return 1;
	if (BurnLoadRom(DrvTileROM + 0x00000,14, 1)) return 1;
	if (BurnLoadRom(DrvTileROM + 0x20000,15, 1)) return 1;
	if (BurnLoadRom(DrvTileROM + 0x40000,16, 1)) return 1;
	if (BurnLoadRom(DrvSndROM,          17, 1)) return 1;

	memcpy(Drv68KROM[0] + 0x080000, Drv68KROM[0], 0x080000);
	memcpy(Drv68KROM[0] + 0x100000, Drv68KROM[0], 0x080000);
	memcpy(Drv68KROM[0] + 0x180000, Drv68KROM[0], 0x080000);
	memcpy(Drv68KROM[1] + 0x080000, Drv68KROM[1], 0x080000);

	DrvGfxDecode();
	map_68000(0);
	map_68000(1);

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 0x0000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM, 0xe000, 0xfeff, MAP_RAM);
	ZetSetWriteHandler(sound_write);
	ZetSetReadHandler(sound_read);
	ZetClose();

	cxd1095Init(0, NULL, io0_read);
	cxd1095Init(1, io1_write, io1_read);

	BurnYM2151InitBuffered(4000000, 1, NULL, 0);
	BurnYM2151SetIrqHandler(ym2151_irq_handler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.45, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.45, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachZet(4000000);

	MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	DrvDoReset();
	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	BurnYM2151Exit();
	MSM6295Exit(0);
	SekExit();
	ZetExit();
	BurnFreeMemIndex();
	MSM6295ROM = NULL;
	return 0;
}

static inline UINT16 read_vram_word(INT32 bank, INT32 offset)
{
	return BURN_ENDIAN_SWAP_INT16(((UINT16*)DrvVidRAM[bank])[offset & 0x7fff]);
}

static void draw_layer(INT32 layer, INT32 high_priority)
{
	static const INT32 scroll_bank[4] = { 0, 0, 1, 1 };
	static const INT32 scroll_xoffs[4] = { 0x200, 0x000, 0x200, 0x000 };
	static const INT32 scroll_yoffs[4] = { 0x300, 0x100, 0x300, 0x100 };
	INT32 bank = scroll_bank[layer];
	UINT16 scroll0 = read_vram_word(bank, scroll_xoffs[layer]);
	INT32 rowscroll = (scroll0 & 0x1000) == 0;
	INT32 colscroll = (scroll0 & 0x2000) == 0;
	UINT16 page_select = read_vram_word(bank, scroll_yoffs[layer] + 0xff);
	INT32 dx = (layer == 0) ? 0x10 : 8;

	for (INT32 y = 0; y < nScreenHeight; y++) {
		INT32 ybase = rowscroll ? y : 0;
		INT32 xbase = colscroll ? y : 0;
		UINT16 syreg = read_vram_word(bank, scroll_yoffs[layer] + ybase);
		INT32 sy = syreg & 0x7ff;
		INT32 sx = read_vram_word(bank, scroll_xoffs[layer] + xbase) & 0x7ff;
		if ((sy + y - page_select) >= 0x200 && (syreg & 0x800) == 0) sy -= 0x200;

		INT32 mapy = (y + sy) & 0x7ff;
		INT32 row = (mapy >> 3) * 128;
		INT32 py = (mapy & 7) * 8;
		UINT16 *dst = pTransDraw + (y * nScreenWidth);

		for (INT32 x = 0; x < nScreenWidth; x++) {
			INT32 mapx = (x + sx + dx) & 0x3ff;
			UINT16 tile = read_vram_word(bank, row + (mapx >> 3));
			if (((tile >> 15) & 1) != high_priority) continue;

			INT32 nibble = (tile >> 10) & 3;
			INT32 code = (tile & 0x3ff) | (((tile_bank >> (nibble * 4)) & 0xf) << 10);
			INT32 pixel = (code * 64) + py + (mapx & 7);
			if ((tile & 0x4000) == 0 && DrvTileOpaque[pixel] == 0) continue;

			INT32 color = (tile >> 12) & 3;
			if (layer != 0) color |= 4;
			dst[x] = (color << 8) | DrvTileGfx[pixel];
		}
	}
}

static inline UINT8 sprite_clut(INT32 color, INT32 pixel)
{
	static const INT32 order[4] = { 1, 0, 3, 2 };
	INT32 group = pixel >> 3;
	INT32 half = (pixel >> 2) & 1;
	INT32 index = (color * 8) + (group * 4) + order[pixel & 3];
	return DrvSpriteROM[half][0x1ff000 + index];
}

static void draw_sprite_tile(INT32 bank, INT32 code, INT32 color, INT32 flipx, INT32 flipy,
	INT32 xpos, INT32 ypos, INT32 scale, INT32 priority_only)
{
	INT32 block = 8 * scale;
	INT32 sw = ((xpos & 0xffff) + block) >> 16;
	INT32 sh = ((ypos & 0xffff) + block) >> 16;
	if (sw <= 0 || sh <= 0) return;

	INT32 sx = xpos >> 16;
	INT32 sy = ypos >> 16;
	INT32 dx = (8 << 16) / sw;
	INT32 dy = (8 << 16) / sh;
	INT32 xbase = flipx ? (sw - 1) * dx : 0;
	INT32 yindex = flipy ? (sh - 1) * dy : 0;
	if (flipx) dx = -dx;
	if (flipy) dy = -dy;
	UINT8 *gfx = DrvSpriteGfx[bank] + ((code & 0xffff) * 64);

	for (INT32 y = 0; y < sh; y++, yindex += dy) {
		INT32 yy = sy + y;
		if ((UINT32)yy >= (UINT32)nScreenHeight) continue;
		INT32 xindex = xbase;
		for (INT32 x = 0; x < sw; x++, xindex += dx) {
			INT32 xx = sx + x;
			if ((UINT32)xx >= (UINT32)nScreenWidth) continue;
			UINT8 pixel = gfx[((yindex >> 16) & 7) * 8 + ((xindex >> 16) & 7)];
			if (pixel == 0) continue;
			UINT8 pal = sprite_clut(color, pixel);
			INT32 pos = yy * nScreenWidth + xx;
			if (priority_only) {
				DrvPriRAM[pos] = (pal == 0xff);
			} else if (pal != 0xff) {
				pTransDraw[pos] = 0x1000 + pal;
			}
		}
	}
}

static void draw_sprites(INT32 priority_only)
{
	UINT16 *ram = (UINT16*)DrvSprRAM;
	UINT16 *ctrl = (UINT16*)DrvSprCtrl;
	INT32 rambank = (BURN_ENDIAN_SWAP_INT16(ctrl[0xe0]) & 0x1000) ? 0x1000 : 0;

	for (INT32 offs = rambank; offs < rambank + 0x800; offs += 6) {
		UINT16 index = BURN_ENDIAN_SWAP_INT16(ram[offs + 0]);
		UINT16 attr  = BURN_ENDIAN_SWAP_INT16(ram[offs + 1]);
		UINT16 scale_word = BURN_ENDIAN_SWAP_INT16(ram[offs + 4]);
		if (index == 0xffff || scale_word == 0xffff) return;
		if (index >= 0x4000) continue;

		INT32 x = (INT16)BURN_ENDIAN_SWAP_INT16(ram[offs + 2]);
		INT32 y = (INT16)BURN_ENDIAN_SWAP_INT16(ram[offs + 3]);
		INT32 scale = (scale_word & 0x1ff) << 9;
		INT32 color = (attr >> 3) & 0x1ff;
		INT32 flipx = attr & 0x8000;
		INT32 flipy = attr & 0x4000;
		UINT8 *src1 = DrvSpriteROM[0] + index * 4;
		UINT8 *src2 = DrvSpriteROM[1] + index * 4;
		INT32 lines = src1[2];
		INT32 yoffs = src1[0] & 0xf8;
		lines -= yoffs;
		INT32 renderx = x << 16;
		INT32 rendery = y << 16;
		rendery += flipy ? -(yoffs * scale) : yoffs * scale;
		src1 += 4;

		for (INT32 h = 0; lines > 0; h++, lines -= 8) {
			UINT8 *src = (h & 1) ? src1 : src2;
			INT32 width = src[0] + 1;
			INT32 xoffs = src[1] * scale * 8;
			INT32 base = (src[2] | (src[3] << 8)) * 2;
			INT32 xpos = flipx ? renderx - xoffs - scale * 8 : renderx + xoffs;

			for (INT32 w = 0; w < width; w++, base++) {
				draw_sprite_tile(base & 1, base >> 1, color, flipx, flipy, xpos, rendery, scale, priority_only);
				xpos += flipx ? -(scale * 8) : scale * 8;
			}

			if (h & 1) src1 += 4; else src2 += 4;
			rendery += flipy ? -(scale * 8) : scale * 8;
		}
	}
}

static void palette_update()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;
	for (INT32 i = 0; i < 0x2000; i++) {
		UINT16 d = BURN_ENDIAN_SWAP_INT16(pal[i]);
		INT32 r = (d >> 10) & 0x1f;
		INT32 g = (d >> 5) & 0x1f;
		INT32 b = d & 0x1f;
		DrvPalette[i] = BurnHighCol(r << 3, g << 3, b << 3, 0);
		DrvPalette[i + 0x2000] = BurnHighCol(r << 2, g << 2, b << 2, 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalc = 0;
	}
	palette_update();
	BurnTransferClear(0);
	memset(DrvPriRAM, 0, nScreenWidth * nScreenHeight);

	draw_sprites(1);
	if (nBurnLayer & 1) draw_layer(3, 0);
	if (nBurnLayer & 2) draw_layer(2, 0);
	if (nBurnLayer & 4) draw_layer(1, 0);
	if (nBurnLayer & 8) draw_layer(0, 0);

	INT32 invert = mixing_control & 1;
	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		if ((DrvPriRAM[i] != 0) ^ invert) pTransDraw[i] |= 0x2000;
	}

	if (nSpriteEnable & 1) draw_sprites(0);
	if (nBurnLayer & 1) draw_layer(3, 1);
	if (nBurnLayer & 2) draw_layer(2, 1);
	if (nBurnLayer & 4) draw_layer(1, 1);
	if (nBurnLayer & 8) draw_layer(0, 1);

	BurnTransferCopy(DrvPalette);
	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) DrvDoReset();

	memset(DrvInputs, 0xff, sizeof(DrvInputs));
	for (INT32 port = 0; port < 5; port++) {
		for (INT32 bit = 0; bit < 8; bit++) DrvInputs[port] ^= (DrvJoy[port][bit] & 1) << bit;
	}

	SekNewFrame();
	ZetNewFrame();

	const INT32 nInterleave = 544;
	INT32 nCyclesTotal[3] = {
		(INT32)(12500000.0 / nBurnFPS * 100.0),
		(INT32)(12500000.0 / nBurnFPS * 100.0),
		(INT32)( 4000000.0 / nBurnFPS * 100.0)
	};
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		SekOpen(0);
		CPU_RUN(0, Sek);
		if (i == 480) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		SekClose();

		SekOpen(1);
		CPU_RUN(1, Sek);
		if (i == 480) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		SekClose();

		ZetOpen(0);
		CPU_RUN_TIMER(2);
		// Commands sent during the sound ROM self-test must wait until it is ready.
		if (sound_nmi_pending && DrvZ80RAM[0x1fb8] == 0) {
			ZetNmi();
			sound_nmi_pending = 0;
		}
		ZetClose();
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}
	if (pBurnDraw) DrvDraw();
	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) *pnMin = 0x029744;

	if (nAction & ACB_VOLATILE) {
		BurnArea ba;
		memset(&ba, 0, sizeof(ba));
		ba.Data = AllRam;
		ba.nLen = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);
		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);
		cxd1095Scan(nAction);
		SCAN_VAR(video_config);
		SCAN_VAR(tile_bank);
		SCAN_VAR(mixing_control);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_nmi_pending);
		SCAN_VAR(control_word);
		SCAN_VAR(last_control);
		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) SekSetHALT(1, (control_word & 4) ? 1 : 0);
	return 0;
}

static struct BurnRomInfo bigfightRomDesc[] = {
	{ "rom16.ic77",  0x40000, 0xe7304ec8, 1 | BRF_PRG | BRF_ESS },
	{ "rom17.ic98",  0x40000, 0x4cf090f6, 1 | BRF_PRG | BRF_ESS },
	{ "rom18.ic100", 0x40000, 0x49df6207, 2 | BRF_PRG | BRF_ESS },
	{ "rom19.ic102", 0x40000, 0xc12aa9e9, 2 | BRF_PRG | BRF_ESS },
	{ "rom20.ic91",  0x10000, 0xb3add091, 3 | BRF_PRG | BRF_ESS },

	{ "rom0.ic26",   0x80000, 0xa4a3c8d6, 4 | BRF_GRA },
	{ "rom8.ic45",   0x80000, 0x220956ed, 4 | BRF_GRA },
	{ "rom2.ic28",   0x80000, 0xc4f6d243, 4 | BRF_GRA },
	{ "rom10.ic47",  0x80000, 0x0212d472, 4 | BRF_GRA },
	{ "rom4.ic30",   0x80000, 0x999ff7e9, 5 | BRF_GRA },
	{ "rom12.ic49",  0x80000, 0xcb4c1f0b, 5 | BRF_GRA },
	{ "rom6.ic32",   0x80000, 0xf70e2d47, 5 | BRF_GRA },
	{ "rom14.ic51",  0x80000, 0x77430bc9, 5 | BRF_GRA },

	{ "rom21.ic128", 0x20000, 0xda027dcf, 6 | BRF_GRA },
	{ "rom24.ic73",  0x20000, 0xc564185d, 7 | BRF_GRA },
	{ "rom23.ic72",  0x20000, 0xf8bb340b, 7 | BRF_GRA },
	{ "rom22.ic71",  0x20000, 0xfb505074, 7 | BRF_GRA },
	{ "rom15.ic39",  0x40000, 0x58d136e8, 8 | BRF_SND },
};

STD_ROM_PICK(bigfight)
STD_ROM_FN(bigfight)

struct BurnDriver BurnDrvBigfight = {
	"bigfight", NULL, NULL, NULL, "1992",
	"Big Fight - Big Trouble In The Atlantic Ocean\0", NULL, "Tatsumi", "Tatsumi ABA-011",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, bigfightRomInfo, bigfightRomName, NULL, NULL, NULL, NULL, BigfightInputInfo, BigfightDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	320, 240, 4, 3
};
