// FinalBurn Neo From TV Animation Slam Dunk - Super Slams driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "konamiic.h"
#include "burn_ym2610.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvSndROM[2];
static UINT8 *Drv68KRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprTableRAM;
static UINT8 *DrvScreenRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvVRegs;
static UINT8 *DrvK053936LineRAM;
static UINT8 *DrvK053936CtrlRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static INT32 soundlatch;
static INT32 sound_flag;
static INT32 sound_bank;
static INT32 screen_bank;
static INT32 bg_bank;
static INT32 sprite_ctrl;
static UINT16 gfx_bank_reg;
static UINT8 vs9209_latch[8];
static UINT8 vs9209_dir[8];

static struct BurnInputInfo SuprslamInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 7,	"service"	},
	{"Test",			BIT_DIGITAL,	DrvJoy3 + 5,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 6,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Suprslam)

static struct BurnDIPInfo SuprslamDIPList[]=
{
	{0x18, 0xff, 0xff, 0xff, NULL				},
	{0x19, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"		},
	{0x18, 0x01, 0x01, 0x01, "Common"			},
	{0x18, 0x01, 0x01, 0x00, "Separate"			},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x18, 0x01, 0x0e, 0x0a, "3 Coins 1 Credit"	},
	{0x18, 0x01, 0x0e, 0x0c, "2 Coins 1 Credit"	},
	{0x18, 0x01, 0x0e, 0x0e, "1 Coin 1 Credit"	},
	{0x18, 0x01, 0x0e, 0x08, "1 Coin 2 Credits"	},
	{0x18, 0x01, 0x0e, 0x06, "1 Coin 3 Credits"	},
	{0x18, 0x01, 0x0e, 0x04, "1 Coin 4 Credits"	},
	{0x18, 0x01, 0x0e, 0x02, "1 Coin 5 Credits"	},
	{0x18, 0x01, 0x0e, 0x00, "1 Coin 6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x18, 0x01, 0x70, 0x50, "3 Coins 1 Credit"	},
	{0x18, 0x01, 0x70, 0x60, "2 Coins 1 Credit"	},
	{0x18, 0x01, 0x70, 0x70, "1 Coin 1 Credit"	},
	{0x18, 0x01, 0x70, 0x40, "1 Coin 2 Credits"	},
	{0x18, 0x01, 0x70, 0x30, "1 Coin 3 Credits"	},
	{0x18, 0x01, 0x70, 0x20, "1 Coin 4 Credits"	},
	{0x18, 0x01, 0x70, 0x10, "1 Coin 5 Credits"	},
	{0x18, 0x01, 0x70, 0x00, "1 Coin 6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x18, 0x01, 0x80, 0x80, "Off"				},
	{0x18, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x19, 0x01, 0x03, 0x02, "Easy"				},
	{0x19, 0x01, 0x03, 0x03, "Normal"			},
	{0x19, 0x01, 0x03, 0x01, "Hard"				},
	{0x19, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Play Time"		},
	{0x19, 0x01, 0x0c, 0x08, "2:00"				},
	{0x19, 0x01, 0x0c, 0x0c, "3:00"				},
	{0x19, 0x01, 0x0c, 0x04, "4:00"				},
	{0x19, 0x01, 0x0c, 0x00, "5:00"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x19, 0x01, 0x10, 0x10, "Off"				},
	{0x19, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x19, 0x01, 0x20, 0x00, "Off"				},
	{0x19, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x19, 0x01, 0x40, 0x40, "Off"				},
	{0x19, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Country"			},
	{0x19, 0x01, 0x80, 0x80, "Japan"			},
	{0x19, 0x01, 0x80, 0x00, "World"			},
};

STDDIPINFO(Suprslam)

static struct BurnDIPInfo SuprslamkDIPList[]=
{
	{0x18, 0xff, 0xff, 0xff, NULL				},
	{0x19, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Coin Slots"		},
	{0x18, 0x01, 0x01, 0x01, "Common"			},
	{0x18, 0x01, 0x01, 0x00, "Separate"			},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x18, 0x01, 0x0e, 0x0a, "3 Coins 1 Credit"	},
	{0x18, 0x01, 0x0e, 0x0c, "2 Coins 1 Credit"	},
	{0x18, 0x01, 0x0e, 0x0e, "1 Coin 1 Credit"	},
	{0x18, 0x01, 0x0e, 0x08, "1 Coin 2 Credits"	},
	{0x18, 0x01, 0x0e, 0x06, "1 Coin 3 Credits"	},
	{0x18, 0x01, 0x0e, 0x04, "1 Coin 4 Credits"	},
	{0x18, 0x01, 0x0e, 0x02, "1 Coin 5 Credits"	},
	{0x18, 0x01, 0x0e, 0x00, "1 Coin 6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x18, 0x01, 0x70, 0x50, "3 Coins 1 Credit"	},
	{0x18, 0x01, 0x70, 0x60, "2 Coins 1 Credit"	},
	{0x18, 0x01, 0x70, 0x70, "1 Coin 1 Credit"	},
	{0x18, 0x01, 0x70, 0x40, "1 Coin 2 Credits"	},
	{0x18, 0x01, 0x70, 0x30, "1 Coin 3 Credits"	},
	{0x18, 0x01, 0x70, 0x20, "1 Coin 4 Credits"	},
	{0x18, 0x01, 0x70, 0x10, "1 Coin 5 Credits"	},
	{0x18, 0x01, 0x70, 0x00, "1 Coin 6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x18, 0x01, 0x80, 0x80, "Off"				},
	{0x18, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x19, 0x01, 0x03, 0x02, "Easy"				},
	{0x19, 0x01, 0x03, 0x03, "Normal"			},
	{0x19, 0x01, 0x03, 0x01, "Hard"				},
	{0x19, 0x01, 0x03, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Play Time"		},
	{0x19, 0x01, 0x0c, 0x08, "2:00"				},
	{0x19, 0x01, 0x0c, 0x0c, "3:00"				},
	{0x19, 0x01, 0x0c, 0x04, "4:00"				},
	{0x19, 0x01, 0x0c, 0x00, "5:00"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x19, 0x01, 0x10, 0x10, "Off"				},
	{0x19, 0x01, 0x10, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x19, 0x01, 0x20, 0x00, "Off"				},
	{0x19, 0x01, 0x20, 0x20, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x19, 0x01, 0x40, 0x40, "Off"				},
	{0x19, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Country"			},
	{0x19, 0x01, 0x80, 0x80, "Korea"			},
	{0x19, 0x01, 0x80, 0x00, "World"			},
};

STDDIPINFO(Suprslamk)

static inline void palette_write(INT32 offset)
{
	UINT16 p = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPalRAM + offset)));

	INT32 r = (p >>  0) & 0x1f;
	INT32 b = (p >>  5) & 0x1f;
	INT32 g = (p >> 10) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset / 2] = BurnHighCol(r, g, b, 0);
}

static void gfx_bank_write(UINT16 data)
{
	gfx_bank_reg = data;
	screen_bank = data & 0xf000;
	bg_bank = (data & 0x0f00) << 4;
}

static void vs9209_write(INT32 offset, UINT8 data)
{
	INT32 port = offset & 7;

	if (offset & 8) {
		UINT8 old_dir = vs9209_dir[port];
		vs9209_dir[port] = data;

		if (port == 6 && (data & ~old_dir)) {
			sprite_ctrl = vs9209_latch[port];
		}
	} else {
		vs9209_latch[port] = data;

		if (port == 6 && vs9209_dir[port]) {
			sprite_ctrl = data & vs9209_dir[port];
		}
	}
}

static void __fastcall suprslam_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffff000) == 0xffa000) {
		*((UINT16*)(DrvPalRAM + (address & 0xffe))) = BURN_ENDIAN_SWAP_INT16(data);
		palette_write(address & 0xffe);
		return;
	}

	switch (address)
	{
		case 0xff9000:
			soundlatch = data & 0xff;
			sound_flag = 1;
			ZetNmi();
		return;

		case 0xffe000:
			gfx_bank_write(data);
		return;
	}

	if ((address & 0xffffffe0) == 0xfff000) {
		vs9209_write((address >> 1) & 0x0f, data & 0xff);
		return;
	}
}

static void __fastcall suprslam_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffff000) == 0xffa000) {
		DrvPalRAM[(address & 0xfff) ^ 1] = data;
		palette_write(address & 0xffe);
		return;
	}

	if (address == 0xff9001) {
		soundlatch = data;
		sound_flag = 1;
		ZetNmi();
		return;
	}

	if ((address & 0xfffffffe) == 0xffe000) {
		if (address & 1) {
			gfx_bank_write((gfx_bank_reg & 0xff00) | data);
		} else {
			gfx_bank_write((gfx_bank_reg & 0x00ff) | (data << 8));
		}
		return;
	}

	if ((address & 0xffffffe0) == 0xfff000) {
		vs9209_write((address >> 1) & 0x0f, data);
		return;
	}
}

