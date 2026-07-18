// FB Neo Run and Gun / Slam Dunk driver module
// Based on the MAME driver by R. Belmont, David Haywood and Acho A. Tang.
// Generated with Codex AI (by DsNo)

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "konamiic.h"
#include "k054539.h"
#include "dtimer.h"
#include <math.h>

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROMExp0;
static UINT8 *DrvGfxROMExp1;
static UINT8 *DrvSndROM;
static UINT8 *DrvEEPROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvZ80ExtRAM0;
static UINT8 *DrvZ80ExtRAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSysRegs;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPsacRAM;
static UINT8 *DrvTtlRAM;
static UINT8 *DrvK053936Ctrl;
static UINT8 *DrvK053936Line;

static UINT32 *DrvPalette;
static UINT32 *DrvDualLeftBitmap;
static UINT32 *DrvDualRightBitmap;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvReset;
static UINT16 DrvInputs[5];
static UINT8 DrvDips[2];
static UINT8 DrvPrevDips[2];

static UINT8 soundlatch[4];
static UINT8 k054321_active;
static UINT8 k054321_volume;
static UINT8 ccu_regs[0x10];
static UINT8 sound_ctrl;
static UINT8 sound_nmi_clk;
static UINT8 sound_mute;
static UINT8 roz_rombase;
static UINT8 video_mux_bank;
static UINT8 spriteram_bank;
static UINT8 current_display_bank;
static UINT8 single_screen_mode;
static UINT8 video_priority_mode;
static UINT8 irq5_enable;
static UINT8 is_dual_screen;
static UINT16 ccu_vc;
static INT32 nExtraCycles[2];
static UINT8 rng_eeprom_cs;
static UINT8 rng_eeprom_clk;
static UINT8 rng_eeprom_di;
static UINT8 rng_eeprom_do;
static UINT8 rng_eeprom_state;
static UINT8 rng_eeprom_bits;
static UINT16 rng_eeprom_cmd;
static UINT8 rng_eeprom_addr;
static UINT32 rng_eeprom_shift;

static const INT32 RNG_VISIBLE_X = 88;
static const INT32 RNG_VISIBLE_Y = 24;
static const INT32 RUNGUN_SOUND_GAIN = 8;

static INT32 RungunDualOutputEnabled()
{
	return is_dual_screen || (DrvDips[1] & 0x10);
}

static void RungunCheckScreenSize()
{
	INT32 dual = RungunDualOutputEnabled();
	INT32 width = dual ? 768 : 384;

	if (width == nScreenWidth) return;

	BurnTransferSetDimensions(width, 224);
	GenericTilesSetClipRaw(0, width, 0, 224);
	BurnDrvSetVisibleSize(width, 224);
	BurnDrvSetAspect(dual ? 8 : 4, 3);
	ReinitialiseVideo();
	BurnTransferRealloc();
	KonamiAllocateBitmaps();
}

enum {
	RNG_EEPROM_RESET = 0,
	RNG_EEPROM_START,
	RNG_EEPROM_COMMAND,
	RNG_EEPROM_READ,
	RNG_EEPROM_WRITE
};

static struct BurnInputInfo RungunInputList[] = {
	{"P1 Coin",       BIT_DIGITAL,   DrvJoy1 + 0,  "p1 coin"	},
	{"P1 Start",      BIT_DIGITAL,   DrvJoy2 + 7,  "p1 start"	},
	{"P1 Up",         BIT_DIGITAL,   DrvJoy2 + 2,  "p1 up"		},
	{"P1 Down",       BIT_DIGITAL,   DrvJoy2 + 3,  "p1 down"	},
	{"P1 Left",       BIT_DIGITAL,   DrvJoy2 + 0,  "p1 left"	},
	{"P1 Right",      BIT_DIGITAL,   DrvJoy2 + 1,  "p1 right"	},
	{"P1 Button 1",   BIT_DIGITAL,   DrvJoy2 + 4,  "p1 fire 1"	},
	{"P1 Button 2",   BIT_DIGITAL,   DrvJoy2 + 5,  "p1 fire 2"	},
	{"P1 Button 3",   BIT_DIGITAL,   DrvJoy2 + 6,  "p1 fire 3"	},

	{"P2 Coin",       BIT_DIGITAL,   DrvJoy1 + 1,  "p2 coin"	},
	{"P2 Start",      BIT_DIGITAL,   DrvJoy3 + 7,  "p2 start"	},
	{"P2 Up",         BIT_DIGITAL,   DrvJoy3 + 2,  "p2 up"		},
	{"P2 Down",       BIT_DIGITAL,   DrvJoy3 + 3,  "p2 down"	},
	{"P2 Left",       BIT_DIGITAL,   DrvJoy3 + 0,  "p2 left"	},
	{"P2 Right",      BIT_DIGITAL,   DrvJoy3 + 1,  "p2 right"	},
	{"P2 Button 1",   BIT_DIGITAL,   DrvJoy3 + 4,  "p2 fire 1"	},
	{"P2 Button 2",   BIT_DIGITAL,   DrvJoy3 + 5,  "p2 fire 2"	},
	{"P2 Button 3",   BIT_DIGITAL,   DrvJoy3 + 6,  "p2 fire 3"	},

	{"P3 Coin",       BIT_DIGITAL,   DrvJoy1 + 2,  "p3 coin"	},
	{"P3 Start",      BIT_DIGITAL,   DrvJoy4 + 7,  "p3 start"	},
	{"P3 Up",         BIT_DIGITAL,   DrvJoy4 + 2,  "p3 up"		},
	{"P3 Down",       BIT_DIGITAL,   DrvJoy4 + 3,  "p3 down"	},
	{"P3 Left",       BIT_DIGITAL,   DrvJoy4 + 0,  "p3 left"	},
	{"P3 Right",      BIT_DIGITAL,   DrvJoy4 + 1,  "p3 right"	},
	{"P3 Button 1",   BIT_DIGITAL,   DrvJoy4 + 4,  "p3 fire 1"	},
	{"P3 Button 2",   BIT_DIGITAL,   DrvJoy4 + 5,  "p3 fire 2"	},
	{"P3 Button 3",   BIT_DIGITAL,   DrvJoy4 + 6,  "p3 fire 3"	},

	{"P4 Coin",       BIT_DIGITAL,   DrvJoy1 + 3,  "p4 coin"	},
	{"P4 Start",      BIT_DIGITAL,   DrvJoy5 + 7,  "p4 start"	},
	{"P4 Up",         BIT_DIGITAL,   DrvJoy5 + 2,  "p4 up"		},
	{"P4 Down",       BIT_DIGITAL,   DrvJoy5 + 3,  "p4 down"	},
	{"P4 Left",       BIT_DIGITAL,   DrvJoy5 + 0,  "p4 left"	},
	{"P4 Right",      BIT_DIGITAL,   DrvJoy5 + 1,  "p4 right"	},
	{"P4 Button 1",   BIT_DIGITAL,   DrvJoy5 + 4,  "p4 fire 1"	},
	{"P4 Button 2",   BIT_DIGITAL,   DrvJoy5 + 5,  "p4 fire 2"	},
	{"P4 Button 3",   BIT_DIGITAL,   DrvJoy5 + 6,  "p4 fire 3"	},

