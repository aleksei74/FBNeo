// FinalBurn Neo Seta ST-0016 driver
// Based on the MAME driver by David Haywood, Tomasz Slanina, R. Belmont et al.
// Generated with Claude AI (by aleksei74)
//
// ST-0016 = Z80 CPU + integrated video (RAM-based sprites/tilemaps) + 8ch PCM sound.
// First target: Renju Kizoku (Visco, 1994) - the simplest ST-0016 board.
// Sound device (st0016_snd) ported separately.

#include "tiles_generic.h"
#include "z80_intf.h"
#include "st0016_snd.h"

extern int ActiveZ80GetIFF1();

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;

static UINT8 *DrvZ80ROM;       // external program ROM (maincpu region, up to 4MB)
static UINT8 *DrvCharRAM;      // 0x200000 character RAM (sprite/tile gfx)
static UINT8 *DrvCharExp;      // expanded gfx (1 byte/pixel) from DrvCharRAM
static UINT8 *DrvSprRAM;       // 0x10000 sprite/object RAM
static UINT8 *DrvPalRAM;       // 0x800 palette RAM
static UINT8 *DrvWorkRAM;      // 0xe000-0xe7ff, 0xe800-0xe87f, 0xf000-0xffff
static UINT32 *DrvPalette;

static UINT8 st_rom_bank;
static UINT8 st_spr_bank[2];
static UINT16 st_char_bank;
static UINT8 st_pal_bank;
static UINT8 st_mux_port;
static UINT8 st_vregs[0xc0];
static UINT32 st_fixedrom_offset;
static INT32 st_spr_dx, st_spr_dy;

static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];   // P1   (port 0xc0)
static UINT8 DrvJoy2[8];   // P2   (port 0xc1/0xc3)
static UINT8 DrvJoy3[8];   // SYSTEM coins/service (mux low nibble)
static UINT8 DrvDip[2];
static UINT8 DrvInput[3];
static UINT8 DrvReset;

#define ST_ROM_SIZE   0x400000
#define CHAR_RAM_SIZE 0x200000
#define CHAR_BANK_MASK ((CHAR_RAM_SIZE / 32) - 1)   // tiles are 32 bytes (8x8x4bpp)
#define SPR_BANK_SIZE 0x1000
#define MAX_SPR_BANK  0x10
#define PAL_BANK_SIZE 0x200
#define ST_UNUSED_PEN 1024

static struct BurnInputInfo RenjuInputList[] = {
	{"P1 Coin",      BIT_DIGITAL, DrvJoy3 + 0, "p1 coin"  }, // SYSTEM bit0
	{"P1 Start",     BIT_DIGITAL, DrvJoy1 + 7, "p1 start" }, // P1 bit7
	{"P1 Up",        BIT_DIGITAL, DrvJoy1 + 0, "p1 up"    },
	{"P1 Down",      BIT_DIGITAL, DrvJoy1 + 1, "p1 down"  },
	{"P1 Left",      BIT_DIGITAL, DrvJoy1 + 2, "p1 left"  },
	{"P1 Right",     BIT_DIGITAL, DrvJoy1 + 3, "p1 right" },
	{"P1 Button 1",  BIT_DIGITAL, DrvJoy1 + 4, "p1 fire 1"},
	{"P1 Button 2",  BIT_DIGITAL, DrvJoy1 + 5, "p1 fire 2"},
	{"P1 Button 3",  BIT_DIGITAL, DrvJoy1 + 6, "p1 fire 3"},

	{"P2 Coin",      BIT_DIGITAL, DrvJoy3 + 1, "p2 coin"  }, // SYSTEM bit1
	{"P2 Up",        BIT_DIGITAL, DrvJoy2 + 0, "p2 up"    },
	{"P2 Down",      BIT_DIGITAL, DrvJoy2 + 1, "p2 down"  },
	{"P2 Left",      BIT_DIGITAL, DrvJoy2 + 2, "p2 left"  },
	{"P2 Right",     BIT_DIGITAL, DrvJoy2 + 3, "p2 right" },
	{"P2 Button 1",  BIT_DIGITAL, DrvJoy2 + 4, "p2 fire 1"},
	{"P2 Button 2",  BIT_DIGITAL, DrvJoy2 + 5, "p2 fire 2"},
	{"P2 Button 3",  BIT_DIGITAL, DrvJoy2 + 6, "p2 fire 3"},
	{"P2 Start",     BIT_DIGITAL, DrvJoy2 + 7, "p2 start" },