static UINT16 __fastcall suprslam_main_read_word(UINT32 address)
{
	if ((address & 0xfffffff0) == 0xfff000) {
		switch ((address >> 1) & 7) {
			case 0: return DrvInputs[0];
			case 1: return DrvInputs[1];
			case 2: return DrvInputs[2] | (sound_flag ? 0x0000 : 0x0100);
			case 3: return DrvDips[0];
			case 4: return DrvDips[1];
		}
	}

	return 0xffff;
}

static UINT8 __fastcall suprslam_main_read_byte(UINT32 address)
{
	if ((address & 0xfffffff0) == 0xfff000) {
		return suprslam_main_read_word(address & ~1) & 0xff;
	}

	return 0xff;
}

static void bankswitch(INT32 data)
{
	sound_bank = data & 3;

	ZetMapMemory(DrvZ80ROM + sound_bank * 0x8000, 0x8000, 0xffff, MAP_ROM);
}

static void __fastcall suprslam_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			bankswitch(data);
		return;

		case 0x04:
			sound_flag = 0;
		return;

		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			BurnYM2610Write(port & 3, data);
		return;
	}
}

static UINT8 __fastcall suprslam_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x04:
			return soundlatch;

		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			return BurnYM2610Read(port & 3);
	}

	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	if (ZetGetActive() == -1) return;

	ZetSetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static tilemap_callback( bg )
{
	UINT16 attr = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvBgRAM + offs * 2)));

	INT32 code = (attr & 0x0fff) + bg_bank;
	INT32 color = (attr >> 12) + 0x10;

	TILE_SET_INFO(1, code, color, 0);
}

