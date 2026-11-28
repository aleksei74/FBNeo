// FinalBurn Neo Sega ST-V driver.
// References: MAME 0.289 stv.cpp / saturn_scu.cpp / smpc.cpp;
// Saturn_MiSTer rtl/Saturn/{STV,SMPC_HLE,SCU,VDP1,VDP2}.
// VDP1/VDP2, SCSP and SCU DSP are adapted from the referenced MAME devices.
// Cotton 2 is verified through gameplay; remaining renderer/timing limitations follow MAME.

#include "tiles_generic.h"
#include "sh2_intf.h"
#include "eeprom.h"
#include "burn_gun.h"
#include "m68000_intf.h"
#include "stv_scsp.h"
#include "stv_video.h"
#include "stv_rax.h"
#include "stv_5881.h"
#include "stv_5838.h"
#include <chrono>

// Optional host-time measurements; these never alter emulated cycle counts.
static bool ProfileEnabled;
static double ProfileSamplesMs;
static FILE *ProfilePcmFile;
static INT32 ProfilePcmFrames;
static double ProfileNow()
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static stv_scsp::Core *Sound;
static constexpr INT32 SoundPcmCapacity = 1024;
static INT16 SoundPcm[SoundPcmCapacity][2];
static double SoundGain;
static double SoundDcInput[2], SoundDcOutput[2];
static INT32 SoundEnabled, SoundResetPending, SoundCpuActive, SoundCycles, SoundClock;
static INT32 FrameLines = 263;
static INT32 StvRegion = 1;
static UINT32 CryptKey, CryptRegisters[4];
static stv_5881::Core Crypt;
static UINT16 CryptByteLatch;
static bool CryptFpgaRegisters;
enum StvMahjongMode { NoMahjongPanel, MahjongPanel, MahjongJoystick, MahjongPanelWithJamma };
static INT32 Has5838;
static StvMahjongMode MahjongMode;
static UINT8 MyfairldControls;
static UINT8 MahjongJoy[2][5][8], MahjongInputs[2][5];
static UINT32 CompBank;
static UINT16 CompLatch;
static stv_5838::Core Comp;
static INT32 SoundFrameSamples, SoundSampleRemainder;
static stv_video::Video *Video;
static stv_video::bitmap_rgb32 VideoBitmap;

static UINT8 *AllMem, *MemEnd, *AllRam, *RamEnd;
static UINT8 *DrvBios, *DrvCart, *DrvWorkL, *DrvWorkH;
static UINT8 *DrvSoundRAM;
static UINT8 *DrvBackupRAM, *DrvCache[2];
static UINT8 DrvRecalc, DrvReset, DrvJoy1[8], DrvJoy2[8], DrvJoy3[8], DrvJoy4[8], DrvInputs[4];
static UINT8 Smpc[0x80], SmpcMemory[4], SmpcRtc[7], Ioga[0x20];
static bool HopperTrackballControls, HopperControls;
static bool ShinkansenControls;
static bool TsuriboriControls;
static INT16 TsuriboriRod[4];
static bool SafariControls;
static bool ChoroqhrIndirectDma;
static bool HammerControls;
static bool DanchiDmaTiming;
static INT32 DanchiDmaIrqCycles;
static INT16 HammerAim[2];
static INT16 SkychalTrackball[2];
static UINT16 SkychalCounterBase[2];
static INT32 SkychalHopperPhase;
static UINT8 SkychalHopperSensor;
static UINT32 Scu[0x34];
static UINT8 IntbackRequest[3], IntbackStage, IntbackMode, IntbackContinueBit;
static INT32 IntbackDelay;
struct StvDma {
	UINT32 source, destination, remaining, descriptor;
	UINT32 sourceStep, destinationStep;
	UINT8 active, indirect, last, half;
};
static StvDma Dma[3];

static UINT32 DmaReadLong(UINT32 a)
{
	return (UINT32(Sh2ReadByte(a)) << 24) | (UINT32(Sh2ReadByte(a + 1)) << 16)
		| (Sh2ReadByte(a + 2) << 8) | Sh2ReadByte(a + 3);
}

static void DmaStart(INT32 level)
{
	UINT32 *r = Scu + level * 8;
	if (Dma[level].active || !(r[4] & 0x100)) return;
	StvDma &d = Dma[level];
	d.source = r[0] & 0x07ffffff;
	d.destination = r[1] & 0x07ffffff;
	d.sourceStep = (r[3] & 0x100) ? 4 : 0;
	d.destinationStep = (r[3] & 7) ? (1U << (r[3] & 7)) : 0;
	d.remaining = r[2] & (level ? 0xfff : 0xfffff);
	if (!d.remaining) d.remaining = level ? 0x1000 : 0x100000;
	d.half = ((d.source >> 1) & 1) ^ 1;
	d.indirect = (r[5] >> 24) & 1;
	d.descriptor = d.destination;
	d.last = !d.indirect;
	if (d.indirect) d.remaining = 0;
	d.active = 1;
	Scu[0x7c / 4] |= 0x10 << (level * 4);
}
static INT32 SlaveEnabled, SlaveResetPending, MasterNmiPending, ResetPending;
static INT32 SmpcDelay, CurrentLine, DotSelect, IrqBit, ExtraCycles[2], FrtPending[2];

static struct BurnInputInfo StvInputList[] = {
	{"P1 Coin",       BIT_DIGITAL, DrvJoy3 + 0, "p1 coin"   },
	{"P1 Start",      BIT_DIGITAL, DrvJoy3 + 4, "p1 start"  },
	{"P1 Up",         BIT_DIGITAL, DrvJoy1 + 5, "p1 up"     },
	{"P1 Down",       BIT_DIGITAL, DrvJoy1 + 4, "p1 down"   },
	{"P1 Left",       BIT_DIGITAL, DrvJoy1 + 7, "p1 left"   },
	{"P1 Right",      BIT_DIGITAL, DrvJoy1 + 6, "p1 right"  },
	{"P1 Button 1",   BIT_DIGITAL, DrvJoy1 + 0, "p1 fire 1" },
	{"P1 Button 2",   BIT_DIGITAL, DrvJoy1 + 1, "p1 fire 2" },
	{"P1 Button 3",   BIT_DIGITAL, DrvJoy1 + 2, "p1 fire 3" },
	{"P1 Button 4",   BIT_DIGITAL, DrvJoy1 + 3, "p1 fire 4" },

	{"P2 Coin",       BIT_DIGITAL, DrvJoy3 + 1, "p2 coin"   },
	{"P2 Start",      BIT_DIGITAL, DrvJoy3 + 5, "p2 start"  },
	{"P2 Up",         BIT_DIGITAL, DrvJoy2 + 5, "p2 up"     },
	{"P2 Down",       BIT_DIGITAL, DrvJoy2 + 4, "p2 down"   },
	{"P2 Left",       BIT_DIGITAL, DrvJoy2 + 7, "p2 left"   },
	{"P2 Right",      BIT_DIGITAL, DrvJoy2 + 6, "p2 right"  },
	{"P2 Button 1",   BIT_DIGITAL, DrvJoy2 + 0, "p2 fire 1" },
	{"P2 Button 2",   BIT_DIGITAL, DrvJoy2 + 1, "p2 fire 2" },
	{"P2 Button 3",   BIT_DIGITAL, DrvJoy2 + 2, "p2 fire 3" },
	{"P2 Button 4",   BIT_DIGITAL, DrvJoy2 + 3, "p2 fire 4" },

	{"Reset",         BIT_DIGITAL, &DrvReset,  "reset"     },
	{"Service",       BIT_DIGITAL, DrvJoy3 + 3, "service"   },
	{"Diagnostics",   BIT_DIGITAL, DrvJoy3 + 2, "diag"      },
};

STDINPUTINFO(Stv)

static struct BurnInputInfo CritcrshInputList[] = {
	{ "P1 Coin",       BIT_DIGITAL,    DrvJoy3 + 0,           "p1 coin"      },
	{ "P1 Hammer Hit", BIT_DIGITAL,    DrvJoy3 + 4,           "mouse button 1" },
	{ "P1 Gun X",      BIT_ANALOG_REL, (UINT8*)(HammerAim + 0), "mouse x-axis" },
	{ "P1 Gun Y",      BIT_ANALOG_REL, (UINT8*)(HammerAim + 1), "mouse y-axis" },

	{ "Reset",         BIT_DIGITAL,    &DrvReset,            "reset"        },
	{ "Service",       BIT_DIGITAL,    DrvJoy3 + 2,           "service"      },
	{ "Diagnostics",   BIT_DIGITAL,    DrvJoy3 + 3,           "diag"         },
};

STDINPUTINFO(Critcrsh)

static struct BurnInputInfo BatmanfrInputList[] = {
	{"P1 Coin",       BIT_DIGITAL, DrvJoy3 + 0, "p1 coin"   },
	{"P1 Start",      BIT_DIGITAL, DrvJoy3 + 4, "p1 start"  },
	{"P1 Up",         BIT_DIGITAL, DrvJoy1 + 5, "p1 up"     },
	{"P1 Down",       BIT_DIGITAL, DrvJoy1 + 4, "p1 down"   },
	{"P1 Left",       BIT_DIGITAL, DrvJoy1 + 7, "p1 left"   },
	{"P1 Right",      BIT_DIGITAL, DrvJoy1 + 6, "p1 right"  },
	{"P1 Jump",      BIT_DIGITAL, DrvJoy1 + 1, "p1 fire 1" },
	{"P1 Punch",      BIT_DIGITAL, DrvJoy1 + 2, "p1 fire 2" },
	{"P1 Kick",      BIT_DIGITAL, DrvJoy1 + 3, "p1 fire 3" },

	{"P2 Coin",       BIT_DIGITAL, DrvJoy3 + 1, "p2 coin"   },
	{"P2 Start",      BIT_DIGITAL, DrvJoy3 + 5, "p2 start"  },
	{"P2 Up",         BIT_DIGITAL, DrvJoy2 + 5, "p2 up"     },
	{"P2 Down",       BIT_DIGITAL, DrvJoy2 + 4, "p2 down"   },
	{"P2 Left",       BIT_DIGITAL, DrvJoy2 + 7, "p2 left"   },
	{"P2 Right",      BIT_DIGITAL, DrvJoy2 + 6, "p2 right"  },
	{"P2 Jump",      BIT_DIGITAL, DrvJoy2 + 1, "p2 fire 1" },
	{"P2 Punch",      BIT_DIGITAL, DrvJoy2 + 2, "p2 fire 2" },
	{"P2 Kick",      BIT_DIGITAL, DrvJoy2 + 3, "p2 fire 3" },

	{"Reset",         BIT_DIGITAL, &DrvReset,  "reset"     },
	{"Service",       BIT_DIGITAL, DrvJoy3 + 3, "service"   },
	{"Diagnostics",   BIT_DIGITAL, DrvJoy3 + 2, "diag"      },
};

STDINPUTINFO(Batmanfr)

static struct BurnInputInfo WwshinInputList[] = {
	{"Coin",        BIT_DIGITAL, DrvJoy3 + 0, "p1 coin" },
	{"Accelerator", BIT_DIGITAL, DrvJoy1 + 5, "p1 up"   },
	{"Brake",       BIT_DIGITAL, DrvJoy1 + 4, "p1 down" },
	{"Reset",       BIT_DIGITAL, &DrvReset,   "reset"   },
	{"Service",     BIT_DIGITAL, DrvJoy3 + 3, "service" },
	{"Diagnostics", BIT_DIGITAL, DrvJoy3 + 2, "diag"    },
};

STDINPUTINFO(Wwshin)

static struct BurnInputInfo TsuriborInputList[] = {
	{"P1 Coin",     BIT_DIGITAL,    DrvJoy3 + 0,             "p1 coin"   },
	{"P1 Start",    BIT_DIGITAL,    DrvJoy3 + 4,             "p1 start"  },
	{"P1 Up",       BIT_DIGITAL,    DrvJoy1 + 5,             "p1 up"     },
	{"P1 Down",     BIT_DIGITAL,    DrvJoy1 + 4,             "p1 down"   },
	{"P1 Left",     BIT_DIGITAL,    DrvJoy1 + 7,             "p1 left"   },
	{"P1 Right",    BIT_DIGITAL,    DrvJoy1 + 6,             "p1 right"  },
	{"P1 Button 1", BIT_DIGITAL,    DrvJoy1 + 0,             "p1 fire 1" },
	{"P1 Rod X",    BIT_ANALOG_ABS, (UINT8 *)&TsuriboriRod[0], "p1 x-axis" },
	{"P1 Rod Y",    BIT_ANALOG_ABS, (UINT8 *)&TsuriboriRod[1], "p1 y-axis" },

	{"P2 Coin",     BIT_DIGITAL,    DrvJoy3 + 1,             "p2 coin"   },
	{"P2 Start",    BIT_DIGITAL,    DrvJoy3 + 5,             "p2 start"  },
	{"P2 Up",       BIT_DIGITAL,    DrvJoy2 + 5,             "p2 up"     },
	{"P2 Down",     BIT_DIGITAL,    DrvJoy2 + 4,             "p2 down"   },
	{"P2 Left",     BIT_DIGITAL,    DrvJoy2 + 7,             "p2 left"   },
	{"P2 Right",    BIT_DIGITAL,    DrvJoy2 + 6,             "p2 right"  },
	{"P2 Button 1", BIT_DIGITAL,    DrvJoy2 + 0,             "p2 fire 1" },
	{"P2 Rod X",    BIT_ANALOG_ABS, (UINT8 *)&TsuriboriRod[2], "p2 x-axis" },
	{"P2 Rod Y",    BIT_ANALOG_ABS, (UINT8 *)&TsuriboriRod[3], "p2 y-axis" },

	{"Reset",       BIT_DIGITAL,    &DrvReset,              "reset"     },
	{"Service",     BIT_DIGITAL,    DrvJoy3 + 3,             "service"   },
	{"Diagnostics", BIT_DIGITAL,    DrvJoy3 + 2,             "diag"      },
};

STDINPUTINFO(Tsuribor)

static struct BurnInputInfo WasafariInputList[] = {
	{"Coin",        BIT_DIGITAL, DrvJoy3 + 0, "p1 coin"   },
	{"Button 1",    BIT_DIGITAL, DrvJoy1 + 0, "p1 fire 1" },
	{"Button 2",    BIT_DIGITAL, DrvJoy1 + 1, "p1 fire 2" },
	{"Winch",       BIT_DIGITAL, DrvJoy1 + 2, "p1 fire 3" },
	{"Reset",       BIT_DIGITAL, &DrvReset,   "reset"     },
	{"Service",     BIT_DIGITAL, DrvJoy3 + 3, "service"   },
	{"Diagnostics", BIT_DIGITAL, DrvJoy3 + 2, "diag"      },
};

STDINPUTINFO(Wasafari)

static struct BurnInputInfo HopperTrackballInputList[] = {
	{"Medal",          BIT_DIGITAL, DrvJoy2 + 5, "p1 coin"   },
	{"10 Yen",         BIT_DIGITAL, DrvJoy3 + 0, "p2 coin"   },
	{"100 Yen",        BIT_DIGITAL, DrvJoy3 + 1, "p3 coin"   },
	{"Select",         BIT_DIGITAL, DrvJoy3 + 4, "p1 start"  },
	{"Power",          BIT_DIGITAL, DrvJoy3 + 5, "p1 fire 1" },
	{"Trackball X",    BIT_ANALOG_REL, (UINT8 *)&SkychalTrackball[0], "p1 x-axis" },
	{"Trackball Y",    BIT_ANALOG_REL, (UINT8 *)&SkychalTrackball[1], "p1 y-axis" },
	{"P1 Up",          BIT_DIGITAL, DrvJoy1 + 5, "p1 up"     },
	{"P1 Down",        BIT_DIGITAL, DrvJoy1 + 4, "p1 down"   },
	{"P1 Left",        BIT_DIGITAL, DrvJoy1 + 7, "p1 left"   },
	{"P1 Right",       BIT_DIGITAL, DrvJoy1 + 6, "p1 right"  },
	{"Hopper Switch",  BIT_DIGITAL, DrvJoy1 + 0, "p1 fire 2" },
	{"Cabinet Switch", BIT_DIGITAL, DrvJoy2 + 0, "p1 fire 3" },
	{"Door Switch",    BIT_DIGITAL, DrvJoy2 + 1, "p1 fire 4" },
	{"Cabinet Input",  BIT_DIGITAL, DrvJoy2 + 2, "p1 fire 5" },
	{"Reset",          BIT_DIGITAL, &DrvReset,   "reset"     },
	{"Service",        BIT_DIGITAL, DrvJoy3 + 3, "service"   },
	{"Diagnostics",    BIT_DIGITAL, DrvJoy3 + 2, "diag"      },
};

STDINPUTINFO(HopperTrackball)

static struct BurnInputInfo MicrombcInputList[] = {
	{"Medal",          BIT_DIGITAL, DrvJoy2 + 5, "p1 coin"   },
	{"10 Yen",         BIT_DIGITAL, DrvJoy3 + 0, "p2 coin"   },
	{"100 Yen",        BIT_DIGITAL, DrvJoy3 + 1, "p3 coin"   },
	{"Shoot",          BIT_DIGITAL, DrvJoy3 + 4, "p1 start"  },
	{"Power",          BIT_DIGITAL, DrvJoy1 + 5, "p1 fire 1" },
	{"Hopper Switch",  BIT_DIGITAL, DrvJoy1 + 0, "p1 fire 2" },
	{"Cabinet Switch", BIT_DIGITAL, DrvJoy2 + 0, "p1 fire 3" },
	{"Door Switch",    BIT_DIGITAL, DrvJoy2 + 1, "p1 fire 4" },
	{"Magnet Sensor",  BIT_DIGITAL, DrvJoy2 + 2, "p1 fire 5" },
	{"Reset",          BIT_DIGITAL, &DrvReset,   "reset"     },
	{"Service",        BIT_DIGITAL, DrvJoy3 + 3, "service"   },
	{"Diagnostics",    BIT_DIGITAL, DrvJoy3 + 2, "diag"      },
};

STDINPUTINFO(Micrombc)

static struct BurnInputInfo Stv6bInputList[] = {
	{"P1 Coin",       BIT_DIGITAL, DrvJoy3 + 0, "p1 coin"   },
	{"P1 Start",      BIT_DIGITAL, DrvJoy3 + 4, "p1 start"  },
	{"P1 Up",         BIT_DIGITAL, DrvJoy1 + 5, "p1 up"     },
	{"P1 Down",       BIT_DIGITAL, DrvJoy1 + 4, "p1 down"   },
	{"P1 Left",       BIT_DIGITAL, DrvJoy1 + 7, "p1 left"   },
	{"P1 Right",      BIT_DIGITAL, DrvJoy1 + 6, "p1 right"  },
	{"P1 Button 1",   BIT_DIGITAL, DrvJoy1 + 0, "p1 fire 1" },
	{"P1 Button 2",   BIT_DIGITAL, DrvJoy1 + 1, "p1 fire 2" },
	{"P1 Button 3",   BIT_DIGITAL, DrvJoy1 + 2, "p1 fire 3" },
	{"P1 Button 4",   BIT_DIGITAL, DrvJoy4 + 0, "p1 fire 4" },
	{"P1 Button 5",   BIT_DIGITAL, DrvJoy4 + 1, "p1 fire 5" },
	{"P1 Button 6",   BIT_DIGITAL, DrvJoy4 + 2, "p1 fire 6" },

	{"P2 Coin",       BIT_DIGITAL, DrvJoy3 + 1, "p2 coin"   },
	{"P2 Start",      BIT_DIGITAL, DrvJoy3 + 5, "p2 start"  },
	{"P2 Up",         BIT_DIGITAL, DrvJoy2 + 5, "p2 up"     },
	{"P2 Down",       BIT_DIGITAL, DrvJoy2 + 4, "p2 down"   },
	{"P2 Left",       BIT_DIGITAL, DrvJoy2 + 7, "p2 left"   },
	{"P2 Right",      BIT_DIGITAL, DrvJoy2 + 6, "p2 right"  },
	{"P2 Button 1",   BIT_DIGITAL, DrvJoy2 + 0, "p2 fire 1" },
	{"P2 Button 2",   BIT_DIGITAL, DrvJoy2 + 1, "p2 fire 2" },
	{"P2 Button 3",   BIT_DIGITAL, DrvJoy2 + 2, "p2 fire 3" },
	{"P2 Button 4",   BIT_DIGITAL, DrvJoy4 + 4, "p2 fire 4" },
	{"P2 Button 5",   BIT_DIGITAL, DrvJoy4 + 5, "p2 fire 5" },
	{"P2 Button 6",   BIT_DIGITAL, DrvJoy4 + 6, "p2 fire 6" },

	{"Reset",         BIT_DIGITAL, &DrvReset,  "reset"     },
	{"Service",       BIT_DIGITAL, DrvJoy3 + 3, "service"   },
	{"Diagnostics",   BIT_DIGITAL, DrvJoy3 + 2, "diag"      },
};

STDINPUTINFO(Stv6b)

#define STV_MAHJONG_P1_INPUTS(prefix, start) \
	{"P1 Coin", BIT_DIGITAL, DrvJoy3 + 0, "p1 coin" }, \
	{"P1 Kan", BIT_DIGITAL, &MahjongJoy[0][0][0], prefix "mah kan" }, \
	{"P1 Start", BIT_DIGITAL, start, "p1 start" }, \
	{"P1 E", BIT_DIGITAL, &MahjongJoy[0][0][4], prefix "mah e" }, \
	{"P1 A", BIT_DIGITAL, &MahjongJoy[0][0][5], prefix "mah a" }, \
	{"P1 M", BIT_DIGITAL, &MahjongJoy[0][0][6], prefix "mah m" }, \
	{"P1 I", BIT_DIGITAL, &MahjongJoy[0][0][7], prefix "mah i" }, \
	{"P1 Reach", BIT_DIGITAL, &MahjongJoy[0][1][0], prefix "mah reach" }, \
	{"P1 F", BIT_DIGITAL, &MahjongJoy[0][1][4], prefix "mah f" }, \
	{"P1 B", BIT_DIGITAL, &MahjongJoy[0][1][5], prefix "mah b" }, \
	{"P1 N", BIT_DIGITAL, &MahjongJoy[0][1][6], prefix "mah n" }, \
	{"P1 J", BIT_DIGITAL, &MahjongJoy[0][1][7], prefix "mah j" }, \
	{"P1 Ron", BIT_DIGITAL, &MahjongJoy[0][2][0], prefix "mah ron" }, \
	{"P1 G", BIT_DIGITAL, &MahjongJoy[0][2][4], prefix "mah g" }, \
	{"P1 C", BIT_DIGITAL, &MahjongJoy[0][2][5], prefix "mah c" }, \
	{"P1 Chi", BIT_DIGITAL, &MahjongJoy[0][2][6], prefix "mah chi" }, \
	{"P1 K", BIT_DIGITAL, &MahjongJoy[0][2][7], prefix "mah k" }, \
	{"P1 H", BIT_DIGITAL, &MahjongJoy[0][3][4], prefix "mah h" }, \
	{"P1 D", BIT_DIGITAL, &MahjongJoy[0][3][5], prefix "mah d" }, \
	{"P1 Pon", BIT_DIGITAL, &MahjongJoy[0][3][6], prefix "mah pon" }, \
	{"P1 L", BIT_DIGITAL, &MahjongJoy[0][3][7], prefix "mah l" },

static struct BurnInputInfo StvMahjongInputList[] = {
	STV_MAHJONG_P1_INPUTS("p1 ", &MahjongJoy[0][0][1])
	{"P1 Flip Flop", BIT_DIGITAL, &MahjongJoy[0][4][6], "p1 mah flipflop" },

	{"P2 Coin", BIT_DIGITAL, DrvJoy3 + 1, "p2 coin" },
	{"P2 Kan", BIT_DIGITAL, &MahjongJoy[1][0][0], "p2 mah kan" },
	{"P2 Start", BIT_DIGITAL, &MahjongJoy[1][0][1], "p2 start" },
	{"P2 E", BIT_DIGITAL, &MahjongJoy[1][0][4], "p2 mah e" },
	{"P2 A", BIT_DIGITAL, &MahjongJoy[1][0][5], "p2 mah a" },
	{"P2 M", BIT_DIGITAL, &MahjongJoy[1][0][6], "p2 mah m" },
	{"P2 I", BIT_DIGITAL, &MahjongJoy[1][0][7], "p2 mah i" },
	{"P2 Reach", BIT_DIGITAL, &MahjongJoy[1][1][0], "p2 mah reach" },
	{"P2 F", BIT_DIGITAL, &MahjongJoy[1][1][4], "p2 mah f" },
	{"P2 B", BIT_DIGITAL, &MahjongJoy[1][1][5], "p2 mah b" },
	{"P2 N", BIT_DIGITAL, &MahjongJoy[1][1][6], "p2 mah n" },
	{"P2 J", BIT_DIGITAL, &MahjongJoy[1][1][7], "p2 mah j" },
	{"P2 Ron", BIT_DIGITAL, &MahjongJoy[1][2][0], "p2 mah ron" },
	{"P2 G", BIT_DIGITAL, &MahjongJoy[1][2][4], "p2 mah g" },
	{"P2 C", BIT_DIGITAL, &MahjongJoy[1][2][5], "p2 mah c" },
	{"P2 Chi", BIT_DIGITAL, &MahjongJoy[1][2][6], "p2 mah chi" },
	{"P2 K", BIT_DIGITAL, &MahjongJoy[1][2][7], "p2 mah k" },
	{"P2 H", BIT_DIGITAL, &MahjongJoy[1][3][4], "p2 mah h" },
	{"P2 D", BIT_DIGITAL, &MahjongJoy[1][3][5], "p2 mah d" },
	{"P2 Pon", BIT_DIGITAL, &MahjongJoy[1][3][6], "p2 mah pon" },
	{"P2 L", BIT_DIGITAL, &MahjongJoy[1][3][7], "p2 mah l" },
	{"P2 Flip Flop", BIT_DIGITAL, &MahjongJoy[1][4][6], "p2 mah flipflop" },