	{"Reset",         BIT_DIGITAL,   &DrvReset,    "reset"		},
	{"Service 1",     BIT_DIGITAL,   DrvJoy1 + 4,  "service"	},
	{"Service 2",     BIT_DIGITAL,   DrvJoy1 + 5,  "service"	},
	{"Service 3",     BIT_DIGITAL,   DrvJoy1 + 6,  "service"	},
	{"Service 4",     BIT_DIGITAL,   DrvJoy1 + 7,  "service"	},
	{"Dip A",         BIT_DIPSWITCH, DrvDips + 0,  "dip"		},
	{"Dip B",         BIT_DIPSWITCH, DrvDips + 1,  "dip"		},
};

STDINPUTINFO(Rungun)

static struct BurnDIPInfo RungunDIPList[] = {
	{0x29, 0xff, 0xff, 0x00, NULL},
	{0x2a, 0xff, 0xff, 0x8c, NULL},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x29, 0x01, 0x01, 0x00, "Off"					},
	{0x29, 0x01, 0x01, 0x01, "On"					},

	{0   , 0xfe, 0   ,    2, "Bit2 (Unknown)"		},
	{0x2a, 0x01, 0x04, 0x04, "Off"					},
	{0x2a, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x2a, 0x01, 0x08, 0x08, "Off"					},
	{0x2a, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Monitors"				},
	{0x2a, 0x01, 0x10, 0x00, "1"					},
	{0x2a, 0x01, 0x10, 0x10, "2"					},

	{0   , 0xfe, 0   ,    2, "Number of players"	},
	{0x2a, 0x01, 0x20, 0x00, "2"					},
	{0x2a, 0x01, 0x20, 0x20, "4"					},

	{0   , 0xfe, 0   ,    2, "Sound Output"			},
	{0x2a, 0x01, 0x40, 0x40, "Mono"					},
	{0x2a, 0x01, 0x40, 0x00, "Stereo"				},

	{0   , 0xfe, 0   ,    2, "Bit7 (Unknown)"		},
	{0x2a, 0x01, 0x80, 0x80, "Off"					},
	{0x2a, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Rungun)

static void DrvPaletteUpdate(INT32 bank)
{
	UINT16 *p = (UINT16*)(DrvPalRAM + (bank * 0x800));

	for (INT32 i = 0; i < 0x800 / 2; i++)
	{
		UINT16 d = BURN_ENDIAN_SWAP_INT16(p[i]);
		UINT8 r = (d >>  0) & 0x1f;
		UINT8 g = (d >>  5) & 0x1f;
		UINT8 b = (d >> 10) & 0x1f;

		DrvPalette[i] = BurnHighCol(r << 3, g << 3, b << 3, 0);
	}
}

static void sprite_callback(INT32 *, INT32 *color, INT32 *)
{
	*color = 0x20 + (*color & 0x1f);
}

static void psac_callback(INT32 offset, UINT16 *ram, INT32 *code, INT32 *color, INT32 *, INT32 *, INT32 *fx, INT32 *fy)
{
	UINT16 attr = BURN_ENDIAN_SWAP_INT16(ram[(offset * 2) + 0]);
	UINT16 tile = BURN_ENDIAN_SWAP_INT16(ram[(offset * 2) + 1]);

	*code = tile & 0x3fff;
	*fx = (tile >> 14) & 1;
	*fy = (tile >> 15) & 1;
	*color = (0x10 + (attr & 0x0f)) << 4;
}

static void sound_nmi_callback(INT32 state)
{
	if ((sound_ctrl & 0x10) && !sound_nmi_clk && state) {
		ZetNmi();
	}

	sound_nmi_clk = state;
}

static void k054321_update_volume()
{
	float volume = powf(2.0f, (k054321_volume - 40) / 10.0f);
	if (sound_mute) volume = 0.0f;

	float left = (k054321_active & 2) ? volume : 0.0f;
	float right = (k054321_active & 1) ? volume : 0.0f;

	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00 * right, BURN_SND_ROUTE_RIGHT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00 * left, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_1, 0.60 * right, BURN_SND_ROUTE_RIGHT);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_2, 0.60 * left, BURN_SND_ROUTE_LEFT);
}

static void k054321_main_write(UINT8 offset, UINT8 data)
{
	switch (offset & 0x1f)
	{
		case 0x00:
			k054321_active = data;
			k054321_update_volume();
		return;

		case 0x02:
			k054321_volume = 0;
			k054321_update_volume();
		return;

		case 0x03:
			if (data && k054321_volume < 64) {
				k054321_volume++;
				k054321_update_volume();
			}
		return;

		case 0x06:
		case 0x0c:
			soundlatch[0] = data;
		return;

		case 0x07:
		case 0x0e:
			soundlatch[1] = data;
		return;
	}
}

static void rng_eeprom_reset()
{
	rng_eeprom_state = RNG_EEPROM_RESET;
	rng_eeprom_cs = 0;
	rng_eeprom_clk = 0;
	rng_eeprom_di = 0;
	rng_eeprom_do = 1;
	rng_eeprom_bits = 0;
	rng_eeprom_cmd = 0;
	rng_eeprom_addr = 0;
	rng_eeprom_shift = 0;
}

static void rng_eeprom_write(INT32 di, INT32 cs, INT32 clk)
{
	UINT8 old_cs = rng_eeprom_cs;
	UINT8 old_clk = rng_eeprom_clk;

	rng_eeprom_di = di & 1;
	rng_eeprom_cs = cs & 1;
	rng_eeprom_clk = clk & 1;

	if (!old_cs && rng_eeprom_cs) {
		rng_eeprom_state = RNG_EEPROM_START;
		rng_eeprom_bits = 0;
		rng_eeprom_cmd = 0;
		rng_eeprom_do = 1;
		return;
	}

	if (old_cs && !rng_eeprom_cs) {
		rng_eeprom_state = RNG_EEPROM_RESET;
		rng_eeprom_do = 1;
		return;
	}

	if (!rng_eeprom_cs || old_clk || !rng_eeprom_clk) return;

	switch (rng_eeprom_state)
	{
		case RNG_EEPROM_START:
			if (rng_eeprom_di) {
				rng_eeprom_state = RNG_EEPROM_COMMAND;
				rng_eeprom_bits = 0;
				rng_eeprom_cmd = 0;
			}
		break;

		case RNG_EEPROM_COMMAND:
			rng_eeprom_cmd = (rng_eeprom_cmd << 1) | rng_eeprom_di;
			rng_eeprom_bits++;

			if (rng_eeprom_bits == 11) {
				INT32 opcode = rng_eeprom_cmd >> 9;
				rng_eeprom_addr = rng_eeprom_cmd & 0x7f;

				if (opcode == 2) {
					rng_eeprom_state = RNG_EEPROM_READ;
					rng_eeprom_bits = 0;
					rng_eeprom_shift = 0;
					rng_eeprom_do = 0;
				} else if (opcode == 1 || opcode == 3) {
					rng_eeprom_state = RNG_EEPROM_WRITE;
					rng_eeprom_bits = 0;
					rng_eeprom_shift = 0;
				} else {
					rng_eeprom_state = RNG_EEPROM_RESET;
					rng_eeprom_do = 1;
				}
			}
		break;

		case RNG_EEPROM_READ:
			if ((rng_eeprom_bits & 7) == 0) {
				rng_eeprom_shift = DrvEEPROM[rng_eeprom_addr] << 24;
			} else {
				rng_eeprom_shift = (rng_eeprom_shift << 1) | 1;
			}

			rng_eeprom_do = (rng_eeprom_shift >> 31) & 1;
			rng_eeprom_bits++;
		break;

		case RNG_EEPROM_WRITE:
			rng_eeprom_shift = (rng_eeprom_shift << 1) | rng_eeprom_di;
			rng_eeprom_bits++;

			if (rng_eeprom_bits == 8) {
				DrvEEPROM[rng_eeprom_addr] = rng_eeprom_shift & 0xff;
				rng_eeprom_state = RNG_EEPROM_RESET;
				rng_eeprom_do = 1;
			}
		break;
	}
}