static void suprslam_roz_callback(INT32 offset, UINT16 *ram, INT32 *code, INT32 *color, INT32 *, INT32 *, INT32 *, INT32 *)
{
	UINT16 attr = BURN_ENDIAN_SWAP_INT16(ram[offset]);

	*code = (attr & 0x0fff) + bg_bank;
	*color = ((attr >> 12) + 0x10) << 4;
}

static tilemap_callback( screen )
{
	UINT16 attr = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvScreenRAM + offs * 2)));

	INT32 code = (attr & 0x0fff) + screen_bank;
	INT32 color = attr >> 12;

	TILE_SET_INFO(0, code, color, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM2610Reset();
	K053936Reset();
	bankswitch(0);
	ZetClose();

	soundlatch = 0;
	sound_flag = 0;
	screen_bank = 0;
	bg_bank = 0;
	sprite_ctrl = 0;
	gfx_bank_reg = 0;
	memset(vs9209_latch, 0, sizeof(vs9209_latch));
	memset(vs9209_dir, 0, sizeof(vs9209_dir));

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x100000;
	DrvZ80ROM		= Next; Next += 0x020000;

	DrvGfxROM[0]	= Next; Next += 0x400000;
	DrvGfxROM[1]	= Next; Next += 0x1000000;
	DrvGfxROM[2]	= Next; Next += 0x800000;

	DrvSndROM[0]	= Next; Next += 0x200000;
	DrvSndROM[1]	= Next; Next += 0x100000;

	DrvPalette		= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x002000;
	DrvSprTableRAM	= Next; Next += 0x010000;
	Drv68KRAM		= Next; Next += 0x010000;
	DrvScreenRAM	= Next; Next += 0x001000;
	DrvBgRAM		= Next; Next += 0x002000;
	DrvVRegs		= Next; Next += 0x000040;
	DrvK053936LineRAM	= Next; Next += 0x001000;
	DrvK053936CtrlRAM	= Next; Next += 0x000040;
	DrvPalRAM		= Next; Next += 0x001000;
	DrvZ80RAM		= Next; Next += 0x000800;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(UINT8 *tmp)
{
	INT32 Plane[4] = { 0, 1, 2, 3 };
	INT32 XOffs8x8[8] = { 4, 0, 12, 8, 20, 16, 28, 24 };
	INT32 YOffs8x8[8] = { 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 };
	INT32 XOffs16x16[16] = {
		8, 12, 0, 4, 24, 28, 16, 20,
		32+8, 32+12, 32+0, 32+4, 32+24, 32+28, 32+16, 32+20
	};
	INT32 YOffs16x16[16] = {
		0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
		8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64
	};

	if (BurnLoadRom(tmp + 0x000000,  5, 1)) return 1;

	GfxDecode(0x10000, 4,  8,  8, Plane, XOffs8x8, YOffs8x8, 8*32, tmp, DrvGfxROM[0]);

	if (BurnLoadRom(tmp + 0x000000,  6, 1)) return 1;
	if (BurnLoadRom(tmp + 0x200000,  7, 1)) return 1;
	if (BurnLoadRom(tmp + 0x400000,  8, 1)) return 1;
	if (BurnLoadRom(tmp + 0x600000,  9, 1)) return 1;

	GfxDecode(0x10000, 4, 16, 16, Plane, XOffs16x16, YOffs16x16, 16*64, tmp, DrvGfxROM[1]);

	if (BurnLoadRom(tmp + 0x000000, 10, 1)) return 1;
	if (BurnLoadRom(tmp + 0x200000, 11, 1)) return 1;

	GfxDecode(0x08000, 4, 16, 16, Plane, XOffs16x16, YOffs16x16, 16*64, tmp, DrvGfxROM[2]);

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;

	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;

	memset(AllMem, 0, nLen);
	MemIndex();

	if (BurnLoadRom(Drv68KROM + 0x000000,  0, 1)) return 1;
	if (BurnLoadRom(Drv68KROM + 0x080000,  1, 1)) return 1;

	if (BurnLoadRom(DrvZ80ROM + 0x000000,  2, 1)) return 1;

	if (BurnLoadRom(DrvSndROM[0] + 0x000000,  3, 1)) return 1;
	if (BurnLoadRom(DrvSndROM[1] + 0x000000,  4, 1)) return 1;

	UINT8 *tmp = (UINT8*)BurnMalloc(0x800000);

	if (tmp == NULL) return 1;

	INT32 rc = DrvGfxDecode(tmp);
	BurnFree(tmp);

	if (rc) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,			0xfb0000, 0xfb1fff, MAP_RAM);
	SekMapMemory(DrvSprTableRAM,		0xfc0000, 0xfcffff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xfd0000, 0xfdffff, MAP_RAM);
	SekMapMemory(DrvScreenRAM,		0xfe0000, 0xfe0fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,			0xff0000, 0xff1fff, MAP_RAM);
	SekMapMemory(DrvVRegs,			0xff2000, 0xff203f, MAP_RAM);
	SekMapMemory(DrvK053936LineRAM,	0xff8000, 0xff8fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xffa000, 0xffafff, MAP_ROM);
	SekMapMemory(DrvK053936CtrlRAM,	0xffd000, 0xffd01f, MAP_RAM);
	SekSetWriteWordHandler(0,		suprslam_main_write_word);
	SekSetWriteByteHandler(0,		suprslam_main_write_byte);
	SekSetReadWordHandler(0,			suprslam_main_read_word);
	SekSetReadByteHandler(0,			suprslam_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x77ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x7800, 0x7fff, MAP_RAM);
	ZetSetOutHandler(suprslam_sound_write_port);
	ZetSetInHandler(suprslam_sound_read_port);
	ZetClose();

	INT32 DrvSndROM0Len = 0x200000;
	INT32 DrvSndROM1Len = 0x100000;
	BurnYM2610Init(8000000, DrvSndROM[0], &DrvSndROM0Len, DrvSndROM[1], &DrvSndROM1Len, &DrvFMIRQHandler, 0);
	BurnTimerAttach(&ZetConfig, 4000000);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, screen_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8,  8, 0x400000, 0x000, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM[2], 4, 16, 16, 0x800000, 0x100, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 4, 16, 16, 0x1000000, 0x200, 0x0f);
	GenericTilemapSetTransparent(0, 0x0f);
	BurnBitmapAllocate(1, 64 * 16, 64 * 16, true);

	K053936Init(0, DrvBgRAM, 0x2000, 64 * 16, 64 * 16, suprslam_roz_callback);
	K053936EnableWrap(0, 1);
	K053936SetOffset(0, -45, -21);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	K053936Exit();
	GenericTilesExit();
	BurnYM2610Exit();

	ZetExit();
	SekExit();

	BurnFree(AllMem);

	return 0;
}