	{"Reset", BIT_DIGITAL, &DrvReset, "reset" },
	{"Service", BIT_DIGITAL, DrvJoy3 + 3, "service" },
	{"Diagnostics", BIT_DIGITAL, DrvJoy3 + 2, "diag" },

	{"P1 Up", BIT_DIGITAL, DrvJoy1 + 5, "p1 up" },
	{"P1 Down", BIT_DIGITAL, DrvJoy1 + 4, "p1 down" },
	{"P1 Left", BIT_DIGITAL, DrvJoy1 + 7, "p1 left" },
	{"P1 Right", BIT_DIGITAL, DrvJoy1 + 6, "p1 right" },
	{"P1 Button 1", BIT_DIGITAL, DrvJoy1 + 0, "p1 fire 1" },
	{"P1 Button 2", BIT_DIGITAL, DrvJoy1 + 1, "p1 fire 2" },
	{"P1 Button 3", BIT_DIGITAL, DrvJoy1 + 2, "p1 fire 3" },
	{"P1 Button 4", BIT_DIGITAL, DrvJoy1 + 3, "p1 fire 4" },
	{"P2 Up", BIT_DIGITAL, DrvJoy2 + 5, "p2 up" },
	{"P2 Down", BIT_DIGITAL, DrvJoy2 + 4, "p2 down" },
	{"P2 Left", BIT_DIGITAL, DrvJoy2 + 7, "p2 left" },
	{"P2 Right", BIT_DIGITAL, DrvJoy2 + 6, "p2 right" },
	{"P2 Button 1", BIT_DIGITAL, DrvJoy2 + 0, "p2 fire 1" },
	{"P2 Button 2", BIT_DIGITAL, DrvJoy2 + 1, "p2 fire 2" },
	{"P2 Button 3", BIT_DIGITAL, DrvJoy2 + 2, "p2 fire 3" },
	{"P2 Button 4", BIT_DIGITAL, DrvJoy2 + 3, "p2 fire 4" },
};

STDINPUTINFO(StvMahjong)

static struct BurnInputInfo VmahjongInputList[] = {
	STV_MAHJONG_P1_INPUTS("", &MahjongJoy[0][0][1])
	{"P1 Bet", BIT_DIGITAL, &MahjongJoy[0][1][1], "mah bet" },
	{"P2 Coin", BIT_DIGITAL, DrvJoy3 + 1, "p2 coin" },
	{"Reset", BIT_DIGITAL, &DrvReset, "reset" },
	{"Service", BIT_DIGITAL, DrvJoy3 + 3, "service" },
	{"Diagnostics", BIT_DIGITAL, DrvJoy3 + 2, "diag" },
};

STDINPUTINFO(Vmahjong)

static struct BurnInputInfo MyfairldInputList[] = {
	STV_MAHJONG_P1_INPUTS("", DrvJoy3 + 4)
	{"P1 Bet",      BIT_DIGITAL, &MahjongJoy[0][1][1], "mah bet"  },
	{"P2 Coin",     BIT_DIGITAL, DrvJoy3 + 1, "p2 coin"          },
	{"P1 Up",       BIT_DIGITAL, DrvJoy1 + 5, "p1 up"            },
	{"P1 Down",     BIT_DIGITAL, DrvJoy1 + 4, "p1 down"          },
	{"P1 Left",     BIT_DIGITAL, DrvJoy1 + 7, "p1 left"          },
	{"P1 Right",    BIT_DIGITAL, DrvJoy1 + 6, "p1 right"         },
	{"P1 Button 1", BIT_DIGITAL, DrvJoy1 + 0, "p1 fire 1"        },
	{"P1 Button 2", BIT_DIGITAL, DrvJoy1 + 1, "p1 fire 2"        },
	{"Reset",       BIT_DIGITAL, &DrvReset,   "reset"            },
	{"Service",     BIT_DIGITAL, DrvJoy3 + 3, "service"          },
	{"Diagnostics", BIT_DIGITAL, DrvJoy3 + 2, "diag"             },
	{"Dip",         BIT_DIPSWITCH, &MyfairldControls, "dip"      },
};

STDINPUTINFO(Myfairld)

static struct BurnDIPInfo MyfairldDIPList[] = {
	DIP_OFFSET(sizeof(MyfairldInputList) / sizeof(MyfairldInputList[0]) - 1)
	{0x00, 0xff, 0x01, 0x01, NULL},

	{0x00, 0xfe, 0x00, 2, "I/O Device Type"},
	{0x00, 0x01, 0x01, 0x01, "Joystick"},
	{0x00, 0x01, 0x01, 0x00, "Mahjong Panel"},
};

STDDIPINFO(Myfairld)

#undef STV_MAHJONG_P1_INPUTS

// SH-2 mapped memory is stored in host-native 32-bit words.
static UINT8 MemByte(const UINT8 *p, UINT32 a)
{
#if defined LSB_FIRST
	return p[a ^ 3];
#else
	return p[a];
#endif
}

static void MemWrite(UINT8 *p, UINT32 a, UINT8 d)
{
#if defined LSB_FIRST
	p[a ^ 3] = d;
#else
	p[a] = d;
#endif
}

static UINT16 MemWord(const UINT8 *p, UINT32 a)
{
	return (MemByte(p, a) << 8) | MemByte(p, a + 1);
}

static void ScuRequest(INT32 bit)
{
	Scu[0xa4 / 4] |= 1U << bit;
	INT32 factor = (bit <= 4) ? bit : ((bit == 6) ? 5 : ((bit == 13) ? 6 : -1));
	if (factor >= 0) {
		for (INT32 i = 0; i < 3; i++) {
			if ((Scu[i * 8 + 5] & 7) == UINT32(factor)) DmaStart(i);
		}
	}
}

#include "stv_scudsp.h"
static stv_dsp::Core ScuDsp;

static const INT32 ScuIrqLevels[14] = {15, 14, 13, 12, 11, 10, 9, 8, 8, 6, 6, 5, 3, 2};

static INT32 StvIrqCallback(INT32 level)
{
	// SCU INT_SET/IVECF: the selected vector remains latched until acknowledge,
	// even when software changes IMS or clears IST before the CPU accepts it.
	INT32 bit = IrqBit;
	if (bit >= 0 && ScuIrqLevels[bit] == level) {
		IrqBit = -1;
		return 0x40 + bit;
	}
	return 0x40;
}

// Event-triggered transfers advance in bounded slices.
static void DmaRunChannel(INT32 level, INT32 budget, bool deferIrq = false)
{
	StvDma &d = Dma[level];
	UINT32 *r = Scu + level * 8;
	while (budget-- > 0 && d.active) {
		if (!d.remaining && d.indirect) {
			UINT32 source = DmaReadLong(d.descriptor + 8);
			d.remaining = DmaReadLong(d.descriptor) & (level ? 0x3ffff : 0xfffff);
			d.destination = DmaReadLong(d.descriptor + 4) & 0x07ffffff;
			d.source = source & 0x07ffffff;
			d.last = source >> 31;
			d.descriptor += 12;
			if (!d.remaining) d.remaining = level ? 0x2000 : 0x100000;
			d.half = ((d.source >> 1) & 1) ^ 1;
		}
		if ((d.source & 0x07f00000) == 0) {
			d.active = 0;
			Scu[0x7c / 4] &= ~(0x10 << (level * 4));
			ScuRequest(12);
			break;
		}
		UINT32 source = (d.source & ~3U) + (d.half ? 0 : 2) + (d.source & 1);
		UINT8 hi = Sh2ReadByte(source), lo = Sh2ReadByte(source + 1);
		Sh2WriteByte(d.destination, hi);
		Sh2WriteByte(d.destination + 1, lo);
		d.half ^= 1;
		if (d.half) d.source += d.sourceStep;
		d.destination += ((d.destination & 0x07000000) == 0x06000000) ? 2 : d.destinationStep;
		d.remaining = (d.remaining > 2) ? d.remaining - 2 : 0;
		if (!d.remaining && d.last) {
			d.active = 0;
			if (!d.indirect) {
				if (r[5] & 0x10000) r[0] = d.source;
				if (r[5] & 0x100) r[1] = d.destination;
			}
			Scu[0x7c / 4] &= ~(0x10 << (level * 4));
			if (!deferIrq) ScuRequest(11 - level);
		}
	}
}

static void DmaRun()
{
	for (INT32 level = 0; level < 3; level++) DmaRunChannel(level, 256);
}

// Called with the master CPU open; never changes CPUs inside a memory callback.
static void ScuUpdateIrq()
{
	// MiSTer SCU.sv transfers IST into a latched request; it does not withdraw
	// that request at the next scanline. A new event remains pending in IST.
	if (IrqBit >= 0) return;
	for (INT32 i = 0; i < 14; i++) {
		if (Scu[0xa4 / 4] & ~Scu[0xa0 / 4] & (1U << i)) {
			IrqBit = i;
			Scu[0xa4 / 4] &= ~(1U << i);
			Sh2SetIRQLine(ScuIrqLevels[i], CPU_IRQSTATUS_HOLD);
			return;
		}
	}
}

static void SmpcContinue()
{
	if (IntbackStage == 1) {
		// ST-V JAMMA inputs are on IOGA; neither Saturn controller port is populated.
		Smpc[0x21] = Smpc[0x23] = 0xf0;
	}
	if (IntbackStage == 2) { Smpc[0x61] = 0x80 | IntbackMode; IntbackStage = 0; }
	else { Smpc[0x61] = 0xc0 | IntbackMode; IntbackStage++; }
	Smpc[0x5f] = 0x10;
	Smpc[0x63] = 0;
	ScuRequest(7);
}

static bool SmpcClockHalted()
{
	const UINT8 command = Smpc[0x1f] & 0x1f;
	return SmpcDelay && (command == 0x0e || command == 0x0f);
}

static void SmpcComplete()
{
	UINT8 cmd = Smpc[0x1f] & 0x1f;
	switch (cmd) {
		case 0x02:
			SlaveEnabled = 1;
			SlaveResetPending = 1;
			break;
		case 0x03:
			SlaveEnabled = 0;
			break;
		case 0x06:
			SoundEnabled = SoundResetPending = 1;
			break;
		case 0x07:
			SoundEnabled = 0;
			break;
		case 0x0d:
			ResetPending = 1;
			break;
		case 0x0e:
		case 0x0f:
			DotSelect = (cmd & 1) ^ 1;
			SlaveEnabled = 0;
			MasterNmiPending = 1;
			break;
		case 0x10:
			IntbackStage = (IntbackRequest[1] & 8) ? 1 : 0;
			IntbackMode = IntbackRequest[0] >> 4;
			IntbackContinueBit = 0;
			if (!IntbackRequest[0]) {
				if (IntbackStage) SmpcContinue();
				break;
			}
			Smpc[0x21] = 0x80;
			for (INT32 i = 0; i < 7; i++) Smpc[0x23 + i * 2] = SmpcRtc[i];
			Smpc[0x31] = 0;
			Smpc[0x33] = StvRegion; // Region must agree with the selected BIOS.
			Smpc[0x35] = 0x34 | (DotSelect << 6);
			Smpc[0x37] = 0;
			for (INT32 i = 0; i < 4; i++) Smpc[0x39 + i * 2] = SmpcMemory[i];
			for (INT32 i = 16; i < 31; i++) Smpc[0x21 + i * 2] = 0xff;
			Smpc[0x61] = 0x40 | (IntbackStage << 5);
			ScuRequest(7);
			break;
		case 0x16:
			for (INT32 i = 0; i < 7; i++) SmpcRtc[i] = Smpc[1 + i * 2];
			break;
		case 0x17:
			for (INT32 i = 0; i < 4; i++) SmpcMemory[i] = Smpc[1 + i * 2];
			break;
		case 0x18:
			MasterNmiPending = 1;
			break;
	}
	Smpc[0x5f] = cmd;
	Smpc[0x63] = 0;
}

// Video registers are 16-bit; VRAM and framebuffer buses are 32-bit.
static INT32 SoundPosition()
{
	if (SoundCpuActive) return SekTotalCycles();
	if (Sh2GetActive() >= 0) return INT32(INT64(Sh2TotalCycles()) * 11289600 / SoundClock);
	return SoundCycles;
}