static void sound_ctrl_write(UINT8 data)
{
	sound_ctrl = data;
	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80ROM + 0x10000 + ((data & 7) * 0x4000));
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80ROM + 0x10000 + ((data & 7) * 0x4000));

	if ((data & 0x10) == 0) {
		ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
	}
}

static UINT8 __fastcall rungun_sound_read(UINT16 address)
{
	if (address >= 0xe000 && address <= 0xe22f) return K054539Read(0, address & 0x3ff);
	if (address >= 0xe230 && address <= 0xe3ff) return DrvZ80ExtRAM0[address - 0xe230];
	if (address >= 0xe400 && address <= 0xe62f) return K054539Read(1, address & 0x3ff);
	if (address >= 0xe630 && address <= 0xe7ff) return DrvZ80ExtRAM1[address - 0xe630];

	if (address >= 0xf000 && address <= 0xf003) {
		switch (address & 3)
		{
			case 0x02:
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
				return soundlatch[0];

			case 0x03:
				ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
				return soundlatch[1];
		}

		return 0xff;
	}

	return 0xff;
}

static void __fastcall rungun_sound_write(UINT16 address, UINT8 data)
{
	if (address >= 0xe000 && address <= 0xe22f) {
		K054539Write(0, address & 0x3ff, data);
		return;
	}

	if (address >= 0xe230 && address <= 0xe3ff) {
		DrvZ80ExtRAM0[address - 0xe230] = data;
		return;
	}

	if (address >= 0xe400 && address <= 0xe62f) {
		K054539Write(1, address & 0x3ff, data);
		return;
	}

	if (address >= 0xe630 && address <= 0xe7ff) {
		DrvZ80ExtRAM1[address - 0xe630] = data;
		return;
	}

	if (address >= 0xf000 && address <= 0xf003) {
		if ((address & 3) == 0) {
			soundlatch[2] = data;
		}
		return;
	}

	if (address == 0xf800) {
		sound_ctrl_write(data);
		return;
	}
}

static void sprite_dma_copy(INT32 src_bank)
{
	UINT16 *dst = (UINT16*)K053247Ram;
	UINT16 *src = (UINT16*)DrvSprRAM;
	INT32 src_offset = (src_bank & 1) * 0x1000;

	for (INT32 i = 0; i < 0x1000 / 2; i++) {
		dst[i] = src[src_offset + i];
	}
}

static void sprite_dma_trigger()
{
	sprite_dma_copy(single_screen_mode ? 1 : current_display_bank);
}

static UINT16 sysregs_read(INT32 offset)
{
	UINT16 *sys = (UINT16*)DrvSysRegs;
	UINT16 data = BURN_ENDIAN_SWAP_INT16(sys[offset & 0xf]);

	switch ((offset & 0xf) << 1)
	{
		case 0x00:
			return (DrvInputs[1] & 0xff) | ((DrvInputs[3] & 0xff) << 8);

		case 0x02:
			return (DrvInputs[2] & 0xff) | ((DrvInputs[4] & 0xff) << 8);

		case 0x04:
		{
			UINT16 field = (~nCurrentFrame & 1) << 9;
			if (single_screen_mode) field = 0x0200;
			return (DrvInputs[0] & 0x00ff) | ((DrvDips[0] & 1) << 8) | field | 0x0c00;
		}

		case 0x06:
			return (data & 0xff00) | (DrvDips[1] & 0xfc) | (rng_eeprom_do ? 0x01 : 0x00) | 0x02;
	}

	return data;
}

static void sysregs_write(INT32 offset, UINT16 data, INT32 access_high, INT32 access_low)
{
	UINT16 *sys = (UINT16*)DrvSysRegs;
	UINT16 old = BURN_ENDIAN_SWAP_INT16(sys[offset & 0xf]);
	UINT16 combined = old;

	if (access_high) combined = (combined & 0x00ff) | (data & 0xff00);
	if (access_low) combined = (combined & 0xff00) | (data & 0x00ff);

	sys[offset & 0xf] = BURN_ENDIAN_SWAP_INT16(combined);

	switch ((offset & 0xf) << 1)
	{
		case 0x08:
		if (access_low) {
			rng_eeprom_write((combined >> 0) & 1, (combined >> 1) & 1, (combined >> 2) & 1);
			spriteram_bank = (combined >> 7) & 1;
			video_mux_bank = ((combined >> 7) & 1) ^ 1;
		}

			if (access_high) {
				irq5_enable = (combined >> 10) & 1;
				single_screen_mode = (combined >> 12) & 1;
				video_priority_mode = (combined >> 14) & 1;
				if (!irq5_enable) SekSetIRQLine(5, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0x0c:
			K053246_set_OBJCHA_line((combined & 0x04) ? 1 : 0);
			sound_mute = (combined & 0x08) ? 0 : 1;
			k054321_update_volume();
			roz_rombase = (combined >> 4) & 0x0f;
		return;
	}
}

static UINT16 __fastcall rungun_main_read_word(UINT32 address)
{
	if (address >= 0x300000 && address <= 0x3007ff) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPalRAM + ((address & 0x7fe) + (video_mux_bank * 0x800)))));
	}

	if (address >= 0x400000 && address <= 0x43ffff) {
		UINT32 addr = ((address & 0x3ffff) >> 1) + (roz_rombase * 0x20000);
		return DrvGfxROM0[addr & 0x3fffff];
	}

	if (address >= 0x480000 && address <= 0x48001f) {
		return sysregs_read((address & 0x1f) >> 1);
	}

	if (address >= 0x4c0000 && address <= 0x4c001f) {
		INT32 offset = (address & 0x1f) >> 1;
		INT32 scanline = (SekTotalCycles() * 264) / (16000000 / 60);
		INT32 vct = (scanline - ccu_vc) & 0x1ff;

		if (offset == 0x0e) return (vct >> 8) & 1;
		if (offset == 0x0f) return vct & 0xff;

		return ccu_regs[offset];
	}

	if (address >= 0x580000 && address <= 0x58001f) {
		switch ((address & 0x1f) >> 1)
		{
			case 0x08:
				return 0x0000;

			case 0x0a:
				return soundlatch[2] << 8;
		}

		return 0x0000;
	}

	if (address >= 0x5c0000 && address <= 0x5c000f) {
		return K053246Read(address & 0x0f);
	}

	if (address >= 0x600000 && address <= 0x601fff) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvSprRAM + (spriteram_bank * 0x2000) + (address & 0x1ffe))));
	}

	if (address >= 0x6c0000 && address <= 0x6cffff) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPsacRAM + (video_mux_bank * 0x100000) + (address & 0xfffe))));
	}

	if (address >= 0x700000 && address <= 0x7007ff) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvK053936Line + (address & 0x7fe))));
	}

	if (address >= 0x740000 && address <= 0x741fff) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvTtlRAM + (video_mux_bank * 0x2000) + (address & 0x1ffe))));
	}

	return 0xffff;
}

