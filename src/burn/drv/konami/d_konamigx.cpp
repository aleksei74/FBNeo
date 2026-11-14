// FB Neo Konami System GX (Type 2) driver
// Based on the MAME driver by R. Belmont, Acho A. Tang, Phil Stroffolino
// and Olivier Galibert.
// Generated with Codex AI (by DsNo) & Claude AI (by aleksei74)
//
// The two K054539 chips are mixed directly and through the TMS57002 DASP,
// matching the GX audio routing used by MAME.

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "konamiic.h"
#include "k054539.h"
#include "gx_dasp_clock.h"
#include "gx_palette.h"
#include "gx_monitor_copy.h"

#include "gx_esc_chip.h"
#include "../../../cpu/tms57002/tms57002.h"
#include "dtimer.h"
#include "eeprom.h"
#include "burn_gun.h"

static INT32 gx_sample_cycles = -1;
static bool gx_sample_irq_refresh = false;

// Only the main/caller thread waits here, before touching sound CPU context
// or shared host command/status registers. Never call from the sound worker.

static UINT8 *AllMem;
static UINT8 *DrvMainROM;
static UINT8 *DrvSoundROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvPsacMapROM;
static UINT16 *DrvType3RozBitmap;
static INT32 gx_psac_cached_bank = -1;
static UINT8 *DrvGfxROMExp0;
static UINT8 *DrvGfxROMExp1;
static UINT8 *DrvSndROM;
static UINT8 *DrvEEPROM;
static UINT8 *AllRam;
static UINT8 *DrvMainRAM;
static UINT8 *DrvSoundRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSubPalRAM;
static UINT8 *DrvTileRAM;
static UINT8 *DrvType4RAM;
static UINT8 *DrvType4CtrlRAM;
static UINT8 *DrvType4LineRAM;
static UINT8 *DrvType4PsacRAM;
static UINT8 *DrvSoundCom;
static UINT8 *DrvCCURegs;
static UINT8 *DrvTmsRAM;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static UINT32 *DrvPalette;
static UINT32 gx_type4_palette_cache[2][0x2000];
static UINT8 gx_type4_palette_dirty[2][0x2000];
static UINT16 gx_type4_palette_dirty_list[2][0x2000];
static INT32 gx_type4_palette_dirty_count[2];
static UINT16 *pGxPsacBitmap;
static UINT32 *pGxType4LeftBitmap;
static UINT32 *pGxType4RightBitmap;
static UINT16 gx_type4_ctrl_words[0x10];
static UINT16 gx_type4_line_words[0x800];
static UINT8 gx_type4_ctrl_dirty[0x10];
static UINT8 gx_type4_line_dirty[0x800];
static UINT16 gx_type4_line_dirty_list[0x800];
static INT32 gx_type4_ctrl_dirty_count;
static INT32 gx_type4_line_dirty_count;
static UINT8 DrvRecalc;
static GxPaletteUpdates gx_palette_updates;

static INT32 gx_monitor_width = 384;
#define GX_TYPE4_MONITOR_WIDTH gx_monitor_width
#define GX_TYPE4_FLIP_X_OFFSET 60
#define GX_TYPE4_FLIP_Y_OFFSET 2
#define GX_TYPE4_FLIP_SPRITE_Y_ADJUST 4
#define GX_TYPE4_MONITOR_SEPARATOR_WIDTH 1

enum {
	GX_SPECIAL_NONE = 0,
	GX_SPECIAL_DAISKISS = 5,
	GX_SPECIAL_TKMMPZDM = 2,
	GX_SPECIAL_SEXYPARO = 4,
	GX_SPECIAL_TBYAHHOO = 8,
	GX_SPECIAL_FANTJOUR = 9,
	GX_SPECIAL_TYPE4SD2 = 10,
	GX_SPECIAL_SALMNDR2 = 12,
	GX_SPECIAL_DRAGOON = 13,
	GX_SPECIAL_WINSPIKE = 14,
	GX_SPECIAL_LE2 = 15,
	GX_SPECIAL_VSNET = 16,
	GX_SPECIAL_RUSHHERO = 17,
	GX_SPECIAL_SOCCERSS = 18
};

enum { GX_TILE_LAYOUT_5BPP = 0, GX_TILE_LAYOUT_6BPP = 1, GX_TILE_LAYOUT_8BPP = 2 };

struct GxGameConfig {
	const char *name;
	UINT8 cfgport;
	UINT8 special;
	UINT8 tile_layout;
	UINT8 tile_bpp;
	UINT8 tile_color_granularity;
	UINT32 tile_rom_size;
	UINT32 sprite_word_size;
	INT32 sprite_pair0_a;
	INT32 sprite_pair0_b;
	INT32 sprite_pair1_a;
	INT32 sprite_pair1_b;
	INT32 sprite_fifth;
	INT32 sample0;
	INT32 sample1;
	INT32 eeprom;
	INT32 sprite_offset_x;
	INT32 sprite_offset_y;
	INT32 mixer_primode;
	UINT8 sprite_bpp;
};

static const GxGameConfig GxGameConfigs[] = {
	{ "soccerss",  7, GX_SPECIAL_SOCCERSS,GX_TILE_LAYOUT_6BPP,  6,  0, 0x500000, 0x0c00000, -1, -1, -1, -1, -1, -1, -1, -1, -132, -23,  0, 6 },
	{ "soccerssu", 7, GX_SPECIAL_SOCCERSS,GX_TILE_LAYOUT_6BPP,  6,  0, 0x500000, 0x0c00000, -1, -1, -1, -1, -1, -1, -1, -1, -132, -23,  0, 6 },
	{ "soccerssj", 7, GX_SPECIAL_SOCCERSS,GX_TILE_LAYOUT_6BPP,  6,  0, 0x500000, 0x0c00000, -1, -1, -1, -1, -1, -1, -1, -1, -132, -23,  0, 6 },
	{ "soccerssja",7, GX_SPECIAL_SOCCERSS,GX_TILE_LAYOUT_6BPP,  6,  0, 0x500000, 0x0c00000, -1, -1, -1, -1, -1, -1, -1, -1, -132, -23,  0, 6 },
	{ "soccerssa", 7, GX_SPECIAL_SOCCERSS,GX_TILE_LAYOUT_6BPP,  6,  0, 0x500000, 0x0c00000, -1, -1, -1, -1, -1, -1, -1, -1, -132, -23,  0, 6 },

	{ "vsnetscr",  7, GX_SPECIAL_VSNET,GX_TILE_LAYOUT_8BPP,     8,  0, 0x500000, 0x1800000, -1, -1, -1, -1, -1, 16, -1, -1, -132, -23,  0, 6 },
	{ "vsnetscreb",7, GX_SPECIAL_VSNET,GX_TILE_LAYOUT_8BPP,     8,  0, 0x500000, 0x1800000, -1, -1, -1, -1, -1, 16, -1, -1, -132, -23,  0, 6 },
	{ "vsnetscru", 7, GX_SPECIAL_VSNET,GX_TILE_LAYOUT_8BPP,     8,  0, 0x500000, 0x1800000, -1, -1, -1, -1, -1, 16, -1, -1, -132, -23,  0, 6 },
	{ "vsnetscra", 7, GX_SPECIAL_VSNET,GX_TILE_LAYOUT_8BPP,     8,  0, 0x500000, 0x1800000, -1, -1, -1, -1, -1, 16, -1, -1, -132, -23,  0, 6 },
	{ "vsnetscrj", 7, GX_SPECIAL_VSNET,GX_TILE_LAYOUT_8BPP,     8,  0, 0x500000, 0x1800000, -1, -1, -1, -1, -1, 16, -1, -1, -132, -23,  0, 6 },
	{ "rushhero",  7, GX_SPECIAL_RUSHHERO,GX_TILE_LAYOUT_8BPP,  8,  0, 0x600000, 0x3000000, -1, -1, -1, -1, -1, 22, -1, -1,  -79, -24,  0, 6 },

	{ "le2",      13, GX_SPECIAL_LE2,GX_TILE_LAYOUT_8BPP,       8,  0, 0x800000, 0x0800000, -1, -1, -1, -1, -1, 15, 16, 17,  -70, -39, -1, 8 },
	{ "le2u",     13, GX_SPECIAL_LE2,GX_TILE_LAYOUT_8BPP,       8,  0, 0x800000, 0x0800000, -1, -1, -1, -1, -1, 15, 16, 17,  -70, -39, -1, 8 },
	{ "le2j",     13, GX_SPECIAL_LE2,GX_TILE_LAYOUT_8BPP,       8,  0, 0x800000, 0x0800000, -1, -1, -1, -1, -1, 15, 16, 17,  -70, -39, -1, 8 },

	{ "dragoona",  7, GX_SPECIAL_DRAGOON, GX_TILE_LAYOUT_5BPP,  5, 16, 0x400000, 0x1000000, -1, -1, -1, -1, -1, 16, -1, 17,  -93, -39,  0, 4 },
	{ "dragoonj",  7, GX_SPECIAL_DRAGOON, GX_TILE_LAYOUT_5BPP,  5, 16, 0x400000, 0x1000000, -1, -1, -1, -1, -1, 16, -1, 17,  -93, -39,  0, 4 },
	{ "winspike",  8, GX_SPECIAL_WINSPIKE, GX_TILE_LAYOUT_8BPP, 8,  0, 0x800000, 0x1000000, -1, -1, -1, -1, -1, 11, -1, -1,  -92, -39,  0, 8 },
	{ "winspikea", 8, GX_SPECIAL_WINSPIKE, GX_TILE_LAYOUT_8BPP, 8,  0, 0x800000, 0x1000000, -1, -1, -1, -1, -1, 11, -1, -1,  -92, -39,  0, 8 },
	{ "winspikej", 8, GX_SPECIAL_WINSPIKE, GX_TILE_LAYOUT_8BPP, 8,  0, 0x800000, 0x1000000, -1, -1, -1, -1, -1, 11, -1, -1,  -92, -39,  0, 8 },

	{ "fantjour",  7, GX_SPECIAL_FANTJOUR, GX_TILE_LAYOUT_5BPP, 5, 16, 0x280000, 0x0400000,  7,  8, -1, -1,  9, 10, 11, 12,  -70, -38,  0, 5 },
	{ "fantjoura", 7, GX_SPECIAL_FANTJOUR, GX_TILE_LAYOUT_5BPP, 5, 16, 0x280000, 0x0400000,  7,  8, -1, -1,  9, 10, 11, 12,  -70, -38,  0, 5 },
	{ "gokuparo",  7, GX_SPECIAL_NONE,     GX_TILE_LAYOUT_5BPP, 5, 16, 0x280000, 0x0400000,  7,  8, -1, -1,  9, 10, 11, 12,  -70, -38,  0, 5 },
	{ "crzcross",  7, GX_SPECIAL_NONE,     GX_TILE_LAYOUT_5BPP, 5, 16, 0xa00000, 0x0400000,  7,  8, -1, -1,  9, 10, 11, 12,  -70, -38,  5, 5 },
	{ "puzldama",  7, GX_SPECIAL_NONE,     GX_TILE_LAYOUT_5BPP, 5, 16, 0xa00000, 0x0400000,  7,  8, -1, -1,  9, 10, 11, 12,  -70, -38,  5, 5 },
	{ "tbyahhoo",  7, GX_SPECIAL_TBYAHHOO, GX_TILE_LAYOUT_5BPP, 5, 16, 0x280000, 0x0400000,  7,  8, -1, -1,  9, 10, 11, 12,  -70, -38,  0, 5 },
	{ "mtwinbee",  7, GX_SPECIAL_TBYAHHOO, GX_TILE_LAYOUT_5BPP, 5, 16, 0x280000, 0x0400000,  7,  8, -1, -1,  9, 10, 11, 12,  -70, -38,  0, 5 },
	{ "tkmmpzdm",  7, GX_SPECIAL_TKMMPZDM, GX_TILE_LAYOUT_6BPP, 6, 16, 0x180000, 0x0800000,  7,  8,  9, 10, 11, 12, 13, 14,  -70, -38,  5, 5 },
	{ "daiskiss",  7, GX_SPECIAL_DAISKISS, GX_TILE_LAYOUT_5BPP, 5, 16, 0x280000, 0x0200000,  7,  8, -1, -1,  9, 10, -1, -1,  -70, -38,  4, 5 },
	{ "sexyparo",  7, GX_SPECIAL_SEXYPARO, GX_TILE_LAYOUT_5BPP, 5, 16, 0x280000, 0x0400000,  7,  8, -1, -1,  9, 10, 11, -1,  -70, -39,  0, 5 },
	{ "sexyparoa", 7, GX_SPECIAL_SEXYPARO, GX_TILE_LAYOUT_5BPP, 5, 16, 0x280000, 0x0400000,  7,  8, -1, -1,  9, 10, 11, -1,  -70, -39,  0, 5 },
	{ "salmndr2",  7, GX_SPECIAL_SALMNDR2, GX_TILE_LAYOUT_6BPP, 6, 16, 0x800000, 0x0600000,  8,  9, -1, -1, 10, 11, 12, 13,  -70, -38,  0, 6 },
	{ "salmndr2a", 7, GX_SPECIAL_SALMNDR2, GX_TILE_LAYOUT_6BPP, 6, 16, 0x800000, 0x0600000,  8,  9, -1, -1, 10, 11, 12, 13,  -70, -38,  0, 6 },
	{ "tokkae",    7, GX_SPECIAL_NONE,     GX_TILE_LAYOUT_6BPP, 6, 16, 0x180000, 0x0800000,  7,  8,  9, 10, 11, 12, 13, 14,  -70, -38,  5, 5 },
	// Type 4 retains the device default: old MSVC builds converted 256 to UINT8(0).
	{ "rungun2",   7, GX_SPECIAL_TYPE4SD2, GX_TILE_LAYOUT_8BPP, 8,  0, 0x200000, 0x1800000, -1, -1, -1, -1, -1, 22, 23, -1,  -81, -23,  0, 6 },
	{ "slamdnk2",  7, GX_SPECIAL_TYPE4SD2, GX_TILE_LAYOUT_8BPP, 8,  0, 0x200000, 0x1800000, -1, -1, -1, -1, -1, 22, 23, -1,  -81, -23,  0, 6 },

	// Korean Translation
	{ "tbyahhook", 7, GX_SPECIAL_TBYAHHOO, GX_TILE_LAYOUT_5BPP, 5, 16, 0x280000, 0x400000,   7,  8, -1, -1,  9, 10, 11, 12,  -70, -38,  0, 5 },
	{ NULL,        0, GX_SPECIAL_NONE,     GX_TILE_LAYOUT_5BPP, 5, 16,  0,        0,        -1, -1, -1, -1, -1, -1, -1, -1,    0,   0,  0, 5 }
};

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvReset;
static INT16 GxGunAxis[4];
static UINT8 GxGunReload[2];
static bool gx_le2_flip_y;
static INT32 gx_post_frames;
static UINT16 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvConfig[1];

static INT32 gx_dasp_enable = 1;
static INT32 gx_dsp_force;
#define GX_DSP_FORCE_BOOT 900
#define GX_DSP_FORCE_WINDOW 300

static UINT8 gx_tilebanks[8];
static UINT8 gx_tile_ram_bank;
static UINT8 gx_tile_ram_page;
static UINT8 gx_cfgport_default;
static UINT8 gx_cfgport;
static UINT32 gx_tile_rom_size;
static UINT8 gx_tile_bpp;
static UINT8 gx_tile_color_granularity;
static UINT32 gx_sprite_word_size;
static UINT8 gx_sprite_bpp;
static UINT8 gx_type3_psac2_bank;
static UINT8 gx_type3_spriteram_bank;
static UINT8 gx_type4_render_spriteram_bank;
static UINT16 gx_type4_sprite_latch[2][0x800];
static UINT8 gx_type4_sprite_latch_valid[2];
static UINT8 gx_type4_psac_dirty;
static UINT8 gx_type4_psac_tile_dirty[128 * 128];
static UINT16 gx_type4_psac_dirty_list[128 * 128];
static INT32 gx_type4_psac_dirty_count;
static INT32 gx_type4_psac_colorbase;
static INT32 gx_type4_left_bitmap_valid;
static INT32 gx_type4_right_bitmap_valid;
static INT32 gx_type4_display_hold;
static INT32 gx_type4_enable;
static INT32 gx_type4_current_frame;
static INT32 gx_type4_last_dual_screen;
static INT32 gx_type4_last_flip_bits;
static UINT8 gx_irq_control;
static UINT8 gx_irq_status;
static UINT8 gx_syncen;
static UINT8 gx_control;
static UINT8 gx_special;
static INT32 gx_current_scanline;
static UINT8 sound_control;
static UINT8 sound_irq_state;
static UINT8 sound_timer_irq_pending;
static UINT8 sound_int_enabled;
static UINT8 sound_int_pending;
static UINT8 sound_host_status;
static UINT8 sound_host_status_wait;
static INT32 sound_irq_line;
static UINT8 tms_pload;
static UINT8 tms_cload;
static UINT8 tms_reset;
static UINT8 tms_hidx;
static UINT8 tms_pc_nonzero;
static UINT8 tms_program_state;
static UINT8 tms_cload_cval;
static UINT8 tms_host_pending;
static UINT8 tms_host_index;
static UINT32 tms_cycle_frac;
static GxDaspClock gx_dasp_clock;
static tms57002_device DrvTms;
static INT32 gx_fantjour_dma_enable;
static double gx_frame_rate;
static INT32 gx_rushingheroes_hack;
static INT32 gx_default_eeprom_enable;
static INT32 gx_last_prot_op;
static INT32 gx_last_prot_param;
static INT32 gx_last_prot_clk;
static INT16 gx_soundbuf0[4096 * 2];
static INT16 gx_soundbuf1[4096 * 2];
static INT32 gx_sound_buffer_pos;
static INT32 gx_sound_cycles_total;

static inline INT16 gx_clip_int32(INT32 sample)
{
	return BURN_SND_CLIP(sample);
}

static void gx_render_sound_segment(INT32 nSegmentEnd)
{

	if (!pBurnSoundOut || nBurnSoundLen <= 0)
		return;

	if (nSegmentEnd < gx_sound_buffer_pos)
		nSegmentEnd = gx_sound_buffer_pos;
	if (nSegmentEnd > nBurnSoundLen)
		nSegmentEnd = nBurnSoundLen;

	INT32 nSegmentLength = nSegmentEnd - gx_sound_buffer_pos;
	if (nSegmentLength <= 0)
		return;
	const bool dasp_active = tms_reset && (gx_dasp_enable || gx_dsp_force > 0);
	// Disabled PCM updates are no-ops; only DASP can still generate output.
	if (!dasp_active && !(K054539Read(0, 0x22f) & 1) && !(K054539Read(1, 0x22f) & 1)) {
		gx_sound_buffer_pos = nSegmentEnd;
		return;
	}

	INT16 *pSoundBuf = pBurnSoundOut + (gx_sound_buffer_pos << 1);
	memset(gx_soundbuf0, 0, nSegmentLength * 2 * sizeof(INT16));
	K054539Update(0, gx_soundbuf0, nSegmentLength);
	memset(gx_soundbuf1, 0, nSegmentLength * 2 * sizeof(INT16));
	K054539Update(1, gx_soundbuf1, nSegmentLength);

	if (dasp_active) {
		const UINT32 tms_cycle_step = gx_dasp_clock.Step(nBurnSoundRate);

		for (INT32 i = 0; i < nSegmentLength; i++) {
			INT16 dasp_in[4];
			INT16 dasp_out[4]; // process_sample writes all four outputs, including idle/load states.
			INT32 pos = i << 1;

			dasp_in[0] = gx_clip_int32(gx_soundbuf0[pos + 0] / 2);
			dasp_in[1] = gx_clip_int32(gx_soundbuf0[pos + 1] / 2);
			dasp_in[2] = gx_clip_int32(gx_soundbuf1[pos + 0] / 2);
			dasp_in[3] = gx_clip_int32(gx_soundbuf1[pos + 1] / 2);

			tms_cycle_frac += tms_cycle_step;
			INT32 tms_cycles = tms_cycle_frac >> 16;
			tms_cycle_frac &= 0xffff;
			DrvTms.process_sample(dasp_in, dasp_out, tms_cycles);

			INT32 left = pSoundBuf[pos + 0] + gx_soundbuf0[pos + 0] + gx_soundbuf1[pos + 0];
			INT32 right = pSoundBuf[pos + 1] + gx_soundbuf0[pos + 1] + gx_soundbuf1[pos + 1];

			left += (dasp_out[0] * 3) / 10;
			left += (dasp_out[2] * 3) / 10;
			right += (dasp_out[1] * 3) / 10;
			right += (dasp_out[3] * 3) / 10;

			pSoundBuf[pos + 0] = gx_clip_int32(left);
			pSoundBuf[pos + 1] = gx_clip_int32(right);
		}
	} else {
		for (INT32 i = 0; i < nSegmentLength * 2; i++) {
			pSoundBuf[i] = gx_clip_int32(pSoundBuf[i] + gx_soundbuf0[i] + gx_soundbuf1[i]);
		}
	}

	gx_sound_buffer_pos = nSegmentEnd;
}

static void gx_sync_sound_stream()
{
	if (!pBurnSoundOut || nBurnSoundLen <= 0 || gx_sound_cycles_total <= 0)
		return;

	INT32 cycles = gx_sample_cycles >= 0 ? gx_sample_cycles : SekTotalCycles(1);
	if (cycles < 0)
		cycles = 0;
	if (cycles > gx_sound_cycles_total)
		cycles = gx_sound_cycles_total;

	const INT64 scaled_cycles = (INT64)nBurnSoundLen * cycles;
	// Register accesses within an already rendered sample need no division or mixing.
	if (scaled_cycles < ((INT64)gx_sound_buffer_pos + 1) * gx_sound_cycles_total)
		return;
	gx_render_sound_segment((INT32)(scaled_cycles / gx_sound_cycles_total));
}

static UINT8 fantjour_dma[0x20];
static INT32 nExtraCycles[2];

static INT32 layer_colorbase[4];
static INT32 sprite_colorbase;

static UINT32 gx_esc_latch;
static GxEscChip gx_esc_chip;
static UINT8 gx_esc_enabled;
static UINT8 gx_objscan;

static UINT8 __fastcall gx_main_read_byte(UINT32 address);
static UINT16 __fastcall gx_main_read_word(UINT32 address);
static UINT32 __fastcall gx_main_read_long(UINT32 address);
static void __fastcall gx_main_write_byte(UINT32 address, UINT8 data);
static void __fastcall gx_main_write_word(UINT32 address, UINT16 data);
static void __fastcall gx_main_write_long(UINT32 address, UINT32 data);
static void gx_esc_write(UINT32 data);
static void gx_type4_prot_write(UINT32 address, UINT32 data);
static const GxGameConfig *gx_get_game_config();

static void gx_patch_mainrom_long(UINT32 address, UINT32 set_mask, UINT32 clear_mask)
{
	UINT32 data = BURN_ENDIAN_SWAP_INT32(*((UINT32 *)(DrvMainROM + address)));

	data &= ~clear_mask;
	data |= set_mask;

	*((UINT32 *)(DrvMainROM + address)) = BURN_ENDIAN_SWAP_INT32(data);
}

static inline UINT8 gx_ram_read_byte(UINT8 *ram, UINT32 address, UINT32 mask)
{
	return ram[(address & mask) ^ 3];
}

static inline UINT16 gx_ram_read_word(UINT8 *ram, UINT32 address, UINT32 mask)
{
	return (gx_ram_read_byte(ram, address + 0, mask) << 8) | gx_ram_read_byte(ram, address + 1, mask);
}

static inline UINT32 gx_ram_read_long(UINT8 *ram, UINT32 address, UINT32 mask)
{
	return (gx_ram_read_word(ram, address + 0, mask) << 16) | gx_ram_read_word(ram, address + 2, mask);
}

static inline void gx_ram_write_byte(UINT8 *ram, UINT32 address, UINT32 mask, UINT8 data)
{
	ram[(address & mask) ^ 3] = data;
}

static inline void gx_ram_write_word(UINT8 *ram, UINT32 address, UINT32 mask, UINT16 data)
{
	gx_ram_write_byte(ram, address + 0, mask, data >> 8);
	gx_ram_write_byte(ram, address + 1, mask, data);
}

static inline void gx_palette_write_byte(UINT32 address, UINT8 data)
{
	const UINT32 offset = (address & 0x7fff) ^ 3;
	if (DrvPalRAM[offset] == data)
		return;
	DrvPalRAM[offset] = data;
	// Ordinary GX palettes use only the low 24 bits; retain unused bytes in RAM.
#ifdef LSB_FIRST
	if (!(address & 3))
		return;
#endif
	gx_palette_updates.Mark(address);
}

static void gx_type4_mark_all_palettes_dirty()
{
	memset(gx_type4_palette_dirty, 1, sizeof(gx_type4_palette_dirty));
	gx_type4_palette_dirty_count[0] = 0x2000;
	gx_type4_palette_dirty_count[1] = 0x2000;

	// Full-bank updates bypass the sparse lists.
}

static inline void gx_type4_mark_palette_dirty(INT32 use_sub_palette, UINT32 address, INT32 bytes)
{
	if (bytes <= 0)
		return;
	INT32 palette = use_sub_palette ? 1 : 0;
	const UINT32 first = (address & 0x7fff) >> 2;
	const UINT32 count = ((address & 3) + (UINT32)bytes + 3) >> 2;

	for (UINT32 i = 0; i < count; i++) {
		INT32 index = (first + i) & 0x1fff;

		if (!gx_type4_palette_dirty[palette][index]) {
			gx_type4_palette_dirty[palette][index] = 1;
			gx_type4_palette_dirty_list[palette][gx_type4_palette_dirty_count[palette]] = index;
			gx_type4_palette_dirty_count[palette]++;
		}
	}
}

static inline void gx_type4_write_palette(INT32 use_sub_palette, UINT32 address, UINT32 data, UINT32 bytes)
{
	UINT8 *ram = use_sub_palette ? DrvSubPalRAM : DrvPalRAM;
#ifdef LSB_FIRST
	if (bytes == 4 && !(address & 3)) {
		UINT8 *dst = ram + (address & 0x7fff);
		UINT32 old;
		memcpy(&old, dst, sizeof(old));
		const UINT32 changed = old ^ data;
		if (!changed)
			return;
		memcpy(dst, &data, sizeof(data));
		// Preserve the unused high byte without scheduling an RGB conversion.
		if (changed & 0x00ffffff)
			gx_type4_mark_palette_dirty(use_sub_palette, address, 4);
		return;
	}
#endif
	bool rgb_changed = false;
	for (UINT32 i = 0; i < bytes; i++) {
		const UINT32 offset = ((address + i) & 0x7fff) ^ 3;
		const UINT8 value = (UINT8)(data >> ((bytes - i - 1) * 8));
		if (ram[offset] != value) {
			ram[offset] = value;
			if ((address + i) & 3)
				rgb_changed = true;
		}
	}
	// Preserve all RAM bytes, but only RGB changes require palette conversion.
	if (rgb_changed)
		gx_type4_mark_palette_dirty(use_sub_palette, address, bytes);
}

static void gx_type4_mark_all_k053936_dirty()
{
	memset(gx_type4_ctrl_dirty, 1, sizeof(gx_type4_ctrl_dirty));
	memset(gx_type4_line_dirty, 1, sizeof(gx_type4_line_dirty));
	gx_type4_ctrl_dirty_count = 0x10;
	gx_type4_line_dirty_count = 0x800;

	// A full refresh does not consume the sparse dirty list.
}

static inline void gx_type4_mark_k053936_dirty(UINT32 address, INT32 bytes, INT32 line_ram)
{
	if (bytes <= 0)
		return;
	UINT8 *dirty = line_ram ? gx_type4_line_dirty : gx_type4_ctrl_dirty;
	UINT32 mask = line_ram ? 0xfff : 0x1f;
	const UINT32 first = (address & mask) >> 1;
	const UINT32 count = ((address & 1) + (UINT32)bytes + 1) >> 1;

	for (UINT32 i = 0; i < count; i++) {
		INT32 index = (((first + i) & (mask >> 1)) ^ 1);

		if (!dirty[index]) {
			dirty[index] = 1;
			if (line_ram) {
				gx_type4_line_dirty_list[gx_type4_line_dirty_count++] = index;
			} else {
				gx_type4_ctrl_dirty_count++;
			}
		}
	}
}

static inline void gx_type4_write_k053936(UINT32 address, UINT32 data, UINT32 bytes, INT32 line_ram)
{
	UINT8 *ram = line_ram ? DrvType4LineRAM : DrvType4CtrlRAM;
	const UINT32 mask = line_ram ? 0xfff : 0x1f;
#ifdef LSB_FIRST
	if (bytes == 2 && !(address & 1)) {
		UINT8 *dst = ram + ((address & mask) ^ 2);
		UINT16 old;
		const UINT16 value = (UINT16)data;
		memcpy(&old, dst, sizeof(old));
		if (old == value)
			return;
		memcpy(dst, &value, sizeof(value));
		gx_type4_mark_k053936_dirty(address, bytes, line_ram);
		return;
	}
	if (bytes == 4 && !(address & 3)) {
		UINT8 *dst = ram + (address & mask);
		UINT32 old;
		memcpy(&old, dst, sizeof(old));
		if (old == data)
			return;
		memcpy(dst, &data, sizeof(data));
		// Retain the whole-write invalidation order for the two register words.
		gx_type4_mark_k053936_dirty(address, bytes, line_ram);
		return;
	}
#endif
	bool changed = false;
	for (UINT32 i = 0; i < bytes; i++) {
		const UINT32 offset = ((address + i) & mask) ^ 3;
		const UINT8 value = (UINT8)(data >> ((bytes - i - 1) * 8));
		if (ram[offset] != value) {
			ram[offset] = value;
			changed = true;
		}
	}
	if (changed)
		gx_type4_mark_k053936_dirty(address, bytes, line_ram);
}

static inline UINT8 gx_mainram_read_byte(UINT32 address)
{
	return DrvMainRAM[(address & 0x1ffff) ^ 1];
}

static inline UINT16 gx_mainram_read_word(UINT32 address)
{
	return (DrvMainRAM[(address & 0x1ffff) ^ 1] << 8) | DrvMainRAM[((address + 1) & 0x1ffff) ^ 1];
}

static inline UINT32 gx_mainram_read_long(UINT32 address)
{
	return (gx_mainram_read_word(address + 0) << 16) | gx_mainram_read_word(address + 2);
}

static inline void gx_mainram_write_byte(UINT32 address, UINT8 data)
{
	DrvMainRAM[(address & 0x1ffff) ^ 1] = data;
}

static inline void gx_mainram_write_word(UINT32 address, UINT16 data)
{
	DrvMainRAM[(address & 0x1ffff) ^ 1] = data >> 8;
	DrvMainRAM[((address + 1) & 0x1ffff) ^ 1] = data;
}

static inline void gx_mainram_write_long(UINT32 address, UINT32 data)
{
	gx_mainram_write_word(address + 0, data >> 16);
	gx_mainram_write_word(address + 2, data);
}

static inline void gx_type4_mark_all_psac_dirty()
{
	gx_type4_psac_dirty = 1;
	gx_type4_psac_dirty_count = 128 * 128;
	memset(gx_type4_psac_tile_dirty, 1, sizeof(gx_type4_psac_tile_dirty));
}

static inline void gx_type4_mark_psac_dirty(UINT32 address, UINT32 bytes)
{
	// Each 16-bit descriptor owns one tile; include unaligned/wrapping writes.
	const UINT32 first = (address & 0x7fff) >> 1;
	const UINT32 count = ((address & 1) + bytes + 1) >> 1;
	for (UINT32 i = 0; i < count; i++) {
		const UINT32 tile_index = (first + i) & 0x3fff;
		if (!gx_type4_psac_tile_dirty[tile_index]) {
			gx_type4_psac_tile_dirty[tile_index] = 1;
			gx_type4_psac_dirty_list[gx_type4_psac_dirty_count] = tile_index;
			gx_type4_psac_dirty_count++;
		}
	}

	gx_type4_psac_dirty = 1;
}

static inline void gx_type4_write_psac(UINT32 address, UINT32 data, UINT32 bytes)
{
#ifdef LSB_FIRST
	if (bytes == 4 && !(address & 3)) {
		UINT8 *dst = DrvType4PsacRAM + (address & 0x7fff);
		UINT32 old;
		memcpy(&old, dst, sizeof(old));
		const UINT32 changed = old ^ data;
		if (!changed)
			return;
		memcpy(dst, &data, sizeof(data));
		// The high halfword is the first emulated tile descriptor.
		if (changed & 0xffff0000)
			gx_type4_mark_psac_dirty(address, 1);
		if (changed & 0x0000ffff)
			gx_type4_mark_psac_dirty(address + 2, 1);
		return;
	}
	if (bytes == 2 && !(address & 1)) {
		UINT8 *dst = DrvType4PsacRAM + ((address & 0x7fff) ^ 2);
		UINT16 old;
		const UINT16 value = (UINT16)data;
		memcpy(&old, dst, sizeof(old));
		if (old == value)
			return;
		memcpy(dst, &value, sizeof(value));
		// Both bytes belong to one descriptor; queue that tile only once.
		gx_type4_mark_psac_dirty(address, 1);
		return;
	}
#endif
	// PSAC descriptor RAM has no write side effects. Only changed bytes dirty tiles.
	for (UINT32 i = 0; i < bytes; i++) {
		const UINT8 value = (UINT8)(data >> ((bytes - i - 1) * 8));
		const UINT32 offset = ((address + i) & 0x7fff) ^ 3;
		if (DrvType4PsacRAM[offset] != value) {
			DrvType4PsacRAM[offset] = value;
			gx_type4_mark_psac_dirty(address + i, 1);
		}
	}
}