static void SoundUpdateIrq()
{
	// Acknowledge/enable writes must reach the running 68000 immediately.
	// Leaving the previous level asserted until the next scanline can retrigger
	// an acknowledged interrupt when the handler executes RTE.
	if (SoundCpuActive) {
		INT32 level = Sound->m_irq_cb.level;
		SekSetIRQLine(level, level ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	}
}

// One SCSP sample per 256 sound CPU clocks, even with the host audio disabled.
static void SoundSync()
{
	const INT32 target = (SoundPosition() + SoundSampleRemainder) / 256;
	const double start = ProfileEnabled && SoundFrameSamples < target ? ProfileNow() : 0;
	while (SoundFrameSamples < target) {
		Sound->sample();
		if (SoundFrameSamples < SoundPcmCapacity) {
			SoundPcm[SoundFrameSamples][0] = Sound->stream.left;
			SoundPcm[SoundFrameSamples][1] = Sound->stream.right;
		}
		SoundFrameSamples++;
	}
	if (start) ProfileSamplesMs += ProfileNow() - start;
	SoundUpdateIrq();
}

static void SoundRunTo(INT32 target)
{
	if (target <= SoundCycles && !SoundResetPending) return;
	SekOpen(0);
	SoundCpuActive = 1;
	if (SoundResetPending) { SekReset(); SoundResetPending = 0; }
	SekSetIRQLine(Sound->m_irq_cb.level, Sound->m_irq_cb.level ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	// SCSP advances at 44.1 kHz (256 sound-CPU clocks per sample).
	// Deliver timer IRQs at sample boundaries, rather than delaying them
	// until the end of the scanline. Register accesses may sync sooner.
	while (SekTotalCycles() < target) {
		INT32 now = SekTotalCycles();
		INT32 next = std::min(target, ((now + SoundSampleRemainder) / 256 + 1) * 256 - SoundSampleRemainder);
		if (SoundEnabled && !SmpcClockHalted()) SekRun(next - now); else SekIdle(next - now);
		SoundSync();
	}
	SoundCycles = SekTotalCycles();
	SoundCpuActive = 0;
	SekClose();
}

// An SH-2 sound bus access must not generate PCM ahead of the 68000.
static void SoundSyncBus()
{
	if (!SoundCpuActive && Sh2GetActive() >= 0) SoundRunTo(SoundPosition());
	SoundSync();
}

static void SoundRender()
{
	if (!pBurnSoundOut || nBurnSoundLen <= 0) return;
	const INT32 count = std::min(SoundPcmCapacity, SoundFrameSamples);
	if (count == 0) {
		memset(pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));
		return;
	}
	for (INT32 i = 0; i < nBurnSoundLen; i++) {
		const INT32 position = (INT64(i) * count * 0x10000) / nBurnSoundLen;
		const INT32 sample = position >> 16;
		const INT32 fraction = position & 0xffff;
		const INT32 next = std::min(sample + 1, count - 1);
		for (INT32 channel = 0; channel < 2; channel++) {
			const INT32 value = (INT32(SoundPcm[sample][channel]) * (0x10000 - fraction) +
				INT32(SoundPcm[next][channel]) * fraction) >> 16;
			pBurnSoundOut[i * 2 + channel] = BURN_SND_CLIP(INT32(value * SoundGain));
		}
	}
}

static UINT16 __fastcall SoundReadWord(UINT32 a)
{
	if (a < 0x100000) return MemWord(DrvSoundRAM, a & 0xffffe);
	if ((a & 0xfff000) == 0x100000) { SoundSyncBus(); return Sound->read((a & 0xfff) / 2); }
	return 0xffff;
}

static UINT8 __fastcall SoundReadByte(UINT32 a)
{
	return SoundReadWord(a & ~1U) >> ((~a & 1) * 8);
}

static void __fastcall SoundWriteWord(UINT32 a, UINT16 d)
{
	SoundSyncBus();
	if (a < 0x100000) { MemWrite(DrvSoundRAM, a, d >> 8); MemWrite(DrvSoundRAM, a + 1, d); }
	else if ((a & 0xfff000) == 0x100000) Sound->write((a & 0xfff) / 2, d);
	SoundUpdateIrq();
}

static void __fastcall SoundWriteByte(UINT32 a, UINT8 d)
{
	SoundSyncBus();
	if (a < 0x100000) MemWrite(DrvSoundRAM, a, d);
	else if ((a & 0xfff000) == 0x100000) Sound->write((a & 0xfff) / 2, UINT16(d) << ((~a & 1) * 8), UINT16(0xff) << ((~a & 1) * 8));
	SoundUpdateIrq();
}

static UINT32 VideoRead(UINT32 a, INT32 bytes)
{
	if (Sh2GetActive() >= 0) {
		INT32 lineCycles = INT64(SoundClock) * 100 / (nBurnFPS * FrameLines);
		Video->screen.horizontal = INT64(Sh2TotalCycles() % lineCycles) * Video->get_hblank_duration() / lineCycles;
	}
	UINT32 value;
	INT32 bus = 4;
	if (a >= 0x05c00000 && a < 0x05c80000) value = Video->vdp1_vram_r((a & 0x7ffff) / 4);
	else if (a >= 0x05c80000 && a < 0x05d00000) value = Video->vdp1_framebuffer0_r((a & 0x3ffff) / 4);
	else if (a >= 0x05d00000 && a < 0x05d40000) { value = Video->vdp1_regs_r((a & 0x1f) / 2); bus = 2; }
	else if (a >= 0x05e00000 && a < 0x05f00000) value = Video->vdp2_vram_r((a & 0x7ffff) / 4);
	else if (a >= 0x05f00000 && a < 0x05f80000) value = Video->vdp2_cram_r((a & 0xfff) / 4);
	else if (a >= 0x05f80000 && a < 0x05fc0000) { value = Video->vdp2_regs_r((a & 0x1ff) / 2); bus = 2; }
	else return ~0U;
	if (bytes == 4 && bus == 2) return (value << 16) | VideoRead(a + 2, 2);
	return value >> ((bus - bytes - (a & (bus - 1))) * 8);
}

static void VideoWrite(UINT32 a, UINT32 data, INT32 bytes)
{
	INT32 bus = ((a >= 0x05d00000 && a < 0x05d40000) || a >= 0x05f80000) ? 2 : 4;
	if (bytes == 4 && bus == 2) { VideoWrite(a, data >> 16, 2); VideoWrite(a + 2, data, 2); return; }
	INT32 shift = (bus - bytes - (a & (bus - 1))) * 8;
	UINT32 mask = bytes == 4 ? ~0U : ((1U << (bytes * 8)) - 1) << shift;
	data <<= shift;
	if (a < 0x05c80000) Video->vdp1_vram_w((a & 0x7ffff) / 4, data, mask);
	else if (a < 0x05d00000) Video->vdp1_framebuffer0_w((a & 0x3ffff) / 4, data, mask);
	else if (a < 0x05d40000) Video->vdp1_regs_w((a & 0x1f) / 2, data, mask);
	else if (a >= 0x05e00000 && a < 0x05f00000) Video->vdp2_vram_w((a & 0x7ffff) / 4, data, mask);
	else if (a >= 0x05f00000 && a < 0x05f80000) Video->vdp2_cram_w((a & 0xfff) / 4, data, mask);
	else if (a >= 0x05f80000) Video->vdp2_regs_w((a & 0x1ff) / 2, data, mask);
}

static UINT32 DspRead(UINT32 address, INT32 bytes)
{
	UINT32 data = ~0U;
	if ((address & 0xc) == 0) data = ScuDsp.program_control_r();
	if ((address & 0xc) == 0xc) data = ScuDsp.ram_address_r();
	return data >> ((4 - bytes - (address & 3)) * 8);
}

static void DspWrite(UINT32 address, UINT32 data, INT32 bytes)
{
	INT32 shift = (4 - bytes - (address & 3)) * 8;
	UINT32 mask = bytes == 4 ? ~0U : ((1U << (bytes * 8)) - 1) << shift;
	data <<= shift;
	switch (address & 0xc) {
		case 0: ScuDsp.program_control_w(0, data, mask); break;
		case 4: ScuDsp.program_w(data); break;
		case 8: ScuDsp.ram_address_control_w(data); break;
		case 12: ScuDsp.ram_address_w(data); break;
	}
}

static uint16_t stv_5838::Read(uint32_t address)
{
	// MAME's little-endian ROM interface uses XOR 2 inside the device.
	return MemWord(DrvCart, CompBank + ((address ^ 2) & 0x7fffff));
}

static UINT32 CompRead(UINT32 address, INT32 bytes)
{
	if (bytes == 4) { UINT32 high = Comp.data_r(); return (high << 16) | Comp.data_r(); }
	if (bytes == 2) return Comp.data_r();
	if (!(address & 1)) CompLatch = Comp.data_r();
	return (address & 1) ? (CompLatch & 0xff) : (CompLatch >> 8);
}

static void CompWrite(UINT32 address, UINT32 data, INT32 bytes)
{
	CompBank = (address - 0x02000000) & 0x1800000;
	INT32 shift = (4 - bytes - (address & 3)) * 8;
	UINT32 mask = bytes == 4 ? ~0U : ((1U << (bytes * 8)) - 1) << shift;
	data <<= shift;
	if ((address & 0x7ffffc) == 0x7ffff0) Comp.address_w(data, mask);
	if ((address & 0x7ffffc) == 0x7ffff4) {
		if (mask & 0xffff0000) Comp.mode_w(data >> 16);
		if (mask & 0x0000ffff) Comp.table_w(data);
	}
}

// The protection cartridge replaces the final 16 bytes of A-bus ROM.
// Halfword/longword accesses must consume exactly one/two stream words.
static uint16_t stv_5881::Read(uint32_t address)
{
	UINT32 a = address * 2;
	if (a + 1 >= 0x3000000) return 0xffff;
	UINT16 data = MemWord(DrvCart, a);
	return (data >> 8) | (data << 8);
}

static UINT16 CryptNext()
{
	uint8_t *base;
	UINT16 data = Crypt.do_decrypt(base);
	return (data >> 8) | (data << 8);
}

// MiSTer exposes the running ROM word address and one stream word per bus read.
static UINT16 CryptFpgaReadWord(UINT32 address)
{
	switch (address & 0xe) {
		case 0x8: return Crypt.prot_cur_address & 0xffff;
		case 0xa: return (Crypt.prot_cur_address >> 16) & 0x01ff;
		case 0xc:
		case 0xe:
			if (CryptRegisters[0] & 0x10000) CryptByteLatch = CryptNext();
			return CryptByteLatch;
		default: return 0;
	}
}

static UINT32 CryptRead(UINT32 address, INT32 bytes)
{
	if (CryptFpgaRegisters) {
		if (bytes == 4) {
			const UINT32 high = CryptFpgaReadWord(address);
			return (high << 16) | CryptFpgaReadWord(address + 2);
		}
		if (bytes == 2) return CryptFpgaReadWord(address);
		if ((address & 0xc) == 0xc && (address & 1)) return CryptByteLatch & 0xff;
		return (CryptFpgaReadWord(address) >> ((~address & 1) * 8)) & 0xff;
	}
	if ((address & 0xc) == 0xc && (CryptRegisters[0] & 0x10000)) {
		if (bytes == 4) { UINT32 high = CryptNext(); return (high << 16) | CryptNext(); }
		if (bytes == 2) return CryptNext();
		if (!(address & 1)) CryptByteLatch = CryptNext();
		return (address & 1) ? (CryptByteLatch & 0xff) : (CryptByteLatch >> 8);
	}
	UINT32 data = CryptRegisters[(address & 0xf) >> 2];
	if (!(CryptRegisters[0] & 0x10000) && !data) {
		UINT32 a = (address - 0x02000000) & ~3U;
		data = (UINT32(MemWord(DrvCart, a)) << 16) | MemWord(DrvCart, a + 2);
	}
	return data >> ((4 - bytes - (address & 3)) * 8);
}

static void CryptWrite(UINT32 address, UINT32 data, INT32 bytes)
{
	INT32 shift = (4 - bytes - (address & 3)) * 8;
	UINT32 mask = bytes == 4 ? ~0U : ((1U << (bytes * 8)) - 1) << shift;
	UINT32 &reg = CryptRegisters[(address & 0xf) >> 2];
	reg = (reg & ~mask) | ((data << shift) & mask);
	if (CryptFpgaRegisters) {
		// The high address word starts a new stream. Low address and key
		// writes update their respective latches without resetting it.
		if ((address & 0xc) == 8) {
			if (mask & 0xffff0000) Crypt.prot_cur_address = (Crypt.prot_cur_address & 0x01ff0000) | (reg >> 16);
			if (mask & 0x0000ffff) Crypt.set_addr_high(reg & 0x01ff);
		}
		if ((address & 0xc) == 12 && (mask & 0xffff0000)) Crypt.subkey = reg >> 16;
		return;
	}
	if ((address & 0xc) == 8) {
		if (mask & 0xffff0000) Crypt.set_addr_low(reg >> 16);
		if (mask & 0x0000ffff) Crypt.set_addr_high(reg);
	}
	if ((address & 0xc) == 12 && (mask & 0xffff0000)) Crypt.set_subkey(reg >> 16);
}

static UINT8 __fastcall StvReadByte(UINT32 address)
{
	UINT32 a = address & 0x1fffffff;
	if (Has5838 && a >= 0x02000000 && a < 0x03800000 && (a & 0x7ffffc) == 0x7ffff8) return CompRead(a, 1);
	if (CryptKey && (a & 0xfffffff0) == 0x04fffff0) return CryptRead(a, 1);
	if (a >= 0x02000000 && a < 0x05000000) return MemByte(DrvCart, a - 0x02000000);
	if (a >= 0x05c00000 && a < 0x05fc0000) return VideoRead(a, 1);
	if ((a & 0xfffffff0) == 0x05fe0080) return DspRead(a, 1);
	if ((a & 0xfff80000) == 0x00100000) {
		UINT32 r = a & 0x7f;
		if (r == 0x75) return (Smpc[r] & Smpc[0x79]) | (0x7f & ~Smpc[0x79]);
		if (r == 0x77) return (Smpc[r] & Smpc[0x7b]) | ((0x7e | EEPROMRead()) & ~Smpc[0x7b]);
		return Smpc[r];
	}
	if ((a & 0xffff0000) == 0x00180000) return (a & 1) ? DrvBackupRAM[(a & 0xffff) >> 1] : 0;
	if ((a & 0xffffffc0) == 0x00400000) {
		UINT32 r = a & 0x1f;
		if (!(r & 1)) return 0xff;
		if (HammerControls) {
			// Cabinet coordinates are wired to PORT A/B in scrambled bit order.
			// PORT C bit 4 (active low) also drives the strike latch on both axes.
			if (a == 0x00400001 || a == 0x00400003) {
				const UINT8 axis = (a & 2) ? 2 + BurnGunReturnY(0) * 44 / 255 : BurnGunReturnX(0) * 63 / 255;
				return ((axis & 0x04) << 5) | ((axis & 0x08) << 3) |
					((axis & 0x03) << 4) | ((axis & 0x30) >> 4) | ((DrvInputs[2] & 0x10) ? 0 : 4);
			}
			if (r == 1 || r == 3 || r == 9 || r == 0x0b || (r == 0x0d && !(Ioga[0x1d] & 0x80))) return 0xff;
		}
		if (SafariControls) {
			// The two buttons are on PORT A. The active-low winch is on PORT G.
			if (r == 1) return DrvInputs[0] | 0xfc;
			if (r == 3 || r == 9 || r == 0x0b) return 0xff;
			if (r == 0x0d) return (Ioga[0x1d] & 0x80) ? 0 : (0xff ^ (DrvJoy1[2] & 1));
		}
		if (TsuriboriControls && r == 0x1f) {
			// The game selects channel zero and reads two axes for each rod.
			const UINT8 channel = Ioga[r] & 7;
			Ioga[r] = (channel + 1) & 7;
			if (channel >= 4) return 0xff;

			const UINT8 *joy = (channel & 2) ? DrvJoy2 : DrvJoy1;
			const INT32 negative = (channel & 1) ? 5 : 7;
			const INT32 positive = (channel & 1) ? 4 : 6;
			if (joy[negative] != joy[positive]) return joy[negative] ? 0x30 : 0xc0;
			return ProcessAnalog(TsuriboriRod[channel], 0, INPUT_DEADZONE, 0x30, 0xc0);
		}
		if (ShinkansenControls && r == 0x1f) {
			// The game selects ADC channel 0 then reads its two levers in order.
			const UINT8 channel = Ioga[r] & 7;
			Ioga[r] = (channel + 1) & 7;
			if (channel == 0) return DrvJoy1[5] ? 0xff : 0x70;
			if (channel == 1) return DrvJoy1[4] ? 0xff : 0xa0;
			return 0xff;
		}
		if (HopperControls) {
			if (r == 1) return (HopperTrackballControls ? 0xfc : DrvInputs[0] & 0xfc) | (DrvJoy1[0] & 1) | (SkychalHopperSensor << 1);
			if (r == 3) {
				UINT8 value = DrvInputs[1] ^ 2;
				if (DrvJoy2[2]) value &= ~0x0c;
				return value;
			}
			if (r == 0x0b) return 0xff;
		}
		if (HopperTrackballControls) {
			if (r == 0x0d && (Ioga[0x1d] & 0x80)) {
				// PORT-G reads the high/low bytes of the relative counters in order.
				const UINT8 selector = Ioga[0x0d];
				const INT32 axis = (selector >> 1) & 3;
				const UINT16 count = axis < 2 ? BurnTrackballReadWord(0, axis) - SkychalCounterBase[axis] : 0;
				Ioga[0x0d] = (selector & 0xf8) | ((selector + 1) & 7);
				return count >> ((~selector & 1) * 8);
			}
			if (r == 9 || r == 0x0d) return 0xff;
		}
		if (MahjongMode && r <= 3 && !(Ioga[0x11] & 0x10)) {
			UINT8 value = 0xff;
			UINT8 select = Ioga[9] ^ 0xff;
			if (MahjongMode == MahjongJoystick && (MyfairldControls & 1)) {
				// Micronet's joystick is wired through row 3 of the panel multiplexer.
				if (r == 1 && (select & 8)) value = DrvInputs[0];
				return value;
			}
			for (INT32 row = 0; row < 5; row++) {
				if (select & (1 << row)) value &= MahjongInputs[r >> 1][row];
			}
			return value;
		}
		if (MahjongMode == MahjongJoystick && (r <= 3 || r == 0x0b)) return 0xff;
		if (r <= 5) return DrvInputs[r >> 1];
		if (r == 0x0b) return DrvInputs[3];
		if (r == 0x1b) return 0; // Serial COM status.
		return Ioga[r];
	}
	if ((a & 0xfff00000) == 0x05a00000) { SoundSyncBus(); return MemByte(DrvSoundRAM, a & 0xfffff); }
	if ((a & 0xfffff000) == 0x05b00000) { SoundSyncBus(); return Sound->read((a & 0xfff) / 2) >> ((~a & 1) * 8); }
	if ((a & 0xffffff00) == 0x05fe0000 && (a & 0xff) < 0xd0) return Scu[(a & 0xff) >> 2] >> ((~a & 3) * 8);
	return 0xff;
}

static UINT16 __fastcall StvReadWord(UINT32 a)
{
	a &= 0x1fffffff;
	if (Has5838 && a >= 0x02000000 && a < 0x03800000 && (a & 0x7ffffc) == 0x7ffff8) return CompRead(a, 2);
	if (CryptKey && (a & 0xfffffff0) == 0x04fffff0) return CryptRead(a, 2);
	if (a >= 0x02000000 && a < 0x05000000) return MemWord(DrvCart, a - 0x02000000);
	if (a >= 0x05c00000 && a < 0x05fc0000) return VideoRead(a, 2);
	if ((a & 0xfffffff0) == 0x05fe0080) return DspRead(a, 2);
	if ((a & 0xfffff000) == 0x05b00000) return SoundReadWord(0x100000 | (a & 0xfff));
	return (StvReadByte(a) << 8) | StvReadByte(a + 1);
}

static UINT32 __fastcall StvReadLong(UINT32 a)
{
	a &= 0x1fffffff;
	if (Has5838 && a >= 0x02000000 && a < 0x03800000 && (a & 0x7ffffc) == 0x7ffff8) return CompRead(a, 4);
	if (CryptKey && (a & 0xfffffff0) == 0x04fffff0) return CryptRead(a, 4);
	if (a >= 0x02000000 && a < 0x05000000) return (UINT32(MemWord(DrvCart, a - 0x02000000)) << 16) | MemWord(DrvCart, a - 0x02000000 + 2);
	if (a >= 0x05c00000 && a < 0x05fc0000) return VideoRead(a, 4);
	if ((a & 0xfffffff0) == 0x05fe0080) return DspRead(a, 4);
	return (UINT32(StvReadWord(a)) << 16) | StvReadWord(a + 2);
}

static void __fastcall StvWriteByte(UINT32 address, UINT8 d)
{
	UINT32 a = address & 0x1fffffff;
	if (Has5838 && a >= 0x02000000 && a < 0x03800000 && (a & 0x7ffff8) == 0x7ffff0) { CompWrite(a, d, 1); return; }
	if (CryptKey && (a & 0xfffffff0) == 0x04fffff0) { CryptWrite(a, d, 1); return; }
	if (a >= 0x05c00000 && a < 0x05fc0000) { VideoWrite(a, d, 1); return; }
	if ((a & 0xfffffff0) == 0x05fe0080) { DspWrite(a, d, 1); return; }
	if ((a & 0xff800000) == 0x01000000) { FrtPending[1] = 1; Sh2StopRun(); return; }
	if ((a & 0xff800000) == 0x01800000) { FrtPending[0] = 1; Sh2StopRun(); return; }
	if ((a & 0xfff80000) == 0x00100000) {
		UINT32 r = a & 0x7f;
		if (r == 0x77) {
			INT32 enable = !((d & Smpc[0x7b]) & 0x10);
			if (enable && !SoundEnabled) SoundResetPending = 1;
			SoundEnabled = enable;
		}
		if (r == 0x75) { UINT8 out = d & Smpc[0x79]; EEPROMWrite(out & 8, out & 4, out & 0x10); }
		if (r == 0x1f && SmpcDelay) return; // Keep the accepted command until it completes.
		Smpc[r] = d;
		if (r == 1 && IntbackStage) {
			if (d & 0x40) { Smpc[0x61] &= 0x0f; IntbackStage = 0; IntbackDelay = 0; }
			// MiSTer SMPC CONT is an edge/toggle handshake, including the falling edge.
			else if ((d & 0x80) != IntbackContinueBit) {
				IntbackContinueBit = d & 0x80;
				IntbackDelay = 12; Smpc[0x63] = 1;
			}
		}
		if (r == 0x1f) {
			Smpc[0x63] = 1;
			SmpcDelay = 4;
			if (SmpcClockHalted()) {
				// ST-V switches the PLL at the last visible line. MAME halts
				// the three CPUs until that boundary to avoid phantom credits.
				const INT32 lastVisible = Video->get_vblank_start_position() * Video->get_ystep_count() - 1;
				const INT32 line = std::min(FrameLines - 1, std::max(0, lastVisible));
				SmpcDelay = (line - CurrentLine + FrameLines - 1) % FrameLines + 1;
				Sh2StopRun();
			}
			if ((d & 0x1f) == 0x10) {
				for (INT32 i = 0; i < 3; i++) IntbackRequest[i] = Smpc[1 + i * 2];
				if (IntbackRequest[1] & 8) SmpcDelay = 12;
			}
		}
		return;
	}
	if ((a & 0xffff0000) == 0x00180000) {
		if (a & 1) DrvBackupRAM[(a & 0xffff) >> 1] = d;
		return;
	}
	if ((a & 0xffffffc0) == 0x00400000) {
		const UINT32 r = a & 0x1f;
		if (HopperControls && r == 7 && ((Ioga[r] ^ d) & 0x80)) {
			if (d & 0x80) {
				SkychalHopperPhase = 0;
				SkychalHopperSensor = 0;
			} else if (!SkychalHopperSensor) {
				SkychalHopperPhase = 0;
			}
		}
		if (HopperTrackballControls && r == 0x0d && !(d & 0x80)) {
			for (INT32 axis = 0; axis < 2; axis++) SkychalCounterBase[axis] = BurnTrackballReadWord(0, axis);
		}
		Ioga[r] = d;
		return;
	}
	if ((a & 0xfff00000) == 0x05a00000) { SoundSyncBus(); MemWrite(DrvSoundRAM, a & 0xfffff, d); return; }
	if ((a & 0xfffff000) == 0x05b00000) { SoundWriteByte(0x100000 | (a & 0xfff), d); return; }
	if ((a & 0xffffff00) == 0x05fe0000 && (a & 0xff) < 0xd0) {
		UINT32 r = (a & 0xff) >> 2;
		INT32 shift = (~a & 3) * 8;
		if (r == 0xa4 / 4) Scu[r] &= ~(UINT32(0xff ^ d) << shift);
		else Scu[r] = (Scu[r] & ~(UINT32(0xff) << shift)) | (UINT32(d) << shift);
		if ((a & 3) == 3 && r < 24 && (r & 7) == 4 && (Scu[r] & 1) && (Scu[r + 1] & 7) == 7) {
			const INT32 channel = r / 8;
			if (Dma[channel].active) return;
			DmaStart(channel);
			// The CPU can program the same channel again before this scanline
			// ends. Commit a direct software transfer before its next access.
			const StvDma &dma = Dma[channel];
			if (ChoroqhrIndirectDma && channel == 1 && dma.active && dma.indirect) {
				// Fetch the software-started command list before returning
				// to the SH-2 interrupt handler that owns its work RAM.
				DmaRunChannel(channel, 256);
			} else if (dma.active && !dma.indirect) {
				const INT32 size = dma.remaining;
				const bool soundDma = DanchiDmaTiming && channel == 2 &&
					(dma.source & 0x07f00000) && (dma.destination & 0xfff00000) == 0x05a00000;
				if (soundDma) {
					// The shortest transfer still takes multiple bus slots.
					// Keep its completion IRQ behind the CPU's busy-flag store.
					const INT32 deadline = ExtraCycles[0] + Sh2TotalCycles() + std::max(16, size / 4);
					if (DanchiDmaIrqCycles < 0) DanchiDmaIrqCycles = deadline;
					Sh2StopRun();
				}
				DmaRunChannel(channel, (size + 1) / 2, soundDma);
			}
		}
	}
}

static void __fastcall StvWriteWord(UINT32 a, UINT16 d)
{
	a &= 0x1fffffff;
	if (StvRax::Rom && a == 0x04800000) {
		const INT32 cycles = INT64(SoundPosition()) * StvRax::Clock / 11289600;
		StvRax::Command(d, cycles);
		return;
	}
	if (Has5838 && a >= 0x02000000 && a < 0x03800000 && (a & 0x7ffff8) == 0x7ffff0) { CompWrite(a, d, 2); return; }
	if (CryptKey && (a & 0xfffffff0) == 0x04fffff0) { CryptWrite(a, d, 2); return; }
	if (a >= 0x05c00000 && a < 0x05fc0000) { VideoWrite(a, d, 2); return; }
	if ((a & 0xfffffff0) == 0x05fe0080) { DspWrite(a, d, 2); return; }
	if ((a & 0xfffff000) == 0x05b00000) { SoundWriteWord(0x100000 | (a & 0xfff), d); return; }
	StvWriteByte(a, d >> 8);
	StvWriteByte(a + 1, d);
}

static void __fastcall StvWriteLong(UINT32 a, UINT32 d)
{
	a &= 0x1fffffff;
	if (Has5838 && a >= 0x02000000 && a < 0x03800000 && (a & 0x7ffff8) == 0x7ffff0) { CompWrite(a, d, 4); return; }
	if (CryptKey && (a & 0xfffffff0) == 0x04fffff0) { CryptWrite(a, d, 4); return; }
	if (a >= 0x05c00000 && a < 0x05fc0000) { VideoWrite(a, d, 4); return; }
	if ((a & 0xfffffff0) == 0x05fe0080) { DspWrite(a, d, 4); return; }
	StvWriteWord(a, d >> 16);
	StvWriteWord(a + 2, d);
}

static INT32 MemIndex()
{
	UINT8 *Next = AllMem;
	DrvBios      = Next; Next += 0x0080000;
	DrvCart      = Next; Next += 0x3000000;
	DrvBackupRAM = Next; Next += 0x0008000;
	AllRam       = Next;
	DrvWorkL     = Next; Next += 0x0100000;
	DrvWorkH     = Next; Next += 0x0100000;
	DrvSoundRAM  = Next; Next += 0x0100000;
	DrvCache[0]  = Next; Next += 0x0010000;
	DrvCache[1]  = Next; Next += 0x0010000;
	RamEnd       = Next;
	MemEnd       = Next;
	return 0;
}

static void MapRegion(UINT8 *mem, UINT32 start, UINT32 end, INT32 flags)
{
	Sh2MapMemory(mem, start, end, flags);
	Sh2MapMemory(mem, start | 0x20000000, end | 0x20000000, flags);
}

static INT32 DrvDoReset()
{
	StvRax::Reset();
	Comp = stv_5838::Core();
	CompBank = 0;
	CompLatch = 0;
	Crypt = stv_5881::Core();
	Crypt.key = CryptKey;
	memset(CryptRegisters, 0, sizeof(CryptRegisters));
	CryptByteLatch = 0;
	delete Video;
	Video = new stv_video::Video();
	Video->video_start_vdp2_video_start();
	VideoBitmap.allocate(1024, 1024);
	ScuDsp.reset();
	delete Sound; Sound = new stv_scsp::Core();
	Sound->start(DrvSoundRAM);
	SoundEnabled = SoundResetPending = SoundCpuActive = SoundCycles = 0;
	SoundClock = 26846587;
	SoundFrameSamples = SoundSampleRemainder = 0;
	memset(SoundDcInput, 0, sizeof(SoundDcInput));
	memset(SoundDcOutput, 0, sizeof(SoundDcOutput));
	SekOpen(0); SekReset(); SekClose();
	memset(AllRam, 0, RamEnd - AllRam);
	memset(Smpc, 0, sizeof(Smpc));
	memset(SmpcMemory, 0, sizeof(SmpcMemory));
	memset(IntbackRequest, 0, sizeof(IntbackRequest));
	IntbackStage = IntbackMode = IntbackContinueBit = IntbackDelay = 0;
	memset(Scu, 0, sizeof(Scu));
	memset(Dma, 0, sizeof(Dma));
	for (INT32 i = 0; i < 3; i++) Scu[i * 8 + 5] = 7;
	memset(Ioga, 0xff, sizeof(Ioga));
	if (HopperControls) {
		Ioga[7] = Ioga[0x0d] = Ioga[0x1d] = 0;
		SkychalHopperPhase = SkychalHopperSensor = 0;
	}
	if (HopperTrackballControls) {
		BurnTrackballReadReset();
		memset(SkychalCounterBase, 0, sizeof(SkychalCounterBase));
	}
	Scu[0xa0 / 4] = 0xbfff;
	Scu[0xc8 / 4] = 4;
	SmpcRtc[0] = 0x19; SmpcRtc[1] = 0x97; SmpcRtc[2] = 0x11;
	SmpcRtc[3] = 1; SmpcRtc[4] = SmpcRtc[5] = SmpcRtc[6] = 0;
	SlaveEnabled = SlaveResetPending = MasterNmiPending = ResetPending = 0;
	SmpcDelay = CurrentLine = DotSelect = 0;
	IrqBit = -1;
	ExtraCycles[0] = ExtraCycles[1] = 0;
	FrtPending[0] = FrtPending[1] = 0;
	DanchiDmaIrqCycles = -1;
	for (INT32 i = 0; i < 2; i++) { Sh2Open(i); Sh2Reset(); Sh2Close(); }
	EEPROMReset();
	return 0;
}

static INT32 DrvExit()
{
	StvRax::Exit();
	if (ProfilePcmFile) { fclose(ProfilePcmFile); ProfilePcmFile = NULL; }
	ProfilePcmFrames = 0;
	delete Video; Video = NULL;
	VideoBitmap.reset();
	delete Sound; Sound = NULL;

	SekExit();
	Sh2Exit();
	EEPROMExit();
	if (HopperTrackballControls) BurnTrackballExit();
	if (HammerControls) BurnGunExit();
	GenericTilesExit();
	BurnDrvSetVisibleSize(320, 224);
	BurnFree(AllMem);
	return 0;
}

// MAME ROM_LOAD16_WORD_SWAP becomes a word-pair swap in the SH-2 native map.
static INT32 LoadWordRom(UINT8 *dest, INT32 index, UINT32 size)
{
	if (BurnLoadRom(dest, index, 1)) return 1;
	for (UINT32 i = 0; i < size; i += 4) {
		UINT8 a = dest[i], b = dest[i + 1], c = dest[i + 2], d = dest[i + 3];
#if defined LSB_FIRST
		dest[i] = c; dest[i + 1] = d; dest[i + 2] = a; dest[i + 3] = b;
#else
		dest[i] = b; dest[i + 1] = a; dest[i + 2] = d; dest[i + 3] = c;
#endif
	}
	return 0;
}

// Cartridge offsets use the SH-2 bus byte order, independently of host endianness.
struct StvRomLoad {
	INT32 index;
	UINT32 offset, size;
	INT32 mode; // 0: contiguous, 1: word swap, 2: odd/even byte lane
};

struct StvGame {
	const char *name;
	const StvRomLoad *loads;
	UINT32 count;
	INT32 bios, eeprom, region;
};

static const StvGame *CurrentGame;
static const StvGame *FindGame();

static INT32 LoadCartridge()
{
	for (UINT32 n = 0; n < CurrentGame->count; n++) {
		const StvRomLoad &r = CurrentGame->loads[n];
		BurnRomInfo info;
		BurnDrvGetRomInfo(&info, r.index);
		UINT8 *data = (UINT8 *)BurnMalloc(info.nLen);
		if (!data) return 1;
		if (BurnLoadRom(data, r.index, 1)) { BurnFree(data); return 1; }
		for (UINT32 i = 0; i < r.size; i++) {
			UINT32 address = r.offset + (r.mode == 2 ? i * 2 : r.mode == 1 ? (i ^ 1) : i);
			MemWrite(DrvCart, address, data[i]);
		}
		BurnFree(data);
	}
	return 0;
}

static INT32 DrvInit()
{
	CurrentGame = FindGame();
	if (!CurrentGame) return 1;
	StvRegion = CurrentGame->region;
	ShinkansenControls = !strcmp(CurrentGame->name, "wwshin");
	TsuriboriControls = !strcmp(CurrentGame->name, "tsuribor");
	memset(TsuriboriRod, 0, sizeof(TsuriboriRod));
	SafariControls = !strcmp(CurrentGame->name, "wasafari");
	ChoroqhrIndirectDma = !strcmp(CurrentGame->name, "choroqhr");
	DanchiDmaTiming = !strcmp(CurrentGame->name, "danchih") || !strcmp(CurrentGame->name, "danchiq");
	HammerControls = !strcmp(CurrentGame->name, "critcrsh") || !strcmp(CurrentGame->name, "tatacot");
	memset(HammerAim, 0, sizeof(HammerAim));
	HopperTrackballControls = !strcmp(CurrentGame->name, "skychal") ||
		!strcmp(CurrentGame->name, "supgoal") || !strcmp(CurrentGame->name, "techbowl") ||
		!strcmp(CurrentGame->name, "chalgolf") || !strcmp(CurrentGame->name, "patocar");
	HopperControls = HopperTrackballControls || !strcmp(CurrentGame->name, "micrombc");
	MahjongMode = NoMahjongPanel;
	if (!strcmp(CurrentGame->name, "kiwames")) MahjongMode = MahjongPanelWithJamma;
	if (!strcmp(CurrentGame->name, "vmahjong")) MahjongMode = MahjongPanel;
	if (!strcmp(CurrentGame->name, "myfairld")) MahjongMode = MahjongJoystick;
	Has5838 = !strcmp(CurrentGame->name, "decathlt") || !strcmp(CurrentGame->name, "decathlto");
	CryptKey = 0;
	if (!strcmp(CurrentGame->name, "astrass")) CryptKey = 0x052e2901;
	if (!strcmp(CurrentGame->name, "elandore")) CryptKey = 0x05226d41;
	if (!strcmp(CurrentGame->name, "ffrevng10")) CryptKey = 0x0524ac01;
	if (!strcmp(CurrentGame->name, "ffreveng")) CryptKey = 0x0524ac01;
	if (!strcmp(CurrentGame->name, "rsgun")) CryptKey = 0x05272d01;
	if (!strcmp(CurrentGame->name, "sss")) CryptKey = 0x052b6901;
	CryptFpgaRegisters = !strcmp(CurrentGame->name, "twcup98");
	if (CryptFpgaRegisters) CryptKey = 0x05200913;
	if (!strcmp(CurrentGame->name, "twsoc98")) CryptKey = 0x05200913;

	BurnAllocMemIndex();
	if (LoadWordRom(DrvBios, CurrentGame->bios, 0x80000) || LoadCartridge()) {
		BurnFree(AllMem);
		return 1;
	}
	if (!strcmp(CurrentGame->name, "sanjeon")) {
		for (UINT32 i = 0; i < 0x3000000; i++) {
			UINT8 v = DrvCart[i] ^ 0xff;
			v = stv_5881::bitswap<8>(v, 7,2,5,1,3,6,4,0);
			v = stv_5881::bitswap<8>(v, 4,6,5,7,3,2,1,0);
			v = stv_5881::bitswap<8>(v, 7,6,5,4,2,3,1,0);
			v = stv_5881::bitswap<8>(v, 7,0,5,4,3,2,1,6);
			DrvCart[i] = stv_5881::bitswap<8>(v, 3,6,5,4,7,2,1,0);
		}
	}
	if (Sh2Init(2)) { BurnFree(AllMem); return 1; }
	for (INT32 i = 0; i < 2; i++) {
		Sh2Open(i);
		Sh2SetSlave(i);
		// BIOS branches from the uncached alias to the cached alias using PC-relative arithmetic.
		Sh2SetAddressMask(0xffffffff);
		MapRegion(DrvBios, 0, 0x7ffff, MAP_ROM);
		MapRegion(DrvWorkL, 0x00200000, 0x002fffff, MAP_RAM);
		MapRegion(DrvWorkL, 0x00300000, 0x003fffff, MAP_RAM);
		MapRegion(DrvCart, 0x02000000, 0x04ffffff, MAP_ROM);
		if (Has5838) {
			for (UINT32 a = 0x027ff000; a < 0x03800000; a += 0x800000) {
				Sh2MapHandler(0, a, a + 0xfff, MAP_ROM);
				Sh2MapHandler(0, a | 0x20000000, (a | 0x20000000) + 0xfff, MAP_ROM);
			}
		}
		if (CryptKey) {
			// Handler mapping is page-granular; ordinary reads in the last page fall back to ROM.
			Sh2MapHandler(0, 0x04fff000, 0x04ffffff, MAP_ROM);
			Sh2MapHandler(0, 0x24fff000, 0x24ffffff, MAP_ROM);
		}
		for (UINT32 a = 0x06000000; a < 0x08000000; a += 0x100000) MapRegion(DrvWorkH, a, a + 0xfffff, MAP_RAM);
		Sh2MapMemory(DrvCache[i], 0xc0000000, 0xc000ffff, MAP_RAM);
		Sh2SetReadByteHandler(0, StvReadByte);
		Sh2SetReadWordHandler(0, StvReadWord);
		Sh2SetReadLongHandler(0, StvReadLong);
		Sh2SetWriteByteHandler(0, StvWriteByte);
		Sh2SetWriteWordHandler(0, StvWriteWord);
		Sh2SetWriteLongHandler(0, StvWriteLong);
		if (i == 0) Sh2SetIRQCallback(StvIrqCallback);
		Sh2Close();
	}
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapHandler(0, 0x000000, 0x100fff, MAP_RAM);
	SekMapHandler(0, 0x000000, 0x0fffff, MAP_FETCH);
	SekSetReadWordHandler(0, SoundReadWord);
	SekSetReadByteHandler(0, SoundReadByte);
	SekSetWriteWordHandler(0, SoundWriteWord);
	SekSetWriteByteHandler(0, SoundWriteByte);
	SekClose();
	// Preserve Decathlete's output-level adjustment.
	const bool decathlete = !strcmp(CurrentGame->name, "decathlt") || !strcmp(CurrentGame->name, "decathlto");
	SoundGain = decathlete ? 1.5 : 1.0;
	if (!strcmp(CurrentGame->name, "gaxeduel")) SoundGain = 0.8;
	// Match the output level of the Japanese and Taiwanese wrestling sets.
	if (!strcmp(CurrentGame->name, "znpwfv") || !strcmp(CurrentGame->name, "znpwfvt")) SoundGain = 2.0;
	UINT8 defaults[0x80];
	if (CurrentGame->eeprom >= 0 && BurnLoadRom(defaults, CurrentGame->eeprom, 1)) {

		SekExit();
		Sh2Exit();
		BurnFree(AllMem);
		return 1;
	}
	// Tsuribori's dump uses ROM_LOAD16_WORD_SWAP; other EEPROM dumps are byte-linear.
	if (CurrentGame->eeprom >= 0 && !strcmp(CurrentGame->name, "tsuribor")) {
		for (INT32 i = 0; i < 0x80; i += 2) { UINT8 value = defaults[i]; defaults[i] = defaults[i + 1]; defaults[i + 1] = value; }
	}
	if (!strcmp(CurrentGame->name, "batmanfr") && StvRax::Init()) {
		SekExit();
		Sh2Exit();
		BurnFree(AllMem);
		return 1;
	}
	EEPROMInit(&eeprom_interface_93C46);
	if (CurrentGame->eeprom >= 0) EEPROMFill(defaults, 0, sizeof(defaults));
	GenericTilesInit();
	if (HopperTrackballControls) BurnTrackballInit(1);
	if (HammerControls) BurnGunInit(1, true);
	BurnSetRefreshRate(60000.0 / 1001.0);
	return DrvDoReset();
}

static INT32 DrvDraw()
{
	INT32 width = Video->screen.visible.width(), height = Video->screen.visible.height();
	if (width != nScreenWidth || height != nScreenHeight) {
		BurnTransferSetDimensions(width, height);
		GenericTilesSetClipRaw(0, width, 0, height);
		// BurnDrvSetVisibleSize accepts native axes and rotates vertical metadata.
		BurnDrvSetVisibleSize(width, height);
		ReinitialiseVideo();
		BurnTransferRealloc();
		return 0;
	}
	stv_video::rectangle clip(0, nScreenWidth - 1, 0, nScreenHeight - 1);
	Video->screen_update_vdp2(Video->screen, VideoBitmap, clip);
	// BurnHighCol packs independent RGB components (including software gamma).
	// Resolve its 256 values per channel once per frame instead of calling it
	// for every pixel; rebuilding also covers a gamma change without a reset.
	UINT32 red[256], green[256], blue[256];
	bool native_rgb = nBurnBpp == 4;
	for (INT32 i = 0; i < 256; i++) {
		red[i] = BurnHighCol(i, 0, 0, 0);
		green[i] = BurnHighCol(0, i, 0, 0);
		blue[i] = BurnHighCol(0, 0, i, 0);
		native_rgb &= red[i] == UINT32(i << 16) && green[i] == UINT32(i << 8) && blue[i] == UINT32(i);
	}
	for (INT32 y = 0; y < nScreenHeight; y++) {
		UINT8 *dest = pBurnDraw + y * nBurnPitch;
		if (native_rgb) {
			const UINT32 *source = &VideoBitmap.pix(y);
			UINT32 *output = reinterpret_cast<UINT32*>(dest);
			for (INT32 x = 0; x < nScreenWidth; x++) output[x] = source[x] & 0x00ffffff;
			continue;
		}
		for (INT32 x = 0; x < nScreenWidth; x++, dest += nBurnBpp) {
			UINT32 rgb = VideoBitmap.pix(y, x);
			UINT32 color = red[(rgb >> 16) & 255] | green[(rgb >> 8) & 255] | blue[rgb & 255];
			if (nBurnBpp == 4) *((UINT32*)dest) = color;
			else if (nBurnBpp == 2) *((UINT16*)dest) = color;
			else { dest[0] = color; dest[1] = color >> 8; dest[2] = color >> 16; }
		}
	}
	if (HammerControls) BurnGunDrawTargets();
	return 0;
}

static INT32 DrvFrame()
{
	ProfileEnabled = getenv("FBNEO_STV_PROFILE") != NULL;
	Video->profile_enabled = ProfileEnabled;
	double frameStart = ProfileEnabled ? ProfileNow() : 0;
	ProfileSamplesMs = 0;
	if (DrvReset) DrvDoReset();
	else if (ResetPending) {
		// SYSRES resets the Saturn devices, not the SMPC issuing the command.
		// The BIOS checks OREG31 after reset to distinguish this path from power-on.
		UINT8 savedSmpc[0x80], savedMemory[4], savedRtc[7];
		memcpy(savedSmpc, Smpc, sizeof(Smpc));
		memcpy(savedMemory, SmpcMemory, sizeof(SmpcMemory));
		memcpy(savedRtc, SmpcRtc, sizeof(SmpcRtc));
		DrvDoReset();
		memcpy(Smpc, savedSmpc, sizeof(Smpc));
		memcpy(SmpcMemory, savedMemory, sizeof(SmpcMemory));
		memcpy(SmpcRtc, savedRtc, sizeof(SmpcRtc));
	}
	memset(DrvInputs, 0xff, sizeof(DrvInputs));
	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
	}
	if (MahjongMode == MahjongPanelWithJamma) {
		// Kiwame S reads Start on the JAMMA system port as well as on the panel.
		DrvInputs[2] &= ~(((MahjongJoy[0][0][1] & 1) << 4) | ((MahjongJoy[1][0][1] & 1) << 5));
	}
	if (HammerControls) BurnGunMakeInputs(0, HammerAim[0], HammerAim[1]);
	if (HopperTrackballControls) {
		BurnTrackballFrame(0, SkychalTrackball[0], SkychalTrackball[1], 1, 0x3f);
		BurnTrackballUDLR(0, DrvJoy1[5], DrvJoy1[4], DrvJoy1[7], DrvJoy1[6], 8);
		BurnTrackballUpdate(0);
	}
	memset(MahjongInputs, 0xff, sizeof(MahjongInputs));
	for (INT32 player = 0; player < 2; player++) {
		for (INT32 row = 0; row < 5; row++) {
			for (INT32 bit = 0; bit < 8; bit++) MahjongInputs[player][row] ^= (MahjongJoy[player][row][bit] & 1) << bit;
		}
	}
	if (MahjongMode == MahjongJoystick && !(MyfairldControls & 1)) {
		// Start is wired to panel row 0 instead of the system port.
		if (DrvJoy3[4] & 1) MahjongInputs[0][0] &= ~0x02;
		DrvInputs[2] |= 0x10;
	}
	// Sh2NewFrame resets only the selected CPU. Both SH-2 clocks feed
	// SCSP synchronization, so neither may retain cycles from prior frames.
	for (INT32 cpu = 0; cpu < 2; cpu++) {
		Sh2Open(cpu);
		Sh2NewFrame();
		Sh2Close();
	}
	SekNewFrame();
	SoundCycles = 0;
	SoundFrameSamples = 0;
	SoundClock = DotSelect ? 28636363 : 26846587;
	INT32 total = INT32((DotSelect ? 28636363.5 : 26846587.5) * 100.0 / nBurnFPS);
	INT32 done[2] = {ExtraCycles[0], ExtraCycles[1]};
	const char *game = BurnDrvGetTextA(DRV_NAME);
	const UINT32 queueTailLoad = !strcmp(game, "smleague") ? 0x060615ce :
		!strcmp(game, "finlarch") ? 0x060627fa : 0;
	FrameLines = Video->get_vblank_duration();
	Video->screen.counter_mode = Video->m_vdp2_regs[0];
	INT32 vblank = Video->get_vblank_start_position() * Video->get_ystep_count();
	if (StvRax::Rom) StvRax::NewFrame();
	for (CurrentLine = 0; CurrentLine < FrameLines; CurrentLine++) {
		// The hopper toggles every 100 ms and finishes its current pulse when switched off.
		if (HopperControls && ((Ioga[7] & 0x80) || SkychalHopperSensor)) {
			SkychalHopperPhase += 1000;
			const INT32 period = FrameLines * nBurnFPS;
			if (SkychalHopperPhase >= period) {
				SkychalHopperPhase -= period;
				SkychalHopperSensor ^= 1;
			}
		}
		if (Sound->m_main_irq_cb.level) ScuRequest(6);
		if (CurrentLine == 0) ScuRequest(1);
		Video->screen.line = CurrentLine;
		Video->m_vdp2.dotsel = DotSelect;
		// MiSTer VDP1 changes buffers just after VBlank-out.
		if (CurrentLine == 0 && MahjongMode == MahjongPanelWithJamma) Video->vdp1_video_update();
		if (CurrentLine == vblank) {
			ScuRequest(0);
			Video->m_vdp2.odd ^= 1;
			if (MahjongMode != MahjongPanelWithJamma) Video->vdp1_video_update();
		}
		if (CurrentLine == vblank + 1 && Video->VDP1_VBE()) Video->m_vdp1.framebuffer_clear_on_next_frame = 1;
		if (Video->m_vdp1.draw_end_cycles > 0) {
			Video->m_vdp1.draw_end_cycles -= total / FrameLines;
			if (Video->m_vdp1.draw_end_cycles <= 0) Video->vdp1_draw_end();
		}
		if (Video->end_irq) {
			Video->end_irq = false;
			if (!StvRax::Rom) ScuRequest(13);
		}
		// Batman Forever relies on the late VDP1 completion IRQ. Keep CEF
		// tied to command completion and use MAME's scanline for the SCU edge.
		if (StvRax::Rom && CurrentLine == vblank + 8 * Video->get_ystep_count()) ScuRequest(13);
		if (CurrentLine < vblank) ScuRequest(2);
		if (Scu[0x98 / 4] & 1) {
			bool compare = CurrentLine == INT32(Scu[0x90 / 4] & 0x3ff);
			if (compare) ScuRequest(3);
			// MAME scanline scheduling: mode 1 gates timer 1 with timer 0 compare.
			if (!(Scu[0x98 / 4] & 0x100) || compare) ScuRequest(4);
		}
		if (SmpcDelay && --SmpcDelay == 0) SmpcComplete();
		if (IntbackDelay && --IntbackDelay == 0) SmpcContinue();
		Sh2Open(0);
		if (MasterNmiPending) {
			// Keep the SMPC edge pending through an SH-2 branch delay slot.
			Sh2SetIRQLine(16, CPU_IRQSTATUS_HOLD);
			MasterNmiPending = 0;
		}
		DmaRun();
		ScuDsp.run(total / (FrameLines * 2));
		ScuUpdateIrq();
		Sh2Close();

		// Both SH-2s share work RAM. A scanline-sized quantum can hide a complete
		// FIFO reset/refill from the other CPU. MINIT/SINIT also end the issuing
		// timeslice, as MAME's scheduler synchronization does for these pins.
		INT32 lineTarget = (CurrentLine + 1) * total / FrameLines;
		while (done[0] < lineTarget || done[1] < lineTarget) {
			for (INT32 cpu = 0; cpu < 2; cpu++) {
				if (done[cpu] >= lineTarget) continue;
				Sh2Open(cpu);
				if (cpu == 0) {
					if (DanchiDmaIrqCycles >= 0 && done[0] >= DanchiDmaIrqCycles) {
						DanchiDmaIrqCycles = -1;
						ScuRequest(9);
					}
					ScuUpdateIrq();
				}
				if (cpu == 1 && SlaveResetPending) { Sh2Reset(); SlaveResetPending = 0; }
				if (FrtPending[cpu]) { Sh2PulseFRTInput(); FrtPending[cpu] = 0; }
				INT32 cycles = std::min(32, lineTarget - done[cpu]);
				if (cpu == 0 && DanchiDmaIrqCycles >= 0)
					cycles = std::min(cycles, DanchiDmaIrqCycles - done[cpu]);
				if (!SmpcClockHalted() && (cpu == 0 || SlaveEnabled)) {
					done[cpu] += Sh2Run(cycles);
					// Let the baseball games' slave read the queue tail immediately
					// after publishing its head. If the master wraps the queue between
					// these instructions, the slave can skip the end marker.
					if (queueTailLoad && cpu == 1 && Sh2GetPC(1) == queueTailLoad) {
						done[cpu] += Sh2Run(1);
					}
				} else {
					Sh2Idle(cycles);
					done[cpu] += cycles;
				}
				Sh2Close();
			}
		}
		SoundRunTo(INT64(CurrentLine + 1) * 11289600 * 100 / (FrameLines * nBurnFPS));
		if (StvRax::Rom) StvRax::RunTo(INT64(CurrentLine + 1) * StvRax::Clock * 100 / (FrameLines * nBurnFPS));
	}
	for (INT32 i = 0; i < 2; i++) ExtraCycles[i] = done[i] - total;
	if (DanchiDmaIrqCycles >= 0) DanchiDmaIrqCycles = std::max(0, DanchiDmaIrqCycles - total);
	SoundRender();
	if (StvRax::Rom) StvRax::Mix();
	if (pBurnSoundOut && MahjongMode == MahjongPanelWithJamma && nBurnSoundRate > 0) {
		// Match MiSTer's output DC blocker for Kiwame S's biased SCSP stream.
		const double decay = 1.0 - (48000.0 / 512.0) / nBurnSoundRate;
		for (INT32 i = 0; i < nBurnSoundLen; i++) {
			for (INT32 channel = 0; channel < 2; channel++) {
				INT16 &sample = pBurnSoundOut[i * 2 + channel];
				const double input = sample * (0.5 + decay * 0.5);
				SoundDcOutput[channel] = input - SoundDcInput[channel] + decay * SoundDcOutput[channel];
				SoundDcInput[channel] = input;
				sample = BURN_SND_CLIP(INT32(SoundDcOutput[channel]));
			}
		}
	}
	SoundSampleRemainder = (SoundCycles + SoundSampleRemainder) & 255;
	// Bounded diagnostic capture before frontend playback, stereo signed 16-bit.
	if (pBurnSoundOut && getenv("FBNEO_STV_PCM_DUMP") && ProfilePcmFrames < 7200) {
		if (!ProfilePcmFile) ProfilePcmFile = fopen("stv-pcm.raw", "wb");
		if (ProfilePcmFile) fwrite(pBurnSoundOut, sizeof(INT16), nBurnSoundLen * 2, ProfilePcmFile);
		if (++ProfilePcmFrames == 7200 && ProfilePcmFile) { fclose(ProfilePcmFile); ProfilePcmFile = NULL; }
	}
	double drawStart = ProfileEnabled ? ProfileNow() : 0;
	if (pBurnDraw) DrvDraw();
	if (ProfileEnabled) {
		double end = ProfileNow();
		FILE *log = fopen("stv-profile.log", "a");
		if (log) {
			fprintf(log, "%s frame=%d total_ms=%.3f draw_ms=%.3f pcm_ms=%.3f target_fps=%.3f nbg_ms=%.3f rbg_ms=%.3f sprites_ms=%.3f rate=%d samples=%d rbg_cache_ms=%.3f rbg_rebuilds=%u rbg_vram=%u rbg_key=%d rbg_addr=%x/%x\n",
				BurnDrvGetTextA(DRV_NAME), Video->screen.frame, end - frameStart,
				end - drawStart, ProfileSamplesMs, nBurnFPS / 100.0,
				Video->profile_nbg, Video->profile_rbg, Video->profile_sprites, nBurnSoundRate, nBurnSoundLen,
				Video->profile_rotation_cache, Video->profile_rotation_rebuilds, Video->profile_rotation_vram,
				Video->profile_rotation_key, Video->rotation_last_dirty_address[0], Video->rotation_last_dirty_address[1]);
			fclose(log);
		}
	}
	Video->screen.frame++;
	if (getenv("FBNEO_STV_TRACE") && (Video->screen.frame % 60) == 0) {
		FILE *log = fopen("stv-trace.log", "a");
		if (log) {
			Sh2Open(0); UINT32 pc0 = Sh2GetPC(0); Sh2Close();
			Sh2Open(1); UINT32 pc1 = Sh2GetPC(0); Sh2Close();
			fprintf(log, "frame=%d master=%08x slave=%08x enabled=%d smpc=%02x scu=%08x/%08x vdp=%04x/%04x sound=%d irq=%d\n",
				Video->screen.frame, pc0, pc1, SlaveEnabled, Smpc[0x1f], Scu[0xa0/4], Scu[0xa4/4],
				Video->m_vdp1_regs[0x10/2], Video->m_vdp2_regs[0], SoundEnabled, Sound->m_irq_cb.level);
			fprintf(log, "blend lnclen=%04x ramctl=%04x spctl=%04x ccctl=%04x sfccmd=%04x pn=%04x/%04x/%04x sn=%04x/%04x/%04x/%04x ccr=%04x/%04x/%04x/%04x/%04x/%04x/%04x\n",
				Video->m_vdp2_regs[0xe8/2], Video->m_vdp2_regs[0xe/2], Video->m_vdp2_regs[0xe0/2], Video->m_vdp2_regs[0xec/2], Video->m_vdp2_regs[0xea/2],
				Video->m_vdp2_regs[0xf8/2], Video->m_vdp2_regs[0xfa/2], Video->m_vdp2_regs[0xfc/2],
				Video->m_vdp2_regs[0xf0/2], Video->m_vdp2_regs[0xf2/2], Video->m_vdp2_regs[0xf4/2], Video->m_vdp2_regs[0xf6/2],
				Video->m_vdp2_regs[0x100/2], Video->m_vdp2_regs[0x102/2], Video->m_vdp2_regs[0x104/2], Video->m_vdp2_regs[0x106/2],
				Video->m_vdp2_regs[0x108/2], Video->m_vdp2_regs[0x10a/2], Video->m_vdp2_regs[0x10c/2]);
			fclose(log);
		}
	}
	return 0;
}