	{"Service",      BIT_DIGITAL, DrvJoy3 + 2, "service"  }, // SYSTEM bit2

	{"Reset",        BIT_DIGITAL, &DrvReset,   "reset"    },
	{"Dip A",        BIT_DIPSWITCH, DrvDip + 0, "dip"     },
	{"Dip B",        BIT_DIPSWITCH, DrvDip + 1, "dip"     },
};

STDINPUTINFO(Renju)

static struct BurnDIPInfo RenjuDIPList[] = {
	// Dip A / Dip B are input-list entries 20 (0x14) and 21 (0x15)
	{0x14, 0xff, 0xff, 0xff, NULL },
	{0x15, 0xff, 0xff, 0xff, NULL },

	{0,    0xfe, 0,    2,    "Flip Screen"   },
	{0x14, 0x01, 0x02, 0x02, "Off"           },
	{0x14, 0x01, 0x02, 0x00, "On"            },

	{0,    0xfe, 0,    2,    "Demo Sounds"   },
	{0x14, 0x01, 0x08, 0x00, "Off"           },
	{0x14, 0x01, 0x08, 0x08, "On"            },

	{0,    0xfe, 0,    4,    "Difficulty"    },
	{0x15, 0x01, 0x03, 0x02, "Easy"          },
	{0x15, 0x01, 0x03, 0x03, "Normal"        },
	{0x15, 0x01, 0x03, 0x01, "Hard"          },
	{0x15, 0x01, 0x03, 0x00, "Very Hard"     },
};

STDDIPINFO(Renju)

// ---------------------------------------------------------------------------
// Z80 memory map (ST-0016 internal + renju external)

static UINT8 __fastcall st0016_read(UINT16 address)
{
	if (address <= 0x7fff) {
		return DrvZ80ROM[(st_fixedrom_offset + (address & 0x7fff)) & (ST_ROM_SIZE - 1)];
	}
	if (address <= 0xbfff) {
		return DrvZ80ROM[((st_rom_bank << 14) | (address & 0x3fff)) & (ST_ROM_SIZE - 1)];
	}
	if (address <= 0xcfff) {
		return DrvSprRAM[SPR_BANK_SIZE * st_spr_bank[0] + (address & 0x0fff)];
	}
	if (address <= 0xdfff) {
		return DrvSprRAM[SPR_BANK_SIZE * st_spr_bank[1] + (address & 0x0fff)];
	}
	if (address >= 0xe000 && address <= 0xe7ff) {
		return DrvWorkRAM[address & 0x07ff];
	}
	if (address >= 0xe800 && address <= 0xe87f) {
		return DrvWorkRAM[0x0800 + (address & 0x7f)];
	}
	if (address >= 0xe900 && address <= 0xe9ff) {
		return st0016snd_read(address & 0xff);
	}
	if (address >= 0xea00 && address <= 0xebff) {
		return DrvPalRAM[PAL_BANK_SIZE * st_pal_bank + (address & 0x1ff)];
	}
	if (address >= 0xec00 && address <= 0xec1f) {
		// character ram window (banked into 2MB char ram)
		return DrvCharRAM[(0x20 * st_char_bank + (address & 0x1f)) & (CHAR_RAM_SIZE - 1)];
	}
	if (address >= 0xf000) {
		return DrvWorkRAM[0x0900 + (address & 0x0fff)];
	}

	return 0;
}