static inline void gx_type3_bank_write(UINT32 address, UINT8 data)
{
	if ((address & 3) == 0) {
		gx_type3_psac2_bank = (data & 0x10) >> 4;
		gx_type3_spriteram_bank = data & 0x01;

		if (gx_type4_enable && K053247Ram) {
			memcpy(gx_type4_sprite_latch[gx_type3_spriteram_bank], K053247Ram + (gx_type3_spriteram_bank * 0x1000), 0x1000);
			gx_type4_sprite_latch_valid[gx_type3_spriteram_bank] = 1;
		}
	}
}

static void gx_k056832_set_ram_bank(UINT8 data)
{
	gx_tile_ram_bank = data;
	gx_tile_ram_page = ((data >> 1) & 0x0c) | (data & 0x03);
}

static UINT8 gx_system_dsw0_read()
{
	// MAME's cfgport table is obsolete; the hardware reads the physical DIP bank.
	if (gx_special >= GX_SPECIAL_DRAGOON)
		return DrvDips[0];

	if (gx_type4_enable)
		return DrvDips[0] & 0x0f;

	return (DrvDips[0] & 0x01) | ((gx_cfgport << 1) & 0xfe);
}

static struct BurnInputInfo GxInputList[] = {
	{ "P1 Coin",      BIT_DIGITAL,   DrvJoy1 + 0,   "p1 coin"   },
	{ "P1 Start",     BIT_DIGITAL,   DrvJoy2 + 7,   "p1 start"  },
	{ "P1 Up",        BIT_DIGITAL,   DrvJoy2 + 2,   "p1 up"     },
	{ "P1 Down",      BIT_DIGITAL,   DrvJoy2 + 3,   "p1 down"   },
	{ "P1 Left",      BIT_DIGITAL,   DrvJoy2 + 0,   "p1 left"   },
	{ "P1 Right",     BIT_DIGITAL,   DrvJoy2 + 1,   "p1 right"  },
	{ "P1 Button 1",  BIT_DIGITAL,   DrvJoy2 + 4,   "p1 fire 1" },
	{ "P1 Button 2",  BIT_DIGITAL,   DrvJoy2 + 5,   "p1 fire 2" },
	{ "P1 Button 3",  BIT_DIGITAL,   DrvJoy2 + 6,   "p1 fire 3" },

	{ "P2 Coin",      BIT_DIGITAL,   DrvJoy1 + 1,   "p2 coin"   },
	{ "P2 Start",     BIT_DIGITAL,   DrvJoy2 + 15,  "p2 start"  },
	{ "P2 Up",        BIT_DIGITAL,   DrvJoy2 + 10,  "p2 up"     },
	{ "P2 Down",      BIT_DIGITAL,   DrvJoy2 + 11,  "p2 down"   },
	{ "P2 Left",      BIT_DIGITAL,   DrvJoy2 + 8,   "p2 left"   },
	{ "P2 Right",     BIT_DIGITAL,   DrvJoy2 + 9,   "p2 right"  },
	{ "P2 Button 1",  BIT_DIGITAL,   DrvJoy2 + 12,  "p2 fire 1" },
	{ "P2 Button 2",  BIT_DIGITAL,   DrvJoy2 + 13,  "p2 fire 2" },
	{ "P2 Button 3",  BIT_DIGITAL,   DrvJoy2 + 14,  "p2 fire 3" },

	{ "Reset",        BIT_DIGITAL,   &DrvReset,     "reset"     },
	{ "Service Mode", BIT_DIGITAL,   DrvJoy1 + 7,   "diag"      },
	{ "Service 1",    BIT_DIGITAL,   DrvJoy1 + 4,   "service"   },
	{ "Dip A",        BIT_DIPSWITCH, DrvDips + 0,   "dip"       },
	{ "Dip B",        BIT_DIPSWITCH, DrvDips + 1,   "dip"       },
	{ "DASP",         BIT_DIPSWITCH, DrvConfig + 0, "dip"       },
};

STDINPUTINFO(Gx)

static struct BurnInputInfo Gx4pInputList[] = {
	{ "P1 Coin",      BIT_DIGITAL,   DrvJoy1 + 0,   "p1 coin"   },
	{ "P1 Start",     BIT_DIGITAL,   DrvJoy2 + 7,   "p1 start"  },
	{ "P1 Up",        BIT_DIGITAL,   DrvJoy2 + 2,   "p1 up"     },
	{ "P1 Down",      BIT_DIGITAL,   DrvJoy2 + 3,   "p1 down"   },
	{ "P1 Left",      BIT_DIGITAL,   DrvJoy2 + 0,   "p1 left"   },
	{ "P1 Right",     BIT_DIGITAL,   DrvJoy2 + 1,   "p1 right"  },
	{ "P1 Button 1",  BIT_DIGITAL,   DrvJoy2 + 4,   "p1 fire 1" },
	{ "P1 Button 2",  BIT_DIGITAL,   DrvJoy2 + 5,   "p1 fire 2" },
	{ "P1 Button 3",  BIT_DIGITAL,   DrvJoy2 + 6,   "p1 fire 3" },

	{ "P2 Coin",      BIT_DIGITAL,   DrvJoy1 + 1,   "p2 coin"   },
	{ "P2 Start",     BIT_DIGITAL,   DrvJoy2 + 15,  "p2 start"  },
	{ "P2 Up",        BIT_DIGITAL,   DrvJoy2 + 10,  "p2 up"     },
	{ "P2 Down",      BIT_DIGITAL,   DrvJoy2 + 11,  "p2 down"   },
	{ "P2 Left",      BIT_DIGITAL,   DrvJoy2 + 8,   "p2 left"   },
	{ "P2 Right",     BIT_DIGITAL,   DrvJoy2 + 9,   "p2 right"  },
	{ "P2 Button 1",  BIT_DIGITAL,   DrvJoy2 + 12,  "p2 fire 1" },
	{ "P2 Button 2",  BIT_DIGITAL,   DrvJoy2 + 13,  "p2 fire 2" },
	{ "P2 Button 3",  BIT_DIGITAL,   DrvJoy2 + 14,  "p2 fire 3" },

	{ "Reset",        BIT_DIGITAL,   &DrvReset,     "reset"     },
	{ "Service Mode", BIT_DIGITAL,   DrvJoy1 + 7,   "diag"      },
	{ "Service 1",    BIT_DIGITAL,   DrvJoy1 + 4,   "service"   },
	{ "Dip A",        BIT_DIPSWITCH, DrvDips + 0,   "dip"       },
	{ "Dip B",        BIT_DIPSWITCH, DrvDips + 1,   "dip"       },
	{ "DASP",         BIT_DIPSWITCH, DrvConfig + 0, "dip"       },
	{ "P3 Start",     BIT_DIGITAL,   DrvJoy3 + 15,  "p3 start"  },
	{ "P3 Up",        BIT_DIGITAL,   DrvJoy3 + 10,  "p3 up"     },
	{ "P3 Down",      BIT_DIGITAL,   DrvJoy3 + 11,  "p3 down"   },
	{ "P3 Left",      BIT_DIGITAL,   DrvJoy3 + 8,   "p3 left"   },
	{ "P3 Right",     BIT_DIGITAL,   DrvJoy3 + 9,   "p3 right"  },
	{ "P3 Button 1",  BIT_DIGITAL,   DrvJoy3 + 12,  "p3 fire 1" },
	{ "P3 Button 2",  BIT_DIGITAL,   DrvJoy3 + 13,  "p3 fire 2" },
	{ "P3 Button 3",  BIT_DIGITAL,   DrvJoy3 + 14,  "p3 fire 3" },
	{ "P3 Coin",      BIT_DIGITAL,   DrvJoy1 + 2,   "p3 coin"   },
	{ "P4 Start",     BIT_DIGITAL,   DrvJoy3 + 7,   "p4 start"  },
	{ "P4 Up",        BIT_DIGITAL,   DrvJoy3 + 2,   "p4 up"     },
	{ "P4 Down",      BIT_DIGITAL,   DrvJoy3 + 3,   "p4 down"   },
	{ "P4 Left",      BIT_DIGITAL,   DrvJoy3 + 0,   "p4 left"   },
	{ "P4 Right",     BIT_DIGITAL,   DrvJoy3 + 1,   "p4 right"  },
	{ "P4 Button 1",  BIT_DIGITAL,   DrvJoy3 + 4,   "p4 fire 1" },
	{ "P4 Button 2",  BIT_DIGITAL,   DrvJoy3 + 5,   "p4 fire 2" },
	{ "P4 Button 3",  BIT_DIGITAL,   DrvJoy3 + 6,   "p4 fire 3" },
	{ "P4 Coin",      BIT_DIGITAL,   DrvJoy1 + 3,   "p4 coin"   },
};

STDINPUTINFO(Gx4p)

static struct BurnInputInfo DragoonInputList[] = {
	{ "P1 Coin",      BIT_DIGITAL,   DrvJoy1 + 0,   "p1 coin"   },
	{ "P1 Start",     BIT_DIGITAL,   DrvJoy2 + 7,   "p1 start"  },
	{ "P1 Up",        BIT_DIGITAL,   DrvJoy2 + 2,   "p1 up"     },
	{ "P1 Down",      BIT_DIGITAL,   DrvJoy2 + 3,   "p1 down"   },
	{ "P1 Left",      BIT_DIGITAL,   DrvJoy2 + 0,   "p1 left"   },
	{ "P1 Right",     BIT_DIGITAL,   DrvJoy2 + 1,   "p1 right"  },
	{ "P1 Button 1",  BIT_DIGITAL,   DrvJoy2 + 4,   "p1 fire 1" },
	{ "P1 Button 2",  BIT_DIGITAL,   DrvJoy2 + 5,   "p1 fire 2" },
	{ "P1 Button 3",  BIT_DIGITAL,   DrvJoy2 + 6,   "p1 fire 3" },

	{ "P2 Coin",      BIT_DIGITAL,   DrvJoy1 + 1,   "p2 coin"   },
	{ "P2 Start",     BIT_DIGITAL,   DrvJoy2 + 15,  "p2 start"  },
	{ "P2 Up",        BIT_DIGITAL,   DrvJoy2 + 10,  "p2 up"     },
	{ "P2 Down",      BIT_DIGITAL,   DrvJoy2 + 11,  "p2 down"   },
	{ "P2 Left",      BIT_DIGITAL,   DrvJoy2 + 8,   "p2 left"   },
	{ "P2 Right",     BIT_DIGITAL,   DrvJoy2 + 9,   "p2 right"  },
	{ "P2 Button 1",  BIT_DIGITAL,   DrvJoy2 + 12,  "p2 fire 1" },
	{ "P2 Button 2",  BIT_DIGITAL,   DrvJoy2 + 13,  "p2 fire 2" },
	{ "P2 Button 3",  BIT_DIGITAL,   DrvJoy2 + 14,  "p2 fire 3" },

	{ "Reset",        BIT_DIGITAL,   &DrvReset,     "reset"     },
	{ "Service Mode", BIT_DIGITAL,   DrvJoy1 + 7,   "diag"      },
	{ "Service 1",    BIT_DIGITAL,   DrvJoy1 + 4,   "service"   },
	{ "Dip A",        BIT_DIPSWITCH, DrvDips + 0,   "dip"       },
	{ "Dip B",        BIT_DIPSWITCH, DrvDips + 1,   "dip"       },
	{ "DASP",         BIT_DIPSWITCH, DrvConfig + 0, "dip"       },
	{ "P1 Button 4",  BIT_DIGITAL,   DrvJoy3 + 12,  "p1 fire 4" },
	{ "P1 Button 5",  BIT_DIGITAL,   DrvJoy3 + 13,  "p1 fire 5" },
	{ "P1 Button 6",  BIT_DIGITAL,   DrvJoy3 + 14,  "p1 fire 6" },
	{ "P2 Button 4",  BIT_DIGITAL,   DrvJoy3 + 8,   "p2 fire 4" },
	{ "P2 Button 5",  BIT_DIGITAL,   DrvJoy3 + 9,   "p2 fire 5" },
	{ "P2 Button 6",  BIT_DIGITAL,   DrvJoy3 + 10,  "p2 fire 6" },
};

STDINPUTINFO(Dragoon)

static struct BurnInputInfo Le2InputList[] = {
	{ "P1 Coin",      BIT_DIGITAL,    DrvJoy1 + 0,           "p1 coin"        },
	{ "P1 Start",     BIT_DIGITAL,    DrvJoy2 + 7,           "p1 start"       },
	{ "P1 Button 1",  BIT_DIGITAL,    DrvJoy2 + 4,           "mouse button 1" },
	{ "P1 Gun X",     BIT_ANALOG_REL, (UINT8*)&GxGunAxis[0], "mouse x-axis"   },
	{ "P1 Gun Y",     BIT_ANALOG_REL, (UINT8*)&GxGunAxis[1], "mouse y-axis"   },
	{ "P2 Coin",      BIT_DIGITAL,    DrvJoy1 + 1,           "p2 coin"        },
	{ "P2 Start",     BIT_DIGITAL,    DrvJoy2 + 15,          "p2 start"       },
	{ "P2 Button 1",  BIT_DIGITAL,    DrvJoy2 + 12,          "p2 fire 1"      },
	{ "P2 Gun X",     BIT_ANALOG_REL, (UINT8*)&GxGunAxis[2], "p2 x-axis"      },
	{ "P2 Gun Y",     BIT_ANALOG_REL, (UINT8*)&GxGunAxis[3], "p2 y-axis"      },
	{ "Reset",        BIT_DIGITAL,    &DrvReset,             "reset"          },
	{ "Service Mode", BIT_DIGITAL,    DrvJoy1 + 7,           "diag"           },
	{ "Service 1",    BIT_DIGITAL,    DrvJoy1 + 4,           "service"        },
	{ "Dip A",        BIT_DIPSWITCH,  DrvDips + 0,           "dip"            },
	{ "Dip B",        BIT_DIPSWITCH,  DrvDips + 1,           "dip"            },
	{ "DASP",         BIT_DIPSWITCH,  DrvConfig + 0,         "dip"            },
	{ "P1 Button 2",  BIT_DIGITAL,    GxGunReload + 0,       "mouse button 2" },
	{ "P2 Button 2",  BIT_DIGITAL,    GxGunReload + 1,       "p2 fire 2"      },
};

STDINPUTINFO(Le2)

static struct BurnDIPInfo GxDIPList[] = {
	{ 0x15, 0xff, 0xff, 0xfe, NULL                        },
	{ 0x16, 0xff, 0xff, 0xff, NULL                        },
	{ 0x17, 0xff, 0xff, 0x01, NULL                        },

	{ 0,    0xfe, 0,    2,    "DASP (TMS57002) Reverb"    },
	{ 0x17, 0x01, 0x01, 0x01, "On"                        },
	{ 0x17, 0x01, 0x01, 0x00, "Off"                       },

	{ 0,    0xfe, 0,    2,    "Sound Output"              },
	{ 0x15, 0x01, 0x01, 0x00, "Stereo"                    },
	{ 0x15, 0x01, 0x01, 0x01, "Mono"                      },

	{ 0,    0xfe, 0,    2,    "Left Monitor Flip Screen"  },
	{ 0x15, 0x01, 0x02, 0x02, "Off"                       },
	{ 0x15, 0x01, 0x02, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Right Monitor Flip Screen" },
	{ 0x15, 0x01, 0x04, 0x04, "Off"                       },
	{ 0x15, 0x01, 0x04, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Number of Screens"         },
	{ 0x15, 0x01, 0x08, 0x08, "1"                         },
	{ 0x15, 0x01, 0x08, 0x00, "2"                         },
};

STDDIPINFO(Gx)

static struct BurnDIPInfo GxCommonDIPList[] = {
	{ 0x15, 0xff, 0xff, 0xfe, NULL                     },
	{ 0x16, 0xff, 0xff, 0xff, NULL                     },
	{ 0x17, 0xff, 0xff, 0x01, NULL                     },

	{ 0,    0xfe, 0,    2,    "DASP (TMS57002) Reverb" },
	{ 0x17, 0x01, 0x01, 0x01, "On"                     },
	{ 0x17, 0x01, 0x01, 0x00, "Off"                    },

	{ 0,    0xfe, 0,    2,    "Sound Output"           },
	{ 0x15, 0x01, 0x01, 0x00, "Stereo"                 },
	{ 0x15, 0x01, 0x01, 0x01, "Mono"                   },

	{ 0,    0xfe, 0,    2,    "Flip Screen"            },
	{ 0x15, 0x01, 0x02, 0x02, "Off"                    },
	{ 0x15, 0x01, 0x02, 0x00, "On"                     },
};

STDDIPINFO(GxCommon)

static struct BurnDIPInfo Le2DIPList[] = {
	{ 0x0d, 0xff, 0xff, 0xf7, NULL                     },
	{ 0x0e, 0xff, 0xff, 0xff, NULL                     },
	{ 0x0f, 0xff, 0xff, 1,    NULL                     },

	{ 0,    0xfe, 0,    2,    "Sound Output"           },
	{ 0x0d, 1,    1,    1,    "Stereo"                 },
	{ 0x0d, 1,    1,    0,    "Mono"                   },

	{ 0,    0xfe, 0,    2,    "Coin Mechanism"         },
	{ 0x0d, 1,    2,    2,    "Common"                 },
	{ 0x0d, 1,    2,    0,    "Independent"            },

	{ 0,    0xfe, 0,    2,    "Stage Select"           },
	{ 0x0d, 1,    4,    4,    "Off"                    },
	{ 0x0d, 1,    4,    0,    "On"                     },

	{ 0,    0xfe, 0,    2,    "Mirror"                 },
	{ 0x0d, 1,    8,    0,    "Off"                    },
	{ 0x0d, 1,    8,    8,    "On"                     },

	{ 0,    0xfe, 0,    2,    "Flip Screen"            },
	{ 0x0d, 1,    16,   16,   "Off"                    },
	{ 0x0d, 1,    16,   0,    "On"                     },

	{ 0,    0xfe, 0,    2,    "DASP (TMS57002) Reverb" },
	{ 0x0f, 1,    1,    1,    "On"                     },
	{ 0x0f, 1,    1,    0,    "Off"                    },
};

STDDIPINFO(Le2)

static struct BurnDIPInfo Le2uDIPList[] = {
	{ 0x0d, 0xff, 0xff, 0xff, NULL                     },
	{ 0x0e, 0xff, 0xff, 0xff, NULL                     },
	{ 0x0f, 0xff, 0xff, 1,    NULL                     },

	{ 0,    0xfe, 0,    2,    "Sound Output"           },
	{ 0x0d, 1,    1,    1,    "Stereo"                 },
	{ 0x0d, 1,    1,    0,    "Mono"                   },

	{ 0,    0xfe, 0,    2,    "Coin Mechanism"         },
	{ 0x0d, 1,    2,    2,    "Common"                 },
	{ 0x0d, 1,    2,    0,    "Independent"            },

	{ 0,    0xfe, 0,    2,    "Stage Select"           },
	{ 0x0d, 1,    4,    4,    "Off"                    },
	{ 0x0d, 1,    4,    0,    "On"                     },

	{ 0,    0xfe, 0,    2,    "DASP (TMS57002) Reverb" },
	{ 0x0f, 1,    1,    1,    "On"                     },
	{ 0x0f, 1,    1,    0,    "Off"                    },
};

STDDIPINFO(Le2u)

static struct BurnDIPInfo Le2jDIPList[] = {
	{ 0x0d, 0xff, 0xff, 0xf7, NULL                     },
	{ 0x0e, 0xff, 0xff, 0xff, NULL                     },
	{ 0x0f, 0xff, 0xff, 1,    NULL                     },

	{ 0,    0xfe, 0,    2,    "Sound Output"           },
	{ 0x0d, 1,    1,    1,    "Stereo"                 },
	{ 0x0d, 1,    1,    0,    "Mono"                   },

	{ 0,    0xfe, 0,    2,    "Coin Mechanism"         },
	{ 0x0d, 1,    2,    2,    "Common"                 },
	{ 0x0d, 1,    2,    0,    "Independent"            },

	{ 0,    0xfe, 0,    2,    "Stage Select"           },
	{ 0x0d, 1,    4,    0,    "Off"                    },
	{ 0x0d, 1,    4,    4,    "On"                     },

	{ 0,    0xfe, 0,    2,    "Mirror"                 },
	{ 0x0d, 1,    8,    8,    "Off"                    },
	{ 0x0d, 1,    8,    0,    "On"                     },

	{ 0,    0xfe, 0,    2,    "Flip Screen"            },
	{ 0x0d, 1,    16,   16,   "Off"                    },
	{ 0x0d, 1,    16,   0,    "On"                     },

	{ 0,    0xfe, 0,    2,    "DASP (TMS57002) Reverb" },
	{ 0x0f, 1,    1,    1,    "On"                     },
	{ 0x0f, 1,    1,    0,    "Off"                    },
};

STDDIPINFO(Le2j)

static const eeprom_interface gx_eeprom_interface = {
	6, 16, "*110", "*101", "*111", "*10000xxxx", "*10011xxxx", 1, 0,
};

static void gx_tile_callback(INT32 layer, INT32 *code, INT32 *color, INT32 *)
{
	INT32 d = *code;
	INT32 pal = *color;
	INT32 shift = layer << 1;
	INT32 vmx = K055555ReadRegister(K55_BLEND_ENABLES) >> shift & 3;
	INT32 von = K055555ReadRegister(K55_VINMIX_ON) >> shift & 3;
	INT32 pl45 = pal >> 4 & 3;

	*code = (gx_tilebanks[(d >> 13) & 7] << 13) | (d & 0x1fff);
	if (gx_special >= GX_SPECIAL_DRAGOON)
		*code %= gx_tile_rom_size / (gx_tile_bpp * 8);
	else if (gx_special != GX_SPECIAL_SALMNDR2)
		*code &= 0xffff;
	pal &= 0x0f;
	pal |= (pl45 & von) << 4;
	pal |= (K055555GetPaletteIndex(layer) << 6);
	*color = pal;
	(void)vmx;
}

static void gx_sprite_callback(INT32 *code, INT32 *color, INT32 *priority)
{
	static const INT32 coregmasks[5] = { 0x0f, 0x0e, 0x0c, 0x08, 0x00 };
	static const INT32 coregshifts[5] = { 4, 5, 6, 7, 8 };
	INT32 vrcbk[4];
	INT32 reg = K053247ReadRegs(0x08 / 2);
	INT32 attrib = *color;

	vrcbk[0] = (reg & 0x000f) << 14;
	vrcbk[1] = (reg & 0x0f00) << 6;
	reg = K053247ReadRegs(0x0a / 2);
	vrcbk[2] = (reg & 0x000f) << 14;
	vrcbk[3] = (reg & 0x0f00) << 6;
	*code = vrcbk[(*code >> 14) & 3] | (*code & 0x3fff);

	INT32 opset = K053247ReadRegs(0x0c / 2);
	INT32 coregindex = opset & 7;
	if (coregindex > 4)
		coregindex = 4;

	INT32 coregshift = coregshifts[coregindex];
	INT32 coreg = ((opset >> 8) & coregmasks[coregindex]) << 12;
	INT32 c18 = (attrib & 0xff) << coregshift | coreg;

	if (gx_control & 0x04) {
		c18 &= 0x3fff;
	} else if (!(gx_control & 0x08)) {
		c18 = (c18 & 0x3fff) | (attrib << 6 & 0xc000);
	}

	INT32 oinprion = K055555ReadRegister(K55_OINPRI_ON);
	INT32 opon = (oinprion << 8) | 0xff;
	INT32 ocb = (K055555ReadRegister(K55_PALBASE_OBJ) & 7) << 10;

	*color = ((c18 & opon) | (ocb & ~opon)) >> coregshift;

	// The real salmndr2 firmware emits standard depth bits (MiSTer ESC.md).
	*priority = (c18 >> 8 & ~oinprion) | (K055555ReadRegister(K55_PRIINP_8) & oinprion);
	// MiSTer uses the standard OPSET/OINPRI decode for Dragoon; MAME's
	// special callback incorrectly places stage highlights behind the backdrop.
	if (!gx_type4_enable && gx_special != GX_SPECIAL_DRAGOON)
		*color |= ((attrib >> 8) & 3) << K055555_MIXSHIFT;
}

static void gx_sound_update_irq()
{
	// The CPU worker owns its live Musashi context; sample jobs only request IRQ refresh.
	;

	INT32 line = 0;

	if (sound_timer_irq_pending) {
		line = 2;
	} else if (sound_int_enabled && sound_int_pending) {
		line = 1;
	}

	if (line == sound_irq_line) {
		return;
	}

	if (sound_irq_line) {
		SekSetIRQLine(1, sound_irq_line, CPU_IRQSTATUS_NONE);
	}

	sound_irq_line = line;

	if (sound_irq_line) {
		SekSetIRQLine(1, sound_irq_line, CPU_IRQSTATUS_ACK);
	}
}

static void sound_irq(INT32 state)
{
	if ((sound_control & 1) && !sound_irq_state && state)
		sound_timer_irq_pending = 1;
	sound_irq_state = state ? 1 : 0;
	gx_sound_update_irq();
}

static UINT8 gx_soundcom_host_read(INT32 offset)
{
	{
	}
	offset &= 7;

	switch (offset) {
		case 0:
		case 1:
			return DrvSoundCom[4 + offset];

		case 2:
			return 0;
	}

	return 0;
}

static void gx_soundcom_host_write(INT32 offset, UINT8 data)
{
	{
	}
	offset &= 7;

	if (offset < 4) {
		DrvSoundCom[offset] = data;
	} else if (offset == 7 && sound_int_enabled) {
		sound_int_pending = 1;
		gx_sound_update_irq();
	}
}

static UINT8 gx_soundcom_sound_read(INT32 offset)
{
	offset &= 7;
	return (offset < 4) ? DrvSoundCom[offset] : 0;
}

static void gx_soundcom_sound_write(INT32 offset, UINT8 data)
{
	offset &= 7;

	if (offset < 2) {
		DrvSoundCom[4 + offset] = data;
	} else if (offset == 4) {
		sound_int_enabled = data & 1;
		if (sound_int_enabled) {
			gx_sound_update_irq();
		} else {
			sound_int_pending = 0;
			gx_sound_update_irq();
		}
	}
}

static INT32 gx_ccu_vc()
{
	return ((((DrvCCURegs[0x10] << 8) | DrvCCURegs[0x12]) & 0x01ff) + 1);
}

static UINT32 gx_dma_read_long(UINT32 address)
{
	if (address <= 0x01fffc) {
		return BURN_ENDIAN_SWAP_INT32(*((UINT32 *)(DrvMainROM + address)));
	}

	if (address >= 0x200000 && address <= 0x7ffffc) {
		return BURN_ENDIAN_SWAP_INT32(*((UINT32 *)(DrvMainROM + address)));
	}

	if (address >= 0xc00000 && address <= 0xc1fffc) {
		return gx_mainram_read_long(address);
	}

	return gx_main_read_long(address);
}

static void gx_dma_write_long(UINT32 address, UINT32 data)
{
	if (address >= 0xc00000 && address <= 0xc1fffc) {
		gx_mainram_write_long(address, data);
		return;
	}

	gx_main_write_long(address, data);
}

static UINT32 gx_dma_register(INT32 offset)
{
	UINT8 *reg = fantjour_dma + (offset * 4);
	return (reg[0] << 24) | (reg[1] << 16) | (reg[2] << 8) | reg[3];
}

static void gx_dma_execute()
{
	UINT32 sa = gx_dma_register(1);
	UINT32 da = ((gx_dma_register(3) & 0xffff) << 16) | (gx_dma_register(4) >> 16);
	UINT32 db = gx_dma_register(5);
	UINT32 x = gx_dma_register(6);
	UINT8 sz2 = gx_dma_register(0) >> 16;
	UINT8 mode = gx_dma_register(0) >> 24;

	if (mode != 0x93 && mode != 0x8f)
		return;

	for (INT32 row = 0; row <= sz2; row++) {
		for (UINT32 i = 0; i < db; i += 4) {
			UINT32 data = (mode == 0x93) ? (gx_dma_read_long(sa) ^ x) : x;
			gx_dma_write_long(da, data);
			da += 4;
			if (mode == 0x93)
				sa += 4;
		}
	}
}

static UINT16 gx_address_read_word(UINT32 address)
{
	if (address >= 0xc00000 && address <= 0xc1fffe) {
		return gx_mainram_read_word(address);
	}

	if (address < 0x800000) {
		return (DrvMainROM[(address & 0x7fffff) ^ 1] << 8) | DrvMainROM[((address + 1) & 0x7fffff) ^ 1];
	}

	return gx_main_read_word(address);
}

static UINT32 gx_address_read_long(UINT32 address)
{
	if (address >= 0xc00000 && address <= 0xc1fffc) {
		return gx_mainram_read_long(address);
	}

	if (address < 0x800000) {
		return (gx_address_read_word(address + 0) << 16) | gx_address_read_word(address + 2);
	}

	return gx_main_read_long(address);
}

static UINT8 gx_address_read_byte(UINT32 address)
{
	if (address >= 0xc00000 && address <= 0xc1ffff) {
		return gx_mainram_read_byte(address);
	}

	if (address < 0x800000) {
		return DrvMainROM[(address & 0x7fffff) ^ 1];
	}

	return gx_main_read_byte(address);
}

static void gx_address_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0xc00000 && address <= 0xc1fffe) {
		gx_mainram_write_word(address, data);
		return;
	}

	gx_main_write_word(address, data);
}

static void gx_address_write_long(UINT32 address, UINT32 data)
{
	if (address >= 0xc00000 && address <= 0xc1fffc) {
		gx_mainram_write_long(address, data);
		return;
	}

	gx_main_write_long(address, data);
}

static void gx_address_write_byte(UINT32 address, UINT8 data)
{
	if (address >= 0xc00000 && address <= 0xc1ffff) {
		gx_mainram_write_byte(address, data);
		return;
	}

	gx_main_write_byte(address, data);
}

// 056734 host bus: RTL hx_* transfers aligned halfwords and odd bytes.
// FBNeo stores sprite words in native little-endian order on x64. The
// RTL host bus is big-endian, so translate each byte lane at this boundary.
static UINT32 gx_esc_bus_read(UINT32 a, unsigned n)
{
	UINT32 v = 0;
	for (unsigned i = 0; i < n; i++) {
		UINT32 p = (a + i) & 0xffffff;
		UINT8 b = ((p & 0xffc000) == 0xd20000) ? K053247Ram[(p & 0x3fff) ^ 1] : gx_address_read_byte(p);
		v = (v << 8) | b;
	}
	return v;
}

static void gx_esc_bus_write(UINT32 a, unsigned n, UINT32 v)
{
	for (unsigned i = 0; i < n; i++) {
		UINT32 p = (a + i) & 0xffffff;
		UINT8 b = (UINT8)(v >> (8 * (n - 1 - i)));
		if ((p & 0xffc000) == 0xd20000)
			K053247Ram[(p & 0x3fff) ^ 1] = b;
		else
			gx_address_write_byte(p, b);
	}
}

struct GxEscConfig {
	const char *name;
	UINT16 s10;
	UINT8 nib;
	UINT32 xorval;
	UINT64 lanes;
};

static const GxEscConfig gx_esc_configs[] = {
    {"daiskiss",   0x89ee, 0xd, 0x39556cc0, 0xb854d2e7f3a601c9ULL},
    {"crzcross",   0x5d32, 0x3, 0x91c2c6fb, 0xf10ed73a65249c8bULL},
    {"puzldama",   0x5d32, 0x3, 0x91c2c6fb, 0xf10ed73a65249c8bULL},
    {"fantjour",   0x8d8c, 0x3, 0x2c3ea1c4, 0x24ac58167039edbfULL},
    {"fantjoura",  0x8d8c, 0x3, 0x2c3ea1c4, 0x24ac58167039edbfULL},
    {"gokuparo"   ,0x8d8c, 0x3, 0x2c3ea1c4, 0x24ac58167039edbfULL},
    {"tbyahhoo",   0x0424 ,0xa, 0xde78b8ae, 0x97e60a851bd32c4fULL},
    {"mtwinbee",   0x0424, 0xa, 0xde78b8ae, 0x97e60a851bd32c4fULL},
    {"sexyparo",   0x896a, 0xe, 0x1886ae1d, 0xfedcba9876543210ULL},
    {"sexyparoa",  0x896a, 0xe, 0x1886ae1d, 0xfedcba9876543210ULL},
    {"tokkae",     0x9e8e, 0xb, 0x6bfbb5b9, 0x17fe43b8a50d962cULL},
    {"tkmmpzdm",   0x4924, 0xc, 0x88600ef4 ,0x4ca873fb602d15e9ULL},
    // MiSTer rtl/gx_board_cfg.sv: run Dragoon's uploaded 056734 program.
    {"dragoona",   0x5963, 0xb, 0x049db8e5, 0xa2cb1f05d68e3974ULL},
    {"dragoonj",   0x5963, 0xb, 0x049db8e5, 0xa2cb1f05d68e3974ULL},
    {"salmndr2",   0x1ec6, 0xa, 0xb3f135b3, 0xbd53c0196fea4287ULL},
    {"salmndr2a",  0x1ec6, 0xa, 0xb3f135b3, 0xbd53c0196fea4287ULL},
    // Korean Translation
    {"tbyahhook",  0x0424, 0xa, 0xde78b8ae, 0x97e60a851bd32c4fULL}
};