static void draw_sprites()
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;
	UINT16 *spritetable = (UINT16*)DrvSprTableRAM;
	GenericTilesGfx *gfx = &GenericGfxData[2];

	INT32 offs;

	for (offs = 0; offs < (0x2000 / 16); offs++) {
		if (BURN_ENDIAN_SWAP_INT16(spriteram[offs]) & 0x4000) break;
	}

	INT32 end = offs;
	offs = 0;

	while (offs != end)
	{
		if ((BURN_ENDIAN_SWAP_INT16(spriteram[offs]) & 0x8000) == 0x0000)
		{
			INT32 attr_start = (BURN_ENDIAN_SWAP_INT16(spriteram[offs]) & 0x03ff) * 4;
			UINT16 *ram = &spriteram[attr_start];

			INT32 oy = (BURN_ENDIAN_SWAP_INT16(ram[0]) & 0x01ff);
			INT32 ysize = (BURN_ENDIAN_SWAP_INT16(ram[0]) & 0x0e00) >> 9;
			INT32 zoomy = 32 - ((BURN_ENDIAN_SWAP_INT16(ram[0]) & 0xf000) >> 12);
			INT32 ox = (BURN_ENDIAN_SWAP_INT16(ram[1]) & 0x01ff);
			INT32 xsize = (BURN_ENDIAN_SWAP_INT16(ram[1]) & 0x0e00) >> 9;
			INT32 zoomx = 32 - ((BURN_ENDIAN_SWAP_INT16(ram[1]) & 0xf000) >> 12);
			INT32 flipx = BURN_ENDIAN_SWAP_INT16(ram[2]) & 0x4000;
			INT32 flipy = BURN_ENDIAN_SWAP_INT16(ram[2]) & 0x8000;
			INT32 color = (((BURN_ENDIAN_SWAP_INT16(ram[2]) & 0x3f00) >> 8) << gfx->depth) + gfx->color_offset;
			INT32 map = (BURN_ENDIAN_SWAP_INT16(ram[3]) & 0xffff) | ((BURN_ENDIAN_SWAP_INT16(ram[2]) & 0x0001) << 16);

			INT32 ystart = flipy ? ysize : 0;
			INT32 yend = flipy ? -1 : ysize + 1;
			INT32 yinc = flipy ? -1 : 1;

			for (INT32 ycnt = ystart; ycnt != yend; ycnt += yinc) {
				INT32 xstart = flipx ? xsize : 0;
				INT32 xend = flipx ? -1 : xsize + 1;
				INT32 xinc = flipx ? -1 : 1;

				for (INT32 xcnt = xstart; xcnt != xend; xcnt += xinc, map++) {
					INT32 startno = BURN_ENDIAN_SWAP_INT16(spritetable[map & 0x7fff]) % gfx->code_mask;

					RenderZoomedTile(pTransDraw, gfx->gfxbase, startno, color, 0x0f, ox + xcnt * zoomx / 2, oy + ycnt * zoomy / 2, flipx, flipy, 16, 16, zoomx << 11, zoomy << 11);
					RenderZoomedTile(pTransDraw, gfx->gfxbase, startno, color, 0x0f, ox + xcnt * zoomx / 2 - 0x200, oy + ycnt * zoomy / 2, flipx, flipy, 16, 16, zoomx << 11, zoomy << 11);
					RenderZoomedTile(pTransDraw, gfx->gfxbase, startno, color, 0x0f, ox + xcnt * zoomx / 2, oy + ycnt * zoomy / 2 - 0x200, flipx, flipy, 16, 16, zoomx << 11, zoomy << 11);
					RenderZoomedTile(pTransDraw, gfx->gfxbase, startno, color, 0x0f, ox + xcnt * zoomx / 2 - 0x200, oy + ycnt * zoomy / 2 - 0x200, flipx, flipy, 16, 16, zoomx << 11, zoomy << 11);
				}
			}
		}

		offs++;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x1000; i += 2) {
			palette_write(i);
		}

		DrvRecalc = 0;
	}

	BurnTransferClear(0);

	UINT16 *vregs = (UINT16*)DrvVRegs;
	GenericTilemapSetScrollX(0, BURN_ENDIAN_SWAP_INT16(vregs[0x04 / 2]));

	if (nBurnLayer & 1) {
		UINT16 *ctrl = (UINT16*)DrvK053936CtrlRAM;
		UINT16 *line = (UINT16*)DrvK053936LineRAM;

		GenericTilemapDraw(1, 1, TMAP_FORCEOPAQUE);
		K053936Draw(0, ctrl, line, K053936_DRAW_16BIT | K053936_DRAW_CLIP);
	}

	if (!(sprite_ctrl & 8) && (nSpriteEnable & 1)) draw_sprites();
	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);
	if ((sprite_ctrl & 8) && (nSpriteEnable & 1)) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	memset(DrvInputs, 0xff, sizeof(DrvInputs));

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
	}

	SekOpen(0);
	ZetOpen(0);

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 16000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nVBlankStart = nInterleave - ((2300 * 60 * nInterleave + 999999) / 1000000);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == nVBlankStart) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

		BurnTimerUpdate((nCyclesTotal[1] * (i + 1)) / nInterleave);
	}

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= AllRam;
		ba.nLen		= RamEnd - AllRam;
		ba.szName	= "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);
		BurnYM2610Scan(nAction, pnMin);
		K053936Scan(nAction);

		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_flag);
		SCAN_VAR(sound_bank);
		SCAN_VAR(screen_bank);
		SCAN_VAR(bg_bank);
		SCAN_VAR(sprite_ctrl);
		SCAN_VAR(gfx_bank_reg);
		SCAN_VAR(vs9209_latch);
		SCAN_VAR(vs9209_dir);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(sound_bank);
		ZetClose();
	}

	return 0;
}