static UINT8 __fastcall rungun_main_read_byte(UINT32 address)
{
	if (address >= 0x480000 && address <= 0x48001f) {
		return sysregs_read((address & 0x1f) >> 1) >> ((~address & 1) << 3);
	}

	if (address >= 0x580000 && address <= 0x58001f) {
		if (address & 1) return 0;

		switch ((address & 0x1f) >> 1)
		{
			case 0x08:
				return 0;

			case 0x0a:
				return soundlatch[2];
		}

		return 0;
	}

	if (address >= 0x600000 && address <= 0x601fff) {
		return DrvSprRAM[(spriteram_bank * 0x2000) + ((address & 0x1fff) ^ 1)];
	}

	return rungun_main_read_word(address & ~1) >> ((~address & 1) << 3);
}

static void __fastcall rungun_main_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x300000 && address <= 0x3007ff) {
		*((UINT16*)(DrvPalRAM + ((address & 0x7fe) + (video_mux_bank * 0x800)))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (address >= 0x480000 && address <= 0x48001f) {
		sysregs_write((address & 0x1f) >> 1, data, 1, 1);
		return;
	}

	if (address >= 0x540000 && address <= 0x540001) {
		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		return;
	}

	if (address >= 0x4c0000 && address <= 0x4c001f) {
		INT32 offset = (address & 0x1f) >> 1;
		ccu_regs[offset] = data & 0xff;

		if (offset == 0x08 || offset == 0x09) {
			ccu_vc = (((ccu_regs[0x08] << 8) | ccu_regs[0x09]) & 0x01ff) + 1;
		}

		if (offset == 0x0e || offset == 0x0f) {
			SekSetIRQLine(5, CPU_IRQSTATUS_NONE);
		}

		return;
	}

	if (address >= 0x580000 && address <= 0x58001f) {
		k054321_main_write((address & 0x1f) >> 1, data >> 8);
		return;
	}

	if (address >= 0x5c0010 && address <= 0x5c001f) {
		K053247WriteRegsWord(address & 0x0f, data);
		return;
	}

	if (address >= 0x600000 && address <= 0x601fff) {
		*((UINT16*)(DrvSprRAM + (spriteram_bank * 0x2000) + (address & 0x1ffe))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (address >= 0x640000 && address <= 0x640007) {
		K053246Write(address & 0x07, data | 0x10000);
		return;
	}

	if (address >= 0x680000 && address <= 0x68001f) {
		*((UINT16*)(DrvK053936Ctrl + (address & 0x1e))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (address >= 0x6c0000 && address <= 0x6cffff) {
		*((UINT16*)(DrvPsacRAM + (video_mux_bank * 0x100000) + (address & 0xfffe))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (address >= 0x700000 && address <= 0x7007ff) {
		*((UINT16*)(DrvK053936Line + (address & 0x7fe))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (address >= 0x740000 && address <= 0x741fff) {
		*((UINT16*)(DrvTtlRAM + (video_mux_bank * 0x2000) + (address & 0x1ffe))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}
}

static void __fastcall rungun_main_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0x300000 && address <= 0x3007ff) {
		UINT8 *ptr = DrvPalRAM + ((address & 0x7ff) + (video_mux_bank * 0x800));
		*ptr = data;
		return;
	}

	if (address >= 0x480000 && address <= 0x48001f) {
		UINT16 word = (address & 1) ? data : (data << 8);
		sysregs_write((address & 0x1f) >> 1, word, (address & 1) ? 0 : 1, (address & 1) ? 1 : 0);
		return;
	}

	if (address >= 0x580000 && address <= 0x58001f) {
		if (address & 1) return;

		k054321_main_write((address & 0x1f) >> 1, data);
		return;
	}

	if (address >= 0x540000 && address <= 0x540001) {
		if ((address & 1) == 0) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		return;
	}

	if (address >= 0x4c0000 && address <= 0x4c001f) {
		if (address & 1) {
			INT32 offset = (address & 0x1f) >> 1;
			ccu_regs[offset] = data;

			if (offset == 0x08 || offset == 0x09) {
				ccu_vc = (((ccu_regs[0x08] << 8) | ccu_regs[0x09]) & 0x01ff) + 1;
			}

			if (offset == 0x0e || offset == 0x0f) {
				SekSetIRQLine(5, CPU_IRQSTATUS_NONE);
			}
		}
		return;
	}

	if (address >= 0x5c0010 && address <= 0x5c001f) {
		K053247WriteRegsByte(address & 0x0f, data);
		return;
	}

	if (address >= 0x600000 && address <= 0x601fff) {
		DrvSprRAM[(spriteram_bank * 0x2000) + ((address & 0x1fff) ^ 1)] = data;
		return;
	}

	if (address >= 0x640000 && address <= 0x640007) {
		K053246Write((address & 0x07) ^ 1, data);
		return;
	}

	if (address >= 0x680000 && address <= 0x68001f) {
		DrvK053936Ctrl[address & 0x1f] = data;
		return;
	}

	if (address >= 0x6c0000 && address <= 0x6cffff) {
		DrvPsacRAM[(video_mux_bank * 0x100000) + (address & 0xffff)] = data;
		return;
	}

	if (address >= 0x700000 && address <= 0x7007ff) {
		DrvK053936Line[address & 0x7ff] = data;
		return;
	}

	if (address >= 0x740000 && address <= 0x741fff) {
		DrvTtlRAM[(video_mux_bank * 0x2000) + (address & 0x1fff)] = data;
		return;
	}

	UINT16 old = rungun_main_read_word(address & ~1);
	UINT16 word = (address & 1) ? ((old & 0xff00) | data) : ((old & 0x00ff) | (data << 8));
	rungun_main_write_word(address & ~1, word);
}

static void DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	sound_ctrl_write(0);
	ZetClose();

	sound_mute = 1;
	k054321_active = 0;
	k054321_volume = 0;
	k054321_update_volume();

	K053247Reset();
	K053246_set_OBJCHA_line(0);
	K053936Reset();
	K054539Reset(0);
	K054539Reset(1);
	rng_eeprom_reset();

	memset(ccu_regs, 0, sizeof(ccu_regs));
	ccu_regs[0x08] = 1;

	sound_ctrl = 0;
	sound_nmi_clk = 0;
	roz_rombase = 0;
	video_mux_bank = 1;
	spriteram_bank = 0;
	current_display_bank = 0;
	single_screen_mode = 0;
	video_priority_mode = 0;
	irq5_enable = 0;
	ccu_vc = 0;
	nExtraCycles[0] = nExtraCycles[1] = 0;
	memset(DrvDualLeftBitmap, 0, 384 * 224 * sizeof(UINT32));
	memset(DrvDualRightBitmap, 0, 384 * 224 * sizeof(UINT32));
	DrvPrevDips[0] = DrvDips[0];
	DrvPrevDips[1] = DrvDips[1];
}

static INT32 MemIndex()
{
	UINT8 *Next = AllMem;

	Drv68KROM       = Next; Next += 0x300000;
	DrvZ80ROM       = Next; Next += 0x030000;
	DrvGfxROM0      = Next; Next += 0x400000;
	DrvGfxROM1      = Next; Next += 0x800000;
	DrvGfxROM2      = Next; Next += 0x040000;
	DrvGfxROMExp0   = Next; Next += 0x400000;
	DrvGfxROMExp1   = Next; Next += 0x1000000;
	DrvSndROM       = Next; Next += 0x400000;
	DrvEEPROM       = Next; Next += 0x000080;
	DrvPalette      = (UINT32*)Next; Next += 0x400 * sizeof(UINT32);
	DrvDualLeftBitmap  = (UINT32*)Next; Next += 384 * 224 * sizeof(UINT32);
	DrvDualRightBitmap = (UINT32*)Next; Next += 384 * 224 * sizeof(UINT32);

	AllRam          = Next;
	Drv68KRAM       = Next; Next += 0x020000;
	DrvZ80RAM       = Next; Next += 0x002000;
	DrvZ80ExtRAM0   = Next; Next += 0x000200;
	DrvZ80ExtRAM1   = Next; Next += 0x000200;
	DrvPalRAM       = Next; Next += 0x001000;
	DrvSysRegs      = Next; Next += 0x000020;
	DrvSprRAM       = Next; Next += 0x004000;
	DrvPsacRAM      = Next; Next += 0x200000;
	DrvTtlRAM       = Next; Next += 0x004000;
	DrvK053936Ctrl  = Next; Next += 0x000020;
	DrvK053936Line  = Next; Next += 0x000800;
	RamEnd          = Next;

	MemEnd          = Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	static INT32 Plane[4] = { 0, 1, 2, 3 };
	static INT32 XOffs[16] = { STEP16(0,4) };
	static INT32 YOffs[16] = { STEP16(0,16*4) };
	static INT32 XOffs8[8] = { STEP8(0,4) };
	static INT32 YOffs8[8] = { STEP8(0,8*4) };
	static INT32 RngPlane[4] = { 24, 16, 8, 0 };
	static INT32 RngXOffs[16] = { STEP8(0,1), STEP8(32,1) };
	static INT32 RngYOffs[16] = { STEP16(0,64) };

	UINT8 *sprtmp = (UINT8*)BurnMalloc(0x800000);
	if (sprtmp == NULL) return 1;
	memcpy(sprtmp, DrvGfxROM1, 0x800000);
	GfxDecode(0x10000, 4, 16, 16, RngPlane, RngXOffs, RngYOffs, 16*16*4, sprtmp, DrvGfxROMExp1);
	BurnFree(sprtmp);

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
	if (tmp == NULL) return 1;
	memcpy(tmp, DrvGfxROM0, 0x400000);
	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs, YOffs, 16*16*4, tmp, DrvGfxROMExp0);
	memcpy(tmp, DrvGfxROM2, 0x020000);
	GfxDecode(0x1000, 4, 8, 8, Plane, XOffs8, YOffs8, 8*8*4, tmp, DrvGfxROM2);
	BurnFree(tmp);

	return 0;
}

static void RungunPatch68K(UINT32 address, const UINT8 *data, INT32 length)
{
	for (INT32 i = 0; i < length; i++) {
		Drv68KROM[(address + i) ^ 1] = data[i];
	}
}

static INT32 DrvLoadRoms()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) return 1;
	const char *drvname = BurnDrvGetTextA(DRV_NAME);
	const bool is_prototype_eaa = (strcmp(drvname, "rungunb") == 0) || (strcmp(drvname, "rungunbd") == 0);

	if (BurnLoadRom(Drv68KROM + 0x000001, 0, 2)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x000000, 1, 2)) return 1;

	if (BurnLoadRom(DrvZ80ROM + 0x000000, 2, 1)) return 1;
	memcpy(DrvZ80ROM + 0x020000, DrvZ80ROM + 0x010000, 0x010000);
	memcpy(DrvZ80ROM + 0x010000, DrvZ80ROM + 0x000000, 0x010000);

	BurnRomInfo ri;
	BurnDrvGetRomInfo(&ri, 3);
	memset(tmp, 0xff, 0x100000);
	if (BurnLoadRom(tmp, 3, 1)) return 1;
	if (ri.nLen <= 0x80000) {
		for (INT32 i = 0; i < 0x80000; i++) Drv68KROM[0x100001 + (i * 2)] = tmp[i];
	} else {
		for (INT32 i = 0; i < 0x80000; i++) Drv68KROM[0x200001 + (i * 2)] = tmp[i];
		for (INT32 i = 0; i < 0x80000; i++) Drv68KROM[0x100001 + (i * 2)] = tmp[0x80000 + i];
	}

	BurnDrvGetRomInfo(&ri, 4);
	memset(tmp, 0xff, 0x100000);
	if (BurnLoadRom(tmp, 4, 1)) return 1;
	if (ri.nLen <= 0x80000) {
		for (INT32 i = 0; i < 0x80000; i++) Drv68KROM[0x100000 + (i * 2)] = tmp[i];
	} else {
		for (INT32 i = 0; i < 0x80000; i++) Drv68KROM[0x200000 + (i * 2)] = tmp[i];
		for (INT32 i = 0; i < 0x80000; i++) Drv68KROM[0x100000 + (i * 2)] = tmp[0x80000 + i];
	}

	if (is_prototype_eaa) {
		Drv68KROM[0x100000 + (0x73beb * 2)] = 0xde;

		static const UINT8 check_jump[] = {
			0x4e, 0xf9, 0x00, 0x20, 0x72, 0xe2
		};
		static const UINT8 check_patch[] = {
			0xb0, 0x55, 0x67, 0x12, 0x0c, 0x40, 0xf6, 0xa8,
			0x67, 0x0c, 0x0c, 0x40, 0x9a, 0xa5, 0x67, 0x06,
			0x4e, 0xf9, 0x00, 0x02, 0xd8, 0xbe, 0x4e, 0xf9,
			0x00, 0x02, 0xd8, 0xce
		};
		RungunPatch68K(0x02d8b8, check_jump, sizeof(check_jump));
		RungunPatch68K(0x2072e2, check_patch, sizeof(check_patch));
	}

	if (BurnLoadRom(DrvGfxROM0 + 0x000000, 5, 1)) return 1;
	if (BurnLoadRomExt(DrvGfxROM1 + 0x000000, 6, 8, LD_GROUP(2))) return 1;
	if (BurnLoadRomExt(DrvGfxROM1 + 0x000002, 7, 8, LD_GROUP(2))) return 1;
	if (BurnLoadRomExt(DrvGfxROM1 + 0x000004, 8, 8, LD_GROUP(2))) return 1;
	if (BurnLoadRomExt(DrvGfxROM1 + 0x000006, 9, 8, LD_GROUP(2))) return 1;
	if (BurnLoadRom(DrvGfxROM2 + 0x000000, 10, 1)) return 1;
	if (BurnLoadRom(DrvSndROM + 0x000000, 11, 1)) return 1;
	if (BurnLoadRom(DrvSndROM + 0x200000, 12, 1)) return 1;
	if (BurnLoadRom(DrvEEPROM + 0x000000, 13, 1)) return 1;

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInitCommon(INT32 dual)
{
	is_dual_screen = dual;

	BurnAllocMemIndex();

	if (DrvLoadRoms()) return 1;
	if (DrvGfxDecode()) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM, 0x000000, 0x2fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM, 0x380000, 0x39ffff, MAP_RAM);
	SekSetReadWordHandler(0, rungun_main_read_word);
	SekSetReadByteHandler(0, rungun_main_read_byte);
	SekSetWriteWordHandler(0, rungun_main_write_word);
	SekSetWriteByteHandler(0, rungun_main_write_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM);
	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80ROM + 0x10000);
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80ROM + 0x10000);
	ZetMapArea(0xc000, 0xdfff, 0, DrvZ80RAM);
	ZetMapArea(0xc000, 0xdfff, 1, DrvZ80RAM);
	ZetMapArea(0xc000, 0xdfff, 2, DrvZ80RAM);
	ZetSetReadHandler(rungun_sound_read);
	ZetSetWriteHandler(rungun_sound_write);
	ZetClose();

	GenericTilesInit();
	konami_palette32 = DrvPalette;
	K053936SetRenderTarget(konami_bitmap32, konami_palette32, konami_priority_bitmap);

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x7fffff, sprite_callback, 1);
	K053247SetBpp(4);
	K053247SetRamSize(0x4000);
	K053247SetSpriteOffset(-8, -17);
	K053247SetVisibleOffset(RNG_VISIBLE_X, RNG_VISIBLE_Y);
	K053246SetOffsetByteSwap(1);
	K053936Init(0, DrvPsacRAM + 0x000000, 0x100000, 128 * 16, 128 * 16, psac_callback);
	K053936Init(1, DrvPsacRAM + 0x100000, 0x100000, 128 * 16, 128 * 16, psac_callback);
	K053936EnableWrap(0, 1);
	K053936EnableWrap(1, 1);
	K053936SetOffset(0, 34 - RNG_VISIBLE_X, 9 - RNG_VISIBLE_Y);
	K053936SetOffset(1, 34 - RNG_VISIBLE_X, 9 - RNG_VISIBLE_Y);

	K054539Init(0, 18432000, DrvSndROM, 0x400000);
	K054539SetFlags(0, K054539_UPDATE_AT_KEYON);
	K054539SetIRQCallback(0, sound_nmi_callback);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_LEFT);
	K054539Init(1, 18432000, DrvSndROM, 0x400000);
	K054539SetFlags(1, K054539_UPDATE_AT_KEYON);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_1, 0.60, BURN_SND_ROUTE_RIGHT);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_2, 0.60, BURN_SND_ROUTE_LEFT);

	RungunCheckScreenSize();
	DrvDoReset();

	return 0;
}