static void ScanDeviceMemory(void *data, UINT32 length, const char *name)
{
	BurnArea ba = {};
	ba.Data = data; ba.nLen = length; ba.szName = const_cast<char*>(name);
	BurnAcb(&ba);
}

static void ScanDevices(INT32 nAction)
{
	if (!(nAction & ACB_DRIVER_DATA)) return;
	ScanDeviceMemory(Video->m_vdp1_regs.get(), 0x20, "ST-V m_vdp1_regs");
	ScanDeviceMemory(Video->m_vdp1_vram.get(), 0x100000, "ST-V m_vdp1_vram");
	ScanDeviceMemory(Video->m_vdp2_regs.get(), 0x40000, "ST-V m_vdp2_regs");
	ScanDeviceMemory(Video->m_vdp2_vram.get(), 0x100000, "ST-V m_vdp2_vram");
	ScanDeviceMemory(Video->m_vdp2_cram.get(), 0x80000, "ST-V m_vdp2_cram");
	ScanDeviceMemory(Video->m_vdp1.framebuffer[0].get(), 0x100000, "VDP1 framebuffer 0");
	ScanDeviceMemory(Video->m_vdp1.framebuffer[1].get(), 0x100000, "VDP1 framebuffer 1");
	SCAN_VAR(Video->m_vdp1.fbcr_accessed);
	SCAN_VAR(Video->m_vdp1.framebuffer_current_display);
	SCAN_VAR(Video->m_vdp1.framebuffer_current_draw);
	SCAN_VAR(Video->m_vdp1.framebuffer_clear_on_next_frame);
	SCAN_VAR(Video->m_vdp1.local_x);
	SCAN_VAR(Video->m_vdp1.local_y);
	SCAN_VAR(Video->m_vdp1.lopr);
	SCAN_VAR(Video->m_vdp1.copr);
	SCAN_VAR(Video->m_vdp1.ewdr);
	SCAN_VAR(Video->m_vdp1.system_cliprect);
	SCAN_VAR(Video->m_vdp1.user_cliprect);
	SCAN_VAR(Video->m_vdp1.draw_end_cycles);
	SCAN_VAR(Video->m_vdp2.dotsel);
	SCAN_VAR(Video->m_vdp2.pal);
	SCAN_VAR(Video->m_vdp2.odd);
	SCAN_VAR(Video->m_vdp2.h_count);
	SCAN_VAR(Video->m_vdp2.v_count);
	SCAN_VAR(Video->m_vdp2.exltfg);
	SCAN_VAR(Video->m_vdp2.exsyfg);
	SCAN_VAR(Video->m_vdp2.old_crmd);
	SCAN_VAR(Video->m_vdp2.old_tvmd);
	SCAN_VAR(Video->screen.frame); SCAN_VAR(Video->rng); SCAN_VAR(Video->end_irq);
	for (INT32 slot = 0; slot < 32; slot++) {
		SCAN_VAR(Sound->m_Slots[slot].udata);
		SCAN_VAR(Sound->m_Slots[slot].Backwards);
		SCAN_VAR(Sound->m_Slots[slot].active);
		SCAN_VAR(Sound->m_Slots[slot].cur_addr);
		SCAN_VAR(Sound->m_Slots[slot].nxt_addr);
		SCAN_VAR(Sound->m_Slots[slot].step);
		SCAN_VAR(Sound->m_Slots[slot].EG.state);
		SCAN_VAR(Sound->m_Slots[slot].EG.volume);
		SCAN_VAR(Sound->m_Slots[slot].EG.step);
		SCAN_VAR(Sound->m_Slots[slot].EG.AR);
		SCAN_VAR(Sound->m_Slots[slot].EG.D1R);
		SCAN_VAR(Sound->m_Slots[slot].EG.D2R);
		SCAN_VAR(Sound->m_Slots[slot].EG.RR);
		SCAN_VAR(Sound->m_Slots[slot].EG.DL);
		SCAN_VAR(Sound->m_Slots[slot].EG.EGHOLD);
		SCAN_VAR(Sound->m_Slots[slot].EG.LPLINK);
		SCAN_VAR(Sound->m_Slots[slot].PLFO.phase);
		SCAN_VAR(Sound->m_Slots[slot].PLFO.phase_step);
		SCAN_VAR(Sound->m_Slots[slot].ALFO.phase);
		SCAN_VAR(Sound->m_Slots[slot].ALFO.phase_step);
	}
	SCAN_VAR(Sound->m_RINGBUF);
	SCAN_VAR(Sound->m_BUFPTR);
	SCAN_VAR(Sound->m_latched_MSLC);
	SCAN_VAR(Sound->m_latched_MSLC_data);
	SCAN_VAR(Sound->m_IrqTimA);
	SCAN_VAR(Sound->m_IrqTimBC);
	SCAN_VAR(Sound->m_IrqMidi);
	SCAN_VAR(Sound->m_MidiOutStack);
	SCAN_VAR(Sound->m_MidiOutW);
	SCAN_VAR(Sound->m_MidiOutR);
	SCAN_VAR(Sound->m_MidiStack);
	SCAN_VAR(Sound->m_MidiW);
	SCAN_VAR(Sound->m_MidiR);
	SCAN_VAR(Sound->m_TimPris);
	SCAN_VAR(Sound->m_TimCnt);
	SCAN_VAR(Sound->m_dma.dmea);
	SCAN_VAR(Sound->m_dma.drga);
	SCAN_VAR(Sound->m_dma.dtlg);
	SCAN_VAR(Sound->m_dma.dgate);
	SCAN_VAR(Sound->m_dma.ddir);
	SCAN_VAR(Sound->m_mcieb);
	SCAN_VAR(Sound->m_mcipd);
	SCAN_VAR(Sound->m_DSP.RBP);
	SCAN_VAR(Sound->m_DSP.RBL);
	SCAN_VAR(Sound->m_DSP.COEF);
	SCAN_VAR(Sound->m_DSP.MADRS);
	SCAN_VAR(Sound->m_DSP.MPRO);
	SCAN_VAR(Sound->m_DSP.TEMP);
	SCAN_VAR(Sound->m_DSP.MEMS);
	SCAN_VAR(Sound->m_DSP.DEC);
	SCAN_VAR(Sound->m_DSP.MIXS);
	SCAN_VAR(Sound->m_DSP.EXTS);
	SCAN_VAR(Sound->m_DSP.EFREG);
	SCAN_VAR(Sound->m_DSP.Stopped);
	SCAN_VAR(Sound->m_DSP.LastStep);
	SCAN_VAR(Sound->m_udata); SCAN_VAR(Sound->timer_samples); SCAN_VAR(Sound->rng);
	SCAN_VAR(Sound->m_irq_cb.level); SCAN_VAR(Sound->m_main_irq_cb.level);
	if (nAction & ACB_WRITE) {
		Video->vdp1_state_save_postload();
		Video->vdp2_state_save_postload();
		Video->vdp2_dynamic_res_change();
		for (INT32 i = 0; i < 32; i++) Sound->Compute_LFO(&Sound->m_Slots[i]);
		Sound->set_output_gain(0, (Sound->m_udata.data[0] & 15) / 15.0);
		Sound->set_output_gain(1, (Sound->m_udata.data[0] & 15) / 15.0);
	}
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) *pnMin = 0x029744;
	if (nAction & ACB_MEMORY_RAM) {
		BurnArea ba = {};
		ba.Data = AllRam; ba.nLen = RamEnd - AllRam; ba.szName = "ST-V RAM";
		BurnAcb(&ba);
	}
	if (nAction & ACB_NVRAM) {
		BurnArea ba = {};
		ba.Data = DrvBackupRAM; ba.nLen = 0x8000; ba.szName = "ST-V backup RAM";
		BurnAcb(&ba);
	}
	if (nAction & ACB_DRIVER_DATA) {
		if (StvRax::Rom) StvRax::Scan(nAction);
		Sh2Scan(nAction);
		SekScan(nAction);
		SCAN_VAR(SoundSampleRemainder);
		SCAN_VAR(SoundEnabled); SCAN_VAR(SoundResetPending); SCAN_VAR(SoundCycles);
		SCAN_VAR(ScuDsp);
		SCAN_VAR(Comp);
		SCAN_VAR(CompBank);
		SCAN_VAR(CompLatch);
		SCAN_VAR(Crypt);
		SCAN_VAR(CryptRegisters);
		SCAN_VAR(CryptByteLatch);
		SCAN_VAR(IntbackRequest); SCAN_VAR(IntbackStage); SCAN_VAR(IntbackMode); SCAN_VAR(IntbackContinueBit); SCAN_VAR(IntbackDelay);
		SCAN_VAR(Smpc); SCAN_VAR(SmpcMemory); SCAN_VAR(SmpcRtc); SCAN_VAR(Ioga);
		SCAN_VAR(Scu); SCAN_VAR(Dma);
		SCAN_VAR(SlaveEnabled); SCAN_VAR(SlaveResetPending); SCAN_VAR(MasterNmiPending);
		SCAN_VAR(ResetPending); SCAN_VAR(SmpcDelay); SCAN_VAR(CurrentLine);
		SCAN_VAR(DotSelect); SCAN_VAR(IrqBit); SCAN_VAR(ExtraCycles); SCAN_VAR(FrtPending);
		if (DanchiDmaTiming) SCAN_VAR(DanchiDmaIrqCycles);
	}
	ScanDevices(nAction);
	if (HammerControls && (nAction & ACB_DRIVER_DATA)) BurnGunScan();
	EEPROMScan(nAction, pnMin);
	if (HopperTrackballControls && (nAction & ACB_DRIVER_DATA)) {
		BurnTrackballScan();
		SCAN_VAR(SkychalCounterBase);
	}
	if (HopperControls && (nAction & ACB_DRIVER_DATA)) {
		SCAN_VAR(SkychalHopperPhase);
		SCAN_VAR(SkychalHopperSensor);
	}
	return 0;
}