// expand one 8x8x4bpp tile (32 bytes) of char RAM to 1-byte-per-pixel gfx.
// charlayout: planes {0,1,2,3} in a nibble; xoffs {1,0,3,2,5,4,7,6}*4; yoffs n*32 bits.
static void st0016_expand_tile(UINT32 tile)
{
	UINT8 *src = DrvCharRAM + (tile << 5);
	UINT8 *dst = DrvCharExp + (tile << 6);
	for (INT32 y = 0; y < 8; y++) {
		UINT8 *row = src + y * 4;
		for (INT32 x = 0; x < 8; x++) {
			INT32 nib = x ^ 1;
			UINT8 b = row[nib >> 1];
			dst[y * 8 + x] = (nib & 1) ? (b & 0x0f) : (b >> 4);
		}
	}
}

static void st0016_charram_write(UINT32 offset, UINT8 data)
{
	offset &= (CHAR_RAM_SIZE - 1);
	if (DrvCharRAM[offset] == data) return;
	DrvCharRAM[offset] = data;
	st0016_expand_tile(offset >> 5);
}

// vregs $a8 bit5 = DMA start: copy program ROM -> char RAM (loads tiles + samples)
static void st0016_do_dma()
{
	UINT32 src = (st_vregs[0xa0] | (st_vregs[0xa1] << 8) | (st_vregs[0xa2] << 16)) << 1;
	UINT32 dst = (st_vregs[0xa3] | (st_vregs[0xa4] << 8) | (st_vregs[0xa5] << 16)) << 1;
	UINT32 len = ((st_vregs[0xa6] | (st_vregs[0xa7] << 8) | ((st_vregs[0xa8] & 0x1f) << 16)) + 1) << 1;

	UINT32 dmin = dst, dmax = dst;
	while (len > 0) {
		if (src < ST_ROM_SIZE && dst < CHAR_RAM_SIZE) {
			DrvCharRAM[dst] = DrvZ80ROM[src];
			if (dst < dmin) dmin = dst;
			if (dst > dmax) dmax = dst;
			src++; dst++; len--;
		} else {
			break; // out of range (e.g. sound-only DMA to elsewhere)
		}
	}
	// re-expand affected tiles
	for (UINT32 t = dmin >> 5; t <= (dmax >> 5); t++) st0016_expand_tile(t);
}

static void st0016_palette_write(UINT32 offset, UINT8 data)
{
	offset += PAL_BANK_SIZE * st_pal_bank;
	offset &= 0x7ff;
	DrvPalRAM[offset] = data;
	DrvRecalc = 1;
}

static void __fastcall st0016_write(UINT16 address, UINT8 data)
{
	if (address <= 0xbfff) {
		return; // ROM
	}
	if (address <= 0xcfff) {
		DrvSprRAM[SPR_BANK_SIZE * st_spr_bank[0] + (address & 0x0fff)] = data;
		return;
	}
	if (address <= 0xdfff) {
		DrvSprRAM[SPR_BANK_SIZE * st_spr_bank[1] + (address & 0x0fff)] = data;
		return;
	}
	if (address >= 0xe000 && address <= 0xe7ff) {
		DrvWorkRAM[address & 0x07ff] = data;
		return;
	}
	if (address >= 0xe800 && address <= 0xe87f) {
		DrvWorkRAM[0x0800 + (address & 0x7f)] = data;
		return;
	}
	if (address >= 0xe900 && address <= 0xe9ff) {
		st0016snd_write(address & 0xff, data);
		return;
	}
	if (address >= 0xea00 && address <= 0xebff) {
		st0016_palette_write(address & 0x1ff, data);
		return;
	}
	if (address >= 0xec00 && address <= 0xec1f) {
		st0016_charram_write(0x20 * st_char_bank + (address & 0x1f), data);
		return;
	}
	if (address >= 0xf000) {
		DrvWorkRAM[0x0900 + (address & 0x0fff)] = data;
		return;
	}
}

// ---------------------------------------------------------------------------
// Z80 IO map