static INT32 RungunInit()
{
	return DrvInitCommon(0);
}

static INT32 RungunDualInit()
{
	return DrvInitCommon(1);
}

static INT32 DrvExit()
{
	GenericTilesExit();
	K053247Exit();
	K053936Exit();
	K054539Exit();
	SekExit();
	ZetExit();
	BurnFreeMemIndex();

	is_dual_screen = 0;

	return 0;
}

static void DrvRenderScreen(INT32 bank, INT32 xoffs)
{
	current_display_bank = bank;
	DrvPaletteUpdate(bank);

	if (!video_priority_mode) {
		K053936PredrawTiles2(bank, DrvGfxROMExp0);
		K053936Draw(bank, (UINT16*)DrvK053936Ctrl, (UINT16*)DrvK053936Line, K053936_DRAW_CLIP | 1);
		K053247SpritesRender();
	} else {
		K053247SpritesRender();
		K053936PredrawTiles2(bank, DrvGfxROMExp0);
		K053936Draw(bank, (UINT16*)DrvK053936Ctrl, (UINT16*)DrvK053936Line, K053936_DRAW_CLIP | 1);
	}

	UINT16 *vram = (UINT16*)(DrvTtlRAM + (bank * 0x2000));
	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		UINT8 *lvram = (UINT8*)vram;
		INT32 attr = (lvram[(offs << 2) + 0] & 0xf0) >> 4;
		INT32 code = ((lvram[(offs << 2) + 0] & 0x0f) << 8) | lvram[(offs << 2) + 2];
		INT32 sx = (offs & 0x3f) * 8 - RNG_VISIBLE_X + xoffs;
		INT32 sy = (offs >> 6) * 8 - RNG_VISIBLE_Y;

		if (sx >= -7 && sx < nScreenWidth && sy >= -7 && sy < nScreenHeight) {
			UINT8 *gfx = DrvGfxROM2 + (code * 0x40);

			for (INT32 y = 0; y < 8; y++) {
				INT32 yy = sy + y;
				if (yy < 0 || yy >= nScreenHeight) continue;

				UINT32 *dst = konami_bitmap32 + (yy * nScreenWidth);

				for (INT32 x = 0; x < 8; x++) {
					INT32 xx = sx + x;
					if (xx < 0 || xx >= nScreenWidth) continue;

					UINT8 pen = gfx[(y * 8) + x];
					if (pen) {
						dst[xx] = DrvPalette[(attr << 4) | pen];
					}
				}
			}
		}
	}
}