// From TV Animation Slam Dunk - Super Slams

static struct BurnRomInfo suprslamRomDesc[] = {
	{ "eb26ic47.bin",	0x080000, 0x8d051fd8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "eb26ic73.bin",	0x080000, 0xca4ad383, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "eb26ic38.bin",	0x020000, 0x153f2c50, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "eb26ic66.bin",	0x200000, 0x8cb33682, 3 | BRF_SND },           //  3 YM2610 ADPCM-A
	{ "eb26ic59.bin",	0x100000, 0x4ae4095b, 4 | BRF_SND },           //  4 YM2610 ADPCM-B

	{ "eb26ic43.bin",	0x200000, 0x9dfb0959, 5 | BRF_GRA },           //  5 8x8 Tiles

	{ "eb26ic09.bin",	0x200000, 0x5a415365, 6 | BRF_GRA },           //  6 Sprites
	{ "eb26ic10.bin",	0x200000, 0xa04f3140, 6 | BRF_GRA },           //  7
	{ "eb26_100.bin",	0x200000, 0xc2ee5eb6, 6 | BRF_GRA },           //  8
	{ "eb26_101.bin",	0x200000, 0x7df654b7, 6 | BRF_GRA },           //  9

	{ "eb26ic12.bin",	0x200000, 0x14561bd7, 7 | BRF_GRA },           // 10 16x16 BG Tiles
	{ "eb26ic36.bin",	0x200000, 0x92019d89, 7 | BRF_GRA },           // 11
};