// ST-V BIOS (Japan, USA, Taiwan and Europe)

static struct BurnRomInfo StvbiosRomDesc[] = {
	{ "epr-23603.ic8", 0x080000, 0xf688ae60, BRF_BIOS | BRF_PRG | BRF_ESS }, // Japan
	{ "epr-17952a.ic8", 0x080000, 0xd1be2adf, BRF_BIOS | BRF_PRG | BRF_OPT }, // USA
	{ "epr-19854.ic8", 0x080000, 0xe09d1f60, BRF_BIOS | BRF_PRG | BRF_OPT }, // Taiwan
	{ "epr-17954a.ic8", 0x080000, 0xf7722da3, BRF_BIOS | BRF_PRG | BRF_OPT }, // Europe
};

STD_ROM_PICK(Stvbios)
STD_ROM_FN(Stvbios)

struct BurnDriver BurnDrvStvbios = {
	"stvbios", NULL, NULL, NULL, "1994",
	"ST-V BIOS\0", "BIOS only", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_SEGA_MISC, GBF_BIOS, 0,
	NULL, StvbiosRomInfo, StvbiosRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, 0,
	320, 224, 4, 3
};

static struct BurnRomInfo emptyRomDesc[] = {
	{ "", 0, 0, 0 },
};

// Astra SuperStars (J 980514 V1.002)

static struct BurnRomInfo AstrassRomDesc[] = {
	{ "epr20825.13", 0x100000, 0x94a9ad8f, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20827.2", 0x400000, 0x65cabbb3, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20828.3", 0x400000, 0x3934d44a, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20829.4", 0x400000, 0x814308c3, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20830.5", 0x400000, 0xff97fd19, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20831.6", 0x400000, 0x4408e6fb, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20826.1", 0x400000, 0xbdc4b941, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20832.8", 0x400000, 0xaf1b0985, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20833.9", 0x400000, 0xcb6af231, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Astrass, Astrass, Stvbios)
STD_ROM_FN(Astrass)

static const StvRomLoad AstrassLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0400000, 1 },
};

// Baku Baku Animal (J 950407 V1.000)

static struct BurnRomInfo BakubakuRomDesc[] = {
	{ "fpr17969.13", 0x100000, 0xbee327e5, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr17970.2", 0x400000, 0xbc4d6f91, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17971.3", 0x400000, 0xc780a3b3, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17972.4", 0x400000, 0x8f29815a, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17973.5", 0x400000, 0x5f6e0e8b, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Bakubaku, Bakubaku, Stvbios)
STD_ROM_FN(Bakubaku)

static const StvRomLoad BakubakuLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
};

// Batman Forever (JUE 960507 V1.000)

static struct BurnRomInfo BatmanfrRomDesc[] = {
	{ "350-mpa1.u19", 0x100000, 0x2a5a8c3a, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "350-mpa1.u16", 0x100000, 0x735e23ab, 1 | BRF_PRG | BRF_ESS },
	{ "gfx0.u1", 0x400000, 0xa82d0b7e, 1 | BRF_PRG | BRF_ESS },
	{ "gfx1.u3", 0x400000, 0xa41e55d9, 1 | BRF_PRG | BRF_ESS },
	{ "gfx2.u5", 0x400000, 0x4c1ebeb7, 1 | BRF_PRG | BRF_ESS },
	{ "gfx3.u8", 0x400000, 0xf679a3e7, 1 | BRF_PRG | BRF_ESS },
	{ "gfx4.u12", 0x400000, 0x52d95242, 1 | BRF_PRG | BRF_ESS },
	{ "gfx5.u15", 0x400000, 0xe201f830, 1 | BRF_PRG | BRF_ESS },
	{ "gfx6.u18", 0x400000, 0xc6b381a3, 1 | BRF_PRG | BRF_ESS },

	{ "350snda1.u52", 0x080000, 0x9027e7a0, BRF_PRG | BRF_ESS }, // rax
	{ "snd0.u48", 0x200000, 0x02b1927c, BRF_PRG | BRF_ESS },
	{ "snd1.u49", 0x200000, 0x58b18eda, BRF_PRG | BRF_ESS },
	{ "snd2.u50", 0x200000, 0x51d626d6, BRF_PRG | BRF_ESS },
	{ "snd3.u51", 0x200000, 0x31af26ae, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Batmanfr, Batmanfr, Stvbios)
STD_ROM_FN(Batmanfr)

static const StvRomLoad BatmanfrLoads[] = {
	{  0, 0x0000000, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 2 },
	{  1, 0x0000001, 0x0100000, 2 },
	{  1, 0x0200001, 0x0100000, 2 },
	{  2, 0x0400000, 0x0400000, 1 },
	{  3, 0x0800000, 0x0400000, 1 },
	{  4, 0x0c00000, 0x0400000, 1 },
	{  5, 0x1000000, 0x0400000, 1 },
	{  6, 0x1400000, 0x0400000, 1 },
	{  7, 0x1800000, 0x0400000, 1 },
	{  8, 0x1c00000, 0x0400000, 1 },
};

// Columns '97 (JET 961209 V1.000)

static struct BurnRomInfo Colmns97RomDesc[] = {
	{ "fpr19553.13", 0x100000, 0xd4fb6a5e, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr19554.2", 0x400000, 0x5a3ebcac, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19555.3", 0x400000, 0x74f6e6b8, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Colmns97, Colmns97, Stvbios)
STD_ROM_FN(Colmns97)

static const StvRomLoad Colmns97Loads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
};

// Cotton 2 (JUET 970902 V1.000)

static struct BurnRomInfo Cotton2RomDesc[] = {
	{ "mpr20122.7", 0x200000, 0xd616f78a, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20117.2", 0x400000, 0x893656ea, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20118.3", 0x400000, 0x1b6a1d4c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20119.4", 0x400000, 0x5a76e72b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20120.5", 0x400000, 0x7113dd7b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20121.6", 0x400000, 0x8c8fd521, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20116.1", 0x400000, 0xd30b0175, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20123.8", 0x400000, 0x35f1b89f, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Cotton2, Cotton2, Stvbios)
STD_ROM_FN(Cotton2)

static const StvRomLoad Cotton2Loads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
};

// Cotton Boomerang (JUET 980709 V1.000)

static struct BurnRomInfo CottonbmRomDesc[] = {
	{ "mpr21075.7", 0x200000, 0x200b58ba, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr21070.2", 0x400000, 0x56c0bf1d, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21071.3", 0x400000, 0x2bb18df2, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21072.4", 0x400000, 0x7c7cb977, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21073.5", 0x400000, 0xf2e5a5b7, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21074.6", 0x400000, 0x6a7e7a7b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21069.1", 0x400000, 0x6a28e3c5, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Cottonbm, Cottonbm, Stvbios)
STD_ROM_FN(Cottonbm)

static const StvRomLoad CottonbmLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
};

// Critter Crusher (EA 951204 V1.000)

static struct BurnRomInfo CritcrshRomDesc[] = {
	{ "epr-18821.ic13", 0x080000, 0x9a6658e2, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr-18789.ic8", 0x400000, 0xb388616f, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-18788.ic9", 0x400000, 0xfeae5867, 1 | BRF_PRG | BRF_ESS },

	{ "critcrsh.nv", 0x000080, 0x3da9860e, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Critcrsh, Critcrsh, Stvbios)
STD_ROM_FN(Critcrsh)

static const StvRomLoad CritcrshLoads[] = {
	{  0, 0x0000001, 0x0080000, 2 },
	{  0, 0x0100001, 0x0080000, 2 },
	{  0, 0x0200000, 0x0080000, 0 },
	{  0, 0x0300000, 0x0080000, 0 },
	{  1, 0x1c00000, 0x0400000, 1 },
	{  2, 0x2000000, 0x0400000, 1 },
};

// Tatacot (JA 951128 V1.000)

static struct BurnRomInfo TatacotRomDesc[] = {
	{ "epr-18790.ic13", 0x080000, 0xd95155dc, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr-18789.ic8", 0x400000, 0xb388616f, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-18788.ic9", 0x400000, 0xfeae5867, 1 | BRF_PRG | BRF_ESS },