static void DrvRenderDualScreen()
{
	INT32 screen_width = nScreenWidth;
	INT32 bank = single_screen_mode ? 0 : (nCurrentFrame & 1);

	nScreenWidth = 384;
	GenericTilesSetClipRaw(0, 384, 0, 224);

	KonamiClearBitmaps(BurnHighCol(0, 0, 0, 0));
	DrvRenderScreen(bank, 0);
	memcpy(bank ? DrvDualRightBitmap : DrvDualLeftBitmap, konami_bitmap32, 384 * 224 * sizeof(UINT32));

	nScreenWidth = screen_width;
	GenericTilesSetClipRaw(0, nScreenWidth, 0, nScreenHeight);
	KonamiClearBitmaps(BurnHighCol(0, 0, 0, 0));

	for (INT32 y = 0; y < 224; y++) {
		UINT32 *dst = konami_bitmap32 + (y * nScreenWidth);
		memcpy(dst, DrvDualLeftBitmap + (y * 384), 384 * sizeof(UINT32));
		memcpy(dst + 384, DrvDualRightBitmap + (y * 384), 384 * sizeof(UINT32));
		dst[383] = BurnHighCol(0, 0, 0, 0);
	}
}

static INT32 DrvDraw()
{
	if (RungunDualOutputEnabled() && nScreenWidth >= 768) {
		DrvRenderDualScreen();
	} else {
		KonamiClearBitmaps(BurnHighCol(0, 0, 0, 0));
		DrvRenderScreen(single_screen_mode ? 0 : (nCurrentFrame & 1), 0);
	}

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset || DrvPrevDips[0] != DrvDips[0] || DrvPrevDips[1] != DrvDips[1]) {
		RungunCheckScreenSize();
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();
	timerNewFrame();

	DrvInputs[0] = 0xffff;
	DrvInputs[1] = 0xffff;
	DrvInputs[2] = 0xffff;
	DrvInputs[3] = 0xffff;
	DrvInputs[4] = 0xffff;

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[3] = { 16000000 / 60, 8000000 / 60, 18432000 / 60 };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == 248) {
			current_display_bank = nCurrentFrame & 1;
			sprite_dma_trigger();
			if (irq5_enable) {
				SekSetIRQLine(5, CPU_IRQSTATUS_ACK);
			}
		}

		{
			INT32 cycles = nCyclesTotal[1] / nInterleave;
			nCyclesDone[1] += ZetRun(cycles);
		}

		CPU_RUN(2, timer);
	}

	if (pBurnSoundOut) {
		BurnSoundClear();
		K054539Update(0, pBurnSoundOut, nBurnSoundLen);
		K054539Update(1, pBurnSoundOut, nBurnSoundLen);

		for (INT32 i = 0; i < nBurnSoundLen * 2; i++) {
			INT32 sample = pBurnSoundOut[i] * RUNGUN_SOUND_GAIN;
			if (sample > 32767) sample = 32767;
			if (sample < -32768) sample = -32768;
			pBurnSoundOut[i] = sample;
		}
	}

	ZetClose();
	SekClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		BurnArea ba;
		memset(&ba, 0, sizeof(ba));
		ba.Data = AllRam;
		ba.nLen = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);
		K053247Scan(nAction);
		K053936Scan(nAction);
		K054539Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(k054321_active);
		SCAN_VAR(k054321_volume);
		SCAN_VAR(ccu_regs);
		SCAN_VAR(sound_ctrl);
		SCAN_VAR(sound_nmi_clk);
		SCAN_VAR(roz_rombase);
		SCAN_VAR(video_mux_bank);
		SCAN_VAR(spriteram_bank);
		SCAN_VAR(current_display_bank);
		SCAN_VAR(single_screen_mode);
		SCAN_VAR(video_priority_mode);
		SCAN_VAR(irq5_enable);
		SCAN_VAR(ccu_vc);
		SCAN_VAR(rng_eeprom_cs);
		SCAN_VAR(rng_eeprom_clk);
		SCAN_VAR(rng_eeprom_di);
		SCAN_VAR(rng_eeprom_do);
		SCAN_VAR(rng_eeprom_state);
		SCAN_VAR(rng_eeprom_bits);
		SCAN_VAR(rng_eeprom_cmd);
		SCAN_VAR(rng_eeprom_addr);
		SCAN_VAR(rng_eeprom_shift);
		SCAN_VAR(nExtraCycles);
	}

	return 0;
}