static INT32 gx_esc_reset()
{
	gx_esc_enabled = 0;
	const char *name = BurnDrvGetTextA(DRV_NAME);
	for (unsigned i = 0; i < sizeof(gx_esc_configs) / sizeof(gx_esc_configs[0]); i++) {
		const GxEscConfig &cfg = gx_esc_configs[i];
		if (strcmp(name, cfg.name))
			continue;
		gx_esc_enabled = gx_esc_chip.Reset(gx_esc_bus_read, gx_esc_bus_write, cfg.s10, cfg.nib, cfg.xorval, cfg.lanes) ? 1 : 0;
		gx_esc_chip.TakeIrq();
		if (!gx_esc_enabled) {
			bprintf(PRINT_ERROR, _T("GX 056734 kernel failed at PC %04x\n"), gx_esc_chip.state.pc);
			return 1;
		}
		break;
	}
	return 0;
}

static void gx_esc_write(UINT32 data)
{
	if (!gx_esc_enabled)
		return;
	if (!gx_esc_chip.Post(data)) {
		bprintf(PRINT_ERROR, _T("GX 056734 unsupported instruction/timeout at PC %04x\n"), gx_esc_chip.state.pc);
		return;
	}
	if (gx_esc_chip.TakeIrq() && (gx_irq_control & 0x10)) {
		gx_irq_status &= ~0x08;
		SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
	}
}

static void gx_type4_prot_write(UINT32 address, UINT32 data)
{
	INT32 offset = (address & 7) >> 2;

	if (offset == 1) {
		gx_last_prot_op = data >> 16;
		return;
	}

	if ((data & 0xff00) == 0) {
		gx_last_prot_param = data & 0xffff;
	}

	data >>= 16;
	INT32 clk = data & 0x200;

	if (clk == 0 && gx_last_prot_clk != 0 && gx_last_prot_op != -1) {
		if (gx_last_prot_op == 0x0a56 || gx_last_prot_op == 0x0d96 || gx_last_prot_op == 0x0d14 || gx_last_prot_op == 0x0d1c) {
			for (INT32 i = 0; i < 0x400; i += 2) {
				gx_address_write_word(0xc01400 + i, gx_address_read_word(0xc01000 + i));
			}
		} else if (gx_last_prot_op == 0x057a) {
			gx_address_write_long(0xc10f00, gx_address_read_long(0xc00f10));
			gx_address_write_long(0xc10f04, gx_address_read_long(0xc00f14));
			gx_address_write_long(0xc10f20, gx_address_read_long(0xc00f20));
			gx_address_write_long(0xc10f24, gx_address_read_long(0xc00f24));
			gx_address_write_long(0xc0fe00, gx_address_read_long(0xc00f30));
			gx_address_write_long(0xc0fe04, gx_address_read_long(0xc00f34));
		} else if (gx_last_prot_op == 0x0d97) {
			UINT32 src = 0xc09ff0;
			UINT32 dst = 0xd20000;

			if (gx_last_prot_param == 0x0062) {
				src = 0xc19ff0;
				dst = 0xd21000;
			}

			for (INT32 spr = 0; spr < 256; spr++) {
				for (INT32 i = 0; i <= 0x10; i += 4) {
					gx_address_write_long(dst + i, gx_address_read_long(src + i));
				}

				src -= 0x10;
				dst += 0x10;
			}

			gx_address_write_byte(0xc01cc0, ~gx_address_read_byte(0xc00507));
			gx_address_write_byte(0xc01cc1, ~gx_address_read_byte(0xc00527));
			gx_address_write_byte(0xc01cc4, ~gx_address_read_byte(0xc00547));
			gx_address_write_byte(0xc01cc5, ~gx_address_read_byte(0xc00567));
			gx_address_write_byte(0xc11cc0, ~gx_address_read_byte(0xc00507));
			gx_address_write_byte(0xc11cc1, ~gx_address_read_byte(0xc00527));
			gx_address_write_byte(0xc11cc4, ~gx_address_read_byte(0xc00547));
			gx_address_write_byte(0xc11cc5, ~gx_address_read_byte(0xc00567));
		} else if (gx_last_prot_op == 0x0b16) {
			UINT32 src = 0xc01000;
			UINT32 dst = 0xd20000;

			for (INT32 spr = 0; spr < 0x100; spr++) {
				gx_address_write_word(dst, gx_address_read_word(src));
				src += 4;
				dst += 2;
			}
		} else if (gx_last_prot_op == 0x3a4f) {
			UINT32 src = 0xc18400;
			UINT32 dst = 0xd21000;

			for (INT32 spr = 0; spr < 0x400; spr++) {
				gx_address_write_word(dst, gx_address_read_word(src));
				src += 4;
				dst += 2;
			}
		} else if (gx_last_prot_op == 0x0515) {
			for (INT32 adr = 0; adr < 0x400; adr += 2) {
				gx_address_write_word(0xc01c00 + adr, gx_address_read_word(0xc01800 + adr));
			}
		} else if (gx_last_prot_op == 0x115d) {
			for (INT32 adr = 0; adr < 0x400; adr += 2) {
				gx_address_write_word(0xc18c00 + adr, gx_address_read_word(0xc18800 + adr));
			}
		}

		if (gx_irq_control & 0x10) {
			gx_irq_status &= ~0x08;
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}

		gx_last_prot_op = -1;
	}

	gx_last_prot_clk = clk;
}

static void tms57002_stub_reset()
{
	tms_hidx = 0;
	tms_pc_nonzero = 0;
	tms_program_state = 0;
	tms_cload_cval = 0;
	tms_host_pending = 0;
	tms_host_index = 0;
	tms_cycle_frac = 0;
}

static UINT8 tms57002_stub_status()
{
	return (DrvTms.dready_r() ? 0x04 : 0x00) | (DrvTms.pc0_r() ? 0x02 : 0x00) | (DrvTms.empty_r() ? 0x01 : 0x00);
}

static void tms57002_stub_data_write(UINT8 data)
{
	if (!tms_reset) {
		tms57002_stub_reset();
		return;
	}

	DrvTms.data_w(data);

	const INT32 IN_PLOAD = 1;
	const INT32 IN_CLOAD = 2;
	const INT32 mode = (tms_pload ? 0 : IN_PLOAD) | (tms_cload ? 0 : IN_CLOAD);

	if (mode == 0) {
		tms_hidx = 0;
		tms_cload_cval = 0;
		return;
	}

	if (mode == IN_PLOAD) {
		tms_hidx++;
		if (tms_hidx >= 3) {
			tms_hidx = 0;
			if (tms_program_state < 2) {
				tms_program_state++;
			} else {
				tms_pc_nonzero = 1;
			}
		}
		return;
	}

	if (mode == IN_CLOAD) {
		if (tms_cload_cval) {
			tms_hidx++;
			if (tms_hidx >= 4) {
				tms_hidx = 1;
				tms_cload_cval = 0;
			}
		} else {
			tms_hidx = 0;
			tms_cload_cval = 1;
		}
		return;
	}

	if (mode == (IN_PLOAD | IN_CLOAD)) {
		tms_hidx = (tms_hidx + 1) & 3;
		return;
	}
}

static UINT8 __fastcall gx_sound_read_byte(UINT32 address)
{
	if (false && !false && ((address >= 0x200000 && address <= 0x2004ff) || address == 0x300001 || (address & 0xfffffe) == 0x500000)) {
		const INT32 cycles = SekTotalCycles(1);
		UINT8 result = 0;
		{
			gx_sample_cycles = cycles;
			result = gx_sound_read_byte(address);
			gx_sample_cycles = -1;
		}
		if (gx_sample_irq_refresh) {
			gx_sample_irq_refresh = false;
			gx_sound_update_irq();
		}
		return result;
	}

	if (address >= 0x200000 && address <= 0x2004ff) {
		INT32 offset = (address - 0x200000) >> 1;
		return (address & 1) ? K054539Read(1, offset) : K054539Read(0, offset);
	}

	if (address == 0x300001) {
		gx_sync_sound_stream();
		gx_dsp_force = GX_DSP_FORCE_WINDOW;
		if (tms_host_pending) {
			tms_host_index++;
			if (tms_host_index >= 4) {
				tms_host_index = 0;
				tms_host_pending = 0;
			}
		}
		return DrvTms.data_r();
	}

	if ((address & 0xffffe0) == 0x400000) {
		if (!(address & 1))
			return 0;
		return gx_soundcom_sound_read((address & 0x1f) >> 1);
	}

	if ((address & 0xfffffe) == 0x500000) {
		if (!(address & 1))
			return 0x00;
		// Host polling must observe DSP work up to the current sound CPU time.
		gx_sync_sound_stream();
		return tms57002_stub_status();
	}

	return 0;
}

static UINT16 __fastcall gx_sound_read_word(UINT32 address)
{
	if (false && !false &&
	    ((address >= 0x200000 && address <= 0x2004ff) || (address & 0xfffffe) == 0x300000 || (address & 0xfffffe) == 0x500000)) {
		const INT32 cycles = SekTotalCycles(1);
		UINT16 result = 0;
		{
			gx_sample_cycles = cycles;
			result = gx_sound_read_word(address);
			gx_sample_cycles = -1;
		}
		if (gx_sample_irq_refresh) {
			gx_sample_irq_refresh = false;
			gx_sound_update_irq();
		}
		return result;
	}

	if (address >= 0x200000 && address <= 0x2004fe && !(address & 1)) {
		const INT32 offset = (address - 0x200000) >> 1;
		// Read both chips, including data-port side effects, without re-decoding each lane.
		const UINT16 high = K054539Read(0, offset);
		return (high << 8) | K054539Read(1, offset);
	}
	return (gx_sound_read_byte(address) << 8) | gx_sound_read_byte(address + 1);
}

static void __fastcall gx_sound_write_byte(UINT32 address, UINT8 data)
{
	if (false && !false && ((address >= 0x200000 && address <= 0x2004ff) || address == 0x300001 || (address & 0xfffffe) == 0x500000)) {
		const INT32 cycles = SekTotalCycles(1);
		{
			gx_sample_cycles = cycles;
			gx_sound_write_byte(address, data);
			gx_sample_cycles = -1;
		}
		if (gx_sample_irq_refresh) {
			gx_sample_irq_refresh = false;
			gx_sound_update_irq();
		}
		return;
	}

	if (address >= 0x200000 && address <= 0x2004ff) {
		INT32 offset = (address - 0x200000) >> 1;
		gx_sync_sound_stream();
		if (address & 1)
			K054539Write(1, offset, data);
		else
			K054539Write(0, offset, data);
		return;
	}

	if (address == 0x300001) {
		gx_sync_sound_stream();
		tms57002_stub_data_write(data);
		return;
	}

	if ((address & 0xffffe0) == 0x400000) {
		if (address & 1)
			gx_soundcom_sound_write((address & 0x1f) >> 1, data);
		return;
	}

	if ((address & 0xfffffe) == 0x500000 && (address & 1)) {
		gx_sync_sound_stream();
		UINT8 old_pload = tms_pload;
		UINT8 old_cload = tms_cload;
		UINT8 old_reset = tms_reset;
		if (!(data & 0x01)) {
			sound_timer_irq_pending = 0;
		}
		tms_pload = (data & 0x04) ? 1 : 0;
		tms_cload = (data & 0x08) ? 1 : 0;
		tms_reset = (data & 0x10) ? 1 : 0;
		DrvTms.pload_w(data & 0x04);
		DrvTms.cload_w(data & 0x08);
		if (!tms_reset) {
			DrvTms.reset();
			tms57002_stub_reset();
		} else {
			if (!old_reset) {
				DrvTms.reset();
				tms57002_stub_reset();
			}
			if (!tms_pload && old_pload) {
				tms_hidx = 0;
				tms_pc_nonzero = 0;
				tms_program_state = 0;
			}
			if (!tms_cload && old_cload) {
				tms_hidx = 0;
				tms_cload_cval = 0;
			}
			if (data == 0xfc && tms_pc_nonzero) {
				gx_dsp_force = GX_DSP_FORCE_WINDOW;
				tms_host_pending = 1;
				tms_host_index = 0;
			}
		}
		sound_control = data;
		gx_sound_update_irq();
		return;
	}

	if ((address & 0xfffffe) == 0x580000) {
		// NRES: K056602 reset line. MAME maps this as nopw for GX.
		return;
	}
}

static void __fastcall gx_sound_write_word(UINT32 address, UINT16 data)
{
	if (false && !false &&
	    ((address >= 0x200000 && address <= 0x2004ff) || (address & 0xfffffe) == 0x300000 || (address & 0xfffffe) == 0x500000)) {
		const INT32 cycles = SekTotalCycles(1);
		{
			gx_sample_cycles = cycles;
			gx_sound_write_word(address, data);
			gx_sample_cycles = -1;
		}
		if (gx_sample_irq_refresh) {
			gx_sample_irq_refresh = false;
			gx_sound_update_irq();
		}
		return;
	}

	if (address >= 0x200000 && address <= 0x2004fe && !(address & 1) && address != 0x20044e && address != 0x20045e) {
		// Timer/control registers with IRQ callbacks retain the byte-handler path.
		// Both PCM byte lanes share one CPU timestamp; preserve chip write order.
		const INT32 offset = (address - 0x200000) >> 1;
		gx_sync_sound_stream();
		K054539Write(0, offset, data >> 8);
		K054539Write(1, offset, data & 0xff);
		return;
	}
	gx_sound_write_byte(address + 0, data >> 8);
	gx_sound_write_byte(address + 1, data);
}

static void gx_control_write(UINT8 data)
{
	{
	}
	gx_control = data;
	K053246_set_OBJCHA_line((data & 0x10) ? 1 : 0);

	if (data & 0x40) {
		sound_host_status = 0xc0;
		sound_host_status_wait = 0;
		SekSetHALT(1, 0);
		SekSetRESETLine(1, 0);
	} else {
		sound_control = 0;
		sound_irq_state = 0;
		sound_timer_irq_pending = 0;
		sound_int_enabled = 0;
		sound_int_pending = 0;
		tms_pload = 1;
		tms_cload = 1;
		tms_reset = 0;
		DrvTms.reset();
		tms57002_stub_reset();
		if (sound_irq_line)
			SekSetIRQLine(1, sound_irq_line, CPU_IRQSTATUS_NONE);
		sound_irq_line = 0;
		SekSetHALT(1, 1);
		SekSetRESETLine(1, 1);
	}
}

static UINT16 gx_sprite_rom_word_read(INT32 offset)
{
	UINT32 romofs = (K053246ReadRegs(6) << 16) | (K053246ReadRegs(7) << 8) | K053246ReadRegs(4);
	UINT16 *rom = (UINT16 *)DrvGfxROM1;

	if (gx_sprite_bpp != 5) {
		romofs = (romofs >> 2) * gx_sprite_bpp;
		if ((offset & 4) == 0)
			romofs += gx_sprite_bpp >> 1;

		UINT32 index = (romofs + (offset & 3)) % (gx_sprite_word_size / 2);
		if (gx_sprite_bpp == 4 || gx_sprite_bpp == 8)
			return DrvGfxROM1[index * 2] | (DrvGfxROM1[index * 2 + 1] << 8);
		return BURN_ENDIAN_SWAP_INT16(rom[index]);
	}

	UINT8 *fifth = DrvGfxROM1 + gx_sprite_word_size;

	switch (offset & 7) {
		case 0:
			return BURN_ENDIAN_SWAP_INT16(rom[romofs + 2]);
		case 1:
			return BURN_ENDIAN_SWAP_INT16(rom[romofs + 3]);
		case 2:
		case 3:
			return fifth[(romofs >> 1) + 1];
		case 4:
			return BURN_ENDIAN_SWAP_INT16(rom[romofs + 0]);
		case 5:
			return BURN_ENDIAN_SWAP_INT16(rom[romofs + 1]);
		case 6:
		case 7:
			return fifth[romofs >> 1];
	}

	return 0;
}

static UINT8 gx_tile_rom_byte_read(INT32 address)
{
	if (gx_special == GX_SPECIAL_SALMNDR2) {
		return K056832GxRomByteRead6bpp(address);
	}

	return K056832GxRomByteRead(address);
}

static UINT16 gx_tile_rom_word_read(INT32 address)
{
	return (gx_tile_rom_byte_read((address + 0) & 0x1fff) << 8) | gx_tile_rom_byte_read((address + 1) & 0x1fff);
}

static UINT32 gx_tile_rom_long_read(INT32 address)
{
	return (gx_tile_rom_word_read(address + 0) << 16) | gx_tile_rom_word_read(address + 2);
}

static UINT8 __fastcall gx_main_read_byte(UINT32 address)
{
	if (gx_special == GX_SPECIAL_LE2 && (address & 0xfffff8) == 0xd44000) {
		INT32 player = (address & 2) >> 1;
		UINT16 pos;
		if (address & 4) {
			pos = (gx_le2_flip_y ? 255 - BurnGunReturnY(player) : BurnGunReturnY(player)) * 224 / 255;
			if (GxGunReload[player] || pos >= 223)
				pos = 0; // MiSTer gx_guns: off-screen reload.
		} else
			pos = BurnGunReturnX(player) * 290 / 255 + 20;
		return (address & 1) ? pos : pos >> 8;
	}

	// Rendering owns the device state until its published job completes.
	;
	if (address >= 0xc00000 && address <= 0xc1ffff) {
		return gx_mainram_read_byte(address);
	}

	if ((address & 0xffe000) == 0xd00000) {
		return gx_tile_rom_byte_read(address & 0x1fff);
	}

	if ((address & 0xffc000) == 0xd20000) {
		return K053247Read((address & 0x3fff) ^ 1);
	}

	if ((address & 0xfffff0) == 0xd4a000) {
		UINT16 data = gx_sprite_rom_word_read((address & 0x0f) >> 1);
		return data >> ((~address & 1) * 8);
	}

	if ((address & 0xffffe0) == 0xd4c000) {
		if ((address & 0x1f) == 0x1c)
			return ((gx_current_scanline - gx_ccu_vc()) >> 8) & 1;
		if ((address & 0x1f) == 0x1e)
			return (gx_current_scanline - gx_ccu_vc()) & 0xff;
		return DrvCCURegs[address & 0x1f];
	}

	if ((address & 0xffffe0) == 0xd52000) {
		if (address & 1)
			return 0;
		return gx_soundcom_host_read((address & 0x0f) >> 1);
	}

	switch (address) {
		case 0xd5a000:
			return gx_system_dsw0_read();
		case 0xd5a001:
			return DrvDips[1];
		case 0xd5a002:
			// MiSTer Type 2 SYSTEM_DSW bit 7 is active high (idle low).
			return DrvInputs[0] & ((gx_special >= GX_SPECIAL_DRAGOON && !gx_type4_enable) ? 0x7f : 0xff);
		case 0xd5a003: {
			// MAME ioport applies PORT_BIT(0xfe)'s shift after its callback.
			UINT8 data = gx_special >= GX_SPECIAL_DRAGOON ? gx_irq_status : ((gx_irq_status >> 1) & 0x7f);
			data = (data & 0xfe) | (EEPROMRead() ? 0x01 : 0);
			return data;
		}

		case 0xd5c000:
			return DrvInputs[1] & 0xff;
		case 0xd5c001:
			return DrvInputs[1] >> 8;
		case 0xd5c002:
			return gx_special >= GX_SPECIAL_DRAGOON ? DrvInputs[2] >> 8 : 0xff;
		case 0xd5c003:
			return gx_special >= GX_SPECIAL_DRAGOON ? DrvInputs[2] & 0xff : 0xff;

		case 0xd5e000:
			return ((DrvJoy1[7] & 1) ? 0xf7 : 0xff) & ((gx_special == GX_SPECIAL_LE2 && (DrvJoy2[4] || GxGunReload[0])) ? 0xfb : 0xff);
		case 0xd5e001:
			return 0xff;
		case 0xd5e002:
			return (gx_special == GX_SPECIAL_LE2 && (DrvJoy2[12] || GxGunReload[1])) ? 0xfb : 0xff;
		case 0xd5e003:
			return 0xff;
	}

	if ((address & 0xff8000) == 0xd90000) {
		if (gx_type4_enable)
			return gx_ram_read_byte(DrvType4RAM, address, 0x7fff);
		return DrvPalRAM[(address & 0x7fff) ^ 3];
	}

	if ((address & 0xffc000) == 0xda0000) {
		return K056832RamReadByte(address & 0x1fff);
	}

	if (gx_type4_enable) {
		if ((address & 0xffffe0) == 0xe00000)
			return gx_ram_read_byte(DrvType4CtrlRAM, address, 0x1f);
		if ((address & 0xfffff0) == 0xe40000)
			return 0;
		if ((address & 0xfffff000) == 0xe60000)
			return gx_ram_read_byte(DrvType4LineRAM, address, 0xfff);
		if ((address & 0xff8000) == 0xe80000)
			return gx_ram_read_byte(DrvPalRAM, address, 0x7fff);
		if ((address & 0xff8000) == 0xea0000)
			return gx_ram_read_byte(DrvSubPalRAM, address, 0x7fff);
		if ((address & 0xfffffc) == 0xec0000)
			return (gx_type4_current_frame == 0) ? 0xff : 0x00;
		if ((address & 0xff8000) == 0xf00000)
			return gx_ram_read_byte(DrvType4PsacRAM, address, 0x7fff);
	}

	return 0;
}

static UINT16 __fastcall gx_main_read_word(UINT32 address)
{
	// Rendering owns the device state until its published job completes.
	;
	if (address >= 0xc00000 && address <= 0xc1fffe) {
		return gx_mainram_read_word(address);
	}

	if ((address & 0xffe000) == 0xd00000) {
		return gx_tile_rom_word_read(address & 0x1fff);
	}

	if ((address & 0xffc000) == 0xd20000) {
		return (K053247Read((address & 0x3ffe) | 1) << 8) | K053247Read(address & 0x3ffe);
	}

	if ((address & 0xfffff0) == 0xd4a000) {
		return gx_sprite_rom_word_read((address & 0x0f) >> 1);
	}

	if ((address & 0xff8000) == 0xd90000) {
		if (gx_type4_enable)
			return gx_ram_read_word(DrvType4RAM, address, 0x7fff);
		return (DrvPalRAM[((address + 0) & 0x7fff) ^ 3] << 8) | DrvPalRAM[((address + 1) & 0x7fff) ^ 3];
	}

	if ((address & 0xffc000) == 0xda0000) {
		return K056832RamReadWord(address & 0x1fff);
	}

	if (gx_type4_enable) {
		if ((address & 0xffffe0) == 0xe00000)
			return gx_ram_read_word(DrvType4CtrlRAM, address, 0x1f);
		if ((address & 0xfffff0) == 0xe40000)
			return 0;
		if ((address & 0xfffff000) == 0xe60000)
			return gx_ram_read_word(DrvType4LineRAM, address, 0xfff);
		if ((address & 0xff8000) == 0xe80000)
			return gx_ram_read_word(DrvPalRAM, address, 0x7fff);
		if ((address & 0xff8000) == 0xea0000)
			return gx_ram_read_word(DrvSubPalRAM, address, 0x7fff);
		if ((address & 0xfffffc) == 0xec0000)
			return (gx_type4_current_frame == 0) ? 0xffff : 0x0000;
		if ((address & 0xff8000) == 0xf00000)
			return gx_ram_read_word(DrvType4PsacRAM, address, 0x7fff);
	}

	return (gx_main_read_byte(address) << 8) | gx_main_read_byte(address + 1);
}

static UINT32 __fastcall gx_main_read_long(UINT32 address)
{
	// Rendering owns the device state until its published job completes.
	;
	if (address >= 0xc00000 && address <= 0xc1fffc) {
		return gx_mainram_read_long(address);
	}

	if ((address & 0xffe000) == 0xd00000) {
		return gx_tile_rom_long_read(address & 0x1fff);
	}

	if ((address & 0xffc000) == 0xda0000) {
		return (gx_main_read_byte(address + 0) << 24) | (gx_main_read_byte(address + 1) << 16) | (gx_main_read_byte(address + 2) << 8) |
		       gx_main_read_byte(address + 3);
	}

	if (gx_type4_enable) {
		if ((address & 0xffffe0) == 0xe00000)
			return gx_ram_read_long(DrvType4CtrlRAM, address, 0x1f);
		if ((address & 0xfffff0) == 0xe40000)
			return 0;
		if ((address & 0xfffff000) == 0xe60000)
			return gx_ram_read_long(DrvType4LineRAM, address, 0xfff);
		if ((address & 0xff8000) == 0xe80000)
			return gx_ram_read_long(DrvPalRAM, address, 0x7fff);
		if ((address & 0xff8000) == 0xea0000)
			return gx_ram_read_long(DrvSubPalRAM, address, 0x7fff);
		if ((address & 0xfffffc) == 0xec0000)
			return (gx_type4_current_frame == 0) ? 0xffffffff : 0x00000000;
		if ((address & 0xff8000) == 0xf00000)
			return gx_ram_read_long(DrvType4PsacRAM, address, 0x7fff);
	}

	return (gx_main_read_word(address) << 16) | gx_main_read_word(address + 2);
}