static UINT8 st0016_mux_read()
{
	// 76543210 : low nibble = SYSTEM (coins/service), high nibble = multiplexed DIPs
	INT32 ret = DrvInput[2] & 0x0f;
	UINT8 d0 = DrvDip[0], d1 = DrvDip[1];
	switch (st_mux_port & 0x30) {
		case 0x00: ret |= ((d0 & 1) << 4) | ((d0 & 0x10) << 1) | ((d1 & 1) << 6) | ((d1 & 0x10) << 3); break;
		case 0x10: ret |= ((d0 & 2) << 3) | ((d0 & 0x20)     ) | ((d1 & 2) << 5) | ((d1 & 0x20) << 2); break;
		case 0x20: ret |= ((d0 & 4) << 2) | ((d0 & 0x40) >> 1) | ((d1 & 4) << 4) | ((d1 & 0x40) << 1); break;
		case 0x30: ret |= ((d0 & 8) << 1) | ((d0 & 0x80) >> 2) | ((d1 & 8) << 3) | ((d1 & 0x80)     ); break;
	}
	return ret;
}

static UINT8 __fastcall st0016_in(UINT16 port)
{
	port &= 0xff;

	if (port <= 0xbf) {
		// video regs: $0/$1 = scanline/timer (random), else stored vreg
		if (port == 0 || port == 1) return rand() & 0xff;
		return st_vregs[port];
	}

	switch (port) {
		case 0xc0: return DrvInput[0];               // P1
		case 0xc1: return DrvInput[1];               // P2
		case 0xc2: return st0016_mux_read();         // mux (P2 + DIPs)
		case 0xc3: return DrvInput[1];               // P2
		case 0xf0: return 0;                         // DMA status: complete
	}

	return 0;
}

static void __fastcall st0016_out(UINT16 port, UINT8 data)
{
	port &= 0xff;

	if (port <= 0xbf) {
		st_vregs[port] = data;
		if (port == 0xa8 && (data & 0x20)) st0016_do_dma();
		return;
	}

	switch (port) {
		case 0xc0: st_mux_port = data; return;       // mux select
		case 0xc1: return;
		case 0xc2: return;
		case 0xc3: return;
		case 0xe0: return;                           // renju = $40 (unknown)
		case 0xe1: st_rom_bank = data; return;
		case 0xe2:
			st_spr_bank[0] = data & (MAX_SPR_BANK - 1);
			st_spr_bank[1] = (data >> 4) & (MAX_SPR_BANK - 1);
			return;
		case 0xe3: st_char_bank = (st_char_bank & 0xff00) | data; st_char_bank &= CHAR_BANK_MASK; return;
		case 0xe4: st_char_bank = (st_char_bank & 0x00ff) | (data << 8); st_char_bank &= CHAR_BANK_MASK; return;
		case 0xe5: st_pal_bank = data; return;
		case 0xe6: return;                           // ram bank ?
		case 0xe7: return;                           // watchdog
	}
}

// ---------------------------------------------------------------------------

static INT32 MemIndex()
{
	UINT8 *Next = AllMem;

	DrvZ80ROM   = Next; Next += ST_ROM_SIZE;
	DrvCharExp  = Next; Next += (CHAR_RAM_SIZE / 32) * 64; // expanded gfx

	AllRam      = Next;

	DrvCharRAM  = Next; Next += CHAR_RAM_SIZE;
	DrvSprRAM   = Next; Next += 0x10000;
	DrvPalRAM   = Next; Next += 0x800;
	DrvWorkRAM  = Next; Next += 0x2000;

	RamEnd      = Next;

	DrvPalette  = (UINT32*)Next; Next += 0x401 * sizeof(UINT32);

	MemEnd      = Next;

	return 0;
}

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	st_rom_bank = 0;
	st_spr_bank[0] = st_spr_bank[1] = 0;
	st_char_bank = 0;
	st_pal_bank = 0;
	st_mux_port = 0;
	memset(st_vregs, 0, sizeof(st_vregs));

	st_spr_dx = 0; // renju (game_flag 0)
	st_spr_dy = 0;

	st0016snd_reset();

	DrvRecalc = 1;

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8*)0;
	if ((AllMem = (UINT8*)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	// ROM: renjyu-1.u31 @ 0, rnj2.u32 @ 0x200000
	if (BurnLoadRom(DrvZ80ROM + 0x000000, 0, 1)) return 1;
	if (BurnLoadRom(DrvZ80ROM + 0x200000, 1, 1)) return 1;

	st_fixedrom_offset = 0x200000; // renju: set_fixedrom_offset(0x200000)

	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(st0016_read);
	ZetSetWriteHandler(st0016_write);
	ZetSetInHandler(st0016_in);
	ZetSetOutHandler(st0016_out);
	ZetClose();

	st0016snd_init(DrvCharRAM, CHAR_RAM_SIZE); // ST-0016 sound samples live in char RAM

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	st0016snd_exit();
	GenericTilesExit();
	BurnFree(AllMem);
	AllMem = NULL;
	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x400; i++) {
		UINT16 val = DrvPalRAM[i * 2] | (DrvPalRAM[i * 2 + 1] << 8);
		INT32 r = (val >> 0)  & 0x1f;
		INT32 g = (val >> 5)  & 0x1f;
		INT32 b = (val >> 10) & 0x1f;
		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);
		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
	DrvPalette[ST_UNUSED_PEN] = DrvPalette[0]; // background pen = color 0
}