#define RUNGUN_COMMON_EAA \
	{ "247b01.23n",    0x100000, 0x2d774f27, 1 | BRF_PRG | BRF_ESS }, \
	{ "247b02.21n",    0x100000, 0xd088c9de, 1 | BRF_PRG | BRF_ESS }, \
	{ "247a13",        0x200000, 0xc5a8ef29, 3 | BRF_GRA }, \
	{ "247-a11",       0x200000, 0xc3f60854, 4 | BRF_GRA }, \
	{ "247-a08",       0x200000, 0x3e315eef, 4 | BRF_GRA }, \
	{ "247-a09",       0x200000, 0x5ca7bc06, 4 | BRF_GRA }, \
	{ "247-a10",       0x200000, 0xa5ccd243, 4 | BRF_GRA }, \
	{ "247-a12",       0x020000, 0x57a8d26e, 5 | BRF_GRA }, \
	{ "247-a06",       0x200000, 0xb8b2a67e, 6 | BRF_SND }, \
	{ "247-a07",       0x200000, 0x0108142d, 6 | BRF_SND }

#define RUNGUN_COMMON_UAB \
	{ "247a01",        0x080000, 0x8341cf7d, 1 | BRF_PRG | BRF_ESS }, \
	{ "247a02",        0x080000, 0xf5ef3f45, 1 | BRF_PRG | BRF_ESS }, \
	{ "247a13",        0x200000, 0xc5a8ef29, 3 | BRF_GRA }, \
	{ "247-a11",       0x200000, 0xc3f60854, 4 | BRF_GRA }, \
	{ "247-a08",       0x200000, 0x3e315eef, 4 | BRF_GRA }, \
	{ "247-a09",       0x200000, 0x5ca7bc06, 4 | BRF_GRA }, \
	{ "247-a10",       0x200000, 0xa5ccd243, 4 | BRF_GRA }, \
	{ "247-a12",       0x020000, 0x57a8d26e, 5 | BRF_GRA }, \
	{ "247-a06",       0x200000, 0xb8b2a67e, 6 | BRF_SND }, \
	{ "247-a07",       0x200000, 0x0108142d, 6 | BRF_SND }

// Run and Gun (ver EAA 1993 10.8)

static struct BurnRomInfo rungunRomDesc[] = {
	{ "247eaa03.bin",  0x080000, 0xf5c91ec0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "247eaa04.bin",  0x080000, 0x0e62471f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "247a05",        0x020000, 0x64e85430, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_EAA,