static void __fastcall gx_main_write_byte(UINT32 address, UINT8 data)
{
	// Rendering owns the device state until its published job completes.
	;
	if (address >= 0xc00000 && address <= 0xc1ffff) {
		gx_mainram_write_byte(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0xcc0000) {
		if (gx_type4_enable || gx_special == GX_SPECIAL_WINSPIKE) {
			gx_type4_prot_write(address, data << (((~address) & 3) * 8));
		} else {
			UINT32 shift = ((~address) & 3) * 8;
			gx_esc_latch = (gx_esc_latch & ~(0xff << shift)) | (data << shift);
			if ((address & 3) == 3)
				gx_esc_write(gx_esc_latch);
		}
		return;
	}

	if ((address & 0xffffe0) == 0xdb0000) {
		if (!gx_fantjour_dma_enable)
			return;
		fantjour_dma[address & 0x1f] = data;
		if ((address & 0x1f) == 0) {
			gx_dma_execute();
		}
		return;
	}

	if ((address & 0xffc000) == 0xd20000) {
		K053247Write((address & 0x3fff) ^ 1, data);
		return;
	}

	if ((address & 0xffffc0) == 0xd40000) {
		if ((address & 0x3f) == 0x33)
			gx_k056832_set_ram_bank(data);
		K056832ByteWrite(address & 0x3f, data);
		return;
	}

	if ((address & 0xfffff0) == 0xd44000) {
		gx_tilebanks[address & 7] = data;
		return;
	}

	if ((address & 0xfffff8) == 0xd48000) {
		K053246Write(address & 7, data);
		return;
	}

	if ((address & 0xfffff0) == 0xd4a010) {
		K053247WriteRegsByte(address & 0x0f, data);
		return;
	}

	if ((address & 0xffffe0) == 0xd4c000) {
		DrvCCURegs[address & 0x1f] = data;
		if ((address & 0x1f) == 0x1c) {
			SekSetIRQLine(1, CPU_IRQSTATUS_NONE);
			gx_syncen |= 0x20;
		}
		if ((address & 0x1f) == 0x1e) {
			SekSetIRQLine(2, CPU_IRQSTATUS_NONE);
			gx_syncen |= 0x40;
		}
		return;
	}

	if ((address & 0xffffff00) == 0xd50000) {
		K055555ByteWrite(address & 0xff, data);
		return;
	}

	if ((address & 0xffffe0) == 0xd52000) {
		if (!(address & 1)) {
			gx_soundcom_host_write((address & 0x0f) >> 1, data);
		}
		return;
	}

	if ((address & 0xfffffc) == 0xd56000) {
		if ((address & 3) == 0) {
			gx_objscan = data;
			konamigx_set_wrport1_0(data);
			EEPROMWrite(data & 0x04, data & 0x02, data & 0x01);
		} else if ((address & 3) == 1) {
			gx_irq_control = data;
			// rtl/gx_main.sv: do not re-arm from inside an unacknowledged IRQ.
			if (data & 0x80) {
				gx_syncen |= data & 0x1c;
				if (gx_syncen & 0x20)
					gx_syncen |= data & 1;
				if (gx_syncen & 0x40)
					gx_syncen |= data & 2;
			}
		}
		return;
	}

	if ((address & 0xfffffc) == 0xd58000) {
		if ((address & 3) == 1)
			gx_control_write(data);
		return;
	}

	if ((address & 0xffffe0) == 0xd80000) {
		K054338WriteByte(address & 0x1f, data);
		return;
	}

	if ((address & 0xff8000) == 0xd90000) {
		if (gx_type4_enable) {
			gx_ram_write_byte(DrvType4RAM, address, 0x7fff, data);
			return;
		}
		gx_palette_write_byte(address, data);
		return;
	}

	if ((address & 0xffc000) == 0xda0000) {
		K056832RamWriteByte(address & 0x1fff, data);
		DrvTileRAM[(gx_tile_ram_page << 13) | (address & 0x1fff)] = K056832RamReadByte(address & 0x1fff);
		return;
	}

	if (gx_type4_enable) {
		if ((address & 0xffffe0) == 0xe00000) {
			gx_type4_write_k053936(address, data, 1, 0);
			return;
		}
		if ((address & 0xfffff0) == 0xe40000) {
			gx_type3_bank_write(address, data);
			return;
		}
		if ((address & 0xfffff000) == 0xe60000) {
			gx_type4_write_k053936(address, data, 1, 1);
			return;
		}
		if ((address & 0xff8000) == 0xe80000) {
			gx_type4_write_palette(0, address, data, 1);
			return;
		}
		if ((address & 0xff8000) == 0xea0000) {
			gx_type4_write_palette(1, address, data, 1);
			return;
		}
		if ((address & 0xff8000) == 0xf00000) {
			gx_type4_write_psac(address, data, 1);
			return;
		}
	}
}

static void __fastcall gx_main_write_word(UINT32 address, UINT16 data)
{
	// Rendering owns the device state until its published job completes.
	;
	if (address >= 0xc00000 && address <= 0xc1fffe) {
		gx_mainram_write_word(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0xcc0000) {
		if (gx_type4_enable || gx_special == GX_SPECIAL_WINSPIKE) {
			gx_type4_prot_write(address, data << ((address & 2) ? 0 : 16));
		} else {
			UINT32 shift = (address & 2) ? 0 : 16;
			gx_esc_latch = (gx_esc_latch & ~(0xffff << shift)) | (data << shift);
			if (address & 2)
				gx_esc_write(gx_esc_latch);
		}
		return;
	}

	if ((address & 0xffffe0) == 0xdb0000) {
		if (!gx_fantjour_dma_enable)
			return;
		fantjour_dma[(address + 0) & 0x1f] = data >> 8;
		fantjour_dma[(address + 1) & 0x1f] = data;
		if ((address & 0x1f) == 0) {
			gx_dma_execute();
		}
		return;
	}

	if ((address & 0xffc000) == 0xd20000) {
		K053247WriteWord(address & 0x3fff, data);
		return;
	}

	if ((address & 0xffffc0) == 0xd40000) {
		// The 68020 permits unaligned peripheral writes. In particular,
		// vsnetscr restores the tile RAM bank with MOVE.W to $d40033:
		// the high byte belongs to register $33, not register $32.
		if (address & 1) {
			gx_main_write_byte(address, data >> 8);
			gx_main_write_byte(address + 1, data);
			return;
		}
		if ((address & 0x3e) == 0x32)
			gx_k056832_set_ram_bank(data);
		K056832WordWrite(address & 0x3e, data);
		return;
	}

	if ((address & 0xfffff0) == 0xd44000) {
		gx_tilebanks[(address + 0) & 7] = data >> 8;
		gx_tilebanks[(address + 1) & 7] = data;
		return;
	}

	if ((address & 0xfffff8) == 0xd48000) {
		K053246Write((address + 0) & 7, data >> 8);
		K053246Write((address + 1) & 7, data);
		return;
	}

	if ((address & 0xfffff0) == 0xd4a010) {
		K053247WriteRegsWord(address & 0x0f, data);
		return;
	}

	if ((address & 0xffffff00) == 0xd50000) {
		K055555WordWrite(address & 0xff, data >> 8);
		return;
	}

	if ((address & 0xffffe0) == 0xd80000) {
		K054338WriteWord(address & 0x1f, data);
		return;
	}

	if ((address & 0xff8000) == 0xd90000) {
		if (gx_type4_enable) {
			gx_ram_write_word(DrvType4RAM, address, 0x7fff, data);
			return;
		}
		gx_palette_write_byte(address + 0, data >> 8);
		gx_palette_write_byte(address + 1, data);
		return;
	}

	if ((address & 0xffc000) == 0xda0000) {
		UINT32 offset = (gx_tile_ram_page << 13) | (address & 0x1fff);
		if (address & 1) {
			K056832RamWriteByte(address & 0x1fff, data >> 8);
			K056832RamWriteByte((address + 1) & 0x1fff, data);
			DrvTileRAM[offset] = K056832RamReadByte(address & 0x1fff);
			DrvTileRAM[(offset & ~0x1fff) | ((offset + 1) & 0x1fff)] = K056832RamReadByte((address + 1) & 0x1fff);
			return;
		}
		K056832RamWriteWord(address & 0x1fff, data);
		DrvTileRAM[offset] = K056832RamReadByte(address & 0x1fff);
		DrvTileRAM[(offset & ~0x1fff) | ((offset + 1) & 0x1fff)] = K056832RamReadByte((address + 1) & 0x1fff);
		return;
	}

	if (gx_type4_enable) {
		if ((address & 0xffffe0) == 0xe00000) {
			gx_type4_write_k053936(address, data, 2, 0);
			return;
		}
		if ((address & 0xfffff0) == 0xe40000) {
			gx_type3_bank_write(address + 0, data >> 8);
			gx_type3_bank_write(address + 1, data);
			return;
		}
		if ((address & 0xfffff000) == 0xe60000) {
			gx_type4_write_k053936(address, data, 2, 1);
			return;
		}
		if ((address & 0xff8000) == 0xe80000) {
			gx_type4_write_palette(0, address, data, 2);
			return;
		}
		if ((address & 0xff8000) == 0xea0000) {
			gx_type4_write_palette(1, address, data, 2);
			return;
		}
		if ((address & 0xff8000) == 0xf00000) {
			gx_type4_write_psac(address, data, 2);
			return;
		}
	}

	gx_main_write_byte(address + 0, data >> 8);
	gx_main_write_byte(address + 1, data);
}

static void __fastcall gx_main_write_long(UINT32 address, UINT32 data)
{
	// Rendering owns the device state until its published job completes.
	;
	if (address >= 0xc00000 && address <= 0xc1fffc) {
		gx_mainram_write_long(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0xcc0000) {
		if (gx_type4_enable || gx_special == GX_SPECIAL_WINSPIKE) {
			gx_type4_prot_write(address, data);
		} else {
			gx_esc_latch = data;
			gx_esc_write(data);
		}
		return;
	}

	if ((address & 0xffffe0) == 0xdb0000) {
		if (!gx_fantjour_dma_enable)
			return;
		fantjour_dma[(address + 0) & 0x1f] = data >> 24;
		fantjour_dma[(address + 1) & 0x1f] = data >> 16;
		fantjour_dma[(address + 2) & 0x1f] = data >> 8;
		fantjour_dma[(address + 3) & 0x1f] = data;
		if ((address & 0x1f) == 0) {
			gx_dma_execute();
		}
		return;
	}

	if ((address & 0xffc000) == 0xd20000) {
		K053247WriteWord((address + 0) & 0x3fff, data >> 16);
		K053247WriteWord((address + 2) & 0x3fff, data);
		return;
	}

	if ((address & 0xffffc0) == 0xd40000) {
		// Preserve byte lanes when a longword straddles register words.
		if (address & 1) {
			for (INT32 i = 0; i < 4; i++)
				gx_main_write_byte(address + i, data >> (24 - i * 8));
			return;
		}
		UINT16 hi = data >> 16;
		UINT16 lo = data;

		if ((address & 0x3e) == 0x32)
			gx_k056832_set_ram_bank(hi);
		K056832WordWrite(address & 0x3e, hi);

		if (((address + 2) & 0x3e) == 0x32)
			gx_k056832_set_ram_bank(lo);
		K056832WordWrite((address + 2) & 0x3e, lo);
		return;
	}

	if ((address & 0xfffff0) == 0xd44000) {
		gx_tilebanks[(address + 0) & 7] = data >> 24;
		gx_tilebanks[(address + 1) & 7] = data >> 16;
		gx_tilebanks[(address + 2) & 7] = data >> 8;
		gx_tilebanks[(address + 3) & 7] = data;
		return;
	}

	if ((address & 0xfffff8) == 0xd48000) {
		K053246Write((address + 0) & 7, data >> 24);
		K053246Write((address + 1) & 7, data >> 16);
		K053246Write((address + 2) & 7, data >> 8);
		K053246Write((address + 3) & 7, data);
		return;
	}

	if ((address & 0xffffff00) == 0xd50000) {
		K055555LongWrite(address & 0xff, data);
		return;
	}

	if ((address & 0xffc000) == 0xda0000 && (address & 1)) {
		UINT32 offset = (gx_tile_ram_page << 13) | (address & 0x1fff);
		UINT32 page = offset & ~0x1fff;
		UINT32 offs = offset & 0x1fff;
		UINT8 bytes[4] = { (UINT8)(data >> 24), (UINT8)(data >> 16), (UINT8)(data >> 8), (UINT8)data };

		for (INT32 i = 0; i < 4; i++) {
			UINT32 byte_offset = (offs + i) & 0x1fff;
			K056832RamWriteByte((address + i) & 0x1fff, bytes[i]);
			DrvTileRAM[page | byte_offset] = K056832RamReadByte((address + i) & 0x1fff);
		}

		return;
	}

	if (gx_type4_enable) {
		if ((address & 0xffffe0) == 0xe00000) {
			gx_type4_write_k053936(address, data, 4, 0);
			return;
		}
		if ((address & 0xfffff0) == 0xe40000) {
			gx_type3_bank_write(address + 0, data >> 24);
			gx_type3_bank_write(address + 1, data >> 16);
			gx_type3_bank_write(address + 2, data >> 8);
			gx_type3_bank_write(address + 3, data);
			return;
		}
		if ((address & 0xfffff000) == 0xe60000) {
			gx_type4_write_k053936(address, data, 4, 1);
			return;
		}
		if ((address & 0xff8000) == 0xe80000) {
			gx_type4_write_palette(0, address, data, 4);
			return;
		}
		if ((address & 0xff8000) == 0xea0000) {
			gx_type4_write_palette(1, address, data, 4);
			return;
		}
		if ((address & 0xff8000) == 0xf00000) {
			gx_type4_write_psac(address, data, 4);
			return;
		}
	}

	gx_main_write_word(address + 0, data >> 16);
	gx_main_write_word(address + 2, data);
}

static INT32 MemIndex()
{
	UINT8 *Next = AllMem;
	UINT32 sprite_raw_size = gx_sprite_word_size;
	UINT32 sprite_exp_size;

	if (gx_sprite_bpp != 5) {
		UINT32 sprite_decode_size = gx_sprite_word_size;
		sprite_exp_size = (sprite_decode_size / (32 * gx_sprite_bpp)) * 0x100;
	} else {
		sprite_exp_size = (gx_sprite_word_size / 0x80) * 0x100;
		if (!gx_type4_enable) {
			sprite_raw_size += gx_sprite_word_size >> 2;
		}
	}

	if (sprite_raw_size < 0x1000000)
		sprite_raw_size = 0x1000000;
	if (sprite_exp_size < 0x1000000)
		sprite_exp_size = 0x1000000;

	DrvMainROM = Next;
	Next += 0x800000;
	DrvSoundROM = Next;
	Next += 0x040000;
	DrvGfxROM0 = Next;
	Next += 0x1000000;
	DrvGfxROM1 = Next;
	Next += sprite_raw_size;
	DrvGfxROM2 = Next;
	Next += gx_type4_enable ? 0x200000 : 0;
	DrvPsacMapROM = Next;
	Next += gx_special == GX_SPECIAL_SOCCERSS ? 0x80000 : 0;
	DrvType3RozBitmap = (UINT16 *)Next;
	Next += gx_special == GX_SPECIAL_SOCCERSS ? (576 + GX_TYPE4_FLIP_X_OFFSET) * (224 + GX_TYPE4_FLIP_Y_OFFSET) * sizeof(UINT16) : 0;
	DrvGfxROMExp0 = Next;
	Next += 0x1000000;
	DrvGfxROMExp1 = Next;
	Next += sprite_exp_size;
	DrvSndROM = Next;
	Next += 0x400000;
	DrvEEPROM = Next;
	Next += 0x000080;
	DrvPalette = (UINT32 *)Next;
	Next += 0x2000 * sizeof(UINT32);

	AllRam = Next;
	DrvMainRAM = Next;
	Next += 0x020000;
	DrvSoundRAM = Next;
	Next += 0x010000;
	DrvPalRAM = Next;
	Next += 0x008000;
	DrvSubPalRAM = Next;
	Next += 0x008000;
	DrvTileRAM = Next;
	Next += 0x020000;
	DrvType4RAM = Next;
	Next += 0x008000;
	DrvType4CtrlRAM = Next;
	Next += 0x000020;
	DrvType4LineRAM = Next;
	Next += 0x001000;
	DrvType4PsacRAM = Next;
	Next += 0x008000;
	DrvSoundCom = Next;
	Next += 0x000020;
	DrvCCURegs = Next;
	Next += 0x000020;
	DrvTmsRAM = Next;
	Next += 0x040000;
	RamEnd = Next;
	MemEnd = Next;

	return 0;
}

static INT32 DecodeTiles()
{
	INT32 XOffs[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	INT32 YOffs5[8] = { 0, 40, 80, 120, 160, 200, 240, 280 };
	INT32 YOffs6[8] = { 0, 48, 96, 144, 192, 240, 288, 336 };

	if (gx_tile_bpp == 8) {
		INT32 Plane[8] = { 56, 24, 40, 8, 48, 16, 32, 0 };
		INT32 YOffs8[8] = { 0, 64, 128, 192, 256, 320, 384, 448 };
		GfxDecode(gx_tile_rom_size / 64, 8, 8, 8, Plane, XOffs, YOffs8, 512, DrvGfxROM0, DrvGfxROMExp0);
	} else if (gx_tile_bpp == 6) {
		INT32 Plane[6] = { 40, 32, 24, 8, 16, 0 };
		GfxDecode(gx_tile_rom_size / 48, 6, 8, 8, Plane, XOffs, YOffs6, 384, DrvGfxROM0, DrvGfxROMExp0);
	} else {
		INT32 Plane[5] = { 32, 24, 8, 16, 0 };
		GfxDecode(gx_tile_rom_size / 40, 5, 8, 8, Plane, XOffs, YOffs5, 320, DrvGfxROM0, DrvGfxROMExp0);
	}

	return 0;
}

static INT32 DecodeSprites()
{
	if (gx_sprite_bpp == 4 || gx_sprite_bpp == 8) {
		INT32 planes[8], x[16], y[16];
		for (INT32 i = 0; i < gx_sprite_bpp; i++)
			planes[i] = (gx_sprite_bpp - 1 - i) * 8;
		for (INT32 i = 0; i < 16; i++) {
			x[i] = (i & 7) + (i / 8) * gx_sprite_bpp * 8;
			y[i] = i * gx_sprite_bpp * 16;
		}
		GfxDecode(gx_sprite_word_size / (32 * gx_sprite_bpp), gx_sprite_bpp, 16, 16, planes, x, y, 256 * gx_sprite_bpp, DrvGfxROM1,
		          DrvGfxROMExp1);
		return 0;
	}

	INT32 XOffs[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 40, 41, 42, 43, 44, 45, 46, 47 };
	INT32 YOffs[16] = { 0, 80, 160, 240, 320, 400, 480, 560, 640, 720, 800, 880, 960, 1040, 1120, 1200 };

	if (gx_sprite_bpp == 6) {
		INT32 Plane[6] = { 40, 32, 24, 16, 8, 0 };
		INT32 XOffs6[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 48, 49, 50, 51, 52, 53, 54, 55 };
		INT32 YOffs6[16] = { 0, 96, 192, 288, 384, 480, 576, 672, 768, 864, 960, 1056, 1152, 1248, 1344, 1440 };
		UINT32 sprite_decode_size = gx_sprite_word_size;

		GfxDecode(sprite_decode_size / 0xc0, 6, 16, 16, Plane, XOffs6, YOffs6, 1536, DrvGfxROM1, DrvGfxROMExp1);

		return 0;
	}

	INT32 Plane[5] = { 32, 24, 16, 8, 0 };
	UINT32 fifth_size = gx_sprite_word_size >> 2;
	UINT8 *tmp = (UINT8 *)BurnMalloc(gx_sprite_word_size + fifth_size);
	UINT8 *dst = tmp;

	if (tmp == NULL)
		return 1;

	for (UINT32 i = 0; i < gx_sprite_word_size; i += 4) {
		*dst++ = DrvGfxROM1[i + 0];
		*dst++ = DrvGfxROM1[i + 1];
		*dst++ = DrvGfxROM1[i + 2];
		*dst++ = DrvGfxROM1[i + 3];
		*dst++ = DrvGfxROM1[gx_sprite_word_size + (i >> 2)];
	}

	GfxDecode(gx_sprite_word_size / 0x80, 5, 16, 16, Plane, XOffs, YOffs, 1280, tmp, DrvGfxROMExp1);
	BurnFree(tmp);

	return 0;
}

static INT32 DecodePsacTiles()
{
	INT32 Plane[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	INT32 XOffs[16] = { 0 * 128, 1 * 128, 2 * 128,  3 * 128,  4 * 128,  5 * 128,  6 * 128,  7 * 128,
	                    8 * 128, 9 * 128, 10 * 128, 11 * 128, 12 * 128, 13 * 128, 14 * 128, 15 * 128 };
	INT32 YOffs[16] = {
		0 * 8, 1 * 8, 2 * 8, 3 * 8, 4 * 8, 5 * 8, 6 * 8, 7 * 8, 8 * 8, 9 * 8, 10 * 8, 11 * 8, 12 * 8, 13 * 8, 14 * 8, 15 * 8
	};
	UINT8 *tmp = (UINT8 *)BurnMalloc(0x200000);

	if (tmp == NULL)
		return 1;

	memcpy(tmp, DrvGfxROM2, 0x200000);
	GfxDecode(0x2000, 8, 16, 16, Plane, XOffs, YOffs, 16 * 128, tmp, DrvGfxROM2);
	BurnFree(tmp);

	return 0;
}

static INT32 DecodeGraphics()
{
	if (DecodeTiles())
		return 1;
	if (DecodeSprites())
		return 1;
	if (gx_type4_enable && DecodePsacTiles())
		return 1;
	return 0;
}

static void gx_type4_draw_psac_tile(INT32 tile_index)
{
	const UINT16 data = gx_ram_read_word(DrvType4PsacRAM, tile_index << 1, 0x7fff);
	const INT32 code = data & 0x1fff;
	const INT32 color = (data >> 13) & 1;
	const INT32 flipx = data & 0x4000;
	const INT32 flipy = data & 0x8000;

	UINT8 *gfx = DrvGfxROM2 + ((code & 0x1fff) << 8);
	UINT16 pal = 0x1800 + (color << 8);
	INT32 tile_x = tile_index >> 7;
	INT32 tile_y = tile_index & 0x7f;
	UINT16 *dst = pGxPsacBitmap + ((tile_y << 4) * 0x800) + (tile_x << 4);

	const INT32 row_step = flipy ? -16 : 16;
	const INT32 first_row = flipy ? 240 : 0;
	if (flipx) {
		for (INT32 y = 0; y < 16; y++) {
			UINT8 *src = gfx + first_row + y * row_step;
			for (INT32 x = 0; x < 16; x++) {
				dst[x] = src[15 - x] | pal;
			}
			dst += 0x800;
		}
	} else {
		for (INT32 y = 0; y < 16; y++) {
			UINT8 *src = gfx + first_row + y * row_step;
			for (INT32 x = 0; x < 16; x++) {
				dst[x] = src[x] | pal;
			}
			dst += 0x800;
		}
	}
}

static void gx_type4_draw_psac_dirty_range(INT32 start, INT32 end)
{
	if (gx_type4_psac_dirty_count == 128 * 128) {
		// Full queues cover every tile; partition in bitmap row order even after scattered writes.
		for (INT32 i = start; i < end; i++)
			gx_type4_draw_psac_tile(((i & 0x7f) << 7) | (i >> 7));
		return;
	}
	for (INT32 i = start; i < end; i++) {
		INT32 tile_index = gx_type4_psac_dirty_list[i];
		gx_type4_draw_psac_tile(tile_index);
		gx_type4_psac_tile_dirty[tile_index] = 0;
	}
}

static void gx_type4_predraw_psac()
{
	if (gx_special == GX_SPECIAL_SOCCERSS && pGxPsacBitmap) {
		if (gx_psac_cached_bank == gx_type3_psac2_bank)
			return;
		const UINT8 *map = DrvPsacMapROM + gx_type3_psac2_bank * 0x20000;
		// MAME get_gx_psac3[_alt]_tile_info: column-major, 256 x 256 tiles.
		for (INT32 i = 0; i < 65536; i++) {
			const UINT8 attr = map[i * 2 + 1];
			INT32 tile = map[i * 2] | ((attr & 15) << 8);
			UINT16 pal = 0x1000 | ((attr >> 6) << 8);
			for (INT32 y = 0; y < 16; y++)
				for (INT32 x = 0; x < 16; x++) {
					INT32 sx = (attr & 0x10) ? 15 - x : x, sy = (attr & 0x20) ? 15 - y : y;
					pGxPsacBitmap[((i & 255) * 16 + y) * 4096 + (i >> 8) * 16 + x] = DrvGfxROM2[tile * 256 + sy * 16 + sx] | pal;
				}
		}
		gx_psac_cached_bank = gx_type3_psac2_bank;
		return;
	}

	if (!gx_type4_enable || pGxPsacBitmap == NULL)
		return;
	if (!gx_type4_psac_dirty || gx_type4_psac_dirty_count <= 0)
		return;

	gx_type4_draw_psac_dirty_range(0, gx_type4_psac_dirty_count);

	if (gx_type4_psac_dirty_count == 128 * 128) {
		memset(gx_type4_psac_tile_dirty, 0, sizeof(gx_type4_psac_tile_dirty));
	}
	gx_type4_psac_dirty = 0;
	gx_type4_psac_dirty_count = 0;
}

static void gx_type4_update_k053936_regs()
{
	for (INT32 i = 0; i < 0x10; i++) {
		if (!gx_type4_ctrl_dirty[i])
			continue;
		gx_type4_ctrl_words[i] = gx_ram_read_word(DrvType4CtrlRAM, 0xe00000 + ((i ^ 1) << 1), 0x1f);
		gx_type4_ctrl_dirty[i] = 0;
	}
	gx_type4_ctrl_dirty_count = 0;

	for (INT32 i = 0; i < 0x800; i++) {
		if (!gx_type4_line_dirty[i])
			continue;
		gx_type4_line_words[i] = gx_ram_read_word(DrvType4LineRAM, 0xe60000 + ((i ^ 1) << 1), 0xfff);
		gx_type4_line_dirty[i] = 0;
	}
	gx_type4_line_dirty_count = 0;
}

static INT32 DrvDoReset()
{
	gx_sample_cycles = -1;
	gx_sample_irq_refresh = false;
	memset(AllRam, 0, RamEnd - AllRam);
	gx_esc_latch = 0;
	gx_objscan = 0;

	SekReset(0);
	SekReset(1);
	SekSetRESETLine(1, 1);

	KonamiICReset();
	K054338SetBgOffsets(gx_type4_enable ? 0 : (gx_special == GX_SPECIAL_DRAGOON ? 40 : (gx_special == GX_SPECIAL_WINSPIKE ? 39 : 24)), 16);
	K054539Reset(0);
	K054539Reset(1);
	DrvTms.reset();
	EEPROMReset();

	if (!EEPROMAvailable() && gx_default_eeprom_enable)
		EEPROMFill(DrvEEPROM, 0, 0x80);

	memset(gx_tilebanks, 0, sizeof(gx_tilebanks));
	memset(layer_colorbase, 0, sizeof(layer_colorbase));
	gx_tile_ram_bank = 0;
	gx_tile_ram_page = 0;
	gx_cfgport = gx_cfgport_default;
	gx_irq_control = 0;
	gx_irq_status = 0xfc;
	gx_post_frames = 0;
	// K053252 INT outputs begin low; a CCU acknowledge re-arms each edge.
	gx_syncen = 0x60;
	gx_control = 0;
	gx_current_scanline = 0;
	sound_control = 0;
	sound_irq_state = 0;
	sound_timer_irq_pending = 0;
	sound_int_enabled = 0;
	sound_int_pending = 0;
	sound_host_status = 0xc0;
	sound_host_status_wait = 0;
	sound_irq_line = 0;
	tms_pload = 1;
	tms_cload = 1;
	tms_reset = 0;
	tms_hidx = 0;
	tms_pc_nonzero = 0;
	tms_program_state = 0;
	tms_cload_cval = 0;
	tms_host_pending = 0;
	tms_host_index = 0;
	tms_cycle_frac = 0;
	gx_dsp_force = GX_DSP_FORCE_BOOT;
	gx_sound_buffer_pos = 0;
	gx_sound_cycles_total = 0;
	gx_type3_psac2_bank = 0;
	gx_psac_cached_bank = -1;
	gx_type3_spriteram_bank = 0;
	gx_type4_render_spriteram_bank = 0;
	memset(gx_type4_sprite_latch, 0, sizeof(gx_type4_sprite_latch));
	memset(gx_type4_sprite_latch_valid, 0, sizeof(gx_type4_sprite_latch_valid));
	gx_last_prot_op = -1;
	gx_last_prot_param = 0;
	gx_last_prot_clk = 0;
	gx_type4_current_frame = 0;
	gx_type4_last_dual_screen = gx_type4_enable ? ((DrvDips[0] & 0x08) == 0) : -1;
	gx_type4_last_flip_bits = gx_type4_enable ? (DrvDips[0] & 0x06) : -1;
	gx_type4_psac_colorbase = 0;
	gx_type4_left_bitmap_valid = 0;
	gx_type4_right_bitmap_valid = 0;
	gx_type4_display_hold = 1;
	gx_type4_mark_all_psac_dirty();
	gx_type4_mark_all_palettes_dirty();
	gx_type4_mark_all_k053936_dirty();
	DrvRecalc = 1;
	memset(fantjour_dma, 0, sizeof(fantjour_dma));
	sprite_colorbase = 0;
	nExtraCycles[0] = nExtraCycles[1] = 0;

	if (gx_type4_enable) {
		K053936_external_bitmap = pGxPsacBitmap;
		gx_type4_update_k053936_regs();
		m_k053936_0_ctrl_16 = gx_type4_ctrl_words;
		m_k053936_0_linectrl_16 = gx_type4_line_words;
		m_k053936_0_ctrl = NULL;
		m_k053936_0_linectrl = NULL;
		K053936GP_enable(0, 1);
		K053936GP_wraparound_enable(
		    0, (gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_RUSHHERO || gx_special == GX_SPECIAL_SOCCERSS) ? 1 : 0);
		K053936GP_set_offset(
		    0, (gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_SOCCERSS) ? -30 : -36,
		    gx_special == GX_SPECIAL_VSNET ? 0 : ((gx_special == GX_SPECIAL_RUSHHERO || gx_special == GX_SPECIAL_SOCCERSS) ? 1 : -1));
		K053936GP_set_visible_offset(0, 0, 16);
		K053936GP_set_source_size(0, gx_special == GX_SPECIAL_SOCCERSS ? 4096 : 2048, gx_special == GX_SPECIAL_SOCCERSS ? 4096 : 2048);
		K053936GP_set_colorbase(0, 0);
	}

	HiscoreReset();

	if (gx_special == GX_SPECIAL_LE2)
		K055555WriteReg(K55_INPUT_ENABLES, 1);
	return gx_esc_reset();
}

static const GxGameConfig *gx_get_game_config()
{
	const char *name = BurnDrvGetTextA(DRV_NAME);

	for (INT32 i = 0; GxGameConfigs[i].name; i++) {
		if (!strcmp(name, GxGameConfigs[i].name)) {
			return &GxGameConfigs[i];
		}
	}

	return NULL;
}

// rtl/video/gx_tilemap.sv derives six colour bits from REG3, then
// gx_mixer.v selects each tile's own mix code through VINMIX/VMIXON.
// Mode 3 performs this hardware decode, with no per-title scene heuristics.
static void gx_apply_alpha_tile_config()
{
	K056832SetAlphaTileMode(gx_special == GX_SPECIAL_TYPE4SD2 ? 0 : 3);
	extern INT32 konamigx_alpha_tile_additive;
	konamigx_alpha_tile_additive = 0;
}

// MAME 0.289 Type 4 ROM bus wiring; indices checked against each ROM set.
static INT32 gx_load_additional_type4()
{
	if (BurnLoadRom(DrvMainROM, 0, 1))
		return 1;
	BurnByteswap(DrvMainROM, 0x20000);
	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "vsnetscr") || !strcmp(BurnDrvGetTextA(DRV_NAME), "vsnetscreb") ||
	    !strcmp(BurnDrvGetTextA(DRV_NAME), "vsnetscru") || !strcmp(BurnDrvGetTextA(DRV_NAME), "vsnetscra") ||
	    !strcmp(BurnDrvGetTextA(DRV_NAME), "vsnetscrj")) {
		if (BurnLoadRomExt(DrvMainROM + 0x200000, 1, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x200002, 2, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x400000, 3, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x400002, 4, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRom(DrvSoundROM + 0x000000, 5, 2))
			return 1;
		if (BurnLoadRom(DrvSoundROM + 0x000001, 6, 2))
			return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000, 7, 2))
			return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001, 8, 2))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0000000, 9, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0000002, 10, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0000004, 11, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0c00000, 12, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0c00002, 13, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0c00004, 14, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 15, 1))
			return 1;
		if (BurnLoadRom(DrvSndROM + 0x000000, 16, 1))
			return 1;
	}
	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "rushhero")) {
		if (BurnLoadRomExt(DrvMainROM + 0x200000, 1, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x200002, 2, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x400000, 3, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x400002, 4, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRom(DrvSoundROM + 0x000000, 5, 2))
			return 1;
		if (BurnLoadRom(DrvSoundROM + 0x000001, 6, 2))
			return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000, 7, 2))
			return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001, 8, 2))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0000000, 9, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0000002, 10, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0000004, 11, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0c00000, 12, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0c00002, 13, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x0c00004, 14, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x1800000, 15, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x1800002, 16, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x1800004, 17, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x2400000, 18, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x2400002, 19, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x2400004, 20, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 21, 1))
			return 1;
		if (BurnLoadRom(DrvSndROM + 0x000000, 22, 1))
			return 1;
	}
	BurnByteswap(DrvMainROM + 0x200000, 0x100000);
	BurnByteswap(DrvMainROM + 0x400000, 0x200000);
	BurnByteswap(DrvSoundROM, 0x40000);
	return 0;
}

static INT32 gx_load_type3()
{
	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "soccerss") || !strcmp(BurnDrvGetTextA(DRV_NAME), "soccerssu") ||
	    !strcmp(BurnDrvGetTextA(DRV_NAME), "soccerssj") || !strcmp(BurnDrvGetTextA(DRV_NAME), "soccerssja") ||
	    !strcmp(BurnDrvGetTextA(DRV_NAME), "soccerssa")) {
		if (BurnLoadRomExt(DrvMainROM + 0x200000, 1, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x200002, 2, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x400000, 3, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x400002, 4, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRom(DrvSoundROM + 0x000000, 5, 2))
			return 1;
		if (BurnLoadRom(DrvSoundROM + 0x000001, 6, 2))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000, 7, 6, LD_GROUP(4)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000004, 8, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000, 9, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002, 10, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004, 11, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x600000, 12, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x600002, 13, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x600004, 14, 6, LD_GROUP(2)))
			return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 15, 1))
			return 1;
		if (BurnLoadRom(DrvPsacMapROM + 0x000000, 16, 1))
			return 1;
		if (BurnLoadRom(DrvSndROM + 0x000000, 17, 1))
			return 1;
		if (BurnLoadRom(DrvEEPROM + 0x0000, 18, 1))
			return 1;
	}
	if (BurnLoadRom(DrvMainROM, 0, 1))
		return 1;
	BurnByteswap(DrvMainROM, 0x20000);
	BurnByteswap(DrvMainROM + 0x200000, 0x100000);
	BurnByteswap(DrvMainROM + 0x400000, 0x100000);
	BurnByteswap(DrvSoundROM, 0x40000);
	return 0;
}

// MiSTer gx_main.sv: uncached program-space ROM reads take six
// 48 MHz clocks. Add the two 24 MHz wait cycles beyond the normal
// two-clock bus response. Data reads and cached instruction fetches
// retain their existing timing; no ROM or POST result is patched.
static UINT8 __fastcall gx_uncached_rom_fetch_byte(UINT32 address)
{
	if (!(SekGetCACR() & 1))
		SekCyclesBurnRun(2);
	return DrvMainROM[(address & 0x7fffff) ^ 1];
}

static UINT16 __fastcall gx_uncached_rom_fetch_word(UINT32 address)
{
	if (!(SekGetCACR() & 1))
		SekCyclesBurnRun(2);
	return BURN_ENDIAN_SWAP_INT16(*(UINT16 *)(DrvMainROM + (address & 0x7ffffe)));
}

static UINT32 __fastcall gx_uncached_rom_fetch_long(UINT32 address)
{
	return ((UINT32)gx_uncached_rom_fetch_word(address) << 16) | gx_uncached_rom_fetch_word(address + 2);
}