STD_ROM_PICK(suprslam)
STD_ROM_FN(suprslam)

struct BurnDriver BurnDrvSuprslam = {
	"suprslam", NULL, NULL, NULL, "1995",
	"From TV Animation Slam Dunk - Super Slams\0", NULL, "Banpresto / Toei Animation / Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, suprslamRomInfo, suprslamRomName, NULL, NULL, NULL, NULL, SuprslamInputInfo, SuprslamDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 224, 4, 3
};

// From TV Animation Slam Dunk - Super Slams (Korean Translation)

static struct BurnRomInfo suprslamkRomDesc[] = {
	{ "eb26ic47k.bin",	0x080000, 0x3fa42f6e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "eb26ic73k.bin",	0x080000, 0x08d9cdf4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "eb26ic38.bin",	0x020000, 0x153f2c50, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "eb26ic66.bin",	0x200000, 0x8cb33682, 3 | BRF_SND },           //  3 YM2610 ADPCM-A
	{ "eb26ic59.bin",	0x100000, 0x4ae4095b, 4 | BRF_SND },           //  4 YM2610 ADPCM-B

	{ "eb26ic43k.bin",	0x200000, 0xcfc28eed, 5 | BRF_GRA },           //  5 8x8 Tiles

	{ "eb26ic09.bin",	0x200000, 0x5a415365, 6 | BRF_GRA },           //  6 Sprites
	{ "eb26ic10.bin",	0x200000, 0xa04f3140, 6 | BRF_GRA },           //  7
	{ "eb26_100k.bin",	0x200000, 0x87d5f622, 6 | BRF_GRA },           //  8
	{ "eb26_101.bin",	0x200000, 0x7df654b7, 6 | BRF_GRA },           //  9

	{ "eb26ic12.bin",	0x200000, 0x14561bd7, 7 | BRF_GRA },           // 10 16x16 BG Tiles
	{ "eb26ic36.bin",	0x200000, 0x92019d89, 7 | BRF_GRA },           // 11
};

STD_ROM_PICK(suprslamk)
STD_ROM_FN(suprslamk)

struct BurnDriver BurnDrvSuprslamk = {
	"suprslamk", "suprslam", NULL, NULL, "2023",
	"From TV Animation Slam Dunk - Super Slams (Korean Translation)\0", NULL, "Banpresto / Toei Animation / Video System Co.", "Miscellaneous",
	L"From TV Animation Slam Dunk - Super Slams (Korean Translation)\0\uD504\uB86C TV \uC560\uB2C8\uBA54\uC774\uC158 \uC2AC\uB7A8 \uB369\uD06C - \uC288\uD37C \uC2AC\uB7A8 (\uD55C\uAD6D\uC5B4 \uBC88\uC5ED)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, suprslamkRomInfo, suprslamkRomName, NULL, NULL, NULL, NULL, SuprslamInputInfo, SuprslamkDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 224, 4, 3
};