	{ "rungun.nv",     0x000080, 0x7bbf0e3c, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(rungun)
STD_ROM_FN(rungun)

struct BurnDriver BurnDrvRungun = {
	"rungun", NULL, NULL, NULL, "1993",
	"Run and Gun (ver EAA 1993 10.8)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, rungunRomInfo, rungunRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	384, 224, 4, 3
};

// Run and Gun (ver EAA 1993 10.8) (dual screen with demux adapter)

static struct BurnRomInfo rungundRomDesc[] = {
	{ "247eaa03.bin",  0x080000, 0xf5c91ec0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "247eaa04.bin",  0x080000, 0x0e62471f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "247a05",        0x020000, 0x64e85430, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_EAA,

	{ "rungun.nv",     0x000080, 0x7bbf0e3c, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(rungund)
STD_ROM_FN(rungund)

struct BurnDriver BurnDrvRungund = {
	"rungund", "rungun", NULL, NULL, "1993",
	"Run and Gun (ver EAA 1993 10.8) (dual screen with demux adapter)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, rungundRomInfo, rungundRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunDualInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	768, 224, 8, 3
};

// Run and Gun (ver EAA 1993 10.4)

static struct BurnRomInfo rungunaRomDesc[] = {
	{ "247eaa03.rom",  0x080000, 0xfec3e1d6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "247eaa04.rom",  0x080000, 0x1b556af9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.13g",         0x020000, 0xc0b35df9, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_EAA,

	{ "runguna.nv",    0x000080, 0x7bbf0e3c, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(runguna)
STD_ROM_FN(runguna)

struct BurnDriver BurnDrvRunguna = {
	"runguna", "rungun", NULL, NULL, "1993",
	"Run and Gun (ver EAA 1993 10.4)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, rungunaRomInfo, rungunaRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	384, 224, 4, 3
};

// Run and Gun (ver EAA 1993 10.4) (dual screen with demux adapter)

static struct BurnRomInfo rungunadRomDesc[] = {
	{ "247eaa03.rom",  0x080000, 0xfec3e1d6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "247eaa04.rom",  0x080000, 0x1b556af9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.13g",         0x020000, 0xc0b35df9, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_EAA,

	{ "runguna.nv",    0x000080, 0x7bbf0e3c, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(rungunad)
STD_ROM_FN(rungunad)

struct BurnDriver BurnDrvRungunad = {
	"rungunad", "rungun", NULL, NULL, "1993",
	"Run and Gun (ver EAA 1993 10.4) (dual screen with demux adapter)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, rungunadRomInfo, rungunadRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunDualInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	768, 224, 8, 3
};

// Run and Gun (ver EAA 1993 9.10, prototype?)

static struct BurnRomInfo rungunbRomDesc[] = {
	{ "4.18n",         0x080000, 0xd6515edb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "5.16n",         0x080000, 0xf2f03eec, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.13g",         0x020000, 0xc0b35df9, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_EAA,

	{ "runguna.nv",    0x000080, 0x7bbf0e3c, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(rungunb)
STD_ROM_FN(rungunb)

struct BurnDriver BurnDrvRungunb = {
	"rungunb", "rungun", NULL, NULL, "1993",
	"Run and Gun (ver EAA 1993 9.10, prototype?)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, rungunbRomInfo, rungunbRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	384, 224, 4, 3
};

// Run and Gun (ver EAA 1993 9.10, prototype?) (dual screen with demux adapter)

static struct BurnRomInfo rungunbdRomDesc[] = {
	{ "4.18n",         0x080000, 0xd6515edb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "5.16n",         0x080000, 0xf2f03eec, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.13g",         0x020000, 0xc0b35df9, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_EAA,

	{ "runguna.nv",    0x000080, 0x7bbf0e3c, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(rungunbd)
STD_ROM_FN(rungunbd)

struct BurnDriver BurnDrvRungunbd = {
	"rungunbd", "rungun", NULL, NULL, "1993",
	"Run and Gun (ver EAA 1993 9.10, prototype?) (dual screen with demux adapter)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, rungunbdRomInfo, rungunbdRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunDualInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	768, 224, 8, 3
};

// Run and Gun (ver UBA 1993 10.8)

static struct BurnRomInfo rungunubaRomDesc[] = {
	{ "247uba03.bin",  0x080000, 0xc24d7500, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "247uba04.bin",  0x080000, 0x3f255a4a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "247a05",        0x020000, 0x64e85430, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_EAA,

	{ "rungunua.nv",   0x000080, 0x9890d304, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(rungunuba)
STD_ROM_FN(rungunuba)

struct BurnDriver BurnDrvRungunuba = {
	"rungunuba", "rungun", NULL, NULL, "1993",
	"Run and Gun (ver UBA 1993 10.8)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, rungunubaRomInfo, rungunubaRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	384, 224, 4, 3
};

// Run and Gun (ver UBA 1993 10.8) (dual screen with demux adapter)

static struct BurnRomInfo rungunubadRomDesc[] = {
	{ "247uba03.bin",  0x080000, 0xc24d7500, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "247uba04.bin",  0x080000, 0x3f255a4a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "247a05",        0x020000, 0x64e85430, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_EAA,

	{ "rungunua.nv",   0x000080, 0x9890d304, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(rungunubad)
STD_ROM_FN(rungunubad)

struct BurnDriver BurnDrvRungunubad = {
	"rungunubad", "rungun", NULL, NULL, "1993",
	"Run and Gun (ver UBA 1993 10.8) (dual screen with demux adapter)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, rungunubadRomInfo, rungunubadRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunDualInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	768, 224, 8, 3
};

// Slam Dunk (ver JAA 1993 10.8)

static struct BurnRomInfo slmdunkjRomDesc[] = {
	{ "247jaa03.bin",  0x020000, 0x87572078, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "247jaa04.bin",  0x020000, 0xaa105e00, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "247a05",        0x020000, 0x64e85430, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_EAA,

	{ "slmdunkj.nv",   0x000080, 0x531d27bd, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(slmdunkj)
STD_ROM_FN(slmdunkj)

struct BurnDriver BurnDrvSlmdunkj = {
	"slmdunkj", "rungun", NULL, NULL, "1993",
	"Slam Dunk (ver JAA 1993 10.8)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, slmdunkjRomInfo, slmdunkjRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	384, 224, 4, 3
};

// Slam Dunk (ver JAA 1993 10.8) (dual screen with demux adapter)

static struct BurnRomInfo slmdunkjdRomDesc[] = {
	{ "247jaa03.bin",  0x020000, 0x87572078, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "247jaa04.bin",  0x020000, 0xaa105e00, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "247a05",        0x020000, 0x64e85430, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_EAA,

	{ "slmdunkj.nv",   0x000080, 0x531d27bd, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(slmdunkjd)
STD_ROM_FN(slmdunkjd)

struct BurnDriver BurnDrvSlmdunkjd = {
	"slmdunkjd", "rungun", NULL, NULL, "1993",
	"Slam Dunk (ver JAA 1993 10.8) (dual screen with demux adapter)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, slmdunkjdRomInfo, slmdunkjdRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunDualInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	768, 224, 8, 3
};

// Run and Gun (ver UAB 1993 10.12, dedicated twin cabinet)

static struct BurnRomInfo rungunuabdRomDesc[] = {
	{ "247uab03.bin",  0x080000, 0xf259fd11, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "247uab04.bin",  0x080000, 0xb918cf5a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "247a05",        0x020000, 0x64e85430, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_UAB,

	{ "rungunu.nv",    0x000080, 0xd501f579, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(rungunuabd)
STD_ROM_FN(rungunuabd)

struct BurnDriver BurnDrvRungunuabd = {
	"rungunuabd", "rungun", NULL, NULL, "1993",
	"Run and Gun (ver UAB 1993 10.12, dedicated twin cabinet)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, rungunuabdRomInfo, rungunuabdRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunDualInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	768, 224, 8, 3
};

// Run and Gun (ver UAB 1993 9.10, dedicated twin cabinet)

static struct BurnRomInfo rungunuaadRomDesc[] = {
	{ "247uaa03.bin",  0x080000, 0xa05f4cd0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "247uaa04.bin",  0x080000, 0xebb11bef, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "247a05",        0x020000, 0x64e85430, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	RUNGUN_COMMON_UAB,

	{ "rungunu.nv",    0x000080, 0xd501f579, 7 | BRF_PRG | BRF_ESS }, // 13 EEPROM
};

STD_ROM_PICK(rungunuaad)
STD_ROM_FN(rungunuaad)

struct BurnDriver BurnDrvRungunuaad = {
	"rungunuaad", "rungun", NULL, NULL, "1993",
	"Run and Gun (ver UAB 1993 9.10, dedicated twin cabinet)\0", NULL, "Konami", "Konami GX247",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, rungunuaadRomInfo, rungunuaadRomName, NULL, NULL, NULL, NULL, RungunInputInfo, RungunDIPInfo,
	RungunDualInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x400,
	768, 224, 8, 3
};