static INT32 DrvInit()
{
	const GxGameConfig *config = gx_get_game_config();
	if (config == NULL)
		return 1;

	gx_cfgport_default = config->cfgport;
	gx_special = config->special;
	gx_frame_rate = 59.185606;
	BurnSetRefreshRate(gx_frame_rate);
	gx_tile_rom_size = config->tile_rom_size;
	gx_tile_bpp = config->tile_bpp;
	gx_tile_color_granularity = config->tile_color_granularity;
	gx_sprite_word_size = config->sprite_word_size;
	gx_sprite_bpp = config->sprite_bpp;
	gx_fantjour_dma_enable = gx_special == GX_SPECIAL_FANTJOUR;
	gx_type4_enable = gx_special == GX_SPECIAL_TYPE4SD2 || gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_RUSHHERO ||
	                  gx_special == GX_SPECIAL_SOCCERSS;
	gx_le2_flip_y = gx_special == GX_SPECIAL_LE2 && strcmp(BurnDrvGetTextA(DRV_NAME), "le2") != 0;
	gx_monitor_width = (gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_SOCCERSS) ? 576 : 384;
	gx_rushingheroes_hack = gx_type4_enable && gx_special != GX_SPECIAL_SOCCERSS;
	gx_default_eeprom_enable = config->eeprom >= 0 || gx_special == GX_SPECIAL_SOCCERSS;

	if (gx_type4_enable) {
		INT32 width = ((DrvDips[0] & 0x08) == 0) ? (GX_TYPE4_MONITOR_WIDTH * 2) : GX_TYPE4_MONITOR_WIDTH;
		BurnTransferSetDimensions(width, 224);
		GenericTilesSetClipRaw(0, width, 0, 224);
		BurnDrvSetVisibleSize(width, 224);
		BurnDrvSetAspect((width == GX_TYPE4_MONITOR_WIDTH) ? 4 : 8, 3);
	}

	GenericTilesInit();
	if (gx_special == GX_SPECIAL_LE2)
		BurnGunInit(2, true);
	BurnAllocMemIndex();

	if (gx_type4_enable) {
		INT32 width, height;

		pGxPsacBitmap = (UINT16 *)BurnMalloc((gx_special == GX_SPECIAL_SOCCERSS ? 0x1000000 : 0x400000) * sizeof(UINT16));
		if (pGxPsacBitmap == NULL)
			return 1;
		memset(pGxPsacBitmap, 0, (gx_special == GX_SPECIAL_SOCCERSS ? 0x1000000 : 0x400000) * sizeof(UINT16));
		gx_type4_mark_all_psac_dirty();

		BurnDrvGetVisibleSize(&width, &height);
		pGxType4LeftBitmap = (UINT32 *)BurnMalloc(GX_TYPE4_MONITOR_WIDTH * height * sizeof(UINT32));
		pGxType4RightBitmap = (UINT32 *)BurnMalloc(GX_TYPE4_MONITOR_WIDTH * height * sizeof(UINT32));
		if (pGxType4LeftBitmap == NULL || pGxType4RightBitmap == NULL) {
			if (pGxType4LeftBitmap) {
				BurnFree(pGxType4LeftBitmap);
				pGxType4LeftBitmap = NULL;
			}
			if (pGxType4RightBitmap) {
				BurnFree(pGxType4RightBitmap);
				pGxType4RightBitmap = NULL;
			}
			BurnFree(pGxPsacBitmap);
			pGxPsacBitmap = NULL;
			return 1;
		}
		memset(pGxType4LeftBitmap, 0, GX_TYPE4_MONITOR_WIDTH * height * sizeof(UINT32));
		memset(pGxType4RightBitmap, 0, GX_TYPE4_MONITOR_WIDTH * height * sizeof(UINT32));
	}

	if (gx_special == GX_SPECIAL_SOCCERSS) {
		if (gx_load_type3())
			return 1;
	} else if (gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_RUSHHERO) {
		if (gx_load_additional_type4())
			return 1;
	} else if (gx_special == GX_SPECIAL_DRAGOON || gx_special == GX_SPECIAL_WINSPIKE || gx_special == GX_SPECIAL_LE2) {
		if (BurnLoadRom(DrvMainROM, 0, 1))
			return 1;
		BurnByteswap(DrvMainROM, 0x20000);
		if (gx_special == GX_SPECIAL_LE2) {
			for (INT32 i = 0; i < 4; i++)
				if (BurnLoadRom(DrvMainROM + 0x200000 + i, 1 + i, 4))
					return 1;
		} else {
			if (BurnLoadRomExt(DrvMainROM + 0x200002, 1, 4, LD_GROUP(2) | LD_BYTESWAP))
				return 1;
			if (BurnLoadRomExt(DrvMainROM + 0x200000, 2, 4, LD_GROUP(2) | LD_BYTESWAP))
				return 1;
		}
		BurnByteswap(DrvMainROM + 0x200000, 0x100000);
		INT32 snd = 3;
		if (gx_special == GX_SPECIAL_DRAGOON) {
			if (BurnLoadRomExt(DrvMainROM + 0x400002, 3, 4, LD_GROUP(2) | LD_BYTESWAP))
				return 1;
			if (BurnLoadRomExt(DrvMainROM + 0x400000, 4, 4, LD_GROUP(2) | LD_BYTESWAP))
				return 1;
			BurnByteswap(DrvMainROM + 0x400000, 0x200000);
			snd = 5;
			if (BurnLoadRomExt(DrvGfxROM0, 7, 5, LD_GROUP(4)))
				return 1;
			for (INT32 i = 0; i < 8; i++)
				if (BurnLoadRomExt(DrvGfxROM1 + (i / 2) * 0x400000 + (i & 1) * 2, 8 + i, 4, LD_GROUP(2)))
					return 1;
		} else if (gx_special == GX_SPECIAL_LE2) {
			snd = 5;
			for (INT32 i = 0; i < 4; i++)
				if (BurnLoadRom(DrvGfxROM0 + (i / 2) * 0x400000 + (i & 1), 7 + i, 2))
					return 1;
			for (INT32 i = 0; i < 4; i++)
				if (BurnLoadRomExt(DrvGfxROM1 + i * 2, 11 + i, 8, LD_GROUP(2)))
					return 1;
		} else {
			if (BurnLoadRom(DrvGfxROM0, 5, 2) || BurnLoadRom(DrvGfxROM0 + 1, 6, 2))
				return 1;
			for (INT32 i = 0; i < 4; i++)
				if (BurnLoadRomExt(DrvGfxROM1 + i * 2, 7 + i, 8, LD_GROUP(2)))
					return 1;
		}
		if (BurnLoadRom(DrvSoundROM, snd, 2) || BurnLoadRom(DrvSoundROM + 1, snd + 1, 2))
			return 1;
		BurnByteswap(DrvSoundROM, 0x40000);
		if (BurnLoadRom(DrvSndROM, config->sample0, 1))
			return 1;
		if (config->sample1 >= 0 && BurnLoadRom(DrvSndROM + 0x200000, config->sample1, 1))
			return 1;
		if (config->eeprom >= 0 && BurnLoadRom(DrvEEPROM, config->eeprom, 1))
			return 1;
	} else {
		if (BurnLoadRom(DrvMainROM + 0x000000, 0, 1))
			return 1;
		BurnByteswap(DrvMainROM, 0x020000);
		if (BurnLoadRomExt(DrvMainROM + 0x200002, 1, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		if (BurnLoadRomExt(DrvMainROM + 0x200000, 2, 4, LD_GROUP(2) | LD_BYTESWAP))
			return 1;
		BurnByteswap(DrvMainROM + 0x200000, 0x100000);

		if (gx_type4_enable) {
			if (BurnLoadRomExt(DrvMainROM + 0x400002, 3, 4, LD_GROUP(2) | LD_BYTESWAP))
				return 1;
			if (BurnLoadRomExt(DrvMainROM + 0x400000, 4, 4, LD_GROUP(2) | LD_BYTESWAP))
				return 1;
			BurnByteswap(DrvMainROM + 0x400000, 0x200000);
		}

		if (gx_special == GX_SPECIAL_TKMMPZDM) {
			gx_patch_mainrom_long(0x810f1 * 4, 0x00000000, 0x00000001);
			gx_patch_mainrom_long(0x872ea * 4, 0x000e0000, 0x00000000);
		}

		INT32 sound0 = gx_type4_enable ? 5 : 3;
		INT32 sound1 = gx_type4_enable ? 6 : 4;
		if (BurnLoadRom(DrvSoundROM + 0x000000, sound0, 2))
			return 1;
		if (BurnLoadRom(DrvSoundROM + 0x000001, sound1, 2))
			return 1;
		BurnByteswap(DrvSoundROM, 0x040000);

		if (gx_type4_enable) {
			if (BurnLoadRom(DrvGfxROM0 + 0x000000, 7, 2))
				return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x000001, 8, 2))
				return 1;
		} else if (config->tile_layout == GX_TILE_LAYOUT_8BPP) {
			if (BurnLoadRom(DrvGfxROM0 + 0x000000, 7, 2))
				return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x000001, 8, 2))
				return 1;
		} else if (config->tile_layout == GX_TILE_LAYOUT_6BPP) {
			if (gx_special == GX_SPECIAL_SALMNDR2) {
				if (BurnLoadRomExt(DrvGfxROM0 + 0x000000, 5, 6, LD_GROUP(4)))
					return 1;
				if (BurnLoadRomExt(DrvGfxROM0 + 0x300000, 6, 6, LD_GROUP(4)))
					return 1;
				if (BurnLoadRomExt(DrvGfxROM0 + 0x000004, 7, 6, LD_GROUP(2)))
					return 1;
			} else {
				if (BurnLoadRomExt(DrvGfxROM0 + 0x000000, 5, 6, LD_GROUP(4)))
					return 1;
				if (BurnLoadRomExt(DrvGfxROM0 + 0x000004, 6, 6, LD_GROUP(2)))
					return 1;
			}
		} else {
			if (BurnLoadRomExt(DrvGfxROM0 + 0x000000, 5, 5, LD_GROUP(4)))
				return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x000004, 6, 5))
				return 1;
		}

		if (gx_type4_enable) {
			for (INT32 group = 0; group < 4; group++) {
				UINT32 base = group * 0x600000;
				if (BurnLoadRomExt(DrvGfxROM1 + base + 0, 9 + (group * 3), 6, LD_GROUP(2)))
					return 1;
				if (BurnLoadRomExt(DrvGfxROM1 + base + 2, 10 + (group * 3), 6, LD_GROUP(2)))
					return 1;
				if (BurnLoadRomExt(DrvGfxROM1 + base + 4, 11 + (group * 3), 6, LD_GROUP(2)))
					return 1;
			}
			if (BurnLoadRom(DrvGfxROM2 + 0x000000, 21, 1))
				return 1;
		} else {
			if (gx_sprite_bpp == 6) {
				if (config->sprite_pair0_a >= 0 && BurnLoadRomExt(DrvGfxROM1 + 0x000000, config->sprite_pair0_a, 6, LD_GROUP(2)))
					return 1;
				if (config->sprite_pair0_b >= 0 && BurnLoadRomExt(DrvGfxROM1 + 0x000002, config->sprite_pair0_b, 6, LD_GROUP(2)))
					return 1;
				if (config->sprite_fifth >= 0 && BurnLoadRomExt(DrvGfxROM1 + 0x000004, config->sprite_fifth, 6, LD_GROUP(2)))
					return 1;
			} else {
				if (config->sprite_pair0_a >= 0 && BurnLoadRomExt(DrvGfxROM1 + 0x000000, config->sprite_pair0_a, 4, LD_GROUP(2)))
					return 1;
				if (config->sprite_pair0_b >= 0 && BurnLoadRomExt(DrvGfxROM1 + 0x000002, config->sprite_pair0_b, 4, LD_GROUP(2)))
					return 1;
				if (config->sprite_pair1_a >= 0 && BurnLoadRomExt(DrvGfxROM1 + 0x400000, config->sprite_pair1_a, 4, LD_GROUP(2)))
					return 1;
				if (config->sprite_pair1_b >= 0 && BurnLoadRomExt(DrvGfxROM1 + 0x400002, config->sprite_pair1_b, 4, LD_GROUP(2)))
					return 1;
				if (config->sprite_fifth >= 0 && BurnLoadRom(DrvGfxROM1 + config->sprite_word_size, config->sprite_fifth, 1))
					return 1;
			}
		}

		if (config->sample0 >= 0 && BurnLoadRom(DrvSndROM + 0x000000, config->sample0, 1))
			return 1;
		if (config->sample1 >= 0 && BurnLoadRom(DrvSndROM + 0x200000, config->sample1, 1))
			return 1;

		if (config->eeprom >= 0 && BurnLoadRom(DrvEEPROM + 0x000000, config->eeprom, 1))
			return 1;
	}

	if (DecodeGraphics())
		return 1;

	K055555Init();
	K054338Init();
	K054338SetBgOffsets(gx_type4_enable ? 0 : (gx_special == GX_SPECIAL_DRAGOON ? 40 : (gx_special == GX_SPECIAL_WINSPIKE ? 39 : 24)), 16);

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, gx_tile_rom_size, gx_tile_callback);
	K056832SetRomExpTileCount(gx_tile_rom_size / (gx_tile_bpp * 8));
	K056832SetRomBankCount(gx_tile_rom_size / 0x2000);
	K056832SetColorGranularity(gx_tile_color_granularity);
	K056832SetGlobalOffsets(gx_type4_enable ? 0 : 24, 16);
	if (gx_type4_enable) {
		K056832SetLayerOffsets(0, -29, -1);
		K056832SetLayerOffsets(1, -27, -1);
		K056832SetLayerOffsets(2, -26, -1);
		K056832SetLayerOffsets(3, -24, -1);
	} else {
		K056832SetLayerOffsets(0, -2, 0);
		K056832SetLayerOffsets(1, 0, 0);
		K056832SetLayerOffsets(2, 2, 0);
		K056832SetLayerOffsets(3, 3, 0);
	}
	if (gx_special == GX_SPECIAL_DRAGOON || gx_special == GX_SPECIAL_WINSPIKE) {
		INT32 origin = gx_special == GX_SPECIAL_DRAGOON ? 40 : 39;
		K054338SetBgOffsets(origin, 16);
		K056832SetGlobalOffsets(origin, 16);
		if (gx_special == GX_SPECIAL_DRAGOON) {
			K056832SetLayerOffsets(0, -1, 0);
			K056832SetLayerOffsets(1, 1, 0);
			K056832SetLayerOffsets(2, 3, 0);
			K056832SetLayerOffsets(3, 4, 0);
		}
	}
	if (gx_special == GX_SPECIAL_WINSPIKE)
		K056832SetLayerAssociation(0);
	if (gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_SOCCERSS) {
		K056832SetLayerAssociation(0);
		K056832SetLayerOffsets(0, -52, 0);
		K056832SetLayerOffsets(1, -48, 0);
		K056832SetLayerOffsets(2, -48, 0);
		K056832SetLayerOffsets(3, -48, 0);
	} else if (gx_special == GX_SPECIAL_RUSHHERO) {
		K056832SetLayerOffsets(0, -27, 0);
		K056832SetLayerOffsets(1, -25, 0);
		K056832SetLayerOffsets(2, -24, 0);
		K056832SetLayerOffsets(3, -22, 0);
	}
	gx_apply_alpha_tile_config();

	//	K056832SetAlphaTileMode(gx_special == GX_SPECIAL_SALMNDR2 ? 2 : (gx_special == GX_SPECIAL_SEXYPARO ? 1 : 0));
	//	K056832SetAlphaTileCallback(NULL);

	INT32 sprite_tile_count = gx_sprite_word_size / (gx_sprite_bpp == 5 ? 0x80 : (32 * gx_sprite_bpp));
	INT32 sprite_rom_mask = ((gx_sprite_bpp == 6) ? gx_sprite_word_size : 0x1000000) - 1;
	K053247Init(DrvGfxROM1, DrvGfxROMExp1, sprite_rom_mask, gx_sprite_callback, gx_type4_enable ? 3 : 1);
	K053247SetRamSize(0x4000);
	K053247SetSpriteOffset(config->sprite_offset_x, config->sprite_offset_y);
	K053247SetVisibleOffset(0, gx_type4_enable ? 16 : 0);
	K053247SetBpp(gx_sprite_bpp);
	K053247SetDecodedTileMask(sprite_tile_count - 1);
	konamigx_mixer_init(0);
	konamigx_mixer_primode(config->mixer_primode);

	if (gx_type4_enable) {
		K053936_external_bitmap = pGxPsacBitmap;
		gx_type4_update_k053936_regs();
		m_k053936_0_ctrl_16 = gx_type4_ctrl_words;
		m_k053936_0_linectrl_16 = gx_type4_line_words;
		m_k053936_0_ctrl = NULL;
		m_k053936_0_linectrl = NULL;
		K053936GP_enable(0, 1);
		K053936GP_wraparound_enable(
		    0, (gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_RUSHHERO || gx_special == GX_SPECIAL_SOCCERSS) ? 1 : 0);
		K053936GP_set_offset(
		    0, (gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_SOCCERSS) ? -30 : -36,
		    gx_special == GX_SPECIAL_VSNET ? 0 : ((gx_special == GX_SPECIAL_RUSHHERO || gx_special == GX_SPECIAL_SOCCERSS) ? 1 : -1));
		K053936GP_set_visible_offset(0, 0, 16);
		K053936GP_set_source_size(0, gx_special == GX_SPECIAL_SOCCERSS ? 4096 : 2048, gx_special == GX_SPECIAL_SOCCERSS ? 4096 : 2048);
		K053936GP_set_colorbase(0, 0);
	}

	SekInit(0, 0x68EC020);
	SekOpen(0);
	SekMapMemory(DrvMainROM + 0x000000, 0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(DrvMainROM + 0x200000, 0x200000, 0x7fffff, MAP_ROM);
	SekMapMemory(DrvMainRAM, 0xc00000, 0xc1ffff, MAP_RAM);
	if (gx_special == GX_SPECIAL_SOCCERSS) {
		SekMapHandler(1, 0x000000, 0x01ffff, MAP_FETCH);
		SekMapHandler(1, 0x200000, 0x7fffff, MAP_FETCH);
		SekSetReadByteHandler(1, gx_uncached_rom_fetch_byte);
		SekSetReadWordHandler(1, gx_uncached_rom_fetch_word);
		SekSetReadLongHandler(1, gx_uncached_rom_fetch_long);
	}
	SekSetReadByteHandler(0, gx_main_read_byte);
	SekSetReadWordHandler(0, gx_main_read_word);
	SekSetReadLongHandler(0, gx_main_read_long);
	SekSetWriteByteHandler(0, gx_main_write_byte);
	SekSetWriteWordHandler(0, gx_main_write_word);
	SekSetWriteLongHandler(0, gx_main_write_long);
	SekClose();

	SekInit(1, 0x68000);
	SekOpen(1);
	SekMapMemory(DrvSoundROM, 0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvSoundRAM, 0x100000, 0x10ffff, MAP_RAM);
	SekSetReadByteHandler(0, gx_sound_read_byte);
	SekSetReadWordHandler(0, gx_sound_read_word);
	SekSetWriteByteHandler(0, gx_sound_write_byte);
	SekSetWriteWordHandler(0, gx_sound_write_word);
	SekClose();

	EEPROMInit(&gx_eeprom_interface);

	// MiSTer gx_sound/gx_k054539 masks PCM and ROM-test addresses to the ROM bus.
	const INT32 pcm_flags = K054539_UPDATE_AT_KEYON | K054539_GX | (gx_special >= GX_SPECIAL_DRAGOON ? K054539_ROM_WRAP : 0);
	K054539Init(0, 18432000, DrvSndROM, 0x400000);
	K054539SetFlags(0, pcm_flags);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	K054539SetIRQCallback(0, sound_irq);
	K054539_set_gain(0, 4, 2.00);
	K054539_set_gain(0, 5, 2.00);
	K054539_set_gain(0, 6, 2.00);
	K054539_set_gain(0, 7, 2.00);

	K054539Init(1, 18432000, DrvSndROM, 0x400000);
	K054539SetFlags(1, pcm_flags);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_1, 1.40, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(1, BURN_SND_K054539_ROUTE_2, 1.40, BURN_SND_ROUTE_RIGHT);
	K054539_set_gain(1, 0, 2.00);
	K054539_set_gain(1, 1, 2.00);
	K054539_set_gain(1, 2, 2.00);
	K054539_set_gain(1, 3, 2.00);
	K054539_set_gain(1, 4, 2.00);
	K054539_set_gain(1, 5, 2.00);
	K054539_set_gain(1, 6, 2.00);
	K054539_set_gain(1, 7, 2.00);

	DrvTms.init(DrvTmsRAM);

	konami_palette32 = DrvPalette;
	K053936SetRenderTarget(konami_bitmap32, konami_palette32, konami_priority_bitmap);

	if (DrvDoReset())
		return 1;

	return 0;
}

static INT32 DrvExit()
{
	if (gx_special == GX_SPECIAL_LE2)
		BurnGunExit();
	GenericTilesExit();
	KonamiICExit();
	K054539Exit();
	EEPROMExit();
	SekExit();
	K053936GPExit();
	if (pGxPsacBitmap) {
		BurnFree(pGxPsacBitmap);
		pGxPsacBitmap = NULL;
	}
	if (pGxType4LeftBitmap) {
		BurnFree(pGxType4LeftBitmap);
		pGxType4LeftBitmap = NULL;
	}
	if (pGxType4RightBitmap) {
		BurnFree(pGxType4RightBitmap);
		pGxType4RightBitmap = NULL;
	}
	gx_type4_left_bitmap_valid = 0;
	gx_type4_right_bitmap_valid = 0;
	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate(INT32 use_sub_palette = 0)
{
	if (gx_type4_enable) {
		INT32 palette = use_sub_palette ? 1 : 0;
		UINT8 *palram = palette ? DrvSubPalRAM : DrvPalRAM;

		if (DrvRecalc) {
			gx_type4_mark_all_palettes_dirty();
			DrvRecalc = 0;
		}

		if (gx_special == GX_SPECIAL_SOCCERSS) {
			for (INT32 i = 0; i < 0x2000; i++) {
				UINT16 d = gx_ram_read_word(palram, i * 2, 0x3fff);
				INT32 r = d & 31, g = (d >> 5) & 31, b = (d >> 10) & 31;
				gx_type4_palette_cache[palette][i] = BurnHighCol((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2), 0);
			}
		} else if (gx_type4_palette_dirty_count[palette] == 0x2000) {
			// A deduplicated full queue covers every color, independent of write order.
			for (INT32 i = 0; i < 0x2000; i++) {
				UINT32 d = gx_ram_read_long(palram, 0xe80000 + (i << 2), 0x7fff);
				gx_type4_palette_cache[palette][i] = BurnHighCol((d >> 16) & 0xff, (d >> 8) & 0xff, d & 0xff, 0);
			}
			memset(gx_type4_palette_dirty[palette], 0, sizeof(gx_type4_palette_dirty[palette]));
			gx_type4_palette_dirty_count[palette] = 0;
		} else if (gx_type4_palette_dirty_count[palette] > 0) {
			for (INT32 dirty_index = 0; dirty_index < gx_type4_palette_dirty_count[palette]; dirty_index++) {
				INT32 i = gx_type4_palette_dirty_list[palette][dirty_index];

				UINT32 d = gx_ram_read_long(palram, 0xe80000 + (i << 2), 0x7fff);
				gx_type4_palette_cache[palette][i] = BurnHighCol((d >> 16) & 0xff, (d >> 8) & 0xff, d & 0xff, 0);
				gx_type4_palette_dirty[palette][i] = 0;
			}

			gx_type4_palette_dirty_count[palette] = 0;
		}

		// The mixer only reads this cache; keep both monitor palettes independent.
		konami_palette32 = gx_type4_palette_cache[palette];
		K053936SetRenderTarget(konami_bitmap32, konami_palette32, konami_priority_bitmap);
		return;
	}

	gx_palette_updates.Update((const UINT32 *)DrvPalRAM, DrvPalette, DrvRecalc != 0);
	DrvRecalc = 0;
}

static INT32 gx_type4_dual_screen_enabled()
{
	return 1;
}

static INT32 gx_type4_output_dual_screen_enabled()
{
	return (DrvDips[0] & 0x08) == 0;
}

static INT32 gx_type4_monitor_flip_enabled(INT32 use_sub_palette)
{
	return (DrvDips[0] & (use_sub_palette ? 0x04 : 0x02)) == 0;
}

static INT32 gx_type4_check_screen_mode_change()
{
	if (!gx_type4_enable)
		return 0;

	INT32 dual_screen = gx_type4_output_dual_screen_enabled();
	INT32 flip_bits = DrvDips[0] & 0x06;

	if (gx_type4_last_dual_screen < 0) {
		gx_type4_last_dual_screen = dual_screen;
		gx_type4_last_flip_bits = flip_bits;
		return 0;
	}

	if (gx_type4_last_dual_screen != dual_screen) {
		gx_type4_last_dual_screen = dual_screen;
		gx_type4_last_flip_bits = flip_bits;
		gx_type4_left_bitmap_valid = 0;
		gx_type4_right_bitmap_valid = 0;
		gx_type4_current_frame = 0;
		return 1;
	}

	if (gx_type4_last_flip_bits != flip_bits) {
		gx_type4_last_flip_bits = flip_bits;
		gx_type4_left_bitmap_valid = 0;
		gx_type4_right_bitmap_valid = 0;
		gx_type4_current_frame = 0;
	}

	return 0;
}

static INT32 gx_type4_check_screen_size()
{
	INT32 dual_screen = gx_type4_output_dual_screen_enabled();
	INT32 width = dual_screen ? (GX_TYPE4_MONITOR_WIDTH * 2) : GX_TYPE4_MONITOR_WIDTH;

	if (width != nScreenWidth) {
		BurnTransferSetDimensions(width, nScreenHeight);
		GenericTilesSetClipRaw(0, width, 0, nScreenHeight);
		BurnDrvSetVisibleSize(width, nScreenHeight);
		BurnDrvSetAspect(dual_screen ? 8 : 4, 3);
		Reinitialise();
		BurnTransferRealloc();
		KonamiAllocateBitmaps();
		gx_type4_left_bitmap_valid = 0;
		gx_type4_right_bitmap_valid = 0;
		gx_type4_current_frame = 0;

		if (pGxType4LeftBitmap) {
			memset(pGxType4LeftBitmap, 0, GX_TYPE4_MONITOR_WIDTH * nScreenHeight * sizeof(UINT32));
		}

		if (pGxType4RightBitmap) {
			memset(pGxType4RightBitmap, 0, GX_TYPE4_MONITOR_WIDTH * nScreenHeight * sizeof(UINT32));
		}

		return 1;
	}

	return 0;
}

// MAME 0.289 K053936_zoom_draw (Type 3): indexed sampling before SUB2 mixing.
static void gx_type3_render_roz()
{
	const UINT16 *ctrl = gx_type4_ctrl_words;
	const bool line_mode = (ctrl[7] & 0x40) != 0;
	for (INT32 y = 0; y < nScreenHeight; y++) {
		UINT32 startx, starty;
		INT32 incxx, incxy;
		if (line_mode) {
			const UINT16 *line = gx_type4_line_words + (((y + 16 - 1) & 0x1ff) * 4);
			startx = 256 * (INT16)(line[0] + ctrl[0]);
			starty = 256 * (INT16)(line[1] + ctrl[1]);
			incxx = (INT16)line[2];
			incxy = (INT16)line[3];
			if (ctrl[6] & 0x8000)
				incxx *= 256;
			if (ctrl[6] & 0x0080)
				incxy *= 256;
		} else {
			INT32 incyx = (INT16)ctrl[2], incyy = (INT16)ctrl[3];
			incxx = (INT16)ctrl[4];
			incxy = (INT16)ctrl[5];
			if (ctrl[6] & 0x4000) {
				incyx *= 256;
				incyy *= 256;
			}
			if (ctrl[6] & 0x0040) {
				incxx *= 256;
				incxy *= 256;
			}
			startx = (UINT32)(256 * (INT16)ctrl[0]) + (UINT32)incyx * (y + 16 - 1);
			starty = (UINT32)(256 * (INT16)ctrl[1]) + (UINT32)incyy * (y + 16 - 1);
		}
		startx = (startx + (UINT32)incxx * 30) << 5;
		starty = (starty + (UINT32)incxy * 30) << 5;
		const UINT32 stepx = (UINT32)incxx << 5, stepy = (UINT32)incxy << 5;
		UINT16 *dst = DrvType3RozBitmap + y * nScreenWidth;
		for (INT32 x = 0; x < (nScreenWidth + 1) / 2; x++) {
			dst[x] = pGxPsacBitmap[((starty >> 16) & 4095) * 4096 + ((startx >> 16) & 4095)];
			startx += stepx;
			starty += stepy;
		}
	}
}

static void gx_type4_render_monitor(INT32 use_sub_palette, INT32 spriteram_bank, UINT32 *target)
{
	if (!gx_type4_enable || target == NULL)
		return;

	INT32 screen_width = nScreenWidth;
	INT32 screen_height = nScreenHeight;
	INT32 flip_x_offset = gx_type4_monitor_flip_enabled(use_sub_palette) ? GX_TYPE4_FLIP_X_OFFSET : 0;
	INT32 render_width =
	    (flip_x_offset && screen_width > GX_TYPE4_MONITOR_WIDTH) ? (GX_TYPE4_MONITOR_WIDTH + flip_x_offset) : GX_TYPE4_MONITOR_WIDTH;
	INT32 flip_y_offset = (flip_x_offset && screen_width > GX_TYPE4_MONITOR_WIDTH) ? GX_TYPE4_FLIP_Y_OFFSET : 0;
	INT32 render_height = screen_height + flip_y_offset;

	nScreenWidth = render_width;
	nScreenHeight = render_height;
	K056832SetGlobalOffsets(0, 16);
	DrvPaletteUpdate(use_sub_palette);
	konamigx_prepare_frame();

	for (INT32 i = 0; i < 4; i++) {
		layer_colorbase[i] = K055555GetPaletteIndex(i) << 4;
	}
	sprite_colorbase = K055555GetPaletteIndex(4) << 5;

	INT32 new_psac_colorbase = 0;
	if (gx_type4_psac_colorbase != new_psac_colorbase) {
		gx_type4_psac_colorbase = new_psac_colorbase;
		gx_type4_mark_all_psac_dirty();
	}

	gx_type4_predraw_psac();
	gx_type4_update_k053936_regs();
	K053936GP_set_colorbase(0, 0);
	// Rushing Heroes uses a one-column, unit-step ROZ strip for the advisory
	// backdrop. Its absolute source Y is 0..383; the legacy/MiSTer +36 bias
	// reads past that strip. Correct only this degenerate rotated transform:
	// ordinary simple-mode images (the title) and line-mode stadiums need
	// their existing origin. 0x800 is one source pixel in the PSAC increment.
	const UINT16 *psac = gx_type4_ctrl_words;
	const bool absolute_strip = gx_special == GX_SPECIAL_RUSHHERO && !(psac[7] & 0x40) && !(psac[6] & 0x4040) && psac[4] == 0 &&
	                            psac[5] == 0x800 && psac[3] == 0 && (UINT32)psac[2] * nScreenHeight < 0x800;
	const INT32 psac_x_offset = absolute_strip ? 0 : ((gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_SOCCERSS) ? -30 : -36);
	K053936GP_set_offset(
	    0, psac_x_offset,
	    gx_special == GX_SPECIAL_VSNET ? 0 : ((gx_special == GX_SPECIAL_RUSHHERO || gx_special == GX_SPECIAL_SOCCERSS) ? 1 : -1));
	K053936GP_set_visible_offset(0, 0, 16);
	K053936GP_wraparound_enable(
	    0, (flip_x_offset || gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_RUSHHERO || gx_special == GX_SPECIAL_SOCCERSS) ? 1
		                                                                                                                               : 0);
	K053247SetVisibleOffset(-flip_x_offset, 16 + flip_y_offset - (flip_x_offset ? GX_TYPE4_FLIP_SPRITE_Y_ADJUST : 0));
	if (gx_type4_sprite_latch_valid[spriteram_bank & 1]) {
		konamigx_mixer_set_spriteram_latch(gx_type4_sprite_latch[spriteram_bank & 1]);
	} else {
		konamigx_mixer_set_spriteram_bank(spriteram_bank);
	}

	if (gx_special == GX_SPECIAL_SOCCERSS) {
		gx_type3_render_roz();
		konamigx_mixer_set_extra_bitmap(DrvType3RozBitmap);
		konamigx_mixer(0, 0, 0, 0, 0, 1, 0);
		konamigx_mixer_set_extra_bitmap(NULL);
	} else {
		konamigx_mixer(1, GXSUB_8BPP, 0, 0, 0, 0, gx_rushingheroes_hack);
	}

	GxCopyMonitor(target, konami_bitmap32, GX_TYPE4_MONITOR_WIDTH, screen_height, render_width, flip_x_offset, flip_y_offset);

	nScreenWidth = screen_width;
	nScreenHeight = screen_height;
	K056832SetGlobalOffsets(0, 16);
	K053247SetVisibleOffset(0, 16);
	K053936GP_set_offset(
	    0, (gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_SOCCERSS) ? -30 : -36,
	    gx_special == GX_SPECIAL_VSNET ? 0 : ((gx_special == GX_SPECIAL_RUSHHERO || gx_special == GX_SPECIAL_SOCCERSS) ? 1 : -1));
	K053936GP_set_visible_offset(0, 0, 16);
	K053936GP_wraparound_enable(
	    0, (gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_RUSHHERO || gx_special == GX_SPECIAL_SOCCERSS) ? 1 : 0);
}

static void gx_type4_compose_output()
{
	INT32 dual_screen = gx_type4_output_dual_screen_enabled() && nScreenWidth > GX_TYPE4_MONITOR_WIDTH;

	const bool covered = pGxType4LeftBitmap && ((dual_screen && pGxType4RightBitmap && nScreenWidth == 2 * GX_TYPE4_MONITOR_WIDTH) ||
	                                            (!dual_screen && nScreenWidth == GX_TYPE4_MONITOR_WIDTH));
	// Presentation only consumes color; each monitor clears priority before rendering.
	if (!(covered && konami_bitmap32 && konami_priority_bitmap)) {
		KonamiClearBitmaps(0);
	}

	if (!dual_screen && pGxType4LeftBitmap && nScreenWidth == GX_TYPE4_MONITOR_WIDTH) {
		memcpy(konami_bitmap32, pGxType4LeftBitmap, nScreenWidth * nScreenHeight * sizeof(UINT32));
		return;
	}

	for (INT32 y = 0; y < nScreenHeight; y++) {
		UINT32 *dst = konami_bitmap32 + (y * nScreenWidth);

		if (dual_screen) {
			if (pGxType4LeftBitmap) {
				memcpy(dst, pGxType4LeftBitmap + (y * GX_TYPE4_MONITOR_WIDTH), GX_TYPE4_MONITOR_WIDTH * sizeof(UINT32));
			}

			if (pGxType4RightBitmap) {
				memcpy(dst + GX_TYPE4_MONITOR_WIDTH, pGxType4RightBitmap + (y * GX_TYPE4_MONITOR_WIDTH),
				       GX_TYPE4_MONITOR_WIDTH * sizeof(UINT32));
			}

			for (INT32 x = 0; x < GX_TYPE4_MONITOR_SEPARATOR_WIDTH; x++) {
				dst[GX_TYPE4_MONITOR_WIDTH - 1 - x] = 0;
			}
		} else if (pGxType4LeftBitmap) {
			memcpy(dst, pGxType4LeftBitmap + (y * GX_TYPE4_MONITOR_WIDTH), GX_TYPE4_MONITOR_WIDTH * sizeof(UINT32));
		}
	}
}

// --- tbyahhoo Korean-logo tilemap fixup -------------------------------------
// The title logo (K056832 page 0) reuses a few tile codes across cells that
// the Japanese art drew identically, and leaves some cells blank (0x400).
// The Korean redraw needs those cells to differ, so we redirect them to spare
// tile codes (0x5e1..) whose pixels come from the Korean ROM patch.  The
// tilemap is generated at runtime by the game (it exists in no ROM table), so
// this can only be done here.
//
// Gating: (a) the patch writes a MAGIC pattern into spare tile 0x5fe; the
// remap fires only on an exact 64-byte match.  The original ROM holds fully
// inked X-marker placeholder tiles in the spare region, so an any-ink check
// would misfire on unpatched ROMs (it did) - an exact match cannot.
// (b) three anchor cells must hold the logo's Japanese shared codes in the
// logo's relative geometry.  The game rebuilds this tilemap per scene at
// different pages/offsets (title screen page 0, attract vocal-song title
// page 1 shifted 4 columns left), so the whole VRAM is scanned for the
// anchor pattern and the fix table is applied relative to each match.
// Cell colours are preserved.

static void tby_logo_tilemap_fix()
{
	if (gx_special != GX_SPECIAL_TBYAHHOO)
		return;

	// row/col offsets from the anchor cell (title screen: row 6, col 16)
	static const struct {
		INT32 dr, dc;
		UINT16 oldcode, newcode;
	} fix[] = {
		// shared glyph tiles the JP art reused across differing Korean cells
		{ 0, 0, 0x0408, 0x05e1 },
		{ 5, -8, 0x0417, 0x05e2 },
		{ 5, -7, 0x0425, 0x05e3 },
		{ 4, -2, 0x0428, 0x05e4 },
		{ 5, -2, 0x0428, 0x05e5 },
		{ 7, -12, 0x043a, 0x05e6 },
		{ 5, -11, 0x043a, 0x05e7 },
		// blank (0x400) cells the Korean art paints into
		{ 0, -5, 0x0400, 0x05e8 },
		{ 0, 1, 0x0400, 0x05e9 },
		{ 0, 2, 0x0400, 0x05ea },
		{ 1, -3, 0x0400, 0x05eb },
		{ 7, -5, 0x0400, 0x05ec },
		{ 10, -8, 0x0400, 0x05ed },
		{ 10, 9, 0x0400, 0x05ee },
		{ 11, -3, 0x0400, 0x05ef },
		{ 11, 3, 0x0400, 0x05f0 },
		{ 11, 4, 0x0400, 0x05f1 },
	};
	const INT32 nfix = sizeof(fix) / sizeof(fix[0]);

	const UINT8 *exp = DrvGfxROMExp0 + 0x05fe * 0x40;
	for (INT32 i = 0; i < 0x40; i++) {
		if (exp[i] != (UINT8)((3 * (i >> 3) + 5 * (i & 7) + 7) % 32))
			return;
	}

	// 16 pages of 64x32 cells, addressed as 512 linear rows of 64
	#define TBY_CODEIDX(r, c) ((((r) * 64 + (c)) * 2) + 1)
		#define TBY_CODE(r, c) (BURN_ENDIAN_SWAP_INT16(K056832GetVram(TBY_CODEIDX(r, c))) & 0x1fff)
		for (INT32 ar = 0; ar <= 512 - 12; ar++) {
		for (INT32 ac = 12; ac <= 63 - 9; ac++) {
			if (TBY_CODE(ar, ac) != 0x0408)
				continue;
			if (TBY_CODE(ar + 5, ac - 2) != 0x0428)
				continue;
			if (TBY_CODE(ar + 5, ac - 8) != 0x0417)
				continue;

			for (INT32 i = 0; i < nfix; i++) {
				INT32 idx = TBY_CODEIDX(ar + fix[i].dr, ac + fix[i].dc);
				UINT16 cur = BURN_ENDIAN_SWAP_INT16(K056832GetVram(idx));
				if ((cur & 0x1fff) != fix[i].oldcode)
					continue;
				UINT16 nw = (UINT16)((cur & 0xe000) | fix[i].newcode);
				K056832SetVram(idx, BURN_ENDIAN_SWAP_INT16(nw));
			}
		}
	}
#undef TBY_CODE
#undef TBY_CODEIDX
}

static INT32 DrvDrawInternal();

static INT32 DrvDraw()
{
	// Screen-mode changes call the frontend and must stay on the emulation thread.
	if (gx_type4_enable && gx_type4_check_screen_size())
		return 0;
	INT32 result = 0;
	{
		result = DrvDrawInternal();
	}
	return result;
}

static INT32 DrvDrawInternal()
{
	if (gx_type4_enable) {
		gx_type4_compose_output();
		KonamiBlendCopy(konami_palette32);
		return 0;
	}

	tby_logo_tilemap_fix();

	DrvPaletteUpdate();
	konamigx_prepare_frame();

	for (INT32 i = 0; i < 4; i++) {
		layer_colorbase[i] = K055555GetPaletteIndex(i) << 4;
	}
	sprite_colorbase = K055555GetPaletteIndex(4) << 5;

	// rtl/gx_main.sv obj_bank: the firmware names the list half; objscan
	// selects the completed half to display. Never infer a bank from its pixels.
	konamigx_mixer_set_spriteram_bank(gx_esc_enabled ? ((gx_objscan >> 6) & 1) : 0);
	konamigx_mixer(0, 0, 0, 0, K056832GetLastAlphaTileMixCode() << 30, 0, gx_rushingheroes_hack);

	// US/Japan cabinets use a vertically mirrored gun display (MAME FLIP_Y).
	if (gx_le2_flip_y) {
		for (INT32 y = 0; y < nScreenHeight / 2; y++) {
			for (INT32 x = 0; x < nScreenWidth; x++) {
				UINT32 &top = konami_bitmap32[y * nScreenWidth + x];
				UINT32 &bottom = konami_bitmap32[(nScreenHeight - 1 - y) * nScreenWidth + x];
				UINT32 pixel = top;
				top = bottom;
				bottom = pixel;
			}
		}
	}
	KonamiBlendCopy(DrvPalette);
	if (gx_special == GX_SPECIAL_LE2)
		BurnGunDrawTargets();

	return 0;
}

static void gx_type4_render_left_bitmap()
{
	if (!gx_type4_enable || pGxType4LeftBitmap == NULL)
		return;

	gx_type4_render_monitor(0, gx_type4_render_spriteram_bank, pGxType4LeftBitmap);
	gx_type4_left_bitmap_valid = 1;
}

static void gx_type4_render_right_bitmap()
{
	if (!gx_type4_enable || pGxType4RightBitmap == NULL)
		return;

	gx_type4_render_monitor(1, gx_type4_render_spriteram_bank, pGxType4RightBitmap);
	gx_type4_right_bitmap_valid = 1;
}

// One published sound slice remains valid until the next slice is submitted.
struct GxSoundSlice {
	INT32 index;
	INT32 *total;
	INT32 *done;
	bool runCpu;
};

static void gx_run_sound_slice(void *context)
{
	GxSoundSlice &job = *static_cast<GxSoundSlice *>(context);
	if (job.runCpu) {
		SekOpen(1);
		const INT32 cycles = (job.index + 1) * job.total[1] / 256 - job.done[1];
		job.done[1] += SekRun(cycles);
		SekClose();
	}
	const INT32 cycles = (job.index + 1) * job.total[2] / 256 - job.done[2];
	job.done[2] += timerRun(cycles);
}

static INT32 DrvFrame()
{
	if (gx_type4_check_screen_mode_change()) {
		gx_type4_check_screen_size();
		DrvDoReset();
		if (pBurnSoundOut) {
			BurnSoundClear();
		}
		return 0;
	} else if (DrvReset) {
		DrvDoReset();
	}

	memset(DrvInputs, 0xff, sizeof(DrvInputs));
	for (INT32 i = 0; i < 16; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
	}

	if (gx_special == GX_SPECIAL_LE2) {
		BurnGunMakeInputs(0, GxGunAxis[0], GxGunAxis[1]);
		BurnGunMakeInputs(1, GxGunAxis[2], GxGunAxis[3]);
	}
	gx_dasp_enable = (DrvConfig[0] & 0x01) ? 1 : 0;
	if (gx_dsp_force > 0)
		gx_dsp_force--;

	SekNewFrame();
	timerNewFrame();

	// MAME init_posthack: uncached POST program ROM accesses need wait states.
	const INT32 main_clock = ((gx_special == GX_SPECIAL_DRAGOON) && gx_post_frames < (INT32)(12 * gx_frame_rate)) ? 16000000 : 24000000;
	if (gx_post_frames < (INT32)(12 * gx_frame_rate))
		gx_post_frames++;

	INT32 nCyclesTotal[3] = { (INT32)(main_clock / gx_frame_rate), (INT32)(8000000 / gx_frame_rate), (INT32)(18432000 / gx_frame_rate) };
	INT32 nInterleave = 256;
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], 0 };
	GxSoundSlice sound_slice;

	gx_sound_buffer_pos = 0;
	gx_sound_cycles_total = nCyclesTotal[1];
	if (pBurnSoundOut) {
		BurnSoundClear();
	}

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 scanline = i & 0xff;

		gx_current_scanline = scanline;

		SekOpen(0);

		CPU_RUN(0, Sek);

		if (((gx_type4_enable && scanline < 240) || (!gx_type4_enable && scanline == 48)) && (gx_syncen & 0x40)) {
			gx_syncen &= ~0x40;
			if (((gx_irq_control & 0x82) == 0x82) || ((gx_syncen & 0x02) && (gx_irq_control & 0x80))) {
				gx_syncen &= ~0x02;
				SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
			}
		}
		if (scanline == 240) {
			if (gx_type4_enable) {
				gx_type4_render_spriteram_bank = gx_type3_spriteram_bank;
				gx_type4_current_frame ^= 1;

				if (pBurnDraw) {
					if (gx_type4_current_frame == 1 || !gx_type4_dual_screen_enabled()) {
						gx_type4_render_left_bitmap();
					} else if (nScreenWidth > GX_TYPE4_MONITOR_WIDTH) {
						gx_type4_render_right_bitmap();
					}
				}
			}

			if ((gx_syncen & 0x20) || gx_special == GX_SPECIAL_SOCCERSS || gx_special == GX_SPECIAL_VSNET ||
			    gx_special == GX_SPECIAL_RUSHHERO) {
				gx_syncen &= ~0x20;
				if (((gx_irq_control & 0x81) == 0x81) || ((gx_syncen & 0x01) && (gx_irq_control & 0x80))) {
					gx_syncen &= ~0x01;
					SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
				}
			}
			gx_irq_status |= 0x04;
		}
		if ((gx_type4_enable && scanline == 241) || (!gx_type4_enable && scanline == 248)) {
			gx_irq_status &= ~0x04;
			if (((gx_irq_control & 0x84) == 0x84) || ((gx_syncen & 0x04) && (gx_irq_control & 0x80))) {
				gx_irq_status &= ~0x80;
				gx_syncen &= ~0x04;
				SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
			}
		}
		SekClose();

		// Main slice i+1 may overlap sound slice i. Host sound I/O fences the mailbox,
		// retaining the serial driver's command/interrupt order at observable accesses.
		{
		}
		sound_slice = { i, nCyclesTotal, nCyclesDone, (gx_control & 0x40) != 0 };
		gx_run_sound_slice(&sound_slice);
	}

	{
	}
	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		gx_render_sound_segment(nBurnSoundLen);
	}

	if (pBurnDraw)
		DrvDraw();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (gx_special == GX_SPECIAL_LE2 && (nAction & ACB_VOLATILE))
		BurnGunScan();
	if (pnMin)
		*pnMin = 0x029800;

	if (nAction & ACB_VOLATILE) {
		BurnArea ba;
		memset(&ba, 0, sizeof(ba));
		ba.Data = AllRam;
		ba.nLen = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		KonamiICScan(nAction);
		gx_apply_alpha_tile_config(); // re-assert per-game alpha-tile config (KonamiICScan restores scanned K056832AlphaTileMode)
		K054539Scan(nAction, pnMin);
		DrvTms.scan(nAction);
		EEPROMScan(nAction, pnMin);

		SCAN_VAR(gx_tilebanks);
		SCAN_VAR(gx_tile_ram_bank);
		SCAN_VAR(gx_tile_ram_page);
		SCAN_VAR(gx_irq_control);
		SCAN_VAR(gx_irq_status);
		SCAN_VAR(gx_post_frames);
		SCAN_VAR(gx_syncen);
		SCAN_VAR(gx_control);
		SCAN_VAR(gx_current_scanline);
		SCAN_VAR(sound_control);
		SCAN_VAR(sound_irq_state);
		SCAN_VAR(sound_timer_irq_pending);
		SCAN_VAR(sound_int_enabled);
		SCAN_VAR(sound_int_pending);
		SCAN_VAR(sound_host_status);
		SCAN_VAR(sound_host_status_wait);
		SCAN_VAR(sound_irq_line);
		SCAN_VAR(tms_pload);
		SCAN_VAR(tms_cload);
		SCAN_VAR(tms_reset);
		SCAN_VAR(tms_hidx);
		SCAN_VAR(tms_pc_nonzero);
		SCAN_VAR(tms_program_state);
		SCAN_VAR(tms_cload_cval);
		SCAN_VAR(tms_host_pending);
		SCAN_VAR(tms_host_index);
		SCAN_VAR(tms_cycle_frac);
		SCAN_VAR(gx_dsp_force);
		SCAN_VAR(gx_type4_enable);
		SCAN_VAR(gx_type3_psac2_bank);
		SCAN_VAR(gx_type3_spriteram_bank);
		SCAN_VAR(gx_type4_render_spriteram_bank);
		SCAN_VAR(gx_type4_sprite_latch);
		SCAN_VAR(gx_type4_sprite_latch_valid);
		SCAN_VAR(gx_rushingheroes_hack);
		SCAN_VAR(gx_last_prot_op);
		SCAN_VAR(gx_last_prot_param);
		SCAN_VAR(gx_last_prot_clk);
		SCAN_VAR(gx_type4_current_frame);
		SCAN_VAR(gx_type4_last_dual_screen);
		SCAN_VAR(gx_type4_last_flip_bits);
		SCAN_VAR(gx_type4_display_hold);
		SCAN_VAR(gx_type4_left_bitmap_valid);
		SCAN_VAR(gx_type4_right_bitmap_valid);
		SCAN_VAR(gx_type4_psac_colorbase);
		SCAN_VAR(gx_type4_psac_dirty);
		SCAN_VAR(gx_type4_psac_dirty_count);
		SCAN_VAR(gx_type4_psac_tile_dirty);
		SCAN_VAR(fantjour_dma);
		SCAN_VAR(gx_esc_latch);
		SCAN_VAR(gx_objscan);
		SCAN_VAR(gx_esc_chip.state);
		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		gx_psac_cached_bank = -1;
		gx_control_write(gx_control);
		if (gx_type4_enable) {
			gx_type4_mark_all_psac_dirty();
			gx_type4_mark_all_palettes_dirty();
			gx_type4_mark_all_k053936_dirty();
			K053936_external_bitmap = pGxPsacBitmap;
			gx_type4_update_k053936_regs();
			m_k053936_0_ctrl_16 = gx_type4_ctrl_words;
			m_k053936_0_linectrl_16 = gx_type4_line_words;
			m_k053936_0_ctrl = NULL;
			m_k053936_0_linectrl = NULL;
			K053936GP_enable(0, 1);
			K053936GP_wraparound_enable(
			    0, (gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_RUSHHERO || gx_special == GX_SPECIAL_SOCCERSS) ? 1 : 0);
			K053936GP_set_offset(
			    0, (gx_special == GX_SPECIAL_VSNET || gx_special == GX_SPECIAL_SOCCERSS) ? -30 : -36,
			    gx_special == GX_SPECIAL_VSNET ? 0 : ((gx_special == GX_SPECIAL_RUSHHERO || gx_special == GX_SPECIAL_SOCCERSS) ? 1 : -1));
			K053936GP_set_visible_offset(0, 0, 16);
			K053936GP_set_source_size(0, gx_special == GX_SPECIAL_SOCCERSS ? 4096 : 2048, gx_special == GX_SPECIAL_SOCCERSS ? 4096 : 2048);
			K053936GP_set_colorbase(0, 0);
		}
		DrvRecalc = 1;
	}

	return 0;
}