	{ "critcrsh.nv", 0x000080, 0x3da9860e, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Tatacot, Tatacot, Stvbios)
STD_ROM_FN(Tatacot)

static const StvRomLoad TatacotLoads[] = {
	{  0, 0x0000001, 0x0080000, 2 },
	{  0, 0x0100001, 0x0080000, 2 },
	{  0, 0x0200000, 0x0080000, 0 },
	{  0, 0x0300000, 0x0080000, 0 },
	{  1, 0x1c00000, 0x0400000, 1 },
	{  2, 0x2000000, 0x0400000, 1 },
};

// Danchi de Hanafuda (J 990607 V1.400)

static struct BurnRomInfo DanchihRomDesc[] = {
	{ "mpr21974.7", 0x200000, 0xe7472793, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr21970.2", 0x400000, 0x34dd7f4d, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21971.3", 0x400000, 0x8995158c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21972.4", 0x400000, 0x68a39090, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21973.5", 0x400000, 0xb0f23f14, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Danchih, Danchih, Stvbios)
STD_ROM_FN(Danchih)

static const StvRomLoad DanchihLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
};

// Danchi de Quiz: Okusan Yontaku Desuyo! (J 001128 V1.200)

static struct BurnRomInfo DanchiqRomDesc[] = {
	{ "ic22", 0x200000, 0xe216bfc8, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic24", 0x200000, 0xb95aa5ac, 1 | BRF_PRG | BRF_ESS },
	{ "ic26", 0x200000, 0xdf6ebd48, 1 | BRF_PRG | BRF_ESS },
	{ "ic28", 0x200000, 0xcf6a2b76, 1 | BRF_PRG | BRF_ESS },
	{ "ic30", 0x200000, 0x0b6a9901, 1 | BRF_PRG | BRF_ESS },
	{ "ic32", 0x200000, 0x0b4604f5, 1 | BRF_PRG | BRF_ESS },
	{ "ic34", 0x200000, 0x616e20fa, 1 | BRF_PRG | BRF_ESS },
	{ "ic36", 0x200000, 0x43474e08, 1 | BRF_PRG | BRF_ESS },
	{ "ic23", 0x200000, 0xd080eb71, 1 | BRF_PRG | BRF_ESS },
	{ "ic25", 0x200000, 0x9a4109e5, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Danchiq, Danchiq, Stvbios)
STD_ROM_FN(Danchiq)

static const StvRomLoad DanchiqLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
	{  5, 0x0c00000, 0x0200000, 1 },
	{  6, 0x0e00000, 0x0200000, 1 },
	{  7, 0x1000000, 0x0200000, 1 },
	{  8, 0x1200000, 0x0200000, 1 },
	{  9, 0x1400000, 0x0200000, 1 },
};

// Die Hard Arcade (UET 960515 V1.000)

static struct BurnRomInfo DiehardRomDesc[] = {
	{ "fpr19119.13", 0x100000, 0xde5c4f7c, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr19115.2", 0x400000, 0x6fe06a30, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19116.3", 0x400000, 0xaf9e627b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19117.4", 0x400000, 0x74520ff1, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19118.5", 0x400000, 0x2c9702f0, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Diehard, Diehard, Stvbios)
STD_ROM_FN(Diehard)

static const StvRomLoad DiehardLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
};

// Dynamite Deka (J 960515 V1.000)

static struct BurnRomInfo DnmtdekaRomDesc[] = {
	{ "fpr19114.13", 0x100000, 0x1fd22a5f, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr19115.2", 0x400000, 0x6fe06a30, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19116.3", 0x400000, 0xaf9e627b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19117.4", 0x400000, 0x74520ff1, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19118.5", 0x400000, 0x2c9702f0, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Dnmtdeka, Dnmtdeka, Stvbios)
STD_ROM_FN(Dnmtdeka)

static const StvRomLoad DnmtdekaLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
};

// Ejihon Tantei Jimusho (J 950613 V1.000)

static struct BurnRomInfo EjihonRomDesc[] = {
	{ "epr18137.13", 0x080000, 0x151aa9bc, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18138.2", 0x400000, 0xf5567049, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18139.3", 0x400000, 0xf36b4878, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18140.4", 0x400000, 0x228850a0, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18141.5", 0x400000, 0xb51eef36, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18142.6", 0x400000, 0xcf259541, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Ejihon, Ejihon, Stvbios)
STD_ROM_FN(Ejihon)

static const StvRomLoad EjihonLoads[] = {
	{  0, 0x0000001, 0x0080000, 2 },
	{  0, 0x0100001, 0x0080000, 2 },
	{  0, 0x0200000, 0x0080000, 0 },
	{  0, 0x0300000, 0x0080000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
};

// Touryuu Densetsu Elan-Doree / Elan Doree - Legend of Dragoon (JUET 980922 V1.006)

static struct BurnRomInfo ElandoreRomDesc[] = {
	{ "mpr21307.7", 0x200000, 0x966ad472, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr21301.2", 0x400000, 0x1a23b0a0, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21302.3", 0x400000, 0x1c91ca33, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21303.4", 0x400000, 0x07b2350e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21304.5", 0x400000, 0xcfea52ae, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21305.6", 0x400000, 0x46cfc2a2, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21306.1", 0x400000, 0x87a5929c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21308.8", 0x400000, 0x336ec1a4, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Elandore, Elandore, Stvbios)
STD_ROM_FN(Elandore)

static const StvRomLoad ElandoreLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
};

// Final Fight Revenge / Final Revenge (JUET 990714 V1.000)

static struct BurnRomInfo Ffrevng10RomDesc[] = {
	{ "opr21872.7", 0x200000, 0x32d36fee, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr21873.2", 0x400000, 0xdac5bd98, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21874.3", 0x400000, 0x0a7be2f1, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21875.4", 0x400000, 0xccb75029, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21876.5", 0x400000, 0xbb92a7fc, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21877.6", 0x400000, 0xc22a4a75, 1 | BRF_PRG | BRF_ESS },
	{ "opr21878.1", 0x200000, 0x2ea4a64d, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Ffrevng10, Ffrevng10, Stvbios)
STD_ROM_FN(Ffrevng10)

static const StvRomLoad Ffrevng10Loads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0200000, 1 },
};

// Final Fight Revenge / Final Revenge (JUET 990930 V1.100)

static struct BurnRomInfo FfrevengRomDesc[] = {
	{ "ffr110.ic35", 0x100000, 0x3ffea541, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "opr21872.7", 0x200000, 0x32d36fee, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21873.2", 0x400000, 0xdac5bd98, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21874.3", 0x400000, 0x0a7be2f1, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21875.4", 0x400000, 0xccb75029, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21876.5", 0x400000, 0xbb92a7fc, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21877.6", 0x400000, 0xc22a4a75, 1 | BRF_PRG | BRF_ESS },
	{ "opr21878.1", 0x200000, 0x2ea4a64d, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Ffreveng, Ffreveng, Stvbios)
STD_ROM_FN(Ffreveng)

static const StvRomLoad FfrevengLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  1, 0x0200000, 0x0200000, 1 },
	{  2, 0x0400000, 0x0400000, 1 },
	{  3, 0x0800000, 0x0400000, 1 },
	{  4, 0x0c00000, 0x0400000, 1 },
	{  5, 0x1000000, 0x0400000, 1 },
	{  6, 0x1400000, 0x0400000, 1 },
	{  7, 0x1800000, 0x0200000, 1 },
};

// Funky Head Boxers (JUETBKAL 951218 V1.000)

static struct BurnRomInfo FhboxersRomDesc[] = {
	{ "fr18541a.13", 0x100000, 0x8c61a17c, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18538.7", 0x200000, 0x7b5230c5, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18533.2", 0x400000, 0x7181fe51, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18534.3", 0x400000, 0xc87ef125, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18535.4", 0x400000, 0x929a64cf, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18536.5", 0x400000, 0x51b9f64e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18537.6", 0x400000, 0xc364f6a7, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18532.1", 0x400000, 0x39528643, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18539.8", 0x400000, 0x62b3908c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18540.9", 0x400000, 0x4c2b59a4, 1 | BRF_PRG | BRF_ESS },

	{ "fhboxers.nv", 0x000080, 0x590fd6da, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Fhboxers, Fhboxers, Stvbios)
STD_ROM_FN(Fhboxers)

static const StvRomLoad FhboxersLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  1, 0x0200000, 0x0200000, 1 },
	{  2, 0x0400000, 0x0400000, 1 },
	{  3, 0x0800000, 0x0400000, 1 },
	{  4, 0x0c00000, 0x0400000, 1 },
	{  5, 0x1000000, 0x0400000, 1 },
	{  6, 0x1400000, 0x0400000, 1 },
	{  7, 0x1800000, 0x0400000, 1 },
	{  8, 0x1c00000, 0x0400000, 1 },
	{  9, 0x2000000, 0x0400000, 1 },
};

// Zenkoku Seifuku Bishoujo Grand Prix Find Love (J 971212 V1.000)

static struct BurnRomInfo FindloveRomDesc[] = {
	{ "epr20424.13", 0x100000, 0x4e61fa46, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20431.7", 0x200000, 0xea656ced, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20426.2", 0x400000, 0x897d1747, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20427.3", 0x400000, 0xa488a694, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20428.4", 0x400000, 0x4353b3b6, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20429.5", 0x400000, 0x4f566486, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20430.6", 0x400000, 0xd1e11979, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20425.1", 0x400000, 0x67f104c4, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20432.8", 0x400000, 0x79fcdecd, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20433.9", 0x400000, 0x82289f29, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20434.10", 0x400000, 0x85c94afc, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20435.11", 0x400000, 0x263a2e48, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20436.12", 0x400000, 0xe3823f49, 1 | BRF_PRG | BRF_ESS },

	{ "findlove.nv", 0x000080, 0xdf2fa9f6, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Findlove, Findlove, Stvbios)
STD_ROM_FN(Findlove)

static const StvRomLoad FindloveLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  1, 0x0200000, 0x0200000, 1 },
	{  2, 0x0400000, 0x0400000, 1 },
	{  3, 0x0800000, 0x0400000, 1 },
	{  4, 0x0c00000, 0x0400000, 1 },
	{  5, 0x1000000, 0x0400000, 1 },
	{  6, 0x1400000, 0x0400000, 1 },
	{  7, 0x1800000, 0x0400000, 1 },
	{  8, 0x1c00000, 0x0400000, 1 },
	{  9, 0x2000000, 0x0400000, 1 },
	{ 10, 0x2400000, 0x0400000, 1 },
	{ 11, 0x2800000, 0x0400000, 1 },
	{ 12, 0x2c00000, 0x0400000, 1 },
};

// Golden Axe - The Duel (JUETL 950117 V1.000)

static struct BurnRomInfo GaxeduelRomDesc[] = {
	{ "epr17766.13", 0x080000, 0xa83fcd62, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr17768.2", 0x400000, 0xd6808a7d, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17769.3", 0x400000, 0x3471dd35, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17770.4", 0x400000, 0x06978a00, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17771.5", 0x400000, 0xaea2ea3b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17772.6", 0x400000, 0xb3dc0e75, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17767.1", 0x400000, 0x9ba1e7b1, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Gaxeduel, Gaxeduel, Stvbios)
STD_ROM_FN(Gaxeduel)

static const StvRomLoad GaxeduelLoads[] = {
	{  0, 0x0000001, 0x0080000, 2 },
	{  0, 0x0100001, 0x0080000, 2 },
	{  0, 0x0200000, 0x0080000, 0 },
	{  0, 0x0300000, 0x0080000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
};

// Guardian Force (JUET 980318 V0.105)

static struct BurnRomInfo GrdforceRomDesc[] = {
	{ "mpr20844.7", 0x200000, 0x283e7587, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20839.2", 0x400000, 0xfacd4dd8, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20840.3", 0x400000, 0xfe0158e6, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20841.4", 0x400000, 0xd87ac873, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20842.5", 0x400000, 0xbaebc506, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20843.6", 0x400000, 0x263e49cc, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Grdforce, Grdforce, Stvbios)
STD_ROM_FN(Grdforce)

static const StvRomLoad GrdforceLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
};

// Groove on Fight - Gouketsuji Ichizoku 3 (J 970416 V1.001)

static struct BurnRomInfo GroovefRomDesc[] = {
	{ "mpr19820.7", 0x100000, 0xe93c4513, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr19815.2", 0x400000, 0x1b9b14e6, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19816.3", 0x400000, 0x83f5731c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19817.4", 0x400000, 0x525bd6c7, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19818.5", 0x400000, 0x66723ba8, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19819.6", 0x400000, 0xee8c55f4, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19814.1", 0x400000, 0x8f20e9f7, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19821.8", 0x400000, 0xf69a76e6, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19822.9", 0x200000, 0x5e8c4b5f, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Groovef, Groovef, Stvbios)
STD_ROM_FN(Groovef)

static const StvRomLoad GroovefLoads[] = {
	{  0, 0x0200000, 0x0100000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0200000, 1 },
};

// Sakura Taisen - Hanagumi Taisen Columns (J 971007 V1.010)

static struct BurnRomInfo HanagumiRomDesc[] = {
	{ "mpr20143.7", 0x100000, 0x7bfc38d0, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20138.2", 0x400000, 0xfdcf1046, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20139.3", 0x400000, 0x7f0140e5, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20140.4", 0x400000, 0x2fa03852, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20141.5", 0x400000, 0x45d6d21b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20142.6", 0x400000, 0xe38561ec, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20137.1", 0x400000, 0x181d2688, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20144.8", 0x400000, 0x235b43f6, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20145.9", 0x400000, 0xaeaac7a1, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20146.10", 0x400000, 0x39bab9a2, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20147.11", 0x400000, 0x294ab997, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20148.12", 0x400000, 0x5337ccb0, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Hanagumi, Hanagumi, Stvbios)
STD_ROM_FN(Hanagumi)

static const StvRomLoad HanagumiLoads[] = {
	{  0, 0x0200000, 0x0100000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0400000, 1 },
	{  9, 0x2400000, 0x0400000, 1 },
	{ 10, 0x2800000, 0x0400000, 1 },
	{ 11, 0x2c00000, 0x0400000, 1 },
};

// Karaoke Quiz Intro Don Don! (J 960213 V1.000)

static struct BurnRomInfo IntrodonRomDesc[] = {
	{ "epr18937.13", 0x080000, 0x1f40d766, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18944.7", 0x100000, 0xf7f75ce5, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18939.2", 0x400000, 0xef95a6e6, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18940.3", 0x400000, 0xcabab4cd, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18941.4", 0x400000, 0xf4a33a20, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18942.5", 0x400000, 0x8dd0a446, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18943.6", 0x400000, 0xd8702a9e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18938.1", 0x400000, 0x580ecb83, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Introdon, Introdon, Stvbios)
STD_ROM_FN(Introdon)

static const StvRomLoad IntrodonLoads[] = {
	{  0, 0x0000001, 0x0080000, 2 },
	{  0, 0x0100001, 0x0080000, 2 },
	{  1, 0x0200000, 0x0100000, 1 },
	{  1, 0x0300000, 0x0100000, 1 },
	{  2, 0x0400000, 0x0400000, 1 },
	{  3, 0x0800000, 0x0400000, 1 },
	{  4, 0x0c00000, 0x0400000, 1 },
	{  5, 0x1000000, 0x0400000, 1 },
	{  6, 0x1400000, 0x0400000, 1 },
	{  7, 0x1800000, 0x0400000, 1 },
};

// Pro Mahjong Kiwame S (J 951020 V1.208)

static struct BurnRomInfo KiwamesRomDesc[] = {
	{ "epr18737.13", 0x080000, 0xcfad6c49, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18738.2", 0x400000, 0x4b3c175a, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18739.3", 0x400000, 0xeb41fa67, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18740.4", 0x200000, 0x9ca7962f, 1 | BRF_PRG | BRF_ESS },

	{ "kiwames.nv", 0x000080, 0xc7002732, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Kiwames, Kiwames, Stvbios)
STD_ROM_FN(Kiwames)

static const StvRomLoad KiwamesLoads[] = {
	{  0, 0x0000001, 0x0080000, 2 },
	{  0, 0x0100001, 0x0080000, 2 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0200000, 1 },
};

// Maru-Chan de Goo! (J 971216 V1.000)

static struct BurnRomInfo MaruchanRomDesc[] = {
	{ "epr20416.13", 0x100000, 0x8bf0176d, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20417.2", 0x400000, 0x636c2a08, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20418.3", 0x400000, 0x3f0d9e34, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20419.4", 0x400000, 0xec969815, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20420.5", 0x400000, 0xf2902c88, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20421.6", 0x400000, 0xcd0b477c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20422.1", 0x400000, 0x66335049, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20423.8", 0x400000, 0x2bd55832, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20443.9", 0x400000, 0x8ac288f5, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Maruchan, Maruchan, Stvbios)
STD_ROM_FN(Maruchan)

static const StvRomLoad MaruchanLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0400000, 1 },
};

// Mausuke no Ojama the World (J 960314 V1.000)

static struct BurnRomInfo MausukeRomDesc[] = {
	{ "ic13.bin", 0x100000, 0xb456f4cd, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mcj-00.2", 0x200000, 0x4eeacd6f, 1 | BRF_PRG | BRF_ESS },
	{ "mcj-01.3", 0x200000, 0x365a494b, 1 | BRF_PRG | BRF_ESS },
	{ "mcj-02.4", 0x200000, 0x8b8e4931, 1 | BRF_PRG | BRF_ESS },
	{ "mcj-03.5", 0x200000, 0x9015a0e7, 1 | BRF_PRG | BRF_ESS },
	{ "mcj-04.6", 0x200000, 0x9d1beaee, 1 | BRF_PRG | BRF_ESS },
	{ "mcj-05.1", 0x200000, 0xa7626a82, 1 | BRF_PRG | BRF_ESS },
	{ "mcj-06.8", 0x200000, 0x1ab8e90e, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Mausuke, Mausuke, Stvbios)
STD_ROM_FN(Mausuke)

static const StvRomLoad MausukeLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0800000, 0x0200000, 1 },
	{  3, 0x0c00000, 0x0200000, 1 },
	{  4, 0x1000000, 0x0200000, 1 },
	{  5, 0x1400000, 0x0200000, 1 },
	{  6, 0x1800000, 0x0200000, 1 },
	{  7, 0x1c00000, 0x0200000, 1 },
};

// Microman Battle Charge (J 990326 V1.000)

static struct BurnRomInfo MicrombcRomDesc[] = {
	{ "ic22", 0x200000, 0x8385bc45, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic24", 0x200000, 0x84ecb42f, 1 | BRF_PRG | BRF_ESS },
	{ "ic26", 0x200000, 0x869bc19c, 1 | BRF_PRG | BRF_ESS },
	{ "ic28", 0x200000, 0x0c3db354, 1 | BRF_PRG | BRF_ESS },
	{ "ic30", 0x200000, 0x03b9eacf, 1 | BRF_PRG | BRF_ESS },
	{ "ic32", 0x200000, 0x62c10626, 1 | BRF_PRG | BRF_ESS },
	{ "ic34", 0x200000, 0x8d89877e, 1 | BRF_PRG | BRF_ESS },
	{ "ic36", 0x200000, 0x8d89877e, 1 | BRF_PRG | BRF_ESS },

	{ "micrombc.nv", 0x000080, 0x6e89815f, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Micrombc, Micrombc, Stvbios)
STD_ROM_FN(Micrombc)

static const StvRomLoad MicrombcLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
	{  5, 0x0c00000, 0x0200000, 1 },
	{  6, 0x1000000, 0x0200000, 1 },
	{  7, 0x1200000, 0x0200000, 1 },
};

// Virtual Mahjong 2 - My Fair Lady (J 980608 V1.000)

static struct BurnRomInfo MyfairldRomDesc[] = {
	{ "mpr21000.7", 0x200000, 0x2581c560, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20995.2", 0x400000, 0x1bb73f24, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20996.3", 0x400000, 0x993c3859, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20997.4", 0x400000, 0xf0bf64a4, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20998.5", 0x400000, 0xd3b19786, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20999.6", 0x400000, 0x82e31f25, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20994.1", 0x400000, 0xa69243a0, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21001.8", 0x400000, 0x95fbe549, 1 | BRF_PRG | BRF_ESS },

	{ "myfairld.nv", 0x000080, 0xc7cf3a5a, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Myfairld, Myfairld, Stvbios)
STD_ROM_FN(Myfairld)

static const StvRomLoad MyfairldLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
};

// Othello Shiyouyo (J 980423 V1.002)

static struct BurnRomInfo OthellosRomDesc[] = {
	{ "mpr20967.7", 0x200000, 0xefc05b97, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20963.2", 0x400000, 0x2cc4f141, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20964.3", 0x400000, 0x5f5cda94, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20965.4", 0x400000, 0x37044f3e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20966.5", 0x400000, 0xb94b83de, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Othellos, Othellos, Stvbios)
STD_ROM_FN(Othellos)

static const StvRomLoad OthellosLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
};

// Hashire Patrol Car (J 990326 V1.000)

static struct BurnRomInfo PatocarRomDesc[] = {
	{ "ic22.bin", 0x200000, 0xb7e6d425, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic24.bin", 0x200000, 0xcbbb687e, 1 | BRF_PRG | BRF_ESS },
	{ "ic26.bin", 0x200000, 0x91db9dbe, 1 | BRF_PRG | BRF_ESS },
	{ "ic28.bin", 0x200000, 0xbff0cd9c, 1 | BRF_PRG | BRF_ESS },
	{ "ic30.bin", 0x200000, 0x9a4109e5, 1 | BRF_PRG | BRF_ESS },

	{ "patocar.nv", 0x000080, 0xd9873ee8, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Patocar, Patocar, Stvbios)
STD_ROM_FN(Patocar)

static const StvRomLoad PatocarLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
};

// Pebble Beach - The Great Shot (JUE 950913 V0.990)

static struct BurnRomInfo PblbeachRomDesc[] = {
	{ "epr18852.13", 0x080000, 0xd12414ec, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18853.2", 0x400000, 0xb9268c97, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18854.3", 0x400000, 0x3113c8bc, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18855.4", 0x400000, 0xdaf6ad0c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18856.5", 0x400000, 0x214cef24, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Pblbeach, Pblbeach, Stvbios)
STD_ROM_FN(Pblbeach)

static const StvRomLoad PblbeachLoads[] = {
	{  0, 0x0000001, 0x0080000, 2 },
	{  0, 0x0100001, 0x0080000, 2 },
	{  0, 0x0200000, 0x0080000, 0 },
	{  0, 0x0300000, 0x0080000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
};

// Princess Clara Daisakusen (J 960910 V1.000)

static struct BurnRomInfo PrikuraRomDesc[] = {
	{ "mpr19337.7", 0x200000, 0x76f69ff3, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr19333.2", 0x400000, 0xeb57a6a6, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19334.3", 0x400000, 0xc9979981, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19335.4", 0x400000, 0x9e000140, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19336.5", 0x400000, 0x2363fa4b, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Prikura, Prikura, Stvbios)
STD_ROM_FN(Prikura)

static const StvRomLoad PrikuraLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
};

// Puyo Puyo Sun (J 961115 V0.001)

static struct BurnRomInfo PuyosunRomDesc[] = {
	{ "epr19531.13", 0x080000, 0xac81024f, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr19533.2", 0x400000, 0x17ec54ba, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19534.3", 0x400000, 0x820e4781, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19535.4", 0x400000, 0x94fadfa4, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19536.5", 0x400000, 0x5765bc9c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19537.6", 0x400000, 0x8b736686, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19532.1", 0x400000, 0x985f0c9d, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19538.8", 0x400000, 0x915a723e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19539.9", 0x400000, 0x72a297e5, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Puyosun, Puyosun, Stvbios)
STD_ROM_FN(Puyosun)

static const StvRomLoad PuyosunLoads[] = {
	{  0, 0x0000001, 0x0080000, 2 },
	{  0, 0x0100001, 0x0080000, 2 },
	{  0, 0x0200000, 0x0080000, 0 },
	{  0, 0x0300000, 0x0080000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0400000, 1 },
};

// Radiant Silvergun (JUET 980523 V1.000)

static struct BurnRomInfo RsgunRomDesc[] = {
	{ "mpr20958.7", 0x200000, 0xcbe5a449, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20959.2", 0x400000, 0xa953330b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20960.3", 0x400000, 0xb5ab9053, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20961.4", 0x400000, 0x0e06295c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20962.5", 0x400000, 0xf1e6c7fc, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Rsgun, Rsgun, Stvbios)
STD_ROM_FN(Rsgun)

static const StvRomLoad RsgunLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
};

// Taisen Tanto-R Sashissu!! (J 980216 V1.000)

static struct BurnRomInfo SasissuRomDesc[] = {
	{ "epr20542.13", 0x100000, 0x0e632db5, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20544.2", 0x400000, 0x661fff5e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20545.3", 0x400000, 0x8e3a37be, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20546.4", 0x400000, 0x72020886, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20547.5", 0x400000, 0x8362e397, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20548.6", 0x400000, 0xe37534d9, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20543.1", 0x400000, 0x1f688cdf, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Sasissu, Sasissu, Stvbios)
STD_ROM_FN(Sasissu)

static const StvRomLoad SasissuLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
};

// DaeJeon! SanJeon SuJeon (AJTUE 990412 V1.000)

static struct BurnRomInfo SanjeonRomDesc[] = {
	{ "ic11", 0x200000, 0x9abae8d4, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic13", 0x200000, 0xf72c1d13, 1 | BRF_PRG | BRF_ESS },
	{ "ic14", 0x200000, 0xbcd72105, 1 | BRF_PRG | BRF_ESS },
	{ "ic15", 0x200000, 0x8c9c8352, 1 | BRF_PRG | BRF_ESS },
	{ "ic16", 0x200000, 0x07e11512, 1 | BRF_PRG | BRF_ESS },
	{ "ic17", 0x200000, 0x46b7b344, 1 | BRF_PRG | BRF_ESS },
	{ "ic18", 0x200000, 0xd48404e1, 1 | BRF_PRG | BRF_ESS },
	{ "ic19", 0x200000, 0x33d23bb9, 1 | BRF_PRG | BRF_ESS },
	{ "ic20", 0x200000, 0xf8cc1038, 1 | BRF_PRG | BRF_ESS },
	{ "ic21", 0x200000, 0x74ceb649, 1 | BRF_PRG | BRF_ESS },
	{ "ic22", 0x200000, 0x85f31277, 1 | BRF_PRG | BRF_ESS },
	{ "ic12", 0x400000, 0xd5ebc84e, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Sanjeon, Sanjeon, Stvbios)
STD_ROM_FN(Sanjeon)

static const StvRomLoad SanjeonLoads[] = {
	{  0, 0x0000001, 0x0200000, 2 },
	{  1, 0x0400000, 0x0200000, 0 },
	{  2, 0x0600000, 0x0200000, 0 },
	{  3, 0x0800000, 0x0200000, 0 },
	{  4, 0x0a00000, 0x0200000, 0 },
	{  5, 0x0c00000, 0x0200000, 0 },
	{  6, 0x0e00000, 0x0200000, 0 },
	{  7, 0x1000000, 0x0200000, 0 },
	{  8, 0x1200000, 0x0200000, 0 },
	{  9, 0x1400000, 0x0200000, 0 },
	{ 10, 0x1600000, 0x0200000, 0 },
	{ 11, 0x1800000, 0x0400000, 0 },
};

// Sea Bass Fishing (JUET 971110 V0.001)

static struct BurnRomInfo SeabassRomDesc[] = {
	{ "seabassf.13", 0x100000, 0x6d7c39cc, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20551.2", 0x400000, 0x9a0c6dd8, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20552.3", 0x400000, 0x5f46b0aa, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20553.4", 0x400000, 0xc0f8a6b6, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20554.5", 0x400000, 0x215fc1f9, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20555.6", 0x400000, 0x3f5186a9, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20550.1", 0x400000, 0x083e1ca8, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20556.8", 0x400000, 0x1fd70c6c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20557.9", 0x400000, 0x3c9ba442, 1 | BRF_PRG | BRF_ESS },

	{ "seabass.nv", 0x000080, 0x4e7c0944, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Seabass, Seabass, Stvbios)
STD_ROM_FN(Seabass)

static const StvRomLoad SeabassLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0400000, 1 },
};

// Shanghai - The Great Wall / Shanghai Triple Threat (JUE 950623 V1.005)

static struct BurnRomInfo ShanhigwRomDesc[] = {
	{ "mpr18341.7", 0x200000, 0xcc5e8646, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18340.2", 0x200000, 0x8db23212, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Shanhigw, Shanhigw, Stvbios)
STD_ROM_FN(Shanhigw)

static const StvRomLoad ShanhigwLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
};

// Shienryu (JUET 961226 V1.000)

static struct BurnRomInfo ShienryuRomDesc[] = {
	{ "mpr19631.7", 0x200000, 0x3a4b1abc, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr19632.2", 0x400000, 0x985fae46, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19633.3", 0x400000, 0xe2f0b037, 1 | BRF_PRG | BRF_ESS },

	{ "eeprom-shienryu.bin", 0x000080, 0x98db6925, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Shienryu, Shienryu, Stvbios)
STD_ROM_FN(Shienryu)

static const StvRomLoad ShienryuLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
};

// Steep Slope Sliders (JUET 981110 V1.000)

static struct BurnRomInfo SssRomDesc[] = {
	{ "epr21488.13", 0x080000, 0x71c9def1, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr21489.2", 0x400000, 0x4c85152b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21490.3", 0x400000, 0x03da67f8, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21491.4", 0x400000, 0xcf7ee784, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21492.5", 0x400000, 0x57753894, 1 | BRF_PRG | BRF_ESS },
	{ "mpr21493.6", 0x400000, 0xefb2d271, 1 | BRF_PRG | BRF_ESS },

	{ "sss.nv", 0x000080, 0x3473b2f3, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Sss, Sss, Stvbios)
STD_ROM_FN(Sss)

static const StvRomLoad SssLoads[] = {
	{  0, 0x0000001, 0x0080000, 2 },
	{  0, 0x0100001, 0x0080000, 2 },
	{  0, 0x0200000, 0x0080000, 0 },
	{  0, 0x0300000, 0x0080000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
};

// Puzzle & Action: Sando-R (J 951114 V1.000)

static struct BurnRomInfo SandorRomDesc[] = {
	{ "sando-r.13", 0x100000, 0xfe63a239, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18635.8", 0x400000, 0x441e1368, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18636.9", 0x400000, 0xfff1dd80, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18637.10", 0x400000, 0x83aced0f, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18638.11", 0x400000, 0xcaab531b, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Sandor, Sandor, Stvbios)
STD_ROM_FN(Sandor)

static const StvRomLoad SandorLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x1c00000, 0x0400000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x2000000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x2400000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x2800000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
};

// Puzzle & Action: Treasure Hunt (JUET 970901 V2.00E)

static struct BurnRomInfo ThuntRomDesc[] = {
	{ "th-ic7_2.stv", 0x080000, 0xc4e993de, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "th-ic7_1.stv", 0x080000, 0x1355cc18, 1 | BRF_PRG | BRF_ESS },
	{ "th-e-2.ic2", 0x400000, 0x47315694, 1 | BRF_PRG | BRF_ESS },
	{ "th-e-3.ic3", 0x400000, 0xc9290b44, 1 | BRF_PRG | BRF_ESS },
	{ "th-e-4.ic4", 0x400000, 0xc672e40b, 1 | BRF_PRG | BRF_ESS },
	{ "th-e-5.ic5", 0x400000, 0x3914b805, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Thunt, Thunt, Stvbios)
STD_ROM_FN(Thunt)

static const StvRomLoad ThuntLoads[] = {
	{  0, 0x0200000, 0x0080000, 2 },
	{  1, 0x0200001, 0x0080000, 2 },
	{  2, 0x0400000, 0x0400000, 1 },
	{  3, 0x0800000, 0x0400000, 1 },
	{  4, 0x0c00000, 0x0400000, 1 },
	{  5, 0x1000000, 0x0400000, 1 },
};

// Puzzle & Action: BoMulEul Chajara (JUET 970125 V2.00K)

static struct BurnRomInfo ThuntkRomDesc[] = {
	{ "2.ic13_2", 0x080000, 0x6cae2926, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "1.ic13_1", 0x080000, 0x460727c8, 1 | BRF_PRG | BRF_ESS },
	{ "bom210-10.ic2", 0x400000, 0xf59d0827, 1 | BRF_PRG | BRF_ESS },
	{ "bom210-11.ic3", 0x400000, 0x44e5a13e, 1 | BRF_PRG | BRF_ESS },
	{ "bom210-12.ic4", 0x400000, 0xdeabc701, 1 | BRF_PRG | BRF_ESS },
	{ "bom210-13.ic5", 0x400000, 0x5ece1d5c, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Thuntk, Thuntk, Stvbios)
STD_ROM_FN(Thuntk)

static const StvRomLoad ThuntkLoads[] = {
	{  0, 0x0200000, 0x0080000, 2 },
	{  1, 0x0200001, 0x0080000, 2 },
	{  2, 0x1c00000, 0x0400000, 0 },
	{  2, 0x0400000, 0x0400000, 0 },
	{  3, 0x2000000, 0x0400000, 0 },
	{  3, 0x0800000, 0x0400000, 0 },
	{  4, 0x2400000, 0x0400000, 0 },
	{  4, 0x0c00000, 0x0400000, 0 },
	{  5, 0x2800000, 0x0400000, 0 },
	{  5, 0x1000000, 0x0400000, 0 },
};

// Sky Challenger (J 000406 V1.000)

static struct BurnRomInfo SkychalRomDesc[] = {
	{ "ic22.bin", 0x200000, 0xa12ccf64, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic24.bin", 0x200000, 0x9a929dcf, 1 | BRF_PRG | BRF_ESS },
	{ "ic26.bin", 0x200000, 0xed2183d3, 1 | BRF_PRG | BRF_ESS },
	{ "ic28.bin", 0x200000, 0xe7401d68, 1 | BRF_PRG | BRF_ESS },
	{ "ic30.bin", 0x200000, 0x950f7a2f, 1 | BRF_PRG | BRF_ESS },
	{ "ic32.bin", 0x200000, 0xa656212b, 1 | BRF_PRG | BRF_ESS },

	{ "skychal.nv", 0x000080, 0xa6515237, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Skychal, Skychal, Stvbios)
STD_ROM_FN(Skychal)

static const StvRomLoad SkychalLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
	{  5, 0x0c00000, 0x0200000, 1 },
};

// Super Major League (U 960108 V1.000)

static struct BurnRomInfo SmleagueRomDesc[] = {
	{ "epr18777.13", 0x080000, 0x8d180866, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18778.8", 0x400000, 0x25e1300e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18779.9", 0x400000, 0x51e2fabd, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18780.10", 0x400000, 0x8cd4dd74, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18781.11", 0x400000, 0x13ee41ae, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18782.12", 0x200000, 0x9be2270a, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Smleague, Smleague, Stvbios)
STD_ROM_FN(Smleague)

static const StvRomLoad SmleagueLoads[] = {
	{  0, 0x0000001, 0x0080000, 2 },
	{  0, 0x0100001, 0x0080000, 2 },
	{  0, 0x0200000, 0x0080000, 0 },
	{  0, 0x0300000, 0x0080000, 0 },
	{  1, 0x1c00000, 0x0400000, 1 },
	{  2, 0x2000000, 0x0400000, 1 },
	{  3, 0x2400000, 0x0400000, 1 },
	{  4, 0x2800000, 0x0400000, 1 },
	{  5, 0x2c00000, 0x0200000, 1 },
};

// Final Arch (J 950714 V1.001)

static struct BurnRomInfo FinlarchRomDesc[] = {
	{ "finlarch.13", 0x100000, 0x4505fa9e, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18257.2", 0x400000, 0x137fdf55, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18258.3", 0x400000, 0xf519c505, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18259.4", 0x400000, 0x5cabc775, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18260.5", 0x400000, 0xf5b92082, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Finlarch, Finlarch, Stvbios)
STD_ROM_FN(Finlarch)

static const StvRomLoad FinlarchLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  1, 0x1400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  2, 0x1800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  3, 0x1c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  4, 0x2000000, 0x0400000, 1 },
};

// Soukyugurentai / Terra Diver (JUET 960821 V1.000)

static struct BurnRomInfo SokyugrtRomDesc[] = {
	{ "fpr19188.13", 0x100000, 0x45a27e32, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr19189.2", 0x400000, 0x0b202a3e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19190.3", 0x400000, 0x1777ded8, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19191.4", 0x400000, 0xec6eb07b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19192.5", 0x200000, 0xcb544a1e, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Sokyugrt, Sokyugrt, Stvbios)
STD_ROM_FN(Sokyugrt)

static const StvRomLoad SokyugrtLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0200000, 1 },
};

// Suiko Enbu / Outlaws of the Lost Dynasty (JUETL 950314 V2.001)

static struct BurnRomInfo SuikoenbRomDesc[] = {
	{ "fpr17834.13", 0x100000, 0x746ef686, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr17836.2", 0x400000, 0x55e9642d, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17837.3", 0x400000, 0x13d1e667, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17838.4", 0x400000, 0xf9e70032, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17839.5", 0x400000, 0x1b2762c5, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17840.6", 0x400000, 0x0fd4c857, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17835.1", 0x400000, 0x77f5cb43, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17841.8", 0x400000, 0xf48beffc, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17842.9", 0x400000, 0xac8deed7, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Suikoenb, Suikoenb, Stvbios)
STD_ROM_FN(Suikoenb)

static const StvRomLoad SuikoenbLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0400000, 1 },
};

// Nerae! Super Goal (J 981218 V1.000)

static struct BurnRomInfo SupgoalRomDesc[] = {
	{ "ic22.bin", 0x200000, 0xa686f7a2, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic24.bin", 0x200000, 0x56fbbeea, 1 | BRF_PRG | BRF_ESS },
	{ "ic26.bin", 0x200000, 0x64701c2b, 1 | BRF_PRG | BRF_ESS },
	{ "ic28.bin", 0x200000, 0xd9aebe8c, 1 | BRF_PRG | BRF_ESS },
	{ "ic30.bin", 0x200000, 0x26d4ade5, 1 | BRF_PRG | BRF_ESS },

	{ "supgoal.nv", 0x000080, 0x63806aae, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Supgoal, Supgoal, Stvbios)
STD_ROM_FN(Supgoal)

static const StvRomLoad SupgoalLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
};

// Technical Bowling (J 971212 V1.000)

static struct BurnRomInfo TechbowlRomDesc[] = {
	{ "ic22", 0x200000, 0x5058db21, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic24", 0x200000, 0x34090f6d, 1 | BRF_PRG | BRF_ESS },
	{ "ic26", 0x200000, 0xfb073352, 1 | BRF_PRG | BRF_ESS },
	{ "ic28", 0x200000, 0x530e0ceb, 1 | BRF_PRG | BRF_ESS },
	{ "ic30", 0x200000, 0x8d89877e, 1 | BRF_PRG | BRF_ESS },

	{ "techbowl.nv", 0x000080, 0x5bebc2b7, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Techbowl, Techbowl, Stvbios)
STD_ROM_FN(Techbowl)

static const StvRomLoad TechbowlLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
};

// Virtua Fighter Kids (JUET 960319 V0.000)

static struct BurnRomInfo VfkidsRomDesc[] = {
	{ "fpr18914.13", 0x100000, 0xcd35730a, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18916.4", 0x400000, 0x4aae3ddb, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18917.5", 0x400000, 0xedf6edc3, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18918.6", 0x400000, 0xd3a95036, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18915.1", 0x400000, 0x09cc38e5, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18919.8", 0x400000, 0x4ac700de, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18920.9", 0x400000, 0x0106e36c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18921.10", 0x400000, 0xc23d51ad, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18922.11", 0x400000, 0x99d0ab90, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18923.12", 0x400000, 0x30a41ae9, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Vfkids, Vfkids, Stvbios)
STD_ROM_FN(Vfkids)

static const StvRomLoad VfkidsLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0c00000, 0x0400000, 1 },
	{  2, 0x1000000, 0x0400000, 1 },
	{  3, 0x1400000, 0x0400000, 1 },
	{  4, 0x1800000, 0x0400000, 1 },
	{  5, 0x1c00000, 0x0400000, 1 },
	{  6, 0x2000000, 0x0400000, 1 },
	{  7, 0x2400000, 0x0400000, 1 },
	{  8, 0x2800000, 0x0400000, 1 },
	{  9, 0x2c00000, 0x0400000, 1 },
};

// Virtual Mahjong (J 961214 V1.000)

static struct BurnRomInfo VmahjongRomDesc[] = {
	{ "mpr19620.7", 0x200000, 0xc98de7e5, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr19615.2", 0x400000, 0xc62896da, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19616.3", 0x400000, 0xf62207c7, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19617.4", 0x400000, 0xab667e19, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19618.5", 0x400000, 0x9782ceee, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19619.6", 0x400000, 0x0b76866c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19614.1", 0x400000, 0xb83b3f03, 1 | BRF_PRG | BRF_ESS },
	{ "mpr19621.8", 0x400000, 0xf92616b3, 1 | BRF_PRG | BRF_ESS },

	{ "vmahjong.nv", 0x000080, 0x4e6487f4, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Vmahjong, Vmahjong, Stvbios)
STD_ROM_FN(Vmahjong)

static const StvRomLoad VmahjongLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
};

// Waku Waku Shinkansen (J 971031 V0.002)

static struct BurnRomInfo WwshinRomDesc[] = {
	{ "ic22", 0x200000, 0x43f7cafc, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic24", 0x200000, 0xdb560e59, 1 | BRF_PRG | BRF_ESS },
	{ "ic26", 0x200000, 0x2f411ff1, 1 | BRF_PRG | BRF_ESS },
	{ "ic28", 0x200000, 0xca5e0446, 1 | BRF_PRG | BRF_ESS },
	{ "ic30", 0x200000, 0x2653f758, 1 | BRF_PRG | BRF_ESS },
	{ "ic32", 0x200000, 0x8ebfd947, 1 | BRF_PRG | BRF_ESS },
	{ "ic34", 0x200000, 0x22b7180f, 1 | BRF_PRG | BRF_ESS },
	{ "ic36", 0x200000, 0x5b83914c, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Wwshin, Wwshin, Stvbios)
STD_ROM_FN(Wwshin)

static const StvRomLoad WwshinLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
	{  5, 0x0c00000, 0x0200000, 1 },
	{  6, 0x0e00000, 0x0200000, 1 },
	{  7, 0x1000000, 0x0200000, 1 },
};

// Wanpaku Safari (J 981109 V1.000)

static struct BurnRomInfo WasafariRomDesc[] = {
	{ "ic22.bin", 0x200000, 0xd8bb2e2c, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic24.bin", 0x200000, 0xc1b0e173, 1 | BRF_PRG | BRF_ESS },
	{ "ic26.bin", 0x200000, 0xa5c0577c, 1 | BRF_PRG | BRF_ESS },
	{ "ic28.bin", 0x200000, 0x2a8cfa97, 1 | BRF_PRG | BRF_ESS },
	{ "ic30.bin", 0x200000, 0x9a4109e5, 1 | BRF_PRG | BRF_ESS },

	{ "wasafari.nv", 0x000080, 0x50861c5a, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Wasafari, Wasafari, Stvbios)
STD_ROM_FN(Wasafari)

static const StvRomLoad WasafariLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
};

// Winter Heat (JUET 971012 V1.000)

static struct BurnRomInfo WinterhtRomDesc[] = {
	{ "fpr20108.13", 0x100000, 0x1ef9ced0, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20110.2", 0x400000, 0x238ef832, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20111.3", 0x400000, 0xb0a86f69, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20112.4", 0x400000, 0x3ba2b49b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20113.5", 0x400000, 0x8c858b41, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20114.6", 0x400000, 0xb723862c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20109.1", 0x400000, 0xc1a713b8, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20115.8", 0x400000, 0xdd01f2ad, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Winterht, Winterht, Stvbios)
STD_ROM_FN(Winterht)

static const StvRomLoad WinterhtLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
};

// Zen Nippon Pro-Wres Featuring Virtua (J 971123 V1.000)

static struct BurnRomInfo ZnpwfvRomDesc[] = {
	{ "epr20398.13", 0x100000, 0x3fb56a0b, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20400.2", 0x400000, 0x1edfbe05, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20401.3", 0x400000, 0x99e98937, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20402.4", 0x400000, 0x4572aa60, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20403.5", 0x400000, 0x26a8e13e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20404.6", 0x400000, 0x0b70275d, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20399.1", 0x400000, 0xc178a96e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20405.8", 0x400000, 0xf53337b7, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20406.9", 0x400000, 0xb677c175, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20407.10", 0x400000, 0x58356050, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Znpwfv, Znpwfv, Stvbios)
STD_ROM_FN(Znpwfv)

static const StvRomLoad ZnpwfvLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0400000, 1 },
	{  9, 0x2400000, 0x0400000, 1 },
};

// All Japan Pro-Wrestling Featuring Virtua (T 971123 V1.000)

static struct BurnRomInfo ZnpwfvtRomDesc[] = {
	{ "epr20408.13", 0x100000, 0x1d62fcf6, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20400.2", 0x400000, 0x1edfbe05, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20401.3", 0x400000, 0x99e98937, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20402.4", 0x400000, 0x4572aa60, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20403.5", 0x400000, 0x26a8e13e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20404.6", 0x400000, 0x0b70275d, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20399.1", 0x400000, 0xc178a96e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20405.8", 0x400000, 0xf53337b7, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20406.9", 0x400000, 0xb677c175, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20407.10", 0x400000, 0x58356050, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Znpwfvt, Znpwfvt, Stvbios)
STD_ROM_FN(Znpwfvt)

static const StvRomLoad ZnpwfvtLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0400000, 1 },
	{  9, 0x2400000, 0x0400000, 1 },
};

// Stress Busters (J 981020 V1.000)

static struct BurnRomInfo StressRomDesc[] = {
	{ "epr-21300a.ic13", 0x100000, 0x899d829e, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr-21290.ic2", 0x400000, 0xa49d29f3, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21291.ic3", 0x400000, 0x9452ba20, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21292.ic4", 0x400000, 0xf60268e2, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21293.ic5", 0x400000, 0x794946e0, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21294.ic6", 0x400000, 0x550843bb, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21289.ic1", 0x400000, 0xc2ee8bea, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21296.ic8", 0x400000, 0xb825c42a, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21297.ic9", 0x400000, 0x4bff7469, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21298.ic10", 0x400000, 0x68d07144, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21299.ic11", 0x400000, 0xecc521c6, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Stress, Stress, Stvbios)
STD_ROM_FN(Stress)

static const StvRomLoad StressLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0400000, 1 },
	{  9, 0x2400000, 0x0400000, 1 },
	{ 10, 0x2800000, 0x0400000, 1 },
};

// Virtua Fighter Remix (JUETBKAL 950428 V1.000)

static struct BurnRomInfo VfremixRomDesc[] = {
	{ "epr17944.13", 0x080000, 0x3304c175, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr17946.2", 0x400000, 0x4cb245f7, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17947.3", 0x400000, 0xfef4a9fb, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17948.4", 0x400000, 0x3e2b251a, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17949.5", 0x400000, 0xb2ecea25, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17950.6", 0x400000, 0x5b1f981d, 1 | BRF_PRG | BRF_ESS },
	{ "mpr17945.1", 0x200000, 0x03ede188, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Vfremix, Vfremix, Stvbios)
STD_ROM_FN(Vfremix)

static const StvRomLoad VfremixLoads[] = {
	{  0, 0x0000001, 0x0080000, 2 },
	{  0, 0x0100001, 0x0080000, 2 },
	{  0, 0x0200000, 0x0080000, 0 },
	{  0, 0x0300000, 0x0080000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0200000, 1 },
};

// Decathlete (JUET 960709 V1.001)

static struct BurnRomInfo DecathltRomDesc[] = {
	{ "epr18967a.13", 0x100000, 0xac59c186, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18968.2", 0x400000, 0x11a891de, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18969.3", 0x400000, 0x199cc47d, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18970.4", 0x400000, 0x8b7a509e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18971.5", 0x400000, 0xc87c443b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18972.6", 0x400000, 0x45c64fca, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Decathlt, Decathlt, Stvbios)
STD_ROM_FN(Decathlt)

static const StvRomLoad DecathltLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
};

// Decathlete (JUET 960424 V1.000)

static struct BurnRomInfo DecathltoRomDesc[] = {
	{ "epr18967.13", 0x100000, 0xc0446674, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr18968.2", 0x400000, 0x11a891de, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18969.3", 0x400000, 0x199cc47d, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18970.4", 0x400000, 0x8b7a509e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18971.5", 0x400000, 0xc87c443b, 1 | BRF_PRG | BRF_ESS },
	{ "mpr18972.6", 0x400000, 0x45c64fca, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Decathlto, Decathlto, Stvbios)
STD_ROM_FN(Decathlto)

static const StvRomLoad DecathltoLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
};

// Tecmo World Cup '98 (JUET 980410 V1.000)

static struct BurnRomInfo Twcup98RomDesc[] = {
	{ "epr20819.24", 0x100000, 0xd930dfc8, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20821.12", 0x400000, 0x2d930d23, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20822.13", 0x400000, 0x8b33a5e2, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20823.14", 0x400000, 0x6e6d4e95, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20824.15", 0x400000, 0x4cf18a25, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Twcup98, Twcup98, Stvbios)
STD_ROM_FN(Twcup98)

static const StvRomLoad Twcup98Loads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
};

// Tecmo World Soccer '98 (JUET 980410 V1.000)

static struct BurnRomInfo Twsoc98RomDesc[] = {
	{ "epr-20820.ic24", 0x100000, 0xb10451f9, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr20821.12", 0x400000, 0x2d930d23, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20822.13", 0x400000, 0x8b33a5e2, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20823.14", 0x400000, 0x6e6d4e95, 1 | BRF_PRG | BRF_ESS },
	{ "mpr20824.15", 0x400000, 0x4cf18a25, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Twsoc98, Twsoc98, Stvbios)
STD_ROM_FN(Twsoc98)

static const StvRomLoad Twsoc98Loads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
};

// Magical Zunou Power (J 961031 V1.000)

static struct BurnRomInfo MagzunRomDesc[] = {
	{ "flash.ic13", 0x100000, 0xe6f0aca0, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr-19354.ic2", 0x400000, 0xa23822e7, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-19355.ic3", 0x400000, 0xd70e5ebc, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-19356.ic4", 0x400000, 0x3bc43fe9, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-19357.ic5", 0x400000, 0xaa749370, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-19358.ic6", 0x400000, 0x0969f1ec, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-19359.ic1", 0x400000, 0xb0d06f9c, 1 | BRF_PRG | BRF_ESS },

	{ "magzun.nv", 0x000080, 0x42700321, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Magzun, Magzun, Stvbios)
STD_ROM_FN(Magzun)

static const StvRomLoad MagzunLoads[] = {
	{  0, 0x0000001, 0x0100000, 2 },
	{  0, 0x0200000, 0x0100000, 0 },
	{  0, 0x0300000, 0x0100000, 0 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
};

// Yatterman Plus (J 981006 V1.000)

static struct BurnRomInfo YattrmnpRomDesc[] = {
	{ "epr-21122.ic13", 0x080000, 0x49f56e32, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr-21125.ic02", 0x400000, 0x40f5f119, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21130.ic03", 0x400000, 0x84cb4e9c, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21126.ic04", 0x400000, 0x36095649, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21131.ic05", 0x400000, 0xb1c860d8, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21127.ic06", 0x400000, 0xf55901d7, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21132.ic07", 0x400000, 0x809b1b0e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21128.ic08", 0x400000, 0xc2e2f72e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21133.ic09", 0x400000, 0x2dc21956, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21129.ic10", 0x400000, 0x997d67bd, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21124.ic11", 0x400000, 0x10f073ac, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21123.ic12", 0x400000, 0x491b9166, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Yattrmnp, Yattrmnp, Stvbios)
STD_ROM_FN(Yattrmnp)

static const StvRomLoad YattrmnpLoads[] = {
	{  0, 0x0000000, 0x0080000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0400000, 1 },
	{  9, 0x2400000, 0x0400000, 1 },
	{ 10, 0x2800000, 0x0400000, 1 },
	{ 11, 0x2c00000, 0x0400000, 1 },
};

// Choro Q Hyper Racing 5 (J 981230 V1.000)

static struct BurnRomInfo ChoroqhrRomDesc[] = {
	{ "ic22.bin", 0x200000, 0x22c58710, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic24.bin", 0x200000, 0x09b8a154, 1 | BRF_PRG | BRF_ESS },
	{ "ic26.bin", 0x200000, 0x136ca5e9, 1 | BRF_PRG | BRF_ESS },
	{ "ic28.bin", 0x200000, 0x3c949563, 1 | BRF_PRG | BRF_ESS },
	{ "ic30.bin", 0x200000, 0x7e93078d, 1 | BRF_PRG | BRF_ESS },
	{ "ic32.bin", 0x200000, 0x86cdcbd8, 1 | BRF_PRG | BRF_ESS },
	{ "ic34.bin", 0x200000, 0xbe2ed0a0, 1 | BRF_PRG | BRF_ESS },
	{ "ic36.bin", 0x200000, 0x9a4109e5, 1 | BRF_PRG | BRF_ESS },

	{ "choroqhr.nv", 0x000080, 0x6e89815f, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Choroqhr, Choroqhr, Stvbios)
STD_ROM_FN(Choroqhr)

static const StvRomLoad ChoroqhrLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
	{  5, 0x0c00000, 0x0200000, 1 },
	{  6, 0x0e00000, 0x0200000, 1 },
	{  7, 0x1000000, 0x0200000, 1 },
};

// Tsuribori Taikai (JAE 980605 V1.000)

static struct BurnRomInfo TsuriborRomDesc[] = {
	{ "lh28f016sut.ic22", 0x200000, 0xf18764a5, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "lh28f016sut.ic24", 0x200000, 0x47810ffc, 1 | BRF_PRG | BRF_ESS },
	{ "lh28f016sut.ic26", 0x200000, 0xd36a3b4c, 1 | BRF_PRG | BRF_ESS },
	{ "lh28f016sut.ic28", 0x200000, 0x341fb80e, 1 | BRF_PRG | BRF_ESS },
	{ "lh28f016sut.ic30", 0x200000, 0x03b9eacf, 1 | BRF_PRG | BRF_ESS },

	{ "tsuribor.nv", 0x000080, 0x47c7fa6e, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Tsuribor, Tsuribor, Stvbios)
STD_ROM_FN(Tsuribor)

static const StvRomLoad TsuriborLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
};

// Challenge Golf (J 990326 V1.000)

static struct BurnRomInfo ChalgolfRomDesc[] = {
	{ "ic22.bin", 0x200000, 0x1d887acc, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic24.bin", 0x200000, 0x6091ee88, 1 | BRF_PRG | BRF_ESS },
	{ "ic26.bin", 0x200000, 0xaff38d56, 1 | BRF_PRG | BRF_ESS },
	{ "ic28.bin", 0x200000, 0x571cc8da, 1 | BRF_PRG | BRF_ESS },
	{ "ic30.bin", 0x200000, 0xa67eabb0, 1 | BRF_PRG | BRF_ESS },

	{ "chalgolf.nv", 0x000080, 0x6e89815f, 2 | BRF_PRG | BRF_ESS }, // eeprom
};

STDROMPICKEXT(Chalgolf, Chalgolf, Stvbios)
STD_ROM_FN(Chalgolf)

static const StvRomLoad ChalgolfLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
};

// Fantasy Zone (medal game, REV.A) (J 990202 V1.000)

static struct BurnRomInfo FanzonemRomDesc[] = {
	{ "epr-21440a.ic13", 0x080000, 0x28457d58, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr-21441.ic2", 0x400000, 0xb69133a5, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21445.ic1", 0x400000, 0xc02ffbd3, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21442.ic4", 0x400000, 0xd4d3575f, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21446.ic3", 0x400000, 0x4831539e, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21443.ic6", 0x400000, 0xcb1401c9, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21447.ic5", 0x400000, 0x61e0d313, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-21444.ic8", 0x400000, 0xa82ff33b, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Fanzonem, Fanzonem, Stvbios)
STD_ROM_FN(Fanzonem)

static const StvRomLoad FanzonemLoads[] = {
	{  0, 0x0000000, 0x0080000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
};

// Soreyuke Anpanman Crayon Kids (J 001026 V1.000)

static struct BurnRomInfo SackidsRomDesc[] = {
	{ "ic22.bin", 0x200000, 0x4d9d1870, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "ic24.bin", 0x200000, 0x39fca3e5, 1 | BRF_PRG | BRF_ESS },
	{ "ic26.bin", 0x200000, 0xf38c79b6, 1 | BRF_PRG | BRF_ESS },
	{ "ic28.bin", 0x200000, 0x63d09f3c, 1 | BRF_PRG | BRF_ESS },
	{ "ic30.bin", 0x200000, 0xf89811ba, 1 | BRF_PRG | BRF_ESS },
	{ "ic32.bin", 0x200000, 0x1db6c26b, 1 | BRF_PRG | BRF_ESS },
	{ "ic34.bin", 0x200000, 0x0f3622c8, 1 | BRF_PRG | BRF_ESS },
	{ "ic36.bin", 0x200000, 0x9a4109e5, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Sackids, Sackids, Stvbios)
STD_ROM_FN(Sackids)

static const StvRomLoad SackidsLoads[] = {
	{  0, 0x0200000, 0x0200000, 1 },
	{  1, 0x0400000, 0x0200000, 1 },
	{  2, 0x0600000, 0x0200000, 1 },
	{  3, 0x0800000, 0x0200000, 1 },
	{  4, 0x0a00000, 0x0200000, 1 },
	{  5, 0x0c00000, 0x0200000, 1 },
	{  6, 0x0e00000, 0x0200000, 1 },
	{  7, 0x1000000, 0x0200000, 1 },
};

// Dancing Fever Gold (J 000821 V2.001)

static struct BurnRomInfo DfevergRomDesc[] = {
	{ "13", 0x080000, 0xecd7ac4b, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "1", 0x400000, 0xd72f1640, 1 | BRF_PRG | BRF_ESS },
	{ "2", 0x400000, 0xc2e8aee5, 1 | BRF_PRG | BRF_ESS },
	{ "3", 0x400000, 0xcb5b2744, 1 | BRF_PRG | BRF_ESS },
	{ "4", 0x400000, 0x7eca59b2, 1 | BRF_PRG | BRF_ESS },
	{ "5", 0x400000, 0xc3450f2b, 1 | BRF_PRG | BRF_ESS },
	{ "6", 0x400000, 0x1ac57ed5, 1 | BRF_PRG | BRF_ESS },
	{ "8", 0x400000, 0xacc78f10, 1 | BRF_PRG | BRF_ESS },
	{ "9", 0x400000, 0x4ffbba8d, 1 | BRF_PRG | BRF_ESS },
	{ "10", 0x400000, 0x4b2a4397, 1 | BRF_PRG | BRF_ESS },
	{ "11", 0x400000, 0x58877b19, 1 | BRF_PRG | BRF_ESS },
	{ "12", 0x400000, 0x00000000, BRF_OPT | BRF_NODUMP },
};

STDROMPICKEXT(Dfeverg, Dfeverg, Stvbios)
STD_ROM_FN(Dfeverg)

static const StvRomLoad DfevergLoads[] = {
	{  0, 0x0000000, 0x0080000, 1 },
	{  1, 0x0400000, 0x0400000, 1 },
	{  2, 0x0800000, 0x0400000, 1 },
	{  3, 0x0c00000, 0x0400000, 1 },
	{  4, 0x1000000, 0x0400000, 1 },
	{  5, 0x1400000, 0x0400000, 1 },
	{  6, 0x1800000, 0x0400000, 1 },
	{  7, 0x1c00000, 0x0400000, 1 },
	{  8, 0x2000000, 0x0400000, 1 },
	{  9, 0x2400000, 0x0400000, 1 },
	{ 10, 0x2800000, 0x0400000, 1 },
	// IC12 is undumped. Leave its cartridge range zero-filled.
};

// Sport Fishing 2 (UET 951106 V1.10e)

static struct BurnRomInfo Sfish2RomDesc[] = {
	{ "epr18343.bin", 0x080000, 0x48e2eecf, BRF_PRG | BRF_ESS }, // bios

	{ "epr-18427.ic13", 0x100000, 0x3f25bec8, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr-18273.ic2", 0x400000, 0x6fec0193, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-18274.ic3", 0x400000, 0xa6d76d23, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-18275.ic4", 0x200000, 0x7691deca, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Sfish2, Sfish2, Stvbios)
STD_ROM_FN(Sfish2)

static const StvRomLoad Sfish2Loads[] = {
	{  1, 0x0000001, 0x0100000, 2 },
	{  1, 0x0200000, 0x0100000, 0 },
	{  1, 0x0300000, 0x0100000, 0 },
	{  2, 0x0400000, 0x0400000, 1 },
	{  3, 0x0800000, 0x0400000, 1 },
	{  4, 0x0c00000, 0x0200000, 1 },
};

// Sport Fishing 2 (J 951201 V1.100)

static struct BurnRomInfo Sfish2jRomDesc[] = {
	{ "epr18343.bin", 0x080000, 0x48e2eecf, BRF_PRG | BRF_ESS }, // bios

	{ "epr18344.a", 0x100000, 0x5a7de018, 1 | BRF_PRG | BRF_ESS }, // cart
	{ "mpr-18273.ic2", 0x400000, 0x6fec0193, 1 | BRF_PRG | BRF_ESS },
	{ "mpr-18274.ic3", 0x400000, 0xa6d76d23, 1 | BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(Sfish2j, Sfish2j, Stvbios)
STD_ROM_FN(Sfish2j)

static const StvRomLoad Sfish2jLoads[] = {
	{  1, 0x0000001, 0x0100000, 2 },
	{  1, 0x0200000, 0x0100000, 0 },
	{  1, 0x0300000, 0x0100000, 0 },
	{  2, 0x0400000, 0x0400000, 1 },
	{  3, 0x0800000, 0x0400000, 1 },
};

static const StvGame StvGames[] = {
	{ "astrass", AstrassLoads, sizeof(AstrassLoads) / sizeof(AstrassLoads[0]), 128, -1, 1 },
	{ "bakubaku", BakubakuLoads, sizeof(BakubakuLoads) / sizeof(BakubakuLoads[0]), 128, -1, 1 },
	{ "batmanfr", BatmanfrLoads, sizeof(BatmanfrLoads) / sizeof(BatmanfrLoads[0]), 128, -1, 1 },
	{ "colmns97", Colmns97Loads, sizeof(Colmns97Loads) / sizeof(Colmns97Loads[0]), 128, -1, 1 },
	{ "cotton2", Cotton2Loads, sizeof(Cotton2Loads) / sizeof(Cotton2Loads[0]), 128, -1, 1 },
	{ "cottonbm", CottonbmLoads, sizeof(CottonbmLoads) / sizeof(CottonbmLoads[0]), 128, -1, 1 },
	{ "critcrsh", CritcrshLoads, sizeof(CritcrshLoads) / sizeof(CritcrshLoads[0]), 131, 3, 12 },
	{ "tatacot", TatacotLoads, sizeof(TatacotLoads) / sizeof(TatacotLoads[0]), 128, 3, 1 },
	{ "danchih", DanchihLoads, sizeof(DanchihLoads) / sizeof(DanchihLoads[0]), 128, -1, 1 },
	{ "danchiq", DanchiqLoads, sizeof(DanchiqLoads) / sizeof(DanchiqLoads[0]), 128, -1, 1 },
	{ "diehard", DiehardLoads, sizeof(DiehardLoads) / sizeof(DiehardLoads[0]), 129, -1, 4 },
	{ "dnmtdeka", DnmtdekaLoads, sizeof(DnmtdekaLoads) / sizeof(DnmtdekaLoads[0]), 128, -1, 1 },
	{ "ejihon", EjihonLoads, sizeof(EjihonLoads) / sizeof(EjihonLoads[0]), 128, -1, 1 },
	{ "elandore", ElandoreLoads, sizeof(ElandoreLoads) / sizeof(ElandoreLoads[0]), 128, -1, 1 },
	{ "ffrevng10", Ffrevng10Loads, sizeof(Ffrevng10Loads) / sizeof(Ffrevng10Loads[0]), 128, -1, 1 },
	{ "ffreveng", FfrevengLoads, sizeof(FfrevengLoads) / sizeof(FfrevengLoads[0]), 128, -1, 1 },
	{ "fhboxers", FhboxersLoads, sizeof(FhboxersLoads) / sizeof(FhboxersLoads[0]), 128, 10, 1 },
	{ "findlove", FindloveLoads, sizeof(FindloveLoads) / sizeof(FindloveLoads[0]), 128, 13, 1 },
	{ "gaxeduel", GaxeduelLoads, sizeof(GaxeduelLoads) / sizeof(GaxeduelLoads[0]), 128, -1, 1 },
	{ "grdforce", GrdforceLoads, sizeof(GrdforceLoads) / sizeof(GrdforceLoads[0]), 128, -1, 1 },
	{ "groovef", GroovefLoads, sizeof(GroovefLoads) / sizeof(GroovefLoads[0]), 128, -1, 1 },
	{ "hanagumi", HanagumiLoads, sizeof(HanagumiLoads) / sizeof(HanagumiLoads[0]), 128, -1, 1 },
	{ "introdon", IntrodonLoads, sizeof(IntrodonLoads) / sizeof(IntrodonLoads[0]), 128, -1, 1 },
	{ "kiwames", KiwamesLoads, sizeof(KiwamesLoads) / sizeof(KiwamesLoads[0]), 128, 4, 1 },
	{ "maruchan", MaruchanLoads, sizeof(MaruchanLoads) / sizeof(MaruchanLoads[0]), 128, -1, 1 },
	{ "mausuke", MausukeLoads, sizeof(MausukeLoads) / sizeof(MausukeLoads[0]), 128, -1, 1 },
	{ "micrombc", MicrombcLoads, sizeof(MicrombcLoads) / sizeof(MicrombcLoads[0]), 128, 8, 1 },
	{ "myfairld", MyfairldLoads, sizeof(MyfairldLoads) / sizeof(MyfairldLoads[0]), 128, 8, 1 },
	{ "othellos", OthellosLoads, sizeof(OthellosLoads) / sizeof(OthellosLoads[0]), 128, -1, 1 },
	{ "patocar", PatocarLoads, sizeof(PatocarLoads) / sizeof(PatocarLoads[0]), 128, 5, 1 },
	{ "pblbeach", PblbeachLoads, sizeof(PblbeachLoads) / sizeof(PblbeachLoads[0]), 128, -1, 1 },
	{ "prikura", PrikuraLoads, sizeof(PrikuraLoads) / sizeof(PrikuraLoads[0]), 128, -1, 1 },
	{ "puyosun", PuyosunLoads, sizeof(PuyosunLoads) / sizeof(PuyosunLoads[0]), 128, -1, 1 },
	{ "rsgun", RsgunLoads, sizeof(RsgunLoads) / sizeof(RsgunLoads[0]), 128, -1, 1 },
	{ "sasissu", SasissuLoads, sizeof(SasissuLoads) / sizeof(SasissuLoads[0]), 128, -1, 1 },
	{ "sanjeon", SanjeonLoads, sizeof(SanjeonLoads) / sizeof(SanjeonLoads[0]), 128, -1, 1 },
	{ "seabass", SeabassLoads, sizeof(SeabassLoads) / sizeof(SeabassLoads[0]), 128, 9, 1 },
	{ "shanhigw", ShanhigwLoads, sizeof(ShanhigwLoads) / sizeof(ShanhigwLoads[0]), 128, -1, 1 },
	{ "shienryu", ShienryuLoads, sizeof(ShienryuLoads) / sizeof(ShienryuLoads[0]), 128, 3, 1 },
	{ "sss", SssLoads, sizeof(SssLoads) / sizeof(SssLoads[0]), 128, 6, 1 },
	{ "sandor", SandorLoads, sizeof(SandorLoads) / sizeof(SandorLoads[0]), 128, -1, 1 },
	{ "thunt", ThuntLoads, sizeof(ThuntLoads) / sizeof(ThuntLoads[0]), 128, -1, 1 },
	{ "thuntk", ThuntkLoads, sizeof(ThuntkLoads) / sizeof(ThuntkLoads[0]), 128, -1, 1 },
	{ "skychal", SkychalLoads, sizeof(SkychalLoads) / sizeof(SkychalLoads[0]), 128, 6, 1 },
	{ "smleague", SmleagueLoads, sizeof(SmleagueLoads) / sizeof(SmleagueLoads[0]), 129, -1, 4 },
	{ "finlarch", FinlarchLoads, sizeof(FinlarchLoads) / sizeof(FinlarchLoads[0]), 128, -1, 1 },
	{ "sokyugrt", SokyugrtLoads, sizeof(SokyugrtLoads) / sizeof(SokyugrtLoads[0]), 128, -1, 1 },
	{ "suikoenb", SuikoenbLoads, sizeof(SuikoenbLoads) / sizeof(SuikoenbLoads[0]), 128, -1, 1 },
	{ "supgoal", SupgoalLoads, sizeof(SupgoalLoads) / sizeof(SupgoalLoads[0]), 128, 5, 1 },
	{ "techbowl", TechbowlLoads, sizeof(TechbowlLoads) / sizeof(TechbowlLoads[0]), 128, 5, 1 },
	{ "vfkids", VfkidsLoads, sizeof(VfkidsLoads) / sizeof(VfkidsLoads[0]), 128, -1, 1 },
	{ "vmahjong", VmahjongLoads, sizeof(VmahjongLoads) / sizeof(VmahjongLoads[0]), 128, 8, 1 },
	{ "wwshin", WwshinLoads, sizeof(WwshinLoads) / sizeof(WwshinLoads[0]), 128, -1, 1 },
	{ "wasafari", WasafariLoads, sizeof(WasafariLoads) / sizeof(WasafariLoads[0]), 128, 5, 1 },
	{ "winterht", WinterhtLoads, sizeof(WinterhtLoads) / sizeof(WinterhtLoads[0]), 128, -1, 1 },
	{ "znpwfv", ZnpwfvLoads, sizeof(ZnpwfvLoads) / sizeof(ZnpwfvLoads[0]), 128, -1, 1 },
	{ "znpwfvt", ZnpwfvtLoads, sizeof(ZnpwfvtLoads) / sizeof(ZnpwfvtLoads[0]), 130, -1, 2 },
	{ "stress", StressLoads, sizeof(StressLoads) / sizeof(StressLoads[0]), 128, -1, 1 },
	{ "vfremix", VfremixLoads, sizeof(VfremixLoads) / sizeof(VfremixLoads[0]), 128, -1, 1 },
	{ "decathlt", DecathltLoads, sizeof(DecathltLoads) / sizeof(DecathltLoads[0]), 128, -1, 1 },
	{ "decathlto", DecathltoLoads, sizeof(DecathltoLoads) / sizeof(DecathltoLoads[0]), 128, -1, 1 },
	{ "twcup98", Twcup98Loads, sizeof(Twcup98Loads) / sizeof(Twcup98Loads[0]), 128, -1, 1 },
	{ "twsoc98", Twsoc98Loads, sizeof(Twsoc98Loads) / sizeof(Twsoc98Loads[0]), 128, -1, 1 },
	{ "magzun", MagzunLoads, sizeof(MagzunLoads) / sizeof(MagzunLoads[0]), 128, 7, 1 },
	{ "yattrmnp", YattrmnpLoads, sizeof(YattrmnpLoads) / sizeof(YattrmnpLoads[0]), 128, -1, 1 },
	{ "choroqhr", ChoroqhrLoads, sizeof(ChoroqhrLoads) / sizeof(ChoroqhrLoads[0]), 128, 8, 1 },
	{ "tsuribor", TsuriborLoads, sizeof(TsuriborLoads) / sizeof(TsuriborLoads[0]), 128, 5, 1 },
	{ "chalgolf", ChalgolfLoads, sizeof(ChalgolfLoads) / sizeof(ChalgolfLoads[0]), 128, 5, 1 },
	{ "fanzonem", FanzonemLoads, sizeof(FanzonemLoads) / sizeof(FanzonemLoads[0]), 128, -1, 1 },
	{ "sackids", SackidsLoads, sizeof(SackidsLoads) / sizeof(SackidsLoads[0]), 128, -1, 1 },
	{ "dfeverg", DfevergLoads, sizeof(DfevergLoads) / sizeof(DfevergLoads[0]), 128, -1, 1 },
	{ "sfish2", Sfish2Loads, sizeof(Sfish2Loads) / sizeof(Sfish2Loads[0]), 0, -1, 4 },
	{ "sfish2j", Sfish2jLoads, sizeof(Sfish2jLoads) / sizeof(Sfish2jLoads[0]), 0, -1, 1 },
};

static const StvGame *FindGame()
{
	for (UINT32 i = 0; i < sizeof(StvGames) / sizeof(StvGames[0]); i++) {
		if (!strcmp(BurnDrvGetTextA(DRV_NAME), StvGames[i].name)) return &StvGames[i];
	}
	return NULL;
}

struct BurnDriver BurnDrvAstrass = {
	"astrass", NULL, "stvbios", NULL, "1998",
	"Astra SuperStars (J 980514 V1.002)\0", "Protection implemented, not yet verified", "Sunsoft", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, AstrassRomInfo, AstrassRomName, NULL, NULL, NULL, NULL, Stv6bInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvBakubaku = {
	"bakubaku", NULL, "stvbios", NULL, "1995",
	"Baku Baku Animal (J 950407 V1.000)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, BakubakuRomInfo, BakubakuRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvBatmanfr = {
	"batmanfr", NULL, "stvbios", NULL, "1996",
	"Batman Forever (JUE 960507 V1.000)\0", NULL, "Acclaim", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, BatmanfrRomInfo, BatmanfrRomName, NULL, NULL, NULL, NULL, BatmanfrInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvColmns97 = {
	"colmns97", NULL, "stvbios", NULL, "1996",
	"Columns '97 (JET 961209 V1.000)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, Colmns97RomInfo, Colmns97RomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvCotton2 = {
	"cotton2", NULL, "stvbios", NULL, "1997",
	"Cotton 2 (JUET 970902 V1.000)\0", "Imperfect graphics and sound", "Success", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_HORSHOOT, 0,
	NULL, Cotton2RomInfo, Cotton2RomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvCottonbm = {
	"cottonbm", NULL, "stvbios", NULL, "1998",
	"Cotton Boomerang (JUET 980709 V1.000)\0", "Not yet verified", "Success", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_HORSHOOT, 0,
	NULL, CottonbmRomInfo, CottonbmRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvCritcrsh = {
	"critcrsh", NULL, "stvbios", NULL, "1995",
	"Critter Crusher (EA 951204 V1.000)\0", "Cabinet score display and ticket dispenser not emulated", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, CritcrshRomInfo, CritcrshRomName, NULL, NULL, NULL, NULL, CritcrshInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvTatacot = {
	"tatacot", "critcrsh", "stvbios", NULL, "1995",
	"Tatacot (JA 951128 V1.000)\0", "Cabinet score display and ticket dispenser not emulated", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, TatacotRomInfo, TatacotRomName, NULL, NULL, NULL, NULL, CritcrshInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvDanchih = {
	"danchih", NULL, "stvbios", NULL, "1999",
	"Danchi de Hanafuda (J 990607 V1.400)\0", "Not yet verified", "Altron (Tecmo license)", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, DanchihRomInfo, DanchihRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvDanchiq = {
	"danchiq", NULL, "stvbios", NULL, "2000",
	"Danchi de Quiz: Okusan Yontaku Desuyo! (J 001128 V1.200)\0", "Not yet verified", "Altron", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, DanchiqRomInfo, DanchiqRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvDiehard = {
	"diehard", NULL, "stvbios", NULL, "1996",
	"Die Hard Arcade (UET 960515 V1.000)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, DiehardRomInfo, DiehardRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvDnmtdeka = {
	"dnmtdeka", "diehard", "stvbios", NULL, "1996",
	"Dynamite Deka (J 960515 V1.000)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, DnmtdekaRomInfo, DnmtdekaRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvEjihon = {
	"ejihon", NULL, "stvbios", NULL, "1995",
	"Ejihon Tantei Jimusho (J 950613 V1.000)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, EjihonRomInfo, EjihonRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvElandore = {
	"elandore", NULL, "stvbios", NULL, "1998",
	"Touryuu Densetsu Elan-Doree / Elan Doree - Legend of Dragoon (JUET 980922 V1.006)\0", NULL, "Sai-Mate", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, ElandoreRomInfo, ElandoreRomName, NULL, NULL, NULL, NULL, Stv6bInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvFfrevng10 = {
	"ffrevng10", "ffreveng", "stvbios", NULL, "1999",
	"Final Fight Revenge / Final Revenge (JUET 990714 V1.000)\0", NULL, "Capcom", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, Ffrevng10RomInfo, Ffrevng10RomName, NULL, NULL, NULL, NULL, Stv6bInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvFfreveng = {
	"ffreveng", NULL, "stvbios", NULL, "1999",
	"Final Fight Revenge / Final Revenge (JUET 990930 V1.100)\0", NULL, "Capcom", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, FfrevengRomInfo, FfrevengRomName, NULL, NULL, NULL, NULL, Stv6bInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvFhboxers = {
	"fhboxers", NULL, "stvbios", NULL, "1995",
	"Funky Head Boxers (JUETBKAL 951218 V1.000)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, FhboxersRomInfo, FhboxersRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvFindlove = {
	"findlove", NULL, "stvbios", NULL, "1997",
	"Zenkoku Seifuku Bishoujo Grand Prix Find Love (J 971212 V1.000)\0", NULL, "Daiki / FCF", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, FindloveRomInfo, FindloveRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvGaxeduel = {
	"gaxeduel", NULL, "stvbios", NULL, "1994",
	"Golden Axe - The Duel (JUETL 950117 V1.000)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, GaxeduelRomInfo, GaxeduelRomName, NULL, NULL, NULL, NULL, Stv6bInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvGrdforce = {
	"grdforce", NULL, "stvbios", NULL, "1998",
	"Guardian Force (JUET 980318 V0.105)\0", NULL, "Success", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, GrdforceRomInfo, GrdforceRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvGroovef = {
	"groovef", NULL, "stvbios", NULL, "1997",
	"Groove on Fight - Gouketsuji Ichizoku 3 (J 970416 V1.001)\0", NULL, "Atlus", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, GroovefRomInfo, GroovefRomName, NULL, NULL, NULL, NULL, Stv6bInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvHanagumi = {
	"hanagumi", NULL, "stvbios", NULL, "1997",
	"Sakura Taisen - Hanagumi Taisen Columns (J 971007 V1.010)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, HanagumiRomInfo, HanagumiRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvIntrodon = {
	"introdon", NULL, "stvbios", NULL, "1996",
	"Karaoke Quiz Intro Don Don! (J 960213 V1.000)\0", NULL, "Sunsoft / Success", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, IntrodonRomInfo, IntrodonRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvKiwames = {
	"kiwames", NULL, "stvbios", NULL, "1995",
	"Pro Mahjong Kiwame S (J 951020 V1.208)\0", NULL, "Athena", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, KiwamesRomInfo, KiwamesRomName, NULL, NULL, NULL, NULL, StvMahjongInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvMaruchan = {
	"maruchan", NULL, "stvbios", NULL, "1997",
	"Maru-Chan de Goo! (J 971216 V1.000)\0", NULL, "Sega / Toyosuisan", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, MaruchanRomInfo, MaruchanRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvMausuke = {
	"mausuke", NULL, "stvbios", NULL, "1996",
	"Mausuke no Ojama the World (J 960314 V1.000)\0", NULL, "Data East Corporation", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, MausukeRomInfo, MausukeRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvMicrombc = {
	"micrombc", NULL, "stvbios", NULL, "1999",
	"Microman Battle Charge (J 990326 V1.000)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, MicrombcRomInfo, MicrombcRomName, NULL, NULL, NULL, NULL, MicrombcInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvMyfairld = {
	"myfairld", NULL, "stvbios", NULL, "1998",
	"Virtual Mahjong 2 - My Fair Lady (J 980608 V1.000)\0", NULL, "Micronet", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, MyfairldRomInfo, MyfairldRomName, NULL, NULL, NULL, NULL, MyfairldInputInfo, MyfairldDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvOthellos = {
	"othellos", NULL, "stvbios", NULL, "1998",
	"Othello Shiyouyo (J 980423 V1.002)\0", NULL, "Success", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, OthellosRomInfo, OthellosRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvPatocar = {
	"patocar", NULL, "stvbios", NULL, "2001",
	"Hashire Patrol Car (J 990326 V1.000)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, PatocarRomInfo, PatocarRomName, NULL, NULL, NULL, NULL, HopperTrackballInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvPblbeach = {
	"pblbeach", NULL, "stvbios", NULL, "1995",
	"Pebble Beach - The Great Shot (JUE 950913 V0.990)\0", NULL, "T&E Soft", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, PblbeachRomInfo, PblbeachRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvPrikura = {
	"prikura", NULL, "stvbios", NULL, "1996",
	"Princess Clara Daisakusen (J 960910 V1.000)\0", NULL, "Atlus", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, PrikuraRomInfo, PrikuraRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvPuyosun = {
	"puyosun", NULL, "stvbios", NULL, "1996",
	"Puyo Puyo Sun (J 961115 V0.001)\0", NULL, "Compile", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, PuyosunRomInfo, PuyosunRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvRsgun = {
	"rsgun", NULL, "stvbios", NULL, "1998",
	"Radiant Silvergun (JUET 980523 V1.000)\0", NULL, "Treasure", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, RsgunRomInfo, RsgunRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSasissu = {
	"sasissu", NULL, "stvbios", NULL, "1998",
	"Taisen Tanto-R Sashissu!! (J 980216 V1.000)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, SasissuRomInfo, SasissuRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSanjeon = {
	"sanjeon", "sasissu", "stvbios", NULL, "1999",
	"DaeJeon! SanJeon SuJeon (AJTUE 990412 V1.000)\0", NULL, "Sega / Deniam", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, SanjeonRomInfo, SanjeonRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSeabass = {
	"seabass", NULL, "stvbios", NULL, "1997",
	"Sea Bass Fishing (JUET 971110 V0.001)\0", NULL, "A wave inc. (Able license)", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, SeabassRomInfo, SeabassRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvShanhigw = {
	"shanhigw", NULL, "stvbios", NULL, "1995",
	"Shanghai - The Great Wall / Shanghai Triple Threat (JUE 950623 V1.005)\0", NULL, "Sunsoft / Activision", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, ShanhigwRomInfo, ShanhigwRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvShienryu = {
	"shienryu", NULL, "stvbios", NULL, "1997",
	"Shienryu (JUET 961226 V1.000)\0", NULL, "Warashi", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, ShienryuRomInfo, ShienryuRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	224, 320, 3, 4
};

struct BurnDriver BurnDrvSss = {
	"sss", NULL, "stvbios", NULL, "1998",
	"Steep Slope Sliders (JUET 981110 V1.000)\0", NULL, "Capcom / Cave / Victor Interactive Software", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, SssRomInfo, SssRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSandor = {
	"sandor", NULL, "stvbios", NULL, "1995",
	"Puzzle & Action: Sando-R (J 951114 V1.000)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, SandorRomInfo, SandorRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvThunt = {
	"thunt", "sandor", "stvbios", NULL, "1997",
	"Puzzle & Action: Treasure Hunt (JUET 970901 V2.00E)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, ThuntRomInfo, ThuntRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvThuntk = {
	"thuntk", "sandor", "stvbios", NULL, "1997",
	"Puzzle & Action: BoMulEul Chajara (JUET 970125 V2.00K)\0", NULL, "Sega / Deniam", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_PUZZLE, 0,
	NULL, ThuntkRomInfo, ThuntkRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSkychal = {
	"skychal", NULL, "stvbios", NULL, "2000",
	"Sky Challenger (J 000406 V1.000)\0", "Hopper and trackball emulated", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, SkychalRomInfo, SkychalRomName, NULL, NULL, NULL, NULL, HopperTrackballInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSmleague = {
	"smleague", NULL, "stvbios", NULL, "1995",
	"Super Major League (U 960108 V1.000)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, SmleagueRomInfo, SmleagueRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvFinlarch = {
	"finlarch", "smleague", "stvbios", NULL, "1995",
	"Final Arch (J 950714 V1.001)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, FinlarchRomInfo, FinlarchRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSokyugrt = {
	"sokyugrt", NULL, "stvbios", NULL, "1996",
	"Soukyugurentai / Terra Diver (JUET 960821 V1.000)\0", NULL, "Raizing / Eighting", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, SokyugrtRomInfo, SokyugrtRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSuikoenb = {
	"suikoenb", NULL, "stvbios", NULL, "1995",
	"Suiko Enbu / Outlaws of the Lost Dynasty (JUETL 950314 V2.001)\0", NULL, "Data East Corporation", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, SuikoenbRomInfo, SuikoenbRomName, NULL, NULL, NULL, NULL, Stv6bInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSupgoal = {
	"supgoal", NULL, "stvbios", NULL, "1998",
	"Nerae! Super Goal (J 981218 V1.000)\0", "Hopper and trackball emulated", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, SupgoalRomInfo, SupgoalRomName, NULL, NULL, NULL, NULL, HopperTrackballInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvTechbowl = {
	"techbowl", NULL, "stvbios", NULL, "1997",
	"Technical Bowling (J 971212 V1.000)\0", "Hopper and trackball emulated", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, TechbowlRomInfo, TechbowlRomName, NULL, NULL, NULL, NULL, HopperTrackballInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvVfkids = {
	"vfkids", NULL, "stvbios", NULL, "1996",
	"Virtua Fighter Kids (JUET 960319 V0.000)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, VfkidsRomInfo, VfkidsRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvVmahjong = {
	"vmahjong", NULL, "stvbios", NULL, "1997",
	"Virtual Mahjong (J 961214 V1.000)\0", NULL, "Micronet", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, VmahjongRomInfo, VmahjongRomName, NULL, NULL, NULL, NULL, VmahjongInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvWwshin = {
	"wwshin", NULL, "stvbios", NULL, "1997",
	"Waku Waku Shinkansen (J 971031 V0.002)\0", "Cabinet motion not emulated", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, WwshinRomInfo, WwshinRomName, NULL, NULL, NULL, NULL, WwshinInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvWasafari = {
	"wasafari", NULL, "stvbios", NULL, "1998",
	"Wanpaku Safari (J 981109 V1.000)\0", "Does not progress past the mud section", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, WasafariRomInfo, WasafariRomName, NULL, NULL, NULL, NULL, WasafariInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvWinterht = {
	"winterht", NULL, "stvbios", NULL, "1997",
	"Winter Heat (JUET 971012 V1.000)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, WinterhtRomInfo, WinterhtRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvZnpwfv = {
	"znpwfv", NULL, "stvbios", NULL, "1997",
	"Zen Nippon Pro-Wres Featuring Virtua (J 971123 V1.000)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, ZnpwfvRomInfo, ZnpwfvRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvZnpwfvt = {
	"znpwfvt", "znpwfv", "stvbios", NULL, "1997",
	"All Japan Pro-Wrestling Featuring Virtua (T 971123 V1.000)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, ZnpwfvtRomInfo, ZnpwfvtRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvStress = {
	"stress", NULL, "stvbios", NULL, "1998",
	"Stress Busters (J 981020 V1.000)\0", "Camera / printer hardware not emulated", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, StressRomInfo, StressRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvVfremix = {
	"vfremix", NULL, "stvbios", NULL, "1995",
	"Virtua Fighter Remix (JUETBKAL 950428 V1.000)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, VfremixRomInfo, VfremixRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvDecathlt = {
	"decathlt", NULL, "stvbios", NULL, "1996",
	"Decathlete (JUET 960709 V1.001)\0", "Incomplete stv_5838 hardware", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, DecathltRomInfo, DecathltRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvDecathlto = {
	"decathlto", "decathlt", "stvbios", NULL, "1996",
	"Decathlete (JUET 960424 V1.000)\0", "Incomplete stv_5838 hardware", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, DecathltoRomInfo, DecathltoRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvTwcup98 = {
	"twcup98", NULL, "stvbios", NULL, "1998",
	"Tecmo World Cup '98 (JUET 980410 V1.000)\0", NULL, "Tecmo", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, Twcup98RomInfo, Twcup98RomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvTwsoc98 = {
	"twsoc98", "twcup98", "stvbios", NULL, "1998",
	"Tecmo World Soccer '98 (JUET 980410 V1.000)\0", "Protection implemented, not yet verified", "Tecmo", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, Twsoc98RomInfo, Twsoc98RomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvMagzun = {
	"magzun", NULL, "stvbios", NULL, "1996",
	"Magical Zunou Power (J 961031 V1.000)\0", "Voice board not emulated", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, MagzunRomInfo, MagzunRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvYattrmnp = {
	"yattrmnp", NULL, "stvbios", NULL, "1998",
	"Yatterman Plus (J 981006 V1.000)\0", "Stops at ROM HAS CHANGED / PUSH RESET", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, YattrmnpRomInfo, YattrmnpRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvChoroqhr = {
	"choroqhr", NULL, "stvbios", NULL, "1998",
	"Choro Q Hyper Racing 5 (J 981230 V1.000)\0", NULL, "Sega / Takara", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, ChoroqhrRomInfo, ChoroqhrRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvTsuribor = {
	"tsuribor", NULL, "stvbios", NULL, "1998",
	"Tsuribori Taikai (JAE 980605 V1.000)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, TsuriborRomInfo, TsuriborRomName, NULL, NULL, NULL, NULL, TsuriborInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvChalgolf = {
	"chalgolf", NULL, "stvbios", NULL, "1999",
	"Challenge Golf (J 990326 V1.000)\0", NULL, "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, ChalgolfRomInfo, ChalgolfRomName, NULL, NULL, NULL, NULL, HopperTrackballInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvFanzonem = {
	"fanzonem", NULL, "stvbios", NULL, "1999",
	"Fantasy Zone (medal game, REV.A) (J 990202 V1.000)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, FanzonemRomInfo, FanzonemRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSackids = {
	"sackids", NULL, "stvbios", NULL, "2000",
	"Soreyuke Anpanman Crayon Kids (J 001026 V1.000)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, SackidsRomInfo, SackidsRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvDfeverg = {
	"dfeverg", NULL, "stvbios", NULL, "2001",
	"Dancing Fever Gold (J 000821 V2.001)\0", "Not yet verified", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, DfevergRomInfo, DfevergRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSfish2 = {
	"sfish2", NULL, "stvbios", NULL, "1995",
	"Sport Fishing 2 (UET 951106 V1.10e)\0", "Incomplete stvcd hardware", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, Sfish2RomInfo, Sfish2RomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};

struct BurnDriver BurnDrvSfish2j = {
	"sfish2j", "sfish2", "stvbios", NULL, "1995",
	"Sport Fishing 2 (J 951201 V1.100)\0", "Incomplete stvcd hardware", "Sega", "ST-V",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, Sfish2jRomInfo, Sfish2jRomName, NULL, NULL, NULL, NULL, StvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 224, 4, 3
};