// ST-0016 renders palette indices into a 16-bit buffer; pixel = color*16 + pendata,
// ST_UNUSED_PEN (1024) marks background. Transparency: draw if pen!=0 OR dest==bg.
static inline void st_putpixel(INT32 x, INT32 y, INT32 val)
{
	if (x > (nScreenWidth - 1)) x -= 512; // hw wrap-around
	if (x < 0 || x >= nScreenWidth || y < 0 || y >= nScreenHeight) return;
	pTransDraw[y * nScreenWidth + x] = val;
}

static inline INT32 st_getpixel(INT32 x, INT32 y)
{
	if (x > (nScreenWidth - 1)) x -= 512;
	if (x < 0 || x >= nScreenWidth || y < 0 || y >= nScreenHeight) return -1;
	return pTransDraw[y * nScreenWidth + x];
}

static void st_draw_bgmap(INT32 priority)
{
	for (INT32 j = 0; j < 0x40; j += 8) {
		if (st_vregs[j + 1] && ((priority && (st_vregs[j + 3] == 0xff)) || (!priority && (st_vregs[j + 3] != 0xff)))) {
			INT32 i = st_vregs[j + 1] * 0x1000;
			for (INT32 x = 0; x < 32 * 2; x++) {
				for (INT32 y = 0; y < 8 * 4; y++) {
					INT32 code  = DrvSprRAM[i] + 256 * DrvSprRAM[i + 1];
					INT32 color = DrvSprRAM[i + 2] & 0x3f;
					INT32 flipx = DrvSprRAM[i + 3] & 0x80;
					INT32 flipy = DrvSprRAM[i + 3] & 0x40;

					INT32 xpos = x * 8 + st_spr_dx;
					INT32 ypos = y * 8 + st_spr_dy;
					const UINT8 *src = DrvCharExp + ((code & CHAR_BANK_MASK) << 6);
					INT32 gfxoffs = 0;
					for (INT32 yl = 0; yl < 8; yl++) {
						INT32 dy = flipy ? (ypos + 7 - yl) : (ypos + yl);
						for (INT32 xl = 0; xl < 8; xl++) {
							INT32 pix = src[gfxoffs++];
							INT32 dx = flipx ? (xpos + 7 - xl) : (xpos + xl);
							if (st_vregs[j + 7] == 0x12) {
								INT32 cur = st_getpixel(dx, dy);
								if (cur >= 0) st_putpixel(dx, dy, (cur | (pix << 4)) & 0x3ff);
							} else {
								if (pix || st_getpixel(dx, dy) == ST_UNUSED_PEN)
									st_putpixel(dx, dy, pix + color * 16);
							}
						}
					}
					i += 4;
				}
			}
		}
	}
}