static struct BurnRomInfo konamigxRomDesc[] = {
	{ "300a01.34k", 0x020000, 0xd5fa95f5, BRF_PRG | BRF_BIOS },
};

STD_ROM_PICK(konamigx)

STD_ROM_FN(konamigx)

static struct BurnRomInfo racinfrcRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "250eac02.34n", 0x00080000, 0xdf2a48c0, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "250eac03.31n", 0x00080000, 0x6da86a4d, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "250a04.34s",   0x00200000, 0x45e4d43c, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "250a05.31s",   0x00200000, 0xa235af3e, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "250a06.8p",    0x00020000, 0x2d0a3ff1, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "250a07.6p",    0x00020000, 0x612b670a, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "250a15.19y",   0x00100000, 0x60abc472, 3 | BRF_GRA           }, //  7 k056832
	{ "250a14.21y",   0x00080000, 0xd14abf98, 3 | BRF_GRA           }, //  8 k056832

	{ "250a12.26y",   0x00200000, 0xe4ca3cff, 4 | BRF_GRA           }, //  9 k055673
	{ "250a10.31y",   0x00200000, 0x75c02d12, 4 | BRF_GRA           }, // 10 k055673
	{ "250a13.24y",   0x00200000, 0x7aeef929, 4 | BRF_GRA           }, // 11 k055673
	{ "250a11.28y",   0x00200000, 0xdfbce309, 4 | BRF_GRA           }, // 12 k055673
	{ "250a08.36y",   0x00200000, 0x25ff6414, 4 | BRF_GRA           }, // 13 k055673

	{ "250a20.10d",   0x00100000, 0x26a2fcaf, 7 | BRF_GRA           }, // 14 gfx3
	{ "250a21.7d",    0x00100000, 0x370d7771, 7 | BRF_GRA           }, // 15 gfx3
	{ "250a22.5d",    0x00100000, 0xc66a7775, 7 | BRF_GRA           }, // 16 gfx3

	{ "250a24.10h",   0x00100000, 0xa14547da, 8 | BRF_GRA           }, // 17 gfx4
	{ "250a25.7h",    0x00100000, 0x58310501, 8 | BRF_GRA           }, // 18 gfx4
	{ "250a26.5h",    0x00100000, 0xf72e4cbe, 8 | BRF_GRA           }, // 19 gfx4

	{ "250a17.14y",   0x00200000, 0xadefa079, 5 | BRF_SND           }, // 20 k054539
	{ "250a18.12y",   0x00200000, 0x8014a2eb, 5 | BRF_SND           }, // 21 k054539

	{ "racinfrc.nv",  0x00000080, 0xdc88c693, 6 | BRF_PRG | BRF_ESS }, // 22 eeprom
};

STD_ROM_PICK(racinfrc)

STD_ROM_FN(racinfrc)

static struct BurnRomInfo racinfrcuRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "250uab02.34n", 0x00080000, 0x315040c6, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "250uab03.31n", 0x00080000, 0x171134ab, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "250a04.34s",   0x00200000, 0x45e4d43c, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "250a05.31s",   0x00200000, 0xa235af3e, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "250a06.8p",    0x00020000, 0x2d0a3ff1, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "250a07.6p",    0x00020000, 0x612b670a, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "250a15.19y",   0x00100000, 0x60abc472, 3 | BRF_GRA           }, //  7 k056832
	{ "250a14.21y",   0x00080000, 0xd14abf98, 3 | BRF_GRA           }, //  8 k056832

	{ "250a12.26y",   0x00200000, 0xe4ca3cff, 4 | BRF_GRA           }, //  9 k055673
	{ "250a10.31y",   0x00200000, 0x75c02d12, 4 | BRF_GRA           }, // 10 k055673
	{ "250a13.24y",   0x00200000, 0x7aeef929, 4 | BRF_GRA           }, // 11 k055673
	{ "250a11.28y",   0x00200000, 0xdfbce309, 4 | BRF_GRA           }, // 12 k055673
	{ "250a08.36y",   0x00200000, 0x25ff6414, 4 | BRF_GRA           }, // 13 k055673

	{ "250a20.10d",   0x00100000, 0x26a2fcaf, 7 | BRF_GRA           }, // 14 gfx3
	{ "250a21.7d",    0x00100000, 0x370d7771, 7 | BRF_GRA           }, // 15 gfx3
	{ "250a22.5d",    0x00100000, 0xc66a7775, 7 | BRF_GRA           }, // 16 gfx3

	{ "250a24.10h",   0x00100000, 0xa14547da, 8 | BRF_GRA           }, // 17 gfx4
	{ "250a25.7h",    0x00100000, 0x58310501, 8 | BRF_GRA           }, // 18 gfx4
	{ "250a26.5h",    0x00100000, 0xf72e4cbe, 8 | BRF_GRA           }, // 19 gfx4

	{ "250a17.14y",   0x00200000, 0xadefa079, 5 | BRF_SND           }, // 20 k054539
	{ "250a18.12y",   0x00200000, 0x8014a2eb, 5 | BRF_SND           }, // 21 k054539

	{ "racinfrcu.nv", 0x00000080, 0x369e1a84, 6 | BRF_PRG | BRF_ESS }, // 22 eeprom
};

STD_ROM_PICK(racinfrcu)

STD_ROM_FN(racinfrcu)

static struct BurnRomInfo opengolfRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "218eae02.34n", 0x00080000, 0x6f99ddb0, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "218eae03.31n", 0x00080000, 0xc173cf3c, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "218eae02.34n", 0x00080000, 0x6f99ddb0, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "218eae03.31n", 0x00080000, 0xc173cf3c, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu
	{ "218a04.34s",   0x00080000, 0xe50043a7, 1 | BRF_PRG | BRF_ESS }, //  5 maincpu
	{ "218a05.31s",   0x00080000, 0x46c6b5d3, 1 | BRF_PRG | BRF_ESS }, //  6 maincpu

	{ "218a06.8p",    0x00020000, 0x6755ccf9, 2 | BRF_PRG | BRF_ESS }, //  7 soundcpu
	{ "218a07.6p",    0x00020000, 0x221e5293, 2 | BRF_PRG | BRF_ESS }, //  8 soundcpu

	{ "218a15.19y",   0x00200000, 0x78ddc8af, 3 | BRF_GRA           }, //  9 k056832
	{ "218a16.16y",   0x00080000, 0xa41a3ec8, 3 | BRF_GRA           }, // 10 k056832
	{ "218a14.22y",   0x00100000, 0x508cd75e, 3 | BRF_GRA           }, // 11 k056832

	{ "218a12.26y",   0x00200000, 0x83158653, 4 | BRF_GRA           }, // 12 k055673
	{ "218a10.31y",   0x00200000, 0x059bfee3, 4 | BRF_GRA           }, // 13 k055673
	{ "218a08.35y",   0x00200000, 0x5b7098f3, 4 | BRF_GRA           }, // 14 k055673
	{ "218a13.24y",   0x00100000, 0xb9ffd12a, 4 | BRF_GRA           }, // 15 k055673
	{ "218a11.28y",   0x00100000, 0xb57231e5, 4 | BRF_GRA           }, // 16 k055673
	{ "218a09.33y",   0x00100000, 0x13627443, 4 | BRF_GRA           }, // 17 k055673

	{ "218a20.10d",   0x00200000, 0xf0ac2d6f, 7 | BRF_GRA           }, // 18 gfx3
	{ "218a21.7d",    0x00200000, 0xcb15122a, 7 | BRF_GRA           }, // 19 gfx3
	{ "218a22.5d",    0x00200000, 0x1b08d7dc, 7 | BRF_GRA           }, // 20 gfx3
	{ "218a23.3d",    0x00200000, 0x1e4224b5, 7 | BRF_GRA           }, // 21 gfx3

	{ "218a24.10h",   0x00200000, 0xe938d96a, 8 | BRF_GRA           }, // 22 gfx4
	{ "218a25.7h",    0x00200000, 0x11600c2d, 8 | BRF_GRA           }, // 23 gfx4
	{ "218a26.5h",    0x00200000, 0xb37e4b7a, 8 | BRF_GRA           }, // 24 gfx4

	{ "218a17.14y",   0x00200000, 0x0b525127, 5 | BRF_SND           }, // 25 k054539
	{ "218a18.12y",   0x00100000, 0x98ec4cfb, 5 | BRF_SND           }, // 26 k054539

	{ "opengolf.nv",  0x00000080, 0xd49bf7c3, 6 | BRF_PRG | BRF_ESS }, // 27 eeprom
};

STD_ROM_PICK(opengolf)

STD_ROM_FN(opengolf)

static struct BurnRomInfo opengolf2RomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "218ead02.34n", 0x00080000, 0xeeb58816, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "218ead03.31n", 0x00080000, 0x5c36f84c, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "218ead02.34n", 0x00080000, 0xeeb58816, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "218ead03.31n", 0x00080000, 0x5c36f84c, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu
	{ "218a04.34s",   0x00080000, 0xe50043a7, 1 | BRF_PRG | BRF_ESS }, //  5 maincpu
	{ "218a05.31s",   0x00080000, 0x46c6b5d3, 1 | BRF_PRG | BRF_ESS }, //  6 maincpu

	{ "218a06.8p",    0x00020000, 0x6755ccf9, 2 | BRF_PRG | BRF_ESS }, //  7 soundcpu
	{ "218a07.6p",    0x00020000, 0x221e5293, 2 | BRF_PRG | BRF_ESS }, //  8 soundcpu

	{ "218a15.19y",   0x00200000, 0x78ddc8af, 3 | BRF_GRA           }, //  9 k056832
	{ "218a16.16y",   0x00080000, 0xa41a3ec8, 3 | BRF_GRA           }, // 10 k056832
	{ "218a14.22y",   0x00100000, 0x508cd75e, 3 | BRF_GRA           }, // 11 k056832

	{ "218a12.26y",   0x00200000, 0x83158653, 4 | BRF_GRA           }, // 12 k055673
	{ "218a10.31y",   0x00200000, 0x059bfee3, 4 | BRF_GRA           }, // 13 k055673
	{ "218a08.35y",   0x00200000, 0x5b7098f3, 4 | BRF_GRA           }, // 14 k055673
	{ "218a13.24y",   0x00100000, 0xb9ffd12a, 4 | BRF_GRA           }, // 15 k055673
	{ "218a11.28y",   0x00100000, 0xb57231e5, 4 | BRF_GRA           }, // 16 k055673
	{ "218a09.33y",   0x00100000, 0x13627443, 4 | BRF_GRA           }, // 17 k055673

	{ "218a20.10d",   0x00200000, 0xf0ac2d6f, 7 | BRF_GRA           }, // 18 gfx3
	{ "218a21.7d",    0x00200000, 0xcb15122a, 7 | BRF_GRA           }, // 19 gfx3
	{ "218a22.5d",    0x00200000, 0x1b08d7dc, 7 | BRF_GRA           }, // 20 gfx3
	{ "218a23.3d",    0x00200000, 0x1e4224b5, 7 | BRF_GRA           }, // 21 gfx3

	{ "218a24.10h",   0x00200000, 0xe938d96a, 8 | BRF_GRA           }, // 22 gfx4
	{ "218a25.7h",    0x00200000, 0x11600c2d, 8 | BRF_GRA           }, // 23 gfx4
	{ "218a26.5h",    0x00200000, 0xb37e4b7a, 8 | BRF_GRA           }, // 24 gfx4

	{ "218a17.14y",   0x00200000, 0x0b525127, 5 | BRF_SND           }, // 25 k054539
	{ "218a18.12y",   0x00100000, 0x98ec4cfb, 5 | BRF_SND           }, // 26 k054539

	{ "opengolf2.nv", 0x00000080, 0xc09fc0e6, 6 | BRF_PRG | BRF_ESS }, // 27 eeprom
};

STD_ROM_PICK(opengolf2)

STD_ROM_FN(opengolf2)

static struct BurnRomInfo ggreats2RomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "218jac02.34n", 0x00080000, 0xe4d47f92, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "218jac03.31n", 0x00080000, 0xec10c0b2, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "218a04.34s",   0x00080000, 0xe50043a7, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "218a05.31s",   0x00080000, 0x46c6b5d3, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "218a06.8p",    0x00020000, 0x6755ccf9, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "218a07.6p",    0x00020000, 0x221e5293, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "218a15.19y",   0x00200000, 0x78ddc8af, 3 | BRF_GRA           }, //  7 k056832
	{ "218a16.16y",   0x00080000, 0xa41a3ec8, 3 | BRF_GRA           }, //  8 k056832
	{ "218a14.22y",   0x00100000, 0x508cd75e, 3 | BRF_GRA           }, //  9 k056832

	{ "218a12.26y",   0x00200000, 0x83158653, 4 | BRF_GRA           }, // 10 k055673
	{ "218a10.31y",   0x00200000, 0x059bfee3, 4 | BRF_GRA           }, // 11 k055673
	{ "218a08.35y",   0x00200000, 0x5b7098f3, 4 | BRF_GRA           }, // 12 k055673
	{ "218a13.24y",   0x00100000, 0xb9ffd12a, 4 | BRF_GRA           }, // 13 k055673
	{ "218a11.28y",   0x00100000, 0xb57231e5, 4 | BRF_GRA           }, // 14 k055673
	{ "218a09.33y",   0x00100000, 0x13627443, 4 | BRF_GRA           }, // 15 k055673

	{ "218a20.10d",   0x00200000, 0xf0ac2d6f, 7 | BRF_GRA           }, // 16 gfx3
	{ "218a21.7d",    0x00200000, 0xcb15122a, 7 | BRF_GRA           }, // 17 gfx3
	{ "218a22.5d",    0x00200000, 0x1b08d7dc, 7 | BRF_GRA           }, // 18 gfx3
	{ "218a23.3d",    0x00200000, 0x1e4224b5, 7 | BRF_GRA           }, // 19 gfx3

	{ "218a24.10h",   0x00200000, 0xe938d96a, 8 | BRF_GRA           }, // 20 gfx4
	{ "218a25.7h",    0x00200000, 0x11600c2d, 8 | BRF_GRA           }, // 21 gfx4
	{ "218a26.5h",    0x00200000, 0xb37e4b7a, 8 | BRF_GRA           }, // 22 gfx4

	{ "218a17.14y",   0x00200000, 0x0b525127, 5 | BRF_SND           }, // 23 k054539
	{ "218a18.12y",   0x00100000, 0x98ec4cfb, 5 | BRF_SND           }, // 24 k054539

	{ "ggreats2.nv",  0x00000080, 0x4db10b5c, 6 | BRF_PRG | BRF_ESS }, // 25 eeprom
};

STD_ROM_PICK(ggreats2)

STD_ROM_FN(ggreats2)

static struct BurnRomInfo le2RomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "312eaa05.26b", 0x00020000, 0x875f6561, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "312eaa04.28b", 0x00020000, 0xd5fb8d30, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "312eaa03.30b", 0x00020000, 0xcfe07036, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "312eaa02.33b", 0x00020000, 0x5094b965, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "312b06.9c",    0x00020000, 0xa6f62539, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "312b07.7c",    0x00020000, 0x1aa19c41, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "312a14.17h",   0x00200000, 0xdc862f19, 3 | BRF_GRA           }, //  7 k056832
	{ "312a12.22h",   0x00200000, 0x98c04ddd, 3 | BRF_GRA           }, //  8 k056832
	{ "312a15.15h",   0x00200000, 0x516f2941, 3 | BRF_GRA           }, //  9 k056832
	{ "312a13.20h",   0x00200000, 0x16e5fdaa, 3 | BRF_GRA           }, // 10 k056832

	{ "312a11.25g",   0x00200000, 0x5f474357, 4 | BRF_GRA           }, // 11 k055673
	{ "312a10.28g",   0x00200000, 0x3c570d04, 4 | BRF_GRA           }, // 12 k055673
	{ "312a09.30g",   0x00200000, 0xb2c5d6d5, 4 | BRF_GRA           }, // 13 k055673
	{ "312a08.33g",   0x00200000, 0x29015d56, 4 | BRF_GRA           }, // 14 k055673

	{ "312a17.9g",    0x00200000, 0xed101448, 5 | BRF_SND           }, // 15 k054539
	{ "312a18.7g",    0x00100000, 0x5717abd7, 5 | BRF_SND           }, // 16 k054539

	{ "le2.nv",       0x00000080, 0xfec3bc2e, 6 | BRF_PRG | BRF_ESS }, // 17 eeprom
};

STD_ROM_PICK(le2)

STD_ROM_FN(le2)

static struct BurnRomInfo le2uRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "312uaa05.26b", 0x00020000, 0x973aa500, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "312uaa04.28b", 0x00020000, 0xcba39552, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "312uaa03.30b", 0x00020000, 0x20bc94e6, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "312uaa02.33b", 0x00020000, 0x04f3bd9e, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "312a06.9c",    0x00020000, 0xff6f2cd4, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "312a07.7c",    0x00020000, 0x3d31e989, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "312a14.17h",   0x00200000, 0xdc862f19, 3 | BRF_GRA           }, //  7 k056832
	{ "312a12.22h",   0x00200000, 0x98c04ddd, 3 | BRF_GRA           }, //  8 k056832
	{ "312a15.15h",   0x00200000, 0x516f2941, 3 | BRF_GRA           }, //  9 k056832
	{ "312a13.20h",   0x00200000, 0x16e5fdaa, 3 | BRF_GRA           }, // 10 k056832

	{ "312a11.25g",   0x00200000, 0x5f474357, 4 | BRF_GRA           }, // 11 k055673
	{ "312a10.28g",   0x00200000, 0x3c570d04, 4 | BRF_GRA           }, // 12 k055673
	{ "312a09.30g",   0x00200000, 0xb2c5d6d5, 4 | BRF_GRA           }, // 13 k055673
	{ "312a08.33g",   0x00200000, 0x29015d56, 4 | BRF_GRA           }, // 14 k055673

	{ "312a17.9g",    0x00200000, 0xed101448, 5 | BRF_SND           }, // 15 k054539
	{ "312a18.7g",    0x00100000, 0x5717abd7, 5 | BRF_SND           }, // 16 k054539

	{ "le2u.nv",      0x00000080, 0xd46b3878, 6 | BRF_PRG | BRF_ESS }, // 17 eeprom
};

STD_ROM_PICK(le2u)

STD_ROM_FN(le2u)

static struct BurnRomInfo le2jRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "312jaa05.26b", 0x00020000, 0x7eaa6ce2, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "312jaa04.28b", 0x00020000, 0xc3d19ddc, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "312jaa03.30b", 0x00020000, 0x9ad95a7c, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "312jaa02.33b", 0x00020000, 0xe971cb87, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "312b06.9c",    0x00020000, 0xa6f62539, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "312b07.7c",    0x00020000, 0x1aa19c41, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "312a14.17h",   0x00200000, 0xdc862f19, 3 | BRF_GRA           }, //  7 k056832
	{ "312a12.22h",   0x00200000, 0x98c04ddd, 3 | BRF_GRA           }, //  8 k056832
	{ "312a15.15h",   0x00200000, 0x516f2941, 3 | BRF_GRA           }, //  9 k056832
	{ "312a13.20h",   0x00200000, 0x16e5fdaa, 3 | BRF_GRA           }, // 10 k056832

	{ "312a11.25g",   0x00200000, 0x5f474357, 4 | BRF_GRA           }, // 11 k055673
	{ "312a10.28g",   0x00200000, 0x3c570d04, 4 | BRF_GRA           }, // 12 k055673
	{ "312a09.30g",   0x00200000, 0xb2c5d6d5, 4 | BRF_GRA           }, // 13 k055673
	{ "312a08.33g",   0x00200000, 0x29015d56, 4 | BRF_GRA           }, // 14 k055673

	{ "312a17.9g",    0x00200000, 0xed101448, 5 | BRF_SND           }, // 15 k054539
	{ "312a18.7g",    0x00100000, 0x5717abd7, 5 | BRF_SND           }, // 16 k054539

	{ "le2j.nv",      0x00000080, 0xf6790425, 6 | BRF_PRG | BRF_ESS }, // 17 eeprom
};

STD_ROM_PICK(le2j)

STD_ROM_FN(le2j)

static struct BurnRomInfo fantjourRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "321eaa02.21b", 0x00080000, 0xafaf9d17, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "321eaa04.27b", 0x00080000, 0xb2cfe225, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "321b06.9c",    0x00020000, 0xda810554, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "321b07.7c",    0x00020000, 0xc47634c0, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "321b14.17h",   0x00200000, 0x437d0057, 3 | BRF_GRA           }, //  5 k056832
	{ "321b12.13g",   0x00080000, 0x5f9edfa0, 3 | BRF_GRA           }, //  6 k056832

	{ "321b11.25g",   0x00200000, 0xc6e2e74d, 4 | BRF_GRA           }, //  7 k055673
	{ "321b10.28g",   0x00200000, 0xea9f8c48, 4 | BRF_GRA           }, //  8 k055673
	{ "321b09.30g",   0x00100000, 0x94add237, 4 | BRF_GRA           }, //  9 k055673

	{ "321b17.9g",    0x00200000, 0xb3e8d5d8, 5 | BRF_SND           }, // 10 k054539
	{ "321b18.7g",    0x00200000, 0x2c561ad0, 5 | BRF_SND           }, // 11 k054539

	{ "fantjour.nv",  0x00000080, 0x35b7d8e1, 6 | BRF_PRG | BRF_ESS }, // 12 eeprom
};

STD_ROM_PICK(fantjour)

STD_ROM_FN(fantjour)

static struct BurnRomInfo fantjouraRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "321aaa02.21b", 0x00080000, 0x5efb62f8, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "321aaa04.27b", 0x00080000, 0x507becce, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "321b06.9c",    0x00020000, 0xda810554, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "321b07.7c",    0x00020000, 0xc47634c0, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "321b14.17h",   0x00200000, 0x437d0057, 3 | BRF_GRA           }, //  5 k056832
	{ "321b12.13g",   0x00080000, 0x5f9edfa0, 3 | BRF_GRA           }, //  6 k056832

	{ "321b11.25g",   0x00200000, 0xc6e2e74d, 4 | BRF_GRA           }, //  7 k055673
	{ "321b10.28g",   0x00200000, 0xea9f8c48, 4 | BRF_GRA           }, //  8 k055673
	{ "321b09.30g",   0x00100000, 0x94add237, 4 | BRF_GRA           }, //  9 k055673

	{ "321b17.9g",    0x00200000, 0xb3e8d5d8, 5 | BRF_SND           }, // 10 k054539
	{ "321b18.7g",    0x00200000, 0x2c561ad0, 5 | BRF_SND           }, // 11 k054539

	{ "fantjoura.nv", 0x00000080, 0xd13b1ec1, 6 | BRF_PRG | BRF_ESS }, // 12 eeprom
};

STD_ROM_PICK(fantjoura)

STD_ROM_FN(fantjoura)

static struct BurnRomInfo gokuparoRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "321jad02.21b", 0x00080000, 0xc2e548c0, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "321jad04.27b", 0x00080000, 0x916a7951, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "321b06.9c",    0x00020000, 0xda810554, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "321b07.7c",    0x00020000, 0xc47634c0, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "321b14.17h",   0x00200000, 0x437d0057, 3 | BRF_GRA           }, //  5 k056832
	{ "321b12.13g",   0x00080000, 0x5f9edfa0, 3 | BRF_GRA           }, //  6 k056832

	{ "321b11.25g",   0x00200000, 0xc6e2e74d, 4 | BRF_GRA           }, //  7 k055673
	{ "321b10.28g",   0x00200000, 0xea9f8c48, 4 | BRF_GRA           }, //  8 k055673
	{ "321b09.30g",   0x00100000, 0x94add237, 4 | BRF_GRA           }, //  9 k055673

	{ "321b17.9g",    0x00200000, 0xb3e8d5d8, 5 | BRF_SND           }, // 10 k054539
	{ "321b18.7g",    0x00200000, 0x2c561ad0, 5 | BRF_SND           }, // 11 k054539

	{ "gokuparo.nv",  0x00000080, 0x15c0f2d9, 6 | BRF_PRG | BRF_ESS }, // 12 eeprom
};

STD_ROM_PICK(gokuparo)

STD_ROM_FN(gokuparo)

static struct BurnRomInfo crzcrossRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "315eaa02.31b", 0x00080000, 0x9c0faa4b, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "315eaa04.27b", 0x00080000, 0xc89dd3e5, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "315a06.9c",    0x00020000, 0x06580a9f, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "315a07.7c",    0x00020000, 0x431c58f3, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "315a14.17h",   0x00080000, 0x0ab731e0, 3 | BRF_GRA           }, //  5 k056832
	{ "315a12.13g",   0x00200000, 0x3047b8d2, 3 | BRF_GRA           }, //  6 k056832

	{ "315a11.25g",   0x00200000, 0xb8a99c29, 4 | BRF_GRA           }, //  7 k055673
	{ "315a10.28g",   0x00200000, 0x77d175dc, 4 | BRF_GRA           }, //  8 k055673
	{ "315a09.30g",   0x00100000, 0x82580329, 4 | BRF_GRA           }, //  9 k055673

	{ "315a17.9g",    0x00200000, 0xea763d61, 5 | BRF_SND           }, // 10 k054539
	{ "315a18.7g",    0x00200000, 0x6e416cee, 5 | BRF_SND           }, // 11 k054539

	{ "crzcross.nv",  0x00000080, 0x446f178c, 6 | BRF_PRG | BRF_ESS }, // 12 eeprom
};

STD_ROM_PICK(crzcross)

STD_ROM_FN(crzcross)

static struct BurnRomInfo puzldamaRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "315jaa02.31b", 0x00080000, 0xe0a35c7d, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "315jaa04.27b", 0x00080000, 0xabe4f0e7, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "315a06.9c",    0x00020000, 0x06580a9f, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "315a07.7c",    0x00020000, 0x431c58f3, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "315a14.17h",   0x00080000, 0x0ab731e0, 3 | BRF_GRA           }, //  5 k056832
	{ "315a12.13g",   0x00200000, 0x3047b8d2, 3 | BRF_GRA           }, //  6 k056832

	{ "315a11.25g",   0x00200000, 0xb8a99c29, 4 | BRF_GRA           }, //  7 k055673
	{ "315a10.28g",   0x00200000, 0x77d175dc, 4 | BRF_GRA           }, //  8 k055673
	{ "315a09.30g",   0x00100000, 0x82580329, 4 | BRF_GRA           }, //  9 k055673

	{ "315a17.9g",    0x00200000, 0xea763d61, 5 | BRF_SND           }, // 10 k054539
	{ "315a18.7g",    0x00200000, 0x6e416cee, 5 | BRF_SND           }, // 11 k054539

	{ "puzldama.nv",  0x00000080, 0xbda98b84, 6 | BRF_PRG | BRF_ESS }, // 12 eeprom
};

STD_ROM_PICK(puzldama)

STD_ROM_FN(puzldama)

static struct BurnRomInfo tbyahhooRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "424jaa02.31b", 0x00080000, 0x0416ad78, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "424jaa04.27b", 0x00080000, 0xbcbe0e40, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "424a06.9c",    0x00020000, 0xa4760e14, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "424a07.7c",    0x00020000, 0xfa90d7e2, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "424a14.17h",   0x00200000, 0xb1d9fce8, 3 | BRF_GRA           }, //  5 k056832
	{ "424a12.13g",   0x00080000, 0x7f9cb8b1, 3 | BRF_GRA           }, //  6 k056832

	{ "424a11.25g",   0x00200000, 0x29592688, 4 | BRF_GRA           }, //  7 k055673
	{ "424a10.28g",   0x00200000, 0xcf24e5e3, 4 | BRF_GRA           }, //  8 k055673
	{ "424a09.30g",   0x00100000, 0xdaa07224, 4 | BRF_GRA           }, //  9 k055673

	{ "424a17.9g",    0x00200000, 0xe9dd9692, 5 | BRF_SND           }, // 10 k054539
	{ "424a18.7g",    0x00200000, 0x0f0d9f3a, 5 | BRF_SND           }, // 11 k054539

	{ "tbyahhoo.nv",  0x00000080, 0x1e6fa2f8, 6 | BRF_PRG | BRF_ESS }, // 12 eeprom
};

STD_ROM_PICK(tbyahhoo)

STD_ROM_FN(tbyahhoo)

static struct BurnRomInfo mtwinbeeRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "424eaa02.31b", 0x00080000, 0x34659905, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "424eaa04.27b", 0x00080000, 0xf42d3139, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "424a06.9c",    0x00020000, 0xa4760e14, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "424a07.7c",    0x00020000, 0xfa90d7e2, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "424a14.17h",   0x00200000, 0xb1d9fce8, 3 | BRF_GRA           }, //  5 k056832
	{ "424a12.13g",   0x00080000, 0x7f9cb8b1, 3 | BRF_GRA           }, //  6 k056832

	{ "424a11.25g",   0x00200000, 0x29592688, 4 | BRF_GRA           }, //  7 k055673
	{ "424a10.28g",   0x00200000, 0xcf24e5e3, 4 | BRF_GRA           }, //  8 k055673
	{ "424a09.30g",   0x00100000, 0xdaa07224, 4 | BRF_GRA           }, //  9 k055673

	{ "424a17.9g",    0x00200000, 0xe9dd9692, 5 | BRF_SND           }, // 10 k054539
	{ "424a18.7g",    0x00200000, 0x0f0d9f3a, 5 | BRF_SND           }, // 11 k054539

	{ "mtwinbee.nv",  0x00000080, 0x942b4323, 6 | BRF_PRG | BRF_ESS }, // 12 eeprom
};

STD_ROM_PICK(mtwinbee)

STD_ROM_FN(mtwinbee)

static struct BurnRomInfo tkmmpzdmRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "515jab02.31b", 0x00080000, 0x60d4d577, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "515jab03.27b", 0x00080000, 0xc383413d, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "515a04.9c",    0x00020000, 0xa9b7bb45, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "515a05.7c",    0x00020000, 0xdea4ca2f, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "515a11.17h",   0x00100000, 0x8689852d, 3 | BRF_GRA           }, //  5 k056832
	{ "515a12.13c",   0x00080000, 0x6936f94a, 3 | BRF_GRA           }, //  6 k056832

	{ "515a10.25g",   0x00200000, 0xe6e7ab7e, 4 | BRF_GRA           }, //  7 k055673
	{ "515a08.28g",   0x00200000, 0x5613daea, 4 | BRF_GRA           }, //  8 k055673
	{ "515a09.18h",   0x00200000, 0x28ffdb48, 4 | BRF_GRA           }, //  9 k055673
	{ "515a07.27g",   0x00200000, 0x246e6cb1, 4 | BRF_GRA           }, // 10 k055673
	{ "515a06.30g",   0x00200000, 0x13b7b953, 4 | BRF_GRA           }, // 11 k055673

	{ "515a13.9g",    0x00200000, 0x4b066b00, 5 | BRF_SND           }, // 12 k054539
	{ "515a14.7g",    0x00200000, 0x128cc944, 5 | BRF_SND           }, // 13 k054539

	{ "tkmmpzdm.nv",  0x00000080, 0x850ab8c4, 6 | BRF_PRG | BRF_ESS }, // 14 eeprom
};

STD_ROM_PICK(tkmmpzdm)

STD_ROM_FN(tkmmpzdm)

static struct BurnRomInfo dragoonaRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "417aab02.31b", 0x00080000, 0x0421c19c, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "417aab03.27b", 0x00080000, 0x813dd8d5, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "417a04.26c",   0x00100000, 0xdc574747, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "417a05.23c",   0x00100000, 0x2ee2c587, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "417a06.9c",    0x00020000, 0x8addbbee, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "417a07.7c",    0x00020000, 0xc1fd7584, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "417a16.17h",   0x00200000, 0x88b2213b, 3 | BRF_GRA           }, //  7 k056832

	{ "417a15.25g",   0x00200000, 0x83bccd01, 4 | BRF_GRA           }, //  8 k055673
	{ "417a11.28g",   0x00200000, 0x624a7c4c, 4 | BRF_GRA           }, //  9 k055673
	{ "417a14.18h",   0x00200000, 0xfbf551f1, 4 | BRF_GRA           }, // 10 k055673
	{ "417a10.27g",   0x00200000, 0x18fde49f, 4 | BRF_GRA           }, // 11 k055673
	{ "417a13.20h",   0x00200000, 0xd2e3959d, 4 | BRF_GRA           }, // 12 k055673
	{ "417a09.30g",   0x00200000, 0xb5653e24, 4 | BRF_GRA           }, // 13 k055673
	{ "417a12.23h",   0x00200000, 0x25496115, 4 | BRF_GRA           }, // 14 k055673
	{ "417a08.33g",   0x00200000, 0x801e9d93, 4 | BRF_GRA           }, // 15 k055673

	{ "417a17.9g",    0x00200000, 0x88d47dfd, 5 | BRF_SND           }, // 16 k054539

	{ "dragoona.nv",  0x00000080, 0x7980ad2b, 6 | BRF_PRG | BRF_ESS }, // 17 eeprom
};

STD_ROM_PICK(dragoona)

STD_ROM_FN(dragoona)

static struct BurnRomInfo dragoonjRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "417jaa02.31b", 0x00080000, 0x533cbbd5, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "417jaa03.27b", 0x00080000, 0x8e1f883f, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "417a04.26c",   0x00100000, 0xdc574747, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "417a05.23c",   0x00100000, 0x2ee2c587, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "417a06.9c",    0x00020000, 0x8addbbee, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "417a07.7c",    0x00020000, 0xc1fd7584, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "417a16.17h",   0x00200000, 0x88b2213b, 3 | BRF_GRA           }, //  7 k056832

	{ "417a15.25g",   0x00200000, 0x83bccd01, 4 | BRF_GRA           }, //  8 k055673
	{ "417a11.28g",   0x00200000, 0x624a7c4c, 4 | BRF_GRA           }, //  9 k055673
	{ "417a14.18h",   0x00200000, 0xfbf551f1, 4 | BRF_GRA           }, // 10 k055673
	{ "417a10.27g",   0x00200000, 0x18fde49f, 4 | BRF_GRA           }, // 11 k055673
	{ "417a13.20h",   0x00200000, 0xd2e3959d, 4 | BRF_GRA           }, // 12 k055673
	{ "417a09.30g",   0x00200000, 0xb5653e24, 4 | BRF_GRA           }, // 13 k055673
	{ "417a12.23h",   0x00200000, 0x25496115, 4 | BRF_GRA           }, // 14 k055673
	{ "417a08.33g",   0x00200000, 0x801e9d93, 4 | BRF_GRA           }, // 15 k055673

	{ "417a17.9g",    0x00200000, 0x88d47dfd, 5 | BRF_SND           }, // 16 k054539

	{ "dragoonj.nv",  0x00000080, 0xcbe16082, 6 | BRF_PRG | BRF_ESS }, // 17 eeprom
};

STD_ROM_PICK(dragoonj)

STD_ROM_FN(dragoonj)

static struct BurnRomInfo sexyparoRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "533jaa02.31b", 0x00080000, 0xb8030abc, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "533jaa03.27b", 0x00080000, 0x4a95e80d, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "533a08.9c",    0x00020000, 0x06d14cff, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "533a09.7c",    0x00020000, 0xa93c6f0b, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "533a19.17h",   0x00200000, 0x3ec1843e, 3 | BRF_GRA           }, //  5 k056832
	{ "533a18.13g",   0x00080000, 0xd3e0d058, 3 | BRF_GRA           }, //  6 k056832

	{ "533a17.25g",   0x00200000, 0x9947af57, 4 | BRF_GRA           }, //  7 k055673
	{ "533a13.28g",   0x00200000, 0x58f1fc38, 4 | BRF_GRA           }, //  8 k055673
	{ "533a11.30g",   0x00200000, 0x983105e1, 4 | BRF_GRA           }, //  9 k055673

	{ "533a22.9g",    0x00200000, 0x97233814, 5 | BRF_SND           }, // 10 k054539
	{ "533a23.7g",    0x00200000, 0x1bb7552b, 5 | BRF_SND           }, // 11 k054539
};

STD_ROM_PICK(sexyparo)

STD_ROM_FN(sexyparo)

static struct BurnRomInfo sexyparoaRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "533aaa02.31b", 0x00080000, 0x4fdc4298, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "533aaa03.27b", 0x00080000, 0x9c5e07cb, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "533aaa08.9c",  0x00020000, 0xf2e2c963, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "533aaa09.7c",  0x00020000, 0x49086451, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "533a19.17h",   0x00200000, 0x3ec1843e, 3 | BRF_GRA           }, //  5 k056832
	{ "533a18.13g",   0x00080000, 0xd3e0d058, 3 | BRF_GRA           }, //  6 k056832

	{ "533a17.25g",   0x00200000, 0x9947af57, 4 | BRF_GRA           }, //  7 k055673
	{ "533a13.28g",   0x00200000, 0x58f1fc38, 4 | BRF_GRA           }, //  8 k055673
	{ "533a11.30g",   0x00200000, 0x983105e1, 4 | BRF_GRA           }, //  9 k055673

	{ "533a22.9g",    0x00200000, 0x97233814, 5 | BRF_SND           }, // 10 k054539
	{ "533a23.7g",    0x00200000, 0x1bb7552b, 5 | BRF_SND           }, // 11 k054539
};

STD_ROM_PICK(sexyparoa)

STD_ROM_FN(sexyparoa)

static struct BurnRomInfo sexyparoeblRomDesc[] = {
	{ "03.bin", 0x00080000, 0xdaa3a77c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "04.bin", 0x00080000, 0xb7bf9603, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu

	{ "06.bin", 0x00200000, 0x487af653, 3 | BRF_GRA           }, //  2 k056832
	{ "05.bin", 0x00080000, 0xd3e0d058, 3 | BRF_GRA           }, //  3 k056832

	{ "08.bin", 0x00200000, 0xf7e50aa0, 4 | BRF_GRA           }, //  4 k055673
	{ "09.bin", 0x00200000, 0x5f0b3593, 4 | BRF_GRA           }, //  5 k055673
	{ "07.bin", 0x00200000, 0xfafb2066, 4 | BRF_GRA           }, //  6 k055673
	{ "10.bin", 0x00200000, 0x60d0d02f, 4 | BRF_GRA           }, //  7 k055673
	{ "11.bin", 0x00200000, 0x47a74033, 4 | BRF_GRA           }, //  8 k055673

	{ "02.bin", 0x00100000, 0x391b2309, 5 | BRF_SND           }, //  9 oki1
	{ "01.bin", 0x00040000, 0xc48ba934, 5 | BRF_SND           }, // 10 oki2
};

STD_ROM_PICK(sexyparoebl)

STD_ROM_FN(sexyparoebl)

static struct BurnRomInfo daiskissRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "535jaa02.31b", 0x00080000, 0xe5b3e0e5, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "535jaa03.27b", 0x00080000, 0x9dc10140, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "535a08.9c",    0x00020000, 0x449416a7, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "535a09.7c",    0x00020000, 0x8ec57ab4, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "535a19.17h",   0x00200000, 0xfa1c59d1, 3 | BRF_GRA           }, //  5 k056832
	{ "535a18.13g",   0x00080000, 0xd02e5103, 3 | BRF_GRA           }, //  6 k056832

	{ "535a17.25g",   0x00100000, 0xb12070e2, 4 | BRF_GRA           }, //  7 k055673
	{ "535a13.28g",   0x00100000, 0x10cf9d05, 4 | BRF_GRA           }, //  8 k055673
	{ "535a11.30g",   0x00080000, 0x2b176b0f, 4 | BRF_GRA           }, //  9 k055673

	{ "535a22.9g",    0x00200000, 0x7ee59acb, 5 | BRF_SND           }, // 10 k054539
};

STD_ROM_PICK(daiskiss)

STD_ROM_FN(daiskiss)

static struct BurnRomInfo tokkaeRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "615jaa02.31b", 0x00080000, 0xf66d6dbf, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "615jaa03.27b", 0x00080000, 0xb7760e2b, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "615a08.9c",    0x00020000, 0xa5340de4, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "615a09.7c",    0x00020000, 0xc61f954c, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "615a19.17h",   0x00100000, 0x07749e1e, 3 | BRF_GRA           }, //  5 k056832
	{ "615a20.13c",   0x00080000, 0x9911b5a1, 3 | BRF_GRA           }, //  6 k056832

	{ "615a17.25g",   0x00200000, 0xb864654b, 4 | BRF_GRA           }, //  7 k055673
	{ "615a13.28g",   0x00200000, 0x4e8afa1a, 4 | BRF_GRA           }, //  8 k055673
	{ "615a16.18h",   0x00200000, 0xdfa0f0fe, 4 | BRF_GRA           }, //  9 k055673
	{ "615a12.27g",   0x00200000, 0xfbc563fd, 4 | BRF_GRA           }, // 10 k055673
	{ "615a11.30g",   0x00200000, 0xf25946e4, 4 | BRF_GRA           }, // 11 k055673

	{ "615a22.9g",    0x00200000, 0xea7e47dd, 5 | BRF_SND           }, // 12 k054539
	{ "615a23.7g",    0x00200000, 0x22d71f36, 5 | BRF_SND           }, // 13 k054539

	{ "tokkae.nv",    0x00000080, 0x5a6f8da6, 6 | BRF_PRG | BRF_ESS }, // 14 eeprom
};

STD_ROM_PICK(tokkae)

STD_ROM_FN(tokkae)

static struct BurnRomInfo salmndr2RomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "521jaa02.31b", 0x00080000, 0xf6c3a95b, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "521jaa03.27b", 0x00080000, 0xc3be5e0a, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "521-a04.9c",   0x00010000, 0xefddca7a, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "521-a05.7c",   0x00010000, 0x51a3af2c, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "521-a09.17h",  0x00200000, 0xfb9e2f5e, 3 | BRF_GRA           }, //  5 k056832
	{ "521-a11.15h",  0x00100000, 0x25e0a6e5, 3 | BRF_GRA           }, //  6 k056832
	{ "521-a13.13c",  0x00200000, 0x3ed7441b, 3 | BRF_GRA           }, //  7 k056832

	{ "521-a08.25g",  0x00200000, 0xf24f76bd, 4 | BRF_GRA           }, //  8 k055673
	{ "521-a07.28g",  0x00200000, 0x50ef9b7a, 4 | BRF_GRA           }, //  9 k055673
	{ "521-a06.30g",  0x00200000, 0xcba5db2c, 4 | BRF_GRA           }, // 10 k055673

	{ "521-a12.9g",   0x00200000, 0x66614d3b, 5 | BRF_SND           }, // 11 k054539
	{ "521-a13.7g",   0x00100000, 0xc3322475, 5 | BRF_SND           }, // 12 k054539

	{ "salmndr2.nv",  0x00000080, 0x60cdea03, 6 | BRF_PRG | BRF_ESS }, // 13 eeprom
};

STD_ROM_PICK(salmndr2)

STD_ROM_FN(salmndr2)

static struct BurnRomInfo salmndr2aRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "521aab02.31b", 0x00080000, 0xac9d151f, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "521aab03.27b", 0x00080000, 0xfeecf34d, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "521-a04.9c",   0x00010000, 0xefddca7a, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "521-a05.7c",   0x00010000, 0x51a3af2c, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "521-a09.17h",  0x00200000, 0xfb9e2f5e, 3 | BRF_GRA           }, //  5 k056832
	{ "521-a11.15h",  0x00100000, 0x25e0a6e5, 3 | BRF_GRA           }, //  6 k056832
	{ "521-a13.13c",  0x00200000, 0x3ed7441b, 3 | BRF_GRA           }, //  7 k056832

	{ "521-a08.25g",  0x00200000, 0xf24f76bd, 4 | BRF_GRA           }, //  8 k055673
	{ "521-a07.28g",  0x00200000, 0x50ef9b7a, 4 | BRF_GRA           }, //  9 k055673
	{ "521-a06.30g",  0x00200000, 0xcba5db2c, 4 | BRF_GRA           }, // 10 k055673

	{ "521-a12.9g",   0x00200000, 0x66614d3b, 5 | BRF_SND           }, // 11 k054539
	{ "521-a13.7g",   0x00100000, 0xc3322475, 5 | BRF_SND           }, // 12 k054539

	{ "salmndr2a.nv", 0x00000080, 0x3a98a8f9, 6 | BRF_PRG | BRF_ESS }, // 13 eeprom
};

STD_ROM_PICK(salmndr2a)

STD_ROM_FN(salmndr2a)

static struct BurnRomInfo winspikeRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "705eaa02.31b", 0x00080000, 0x522d1bbd, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "705eaa03.27b", 0x00080000, 0x778de17b, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "705a08.9c",    0x00020000, 0x0d531639, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "705a09.7c",    0x00020000, 0x24e58845, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "705a19.17h",   0x00100000, 0xbab84b30, 3 | BRF_GRA           }, //  5 k056832
	{ "705a18.22h",   0x00100000, 0xeb97fb5f, 3 | BRF_GRA           }, //  6 k056832

	{ "705a17.25g",   0x00400000, 0x971d2812, 4 | BRF_GRA           }, //  7 k055673
	{ "705a13.28g",   0x00400000, 0x3b62584b, 4 | BRF_GRA           }, //  8 k055673
	{ "705a11.30g",   0x00400000, 0x68542ce9, 4 | BRF_GRA           }, //  9 k055673
	{ "705a10.33g",   0x00400000, 0xfc4dc78b, 4 | BRF_GRA           }, // 10 k055673

	{ "705a22.9g",    0x00400000, 0x1a9246f6, 5 | BRF_SND           }, // 11 k054539
};

STD_ROM_PICK(winspike)

STD_ROM_FN(winspike)

static struct BurnRomInfo winspikeaRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "705aaa02.31b", 0x00080000, 0x43d1bad9, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "705aaa03.27b", 0x00080000, 0x2af78cca, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "705a08.9c",    0x00020000, 0x0d531639, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "705a09.7c",    0x00020000, 0x24e58845, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "705a19.17h",   0x00100000, 0xbab84b30, 3 | BRF_GRA           }, //  5 k056832
	{ "705a18.22h",   0x00100000, 0xeb97fb5f, 3 | BRF_GRA           }, //  6 k056832

	{ "705a17.25g",   0x00400000, 0x971d2812, 4 | BRF_GRA           }, //  7 k055673
	{ "705a13.28g",   0x00400000, 0x3b62584b, 4 | BRF_GRA           }, //  8 k055673
	{ "705a11.30g",   0x00400000, 0x68542ce9, 4 | BRF_GRA           }, //  9 k055673
	{ "705a10.33g",   0x00400000, 0xfc4dc78b, 4 | BRF_GRA           }, // 10 k055673

	{ "705a22.9g",    0x00400000, 0x1a9246f6, 5 | BRF_SND           }, // 11 k054539
};

STD_ROM_PICK(winspikea)

STD_ROM_FN(winspikea)

static struct BurnRomInfo winspikejRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "705jaa02.31b", 0x00080000, 0x85f11b03, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "705jaa03.27b", 0x00080000, 0x1d5e3922, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "705a08.9c",    0x00020000, 0x0d531639, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "705a09.7c",    0x00020000, 0x24e58845, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "705a19.17h",   0x00100000, 0xbab84b30, 3 | BRF_GRA           }, //  5 k056832
	{ "705a18.22h",   0x00100000, 0xeb97fb5f, 3 | BRF_GRA           }, //  6 k056832

	{ "705a17.25g",   0x00400000, 0x971d2812, 4 | BRF_GRA           }, //  7 k055673
	{ "705a13.28g",   0x00400000, 0x3b62584b, 4 | BRF_GRA           }, //  8 k055673
	{ "705a11.30g",   0x00400000, 0x68542ce9, 4 | BRF_GRA           }, //  9 k055673
	{ "705a10.33g",   0x00400000, 0xfc4dc78b, 4 | BRF_GRA           }, // 10 k055673

	{ "705a22.9g",    0x00400000, 0x1a9246f6, 5 | BRF_SND           }, // 11 k054539
};

STD_ROM_PICK(winspikej)

STD_ROM_FN(winspikej)

static struct BurnRomInfo soccerssRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "427eac02.28m", 0x00080000, 0x1817b218, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "427eac03.30m", 0x00080000, 0x8a17f509, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "427a04.28r",   0x00080000, 0xc7d3e1a2, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "427a05.30r",   0x00080000, 0x5372f0a5, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "427a07.6m",    0x00020000, 0x8dbaf4c7, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "427a06.9m",    0x00020000, 0x979df65d, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "427a15.11r",   0x00100000, 0x33ce2b8e, 3 | BRF_GRA           }, //  7 k056832
	{ "427a14.143",   0x00080000, 0x7575a0ed, 3 | BRF_GRA           }, //  8 k056832

	{ "427a13.18r",   0x00200000, 0x815a9b87, 4 | BRF_GRA           }, //  9 k055673
	{ "427a11.23r",   0x00200000, 0xc1ca74c1, 4 | BRF_GRA           }, // 10 k055673
	{ "427a09.137",   0x00200000, 0x56bdd480, 4 | BRF_GRA           }, // 11 k055673
	{ "427a12.21r",   0x00200000, 0x97d6fd38, 4 | BRF_GRA           }, // 12 k055673
	{ "427a10.25r",   0x00200000, 0x6b3ccb41, 4 | BRF_GRA           }, // 13 k055673
	{ "427a08.140",   0x00200000, 0x221250af, 4 | BRF_GRA           }, // 14 k055673

	{ "427a18.145",   0x00100000, 0xbb6e6ec6, 7 | BRF_GRA           }, // 15 gfx3

	{ "427a17.24c",   0x00080000, 0xfb6eb01f, 8 | BRF_GRA           }, // 16 gfx4

	{ "427a16.9r",    0x00200000, 0x39547265, 5 | BRF_SND           }, // 17 k054539

	{ "soccerss.nv",  0x00000080, 0xf222dae4, 6 | BRF_PRG | BRF_ESS }, // 18 eeprom
};

STD_ROM_PICK(soccerss)

STD_ROM_FN(soccerss)

static struct BurnRomInfo soccerssuRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "427uac02.28m", 0x00080000, 0xcd999967, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "427uac03.30m", 0x00080000, 0x2edd4d49, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "427a04.28r",   0x00080000, 0xc7d3e1a2, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "427a05.30r",   0x00080000, 0x5372f0a5, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "427a07.6m",    0x00020000, 0x8dbaf4c7, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "427a06.9m",    0x00020000, 0x979df65d, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "427a15.11r",   0x00100000, 0x33ce2b8e, 3 | BRF_GRA           }, //  7 k056832
	{ "427a14.143",   0x00080000, 0x7575a0ed, 3 | BRF_GRA           }, //  8 k056832

	{ "427a13.18r",   0x00200000, 0x815a9b87, 4 | BRF_GRA           }, //  9 k055673
	{ "427a11.23r",   0x00200000, 0xc1ca74c1, 4 | BRF_GRA           }, // 10 k055673
	{ "427a09.137",   0x00200000, 0x56bdd480, 4 | BRF_GRA           }, // 11 k055673
	{ "427a12.21r",   0x00200000, 0x97d6fd38, 4 | BRF_GRA           }, // 12 k055673
	{ "427a10.25r",   0x00200000, 0x6b3ccb41, 4 | BRF_GRA           }, // 13 k055673
	{ "427a08.140",   0x00200000, 0x221250af, 4 | BRF_GRA           }, // 14 k055673

	{ "427a18.145",   0x00100000, 0xbb6e6ec6, 7 | BRF_GRA           }, // 15 gfx3

	{ "427a17.24c",   0x00080000, 0xfb6eb01f, 8 | BRF_GRA           }, // 16 gfx4

	{ "427a16.9r",    0x00200000, 0x39547265, 5 | BRF_SND           }, // 17 k054539

	{ "soccerssu.nv", 0x00000080, 0x812f6878, 6 | BRF_PRG | BRF_ESS }, // 18 eeprom
};

STD_ROM_PICK(soccerssu)

STD_ROM_FN(soccerssu)

static struct BurnRomInfo soccerssjRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "427jac02.28m", 0x00080000, 0x399fe89d, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "427jac03.30m", 0x00080000, 0xf9c6ab08, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "427a04.28r",   0x00080000, 0xc7d3e1a2, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "427a05.30r",   0x00080000, 0x5372f0a5, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "427a07.6m",    0x00020000, 0x8dbaf4c7, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "427a06.9m",    0x00020000, 0x979df65d, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "427a15.11r",   0x00100000, 0x33ce2b8e, 3 | BRF_GRA           }, //  7 k056832
	{ "427a14.143",   0x00080000, 0x7575a0ed, 3 | BRF_GRA           }, //  8 k056832

	{ "427a13.18r",   0x00200000, 0x815a9b87, 4 | BRF_GRA           }, //  9 k055673
	{ "427a11.23r",   0x00200000, 0xc1ca74c1, 4 | BRF_GRA           }, // 10 k055673
	{ "427a09.137",   0x00200000, 0x56bdd480, 4 | BRF_GRA           }, // 11 k055673
	{ "427a12.21r",   0x00200000, 0x97d6fd38, 4 | BRF_GRA           }, // 12 k055673
	{ "427a10.25r",   0x00200000, 0x6b3ccb41, 4 | BRF_GRA           }, // 13 k055673
	{ "427a08.140",   0x00200000, 0x221250af, 4 | BRF_GRA           }, // 14 k055673

	{ "427a18.145",   0x00100000, 0xbb6e6ec6, 7 | BRF_GRA           }, // 15 gfx3

	{ "427a17.24c",   0x00080000, 0xfb6eb01f, 8 | BRF_GRA           }, // 16 gfx4

	{ "427a16.9r",    0x00200000, 0x39547265, 5 | BRF_SND           }, // 17 k054539

	{ "soccerssj.nv", 0x00000080, 0x7440255e, 6 | BRF_PRG | BRF_ESS }, // 18 eeprom
};

STD_ROM_PICK(soccerssj)

STD_ROM_FN(soccerssj)