static void st_draw_sprites()
{
	for (INT32 i = 0; i < SPR_BANK_SIZE * MAX_SPR_BANK; i += 8) {
		INT32 x = DrvSprRAM[i + 4] + ((DrvSprRAM[i + 5] & 3) << 8);
		INT32 y = DrvSprRAM[i + 6] + ((DrvSprRAM[i + 7] & 3) << 8);

		const INT32 use_sizes = DrvSprRAM[i + 1] & 0x10;
		const INT32 globalw = (DrvSprRAM[i + 5] & 0x0c) >> 2;
		const INT32 globalh = (DrvSprRAM[i + 7] & 0x0c) >> 2;

		INT32 si = (DrvSprRAM[i + 1] & 0x0f) >> 1;
		INT32 scrollx = (st_vregs[(si << 2) + 0 + 0x40] + 256 * st_vregs[(si << 2) + 1 + 0x40]) & 0x3ff;
		INT32 scrolly = (st_vregs[(si << 2) + 2 + 0x40] + 256 * st_vregs[(si << 2) + 3 + 0x40]) & 0x3ff;

		if (x & 0x200) x -= 0x400; // sign (non-macs)
		if (y & 0x200) y -= 0x400;
		if (scrollx & 0x200) scrollx -= 0x400;
		if (scrolly & 0x200) scrolly -= 0x400;

		x += scrollx;
		y += scrolly;

		if (DrvSprRAM[i + 3] & 0x80) break; // end of list

		INT32 offset = (DrvSprRAM[i + 2] + 256 * DrvSprRAM[i + 3]) << 3;
		const INT32 length = DrvSprRAM[i + 0] + 1 + 256 * (DrvSprRAM[i + 1] & 1);

		if (offset >= SPR_BANK_SIZE * MAX_SPR_BANK) continue;

		for (INT32 j = 0; j < length; j++) {
			INT32 code = DrvSprRAM[offset] + 256 * DrvSprRAM[offset + 1];
			INT32 sx = DrvSprRAM[offset + 4] + ((DrvSprRAM[offset + 5] & 1) << 8);
			INT32 sy = DrvSprRAM[offset + 6] + ((DrvSprRAM[offset + 7] & 1) << 8);
			sx += x;
			sy += y;
			const INT32 color = DrvSprRAM[offset + 2] & 0x3f;
			INT32 lx = use_sizes ? ((DrvSprRAM[offset + 5] >> 2) & 3) : globalw;
			INT32 ly = use_sizes ? ((DrvSprRAM[offset + 7] >> 2) & 3) : globalh;
			const INT32 flipx = DrvSprRAM[offset + 3] & 0x80;
			const INT32 flipy = DrvSprRAM[offset + 3] & 0x40;
			const INT32 merge = DrvSprRAM[offset + 5] & 0x40;

			INT32 i0 = 0;
			for (INT32 x0 = flipx ? ((1 << lx) - 1) : 0; x0 != (flipx ? -1 : (1 << lx)); x0 += flipx ? -1 : 1) {
				const INT32 xpos = sx + x0 * 8 + st_spr_dx;
				for (INT32 y0 = flipy ? ((1 << ly) - 1) : 0; y0 != (flipy ? -1 : (1 << ly)); y0 += flipy ? -1 : 1) {
					const INT32 ypos = sy + y0 * 8 + st_spr_dy;
					const INT32 tileno = (code + i0++) & CHAR_BANK_MASK;
					const UINT8 *src = DrvCharExp + (tileno << 6);
					INT32 gfxoffs = 0;
					for (INT32 yl = 0; yl < 8; yl++) {
						INT32 dy = flipy ? (ypos + 7 - yl) : (ypos + yl);
						for (INT32 xl = 0; xl < 8; xl++) {
							INT32 pix = src[gfxoffs++];
							INT32 dx = flipx ? (xpos + 7 - xl) : (xpos + xl);
							if (merge) {
								INT32 cur = st_getpixel(dx, dy);
								if (cur >= 0) st_putpixel(dx, dy, (cur | (pix << 4)) & 0x3ff);
							} else {
								if (pix || st_getpixel(dx, dy) == ST_UNUSED_PEN)
									st_putpixel(dx, dy, pix + color * 16);
							}
						}
					}
				}
			}
			offset += 8;
			if (offset >= SPR_BANK_SIZE * MAX_SPR_BANK) break;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) pTransDraw[i] = ST_UNUSED_PEN;

	st_draw_bgmap(0);
	st_draw_sprites();
	st_draw_bgmap(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) DrvDoReset();

	{
		DrvInput[0] = DrvInput[1] = DrvInput[2] = 0xff;
		for (INT32 i = 0; i < 8; i++) {
			DrvInput[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInput[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInput[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	// ST-0016: 8MHz Z80 (48MHz/6), 60Hz. Scanline timer fires IRQ(line0) at
	// scanline 240 and an NMI every 64 scanlines (MAME st0016_state::interrupt).
	const INT32 total_scanlines = 262;
	const INT32 cycles_total = 8000000 / 60;
	const INT32 cyc_per_line = cycles_total / total_scanlines;

	ZetOpen(0);
	for (INT32 sl = 0; sl < total_scanlines; sl++) {
		ZetRun(cyc_per_line);
		if ((sl % 64) == 0 && ActiveZ80GetIFF1()) { // MAME gates NMI on IFF1
			ZetNmi();
		}
		if (sl == 240) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
	}
	ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	ZetClose();

	if (pBurnSoundOut) {
		memset(pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));
		st0016snd_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) DrvDraw();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) *pnMin = 0x029702;

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data = AllRam;
		ba.nLen = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		SCAN_VAR(st_rom_bank);
		SCAN_VAR(st_spr_bank);
		SCAN_VAR(st_char_bank);
		SCAN_VAR(st_pal_bank);
		SCAN_VAR(st_mux_port);
		SCAN_VAR(st_vregs);
		st0016snd_scan(nAction);
	}

	if (nAction & ACB_WRITE) {
		DrvRecalc = 1;
		// rebuild expanded gfx
		for (UINT32 i = 0; i < CHAR_RAM_SIZE; i += 32) {
			UINT8 d = DrvCharRAM[i];
			DrvCharRAM[i] ^= 0xff;
			st0016_charram_write(i, d);
		}
	}

	return 0;
}

// ---------------------------------------------------------------------------

// Renju Kizoku - Kira Kira Gomoku Narabe (ver. 1.0)

static struct BurnRomInfo renjuRomDesc[] = {
	{ "renjyu-1.u31", 0x200000, 0xe0fdbe9b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "rnj2.u32",     0x080000, 0x2015289c, 1 | BRF_PRG | BRF_ESS }, //  1
};

STD_ROM_PICK(renju)
STD_ROM_FN(renju)

struct BurnDriver BurnDrvRenju = {
	"renju", NULL, NULL, NULL, "1994",
	"Renju Kizoku - Kira Kira Gomoku Narabe (ver. 1.0)\0", NULL, "Visco", "Seta ST-0016",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SETA_ST0016, GBF_PUZZLE, 0,
	NULL, renjuRomInfo, renjuRomName, NULL, NULL, NULL, NULL, RenjuInputInfo, RenjuDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x401,
	320, 240, 4, 3
};


// Korean translation (clone of renju, modified main program ROM)

static struct BurnRomInfo renjukRomDesc[] = {
	{ "renjyu-1k.u31", 0x200000, 0x2bd052c5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "rnj2.u32",      0x080000, 0x2015289c, 1 | BRF_PRG | BRF_ESS }, //  1
};

STD_ROM_PICK(renjuk)
STD_ROM_FN(renjuk)

struct BurnDriver BurnDrvRenjuk = {
	"renjuk", "renju", NULL, NULL, "1994",
	"Renju Kizoku - Kira Kira Gomoku Narabe (Korean Translation)\0", NULL, "Visco", "Seta ST-0016",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SETA_ST0016, GBF_PUZZLE, 0,
	NULL, renjukRomInfo, renjukRomName, NULL, NULL, NULL, NULL, RenjuInputInfo, RenjuDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x401,
	320, 240, 4, 3
};