static struct BurnRomInfo soccerssjaRomDesc[] = {
	{ "300a01.34k",    0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "427jaa02.28m",  0x00080000, 0x210f9ba7, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "427jaa03.30m",  0x00080000, 0xf76add04, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "427a04.28r",    0x00080000, 0xc7d3e1a2, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "427a05.30r",    0x00080000, 0x5372f0a5, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "427a07.6m",     0x00020000, 0x8dbaf4c7, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "427a06.9m",     0x00020000, 0x979df65d, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "427a15.11r",    0x00100000, 0x33ce2b8e, 3 | BRF_GRA           }, //  7 k056832
	{ "427a14.143",    0x00080000, 0x7575a0ed, 3 | BRF_GRA           }, //  8 k056832

	{ "427a13.18r",    0x00200000, 0x815a9b87, 4 | BRF_GRA           }, //  9 k055673
	{ "427a11.23r",    0x00200000, 0xc1ca74c1, 4 | BRF_GRA           }, // 10 k055673
	{ "427a09.137",    0x00200000, 0x56bdd480, 4 | BRF_GRA           }, // 11 k055673
	{ "427a12.21r",    0x00200000, 0x97d6fd38, 4 | BRF_GRA           }, // 12 k055673
	{ "427a10.25r",    0x00200000, 0x6b3ccb41, 4 | BRF_GRA           }, // 13 k055673
	{ "427a08.140",    0x00200000, 0x221250af, 4 | BRF_GRA           }, // 14 k055673

	{ "427a18.145",    0x00100000, 0xbb6e6ec6, 7 | BRF_GRA           }, // 15 gfx3

	{ "427a17.24c",    0x00080000, 0xfb6eb01f, 8 | BRF_GRA           }, // 16 gfx4

	{ "427a16.9r",     0x00200000, 0x39547265, 5 | BRF_SND           }, // 17 k054539

	{ "soccerssja.nv", 0x00000080, 0x60dba700, 6 | BRF_PRG | BRF_ESS }, // 18 eeprom
};

STD_ROM_PICK(soccerssja)

STD_ROM_FN(soccerssja)

static struct BurnRomInfo soccerssaRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "427aaa02.28m", 0x00080000, 0xa001d4bf, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "427aaa03.30m", 0x00080000, 0x83d37f48, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "427a04.28r",   0x00080000, 0xc7d3e1a2, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "427a05.30r",   0x00080000, 0x5372f0a5, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "427a07.6m",    0x00020000, 0x8dbaf4c7, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "427a06.9m",    0x00020000, 0x979df65d, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "427a15.11r",   0x00100000, 0x33ce2b8e, 3 | BRF_GRA           }, //  7 k056832
	{ "427a14.143",   0x00080000, 0x7575a0ed, 3 | BRF_GRA           }, //  8 k056832

	{ "427a13.18r",   0x00200000, 0x815a9b87, 4 | BRF_GRA           }, //  9 k055673
	{ "427a11.23r",   0x00200000, 0xc1ca74c1, 4 | BRF_GRA           }, // 10 k055673
	{ "427a09.137",   0x00200000, 0x56bdd480, 4 | BRF_GRA           }, // 11 k055673
	{ "427a12.21r",   0x00200000, 0x97d6fd38, 4 | BRF_GRA           }, // 12 k055673
	{ "427a10.25r",   0x00200000, 0x6b3ccb41, 4 | BRF_GRA           }, // 13 k055673
	{ "427a08.140",   0x00200000, 0x221250af, 4 | BRF_GRA           }, // 14 k055673

	{ "427a18.145",   0x00100000, 0xbb6e6ec6, 7 | BRF_GRA           }, // 15 gfx3

	{ "427a17.24c",   0x00080000, 0xfb6eb01f, 8 | BRF_GRA           }, // 16 gfx4

	{ "427a16.9r",    0x00200000, 0x39547265, 5 | BRF_SND           }, // 17 k054539

	{ "soccerssa.nv", 0x00000080, 0xe3a3f3d5, 6 | BRF_PRG | BRF_ESS }, // 18 eeprom
};

STD_ROM_PICK(soccerssa)

STD_ROM_FN(soccerssa)

static struct BurnRomInfo vsnetscrRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "627ead03.29m", 0x00080000, 0x2da707e2, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "627ead02.31m", 0x00080000, 0x01ab336a, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "627a05.29r",   0x00100000, 0xbe4e7b3c, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "627a04.31r",   0x00100000, 0x17334e9a, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "627b06.9m",    0x00020000, 0xc8337b9d, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "627b07.7m",    0x00020000, 0xd7d92579, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "627a21.11r",   0x00100000, 0xd0755fb8, 3 | BRF_GRA           }, //  7 k056832
	{ "627a20.11m",   0x00100000, 0xf68b28f2, 3 | BRF_GRA           }, //  8 k056832

	{ "627a19.14r",   0x00400000, 0x39989087, 4 | BRF_GRA           }, //  9 k055673
	{ "627a15.18r",   0x00400000, 0x94c557e9, 4 | BRF_GRA           }, // 10 k055673
	{ "627a11.23r",   0x00400000, 0x8185b19f, 4 | BRF_GRA           }, // 11 k055673
	{ "627a17.16r",   0x00400000, 0x5c79a0a5, 4 | BRF_GRA           }, // 12 k055673
	{ "627a13.21r",   0x00400000, 0x934c758b, 4 | BRF_GRA           }, // 13 k055673
	{ "627a09.25r",   0x00400000, 0x980b0f87, 4 | BRF_GRA           }, // 14 k055673

	{ "627a24.22h",   0x00200000, 0x2cd73305, 7 | BRF_GRA           }, // 15 gfx3

	{ "627a23.7r",    0x00400000, 0xd70c59dd, 5 | BRF_SND           }, // 16 k054539
};

STD_ROM_PICK(vsnetscr)

STD_ROM_FN(vsnetscr)

static struct BurnRomInfo vsnetscrebRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "627eab03.29m", 0x00080000, 0x878b2369, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "627eab02.31m", 0x00080000, 0xcc76bce8, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "627a05.29r",   0x00100000, 0xbe4e7b3c, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "627a04.31r",   0x00100000, 0x17334e9a, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "627b06.9m",    0x00020000, 0xc8337b9d, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "627b07.7m",    0x00020000, 0xd7d92579, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "627a21.11r",   0x00100000, 0xd0755fb8, 3 | BRF_GRA           }, //  7 k056832
	{ "627a20.11m",   0x00100000, 0xf68b28f2, 3 | BRF_GRA           }, //  8 k056832

	{ "627a19.14r",   0x00400000, 0x39989087, 4 | BRF_GRA           }, //  9 k055673
	{ "627a15.18r",   0x00400000, 0x94c557e9, 4 | BRF_GRA           }, // 10 k055673
	{ "627a11.23r",   0x00400000, 0x8185b19f, 4 | BRF_GRA           }, // 11 k055673
	{ "627a17.16r",   0x00400000, 0x5c79a0a5, 4 | BRF_GRA           }, // 12 k055673
	{ "627a13.21r",   0x00400000, 0x934c758b, 4 | BRF_GRA           }, // 13 k055673
	{ "627a09.25r",   0x00400000, 0x980b0f87, 4 | BRF_GRA           }, // 14 k055673

	{ "627a24.22h",   0x00200000, 0x2cd73305, 7 | BRF_GRA           }, // 15 gfx3

	{ "627a23.7r",    0x00400000, 0xd70c59dd, 5 | BRF_SND           }, // 16 k054539
};

STD_ROM_PICK(vsnetscreb)

STD_ROM_FN(vsnetscreb)

static struct BurnRomInfo vsnetscruRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "627uab03.29m", 0x00080000, 0x53ca7eec, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "627uab02.31m", 0x00080000, 0xc352cc6f, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "627a05.29r",   0x00100000, 0xbe4e7b3c, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "627a04.31r",   0x00100000, 0x17334e9a, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "627b06.9m",    0x00020000, 0xc8337b9d, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "627b07.7m",    0x00020000, 0xd7d92579, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "627a21.11r",   0x00100000, 0xd0755fb8, 3 | BRF_GRA           }, //  7 k056832
	{ "627a20.11m",   0x00100000, 0xf68b28f2, 3 | BRF_GRA           }, //  8 k056832

	{ "627a19.14r",   0x00400000, 0x39989087, 4 | BRF_GRA           }, //  9 k055673
	{ "627a15.18r",   0x00400000, 0x94c557e9, 4 | BRF_GRA           }, // 10 k055673
	{ "627a11.23r",   0x00400000, 0x8185b19f, 4 | BRF_GRA           }, // 11 k055673
	{ "627a17.16r",   0x00400000, 0x5c79a0a5, 4 | BRF_GRA           }, // 12 k055673
	{ "627a13.21r",   0x00400000, 0x934c758b, 4 | BRF_GRA           }, // 13 k055673
	{ "627a09.25r",   0x00400000, 0x980b0f87, 4 | BRF_GRA           }, // 14 k055673

	{ "627a24.22h",   0x00200000, 0x2cd73305, 7 | BRF_GRA           }, // 15 gfx3

	{ "627a23.7r",    0x00400000, 0xd70c59dd, 5 | BRF_SND           }, // 16 k054539
};

STD_ROM_PICK(vsnetscru)

STD_ROM_FN(vsnetscru)

static struct BurnRomInfo vsnetscraRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "627aaa03.29m", 0x00080000, 0x50e23a50, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "627aaa02.31m", 0x00080000, 0xe3d21afe, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "627a05.29r",   0x00100000, 0xbe4e7b3c, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "627a04.31r",   0x00100000, 0x17334e9a, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "627b06.9m",    0x00020000, 0xc8337b9d, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "627b07.7m",    0x00020000, 0xd7d92579, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "627a21.11r",   0x00100000, 0xd0755fb8, 3 | BRF_GRA           }, //  7 k056832
	{ "627a20.11m",   0x00100000, 0xf68b28f2, 3 | BRF_GRA           }, //  8 k056832

	{ "627a19.14r",   0x00400000, 0x39989087, 4 | BRF_GRA           }, //  9 k055673
	{ "627a15.18r",   0x00400000, 0x94c557e9, 4 | BRF_GRA           }, // 10 k055673
	{ "627a11.23r",   0x00400000, 0x8185b19f, 4 | BRF_GRA           }, // 11 k055673
	{ "627a17.16r",   0x00400000, 0x5c79a0a5, 4 | BRF_GRA           }, // 12 k055673
	{ "627a13.21r",   0x00400000, 0x934c758b, 4 | BRF_GRA           }, // 13 k055673
	{ "627a09.25r",   0x00400000, 0x980b0f87, 4 | BRF_GRA           }, // 14 k055673

	{ "627a24.22h",   0x00200000, 0x2cd73305, 7 | BRF_GRA           }, // 15 gfx3

	{ "627a23.7r",    0x00400000, 0xd70c59dd, 5 | BRF_SND           }, // 16 k054539
};

STD_ROM_PICK(vsnetscra)

STD_ROM_FN(vsnetscra)

static struct BurnRomInfo vsnetscrjRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "627jab03.29m", 0x00080000, 0x68c4bb17, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "627jab02.31m", 0x00080000, 0xf10929d7, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "627a05.29r",   0x00100000, 0xbe4e7b3c, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "627a04.31r",   0x00100000, 0x17334e9a, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "627b06.9m",    0x00020000, 0xc8337b9d, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "627b07.7m",    0x00020000, 0xd7d92579, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "627a21.11r",   0x00100000, 0xd0755fb8, 3 | BRF_GRA           }, //  7 k056832
	{ "627a20.11m",   0x00100000, 0xf68b28f2, 3 | BRF_GRA           }, //  8 k056832

	{ "627a19.14r",   0x00400000, 0x39989087, 4 | BRF_GRA           }, //  9 k055673
	{ "627a15.18r",   0x00400000, 0x94c557e9, 4 | BRF_GRA           }, // 10 k055673
	{ "627a11.23r",   0x00400000, 0x8185b19f, 4 | BRF_GRA           }, // 11 k055673
	{ "627a17.16r",   0x00400000, 0x5c79a0a5, 4 | BRF_GRA           }, // 12 k055673
	{ "627a13.21r",   0x00400000, 0x934c758b, 4 | BRF_GRA           }, // 13 k055673
	{ "627a09.25r",   0x00400000, 0x980b0f87, 4 | BRF_GRA           }, // 14 k055673

	{ "627a24.22h",   0x00200000, 0x2cd73305, 7 | BRF_GRA           }, // 15 gfx3

	{ "627a23.7r",    0x00400000, 0xd70c59dd, 5 | BRF_SND           }, // 16 k054539
};

STD_ROM_PICK(vsnetscrj)

STD_ROM_FN(vsnetscrj)

static struct BurnRomInfo rungun2RomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "505uaa02.31b", 0x00080000, 0xcfca23f7, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "505uaa03.27b", 0x00080000, 0xad7f9ded, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "505a04.31r",   0x00100000, 0x11a73f01, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "505a05.29r",   0x00100000, 0x5da5d695, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "505a06.9m",    0x00020000, 0x920013f1, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "505a07.7m",    0x00020000, 0x5641c603, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "505a21.11r",   0x00100000, 0x03fda175, 3 | BRF_GRA           }, //  7 k056832
	{ "505a20.11m",   0x00100000, 0xa6a300fb, 3 | BRF_GRA           }, //  8 k056832

	{ "505a19.14r",   0x00200000, 0xffde4f17, 4 | BRF_GRA           }, //  9 k055673
	{ "505a15.18r",   0x00200000, 0xd9ab1e6c, 4 | BRF_GRA           }, // 10 k055673
	{ "505a11.23r",   0x00200000, 0x75c13df0, 4 | BRF_GRA           }, // 11 k055673
	{ "505a17.16r",   0x00200000, 0x8176f2f5, 4 | BRF_GRA           }, // 12 k055673
	{ "505a13.21r",   0x00200000, 0xe60c5191, 4 | BRF_GRA           }, // 13 k055673
	{ "505a09.25r",   0x00200000, 0x3e1d5a15, 4 | BRF_GRA           }, // 14 k055673
	{ "505a18.18m",   0x00200000, 0xc12bacfe, 4 | BRF_GRA           }, // 15 k055673
	{ "505a14.14m",   0x00200000, 0x356a75b0, 4 | BRF_GRA           }, // 16 k055673
	{ "505a10.23m",   0x00200000, 0xfc315ee0, 4 | BRF_GRA           }, // 17 k055673
	{ "505a16.16m",   0x00200000, 0xca9c2193, 4 | BRF_GRA           }, // 18 k055673
	{ "505a12.21m",   0x00200000, 0x421d5034, 4 | BRF_GRA           }, // 19 k055673
	{ "505a08.25m",   0x00200000, 0x442ed3ec, 4 | BRF_GRA           }, // 20 k055673

	{ "505a24.22h",   0x00200000, 0x70e906da, 7 | BRF_GRA           }, // 21 gfx3

	{ "505a23.7r",    0x00200000, 0x67f03445, 5 | BRF_SND           }, // 22 k054539
	{ "505a22.9r",    0x00200000, 0xc2b67a9d, 5 | BRF_SND           }, // 23 k054539
};

STD_ROM_PICK(rungun2)

STD_ROM_FN(rungun2)

static struct BurnRomInfo slamdnk2RomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "505jaa02.31m", 0x00080000, 0x9f72d48e, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "505jaa03.29m", 0x00080000, 0x52513794, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "505a04.31r",   0x00100000, 0x11a73f01, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "505a05.29r",   0x00100000, 0x5da5d695, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "505a06.9m",    0x00020000, 0x920013f1, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "505a07.7m",    0x00020000, 0x5641c603, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "505a21.11r",   0x00100000, 0x03fda175, 3 | BRF_GRA           }, //  7 k056832
	{ "505a20.11m",   0x00100000, 0xa6a300fb, 3 | BRF_GRA           }, //  8 k056832

	{ "505a19.14r",   0x00200000, 0xffde4f17, 4 | BRF_GRA           }, //  9 k055673
	{ "505a15.18r",   0x00200000, 0xd9ab1e6c, 4 | BRF_GRA           }, // 10 k055673
	{ "505a11.23r",   0x00200000, 0x75c13df0, 4 | BRF_GRA           }, // 11 k055673
	{ "505a17.16r",   0x00200000, 0x8176f2f5, 4 | BRF_GRA           }, // 12 k055673
	{ "505a13.21r",   0x00200000, 0xe60c5191, 4 | BRF_GRA           }, // 13 k055673
	{ "505a09.25r",   0x00200000, 0x3e1d5a15, 4 | BRF_GRA           }, // 14 k055673
	{ "505a18.18m",   0x00200000, 0xc12bacfe, 4 | BRF_GRA           }, // 15 k055673
	{ "505a14.14m",   0x00200000, 0x356a75b0, 4 | BRF_GRA           }, // 16 k055673
	{ "505a10.23m",   0x00200000, 0xfc315ee0, 4 | BRF_GRA           }, // 17 k055673
	{ "505a16.16m",   0x00200000, 0xca9c2193, 4 | BRF_GRA           }, // 18 k055673
	{ "505a12.21m",   0x00200000, 0x421d5034, 4 | BRF_GRA           }, // 19 k055673
	{ "505a08.25m",   0x00200000, 0x442ed3ec, 4 | BRF_GRA           }, // 20 k055673

	{ "505a24.22h",   0x00200000, 0x70e906da, 7 | BRF_GRA           }, // 21 gfx3

	{ "505a23.7r",    0x00200000, 0x67f03445, 5 | BRF_SND           }, // 22 k054539
	{ "505a22.9r",    0x00200000, 0xc2b67a9d, 5 | BRF_SND           }, // 23 k054539
};

STD_ROM_PICK(slamdnk2)

STD_ROM_FN(slamdnk2)

static struct BurnRomInfo rushheroRomDesc[] = {
	{ "300a01.34k",   0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "605uab03.29m", 0x00080000, 0xc5b8d31d, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "605uab02.31m", 0x00080000, 0x94c3d835, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu
	{ "605a05.29r",   0x00100000, 0x9bca4297, 1 | BRF_PRG | BRF_ESS }, //  3 maincpu
	{ "605a04.31r",   0x00100000, 0xf6788154, 1 | BRF_PRG | BRF_ESS }, //  4 maincpu

	{ "605a06.9m",    0x00020000, 0x9ca03dce, 2 | BRF_PRG | BRF_ESS }, //  5 soundcpu
	{ "605a07.7m",    0x00020000, 0x3116a8b0, 2 | BRF_PRG | BRF_ESS }, //  6 soundcpu

	{ "605a21.11r",   0x00100000, 0x0e5add29, 3 | BRF_GRA           }, //  7 k056832
	{ "605a20.11m",   0x00100000, 0xa8fb4288, 3 | BRF_GRA           }, //  8 k056832

	{ "605a19.14r",   0x00400000, 0x293427d0, 4 | BRF_GRA           }, //  9 k055673
	{ "605a15.18r",   0x00400000, 0x19e6e356, 4 | BRF_GRA           }, // 10 k055673
	{ "605a11.23r",   0x00400000, 0xbc61339c, 4 | BRF_GRA           }, // 11 k055673
	{ "605a17.16r",   0x00400000, 0x7a8f1cf9, 4 | BRF_GRA           }, // 12 k055673
	{ "605a13.21r",   0x00400000, 0x9a6dff6d, 4 | BRF_GRA           }, // 13 k055673
	{ "605a09.25r",   0x00400000, 0x624fd486, 4 | BRF_GRA           }, // 14 k055673
	{ "605a18.14m",   0x00400000, 0x4d4dbecb, 4 | BRF_GRA           }, // 15 k055673
	{ "605a14.18m",   0x00400000, 0xb5115d76, 4 | BRF_GRA           }, // 16 k055673
	{ "605a10.23m",   0x00400000, 0x4f47d434, 4 | BRF_GRA           }, // 17 k055673
	{ "605a16.16m",   0x00400000, 0xaab542ca, 4 | BRF_GRA           }, // 18 k055673
	{ "605a12.21m",   0x00400000, 0x194ffad0, 4 | BRF_GRA           }, // 19 k055673
	{ "605a08.25m",   0x00400000, 0xea80ddfd, 4 | BRF_GRA           }, // 20 k055673

	{ "605a24.22h",   0x00200000, 0x73f06065, 7 | BRF_GRA           }, // 21 gfx3

	{ "605a23.7r",    0x00400000, 0x458ecee1, 5 | BRF_SND           }, // 22 k054539
};

STD_ROM_PICK(rushhero)

STD_ROM_FN(rushhero)

struct BurnDriver BurnDrvKonamigx = {
	"konamigx", NULL, NULL, NULL, "1994",
	"Konami System GX BIOS\0", "BIOS only", "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_PREFIX_KONAMI, GBF_BIOS, 0,
	NULL, konamigxRomInfo, konamigxRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvRacinfrc = {
	"racinfrc", NULL, "konamigx", NULL, "1994",
	"Racin' Force (ver EAC)\0", "Preliminary support: ROM registration only", "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, racinfrcRomInfo, racinfrcRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvRacinfrcu = {
	"racinfrcu", "racinfrc", "konamigx", NULL, "1994",
	"Racin' Force (ver UAB)\0", "Preliminary support: ROM registration only", "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, racinfrcuRomInfo, racinfrcuRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvOpengolf = {
	"opengolf", NULL, "konamigx", NULL, "1994",
	"Konami's Open Golf Championship (ver EAE)\0", "Preliminary support: ROM registration only", "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, opengolfRomInfo, opengolfRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvOpengolf2 = {
	"opengolf2", "opengolf", "konamigx", NULL, "1994",
	"Konami's Open Golf Championship (ver EAD)\0", "Preliminary support: ROM registration only", "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, opengolf2RomInfo, opengolf2RomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvGgreats2 = {
	"ggreats2", "opengolf", "konamigx", NULL, "1994",
	"Golfing Greats 2 (ver JAC)\0", "Preliminary support: ROM registration only", "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, ggreats2RomInfo, ggreats2RomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvLe2 = {
	"le2", NULL, "konamigx", NULL, "1994",
	"Lethal Enforcers II: Gun Fighters (ver EAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, le2RomInfo, le2RomName, NULL, NULL, NULL, NULL, Le2InputInfo, Le2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvLe2u = {
	"le2u", "le2", "konamigx", NULL, "1994",
	"Lethal Enforcers II: Gun Fighters (ver UAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, le2uRomInfo, le2uRomName, NULL, NULL, NULL, NULL, Le2InputInfo, Le2uDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvLe2j = {
	"le2j", "le2", "konamigx", NULL, "1994",
	"Lethal Enforcers II: The Western (ver JAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, le2jRomInfo, le2jRomName, NULL, NULL, NULL, NULL, Le2InputInfo, Le2jDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvFantjour = {
	"fantjour", NULL, "konamigx", NULL, "1994",
	"Fantastic Journey (ver EAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, fantjourRomInfo, fantjourRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvFantjoura = {
	"fantjoura", "fantjour", "konamigx", NULL, "1994",
	"Fantastic Journey (ver AAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, fantjouraRomInfo, fantjouraRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvGokuparo = {
	"gokuparo", "fantjour", "konamigx", NULL, "1994",
	"Gokujou Parodius: Kako no Eikou o Motomete (ver JAD)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, gokuparoRomInfo, gokuparoRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvCrzcross = {
	"crzcross", NULL, "konamigx", NULL, "1994",
	"Crazy Cross (ver EAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PUZZLE, 0,
	NULL, crzcrossRomInfo, crzcrossRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvPuzldama = {
	"puzldama", "crzcross", "konamigx", NULL, "1994",
	"Taisen Puzzle-dama (ver JAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PUZZLE, 0,
	NULL, puzldamaRomInfo, puzldamaRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvTbyahhoo = {
	"tbyahhoo", NULL, "konamigx", NULL, "1995",
	"Twin Bee Yahhoo! (ver JAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, tbyahhooRomInfo, tbyahhooRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvMtwinbee = {
	"mtwinbee", "tbyahhoo", "konamigx", NULL, "1995",
	"Magical Twin Bee (ver EAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, mtwinbeeRomInfo, mtwinbeeRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvTkmmpzdm = {
	"tkmmpzdm", NULL, "konamigx", NULL, "1995",
	"Tokimeki Memorial Taisen Puzzle-dama (ver JAB)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PUZZLE, 0,
	NULL, tkmmpzdmRomInfo, tkmmpzdmRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvDragoona = {
	"dragoona", NULL, "konamigx", NULL, "1995",
	"Dragoon Might (ver AAB)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, dragoonaRomInfo, dragoonaRomName, NULL, NULL, NULL, NULL, DragoonInputInfo, GxCommonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 224, 4, 3
};

struct BurnDriver BurnDrvDragoonj = {
	"dragoonj", "dragoona", "konamigx", NULL, "1995",
	"Dragoon Might (ver JAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, dragoonjRomInfo, dragoonjRomName, NULL, NULL, NULL, NULL, DragoonInputInfo, GxCommonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 224, 4, 3
};

struct BurnDriver BurnDrvSexyparo = {
	"sexyparo", NULL, "konamigx", NULL, "1996",
	"Sexy Parodius (ver JAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, sexyparoRomInfo, sexyparoRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvSexyparoa = {
	"sexyparoa", "sexyparo", "konamigx", NULL, "1996",
	"Sexy Parodius (ver AAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, sexyparoaRomInfo, sexyparoaRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvSexyparoebl = {
	"sexyparoebl", "sexyparo", "konamigx", NULL, "1996",
	"Sexy Parodius (ver EAA, bootleg)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, sexyparoeblRomInfo, sexyparoeblRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvDaiskiss = {
	"daiskiss", NULL, "konamigx", NULL, "1996",
	"Daisu-Kiss (ver JAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PUZZLE, 0,
	NULL, daiskissRomInfo, daiskissRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvTokkae = {
	"tokkae", NULL, "konamigx", NULL, "1996",
	"Taisen Tokkae-dama (ver JAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PUZZLE, 0,
	NULL, tokkaeRomInfo, tokkaeRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvSalmndr2 = {
	"salmndr2", NULL, "konamigx", NULL, "1996",
	"Salamander 2 (ver JAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, salmndr2RomInfo, salmndr2RomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvSalmndr2a = {
	"salmndr2a", "salmndr2", "konamigx", NULL, "1996",
	"Salamander 2 (ver AAB)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, salmndr2aRomInfo, salmndr2aRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};

struct BurnDriver BurnDrvWinspike = {
	"winspike", NULL, "konamigx", NULL, "1997",
	"Winning Spike (ver EAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, winspikeRomInfo, winspikeRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxCommonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 224, 4, 3
};

struct BurnDriver BurnDrvWinspikea = {
	"winspikea", "winspike", "konamigx", NULL, "1997",
	"Winning Spike (ver AAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, winspikeaRomInfo, winspikeaRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxCommonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 224, 4, 3
};

struct BurnDriver BurnDrvWinspikej = {
	"winspikej", "winspike", "konamigx", NULL, "1997",
	"Winning Spike (ver JAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, winspikejRomInfo, winspikejRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxCommonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 224, 4, 3
};

struct BurnDriver BurnDrvSoccerss = {
	"soccerss", NULL, "konamigx", NULL, "1994",
	"Soccer Superstars (ver EAC)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, soccerssRomInfo, soccerssRomName, NULL, NULL, NULL, NULL, Gx4pInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	576, 224, 4, 3
};

struct BurnDriver BurnDrvSoccerssu = {
	"soccerssu", "soccerss", "konamigx", NULL, "1994",
	"Soccer Superstars (ver UAC)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, soccerssuRomInfo, soccerssuRomName, NULL, NULL, NULL, NULL, Gx4pInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	576, 224, 4, 3
};

struct BurnDriver BurnDrvSoccerssj = {
	"soccerssj", "soccerss", "konamigx", NULL, "1994",
	"Soccer Superstars (ver JAC)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, soccerssjRomInfo, soccerssjRomName, NULL, NULL, NULL, NULL, Gx4pInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	576, 224, 4, 3
};

struct BurnDriver BurnDrvSoccerssja = {
	"soccerssja", "soccerss", "konamigx", NULL, "1994",
	"Soccer Superstars (ver JAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, soccerssjaRomInfo, soccerssjaRomName, NULL, NULL, NULL, NULL, Gx4pInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	576, 224, 4, 3
};

struct BurnDriver BurnDrvSoccerssa = {
	"soccerssa", "soccerss", "konamigx", NULL, "1994",
	"Soccer Superstars (ver AAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, soccerssaRomInfo, soccerssaRomName, NULL, NULL, NULL, NULL, Gx4pInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	576, 224, 4, 3
};

struct BurnDriver BurnDrvVsnetscr = {
	"vsnetscr", NULL, "konamigx", NULL, "1996",
	"Versus Net Soccer (ver EAD)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, vsnetscrRomInfo, vsnetscrRomName, NULL, NULL, NULL, NULL, Gx4pInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	576, 224, 4, 3
};

struct BurnDriver BurnDrvVsnetscreb = {
	"vsnetscreb", "vsnetscr", "konamigx", NULL, "1996",
	"Versus Net Soccer (ver EAB)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, vsnetscrebRomInfo, vsnetscrebRomName, NULL, NULL, NULL, NULL, Gx4pInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	576, 224, 4, 3
};

struct BurnDriver BurnDrvVsnetscru = {
	"vsnetscru", "vsnetscr", "konamigx", NULL, "1996",
	"Versus Net Soccer (ver UAB)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, vsnetscruRomInfo, vsnetscruRomName, NULL, NULL, NULL, NULL, Gx4pInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	576, 224, 4, 3
};

struct BurnDriver BurnDrvVsnetscra = {
	"vsnetscra", "vsnetscr", "konamigx", NULL, "1996",
	"Versus Net Soccer (ver AAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, vsnetscraRomInfo, vsnetscraRomName, NULL, NULL, NULL, NULL, Gx4pInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	576, 224, 4, 3
};

struct BurnDriver BurnDrvVsnetscrj = {
	"vsnetscrj", "vsnetscr", "konamigx", NULL, "1996",
	"Versus Net Soccer (ver JAB)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, vsnetscrjRomInfo, vsnetscrjRomName, NULL, NULL, NULL, NULL, Gx4pInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	576, 224, 4, 3
};

struct BurnDriver BurnDrvRungun2 = {
	"rungun2", NULL, "konamigx", NULL, "1996",
	"Run and Gun 2 (ver UAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, rungun2RomInfo, rungun2RomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 224, 4, 3
};

struct BurnDriver BurnDrvSlamdnk2 = {
	"slamdnk2", "rungun2", "konamigx", NULL, "1996",
	"Slam Dunk 2 (ver JAA)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, slamdnk2RomInfo, slamdnk2RomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 224, 4, 3
};

struct BurnDriver BurnDrvRushhero = {
	"rushhero", NULL, "konamigx", NULL, "1996",
	"Rushing Heroes (ver UAB)\0", NULL, "Konami", "Konami System GX",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, rushheroRomInfo, rushheroRomName, NULL, NULL, NULL, NULL, Gx4pInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 224, 4, 3
};

//========================
// Add Korean Translation
//========================

static struct BurnRomInfo tbyahhookRomDesc[] = {
	{ "300a01.34k",    0x00020000, 0xd5fa95f5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "424jaa02k.31b", 0x00080000, 0xbc790cef, 1 | BRF_PRG | BRF_ESS }, //  1 maincpu
	{ "424jaa04k.27b", 0x00080000, 0xd553fd5c, 1 | BRF_PRG | BRF_ESS }, //  2 maincpu

	{ "424a06.9c",     0x00020000, 0xa4760e14, 2 | BRF_PRG | BRF_ESS }, //  3 soundcpu
	{ "424a07.7c",     0x00020000, 0xfa90d7e2, 2 | BRF_PRG | BRF_ESS }, //  4 soundcpu

	{ "424a14k.17h",   0x00200000, 0x95c666d0, 3 | BRF_GRA           }, //  5 k056832
	{ "424a12k.13g",   0x00080000, 0x4a7c489f, 3 | BRF_GRA           }, //  6 k056832

	{ "424a11k.25g",   0x00200000, 0x2f96de9f, 4 | BRF_GRA           }, //  7 k055673
	{ "424a10k.28g",   0x00200000, 0xe7bcd822, 4 | BRF_GRA           }, //  8 k055673
	{ "424a09k.30g",   0x00100000, 0xdcae531c, 4 | BRF_GRA           }, //  9 k055673

	{ "424a17.9g",     0x00200000, 0xe9dd9692, 5 | BRF_SND           }, // 10 k054539
	{ "424a18.7g",     0x00200000, 0x0f0d9f3a, 5 | BRF_SND           }, // 11 k054539

	{ "tbyahhoo.nv",   0x00000080, 0x1e6fa2f8, 6 | BRF_PRG | BRF_ESS }, // 12 eeprom
};

STD_ROM_PICK(tbyahhook)

STD_ROM_FN(tbyahhook)

struct BurnDriver BurnDrvTbyahhook = {
	"tbyahhook", "tbyahhoo", "konamigx", NULL, "2026",
	"Twin Bee Yahhoo! (ver JAA, Korean Translation)\0", NULL, "Konami", "Konami System GX",
	L"Twin Bee Yahhoo! (ver JAA, Korean Translation)\0\uD2B8\uC708\uBE44 \uC58F\uD638! - \uC774\uC0C1\uD55C \uB098\uB77C\uC5D0\uC11C\uC758 \uB300\uC18C\uB3D9!! (ver JAA, \uD55C\uAD6D\uC5B4 \uBC88\uC5ED)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, tbyahhookRomInfo, tbyahhookRomName, NULL, NULL, NULL, NULL, GxInputInfo, GxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};
