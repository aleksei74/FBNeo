// license:BSD-3-Clause
// Based on MAME 0.289 ddenlovr.cpp by Nicola Salmoria and Luca Elia.
// FB Neo adaptation: Dynax/Nakanihon third-generation blitter hardware.

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "z80ctc.h"
#include "z80pio.h"
#include "z80daisy.h"
#include "burn_ym2413.h"
#include "ay8910.h"
#include "msm6295.h"
#include <time.h>

static UINT8 *AllMem, *MemEnd, *AllRam, *RamEnd, *DrvROM, *GfxROM, *SndROM, *DrvRAM;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc, DrvReset, Joy[3][8], Inputs[3], Dips[8], MatrixJoy[2][5][6], Matrix[2][5];
static INT32 board, GfxLen, SndLen, RomLen, extraCycles;
static UINT8 *m_ddenlovr_pixmap[8];
static const int *m_ddenlovr_blit_commands;
struct VideoState {
	int m_extra_layers;
	int m_ddenlovr_dest_layer;
	int m_ddenlovr_blit_flip;
	int m_ddenlovr_blit_x;
	int m_ddenlovr_blit_y;
	int m_ddenlovr_blit_address;
	int m_ddenlovr_blit_pen;
	int m_ddenlovr_blit_pen_mode;
	int m_ddenlovr_blitter_irq_flag;
	int m_ddenlovr_blitter_irq_enable;
	int m_ddenlovr_rect_width;
	int m_ddenlovr_rect_height;
	int m_ddenlovr_clip_width;
	int m_ddenlovr_clip_height;
	int m_ddenlovr_line_length;
	int m_ddenlovr_clip_ctrl;
	int m_ddenlovr_clip_x;
	int m_ddenlovr_clip_y;
	int m_ddenlovr_scroll[8 * 2];
	int m_ddenlovr_priority;
	int m_ddenlovr_priority2;
	int m_ddenlovr_bgcolor;
	int m_ddenlovr_bgcolor2;
	int m_ddenlovr_layer_enable;
	int m_ddenlovr_layer_enable2;
	int m_ddenlovr_palette_base[8];
	int m_ddenlovr_palette_mask[8];
	int m_ddenlovr_transparency_pen[8];
	int m_ddenlovr_transparency_mask[8];
	int m_ddenlovr_blit_latch;
	int m_ddenlovr_blit_pen_mask; // not implemented
	int m_ddenlovr_blit_rom_bits; // usually 8, 16 in hanakanz

	int m_ddenlovr_blit_regs[2];
};

static VideoState v;
#define m_extra_layers v.m_extra_layers
#define m_ddenlovr_dest_layer v.m_ddenlovr_dest_layer
#define m_ddenlovr_blit_flip v.m_ddenlovr_blit_flip
#define m_ddenlovr_blit_x v.m_ddenlovr_blit_x
#define m_ddenlovr_blit_y v.m_ddenlovr_blit_y
#define m_ddenlovr_blit_address v.m_ddenlovr_blit_address
#define m_ddenlovr_blit_pen v.m_ddenlovr_blit_pen
#define m_ddenlovr_blit_pen_mode v.m_ddenlovr_blit_pen_mode
#define m_ddenlovr_blitter_irq_flag v.m_ddenlovr_blitter_irq_flag
#define m_ddenlovr_blitter_irq_enable v.m_ddenlovr_blitter_irq_enable
#define m_ddenlovr_rect_width v.m_ddenlovr_rect_width
#define m_ddenlovr_rect_height v.m_ddenlovr_rect_height
#define m_ddenlovr_clip_width v.m_ddenlovr_clip_width
#define m_ddenlovr_clip_height v.m_ddenlovr_clip_height
#define m_ddenlovr_line_length v.m_ddenlovr_line_length
#define m_ddenlovr_clip_ctrl v.m_ddenlovr_clip_ctrl
#define m_ddenlovr_clip_x v.m_ddenlovr_clip_x
#define m_ddenlovr_clip_y v.m_ddenlovr_clip_y
#define m_ddenlovr_scroll v.m_ddenlovr_scroll
#define m_ddenlovr_priority v.m_ddenlovr_priority
#define m_ddenlovr_priority2 v.m_ddenlovr_priority2
#define m_ddenlovr_bgcolor v.m_ddenlovr_bgcolor
#define m_ddenlovr_bgcolor2 v.m_ddenlovr_bgcolor2
#define m_ddenlovr_layer_enable v.m_ddenlovr_layer_enable
#define m_ddenlovr_layer_enable2 v.m_ddenlovr_layer_enable2
#define m_ddenlovr_palette_base v.m_ddenlovr_palette_base
#define m_ddenlovr_palette_mask v.m_ddenlovr_palette_mask
#define m_ddenlovr_transparency_pen v.m_ddenlovr_transparency_pen
#define m_ddenlovr_transparency_mask v.m_ddenlovr_transparency_mask
#define m_ddenlovr_blit_latch v.m_ddenlovr_blit_latch
#define m_ddenlovr_blit_pen_mask v.m_ddenlovr_blit_pen_mask
#define m_ddenlovr_blit_rom_bits v.m_ddenlovr_blit_rom_bits
#define m_ddenlovr_blit_regs v.m_ddenlovr_blit_regs
static UINT8 m_palram[0x200], dswSel, inputSel, okiBank, rtcRegs[16];
static UINT16 prot[2];
static INT32 rtcFrames;
static void BlitIrq(int state)
{
	if (board >= 31 && board <= 44)
		return;
	if (((board >= 23 && board <= 26) || board == 30)) {
		z80pio_port_write(0, state ? 0 : 0x80);
		return;
	}
	if (board == 15 || board == 27 || (board >= 20 && board <= 22))
		return;
	if (board == 14) {
		if (state) {
			ZetSetVector(0xdf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		return;
	}
	if (board >= 7) {
		z80ctc_trg_write(((board >= 9 && board <= 13) || board == 16 || board == 18 || board == 19 || board >= 45) ? 1 : 0, state);
		z80ctc_trg_write(((board >= 9 && board <= 13) || board == 16 || board == 18 || board == 19 || board >= 45) ? 2 : 1, state);
		return;
	}
	if (state && m_ddenlovr_blitter_irq_enable) {
		m_ddenlovr_blitter_irq_flag = 1;
		SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
	}
}

#define logerror(...) ((void)0)
#define LOGBLIT(...) ((void)0)
enum { BLIT_NEXT = 0, BLIT_LINE, BLIT_COPY, BLIT_SKIP, BLIT_CHANGE_NUM, BLIT_CHANGE_PEN, BLIT_UNKNOWN, BLIT_STOP };

static const int ddenlovr_commands[8] = { BLIT_NEXT,    BLIT_LINE,       BLIT_COPY,       BLIT_SKIP,
                                          BLIT_UNKNOWN, BLIT_CHANGE_NUM, BLIT_CHANGE_PEN, BLIT_STOP };
static const int hanakanz_commands[8] = { BLIT_NEXT, BLIT_CHANGE_PEN, BLIT_CHANGE_NUM, BLIT_UNKNOWN,
                                          BLIT_SKIP, BLIT_COPY,       BLIT_LINE,       BLIT_STOP };
static const int mjflove_commands[8] = { BLIT_STOP, BLIT_CHANGE_PEN, BLIT_CHANGE_NUM, BLIT_UNKNOWN,
                                         BLIT_SKIP, BLIT_COPY,       BLIT_LINE,       BLIT_NEXT };
void ddenlovr_flipscreen_w(UINT8 data)
{
	logerror("flipscreen = %02x (%s)\n", data, (data & 1) ? "off" : "on");
}

void ddenlovr_blit_flip_w(UINT8 data)
{
	if ((data ^ m_ddenlovr_blit_flip) & 0xec) {
#ifdef MAME_DEBUG
		popmessage("warning ddenlovr_blit_flip = %02x", data);
#endif
		logerror("warning ddenlovr_blit_flip = %02x\n", data);
	}

	m_ddenlovr_blit_flip = data;
}

void ddenlovr_bgcolor_w(UINT8 data)
{
	m_ddenlovr_bgcolor = data;
}

void ddenlovr_bgcolor2_w(UINT8 data)
{
	m_ddenlovr_bgcolor2 = data;
}

void ddenlovr_priority_w(UINT8 data)
{
	m_ddenlovr_priority = data;
}

void ddenlovr_priority2_w(UINT8 data)
{
	m_ddenlovr_priority2 = data;
}

void ddenlovr_layer_enable_w(UINT8 data)
{
	m_ddenlovr_layer_enable = data;
}

void ddenlovr_layer_enable2_w(UINT8 data)
{
	m_ddenlovr_layer_enable2 = data;
}

void do_plot(int x, int y, int pen)
{
	int addr, temp;
	int xclip, yclip;

	y &= 0x1ff;
	x &= 0x1ff;

	// swap x & y (see hanakanz gal check)
	if (m_ddenlovr_blit_flip & 0x10) {
		temp = x;
		x = y;
		y = temp;
	}

	// clipping rectangle (see hanakanz / hkagerou gal check)
#if 0
	xclip   =   (x < m_ddenlovr_clip_x) || (x > m_ddenlovr_clip_x + m_ddenlovr_clip_width);
	yclip   =   (y < m_ddenlovr_clip_y) || (y > m_ddenlovr_clip_y + m_ddenlovr_clip_height);
#else
	xclip = (x < m_ddenlovr_clip_x) || (x > m_ddenlovr_clip_width);
	yclip = (y < m_ddenlovr_clip_y) || (y > m_ddenlovr_clip_height);
#endif

	if (!(m_ddenlovr_clip_ctrl & 1) && xclip)
		return;
	if (!(m_ddenlovr_clip_ctrl & 2) && !xclip)
		return;
	if (!(m_ddenlovr_clip_ctrl & 4) && yclip)
		return;
	if (!(m_ddenlovr_clip_ctrl & 8) && !yclip)
		return;

	addr = 512 * y + x;

	if (m_ddenlovr_dest_layer & 0x0001)
		m_ddenlovr_pixmap[0][addr] = pen;
	if (m_ddenlovr_dest_layer & 0x0002)
		m_ddenlovr_pixmap[1][addr] = pen;
	if (m_ddenlovr_dest_layer & 0x0004)
		m_ddenlovr_pixmap[2][addr] = pen;
	if (m_ddenlovr_dest_layer & 0x0008)
		m_ddenlovr_pixmap[3][addr] = pen;

	if (!m_extra_layers)
		return;

	if (m_ddenlovr_dest_layer & 0x0100)
		m_ddenlovr_pixmap[4][addr] = pen;
	if (m_ddenlovr_dest_layer & 0x0200)
		m_ddenlovr_pixmap[5][addr] = pen;
	if (m_ddenlovr_dest_layer & 0x0400)
		m_ddenlovr_pixmap[6][addr] = pen;
	if (m_ddenlovr_dest_layer & 0x0800)
		m_ddenlovr_pixmap[7][addr] = pen;
}

static inline int fetch_bit(UINT8 *src_data, int src_len, int *bit_addr)
{
	const int baddrmask = 0x7ffffff;

	int baddr = (*bit_addr) & baddrmask;

	*bit_addr = (baddr + 1) & baddrmask;

	if (baddr / 8 >= src_len) {
#ifdef MAME_DEBUG
//      popmessage("GFX ROM OVER %06x", baddr / 8);
#endif
		return 1;
	}

	return (src_data[baddr / 8] >> (7 - (baddr & 7))) & 1;
}

static inline int fetch_word(UINT8 *src_data, int src_len, int *bit_addr, int word_len)
{
	int res = 0;

	while (word_len--) {
		res = (res << 1) | fetch_bit(src_data, src_len, bit_addr);
	}
	return res;
}

inline void log_draw_error(int src, int cmd)
{
#ifdef MAME_DEBUG
	popmessage("%06x: warning unknown pixel command %02x", src, cmd);
#endif
	logerror("%06x: warning unknown pixel command %02x\n", src, cmd);
}

/*  Copy from ROM
    initialized arguments are
    0D/0E/0F source data pointer
    14 X
    02 Y
    00 dest layer
    05 unknown, related to layer
    04 blit_pen
    06 blit_pen_mode (replace values stored in ROM)
*/

int blit_draw(int src, int sx)
{
	UINT8 *src_data = GfxROM;
	int src_len = GfxLen;
	int bit_addr = (src & 0xffffff) * m_ddenlovr_blit_rom_bits; // convert to bit address
	int pen_size, arg_size, cmd;
	int x;
	int xinc = (m_ddenlovr_blit_flip & 1) ? -1 : 1;
	int yinc = (m_ddenlovr_blit_flip & 2) ? -1 : 1;

	pen_size = fetch_word(src_data, src_len, &bit_addr, 4) + 1;
	arg_size = fetch_word(src_data, src_len, &bit_addr, 4) + 1;

#ifdef MAME_DEBUG
//  if (pen_size > 4 || arg_size > 8)
//      popmessage("warning: pen_size %d arg_size %d", pen_size, arg_size);
#endif

	// sryudens game bug
	if (pen_size == 16 && arg_size == 16)
		return src;

	x = sx;

	for (INT32 guard = 0; guard < GfxLen * 8; guard++) {
		cmd = fetch_word(src_data, src_len, &bit_addr, 3);
		switch (m_ddenlovr_blit_commands[cmd]) {
			case BLIT_NEXT:
				/* next line */
				m_ddenlovr_blit_y += yinc;
				x = sx;
				break;

			case BLIT_LINE: {
				int length = fetch_word(src_data, src_len, &bit_addr, arg_size);
				int pen = fetch_word(src_data, src_len, &bit_addr, pen_size);

				if (m_ddenlovr_blit_pen_mode)
					pen = (m_ddenlovr_blit_pen & 0x0f);
				pen |= m_ddenlovr_blit_pen & 0xf0;

				while (length-- >= 0) {
					do_plot(x, m_ddenlovr_blit_y, pen);
					x += xinc;
				}
			} break;

			case BLIT_COPY: {
				int length = fetch_word(src_data, src_len, &bit_addr, arg_size);

				while (length-- >= 0) {
					int pen = fetch_word(src_data, src_len, &bit_addr, pen_size);
					if (m_ddenlovr_blit_pen_mode)
						pen = (m_ddenlovr_blit_pen & 0x0f);
					pen |= m_ddenlovr_blit_pen & 0xf0;

					do_plot(x, m_ddenlovr_blit_y, pen);
					x += xinc;
				}
			} break;

			case BLIT_SKIP:
				x += xinc * fetch_word(src_data, src_len, &bit_addr, arg_size);
				break;

			case BLIT_CHANGE_NUM:
				arg_size = fetch_word(src_data, src_len, &bit_addr, 4) + 1;
				break;

			case BLIT_CHANGE_PEN:
				pen_size = fetch_word(src_data, src_len, &bit_addr, 3) + 1;
				break;

			default:
				log_draw_error(src, cmd);

			case BLIT_STOP:
				return ((bit_addr + m_ddenlovr_blit_rom_bits - 1) / m_ddenlovr_blit_rom_bits) & 0xffffff;
		}
	}
	return src;
}

//  Draw a simple rectangle

void blit_rect_xywh()
{
	int x, y;

#ifdef MAME_DEBUG
//  if (m_ddenlovr_clip_ctrl != 0x0f)
//      popmessage("RECT clipx=%03x clipy=%03x ctrl=%x", m_ddenlovr_clip_x, m_ddenlovr_clip_y, m_ddenlovr_clip_ctrl);
#endif

	for (y = 0; y <= m_ddenlovr_rect_height; y++)
		for (x = 0; x <= m_ddenlovr_rect_width; x++)
			do_plot(x + m_ddenlovr_blit_x, y + m_ddenlovr_blit_y, m_ddenlovr_blit_pen);
}

/*  Unknown. Initialized arguments are
    00 dest layer
    05 unknown, related to layer
    14 X - always 0?
    02 Y
    0a width - always 0?
    0b height
    04 blit_pen
    0c line_length - always 0?
*/
void blit_rect_yh()
{
	int start = 512 * m_ddenlovr_blit_y;
	int length = 512 * (m_ddenlovr_rect_height + 1);

#ifdef MAME_DEBUG
//  if (m_ddenlovr_clip_ctrl != 0x0f)
//      popmessage("UNK8C clipx=%03x clipy=%03x ctrl=%x", m_ddenlovr_clip_x, m_ddenlovr_clip_y, m_ddenlovr_clip_ctrl);
#endif

	if (start >= 0 && start < 512 * 512) {
		if (start + length > 512 * 512)
			length = 512 * 512 - start;

		if (m_ddenlovr_dest_layer & 0x0001)
			memset(m_ddenlovr_pixmap[0] + start, m_ddenlovr_blit_pen, length);
		if (m_ddenlovr_dest_layer & 0x0002)
			memset(m_ddenlovr_pixmap[1] + start, m_ddenlovr_blit_pen, length);
		if (m_ddenlovr_dest_layer & 0x0004)
			memset(m_ddenlovr_pixmap[2] + start, m_ddenlovr_blit_pen, length);
		if (m_ddenlovr_dest_layer & 0x0008)
			memset(m_ddenlovr_pixmap[3] + start, m_ddenlovr_blit_pen, length);

		if (!m_extra_layers)
			return;

		if (m_ddenlovr_dest_layer & 0x0100)
			memset(m_ddenlovr_pixmap[4] + start, m_ddenlovr_blit_pen, length);
		if (m_ddenlovr_dest_layer & 0x0200)
			memset(m_ddenlovr_pixmap[5] + start, m_ddenlovr_blit_pen, length);
		if (m_ddenlovr_dest_layer & 0x0400)
			memset(m_ddenlovr_pixmap[6] + start, m_ddenlovr_blit_pen, length);
		if (m_ddenlovr_dest_layer & 0x0800)
			memset(m_ddenlovr_pixmap[7] + start, m_ddenlovr_blit_pen, length);
	}
}

/*  Fill from (X,Y) to end of ddenlovr_pixmap
    initialized arguments are
    00 dest layer
    05 unknown, related to layer
    14 X
    02 Y
    04 blit_pen
*/
void blit_fill_xy(int x, int y)
{
	int start = 512 * y + x;
	if (start < 0 || start >= 512 * 512)
		return;

#ifdef MAME_DEBUG
//  if (x || y)
//      popmessage("FILL command X %03x Y %03x", x, y);
#endif

	if (m_ddenlovr_dest_layer & 0x0001)
		memset(m_ddenlovr_pixmap[0] + start, m_ddenlovr_blit_pen, 512 * 512 - start);
	if (m_ddenlovr_dest_layer & 0x0002)
		memset(m_ddenlovr_pixmap[1] + start, m_ddenlovr_blit_pen, 512 * 512 - start);
	if (m_ddenlovr_dest_layer & 0x0004)
		memset(m_ddenlovr_pixmap[2] + start, m_ddenlovr_blit_pen, 512 * 512 - start);
	if (m_ddenlovr_dest_layer & 0x0008)
		memset(m_ddenlovr_pixmap[3] + start, m_ddenlovr_blit_pen, 512 * 512 - start);

	if (!m_extra_layers)
		return;

	if (m_ddenlovr_dest_layer & 0x0100)
		memset(m_ddenlovr_pixmap[4] + start, m_ddenlovr_blit_pen, 512 * 512 - start);
	if (m_ddenlovr_dest_layer & 0x0200)
		memset(m_ddenlovr_pixmap[5] + start, m_ddenlovr_blit_pen, 512 * 512 - start);
	if (m_ddenlovr_dest_layer & 0x0400)
		memset(m_ddenlovr_pixmap[6] + start, m_ddenlovr_blit_pen, 512 * 512 - start);
	if (m_ddenlovr_dest_layer & 0x0800)
		memset(m_ddenlovr_pixmap[7] + start, m_ddenlovr_blit_pen, 512 * 512 - start);
}

/*  Draw horizontal line
    initialized arguments are
    00 dest layer
    05 unknown, related to layer
    14 X
    02 Y
    0c line length
    04 blit_pen
    ddenlovr_blit_x and ddenlovr_blit_y are left pointing to the last pixel at the end of the command
*/
void blit_horiz_line()
{
	int i;

#ifdef MAME_DEBUG
	popmessage("LINE X");

	if (m_ddenlovr_clip_ctrl != 0x0f)
		popmessage("LINE X clipx=%03x clipy=%03x ctrl=%x", m_ddenlovr_clip_x, m_ddenlovr_clip_y, m_ddenlovr_clip_ctrl);

	if (m_ddenlovr_blit_flip)
		popmessage("LINE X flip=%x", m_ddenlovr_blit_flip);
#endif

	for (i = 0; i <= m_ddenlovr_line_length; i++)
		do_plot(m_ddenlovr_blit_x++, m_ddenlovr_blit_y, m_ddenlovr_blit_pen);
}

/*  Draw vertical line
    initialized arguments are
    00 dest layer
    05 unknown, related to layer
    14 X
    02 Y
    0c line length
    04 blit_pen
    ddenlovr_blit_x and ddenlovr_blit_y are left pointing to the last pixel at the end of the command
*/
void blit_vert_line()
{
	int i;

#ifdef MAME_DEBUG
	popmessage("LINE Y");

	if (m_ddenlovr_clip_ctrl != 0x0f)
		popmessage("LINE Y clipx=%03x clipy=%03x ctrl=%x", m_ddenlovr_clip_x, m_ddenlovr_clip_y, m_ddenlovr_clip_ctrl);
#endif

	for (i = 0; i <= m_ddenlovr_line_length; i++)
		do_plot(m_ddenlovr_blit_x, m_ddenlovr_blit_y++, m_ddenlovr_blit_pen);
}

inline void log_blit(int data)
{
	LOGBLIT(
	    "%s: blit src %06x x %03x y %03x flags %02x layer %02x pen %02x penmode %02x w %03x h %03x linelen %03x flip %02x clip: ctrl %x xy %03x %03x wh %03x %03x\n",
	    m_ddenlovr_blit_address, m_ddenlovr_blit_x, m_ddenlovr_blit_y, data, m_ddenlovr_dest_layer, m_ddenlovr_blit_pen,
	    m_ddenlovr_blit_pen_mode, m_ddenlovr_rect_width, m_ddenlovr_rect_height, m_ddenlovr_line_length, m_ddenlovr_blit_flip,
	    m_ddenlovr_clip_ctrl, m_ddenlovr_clip_x, m_ddenlovr_clip_y, m_ddenlovr_clip_width, m_ddenlovr_clip_height);
}

void blitter_w(int blitter, UINT32 offset, UINT8 data)
{
	int hi_bits;

	switch (offset) {
		case 0:
			m_ddenlovr_blit_regs[blitter] = data;
			break;

		case 1:
			hi_bits = (m_ddenlovr_blit_regs[blitter] & 0xc0) << 2;

			switch (m_ddenlovr_blit_regs[blitter] & 0x3f) {
				case 0x00:
					if (blitter)
						m_ddenlovr_dest_layer = (m_ddenlovr_dest_layer & 0x00ff) | (data << 8);
					else
						m_ddenlovr_dest_layer = (m_ddenlovr_dest_layer & 0xff00) | (data << 0);
					break;

				case 0x01:
					ddenlovr_flipscreen_w(data);
					break;

				case 0x02:
					m_ddenlovr_blit_y = data | hi_bits;
					break;

				case 0x03:
					ddenlovr_blit_flip_w(data);
					break;

				case 0x04:
					m_ddenlovr_blit_pen = data;
					break;

				case 0x05:
					m_ddenlovr_blit_pen_mask = data;
					break;

				case 0x06:
					// related to pen, can be 0 or 1 for 0x10 blitter command
					// 0 = only bits 7-4 of ddenlovr_blit_pen contain data
					// 1 = bits 3-0 contain data as well
					m_ddenlovr_blit_pen_mode = data;
					break;

				case 0x0a:
					m_ddenlovr_rect_width = data | hi_bits;
					break;

				case 0x0b:
					m_ddenlovr_rect_height = data | hi_bits;
					break;

				case 0x0c:
					m_ddenlovr_line_length = data | hi_bits;
					break;

				case 0x0d:
					m_ddenlovr_blit_address = (m_ddenlovr_blit_address & 0xffff00) | (data << 0);
					break;
				case 0x0e:
					m_ddenlovr_blit_address = (m_ddenlovr_blit_address & 0xff00ff) | (data << 8);
					break;
				case 0x0f:
					m_ddenlovr_blit_address = (m_ddenlovr_blit_address & 0x00ffff) | (data << 16);
					break;

				case 0x14:
					m_ddenlovr_blit_x = data | hi_bits;
					break;

				case 0x16:
					m_ddenlovr_clip_x = data | hi_bits;
					break;

				case 0x17:
					m_ddenlovr_clip_y = data | hi_bits;
					break;

				case 0x18:
				case 0x19:
				case 0x1a:
				case 0x1b:
				case 0x1c:
				case 0x1d:
				case 0x1e:
				case 0x1f:
					m_ddenlovr_scroll[blitter * 8 + (m_ddenlovr_blit_regs[blitter] & 7)] = data | hi_bits;
					break;

				case 0x20:
					m_ddenlovr_clip_ctrl = data;
					break;

				case 0x24:

					log_blit(data);

					switch (data) {
						case 0x04:
							blit_fill_xy(0, 0);
							break;
						case 0x14:
							blit_fill_xy(m_ddenlovr_blit_x, m_ddenlovr_blit_y);
							break;

						case 0x10:
							m_ddenlovr_blit_address = blit_draw(m_ddenlovr_blit_address, m_ddenlovr_blit_x);
							break;

						case 0x13:
							blit_horiz_line();
							break;
						case 0x1b:
							blit_vert_line();
							break;

						case 0x1c:
							blit_rect_xywh();
							break;

						// These two are issued one after the other (43 then 8c)
						// 8c is issued immediately after 43 has finished, without
						// changing any argument
						case 0x43:
							break;
						case 0x8c:
							blit_rect_yh();
							break;

						default:;
#ifdef MAME_DEBUG
							popmessage("unknown blitter command %02x", data);
							logerror("%06x: unknown blitter command %02x\n", m_maincpu->pc(), data);
#endif
					}

					BlitIrq(0);
					BlitIrq(1);
					break;

				default:
					logerror("%06x: Blitter %d reg %02x = %02x\n", m_maincpu->pc(), blitter, m_ddenlovr_blit_regs[blitter], data);
					break;
			}
	}
}

// differences wrt blitter_data_w: slightly different blitter commands
void blitter_w_funkyfig(int blitter, UINT32 offset, UINT8 data)
{
	int hi_bits;

	switch (offset) {
		case 0:
			m_ddenlovr_blit_regs[blitter] = data;
			break;

		case 1:
			hi_bits = (m_ddenlovr_blit_regs[blitter] & 0xc0) << 2;

			switch (m_ddenlovr_blit_regs[blitter] & 0x3f) {
				case 0x00:
					if (blitter)
						m_ddenlovr_dest_layer = (m_ddenlovr_dest_layer & 0x00ff) | (data << 8);
					else
						m_ddenlovr_dest_layer = (m_ddenlovr_dest_layer & 0xff00) | (data << 0);
					break;

				case 0x01:
					ddenlovr_flipscreen_w(data);
					break;

				case 0x02:
					m_ddenlovr_blit_y = data | hi_bits;
					break;

				case 0x03:
					ddenlovr_blit_flip_w(data);
					break;

				case 0x04:
					m_ddenlovr_blit_pen = data;
					break;

				case 0x05:
					m_ddenlovr_blit_pen_mask = data;
					break;

				case 0x06:
					// related to pen, can be 0 or 1 for 0x10 blitter command
					// 0 = only bits 7-4 of ddenlovr_blit_pen contain data
					// 1 = bits 3-0 contain data as well
					m_ddenlovr_blit_pen_mode = data;
					break;

				case 0x0a:
					m_ddenlovr_rect_width = data | hi_bits;
					break;

				case 0x0b:
					m_ddenlovr_rect_height = data | hi_bits;
					break;

				case 0x0c:
					m_ddenlovr_line_length = data | hi_bits;
					break;

				case 0x0d:
					m_ddenlovr_blit_address = (m_ddenlovr_blit_address & 0xffff00) | (data << 0);
					break;
				case 0x0e:
					m_ddenlovr_blit_address = (m_ddenlovr_blit_address & 0xff00ff) | (data << 8);
					break;
				case 0x0f:
					m_ddenlovr_blit_address = (m_ddenlovr_blit_address & 0x00ffff) | (data << 16);
					break;

				case 0x14:
					m_ddenlovr_blit_x = data | hi_bits;
					break;

				case 0x16:
					m_ddenlovr_clip_x = data | hi_bits;
					break;

				case 0x17:
					m_ddenlovr_clip_y = data | hi_bits;
					break;

				case 0x18:
				case 0x19:
				case 0x1a:
				case 0x1b:
				case 0x1c:
				case 0x1d:
				case 0x1e:
				case 0x1f:
					m_ddenlovr_scroll[blitter * 8 + (m_ddenlovr_blit_regs[blitter] & 7)] = data | hi_bits;
					break;

				case 0x20:
					m_ddenlovr_clip_ctrl = data;
					break;

				case 0x24:

					log_blit(data);

					switch (data) {
						case 0x84: // same as 04?
						case 0x04:
							blit_fill_xy(0, 0);
							break;

							//              unused?
							//              case 0x14:  blit_fill_xy(m_ddenlovr_blit_x, m_ddenlovr_blit_y);
							//                          break;

						case 0x00 /*0x10*/:
							m_ddenlovr_blit_address = blit_draw(m_ddenlovr_blit_address, m_ddenlovr_blit_x);
							break;

						case 0x0b: // same as 03? see the drawing of the R in "cRoss hatch" (key test)
						case 0x03 /*0x13*/:
							blit_horiz_line();
							break;
							//              unused?
							//              case 0x1b:  blit_vert_line();
							//                          break;

						case 0x0c /*0x1c*/:
							blit_rect_xywh();
							break;

						// These two are issued one after the other (43 then 8c)
						// 8c is issued immediately after 43 has finished, without
						// changing any argument
						case 0x43:
							break;
						case 0x8c:
							blit_rect_yh();
							break;

						default:;
#ifdef MAME_DEBUG
							popmessage("unknown blitter command %02x", data);
							logerror("%s: unknown blitter command %02x\n", machine().describe_context(), data);
#endif
					}

					BlitIrq(0);
					BlitIrq(1);
					break;

				default:
					logerror("%s: Blitter %d reg %02x = %02x\n", machine().describe_context(), blitter, m_ddenlovr_blit_regs[blitter],
					         data);
					break;
			}
	}
}

void hanakanz_blitter_reg_w(UINT8 data)
{
	m_ddenlovr_blit_latch = data;
}

// differences wrt blitter_data_w: registers are shuffled around, hi_bits in the low bits, clip_w/h, includes layers registers
void hanakanz_blitter_data_w(UINT8 data)
{
	int hi_bits;

	hi_bits = (m_ddenlovr_blit_latch & 0x03) << 8;

	switch (m_ddenlovr_blit_latch & 0xfe) {
		case 0x00:
			m_ddenlovr_dest_layer = data;
			break;

		case 0x04:
			ddenlovr_flipscreen_w(data);
			break;

		case 0x08:
			m_ddenlovr_blit_y = data | hi_bits;
			break;

		case 0x0c:
			ddenlovr_blit_flip_w(data);
			break;

		case 0x10:
			m_ddenlovr_blit_pen = data;
			break;

		case 0x14:
			m_ddenlovr_blit_pen_mask = data;
			break;

		case 0x18:
			// related to pen, can be 0 or 1 for 0x10 blitter command
			// 0 = only bits 7-4 of ddenlovr_blit_pen contain data
			// 1 = bits 3-0 contain data as well
			m_ddenlovr_blit_pen_mode = data;
			break;

		case 0x28:
			m_ddenlovr_rect_width = data | hi_bits;
			break;

		case 0x2c:
			m_ddenlovr_rect_height = data | hi_bits;
			break;

		case 0x30:
			m_ddenlovr_line_length = data | hi_bits;
			break;

		case 0x34:
			m_ddenlovr_blit_address = (m_ddenlovr_blit_address & 0xffff00) | (data << 0);
			break;
		case 0x38:
			m_ddenlovr_blit_address = (m_ddenlovr_blit_address & 0xff00ff) | (data << 8);
			break;
		case 0x3c:
			m_ddenlovr_blit_address = (m_ddenlovr_blit_address & 0x00ffff) | (data << 16);
			break;

		case 0x50:
			m_ddenlovr_blit_x = data | hi_bits;
			break;

		case 0x58:
			m_ddenlovr_clip_x = data | hi_bits;
			break;

		case 0x5c:
			m_ddenlovr_clip_y = data | hi_bits;
			break;

		case 0x60:
		case 0x64:
		case 0x68:
		case 0x6c:
		case 0x70:
		case 0x74:
		case 0x78:
		case 0x7c:
			m_ddenlovr_scroll[(m_ddenlovr_blit_latch & 0x1c) >> 2] = data | hi_bits;
			break;

		case 0x80:
			m_ddenlovr_clip_ctrl = data;
			break;

		case 0x88:
		case 0x8a: // can be 3ff
			m_ddenlovr_clip_height = data | hi_bits;
			break;

		case 0x8c:
		case 0x8e: // can be 3ff
			m_ddenlovr_clip_width = data | hi_bits;
			break;

		case 0xc0:
		case 0xc2:
		case 0xc4:
		case 0xc6:
			m_ddenlovr_palette_base[(m_ddenlovr_blit_latch >> 1) & 3] = data | (hi_bits & 0x100);
			break;

		case 0xc8:
		case 0xca:
		case 0xcc:
		case 0xce:
			m_ddenlovr_palette_mask[(m_ddenlovr_blit_latch >> 1) & 3] = data;
			break;

		case 0xd0:
		case 0xd2:
		case 0xd4:
		case 0xd6:
			m_ddenlovr_transparency_pen[(m_ddenlovr_blit_latch >> 1) & 3] = data;
			break;

		case 0xd8:
		case 0xda:
		case 0xdc:
		case 0xde:
			m_ddenlovr_transparency_mask[(m_ddenlovr_blit_latch >> 1) & 3] = data;
			break;

		case 0xe4:
			ddenlovr_priority_w(data);
			break;

		case 0xe6:
			ddenlovr_layer_enable_w(data);
			break;

		case 0xe8:
			m_ddenlovr_bgcolor = data | hi_bits;
			break;

		case 0x90:

			log_blit(data);

			switch (data) {
				case 0x04:
					blit_fill_xy(0, 0);
					break;
				case 0x14:
					blit_fill_xy(m_ddenlovr_blit_x, m_ddenlovr_blit_y);
					break;

				case 0x10:
					m_ddenlovr_blit_address = blit_draw(m_ddenlovr_blit_address, m_ddenlovr_blit_x);
					break;

				case 0x13:
					blit_horiz_line();
					break;
				case 0x1b:
					blit_vert_line();
					break;

				case 0x1c:
					blit_rect_xywh();
					break;

				// These two are issued one after the other (43 then 8c)
				// 8c is issued immediately after 43 has finished, without
				// changing any argument
				case 0x43:
					break;
				case 0x8c:
					blit_rect_yh();
					break;

				default:;
#ifdef MAME_DEBUG
					popmessage("unknown blitter command %02x", data);
					logerror("%06x: unknown blitter command %02x\n", m_maincpu->pc(), data);
#endif
			}

			// NO IRQ !?

			break;

		default:
			logerror("%06x: Blitter 0 reg %02x = %02x\n", m_maincpu->pc(), m_ddenlovr_blit_latch, data);
			break;
	}
}

// Board wiring follows the address maps in MAME 0.289.
enum {
	BOARD_DDENLOVR,
	BOARD_DDENLOVJ,
	BOARD_DDENLOVRK,
	BOARD_NETTOQC,
	BOARD_ULTRCHMP,
	BOARD_QUIZ365,
	BOARD_AKAMARU,
	BOARD_QUIZCHQ,
	BOARD_RONGRONG,
	BOARD_MJMYSTER,
	BOARD_MJMYWRLD,
	BOARD_MJMYUNIV,
	BOARD_MJMYWRLDT,
	BOARD_MJMYORNT,
	BOARD_MMPANIC,
	BOARD_FUNKYFIG,
	BOARD_HGINGA,
	BOARD_HPARADIS,
	BOARD_MJSCHUKA,
	BOARD_MJMYORNTR,
	BOARD_SRYUDENS,
	BOARD_JANSHINP,
	BOARD_DTOYOKEN,
	BOARD_SELJAN2,
	BOARD_JONGOH,
	BOARD_DAICHUKA,
	BOARD_DAICHUKAA,
	BOARD_MJFLOVE,
	BOARD_MJCHUUKA,
	BOARD_HTENGOKU,
	BOARD_DAIREACH,
	BOARD_HANAKANZ,
	BOARD_HKAGEROU,
	BOARD_KOTBINYO,
	BOARD_KOTBINSP,
	BOARD_MJREACH1,
	BOARD_JONGTEI,
	BOARD_JONGTEIA,
	BOARD_MJGNIGHT,
	BOARD_HTSUBAKI,
	BOARD_DAIMYOJN,
	BOARD_HNROSE,
	BOARD_MJSWACADS,
	BOARD_MOMOTARO,
	BOARD_MJNIGIRI,
	BOARD_RSELECT,
	BOARD_HGOKOU,
	BOARD_HGOKBANG
};

#define KL_BOARD (board >= BOARD_HANAKANZ && board <= BOARD_MJNIGIRI)
#define GOK_BOARD (board >= BOARD_RSELECT)
#define KL_LATE (KL_BOARD && board >= BOARD_JONGTEI)
#define KL_MUX (KL_BOARD && (board == BOARD_JONGTEIA || board >= BOARD_DAIMYOJN))
#define HANA_BOARD (board == BOARD_MJCHUUKA || KL_BOARD)
#define SEL_BOARD ((board >= BOARD_SELJAN2 && board <= BOARD_DAICHUKAA) || board == BOARD_DAIREACH)
#define SRY_BOARD (board >= BOARD_SRYUDENS && board <= BOARD_DTOYOKEN)
#define LATE_MAHJONG (board == BOARD_MJSCHUKA || board == BOARD_MJMYORNTR)
#define SOUND_CPU (board == BOARD_MMPANIC || board == BOARD_FUNKYFIG)
static UINT8 lockout, mjfloveIrqCause, mjfloveIrqPending;
static void ZIrq(INT32);
static INT32 ExternalIrqState()
{
	return mjfloveIrqPending ? Z80_DAISY_INT : 0;
}

static INT32 ExternalIrqAck()
{
	mjfloveIrqPending = 0;
	ZIrq(0);
	return 0xff;
}

static void ExternalIrq(UINT8 cause)
{
	mjfloveIrqCause = cause;
	mjfloveIrqPending = 1;
	ZIrq(0);
}

static UINT32 hopperTicks;
static UINT16 hanaPalette[512], hanaPaletteIndex;
static UINT8 hanaGfxLatch, hanaPaletteSelect;
// Dynax rev. 2 command stream and interleaved pixels, from MAME's
// dynax_blitter_rev2.cpp (BSD-3-Clause, Luca Elia and the MAME team).
struct Rev2State {
	UINT32 src;
	UINT8 x, y, pen, flags, wrap, bank, dest, flip, half, half2, irqMask, irq;
};

static Rev2State rev2;
static void Rev2Irq()
{
	ZetSetVector(0xc7 | rev2.irq);
	ZetSetIRQLine(0, rev2.irq ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void Rev2Pixel(INT32 offset, UINT8 pen)
{
	if (rev2.flip)
		offset ^= 0xffff;
	if (rev2.dest & 16)
		pen |= (rev2.pen & 8) << 1;
	offset = (offset << 1) | (1 ^ rev2.half ^ rev2.flip);
	for (INT32 l = 0; l < 4; l++)
		if (!(rev2.dest & (1 << l))) {
			m_ddenlovr_pixmap[l][offset] = pen;
			m_ddenlovr_pixmap[l][offset + 0x20000] = pen;
			if (rev2.half2) {
				m_ddenlovr_pixmap[l][offset ^ 1] = pen;
				m_ddenlovr_pixmap[l][(offset + 0x20000) ^ 1] = pen;
			}
		}
}

static void Rev2Plot(INT32 x, INT32 y, UINT8 pen)
{
	if (y > 255 && !(rev2.wrap & 2))
		return;
	if (x > 255 && !(rev2.wrap & 1))
		return;
	x &= 255;
	y &= 255;
	if (rev2.flags & 8) {
		INT32 t = x;
		x = y;
		y = t;
	}
	Rev2Pixel(x | (y << 8), pen);
}

static UINT8 Rev2Byte(UINT32 &src)
{
	UINT32 a = rev2.bank * 0x100000 + (src & 0xfffff);
	src = (src + 1) & 0xfffff;
	return a < UINT32(GfxLen) ? GfxROM[a] : 255;
}

static void Rev2Draw()
{
	INT32 x = rev2.x, y = rev2.y, sx = x, pen = rev2.pen >> 4;
	UINT32 src = rev2.src & 0xfffff;
	if (rev2.flags & 1) {
		for (INT32 a = x | (y << 8); a < 0x10000; a++)
			Rev2Pixel(a, pen);
	} else
		for (INT32 guard = 0; guard < 0x100000; guard++) {
			UINT8 cmd = Rev2Byte(src);
			if (!(rev2.flags & 2))
				pen = cmd >> 4;
			cmd &= 15;
			if (!cmd)
				break;
			if (cmd == 15) {
				y += (rev2.flags & 8) ? -1 : 1;
				x = sx;
				continue;
			}
			if (cmd == 14)
				continue;
			if (cmd == 13) {
				x = sx + Rev2Byte(src);
				cmd = Rev2Byte(src);
			} else if (cmd == 12)
				cmd = Rev2Byte(src);
			while (cmd--)
				Rev2Plot(x++, y, pen);
		}
	rev2.src = (rev2.src & ~0xfffff) | (src & 0xfffff);
	if (rev2.irqMask) {
		rev2.irq |= 0x20;
		Rev2Irq();
	}
}

static void Rev2Reg(UINT8 p, UINT8 d)
{
	switch (p) {
		case 1:
			rev2.flags = d;
			Rev2Draw();
			break;
		case 2:
			rev2.x = d;
			break;
		case 3:
			rev2.y = d;
			break;
		case 4:
			rev2.src = (rev2.src & 0xffff00) | d;
			break;
		case 5:
			rev2.src = (rev2.src & 0xff00ff) | (d << 8);
			break;
		case 6:
			rev2.src = (rev2.src & 0xffff) | (d << 16);
			break;
		case 7:
			if (rev2.src & 0x800000)
				rev2.wrap = d;
			else
				for (INT32 l = 0; l < 4; l++)
					if (l != 2)
						m_ddenlovr_scroll[l + ((rev2.src & 0x400000) ? 4 : 0)] = (rev2.src & 0x400000) ? d : d * 2;
			break;
	}
}

#define MAHJONG_BOARD (board >= 9 && board <= 13)
#define TMP_BOARD (board >= 7 && board != BOARD_MMPANIC && board != BOARD_HTENGOKU && !KL_BOARD)
static UINT8 *SoundROM;
static UINT8 soundLatch;
static INT32 soundExtra;
static FILE *trace;
static INT32 traceFrames, traceWrites;
static UINT64 rtcIrqTicks;
static UINT8 romBank, ramBank, keyRow;
static UINT8 MahjongDsw(UINT32)
{
	for (INT32 i = 0; i < 5; i++)
		if (!(dswSel & (1 << i))) {
			INT32 n = i < 4 ? 3 - i : 4;
			UINT8 d = Dips[n];
			if (MAHJONG_BOARD && n == 1)
				d = (d & ~3) | ((Dips[5] >> 1) & 3);
			if (MAHJONG_BOARD && n == 4)
				d = (d & ~0x40) | ((Dips[5] & 1) << 6);
			return d;
		}
	return 0xff;
}
// TMPZ84C015 SIO register interface. These boards have no serial peer;
// channel B CTS is driven by the blitter, not by a received serial byte.
static UINT8 sioWr[2][8], sioStatus[2];
static UINT8 SioRead(UINT8 port)
{
	INT32 ch = (port >> 1) & 1;
	if (!(port & 1))
		return 0;
	INT32 reg = sioWr[ch][0] & 7;
	sioWr[ch][0] &= ~7;
	if (reg == 0)
		return sioStatus[ch];
	if (reg == 1)
		return 1;
	if (reg == 2)
		return sioWr[1][2];
	return 0;
}

static void SioWrite(UINT8 port, UINT8 data)
{
	INT32 ch = (port >> 1) & 1;
	if (!(port & 1)) {
		sioStatus[ch] &= ~4;
		return;
	}
	INT32 reg = sioWr[ch][0] & 7;
	sioWr[ch][0] &= ~7;
	sioWr[ch][reg] = data;
	if (!reg && (data & 0x38) == 0x18) {
		sioWr[ch][3] &= ~1;
		sioWr[ch][5] &= ~8;
		sioStatus[ch] = (sioStatus[ch] & 0x28) | 0x54;
	}
}
// license:BSD-3-Clause
// KL5C80A12 peripherals adapted from MAME (AJR): KC82 MMU, KP69,
// KP65/KP66 parallel ports and the board-connected KP63/KP64 timers.
// The caller owns scan/reset and advances timers on the emulated CPU clock.
struct KlTimer {
	UINT16 cr, count, latch;
	INT64 end, pulse, pwm;
	UINT8 status, tmp, seq, readSeq, started;
};

struct KlState {
	UINT16 a[4], levels, irr, isr, imr, ler, pgr;
	UINT8 b[5], ivr, ivrWritten, illegal, active;
	UINT8 pa[2], adir[2], pb[3], bdir;
	INT64 ticks;
	INT32 lastCpu;
	KlTimer timer[5];
};

static KlState kl;
static UINT32 klBase[64];
static void KlMap();
static void KlPortOut(INT32, UINT8);
static UINT8 KlPortIn(INT32);
static INT32 KlHighest(UINT16 bits)
{
	for (INT32 i = 15; i >= 0; i--)
		if (bits & (1 << i))
			return i;
	return -1;
}

static INT32 KlActive()
{
	if (kl.illegal)
		return 0;
	if ((kl.irr & kl.pgr) || (kl.isr & kl.pgr))
		return (kl.irr & ~kl.isr & kl.pgr) > (kl.isr & kl.pgr);
	return (kl.irr & ~kl.isr) > kl.isr;
}

static void KlLine()
{
	ZetSetIRQLine(0, kl.active ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void KlUpdate()
{
	kl.active = KlActive();
	KlLine();
}

static INT32 KlIrqState()
{
	return Z80_DAISY_IEO | (kl.active ? Z80_DAISY_INT : 0);
}

static void KlInput(INT32 n, INT32 state)
{
	UINT16 bit = 1 << n;
	if (state && !(kl.levels & bit)) {
		kl.levels |= bit;
		if (!(kl.irr & bit) && !(kl.imr & bit)) {
			UINT16 old = kl.irr | kl.isr;
			kl.irr |= bit;
			if (!kl.illegal && bit > ((kl.pgr & bit) ? old & kl.pgr : old)) {
				kl.active = 1;
				KlLine();
			}
		}
	} else if (!state && (kl.levels & bit)) {
		kl.levels &= ~bit;
		if (!(kl.ler & bit) && (kl.irr & bit)) {
			kl.irr &= ~bit;
			if (!kl.illegal)
				KlUpdate();
		}
	}
}

static INT32 KlIrqAck()
{
	INT32 n = -1;
	if (kl.irr & kl.pgr) {
		n = KlHighest(kl.irr & kl.pgr);
		if ((1 << n) < (kl.isr & kl.pgr))
			n = -1;
	} else if (kl.irr && !(kl.isr & kl.pgr)) {
		n = KlHighest(kl.irr);
		if ((1 << n) < kl.isr)
			n = -1;
	}
	if (n >= 0) {
		if (kl.ler & (1 << n))
			kl.irr &= ~(1 << n);
		kl.isr |= 1 << n;
		kl.active = 0;
		KlLine();
		return kl.ivr | (n << 1);
	}
	kl.illegal = 1;
	kl.active = 0;
	KlLine();
	return kl.ivr;
}

static void KlReti()
{
	if (kl.illegal)
		kl.illegal = 0;
	else if (kl.isr)
		kl.isr &= ~(1 << KlHighest((kl.isr & kl.pgr) ? kl.isr & kl.pgr : kl.isr));
	else
		return;
	KlUpdate();
}

static void KlIrqWrite(UINT8 p, UINT8 d)
{
	if (p < 0x36) {
		INT32 shift = (p & 1) * 8;
		UINT16 mask = 255 << shift;
		if (kl.ivrWritten) {
			UINT16 value = (kl.pgr & ~mask) | (d << shift);
			if (kl.pgr != value) {
				kl.pgr = value;
				if (!kl.illegal && kl.isr)
					KlUpdate();
			}
		} else {
			UINT16 value = (kl.ler & ~mask) | (d << shift), old = kl.irr;
			kl.irr = (kl.levels & ~value) | (kl.irr & kl.ler & value);
			kl.ler = value;
			if (old != kl.irr)
				KlUpdate();
		}
	} else if (p == 0x37 && !kl.ivrWritten) {
		kl.ivr = d & 0xe0;
		kl.ivrWritten = 1;
	} else {
		INT32 shift = (p & 1) * 8;
		UINT16 old = kl.irr;
		kl.imr = (kl.imr & ~(255 << shift)) | (d << shift);
		kl.irr = (kl.irr & ~kl.imr & kl.ler) | (kl.levels & ~kl.imr & ~kl.ler);
		if (old != kl.irr)
			KlUpdate();
	}
}

static void KlRemap()
{
	INT32 n = 4;
	UINT32 base = 0xf0000;
	klBase[0] = 0;
	for (INT32 i = 63; i > 0; i--) {
		while (n && kl.b[n] >= i) {
			n--;
			base = kl.a[n] << 10;
		}
		klBase[i] = base;
	}
	KlMap();
}

static INT32 KlScale(const KlTimer &t)
{
	return (t.status & 2) ? 4 : (t.status & 1) ? 16 : 256;
}

static UINT16 KlCount(INT32 n)
{
	KlTimer &t = kl.timer[n];
	if (t.end < 0)
		return t.count;
	INT64 remaining = t.end - kl.ticks;
	if (remaining < 0)
		remaining = 0;
	if (n < 2)
		return remaining > 65535 ? 65535 : UINT16(remaining);
	INT32 count = INT32(remaining / KlScale(t));
	if ((t.status & 12) == 12)
		return (count & 255) | ((t.cr - (count << 8)) & 0xff00);
	return UINT16(count);
}

static void KlOutput(INT32 n, INT32 state)
{
	KlTimer &t = kl.timer[n];
	if (!!(t.status & 64) != !!state) {
		t.status = (t.status & ~64) | (state ? 64 : 0);
		KlInput(11 + n, state);
	}
}

static void KlReload(INT32 n)
{
	KlTimer &t = kl.timer[n];
	t.started = 1;
	if (n >= 2) {
		t.count = t.cr;
		INT32 scale = KlScale(t);
		t.end = kl.ticks + scale * ((((t.status & 12) == 12) ? t.cr & 255 : t.cr) + 1);
		t.pwm = ((t.status & 12) == 12) ? kl.ticks + scale * ((t.cr >> 8) + 1) : -1;
		return;
	}
	t.count = (t.status & 32) ? 65535 : t.cr;
	t.end = (t.status & 1) && t.count ? kl.ticks + t.count + 1 : -1;
	switch ((t.status >> 3) & 7) {
		case 0:
			if (t.status & 4)
				KlOutput(n, 0);
			break;
		case 1:
			KlOutput(n, 1);
			break;
		case 2:
			KlOutput(n, !!(t.status & 2));
			break;
		case 3:
			KlOutput(n, !(t.status & 2));
			break;
	}
}

static void KlExpire(INT32 n)
{
	KlTimer &t = kl.timer[n];
	t.end = -1;
	if (n >= 2) {
		t.status ^= 128;
		KlInput(11 + n, 1);
		t.pulse = kl.ticks + 4;
		if (t.status & 4)
			KlReload(n);
		else {
			t.count = 65535;
			t.started = 0;
		}
		return;
	}
	if (t.status & 32) {
		t.count = 65535;
		if (t.status & 1)
			t.end = kl.ticks + 65536;
	} else if (t.status & 16) {
		KlOutput(n, !(t.status & 64));
		if ((t.status & 9) == 9)
			t.pulse = kl.ticks + 1;
		t.started = 0;
	} else if (t.status & 8) {
		t.count = ((t.status & 64) ? ~t.cr : t.cr) & ((0x40 << (t.status & 6)) - 1);
		if (t.status & 1)
			t.end = kl.ticks + t.count + 1;
		KlOutput(n, !(t.status & 64));
	} else {
		t.count = t.cr;
		if (t.status & 1) {
			t.end = kl.ticks + t.count + 1;
			if (t.status & 4)
				t.pulse = kl.ticks + 1;
		}
		KlOutput(n, (t.status & 4) || !(t.status & 64));
	}
}

static void KlAdvance(INT64 target)
{
	for (;;) {
		INT64 next = target + 1;
		for (INT32 n = 0; n < 5; n++) {
			KlTimer &t = kl.timer[n];
			if (t.end >= 0 && t.end < next)
				next = t.end;
			if (t.pulse >= 0 && t.pulse < next)
				next = t.pulse;
			if (t.pwm >= 0 && t.pwm < next)
				next = t.pwm;
		}
		if (next > target)
			break;
		kl.ticks = next;
		for (INT32 n = 0; n < 5; n++) {
			KlTimer &t = kl.timer[n];
			if (t.pulse == next) {
				t.pulse = -1;
				if (n >= 2)
					KlInput(11 + n, 0);
				else
					KlOutput(n, (t.status & 16) && !(t.status & 2));
			}
			if (t.pwm == next) {
				t.pwm = -1;
				t.status &= 127;
			}
			if (t.end == next)
				KlExpire(n);
		}
	}
	kl.ticks = target;
}

static void KlSync()
{
	INT32 now = ZetTotalCycles(), delta = now - kl.lastCpu;
	if (delta > 0) {
		KlAdvance(kl.ticks + delta);
		kl.lastCpu = now;
	}
}

static UINT8 KlTimerRead(UINT8 p)
{
	INT32 n = p >= 0x28 ? (p - 0x28) / 2 : (p - 0x20) / 2 + 2;
	KlTimer &t = kl.timer[n];
	if (p & 1) {
		if (n >= 2)
			t.seq = 0;
		return n < 2 && (t.status & 32) ? t.status & ~64 : t.status;
	}
	if (n < 2) {
		UINT16 v = (t.status & 32) ? t.cr : t.latch;
		UINT8 d = v >> (t.readSeq ? 8 : 0);
		t.readSeq ^= 1;
		return d;
	}
	if (t.seq) {
		t.seq = 0;
		return t.tmp;
	}
	UINT16 c = KlCount(n);
	t.tmp = c >> 8;
	t.seq = 1;
	return c;
}

static void KlTimerWrite(UINT8 p, UINT8 d)
{
	INT32 n = p >= 0x28 ? (p - 0x28) / 2 : (p - 0x20) / 2 + 2;
	KlTimer &t = kl.timer[n];
	if (n >= 2) {
		if (p & 1) {
			if (t.started)
				t.count = KlCount(n);
			t.end = t.pwm = -1;
			t.started = 0;
			t.status = d & 31;
		} else if ((t.status & 12) == 8)
			KlReload(n);
		else if (t.seq) {
			t.seq = 0;
			t.cr = (d << 8) | t.tmp;
			if (!(t.status & 8) || !t.started) {
				t.status |= 128;
				KlReload(n);
			}
		} else {
			t.seq = 1;
			t.tmp = d;
		}
		return;
	}
	if (!(p & 1)) {
		if (t.seq) {
			t.seq = 0;
			t.cr = (d << 8) | t.tmp;
			if (!t.started && !(t.status & 48) && t.status & 1)
				KlReload(n);
		} else {
			t.tmp = d;
			t.seq = 1;
		}
		return;
	}
	INT32 mode = (d >> 3) & 7;
	if (mode <= 5) {
		t.status = d & (mode == 0 ? 0x3d : 0x3f);
		t.cr = 65535;
		t.count = mode == 1 ? (0x40 << (d & 6)) - 1 : 65535;
		t.started = 0;
		t.end = t.pulse = -1;
		KlInput(11 + n, 0);
		if (mode == 2 || mode == 3)
			KlOutput(n, !(d & 2));
	} else if (mode == 7) {
		if (!(d & 3))
			t.latch = KlCount(n);
		else {
			if (d & 1)
				t.status &= 127;
			if (d & 2)
				t.seq = t.readSeq = 0;
		}
	}
}

static UINT8 KlDirection(INT32 n)
{
	static const UINT8 dirs[4] = { 0, 15, 240, 255 };
	return n < 2 ? kl.adir[n] : dirs[(kl.bdir >> ((n - 2) * 2)) & 3];
}

static void KlPortUpdate(INT32 n)
{
	UINT8 dir = KlDirection(n);
	KlPortOut(n, (n < 2 ? kl.pa[n] : kl.pb[n - 2]) & dir);
}

static UINT8 KlRead(UINT8 p)
{
	KlSync();
	if (p < 8) {
		INT32 n = p / 2 + 1;
		return (p & 1) ? n == 4 ? 0xf0 : kl.a[n] >> 2 : (n == 4 ? 0 : (kl.a[n] & 3) << 6) | kl.b[n];
	}
	if ((p >= 0x20 && p <= 0x25) || (p >= 0x28 && p <= 0x2b))
		return KlTimerRead(p);
	if (p >= 0x2c && p <= 0x32) {
		INT32 n = p < 0x30 ? (p - 0x2c) / 2 : p - 0x30 + 2;
		if (p < 0x30 && (p & 1))
			return kl.adir[n];
		UINT8 dir = KlDirection(n);
		return ((n < 2 ? kl.pa[n] : kl.pb[n - 2]) & dir) | (KlPortIn(n) & ~dir);
	}
	if (p == 0x33)
		return kl.bdir;
	if (p == 0x34 || p == 0x35)
		return kl.isr >> ((p & 1) * 8);
	if (p == 0x36 || p == 0x37)
		return kl.imr >> ((p & 1) * 8);
	return 255;
}

static void KlWrite(UINT8 p, UINT8 d)
{
	KlSync();
	if (p < 8) {
		INT32 n = p / 2 + 1;
		if (p & 1) {
			if (n < 4)
				kl.a[n] = (d << 2) | (kl.a[n] & 3);
		} else {
			kl.b[n] = d & 63;
			if (n < 4)
				kl.a[n] = (kl.a[n] & 0x3fc) | (d >> 6);
		}
		KlRemap();
		return;
	}
	if ((p >= 0x20 && p <= 0x25) || (p >= 0x28 && p <= 0x2b)) {
		KlTimerWrite(p, d);
		ZetRunEnd();
		return;
	}
	if (p >= 0x2c && p <= 0x32) {
		INT32 n = p < 0x30 ? (p - 0x2c) / 2 : p - 0x30 + 2;
		if (p < 0x30 && (p & 1)) {
			if (kl.adir[n] == d)
				return;
			kl.adir[n] = d;
		} else {
			UINT8 &v = n < 2 ? kl.pa[n] : kl.pb[n - 2];
			UINT8 changed = v ^ d;
			v = d;
			if (!(changed & KlDirection(n)))
				return;
		}
		KlPortUpdate(n);
		return;
	}
	if (p == 0x33) {
		if ((d & 0xc0) == 0xc0) {
			INT32 b = (d >> 1) & 7, n = (d >> 4) & 3;
			if (n < 3) {
				UINT8 old = kl.pb[n];
				kl.pb[n] = (old & ~(1 << b)) | ((d & 1) << b);
				if ((old ^ kl.pb[n]) & KlDirection(n + 2))
					KlPortUpdate(n + 2);
			} else if (b < 6) {
				UINT8 old = kl.bdir;
				kl.bdir = (old & ~(1 << b)) | ((d & 1) << b);
				if (old != kl.bdir)
					KlPortUpdate(2 + b / 2);
			}
			return;
		}
		if (d & 0xc0)
			return;
		UINT8 old = kl.bdir;
		kl.bdir = d & 63;
		for (INT32 n = 0; n < 3; n++)
			if ((old ^ kl.bdir) & (3 << (n * 2)))
				KlPortUpdate(n + 2);
		return;
	}
	if (p >= 0x34 && p <= 0x37)
		KlIrqWrite(p, d);
}

static void KlReset()
{
	memset(&kl, 0, sizeof(kl));
	kl.imr = 65535;
	for (INT32 i = 1; i < 5; i++)
		kl.b[i] = 63;
	for (INT32 n = 0; n < 5; n++) {
		KlTimer &t = kl.timer[n];
		t.cr = t.count = t.latch = 65535;
		t.status = n < 2 ? 1 : 0;
		t.end = t.pulse = t.pwm = -1;
	}
	KlRemap();
}

static INT32 KlRun(INT32 cycles)
{
	INT32 start = ZetTotalCycles();
	while (ZetTotalCycles() - start < cycles) {
		KlSync();
		INT32 run = cycles - (ZetTotalCycles() - start);
		for (INT32 n = 0; n < 5; n++) {
			KlTimer &t = kl.timer[n];
			if (t.end >= kl.ticks && t.end - kl.ticks < run)
				run = INT32(t.end - kl.ticks);
			if (t.pulse >= kl.ticks && t.pulse - kl.ticks < run)
				run = INT32(t.pulse - kl.ticks);
			if (t.pwm >= kl.ticks && t.pwm - kl.ticks < run)
				run = INT32(t.pwm - kl.ticks);
		}
		if (run < 1)
			run = 1;
		ZetRun(run);
	}
	KlSync();
	return ZetTotalCycles() - start;
}

static UINT8 rtcEnabled, rtcLine;
static INT32 rtcPulse;
static void RtcLine(INT32 state)
{
	if (state == rtcLine)
		return;
	rtcLine = state;
	if (state)
		rtcRegs[13] |= 4;
	else
		rtcRegs[13] &= ~4;
	if (KL_BOARD)
		KlInput(1, state);
	else if (board == BOARD_HTENGOKU) {
		rev2.irq = (rev2.irq & ~8) | (state ? 8 : 0);
		Rev2Irq();
	} else if (board == BOARD_MMPANIC) {
		if (state) {
			ZetSetVector(0xdf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
	} else if (board == BOARD_MJFLOVE) {
		if (state) {
			ExternalIrq(2);
		}
	} else if (MAHJONG_BOARD) {
		if (state)
			ZetNmi();
	} else if (SRY_BOARD || SEL_BOARD || HANA_BOARD)
		z80ctc_trg_write(1, state);
	else if ((board == BOARD_HGINGA || LATE_MAHJONG || GOK_BOARD))
		z80pio_port_write(0, state ? 0 : 0x80);
	else if (TMP_BOARD)
		z80ctc_trg_write(board == BOARD_FUNKYFIG ? 1 : 2, !state);
}

static void RtcWrite(INT32 n, UINT8 d)
{
	// MSM6242 calendar registers are writable. Dai Chuuka Ken uses the
	// programmed date as its inspection countdown, not the host calendar.
	if (n < 13) {
		rtcRegs[n] = d & 15;
	} else if (n == 13) {
		if (!(d & 4))
			RtcLine(0);
		rtcRegs[13] = (d & 9) | (rtcRegs[13] & 6);
	} else if (n == 14) {
		rtcRegs[14] = d & 15;
		rtcEnabled = !(d & 1);
		if (!rtcEnabled)
			RtcLine(0);
	} else if (n == 15) {
		UINT8 mode = rtcRegs[15] & 4;
		if ((rtcRegs[15] & 1) && !(d & 1))
			mode = d & 4;
		rtcRegs[15] = (d & 11) | mode;
	}
}

static void RtcTick()
{
	if (rtcPulse > 0) {
		rtcPulse -= 6000;
		if (rtcPulse <= 0)
			RtcLine(0);
	}
	if (rtcEnabled) {
		static const UINT32 periods[4] = { 240, 15360, 921600, 55296000 };
		rtcIrqTicks += 6000;
		if (rtcIrqTicks >= UINT64(periods[(rtcRegs[14] >> 2) & 3]) * nBurnFPS) {
			rtcIrqTicks -= UINT64(periods[(rtcRegs[14] >> 2) & 3]) * nBurnFPS;
			RtcLine(1);
			if (!(rtcRegs[14] & 2))
				rtcPulse = 120 * nBurnFPS;
		}
	}
}

struct RomLoad {
	INT32 index, region, offset, stride, source, length;
};

static void OkiBank(INT32 n)
{
	okiBank = n;
	MSM6295SetBank(0, SndROM + ((n * 0x40000) % SndLen), 0, 0x3ffff);
}

static void PaletteWrite(INT32 offset, UINT8 data)
{
	offset &= 0x1ff;
	m_palram[offset] = data;
	INT32 a = m_palram[offset & ~0x10], b = m_palram[offset | 0x10];
	INT32 r = a & 31, g = b & 31, bl = ((a >> 5) & 7) | ((b >> 3) & 24);
	DrvPalette[((offset & 0x1e0) >> 1) | (offset & 15)] = BurnHighCol((r << 3) | (r >> 2), (g << 3) | (g >> 2), (bl << 3) | (bl >> 2), 0);
}
// Hanakanz-family RGB words are retained for palette recalculation and save states.
static void HanaColour(INT32 i)
{
	UINT16 rgb = hanaPalette[i];
	INT32 r = rgb & 31, g = (rgb >> 8) & 31, b = ((rgb >> 5) & 7) | ((rgb >> 10) & 24);
	DrvPalette[i] = BurnHighCol((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2), 0);
}

static void HanaPaletteWrite(UINT16 port, UINT8 d)
{
	UINT16 rgb = (port & 0xff00) | d;
	if (rgb & 0x8000)
		hanaPaletteIndex = rgb & 511;
	else {
		INT32 i = hanaPaletteIndex++ & 511;
		hanaPalette[i] = rgb;
		HanaColour(i);
	}
}

static UINT8 HanaGfxRead()
{
	INT32 a = (m_ddenlovr_blit_address * 2) % GfxLen;
	hanaGfxLatch = GfxROM[(a + 1) % GfxLen];
	m_ddenlovr_blit_address = (m_ddenlovr_blit_address + 1) & 0xffffff;
	return GfxROM[a];
}

static UINT8 GfxRead()
{
	INT32 a = m_ddenlovr_blit_address;
	m_ddenlovr_blit_address = (a + 1) & 0xffffff;
	return GfxROM[a % GfxLen];
}

static UINT8 DswRead(UINT32)
{
	for (INT32 i = 0; i < 3; i++)
		if (!(dswSel & (1 << i)))
			return Dips[i];
	return 0xff;
}

static void DswSelect(UINT32, UINT32 data)
{
	dswSel = data;
}

static UINT16 SysRead()
{
	if (board == BOARD_DDENLOVJ || board == BOARD_NETTOQC)
		return (Inputs[2] & 15) | (m_ddenlovr_blitter_irq_flag ? 0x60 : 0);
	if (board == BOARD_AKAMARU)
		return Inputs[2];
	return (Inputs[2] & 0x3f) | (m_ddenlovr_blitter_irq_flag ? 0x40 : 0);
}

static UINT32 IOBase()
{
	return (board == BOARD_DDENLOVJ || board == BOARD_NETTOQC) ? 0x300000 : board == BOARD_QUIZ365 ? 0x300200 : 0xe00000;
}

static UINT8 __fastcall ReadByte(UINT32 address);
static UINT16 __fastcall ReadWord(UINT32 a)
{
	UINT32 base = IOBase();
	bool jp = board == BOARD_DDENLOVJ || board == BOARD_NETTOQC;
	if (board == BOARD_DDENLOVRK) {
		if (a == 0x100000)
			return prot[0] == 0x7e ? 0xaa : prot[0];
		if (a == 0x200000)
			return prot[0] == 0 ? prot[1] : 0x80;
	}
	if (board == BOARD_ULTRCHMP && a == 0x224680)
		return prot[1] == 5 ? 15 : prot[1] == 10 ? 7 : prot[1];
	if (board == BOARD_AKAMARU) {
		if (a == 0x624680)
			return (prot[0] & 8) ? 1 : 0;
		if (a == 0xe00106)
			return 0x55;
		if (a == 0xe0010c)
			return 0xffff;
		if (a == 0xe00204)
			return m_ddenlovr_blitter_irq_flag << 6;
	}
	if ((a == 0x200c02 && (board == BOARD_NETTOQC || board == BOARD_QUIZ365)) || (a == 0xd00c02 && board == BOARD_ULTRCHMP))
		return (prot[0] & 0xff) == 0x3a ? 0x51 : 0x88;
	if (board == BOARD_QUIZ365 && a >= 0x300204 && a <= 0x300206) {
		if (inputSel == 0x10)
			return Inputs[0];
		if (inputSel == 0x11)
			return Inputs[1];
		if (inputSel == 0x12)
			return SysRead();
		return 0xff;
	}
	UINT32 inp = base + (jp ? 0x180 : 0x100);
	if (board != BOARD_QUIZ365) {
		if (a == inp)
			return Inputs[0];
		if (a == inp + 2)
			return Inputs[1];
		if (a == inp + 4)
			return SysRead();
	}
	if (jp && a == base + 0x186) {
		if (board == BOARD_NETTOQC)
			return DswRead(0);
		UINT16 d = 0;
		for (INT32 i = 0; i < 3; i++)
			if (!(dswSel & (1 << i)))
				d |= Dips[i];
		return d;
	}
	if (!jp && a == base + 0x200) {
		if (board == BOARD_AKAMARU)
			return ((dswSel & 1) ? Dips[0] : 0) | ((dswSel & 2) ? Dips[1] : 0);
		return Dips[0];
	}
	return 0xff00 | ReadByte(a + 1);
}

static UINT8 __fastcall ReadByte(UINT32 a)
{
	UINT32 b = IOBase();
	bool jp = board == BOARD_DDENLOVJ || board == BOARD_NETTOQC;
	bool q = board == BOARD_QUIZ365;
	if (a == b + 0x87)
		return GfxRead();
	if (a == b + 0x71)
		return (board == BOARD_DDENLOVRK || board == BOARD_ULTRCHMP) ? 0 : 0x78;
	UINT32 rtc = b + (jp ? 0x100 : q ? 0x140 : 0x500), oki = b + (jp ? 0x241 : q ? 0xc1 : 0x701), ay = b + (jp ? 0x140 : q ? 0x180 : 0x600);
	if (a == oki)
		return MSM6295Read(0);
	if (a == ay + 5)
		return AY8910Read(0);
	if (a >= rtc && a < rtc + 32)
		return (a & 1) ? rtcRegs[(a - rtc) >> 1] : 0xff;
	// Only recurse for addresses which have word-wide input/protection handlers.
	if ((a >= b + 0x100 && a < b + 0x188 && !q) || (a >= b + 0x200 && a <= b + 0x205 && !jp && !q) ||
	    (q && a >= 0x300204 && a <= 0x300207) || a == 0x100000 || a == 0x100001 || a == 0x200000 || a == 0x200001 ||
	    (a >= 0x224680 && a <= 0x224681) || (a >= 0x624680 && a <= 0x624681) || a == 0x200c02 || a == 0x200c03 || a == 0xd00c02 ||
	    a == 0xd00c03) {
		UINT32 w = a & ~1;
		UINT32 inp = b + (jp ? 0x180 : 0x100);
		if (w == inp || w == inp + 2 || w == inp + 4 || (jp && w == b + 0x186) || (!jp && w == b + 0x200) ||
		    (board == BOARD_AKAMARU && (w == 0xe00106 || w == 0xe0010c || w == 0xe00204 || w == 0x624680)) ||
		    (board == BOARD_DDENLOVRK && (w == 0x100000 || w == 0x200000)) || (board == BOARD_ULTRCHMP && w == 0x224680) || w == 0x200c02 ||
		    w == 0xd00c02 || (q && w >= 0x300204 && w <= 0x300206))
			return ReadWord(w) >> ((~a & 1) * 8);
	}
	return 0xff;
}

static void __fastcall WriteByte(UINT32 a, UINT8 d)
{
	UINT32 w = a & ~1;
	INT32 pi = -1;
	if (board == BOARD_DDENLOVRK && (w == 0x100000 || w == 0x200000))
		pi = w == 0x100000 ? 0 : 1;
	if (board == BOARD_ULTRCHMP && w == 0x224680)
		pi = 1;
	if (board == BOARD_AKAMARU && w == 0x213570)
		pi = 0;
	if ((board == BOARD_NETTOQC || board == BOARD_QUIZ365) && (w == 0x200e0a || w == 0x200e0c))
		pi = (w & 2) ? 0 : 1;
	if (board == BOARD_ULTRCHMP && (w == 0xd00e0a || w == 0xd00e0c))
		pi = (w & 2) ? 0 : 1;
	if (pi >= 0) {
		prot[pi] = (prot[pi] & ((a & 1) ? 0xff00 : 0xff)) | (UINT16(d) << ((~a & 1) * 8));
		if (board == BOARD_DDENLOVRK && w == 0x200000)
			OkiBank(prot[1] & 7);
		if (board == BOARD_AKAMARU && w == 0x213570)
			OkiBank(((prot[0] >> 4) & 15) % 10 * 10 + (prot[0] & 15) % 10);
		return;
	}
	if (board == BOARD_ULTRCHMP && a == 0x313571) {
		OkiBank(d & 15);
		return;
	}
	UINT32 b = IOBase();
	bool jp = board == BOARD_DDENLOVJ || board == BOARD_NETTOQC, q = board == BOARD_QUIZ365;
	UINT32 pal = (jp || q) ? 0x200000 : 0xd00000;
	if (a >= pal && a < pal + 0x400) {
		if (a & 1)
			PaletteWrite((a - pal) >> 1, d);
		return;
	}
	if (!(a & 1))
		return;
	if (a >= b + 0x40 && a < b + 0x60) {
		INT32 o = (a - b - 0x40) >> 1;
		if (o < 4)
			m_ddenlovr_palette_base[o] = d;
		else if (o < 8)
			m_ddenlovr_palette_mask[o - 4] = d;
		else if (o < 12)
			m_ddenlovr_transparency_pen[o - 8] = d;
		else
			m_ddenlovr_transparency_mask[o - 12] = d;
		return;
	}
	if (a == b + 0x69) {
		m_ddenlovr_bgcolor = d;
		return;
	}
	if (a == b + 0x6b) {
		m_ddenlovr_priority = d;
		return;
	}
	if (a == b + 0x6d) {
		if (board == BOARD_AKAMARU)
			m_ddenlovr_bgcolor = d;
		else
			m_ddenlovr_layer_enable = d;
		return;
	}
	if (a == b + 0x81 || a == b + 0x83) {
		blitter_w(0, (a - b - 0x80) >> 1, d);
		return;
	}
	UINT32 ym = b + (jp  ? 0xc0
	                 : q ? 0x100
	                     : 0x400),
	       rtc = b + (jp  ? 0x100
		              : q ? 0x140
		                  : 0x500),
	       ay = b + (jp  ? 0x140
		             : q ? 0x180
		                 : 0x600),
	       latch = b + (jp  ? 0x1c0
		                : q ? 0x1c0
		                    : 0x300),
	       oki = b + (jp  ? 0x241
		              : q ? 0xc1
		                  : 0x701);
	if (a >= ym && a < ym + 4) {
		BurnYM2413Write((a - ym) >> 1, d);
		return;
	}
	if (a >= ay && a < ay + 4) {
		AY8910Write(0, (a - ay) >> 1, d);
		return;
	}
	if (a == oki) {
		MSM6295Write(0, d);
		return;
	}
	if (a >= rtc && a < rtc + 32) {
		RtcWrite((a - rtc) >> 1, d);
		return;
	}
	if (a >= latch && a < latch + 16) {
		INT32 n = (a - latch) >> 1, bit = d & 1;
		if (n == ((jp || q) ? 5 : 1)) {
			m_ddenlovr_blitter_irq_enable = bit;
			if (!bit)
				m_ddenlovr_blitter_irq_flag = 0;
		}
		if (q && n == 1)
			OkiBank((okiBank & 2) | bit);
		if (q && n == 6)
			OkiBank((okiBank & 1) | (bit << 1));
		if (board == BOARD_AKAMARU && n == 2)
			dswSel = (dswSel & 1) | (bit << 1);
		if (board == BOARD_AKAMARU && n == 3)
			dswSel = (dswSel & 2) | bit;
		return;
	}
	if (jp && a == b + 0x18b) {
		dswSel = d;
		return;
	}
	if (jp && a == b + 0x18d) {
		OkiBank(d & (board == BOARD_NETTOQC ? 3 : 7));
		return;
	}
	if (q && a == 0x300201) {
		inputSel = d;
		return;
	}
	if (board == BOARD_DDENLOVR && a == 0x300001) {
		OkiBank(d & 7);
		return;
	}
}

static void __fastcall WriteWord(UINT32 a, UINT16 d)
{
	if (board == BOARD_DDENLOVRK) {
		if (a == 0x100000) {
			prot[0] = d;
			return;
		}
		if (a == 0x200000) {
			prot[1] = d;
			OkiBank(d & 7);
			return;
		}
	}
	if (board == BOARD_ULTRCHMP) {
		if (a == 0x224680) {
			prot[1] = d;
			return;
		}
		if (a == 0x313570) {
			OkiBank(d & 15);
			return;
		}
	}
	if (board == BOARD_AKAMARU && a == 0x213570) {
		prot[0] = d;
		OkiBank(((d >> 4) & 15) % 10 * 10 + (d & 15) % 10);
		return;
	}
	if ((a == 0x200e0a || a == 0x200e0c) && (board == BOARD_NETTOQC || board == BOARD_QUIZ365)) {
		prot[(a & 2) ? 0 : 1] = d;
		return;
	}
	if ((a == 0xd00e0a || a == 0xd00e0c) && board == BOARD_ULTRCHMP) {
		prot[(a & 2) ? 0 : 1] = d;
		return;
	}
	WriteByte(a + 1, d);
}

static UINT8 MatrixRead(INT32 offset)
{
	UINT8 val = 0x3f;
	for (INT32 row = 0; row < 5; row++)
		if (!(keyRow & (1 << row)))
			val &= Matrix[offset ? 0 : 1][row];
	return val | (Dips[offset ? 7 : 6] & 0x40);
}

static void MixWrite(INT32 group, INT32 off, UINT8 d)
{
	INT32 l = group * 4 + (off & 3);
	if (off < 4)
		m_ddenlovr_palette_base[l] = d;
	else if (off < 8)
		m_ddenlovr_palette_mask[l] = d;
	else if (off < 12)
		m_ddenlovr_transparency_pen[l] = d;
	else
		m_ddenlovr_transparency_mask[l] = d;
}

static UINT8 __fastcall SoundIn(UINT16 p)
{
	return (p & 255) == (board == BOARD_FUNKYFIG ? 2 : 0) ? soundLatch : 0xff;
}

static void __fastcall SoundOut(UINT16 p, UINT8 d)
{
	p &= 255;
	if (p == 8 || p == 9)
		BurnYM2413Write(p & 1, d);
	if (p == 0x0c || p == 0x0e)
		AY8910Write(0, p == 0x0c ? 1 : 0, d);
}

static void ZRamBank(INT32, UINT8 d)
{
	if (KL_BOARD) {
		ramBank = d & 7;
		KlMap();
		return;
	}
	if (board == BOARD_HTENGOKU) {
		ZetMapMemory(DrvRAM + 0x1000, 0x7000, 0x7fff, MAP_RAM);
		return;
	}
	if (HANA_BOARD) {
		ramBank = d & 7;
		ZetMapMemory(DrvRAM + ramBank * 0x1000, 0x7000, 0x7fff, MAP_RAM);
		return;
	}
	ramBank = d & ((board == BOARD_MJSCHUKA || SRY_BOARD || SEL_BOARD) ? 15 : 7);
	ZetMapMemory(DrvRAM + 0x1000 + ramBank * 0x1000, 0x7000, 0x7fff, MAP_RAM);
}

static void ZBank(INT32, UINT8 d)
{
	romBank = d;
	if (KL_BOARD) {
		ramBank = (d >> 4) & 7;
		KlMap();
		return;
	}
	if (board == BOARD_HTENGOKU) {
		ZetMapMemory((d & 16) ? NULL : DrvROM + (d & 7) * 0x8000, 0x8000, 0xffff, MAP_ROM);
		return;
	}
	if (GOK_BOARD) {
		ZetMapMemory(DrvROM + 0x10000 + (d & (board == BOARD_RSELECT ? 15 : 7)) * 0x8000, 0x8000, 0xffff, MAP_ROM);
		ZetMapMemory(NULL, board == BOARD_RSELECT ? 0xf600 : 0xe600, board == BOARD_RSELECT ? 0xf6ff : 0xe6ff, MAP_READ);
		return;
	}
	if (HANA_BOARD) {
		ZetMapMemory(DrvROM + (d & 15) * 0x8000, 0x8000, 0xffff, MAP_ROM);
		ZRamBank(0, d >> 4);
		return;
	}
	if (SEL_BOARD) {
		ZetMapMemory((d & 16) ? DrvRAM + 0x18000 : DrvROM + 0x10000 + (d & 15) * 0x8000, 0x8000, 0xffff, MAP_ROM);
		return;
	}
	INT32 n = d & ((board == BOARD_FUNKYFIG || board == BOARD_MJSCHUKA || board == BOARD_MJFLOVE || SRY_BOARD) ? 15
	               : board == BOARD_MJMYORNTR                                                                  ? 7
	               : (board == BOARD_HGINGA || board == BOARD_HPARADIS)                                        ? 7
	               : (MAHJONG_BOARD || board == BOARD_MMPANIC)                                                 ? 7
	                                                                                                           : 31);
	ZetMapMemory(DrvROM + 0x10000 + n * 0x8000, 0x8000, 0xffff, MAP_ROM);
	if ((board == BOARD_MJMYWRLD || board == BOARD_HGINGA))
		ZetMapMemory(NULL, 0xf600, 0xf6ff, MAP_READ);
	if (board == BOARD_HPARADIS)
		keyRow = 0;
	if (board < 9 || board == BOARD_FUNKYFIG || board == BOARD_HPARADIS) {
		dswSel = d;
		ZRamBank(0, d >> 5);
	}
}

static void KeySelect(INT32, UINT8 d)
{
	keyRow = d;
}

static void HanaOkiBank(INT32, UINT8 d)
{
	OkiBank(d & 1);
}

static void CoinSelect(INT32, UINT8 d)
{
	lockout = d;
}

static UINT8 Rev2Dsw(UINT32)
{
	UINT8 d = 255;
	for (INT32 i = 0; i < 5; i++)
		if (!(dswSel & (1 << i)))
			d &= Dips[i];
	return d;
}

static UINT8 SelDsw(UINT32)
{
	for (INT32 i = 0; i < 5; i++)
		if (!(dswSel & (1 << i)))
			return Dips[i];
	return 255;
}

static UINT8 ZDsw(INT32)
{
	if (HANA_BOARD)
		return 0;
	if (board == BOARD_MJFLOVE)
		return Dips[1];
	if (SEL_BOARD)
		return 0;
	if (board == BOARD_HGINGA || LATE_MAHJONG || GOK_BOARD)
		return rtcLine ? 0 : 0x80;
	if (MAHJONG_BOARD || SRY_BOARD)
		return 0;
	if (board == BOARD_FUNKYFIG)
		return DswRead(0);
	for (INT32 i = 0; i < 5; i++)
		if (!(dswSel & (1 << i)))
			return i < 2 ? Dips[i] : i == 4 ? Dips[2] : 0xff;
	return 0xff;
}

static void ZIrq(INT32)
{
	INT32 state = z80ctc_irq_state();
	if (!(state & Z80_DAISY_IEO))
		state |= z80pio_irq_state();
	if (board == BOARD_MJFLOVE && !(state & Z80_DAISY_IEO))
		state |= ExternalIrqState();
	ZetSetIRQLine(0, (state & Z80_DAISY_INT) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void __fastcall ZWrite(UINT16 a, UINT8 d)
{
	if (GOK_BOARD) {
		UINT16 base = board == BOARD_RSELECT ? 0xf000 : 0xe000;
		if (a >= base && a < base + 512)
			PaletteWrite(a - base, d);
		return;
	}
	if (board == BOARD_HTENGOKU) {
		if ((romBank & 16) && a >= 0x8000 && a <= 0x81ff)
			PaletteWrite(a & 511, d);
		return;
	}
	if (HANA_BOARD)
		return;
	if (SEL_BOARD) {
		if ((romBank & 16) && a >= 0xb000 && a <= 0xb1ff) {
			PaletteWrite(a - 0xb000, d);
			DrvRAM[0x18000 + a - 0x8000] = d;
		}
		return;
	}
	UINT16 base = board == BOARD_HPARADIS                                            ? 0xc000
	              : (board == BOARD_QUIZCHQ || board == BOARD_MJMYORNT || SOUND_CPU) ? 0x8000
	              : (board == BOARD_MJMYWRLDT || SRY_BOARD)                          ? 0xe000
	                                                                                 : 0xf000;
	if (a >= base && a < base + 512)
		PaletteWrite(a - base, d);
}

static UINT8 __fastcall ZRead(UINT16 a)
{
	if (GOK_BOARD) {
		if ((board == BOARD_RSELECT && a == 0xf601 && (romBank & 16)) || (board != BOARD_RSELECT && a == 0xe601 && romBank == 0))
			return BurnRandom();
		return DrvROM[0x10000 + (romBank & (board == BOARD_RSELECT ? 15 : 7)) * 0x8000 + a - 0x8000];
	}
	if (KL_BOARD)
		return 255;
	if (board == BOARD_HTENGOKU)
		return 255;
	if (board == BOARD_MMPANIC && a < 0x100)
		return a == 0x51 ? 1 : DrvROM[a];
	if ((board == BOARD_MJMYWRLD || board == BOARD_HGINGA) && a >= 0xf600) {
		if (a == 0xf601 && (romBank & 0x10))
			return BurnRandom();
		return DrvROM[0x10000 + (romBank & 7) * 0x8000 + a - 0x8000];
	}
	if (board == BOARD_RONGRONG && a == 0x60d4)
		return 0;
	return DrvRAM[a & 0xfff];
}
// KC82 maps logical 1 KiB pages into a 20-bit physical bus. Map at
// the Z80 core's 256-byte granularity so its internal top 512-byte RAM is exact.
static void KlMap()
{
	for (INT32 a = 0; a < 0x10000; a += 256) {
		UINT32 phys = (a + klBase[a >> 10]) & 0xfffff;
		UINT8 *ptr = NULL;
		INT32 flags = MAP_ROM;
		if (phys < 0x6000)
			ptr = DrvROM + phys;
		else if (phys < 0x7000) {
			ptr = DrvRAM + phys - 0x6000;
			flags = MAP_RAM;
		} else if (phys < 0x8000) {
			ptr = DrvRAM + ramBank * 0x1000 + phys - 0x7000;
			flags = MAP_RAM;
		} else if (phys < 0x10000) {
			UINT32 offset = (romBank & 15) * 0x8000 + phys - 0x8000;
			if (offset < UINT32(RomLen))
				ptr = DrvROM + offset;
		} else if (phys >= 0xffe00) {
			ptr = DrvRAM + 0x18000 + phys - 0xffe00;
			flags = MAP_RAM;
		}
		ZetMapMemory(NULL, a, a + 255, MAP_RAM);
		if (ptr)
			ZetMapMemory(ptr, a, a + 255, flags);
	}
}

static void KlPortOut(INT32 n, UINT8 d)
{
	switch (n) {
		case 0:
			OkiBank(KL_LATE ? (d >> 4) & (board >= BOARD_DAIMYOJN ? 1 : 7) : (d >> 6) & 1);
			break;
		case 1:
			if (KL_MUX)
				hanaPaletteSelect = d;
			else
				hanakanz_blitter_reg_w(d);
			break;
		case 2:
			if (KL_MUX)
				hanakanz_blitter_reg_w(d);
			else
				ZBank(0, d);
			break;
		case 3:
			dswSel = d;
			if (KL_LATE)
				keyRow = d;
			break;
	}
}

static UINT8 KlPortIn(INT32 n)
{
	return n == 0 ? (KL_LATE ? 4 : 0x80) : n == 4 ? SelDsw(0) : 0;
}

static void KlPalette(UINT8 d)
{
	UINT16 rgb = (m_ddenlovr_blit_latch << 8) | d;
	if (rgb & 0x8000)
		hanaPaletteIndex = rgb & 511;
	else {
		INT32 index = hanaPaletteIndex++ & 511;
		hanaPalette[index] = rgb;
		HanaColour(index);
	}
}

static void KlSeparateRom(UINT8 d)
{
	romBank = d & 15;
	KlMap();
}

static UINT8 KlLateRead(UINT8 p)
{
	if (!KL_MUX) {
		if (p == 0x40)
			return Inputs[2];
		if (p == 0x41 || p == 0x42)
			return MatrixRead(p - 0x41);
		if (p == 0x46)
			return BurnRandom();
		if (p == 0x47 && board == BOARD_MJGNIGHT)
			return prot[0] == 0xc0 ? 1 : prot[0];
		if (p == 0x63)
			return HanaGfxRead();
		if (p == 0x64)
			return hanaGfxLatch;
		if (p == 0xa0)
			return MSM6295Read(0);
		if (p >= 0xc0 && p <= 0xcf)
			return rtcRegs[p & 15];
		return 255;
	}
	UINT8 fm = board == BOARD_JONGTEIA ? 0x70 : board == BOARD_HNROSE ? 0x60 : 0xa0;
	UINT8 sys = fm + 8, key = fm + 10, random = board == BOARD_JONGTEIA || board == BOARD_HNROSE ? 0xa0 : 0xd0,
	      protect = board == BOARD_JONGTEIA || board == BOARD_HNROSE ? 0xb0 : 0xe0;
	if (p == 0x39 && board == BOARD_MOMOTARO)
		return 4; // MAME workaround for unimplemented USART.
	if (p == 0x42)
		return HanaGfxRead();
	if (p == 0x43 || (p == 0x44 && board != BOARD_JONGTEIA))
		return hanaGfxLatch;
	if (p >= 0x80 && p <= 0x8f) {
		if (board != BOARD_JONGTEIA && (p == 0x8a || p == 0x8b))
			return p & 1;
		return rtcRegs[p & 15];
	}
	if (p == fm + 2)
		return MSM6295Read(0);
	if (p == sys)
		return Inputs[2];
	if (p >= key && p <= key + 2) {
		INT32 off = p - key;
		UINT8 v = MatrixRead(off ? 1 : 0);
		if (board == BOARD_JONGTEIA || board == BOARD_HNROSE)
			return v;
		return off ? v : v | ((lockout & 4) && !(hopperTicks % 10) ? 0 : 64);
	}
	if (p == random)
		return BurnRandom();
	if (p == protect)
		return prot[0] ^ (board == BOARD_JONGTEIA    ? 0x18
		                  : board == BOARD_HNROSE    ? 0x99
		                  : board == BOARD_MJSWACADS ? 0x0a
		                  : board == BOARD_MOMOTARO  ? 0x0d
		                  : board == BOARD_MJNIGIRI  ? 0xf2
		                                             : 0x42);
	return 255;
}

static void KlLateWrite(UINT8 p, UINT8 d)
{
	if (!KL_MUX) {
		if (p == 0x43)
			lockout = d;
		if (board == BOARD_MJGNIGHT && (p == 0x46 || p == 0x47)) {
			prot[0] = d;
			lockout = d;
		}
		if (p == 0x60)
			hanakanz_blitter_data_w(d);
		if (p == 0x61)
			KlPalette(d);
		if (p == 0x80 || p == 0x81)
			BurnYM2413Write(p & 1, d);
		if (p == 0xa0)
			MSM6295Write(0, d);
		if (p >= 0xc0 && p <= 0xcf)
			RtcWrite(p & 15, d);
		return;
	}
	if (p == 0x40) {
		if (hanaPaletteSelect & 1)
			KlPalette(d);
		else
			hanakanz_blitter_data_w(d);
	}
	UINT8 fm = board == BOARD_JONGTEIA ? 0x70 : board == BOARD_HNROSE ? 0x60 : 0xa0;
	if (p == fm || p == fm + 1)
		BurnYM2413Write(p & 1, d);
	if (p == fm + 2)
		MSM6295Write(0, d);
	if (p == fm + 14)
		lockout = d;
	if (p >= 0x80 && p <= 0x8f)
		RtcWrite(p & 15, d);
	UINT8 rom = board == BOARD_JONGTEIA   ? 0x50
	            : board == BOARD_MJNIGIRI ? 0xb0
	                                      : 0xc0,
	      ram = board == BOARD_JONGTEIA   ? 0x60
		        : board == BOARD_HNROSE   ? 0x70
		        : board == BOARD_MJNIGIRI ? 0xc0
		                                  : 0xb0;
	if (p == rom)
		KlSeparateRom(d);
	if (p == ram)
		ZRamBank(0, d);
	if (p == (board == BOARD_JONGTEIA || board == BOARD_HNROSE ? 0xb0 : 0xe0))
		prot[0] = d;
}

static UINT8 KlBoardRead(UINT8 p)
{
	if (p < 8 || (p >= 0x20 && p <= 0x25) || (p >= 0x28 && p <= 0x37))
		return KlRead(p);
	if (KL_LATE)
		return KlLateRead(p);
	if (p == 0x83)
		return HanaGfxRead();
	if (p == 0x84)
		return hanaGfxLatch;
	if (p == 0xc0)
		return MSM6295Read(0);
	if (p >= 0xe0 && p <= 0xef && board != BOARD_KOTBINYO && board != BOARD_KOTBINSP)
		return rtcRegs[p & 15];
	if (board == BOARD_MJREACH1) {
		if (p == 0x92)
			return BurnRandom();
		if (p == 0x93)
			return prot[0];
		if (p == 0x94)
			return Inputs[2];
		if (p == 0x95 || p == 0x96)
			return MatrixRead(p - 0x95);
		return 255;
	}
	UINT8 base = (board == BOARD_HKAGEROU || board == BOARD_KOTBINYO) ? 0xb0 : 0x90;
	if (p == base)
		return Inputs[2];
	if (p == base + 6)
		return BurnRandom();
	if (p == base + 1 || p == base + 2) {
		if (board == BOARD_KOTBINYO || board == BOARD_KOTBINSP)
			return Inputs[p - base - 1];
		return MatrixRead(p - base - 1);
	}
	return 255;
}

static void KlBoardWrite(UINT8 p, UINT8 d)
{
	if (p < 8 || (p >= 0x20 && p <= 0x25) || (p >= 0x28 && p <= 0x37)) {
		KlWrite(p, d);
		return;
	}
	if (KL_LATE) {
		KlLateWrite(p, d);
		return;
	}
	if (p == 0x80)
		hanakanz_blitter_data_w(d);
	if (p == 0x81)
		KlPalette(d);
	if (p == 0xa0 || p == 0xa1)
		BurnYM2413Write(p & 1, d);
	if (p == 0xc0)
		MSM6295Write(0, d);
	if (p >= 0xe0 && p <= 0xef && board != BOARD_KOTBINYO && board != BOARD_KOTBINSP)
		RtcWrite(p & 15, d);
	if (board == BOARD_MJREACH1) {
		if (p == 0x90)
			keyRow = d;
		if (p == 0x93)
			prot[0] = d;
		if (p == 0x97)
			lockout = d;
		return;
	}
	UINT8 base = (board == BOARD_HKAGEROU || board == BOARD_KOTBINYO) ? 0xb0 : 0x90;
	if (p == base + 3)
		lockout = d;
	if (p == base + 4)
		keyRow = d;
}

static void hginga_blitter_w(UINT8 p, UINT8 d)
{
	if (p == 0)
		m_ddenlovr_blit_latch = d;
	else {
		if ((m_ddenlovr_blit_latch & 63) == 0 && ((d & 15) == 3 || (d & 15) == 6 || (d & 15) == 10))
			d &= ~2;
		if ((m_ddenlovr_blit_latch & 63) == 0x24 && d == 0x1b)
			d = 0x13;
	}
	blitter_w(0, p, d);
}

static UINT8 GokPlayer(INT32 player)
{
	// The panel drives six key bits; bit 7 is blitter busy and bit 6 is the hopper.
	// Blits complete synchronously, so busy must remain clear.
	UINT8 v = 0x3f;
	for (INT32 r = 0; r < 5; r++)
		if (!(inputSel & (1 << r)))
			v &= Matrix[player][r];
	return v | ((lockout & 4) && !(hopperTicks % 10) ? 0 : 64);
}

static UINT8 GokInput()
{
	switch (dswSel) {
		case 0x20:
			return Inputs[2];
		case 0x21:
			return GokPlayer(1);
		case 0x22:
			return GokPlayer(0);
		case 0x23:
			return lockout;
	}
	return 255;
}

static UINT8 GokRead(UINT8 p)
{
	UINT8 snd = board == BOARD_HGOKBANG ? 0x20 : 0x80, rtc = board == BOARD_HGOKBANG ? 0x60 : 0x20,
	      inp = board == BOARD_HGOKBANG ? 0x40 : 0x60;
	if (p < 2)
		return 255;
	if (p == 3)
		return GfxRead();
	if (p >= rtc && p < rtc + 16)
		return rtcRegs[p & 15];
	if (p == snd)
		return MSM6295Read(0);
	if (p == snd + 4)
		return AY8910Read(0);
	if (p == (board == BOARD_HGOKBANG ? 0xb8 : 0x58))
		return 0x78;
	if (p == (board == BOARD_HGOKBANG ? 0xe0 : 0xb0))
		return BurnRandom();
	if (p == inp + 2)
		return GokInput();
	if (board == BOARD_HGOKBANG && p == 0x43) {
		if (dswSel == 0x2d) {
			if (inputSel == 255)
				inputSel = 254;
			return 0;
		}
		if (dswSel == 0xa1 || dswSel == 0xa2) {
			UINT8 v = GokPlayer(dswSel == 0xa1 ? 1 : 0);
			inputSel = (inputSel << 1) | 1;
			return v;
		}
	}
	return 255;
}

static void GokWrite(UINT8 p, UINT8 d)
{
	UINT8 snd = board == BOARD_HGOKBANG ? 0x20 : 0x80, rtc = board == BOARD_HGOKBANG ? 0x60 : 0x20,
	      inp = board == BOARD_HGOKBANG ? 0x40 : 0x60, mix = board == BOARD_HGOKBANG ? 0xa0 : 0x40;
	if (p < 2)
		hginga_blitter_w(p, d);
	if (p >= rtc && p < rtc + 16)
		RtcWrite(p & 15, d);
	if (p == snd)
		MSM6295Write(0, d);
	if (p == snd + 2 || p == snd + 3)
		BurnYM2413Write(p & 1, d);
	if (p == snd + 6 || p == snd + 8)
		AY8910Write(0, p == snd + 6, d);
	if (p >= mix && p < mix + 16)
		MixWrite(0, p - mix, d);
	if (p == mix + 20)
		m_ddenlovr_bgcolor = d;
	if (p == mix + 21)
		m_ddenlovr_priority = d;
	if (p == mix + 22)
		m_ddenlovr_layer_enable = d;
	if (p == inp)
		dswSel = d;
	if (p == inp + 1) {
		if (dswSel == 0x2c)
			lockout = d;
		if (dswSel == 0x2d)
			inputSel = d;
	}
}

static UINT8 __fastcall ZIn(UINT16 port)
{
	UINT8 p = port;
	if (KL_BOARD)
		return KlBoardRead(p);
	if (GOK_BOARD && !(p >= 0x10 && p <= 0x1f))
		return GokRead(p);
	if (board == BOARD_HTENGOKU) {
		if (p == 0x42)
			return AY8910Read(0);
		if (p == 0xb8)
			return 0x78;
		if (p >= 0x80 && p <= 0x8f)
			return rtcRegs[p & 15];
		if (p == 0x22) {
			if (inputSel == 0)
				return Inputs[2];
			if (inputSel == 2)
				return 0xbf | ((lockout & 4) && ((hopperTicks / 3) & 1) ? 0x40 : 0);
			if (inputSel == 3)
				return lockout;
		}
		if (p == 0x23 && (inputSel == 0x81 || inputSel == 0x82))
			return keyRow < 5 ? Matrix[inputSel & 1][keyRow++] : 255;
		return 255;
	}
	if (HANA_BOARD && !(p >= 0x10 && p <= 0x1f)) {
		if (p == 0x23)
			return HanaGfxRead();
		if (p == 0x45)
			return hanaGfxLatch;
		if (p == 0x42)
			return Inputs[2];
		if (p == 0x43 || p == 0x44)
			return MatrixRead(p - 0x43) | (p == 0x44 ? 0x80 : 0);
		if (p >= 0x60 && p <= 0x64)
			return Dips[p - 0x60];
		if (p == 0x80)
			return MSM6295Read(0);
		if (p >= 0xc0 && p <= 0xcf)
			return rtcRegs[p & 15];
		return 255;
	}
	// This board decodes the entire I/O address, except explicit mirrored ports.
	if (board == BOARD_MJFLOVE && !(p >= 0x10 && p <= 0x1f)) {
		if (p == 0xda)
			return 0x27;
		if (port == 0x43)
			return GfxRead();
		if (port == 0x80 || port == 0x81) {
			UINT8 val = 255;
			for (INT32 row = 0; row < 5; row++)
				if (!(keyRow & (1 << row)))
					val &= Matrix[port & 1][row];
			return val;
		}
		if (port == 0x82)
			return (Inputs[2] & 0x1f) | (mjfloveIrqCause << 5);
		if (port == 0x100)
			return Dips[0];
		if (port >= 0x280 && port <= 0x28f)
			return rtcRegs[port & 15];
		if (port == 0x380)
			return MSM6295Read(0);
		return 255;
	}
	if (board == BOARD_MMPANIC) {
		if (p < 16)
			return rtcRegs[p];
		if (p == 0x38 || p == 0x58)
			return 0x78;
		if (p == 0x63)
			return GfxRead();
		if (p == 0x6a)
			return Inputs[0];
		if (p == 0x6b)
			return Inputs[1];
		if (p == 0x7c)
			return MSM6295Read(0);
		if (p == 0x94 || p == 0x98 || p == 0x9c)
			return Dips[(p - 0x94) / 4];
		return 0xff;
	}
	if (board == BOARD_DAIREACH) {
		if (p < 16)
			return rtcRegs[p];
		if (p == 0x43)
			return GfxRead();
		if (p == 0x60)
			return AY8910Read(0);
		if (p == 0x66)
			return MSM6295Read(0);
		if (p == 0x6c)
			return BurnRandom();
		if (p == 0x6e)
			return prot[0] ^ 0x17;
		if (p == 0x70)
			return Inputs[2];
		if (p == 0x72)
			return MatrixRead(0) | ((lockout & 8) && !(hopperTicks % 10) ? 0 : 64);
		if (p == 0x74)
			return MatrixRead(1) | 0x80;
		if (p >= 0x10 && p <= 0x13)
			return z80ctc_read(p & 3);
		if (p >= 0x1c && p <= 0x1f)
			return z80pio_read_alt(p & 3);
		if (p >= 0x18 && p <= 0x1b)
			return SioRead(p);
		return 255;
	}
	if (SEL_BOARD) {
		if (p < 16)
			return rtcRegs[p];
		if (p == 0x43)
			return GfxRead();
		if (p == 0x54)
			return MSM6295Read(0);
		if (p == 0x5c)
			return AY8910Read(0);
		if (p == 0x80)
			return Inputs[2];
		if (p == 0x84)
			return (MatrixRead(0) & 63) | ((lockout & 4) && !(hopperTicks % 10) ? 0 : 64);
		if (p == 0x88)
			return MatrixRead(1);
		if (p == 0xa0)
			return BurnRandom();
		if (p == 0xb0 && board != BOARD_SELJAN2)
			return prot[0] ^ (board == BOARD_JONGOH ? 6 : board == BOARD_DAICHUKA ? 7 : 5);
	}
	if (SRY_BOARD) {
		UINT8 in = board == BOARD_SRYUDENS ? 0x98 : 8, rtc = board == BOARD_SRYUDENS ? 0x80 : 0x50,
		      dsw = board == BOARD_SRYUDENS ? 0x90 : 0, snd = board == BOARD_SRYUDENS ? 0 : 0x90;
		if (p >= dsw && p < dsw + 5) {
			static const UINT8 order[5] = { 0, 1, 3, 2, 4 };
			return Dips[order[p - dsw]];
		}
		if (p == in + 2)
			return Inputs[2];
		if (p == in + 3 || p == in + 4)
			return MatrixRead(p - in - 3);
		if (p >= rtc && p < rtc + 16)
			return rtcRegs[p - rtc];
		if (p == snd)
			return MSM6295Read(0);
		if (p == 0x43)
			return GfxRead();
		if (p == (board == BOARD_SRYUDENS ? 0x50 : 0x60))
			return BurnRandom();
		if (board != BOARD_SRYUDENS && p == 0x80)
			return DrvRAM[0x1f000];
	}
	if (p >= 0x10 && p <= 0x13)
		return z80ctc_read(p & 3);
	if (p >= 0x1c && p <= 0x1f)
		return z80pio_read_alt(p & 3);
	if (p >= 0x18 && p <= 0x1b)
		return SioRead(p);
	if (board == BOARD_FUNKYFIG) {
		if (p == 0)
			return MSM6295Read(0);
		if (p == 4)
			return 0;
		if (p == 0x23)
			return GfxRead();
		if (p >= 0x40 && p < 0x50)
			return rtcRegs[p - 0x40];
		if (p == 0x78)
			return 0x78;
		if (p == 0x82)
			return inputSel == 0x22 ? Inputs[2] : inputSel == 0x23 ? lockout : 0xff;
		if (p == 0x83)
			return inputSel == 0x20 ? Inputs[0] : inputSel == 0x21 ? Inputs[1] : 0xff;
		return 0xff;
	}
	if (LATE_MAHJONG) {
		if (p < 16)
			return rtcRegs[p];
		if (p == 0x43)
			return GfxRead();
		if (p == 0x74)
			return MSM6295Read(0);
		INT32 inp = board == BOARD_MJSCHUKA ? 0x62 : 0x52, dip = board == BOARD_MJSCHUKA ? 0x68 : 0x58;
		if (p == inp)
			return Inputs[2];
		if (p == inp + 1 || p == inp + 2)
			return MatrixRead(p - inp - 1);
		if (p >= dip && p < dip + 5)
			return Dips[p - dip];
		if (board == BOARD_MJSCHUKA && p == 0x54)
			return prot[0] == 0xa8 ? 0x13 : prot[0];
		if (p == (board == BOARD_MJSCHUKA ? 0x5c : 0x6c))
			return BurnRandom();
		return 0xff;
	}
	if (board == BOARD_HGINGA) {
		if (p == 3)
			return GfxRead();
		if (p == 0x24)
			return AY8910Read(0);
		if (p >= 0x60 && p < 0x70)
			return rtcRegs[p - 0x60];
		if (p == 0xb8)
			return 0x78;
		if (p == 0x42) {
			if (inputSel == 0x20)
				return Inputs[2];
			if (inputSel == 0x21)
				return 0xff;
			if (inputSel == 0x22)
				return 0x3f | ((lockout & 4) && ((hopperTicks / 3) & 1) ? 0x40 : 0);
			if (inputSel == 0x23)
				return lockout;
		}
		if (p == 0x43 && (inputSel == 0xa1 || inputSel == 0xa2))
			return keyRow < 5 ? Matrix[(~inputSel >> 1) & 1][keyRow++] : 0xff;
		return 0xff;
	}
	if (board == BOARD_HPARADIS && (p == 0xa2 || p == 0xa3)) {
		if (inputSel == 0x0d)
			return 0;
		if (inputSel == 0x80 || inputSel == 0x81)
			return keyRow < 5 ? Matrix[inputSel & 1][keyRow++] : 0xff;
		return inputSel < 3 ? Inputs[inputSel] : 0xff;
	}
	if (MAHJONG_BOARD) {
		if (p == 0x22) {
			if (board == BOARD_MJMYWRLD)
				return inputSel == 0x80 ? Inputs[2] : (inputSel == 0x81 || inputSel == 0x83) ? 0 : 0xff;
			return inputSel == 0 ? Inputs[2] : 0xff;
		}
		if (p == 0x23)
			return keyRow < 5 ? Matrix[inputSel & 1][keyRow++] : 0xff;
		if (p == 0x44)
			return AY8910Read(0);
		if (p >= 0x60 && p < 0x70)
			return rtcRegs[p - 0x60];
		if (p == (board == BOARD_MJMYWRLD ? 0xe0 : 0xc3))
			return MahjongDsw(0);
		if (p == (board == BOARD_MJMYWRLD ? 0xc0 : 0xc2))
			return BurnRandom();
	}
	if (p == 3)
		return GfxRead();
	if (p == 0x40)
		return MSM6295Read(0);
	if (p == 0x98)
		return 0x78;
	if (MAHJONG_BOARD)
		return 0xff;
	UINT8 rtc = board == BOARD_QUIZCHQ ? 0xa0 : 0x20, inp = board == BOARD_QUIZCHQ ? 0x22 : 0xa2;
	if (p >= rtc && p < rtc + 16)
		return rtcRegs[p - rtc];
	if (p == inp || p == inp + 1)
		return inputSel < 3 ? Inputs[inputSel] : 0xff;
	if (p == 0xf0)
		return 0xfb;
	return 0xff;
}

static void __fastcall ZOut(UINT16 port, UINT8 d)
{
	UINT8 p = port;
	if (trace && traceWrites++ < 256)
		fprintf(trace, "OUT %04x=%02x PC=%04x\n", port, d, ZetGetPC(-1));
	if (KL_BOARD) {
		KlBoardWrite(p, d);
		return;
	}
	if (GOK_BOARD && !(p >= 0x10 && p <= 0x1f)) {
		GokWrite(p, d);
		return;
	}
	if (board == BOARD_HTENGOKU) {
		if (p >= 1 && p <= 7)
			Rev2Reg(p, d);
		if (p == 0x20) {
			inputSel = d;
			keyRow = 0;
		}
		if (p == 0x21 && inputSel == 0x0c)
			lockout = d;
		if (p == 0x40 || p == 0x44)
			AY8910Write(0, p == 0x44, d);
		if (p == 0x46 || p == 0x47)
			BurnYM2413Write(p & 1, d);
		if (p >= 0x80 && p <= 0x8f)
			RtcWrite(p & 15, d);
		if (p >= 0xa0 && p <= 0xaf)
			MixWrite(0, p & 15, d);
		if (p == 0xb4)
			m_ddenlovr_bgcolor = d;
		if (p == 0xb5)
			m_ddenlovr_priority = d;
		if (p == 0xb6)
			m_ddenlovr_layer_enable = d;
		if (p == 0xc0)
			m_ddenlovr_scroll[2] = d * 2;
		if (p == 0xc1)
			m_ddenlovr_scroll[6] = d;
		if (p == 0xc2)
			ZBank(0, d);
		if (p == 0xc3) {
			rev2.irq &= ~0x10;
			Rev2Irq();
		}
		if (p == 0xc4)
			rev2.pen = d;
		if (p == 0xc5)
			rev2.dest = d;
		if (p == 0xc6) {
			if (d == 0x80)
				rev2.bank = 0;
			if (d == 0x81)
				rev2.bank = 1;
			if (d == 0)
				rev2.bank = 2;
		}
		if (p >= 0xe0 && p <= 0xe7) {
			UINT8 bit = (d >> 1) & 1;
			if (p == 0xe0)
				rev2.flip = bit;
			if (p == 0xe1)
				rev2.half = !bit;
			if (p == 0xe2)
				rev2.half2 = !bit;
			if (p == 0xe5) {
				rev2.irqMask = bit;
				if (!bit) {
					rev2.irq &= ~0x20;
					Rev2Irq();
				}
			}
		}
		return;
	}
	if (board == BOARD_MMPANIC) {
		if (p < 16) {
			RtcWrite(p, d);
			return;
		}
		if (p >= 0x20 && p < 0x30) {
			MixWrite(0, p - 0x20, d);
			return;
		}
		if (p >= 0x40 && p < 0x50) {
			MixWrite(1, p - 0x40, d);
			return;
		}
		if (p == 0x34)
			m_ddenlovr_bgcolor = d;
		if (p == 0x35)
			m_ddenlovr_priority = d;
		if (p == 0x36)
			m_ddenlovr_layer_enable = d;
		if (p == 0x54)
			m_ddenlovr_bgcolor2 = d;
		if (p == 0x55)
			m_ddenlovr_priority2 = d;
		if (p == 0x56)
			m_ddenlovr_layer_enable2 = d;
		if (p == 0x60 || p == 0x61)
			blitter_w(0, p & 1, d);
		if (p == 0x64 || p == 0x65)
			blitter_w(1, p & 1, d);
		if (p == 0x68)
			dswSel = d;
		if (p == 0x74)
			ZBank(0, d);
		if (p == 0x7c)
			MSM6295Write(0, d);
		if (p == 0x8c) {
			soundLatch = d;
			ZetNmi(1);
		}
		return;
	}
	if (HANA_BOARD && !(p >= 0x10 && p <= 0x1f)) {
		if (p == 0x20) {
			hanakanz_blitter_reg_w(port >> 8);
			hanakanz_blitter_data_w(d);
		}
		if (p == 0x21)
			HanaPaletteWrite(port, d);
		if (p == 0x40)
			lockout = d;
		if (p == 0x41)
			keyRow = d;
		if (p == 0x80)
			MSM6295Write(0, d);
		if (p == 0xa0 || p == 0xa1)
			BurnYM2413Write(p & 1, d);
		if (p >= 0xc0 && p <= 0xcf)
			RtcWrite(p & 15, d);
		if (p == 0xe0 || p == 0xe1)
			AY8910Write(0, p & 1, d);
		return;
	}
	if (board == BOARD_MJFLOVE && !(p >= 0x10 && p <= 0x1f)) {
		if (p == 0x40 || p == 0x41) {
			blitter_w(0, p & 1, d);
			return;
		}
		if (p == 0xf2) {
			ZRamBank(0, d);
			return;
		}
		if (p == 0xf8) {
			ZBank(0, d);
			return;
		}
		if (port >= 0x20 && port < 0x30)
			MixWrite(0, port - 0x20, d);
		if (port == 0x34)
			m_ddenlovr_bgcolor = d;
		if (port == 0x35)
			m_ddenlovr_priority = d;
		if (port == 0x36)
			m_ddenlovr_layer_enable = d;
		if (port == 0xfa)
			OkiBank(d & 7);
		if (port == 0x200 || port == 0x201)
			BurnYM2413Write(port & 1, d);
		if (port >= 0x280 && port <= 0x28f)
			RtcWrite(port & 15, d);
		if (port == 0x300 || port == 0x301)
			AY8910Write(0, port & 1, d);
		if (port == 0x380)
			MSM6295Write(0, d);
		return;
	}
	if (p >= 0x10 && p <= 0x13) {
		z80ctc_write(p & 3, d);
		return;
	}
	if (p >= 0x1c && p <= 0x1f) {
		z80pio_write_alt(p & 3, d);
		return;
	}
	if (board == BOARD_DAIREACH && !(p >= 0x18 && p <= 0x1b)) {
		if (p < 16)
			RtcWrite(p, d);
		if (p >= 0x20 && p < 0x30)
			MixWrite(0, p - 0x20, d);
		if (p == 0x34)
			m_ddenlovr_bgcolor = d;
		if (p == 0x35)
			m_ddenlovr_priority = d;
		if (p == 0x36)
			m_ddenlovr_layer_enable = d;
		if (p == 0x40 || p == 0x41)
			blitter_w(0, p & 1, d);
		if (p == 0x60 || p == 0x62)
			AY8910Write(0, p == 0x62, d);
		if (p == 0x64 || p == 0x65)
			BurnYM2413Write(p & 1, d);
		if (p == 0x66)
			MSM6295Write(0, d);
		if (p == 0x68)
			ZBank(0, d & 15);
		if (p == 0x6a)
			ZRamBank(0, d);
		if (p == 0x6e)
			prot[0] = d;
		if (p == 0x78)
			OkiBank(d & 1);
		if (p == 0x7e)
			ZBank(0, (romBank & 15) | ((d & 1) << 4));
		return;
	}
	if (SEL_BOARD && !(p >= 0x18 && p <= 0x1b)) {
		if (p < 16)
			RtcWrite(p, d);
		if (p >= 0x20 && p < 0x30)
			MixWrite(0, p - 0x20, d);
		if (p == 0x34)
			m_ddenlovr_bgcolor = d;
		if (p == 0x35)
			m_ddenlovr_priority = d;
		if (p == 0x36)
			m_ddenlovr_layer_enable = d;
		if (p == 0x40 || p == 0x41)
			blitter_w(0, p & 1, d);
		if (p == 0x50 || p == 0x51)
			BurnYM2413Write(p & 1, d);
		if (p == 0x54)
			MSM6295Write(0, d);
		if (p == 0x58 || p == 0x5c)
			AY8910Write(0, p == 0x5c, d);
		if (p == (board == BOARD_SELJAN2 ? 0x60 : 0x70))
			ZRamBank(0, d);
		if (p == (board == BOARD_SELJAN2 ? 0x70 : 0x60))
			ZBank(0, d & 15);
		if (p == (board == BOARD_SELJAN2 ? 0x80 : board == BOARD_JONGOH ? 0xe4 : board == BOARD_DAICHUKA ? 0xd4 : 0xd8))
			ZBank(0, (romBank & 15) | ((d & 1) << 4));
		if (board != BOARD_SELJAN2 && p == (board == BOARD_JONGOH ? 0xe8 : 0xdc))
			prot[0] = d;
		if (p == 0x90)
			OkiBank(d & 1);
		return;
	}
	if (SRY_BOARD && !(p >= 0x18 && p <= 0x1b)) {
		UINT8 in = board == BOARD_SRYUDENS ? 0x98 : 8, rtc = board == BOARD_SRYUDENS ? 0x80 : 0x50,
		      snd = board == BOARD_SRYUDENS ? 0 : 0x90;
		if (p == in)
			lockout = d;
		if (p == in + 1)
			keyRow = d;
		if (p >= rtc && p < rtc + 16)
			RtcWrite(p - rtc, d);
		if (p == snd)
			MSM6295Write(0, d);
		if (p == snd + 2 || p == snd + 3)
			BurnYM2413Write(p & 1, d);
		if (p == snd + 4 || p == snd + 5)
			AY8910Write(0, p & 1, d);
		if (p >= 0x20 && p < 0x30)
			MixWrite(0, p - 0x20, d);
		if (p == 0x34)
			m_ddenlovr_bgcolor = d;
		if (p == 0x35)
			m_ddenlovr_priority = d;
		if (p == 0x36)
			m_ddenlovr_layer_enable = d;
		if (p == 0x40 || p == 0x41)
			blitter_w(0, p & 1, d);
		if (p == 0x70)
			OkiBank(d & 1);
		if (board != BOARD_SRYUDENS && p == 0x80)
			DrvRAM[0x1f000] = d;
		return;
	}

	if (p >= 0x18 && p <= 0x1b) {
		SioWrite(p, d);
		return;
	}
	if (board == BOARD_FUNKYFIG) {
		if (p == 0)
			MSM6295Write(0, d);
		if (p == 2) {
			soundLatch = d;
			ZetNmi(1);
		}
		if (p == 0x20 || p == 0x21)
			blitter_w_funkyfig(0, p & 1, d);
		if (p >= 0x40 && p < 0x50)
			RtcWrite(p - 0x40, d);
		if (p >= 0x60 && p < 0x70)
			MixWrite(0, p - 0x60, d);
		if (p == 0x74)
			m_ddenlovr_bgcolor = d;
		if (p == 0x75)
			m_ddenlovr_priority = d;
		if (p == 0x76)
			m_ddenlovr_layer_enable = d;
		if (p == 0x80)
			inputSel = d;
		if (p == 0x81 && inputSel == 0x2c)
			lockout = d;
		return;
	}
	if (LATE_MAHJONG) {
		if (p < 16) {
			RtcWrite(p, d);
			return;
		}
		if (p >= 0x20 && p < 0x30)
			MixWrite(0, p - 0x20, d);
		if (p == 0x34)
			m_ddenlovr_bgcolor = d;
		if (p == 0x35)
			m_ddenlovr_priority = d;
		if (p == 0x36)
			m_ddenlovr_layer_enable = d;
		if (p == 0x40 || p == 0x41)
			blitter_w(0, p & 1, d);
		if (p == 0x70 || p == 0x71)
			AY8910Write(0, p & 1, d);
		if (p == 0x74)
			MSM6295Write(0, d);
		if (p == 0x78 || p == 0x79)
			BurnYM2413Write(p & 1, d);
		if (board == BOARD_MJSCHUKA) {
			if (p == 0x50)
				OkiBank(d & 7);
			if (p == 0x54)
				prot[0] = d;
			if (p == 0x60)
				lockout = d;
			if (p == 0x61)
				keyRow = d;
		} else {
			if (p == 0x50)
				lockout = d;
			if (p == 0x51)
				keyRow = d;
		}
		return;
	}
	if (board == BOARD_HGINGA) {
		if (p < 2) {
			hginga_blitter_w(p, d);
			return;
		}
		if (p == 0x22 || p == 0x23)
			BurnYM2413Write(p & 1, d);
		if (p == 0x26 || p == 0x28)
			AY8910Write(0, p == 0x26 ? 1 : 0, d);
		if (p == 0x40) {
			inputSel = d;
			keyRow = 0;
		}
		if (p == 0x41 && inputSel == 0x2c)
			lockout = d;
		if (p >= 0x60 && p < 0x70)
			RtcWrite(p - 0x60, d);
		if (p >= 0xa0 && p < 0xb0)
			MixWrite(0, p - 0xa0, d);
		if (p == 0xb4)
			m_ddenlovr_bgcolor = d;
		if (p == 0xb5)
			m_ddenlovr_priority = d;
		if (p == 0xb6)
			m_ddenlovr_layer_enable = d;
		return;
	}
	if (board == BOARD_HPARADIS && p == 0xa0) {
		inputSel = d;
		keyRow = 0;
		return;
	}
	if (MAHJONG_BOARD) {
		if (p == 0x20) {
			inputSel = d;
			if (d & 0x80)
				keyRow = 0;
			return;
		}
		if (p == 0x42 || p == 0x43) {
			BurnYM2413Write(p & 1, d);
			return;
		}
		if (p == 0x46 || p == 0x48) {
			AY8910Write(0, p == 0x46 ? 1 : 0, d);
			return;
		}
		if (p >= 0x60 && p < 0x70) {
			RtcWrite(p - 0x60, d);
			return;
		}
	}
	if (p < 2) {
		blitter_w(0, p, d);
		return;
	}
	if (p == 0x40) {
		MSM6295Write(0, d);
		return;
	}
	if (p == 0x60 || p == 0x61) {
		BurnYM2413Write(p & 1, d);
		return;
	}
	if (p >= 0x80 && p < 0x90) {
		INT32 o = p - 0x80;
		if (o < 4)
			m_ddenlovr_palette_base[o] = d;
		else if (o < 8)
			m_ddenlovr_palette_mask[o - 4] = d;
		else if (o < 12)
			m_ddenlovr_transparency_pen[o - 8] = d;
		else
			m_ddenlovr_transparency_mask[o - 12] = d;
		return;
	}
	if (p == 0x94)
		m_ddenlovr_bgcolor = d;
	if (p == 0x95)
		m_ddenlovr_priority = d;
	if (p == 0x96)
		m_ddenlovr_layer_enable = d;
	if (MAHJONG_BOARD)
		return;
	UINT8 rtc = board == BOARD_QUIZCHQ ? 0xa0 : 0x20, inp = board == BOARD_QUIZCHQ ? 0x20 : 0xa0;
	if (p >= rtc && p < rtc + 16)
		RtcWrite(p - rtc, d);
	if (p == inp)
		inputSel = d;
	if (board == BOARD_QUIZCHQ && p == 0xc0)
		OkiBank(d & 1);
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 512; i++) {
			if (HANA_BOARD)
				HanaColour(i);
			else
				PaletteWrite(i, m_palram[i]);
		}
		DrvRecalc = 0;
	}
	BurnTransferClear(m_ddenlovr_bgcolor);
	static const INT32 order[24][4] = { { 3, 2, 1, 0 }, { 2, 3, 1, 0 }, { 3, 1, 2, 0 }, { 1, 3, 2, 0 }, { 2, 1, 3, 0 }, { 1, 2, 3, 0 },
	                                    { 3, 2, 0, 1 }, { 2, 3, 0, 1 }, { 3, 0, 2, 1 }, { 0, 3, 2, 1 }, { 2, 0, 3, 1 }, { 0, 2, 3, 1 },
	                                    { 3, 1, 0, 2 }, { 1, 3, 0, 2 }, { 3, 0, 1, 2 }, { 0, 3, 1, 2 }, { 1, 0, 3, 2 }, { 0, 1, 3, 2 },
	                                    { 2, 1, 0, 3 }, { 1, 2, 0, 3 }, { 2, 0, 1, 3 }, { 0, 2, 1, 3 }, { 1, 0, 2, 3 }, { 0, 1, 2, 3 } };
	for (INT32 group = 0; group <= (m_extra_layers ? 1 : 0); group++) {
		INT32 pri = group ? m_ddenlovr_priority2 : m_ddenlovr_priority;
		if (pri >= 24)
			pri = 0;
		for (INT32 i = 0; i < 4; i++) {
			INT32 l = order[pri][i] + group * 4;
			if (!((group ? m_ddenlovr_layer_enable2 : m_ddenlovr_layer_enable) & (1 << (l & 3))))
				continue;
			INT32 mask = m_ddenlovr_palette_mask[l], base = m_ddenlovr_palette_base[l] & ~mask, tm = m_ddenlovr_transparency_mask[l],
			      tp = m_ddenlovr_transparency_pen[l] & tm;
			for (INT32 y = 0; y < nScreenHeight; y++)
				for (INT32 x = 0; x < nScreenWidth; x++) {
					INT32 pen = m_ddenlovr_pixmap
					    [l][(((y +
						       (board == BOARD_HTENGOKU                                                                         ? 8
						        : (board == BOARD_MJGNIGHT || board == BOARD_JONGTEIA || (KL_BOARD && board >= BOARD_DAIMYOJN)) ? 1
						        : board == BOARD_MJMYORNT                                                                       ? 4
						                                                                                                        : 5) +
						       m_ddenlovr_scroll[group * 8 + (l & 3) + 4]) &
						      511) *
						     512) +
						    ((x + m_ddenlovr_scroll[group * 8 + (l & 3)]) & 511)];
					if ((pen & tm) != tp)
						pTransDraw[y * nScreenWidth + x] = base | (pen & mask);
				}
		}
	}
	BurnTransferCopy(DrvPalette);
	return 0;
}

static INT32 DrvResetFunc()
{
	// Preserve battery-backed RAM on a soft reset.
	mjfloveIrqPending = 0;
	memset(&rev2, 0, sizeof(rev2));
	memset(AllRam + (HANA_BOARD                ? 0x8000
	                 : board == BOARD_HTENGOKU ? 0x1000
	                                           : 0),
	       0,
	       RamEnd - AllRam -
	           (HANA_BOARD                ? 0x8000
	            : board == BOARD_HTENGOKU ? 0x1000
	                                      : 0));
	memset(hanaPalette, 0, sizeof(hanaPalette));
	hanaPaletteIndex = hanaGfxLatch = hanaPaletteSelect = 0;
	memset(&v, 0, sizeof(v));
	memset(m_palram, 0, sizeof(m_palram));
	memset(prot, 0, sizeof(prot));
	m_ddenlovr_clip_ctrl = 15;
	m_ddenlovr_clip_width = m_ddenlovr_clip_height = 0x400;
	m_ddenlovr_blit_pen_mask = 255;
	m_ddenlovr_blit_rom_bits = 8;
	m_extra_layers = board == BOARD_MMPANIC;
	m_ddenlovr_layer_enable2 = 15;
	m_ddenlovr_layer_enable = 15;
	m_ddenlovr_blit_commands = (board == BOARD_ULTRCHMP || board == BOARD_MJSCHUKA || board == BOARD_SRYUDENS || board == BOARD_DTOYOKEN ||
	                            board == BOARD_MJFLOVE || (SEL_BOARD && board != BOARD_DAIREACH))
	                               ? mjflove_commands
	                               : ddenlovr_commands;
	if (HANA_BOARD) {
		m_ddenlovr_blit_commands = hanakanz_commands;
		m_ddenlovr_blit_rom_bits = 16;
	}
	memset(sioWr, 0, sizeof(sioWr));
	sioStatus[0] = sioStatus[1] = 0x54;
	romBank = ramBank = keyRow = 0;
	dswSel = inputSel = 0;
	extraCycles = rtcFrames = 0;
	rtcIrqTicks = 0;
	rtcEnabled = rtcLine = rtcPulse = 0;
	// A CPU reset must not reset this battery-powered inspection clock.
	if (board != BOARD_DAICHUKAA || !(rtcRegs[8] + 10 * rtcRegs[9])) {
		memset(rtcRegs, 0, sizeof(rtcRegs));
		time_t now = time(NULL);
		tm *t = localtime(&now);
		if (t) {
			INT32 a[6] = { t->tm_sec, t->tm_min, t->tm_hour, t->tm_mday, t->tm_mon + 1, t->tm_year % 100 };
			for (INT32 i = 0; i < 6; i++) {
				rtcRegs[i * 2] = a[i] % 10;
				rtcRegs[i * 2 + 1] = a[i] / 10;
			}
			rtcRegs[12] = t->tm_wday;
		}
	}
	rtcRegs[13] = rtcRegs[14] = 0;
	rtcRegs[15] = 4;
	if (board >= 7) {
		ZetOpen(0);
		ZetReset();
		if (KL_BOARD)
			KlReset();
		ZBank(0, 0);
		ZRamBank(0, 0);
		ZetClose();
	} else {
		SekOpen(0);
		SekReset();
		SekClose();
	}
	if (SOUND_CPU) {
		ZetOpen(1);
		ZetReset();
		ZetClose();
	}
	hopperTicks = 0;
	mjfloveIrqCause = mjfloveIrqPending = 0;
	lockout = soundLatch = 0;
	soundExtra = 0;
	BurnYM2413Reset();
	AY8910Reset(0);
	MSM6295Reset(0);
	OkiBank(0);
	DrvRecalc = 1;
	return 0;
}

static INT32 DrvInit(INT32 type, INT32 romlen, INT32 gfxlen, INT32 sndlen, const RomLoad *loads, INT32 count)
{
	memset(rtcRegs, 0, sizeof(rtcRegs));
	const char *path = getenv("DYNAX_TRACE");
	if (path && *path)
		trace = fopen(path, "w");
	traceFrames = traceWrites = 0;
	board = type;
	BurnSetRefreshRate((KL_BOARD && board >= BOARD_DAIMYOJN)                  ? 59.7922
	                   : (SRY_BOARD || SEL_BOARD)                             ? 60.8532
	                   : (board == BOARD_KOTBINYO || board == BOARD_KOTBINSP) ? 60.1656
	                                                                          : 60.0);
	RomLen = romlen;
	GfxLen = gfxlen;
	SndLen = sndlen;
	INT32 len = RomLen + GfxLen + SndLen + 0x20000 + 8 * 512 * 512 + 0x800 + 0x20000;
	AllMem = (UINT8 *)BurnMalloc(len);
	if (!AllMem)
		return 1;
	memset(AllMem, 0, len);
	SoundROM = AllMem;
	DrvROM = SoundROM + 0x20000;
	GfxROM = DrvROM + RomLen;
	SndROM = GfxROM + GfxLen;
	DrvPalette = (UINT32 *)(SndROM + SndLen);
	AllRam = (UINT8 *)(DrvPalette + 512);
	DrvRAM = AllRam;
	for (INT32 i = 0; i < 8; i++)
		m_ddenlovr_pixmap[i] = DrvRAM + 0x20000 + i * 512 * 512;
	RamEnd = MemEnd = AllMem + len;
	UINT8 *tmp = (UINT8 *)BurnMalloc(0x800000);
	if (!tmp) {
		BurnFree(AllMem);
		return 1;
	}
	for (INT32 i = 0; i < count; i++) {
		const RomLoad &r = loads[i];
		BurnRomInfo info = {};
		BurnDrvGetRomInfo(&info, r.index);
		INT32 capacity = r.region == 0 ? RomLen : r.region == 1 ? GfxLen : r.region == 2 ? SndLen : 0x20000;
		if (r.region < 0 || r.region > 3 || r.offset < 0 || r.source < 0 || r.length <= 0 || r.stride <= 0 || info.nLen > 0x800000 ||
		    UINT32(r.source + r.length) > info.nLen || INT64(r.offset) + INT64(r.length - 1) * r.stride >= capacity) {
			BurnFree(tmp);
			BurnFree(AllMem);
			return 1;
		}
		if (BurnLoadRom(tmp, r.index, 1)) {
			BurnFree(tmp);
			BurnFree(AllMem);
			return 1;
		}
		UINT8 *dst = r.region == 0 ? DrvROM : r.region == 1 ? GfxROM : r.region == 2 ? SndROM : SoundROM;
		for (INT32 j = 0; j < r.length; j++)
			dst[r.offset + j * r.stride] = tmp[r.source + j];
	}
	BurnFree(tmp);
	if (board >= 7) {
		ZetInit(0);
		ZetOpen(0);
		if (KL_BOARD) {
			ZetDaisyInit(0, 0);
			z80daisy_set_external_irq(KlIrqState, KlIrqAck, KlReti);
		}
		if (TMP_BOARD) {
			ZetDaisyInit(Z80_CTC, Z80_PIO);
			if (board == BOARD_MJFLOVE)
				z80daisy_set_external_irq(ExternalIrqState, ExternalIrqAck);
			z80ctc_init(8000000, 8000000, 0, ZIrq, NULL, NULL, NULL);
			z80pio_init(ZIrq, ZDsw, NULL,
			            HANA_BOARD                                                                           ? ZBank
			            : SEL_BOARD                                                                          ? KeySelect
			            : (MAHJONG_BOARD || board == BOARD_HGINGA || LATE_MAHJONG || GOK_BOARD || SRY_BOARD) ? ZRamBank
			                                                                                                 : NULL,
			            HANA_BOARD               ? HanaOkiBank
			            : board == BOARD_MJFLOVE ? KeySelect
			            : SEL_BOARD              ? CoinSelect
			                                     : ZBank,
			            NULL, NULL);
		}
		ZetMapMemory(DrvROM, 0, 0x5fff, MAP_ROM);
		ZetMapMemory(DrvRAM, 0x6000, 0x6fff, MAP_RAM);
		if (board == BOARD_MMPANIC)
			ZetMapMemory(NULL, 0, 0xff, MAP_READ);
		if (board == BOARD_RONGRONG)
			ZetMapMemory(NULL, 0x6000, 0x60ff, MAP_READ);
		ZetSetReadHandler(ZRead);
		ZetSetWriteHandler(ZWrite);
		ZetSetInHandler(ZIn);
		ZetSetOutHandler(ZOut);
		ZetClose();
	} else {
		SekInit(0, 0x68000);
		SekOpen(0);
		SekMapMemory(DrvROM, 0, RomLen - 1, MAP_ROM);
		SekMapMemory(DrvRAM, 0xff0000, 0xffffff, MAP_RAM);
		SekSetReadByteHandler(0, ReadByte);
		SekSetReadWordHandler(0, ReadWord);
		SekSetWriteByteHandler(0, WriteByte);
		SekSetWriteWordHandler(0, WriteWord);
		SekClose();
	}
	if (SOUND_CPU) {
		ZetInit(1);
		ZetOpen(1);
		ZetMapMemory(SoundROM, 0, 0x5fff, MAP_ROM);
		ZetMapMemory(SoundROM + 0x8000, 0x8000, 0xffff, MAP_ROM);
		ZetMapMemory(DrvRAM + 0x9000, 0x6000, 0x66ff, MAP_RAM);
		ZetSetInHandler(SoundIn);
		ZetSetOutHandler(SoundOut);
		ZetClose();
	}
	BurnYM2413Init(((HANA_BOARD && !KL_LATE) ? 28375160 : 28636363) / 8);
	BurnYM2413SetAllRoutes(board == BOARD_HTENGOKU ? 1.0 : 0.8, BURN_SND_ROUTE_BOTH);
	AY8910Init(0,
	           board == BOARD_HTENGOKU ? 1250000
	           : (board == BOARD_MJMYSTER || board == BOARD_MJMYWRLD || board == BOARD_HGINGA || GOK_BOARD || LATE_MAHJONG || SRY_BOARD ||
	              SEL_BOARD || board == BOARD_MJFLOVE)
	               ? 3579545
				   : 28636363 / 16,
	           1);
	AY8910SetAllRoutes(0, board == BOARD_HTENGOKU ? 0.2 : HANA_BOARD ? 1.0 : 0.3, BURN_SND_ROUTE_BOTH);
	if (board == BOARD_HTENGOKU)
		AY8910SetPorts(0, Rev2Dsw, NULL, NULL, DswSelect);
	if (SEL_BOARD)
		AY8910SetPorts(0, SelDsw, NULL, NULL, DswSelect);
	if (board == BOARD_HGINGA || GOK_BOARD)
		AY8910SetPorts(0, MahjongDsw, NULL, NULL, DswSelect);
	if (MAHJONG_BOARD)
		AY8910SetPorts(0, NULL, NULL, NULL, DswSelect);
	if (board == BOARD_QUIZ365)
		AY8910SetPorts(0, DswRead, NULL, NULL, DswSelect);
	MSM6295Init(0, ((HANA_BOARD && !KL_LATE) ? 28375160 : 28636363) / 28 / 132, 1);
	MSM6295SetRoute(0, 0.8, BURN_SND_ROUTE_BOTH);
	GenericTilesInit();
	return DrvResetFunc();
}

static INT32 DrvExit()
{
	if (board >= 7) {
		ZetExit();
	} else
		SekExit();
	BurnYM2413Exit();
	AY8910Exit(0);
	MSM6295Exit(0);
	GenericTilesExit();
	BurnFree(AllMem);
	if (trace) {
		fclose(trace);
		trace = NULL;
	}
	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset)
		DrvResetFunc();
	hopperTicks++;
	if (board >= 7) {
		ZetNewFrame();
		if (KL_BOARD)
			kl.lastCpu = 0;
	} else
		SekNewFrame();
	memset(Inputs, 255, 3);
	for (INT32 p = 0; p < 3; p++)
		for (INT32 b = 0; b < 8; b++)
			Inputs[p] ^= (Joy[p][b] & 1) << b;
	memset(Matrix, 255, sizeof(Matrix));
	for (INT32 p = 0; p < 2; p++)
		for (INT32 r = 0; r < 5; r++)
			for (INT32 b = 0; b < 6; b++)
				Matrix[p][r] ^= (MatrixJoy[p][r][b] & 1) << b;
	if (board >= 7)
		ZetOpen(0);
	else
		SekOpen(0);
	INT32 total = INT32((KL_BOARD                  ? 1000000000LL
	                     : board == BOARD_HTENGOKU ? 500000000LL
	                     : board >= 7              ? 800000000LL
	                                               : 1200000000LL) /
	                    nBurnFPS);
	INT32 soundDone = soundExtra;
	INT32 done = extraCycles, sound = 0;
	for (INT32 i = 0; i < 256; i++) {
		INT32 cycles = (i + 1) * total / 256 - done;
		done += KL_BOARD ? KlRun(cycles) : board >= 7 ? ZetRun(cycles) : SekRun(cycles);
		if (board >= 7) {
			if (KL_BOARD) {
				if (i == 0)
					KlInput(0, 0);
				if (i == 245)
					KlInput(0, 1);
			} else if (board == BOARD_HTENGOKU) {
				if (i == 248) {
					rev2.irq |= 0x10;
					Rev2Irq();
				}
			} else if (board == BOARD_MJFLOVE) {
				if (i == 0)
					z80ctc_trg_write(0, 0);
				if (i == 244) {
					z80ctc_trg_write(0, 1);
					ExternalIrq(1);
				}
			} else if (board == BOARD_MMPANIC) {
				if (i == 244) {
					ZetSetVector(0xcf);
					ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
					ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD);
				}
			} else if (board == BOARD_HGINGA || GOK_BOARD || LATE_MAHJONG || SRY_BOARD || SEL_BOARD || HANA_BOARD) {
				if (i == 0)
					z80ctc_trg_write(0, 0);
				if (i == 244)
					z80ctc_trg_write(0, 1);
			} else if (board == BOARD_FUNKYFIG) {
				if (i == 0)
					z80ctc_trg_write(0, 0);
				if (i == 244) {
					z80ctc_trg_write(0, 1);
					ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD);
				}
			} else if (MAHJONG_BOARD) {
				if (i == 0)
					z80ctc_trg_write(0, 1);
				if (i == 244)
					z80ctc_trg_write(0, 0);
			} else {
				if (i == 0)
					z80pio_strobeA(1);
				if (i == 244)
					z80pio_strobeA(0);
			}
			RtcTick();
		} else if (i == 244)
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		if (SOUND_CPU) {
			ZetClose();
			ZetOpen(1);
			soundDone += ZetRun((i + 1) * (14318181 / 4 / 60) / 256 - soundDone);
			ZetClose();
			ZetOpen(0);
		}
		INT32 target = (i + 1) * nBurnSoundLen / 256, n = target - sound;
		if (pBurnSoundOut && n) {
			BurnYM2413Render(pBurnSoundOut + sound * 2, n);
			if (!KL_BOARD)
				AY8910Render(pBurnSoundOut + sound * 2, n);
			MSM6295Render(0, pBurnSoundOut + sound * 2, n);
		}
		sound = target;
	}
	soundExtra = soundDone - (SOUND_CPU ? 14318181 / 4 / 60 : 0);
	extraCycles = done - total;
	if (board >= 7)
		ZetClose();
	else
		SekClose();
	rtcFrames += 100;
	if (rtcFrames >= nBurnFPS) {
		rtcFrames -= nBurnFPS;
		if (!(rtcRegs[13] & 1) && !(rtcRegs[15] & 2)) {
			// Carry through the calendar as well as the time: the inspection
			// countdown must expire naturally as the emulated date advances.
			INT32 month = rtcRegs[8] + 10 * rtcRegs[9];
			INT32 year = rtcRegs[10] + 10 * rtcRegs[11];
			static const INT32 days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
			INT32 monthDays = month >= 1 && month <= 12 ? days[month - 1] : 31;
			if (month == 2 && !(year & 3))
				monthDays++;
			INT32 limits[6] = { 60, 60, 24, monthDays + 1, 13, 100 };
			for (INT32 i = 0; i < 6; i++) {
				if (i == 3)
					rtcRegs[12] = (rtcRegs[12] + 1) % 7;
				INT32 n = rtcRegs[2 * i] + 10 * rtcRegs[2 * i + 1] + 1;
				INT32 value = n < limits[i] ? n : (i == 3 || i == 4) ? 1 : 0;
				rtcRegs[2 * i] = value % 10;
				rtcRegs[2 * i + 1] = value / 10;
				if (n < limits[i])
					break;
			}
		}
	}
	if (trace && ++traceFrames % 60 == 0) {
		fprintf(trace, "frame=%d pc=%06x bank=%02x pri=%d layers=%x src=%x xy=%d,%d irq=%d pio=%x ctc=%x\n", traceFrames,
		        board >= 7 ? ZetGetPC(0) : SekGetPC(0), dswSel, m_ddenlovr_priority, m_ddenlovr_layer_enable, m_ddenlovr_blit_address,
		        m_ddenlovr_blit_x, m_ddenlovr_blit_y, m_ddenlovr_blitter_irq_flag, TMP_BOARD ? z80pio_irq_state() : 0,
		        TMP_BOARD ? z80ctc_irq_state() : 0);
		fflush(trace);
	}
	if (pBurnDraw)
		DrvDraw();
	return 0;
}

static INT32 DrvScan(INT32 action, INT32 *min)
{
	if (min)
		*min = 0x029418;
	if ((HANA_BOARD || board == BOARD_HTENGOKU) && (action & ACB_NVRAM)) {
		BurnArea ba = {};
		ba.Data = DrvRAM;
		ba.nLen = HANA_BOARD ? 0x8000 : 0x1000;
		ba.szName = "Battery RAM";
		BurnAcb(&ba);
	}
	if (board == BOARD_DAICHUKAA && (action & ACB_NVRAM)) {
		BurnArea ba = {};
		ba.Data = rtcRegs;
		ba.nLen = 13; // Calendar only; IRQ and control registers reset normally.
		ba.szName = "Inspection RTC";
		BurnAcb(&ba);
	}
	if (action & ACB_VOLATILE) {
		BurnRandomScan(action);
		BurnArea ba = {};
		ba.Data = AllRam;
		ba.nLen = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
		if (board >= 7) {
			ZetScan(action);
		} else
			SekScan(action);
		BurnYM2413Scan(action, min);
		AY8910Scan(action, min);
		MSM6295Scan(action, min);
		SCAN_VAR(kl);
		SCAN_VAR(v);
		SCAN_VAR(m_palram);
		SCAN_VAR(prot);
		SCAN_VAR(dswSel);
		SCAN_VAR(romBank);
		SCAN_VAR(ramBank);
		SCAN_VAR(keyRow);
		SCAN_VAR(inputSel);
		SCAN_VAR(okiBank);
		SCAN_VAR(rtcRegs);
		SCAN_VAR(sioWr);
		SCAN_VAR(sioStatus);
		SCAN_VAR(rtcFrames);
		SCAN_VAR(rtcIrqTicks);
		SCAN_VAR(rtcEnabled);
		SCAN_VAR(rtcLine);
		SCAN_VAR(rtcPulse);
		SCAN_VAR(extraCycles);
		SCAN_VAR(soundExtra);
		SCAN_VAR(soundLatch);
		SCAN_VAR(lockout);
		SCAN_VAR(mjfloveIrqCause);
		SCAN_VAR(mjfloveIrqPending);
		SCAN_VAR(hanaPalette);
		SCAN_VAR(hanaPaletteIndex);
		SCAN_VAR(hanaGfxLatch);
		SCAN_VAR(hanaPaletteSelect);
		SCAN_VAR(rev2);
		SCAN_VAR(hopperTicks);
	}
	if (action & ACB_WRITE) {
		if (board >= 7) { // Restore mapping without replaying bank-write side effects on the input sequencer.
			UINT8 savedRam = ramBank, savedDsw = dswSel, savedKey = keyRow;
			ZetOpen(0);
			if (KL_BOARD)
				KlRemap();
			ZBank(0, romBank);
			ZRamBank(0, savedRam);
			if (board == BOARD_HTENGOKU)
				Rev2Irq();
			if (KL_BOARD)
				KlLine();
			ZetClose();
			dswSel = savedDsw;
			keyRow = savedKey;
		}
		OkiBank(okiBank);
		DrvRecalc = 1;
	}
	return 0;
}

static struct BurnInputInfo akamaruInputList[] = {
	{ "P1 Start",    BIT_DIGITAL,   Joy[0] + 0, "p1 start"  },
	{ "P1 Up",       BIT_DIGITAL,   Joy[0] + 1, "p1 up"     },
	{ "P1 Down",     BIT_DIGITAL,   Joy[0] + 2, "p1 down"   },
	{ "P1 Left",     BIT_DIGITAL,   Joy[0] + 3, "p1 left"   },
	{ "P1 Right",    BIT_DIGITAL,   Joy[0] + 4, "p1 right"  },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[0] + 5, "p1 fire 1" },
	{ "P2 Start",    BIT_DIGITAL,   Joy[1] + 0, "p2 start"  },
	{ "P2 Up",       BIT_DIGITAL,   Joy[1] + 1, "p2 up"     },
	{ "P2 Down",     BIT_DIGITAL,   Joy[1] + 2, "p2 down"   },
	{ "P2 Left",     BIT_DIGITAL,   Joy[1] + 3, "p2 left"   },
	{ "P2 Right",    BIT_DIGITAL,   Joy[1] + 4, "p2 right"  },
	{ "P2 Button 1", BIT_DIGITAL,   Joy[1] + 5, "p2 fire 1" },
	{ "P1 Coin",     BIT_DIGITAL,   Joy[2] + 0, "p1 coin"   },
	{ "P2 Coin",     BIT_DIGITAL,   Joy[2] + 1, "p2 coin"   },
	{ "Service",     BIT_DIGITAL,   Joy[2] + 2, "service"   },
	{ "Test",        BIT_DIGITAL,   Joy[2] + 3, "diag"      },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(akamaru)

static struct BurnDIPInfo akamaruDIPList[] = {
	{ 0x11, 0xff, 0xff, 0xff, NULL          },
	{ 0x12, 0xff, 0xff, 0xff, NULL          },
	{ 0x13, 0xff, 0xff, 0xff, NULL          },
	{ 0x14, 0xff, 0xff, 0xff, NULL          },
	{ 0x15, 0xff, 0xff, 0xff, NULL          },
	{ 0x16, 0xff, 0xff, 0xff, NULL          },
	{ 0x17, 0xff, 0xff, 0xff, NULL          },
	{ 0x18, 0xff, 0xff, 0xff, NULL          },

	{ 0,    0xfe, 0,    4,    "Coin A"      },
	{ 0x11, 1,    0x03, 0x00, "3C 1C"       },
	{ 0x11, 1,    0x03, 0x01, "2C 1C"       },
	{ 0x11, 1,    0x03, 0x03, "1C 1C"       },
	{ 0x11, 1,    0x03, 0x02, "1C 2C"       },

	{ 0,    0xfe, 0,    4,    "Coin B"      },
	{ 0x11, 1,    0x0c, 0x00, "3C 1C"       },
	{ 0x11, 1,    0x0c, 0x04, "2C 1C"       },
	{ 0x11, 1,    0x0c, 0x0c, "1C 1C"       },
	{ 0x11, 1,    0x0c, 0x08, "1C 2C"       },

	{ 0,    0xfe, 0,    2,    "Demo Sounds" },
	{ 0x11, 1,    0x10, 0x00, "Off"         },
	{ 0x11, 1,    0x10, 0x10, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 1-5" },
	{ 0x11, 1,    0x20, 0x20, "Off"         },
	{ 0x11, 1,    0x20, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Flip Screen" },
	{ 0x11, 1,    0x40, 0x40, "Off"         },
	{ 0x11, 1,    0x40, 0x00, "On"          },

	{ 0,    0xfe, 0,    4,    "Difficulty"  },
	{ 0x12, 1,    0x03, 0x02, "Easy"        },
	{ 0x12, 1,    0x03, 0x03, "Normal"      },
	{ 0x12, 1,    0x03, 0x01, "Hard"        },
	{ 0x12, 1,    0x03, 0x00, "Hardest"     },

	{ 0,    0xfe, 0,    2,    "Unknown 2-2" },
	{ 0x12, 1,    0x04, 0x04, "Off"         },
	{ 0x12, 1,    0x04, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-3" },
	{ 0x12, 1,    0x08, 0x08, "Off"         },
	{ 0x12, 1,    0x08, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-4" },
	{ 0x12, 1,    0x10, 0x10, "Off"         },
	{ 0x12, 1,    0x10, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-5" },
	{ 0x12, 1,    0x20, 0x20, "Off"         },
	{ 0x12, 1,    0x20, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-6" },
	{ 0x12, 1,    0x40, 0x40, "Off"         },
	{ 0x12, 1,    0x40, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-7" },
	{ 0x12, 1,    0x80, 0x80, "Off"         },
	{ 0x12, 1,    0x80, 0x00, "On"          },
};

STDDIPINFO(akamaru)

static struct BurnInputInfo animaljrInputList[] = {
	{ "P1 Coin",     BIT_DIGITAL,   Joy[0] + 0, "p1 coin"   },
	{ "Service",     BIT_DIGITAL,   Joy[0] + 2, "service"   },
	{ "Test",        BIT_DIGITAL,   Joy[0] + 3, "diag"      },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[1] + 1, "p1 fire 1" },
	{ "P1 Button 2", BIT_DIGITAL,   Joy[1] + 2, "p1 fire 2" },
	{ "P1 Button 3", BIT_DIGITAL,   Joy[1] + 3, "p1 fire 3" },
	{ "P1 Button 4", BIT_DIGITAL,   Joy[1] + 4, "p1 fire 4" },
	{ "P1 Button 5", BIT_DIGITAL,   Joy[1] + 5, "p1 fire 5" },
	{ "P1 Button 6", BIT_DIGITAL,   Joy[1] + 6, "p1 fire 6" },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(animaljr)

static struct BurnDIPInfo animaljrDIPList[] = {
	{ 0x0a, 0xff, 0xff, 0xff, NULL                       },
	{ 0x0b, 0xff, 0xff, 0xff, NULL                       },
	{ 0x0c, 0xff, 0xff, 0xff, NULL                       },
	{ 0x0d, 0xff, 0xff, 0xff, NULL                       },
	{ 0x0e, 0xff, 0xff, 0xff, NULL                       },
	{ 0x0f, 0xff, 0xff, 0xff, NULL                       },
	{ 0x10, 0xff, 0xff, 0xff, NULL                       },
	{ 0x11, 0xff, 0xff, 0xff, NULL                       },

	{ 0,    0xfe, 0,    4,    "DSWA:1,2"                 },
	{ 0x0a, 1,    0x03, 0x00, "3C 1C"                    },
	{ 0x0a, 1,    0x03, 0x02, "2C 1C"                    },
	{ 0x0a, 1,    0x03, 0x03, "1C 1C"                    },
	{ 0x0a, 1,    0x03, 0x01, "1C 2C"                    },

	{ 0,    0xfe, 0,    8,    "Difficulty"               },
	{ 0x0a, 1,    0x1c, 0x08, "Level 1"                  },
	{ 0x0a, 1,    0x1c, 0x18, "Level 2"                  },
	{ 0x0a, 1,    0x1c, 0x14, "Level 3"                  },
	{ 0x0a, 1,    0x1c, 0x1c, "Level 4"                  },
	{ 0x0a, 1,    0x1c, 0x0c, "Level 5"                  },
	{ 0x0a, 1,    0x1c, 0x04, "Level 6"                  },
	{ 0x0a, 1,    0x1c, 0x00, "unknown Level (00)"       },
	{ 0x0a, 1,    0x1c, 0x10, "unknown Level (10)"       },

	{ 0,    0xfe, 0,    8,    "Game Hit Probability"     },
	{ 0x0a, 1,    0xe0, 0x40, "1/15"                     },
	{ 0x0a, 1,    0xe0, 0x60, "1/10"                     },
	{ 0x0a, 1,    0xe0, 0x80, "1/7"                      },
	{ 0x0a, 1,    0xe0, 0xe0, "1/5"                      },
	{ 0x0a, 1,    0xe0, 0xa0, "1/4"                      },
	{ 0x0a, 1,    0xe0, 0xc0, "1/3"                      },
	{ 0x0a, 1,    0xe0, 0x00, "unknown Probability (00)" },
	{ 0x0a, 1,    0xe0, 0x20, "unknown Probability (20)" },

	{ 0,    0xfe, 0,    2,    "DSWB:1"                   },
	{ 0x0b, 1,    0x01, 0x01, "Off"                      },
	{ 0x0b, 1,    0x01, 0x00, "On"                       },

	{ 0,    0xfe, 0,    2,    "DSWB:2"                   },
	{ 0x0b, 1,    0x02, 0x00, "Off"                      },
	{ 0x0b, 1,    0x02, 0x02, "On"                       },

	{ 0,    0xfe, 0,    2,    "Attract Sound Type"       },
	{ 0x0b, 1,    0x04, 0x04, "Type 1"                   },
	{ 0x0b, 1,    0x04, 0x00, "Type 2"                   },

	{ 0,    0xfe, 0,    4,    "Bonus Game Type"          },
	{ 0x0b, 1,    0x18, 0x18, "Type 1"                   },
	{ 0x0b, 1,    0x18, 0x10, "Type 2"                   },
	{ 0x0b, 1,    0x18, 0x08, "Without"                  },
	{ 0x0b, 1,    0x18, 0x00, "Type 2 (duplicate)"       },

	{ 0,    0xfe, 0,    2,    "Unknown B-6 (debug?)"     },
	{ 0x0b, 1,    0x20, 0x20, "Off"                      },
	{ 0x0b, 1,    0x20, 0x00, "On"                       },

	{ 0,    0xfe, 0,    2,    "Pirate Fight (debug?)"    },
	{ 0x0b, 1,    0x40, 0x40, "Off"                      },
	{ 0x0b, 1,    0x40, 0x00, "On"                       },

	{ 0,    0xfe, 0,    2,    "Taito Copyright (debug?)" },
	{ 0x0b, 1,    0x80, 0x00, "Off"                      },
	{ 0x0b, 1,    0x80, 0x80, "On"                       },

	{ 0,    0xfe, 0,    2,    "Unknown A-9 (debug?)"     },
	{ 0x0c, 1,    0x01, 0x01, "Off"                      },
	{ 0x0c, 1,    0x01, 0x00, "On"                       },

	{ 0,    0xfe, 0,    2,    "Tickets"                  },
	{ 0x0c, 1,    0x02, 0x02, "Off"                      },
	{ 0x0c, 1,    0x02, 0x00, "On"                       },

	{ 0,    0xfe, 0,    2,    "Unknown B-9 (debug?)"     },
	{ 0x0c, 1,    0x04, 0x04, "Off"                      },
	{ 0x0c, 1,    0x04, 0x00, "On"                       },
};

STDDIPINFO(animaljr)

static struct BurnInputInfo daimyojnInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P3 Coin",        BIT_DIGITAL,   Joy[2] + 7,          "p3 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(daimyojn)

static struct BurnDIPInfo daimyojnDIPList[] = {
	{ 0x3e, 0xff, 0xff, 0x47, NULL                                },
	{ 0x3f, 0xff, 0xff, 0x3f, NULL                                },
	{ 0x40, 0xff, 0xff, 0xec, NULL                                },
	{ 0x41, 0xff, 0xff, 0xe1, NULL                                },
	{ 0x42, 0xff, 0xff, 0xfa, NULL                                },
	{ 0x43, 0xff, 0xff, 0xff, NULL                                },
	{ 0x44, 0xff, 0xff, 0xff, NULL                                },
	{ 0x45, 0xff, 0xff, 0xff, NULL                                },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                       },
	{ 0x3e, 1,    0x0f, 0x00, "50%"                               },
	{ 0x3e, 1,    0x0f, 0x01, "53%"                               },
	{ 0x3e, 1,    0x0f, 0x02, "56%"                               },
	{ 0x3e, 1,    0x0f, 0x03, "59%"                               },
	{ 0x3e, 1,    0x0f, 0x04, "62%"                               },
	{ 0x3e, 1,    0x0f, 0x05, "65%"                               },
	{ 0x3e, 1,    0x0f, 0x06, "68%"                               },
	{ 0x3e, 1,    0x0f, 0x07, "71%"                               },
	{ 0x3e, 1,    0x0f, 0x08, "75%"                               },
	{ 0x3e, 1,    0x0f, 0x09, "78%"                               },
	{ 0x3e, 1,    0x0f, 0x0a, "81%"                               },
	{ 0x3e, 1,    0x0f, 0x0b, "84%"                               },
	{ 0x3e, 1,    0x0f, 0x0c, "87%"                               },
	{ 0x3e, 1,    0x0f, 0x0d, "90%"                               },
	{ 0x3e, 1,    0x0f, 0x0e, "93%"                               },
	{ 0x3e, 1,    0x0f, 0x0f, "96%"                               },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                         },
	{ 0x3e, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"               },
	{ 0x3e, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"                },
	{ 0x3e, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"               },
	{ 0x3e, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"              },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                       },
	{ 0x3e, 1,    0xc0, 0xc0, "1"                                 },
	{ 0x3e, 1,    0xc0, 0x80, "5"                                 },
	{ 0x3e, 1,    0xc0, 0x40, "10"                                },
	{ 0x3e, 1,    0xc0, 0x00, "20"                                },

	{ 0,    0xfe, 0,    4,    "Coinage"                           },
	{ 0x3f, 1,    0x03, 0x03, "1C 1C"                             },
	{ 0x3f, 1,    0x03, 0x02, "1C 2C"                             },
	{ 0x3f, 1,    0x03, 0x01, "1C 5C"                             },
	{ 0x3f, 1,    0x03, 0x00, "1C 10C"                            },

	{ 0,    0xfe, 0,    8,    "Minimum Bet"                       },
	{ 0x3f, 1,    0x38, 0x38, "1"                                 },
	{ 0x3f, 1,    0x38, 0x30, "2"                                 },
	{ 0x3f, 1,    0x38, 0x28, "3"                                 },
	{ 0x3f, 1,    0x38, 0x20, "4"                                 },
	{ 0x3f, 1,    0x38, 0x18, "5"                                 },
	{ 0x3f, 1,    0x38, 0x10, "6"                                 },
	{ 0x3f, 1,    0x38, 0x08, "7"                                 },
	{ 0x3f, 1,    0x38, 0x00, "8"                                 },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                      },
	{ 0x3f, 1,    0xc0, 0xc0, "300"                               },
	{ 0x3f, 1,    0xc0, 0xc0, "600"                               },
	{ 0x3f, 1,    0xc0, 0xc0, "1500"                              },
	{ 0x3f, 1,    0xc0, 0xc0, "3000"                              },
	{ 0x3f, 1,    0xc0, 0x80, "500"                               },
	{ 0x3f, 1,    0xc0, 0x80, "1000"                              },
	{ 0x3f, 1,    0xc0, 0x80, "2500"                              },
	{ 0x3f, 1,    0xc0, 0x80, "5000"                              },
	{ 0x3f, 1,    0xc0, 0x40, "700"                               },
	{ 0x3f, 1,    0xc0, 0x40, "1400"                              },
	{ 0x3f, 1,    0xc0, 0x40, "3500"                              },
	{ 0x3f, 1,    0xc0, 0x40, "7000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "1000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "2000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "5000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "10000"                             },

	{ 0,    0xfe, 0,    8,    "Bonus Point Cycle"                 },
	{ 0x40, 1,    0x07, 0x07, "None"                              },
	{ 0x40, 1,    0x07, 0x06, "First time only"                   },
	{ 0x40, 1,    0x07, 0x05, "Every 300 coins"                   },
	{ 0x40, 1,    0x07, 0x04, "Every 500 coins"                   },
	{ 0x40, 1,    0x07, 0x03, "Every 700 coins"                   },
	{ 0x40, 1,    0x07, 0x02, "Every 1000 coins"                  },
	{ 0x40, 1,    0x07, 0x01, "Every 1000 coins"                  },
	{ 0x40, 1,    0x07, 0x00, "Every 1000 coins"                  },

	{ 0,    0xfe, 0,    2,    "Yakuman Chances Per Cycle"         },
	{ 0x40, 1,    0x08, 0x08, "1"                                 },
	{ 0x40, 1,    0x08, 0x00, "2"                                 },

	{ 0,    0xfe, 0,    2,    "Allow Sanbaiman as Yakuman Chance" },
	{ 0x40, 1,    0x10, 0x10, "No"                                },
	{ 0x40, 1,    0x10, 0x00, "Yes"                               },

	{ 0,    0xfe, 0,    2,    "Super Time Trial Frequency"        },
	{ 0x40, 1,    0x20, 0x20, "Normal"                            },
	{ 0x40, 1,    0x20, 0x00, "Somewhat common"                   },

	{ 0,    0xfe, 0,    2,    "DIP-SW3:7"                         },
	{ 0x40, 1,    0x40, 0x40, "Off"                               },
	{ 0x40, 1,    0x40, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "DIP-SW4:1"                         },
	{ 0x41, 1,    0x01, 0x01, "Off"                               },
	{ 0x41, 1,    0x01, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Don Den Button"                    },
	{ 0x41, 1,    0x02, 0x02, "Start"                             },
	{ 0x41, 1,    0x02, 0x00, "Flip Flop"                         },

	{ 0,    0xfe, 0,    2,    "Last Chance"                       },
	{ 0x41, 1,    0x04, 0x04, "Off"                               },
	{ 0x41, 1,    0x04, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                        },
	{ 0x41, 1,    0x08, 0x08, "Off"                               },
	{ 0x41, 1,    0x08, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Double Bet"                        },
	{ 0x41, 1,    0x10, 0x10, "Off"                               },
	{ 0x41, 1,    0x10, 0x00, "On"                                },

	{ 0,    0xfe, 0,    8,    "Credits Per Note"                  },
	{ 0x42, 1,    0x01, 0x01, "5"                                 },
	{ 0x42, 1,    0x01, 0x01, "10"                                },
	{ 0x42, 1,    0x01, 0x01, "25"                                },
	{ 0x42, 1,    0x01, 0x01, "50"                                },
	{ 0x42, 1,    0x01, 0x00, "10"                                },
	{ 0x42, 1,    0x01, 0x00, "20"                                },
	{ 0x42, 1,    0x01, 0x00, "50"                                },
	{ 0x42, 1,    0x01, 0x00, "100"                               },

	{ 0,    0xfe, 0,    2,    "DIP-SW1:10"                        },
	{ 0x42, 1,    0x02, 0x02, "Off"                               },
	{ 0x42, 1,    0x02, 0x00, "On"                                },

	{ 0,    0xfe, 0,    4,    "Computer Strength"                 },
	{ 0x42, 1,    0x0c, 0x00, "Weak"                              },
	{ 0x42, 1,    0x0c, 0x04, "Normal"                            },
	{ 0x42, 1,    0x0c, 0x08, "Somewhat strong"                   },
	{ 0x42, 1,    0x0c, 0x0c, "Strong"                            },

	{ 0,    0xfe, 0,    2,    "Game Style"                        },
	{ 0x42, 1,    0x10, 0x10, "Credit"                            },
	{ 0x42, 1,    0x10, 0x00, "Credit timer"                      },

	{ 0,    0xfe, 0,    2,    "Game Start Method"                 },
	{ 0x42, 1,    0x20, 0x20, "Normal"                            },
	{ 0x42, 1,    0x20, 0x00, "Automatic minimum bet"             },

	{ 0,    0xfe, 0,    2,    "Game Mode"                         },
	{ 0x42, 1,    0x40, 0x40, "Coin Pool"                         },
	{ 0x42, 1,    0x40, 0x00, "Medal/credit timer"                },

	{ 0,    0xfe, 0,    2,    "Bets?"                             },
	{ 0x44, 1,    0x40, 0x40, "0"                                 },
	{ 0x44, 1,    0x40, 0x00, "1"                                 },

	{ 0,    0xfe, 0,    2,    "Unknown"                           },
	{ 0x45, 1,    0x40, 0x40, "Off"                               },
	{ 0x45, 1,    0x40, 0x00, "On"                                },
};

STDDIPINFO(daimyojn)

static struct BurnInputInfo ddenlovjInputList[] = {
	{ "P1 Start",    BIT_DIGITAL,   Joy[0] + 0, "p1 start"  },
	{ "P1 Up",       BIT_DIGITAL,   Joy[0] + 1, "p1 up"     },
	{ "P1 Down",     BIT_DIGITAL,   Joy[0] + 2, "p1 down"   },
	{ "P1 Left",     BIT_DIGITAL,   Joy[0] + 3, "p1 left"   },
	{ "P1 Right",    BIT_DIGITAL,   Joy[0] + 4, "p1 right"  },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[0] + 5, "p1 fire 1" },
	{ "P1 Button 2", BIT_DIGITAL,   Joy[0] + 6, "p1 fire 2" },
	{ "P2 Start",    BIT_DIGITAL,   Joy[1] + 0, "p2 start"  },
	{ "P2 Up",       BIT_DIGITAL,   Joy[1] + 1, "p2 up"     },
	{ "P2 Down",     BIT_DIGITAL,   Joy[1] + 2, "p2 down"   },
	{ "P2 Left",     BIT_DIGITAL,   Joy[1] + 3, "p2 left"   },
	{ "P2 Right",    BIT_DIGITAL,   Joy[1] + 4, "p2 right"  },
	{ "P2 Button 1", BIT_DIGITAL,   Joy[1] + 5, "p2 fire 1" },
	{ "P2 Button 2", BIT_DIGITAL,   Joy[1] + 6, "p2 fire 2" },
	{ "P1 Coin",     BIT_DIGITAL,   Joy[2] + 0, "p1 coin"   },
	{ "P2 Coin",     BIT_DIGITAL,   Joy[2] + 1, "p2 coin"   },
	{ "Service",     BIT_DIGITAL,   Joy[2] + 2, "service"   },
	{ "Test",        BIT_DIGITAL,   Joy[2] + 3, "diag"      },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(ddenlovj)

static struct BurnDIPInfo ddenlovjDIPList[] = {
	{ 0x13, 0xff, 0xff, 0xff, NULL               },
	{ 0x14, 0xff, 0xff, 0xff, NULL               },
	{ 0x15, 0xff, 0xff, 0x7f, NULL               },
	{ 0x16, 0xff, 0xff, 0xff, NULL               },
	{ 0x17, 0xff, 0xff, 0xff, NULL               },
	{ 0x18, 0xff, 0xff, 0xff, NULL               },
	{ 0x19, 0xff, 0xff, 0xff, NULL               },
	{ 0x1a, 0xff, 0xff, 0xff, NULL               },

	{ 0,    0xfe, 0,    4,    "Coin A"           },
	{ 0x13, 1,    0x03, 0x00, "3C 1C"            },
	{ 0x13, 1,    0x03, 0x01, "2C 1C"            },
	{ 0x13, 1,    0x03, 0x03, "1C 1C"            },
	{ 0x13, 1,    0x03, 0x02, "1C 2C"            },

	{ 0,    0xfe, 0,    4,    "Coin B"           },
	{ 0x13, 1,    0x0c, 0x00, "3C 1C"            },
	{ 0x13, 1,    0x0c, 0x04, "2C 1C"            },
	{ 0x13, 1,    0x0c, 0x0c, "1C 1C"            },
	{ 0x13, 1,    0x0c, 0x08, "1C 2C"            },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"      },
	{ 0x13, 1,    0x10, 0x00, "Off"              },
	{ 0x13, 1,    0x10, 0x10, "On"               },

	{ 0,    0xfe, 0,    2,    "Helps"            },
	{ 0x13, 1,    0x20, 0x20, "1"                },
	{ 0x13, 1,    0x20, 0x00, "2"                },

	{ 0,    0xfe, 0,    2,    "Unknown 1-6"      },
	{ 0x13, 1,    0x40, 0x40, "Off"              },
	{ 0x13, 1,    0x40, 0x00, "On"               },

	{ 0,    0xfe, 0,    2,    "Unknown 1-7"      },
	{ 0x13, 1,    0x80, 0x80, "Off"              },
	{ 0x13, 1,    0x80, 0x00, "On"               },

	{ 0,    0xfe, 0,    8,    "Difficulty"       },
	{ 0x14, 1,    0x07, 0x06, "1 (Easy)"         },
	{ 0x14, 1,    0x07, 0x05, "2"                },
	{ 0x14, 1,    0x07, 0x04, "3"                },
	{ 0x14, 1,    0x07, 0x07, "4 (Normal)"       },
	{ 0x14, 1,    0x07, 0x03, "5"                },
	{ 0x14, 1,    0x07, 0x02, "6"                },
	{ 0x14, 1,    0x07, 0x01, "7"                },
	{ 0x14, 1,    0x07, 0x00, "8 (Hard)"         },

	{ 0,    0xfe, 0,    4,    "Timer Speed"      },
	{ 0x14, 1,    0x18, 0x10, "Slow"             },
	{ 0x14, 1,    0x18, 0x18, "Normal"           },
	{ 0x14, 1,    0x18, 0x08, "Fast"             },
	{ 0x14, 1,    0x18, 0x00, "Fastest"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-5"      },
	{ 0x14, 1,    0x20, 0x20, "Off"              },
	{ 0x14, 1,    0x20, 0x00, "On"               },

	{ 0,    0xfe, 0,    2,    "Unknown 2-6"      },
	{ 0x14, 1,    0x40, 0x40, "Off"              },
	{ 0x14, 1,    0x40, 0x00, "On"               },

	{ 0,    0xfe, 0,    2,    "Unknown 2-7"      },
	{ 0x14, 1,    0x80, 0x80, "Off"              },
	{ 0x14, 1,    0x80, 0x00, "On"               },

	{ 0,    0xfe, 0,    2,    "Flip Screen"      },
	{ 0x15, 1,    0x01, 0x01, "Off"              },
	{ 0x15, 1,    0x01, 0x00, "On"               },

	{ 0,    0xfe, 0,    2,    "Unknown 2-8"      },
	{ 0x15, 1,    0x04, 0x04, "Off"              },
	{ 0x15, 1,    0x04, 0x00, "On"               },

	{ 0,    0xfe, 0,    2,    "Unknown 2-9"      },
	{ 0x15, 1,    0x08, 0x08, "Off"              },
	{ 0x15, 1,    0x08, 0x00, "On"               },

	{ 0,    0xfe, 0,    2,    "Unknown 3-4"      },
	{ 0x15, 1,    0x10, 0x10, "Off"              },
	{ 0x15, 1,    0x10, 0x00, "On"               },

	{ 0,    0xfe, 0,    2,    "Unknown 3-5"      },
	{ 0x15, 1,    0x20, 0x20, "Off"              },
	{ 0x15, 1,    0x20, 0x00, "On"               },

	{ 0,    0xfe, 0,    2,    "Unknown 3-6"      },
	{ 0x15, 1,    0x40, 0x40, "Off"              },
	{ 0x15, 1,    0x40, 0x00, "On"               },

	{ 0,    0xfe, 0,    2,    "Force Test Mode?" },
	{ 0x15, 1,    0x80, 0x00, "No"               },
	{ 0x15, 1,    0x80, 0x80, "Yes"              },
};

STDDIPINFO(ddenlovj)

static struct BurnInputInfo ddenlovrInputList[] = {
	{ "P1 Start",    BIT_DIGITAL,   Joy[0] + 0, "p1 start"  },
	{ "P1 Up",       BIT_DIGITAL,   Joy[0] + 1, "p1 up"     },
	{ "P1 Down",     BIT_DIGITAL,   Joy[0] + 2, "p1 down"   },
	{ "P1 Left",     BIT_DIGITAL,   Joy[0] + 3, "p1 left"   },
	{ "P1 Right",    BIT_DIGITAL,   Joy[0] + 4, "p1 right"  },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[0] + 5, "p1 fire 1" },
	{ "P1 Button 2", BIT_DIGITAL,   Joy[0] + 6, "p1 fire 2" },
	{ "P2 Start",    BIT_DIGITAL,   Joy[1] + 0, "p2 start"  },
	{ "P2 Up",       BIT_DIGITAL,   Joy[1] + 1, "p2 up"     },
	{ "P2 Down",     BIT_DIGITAL,   Joy[1] + 2, "p2 down"   },
	{ "P2 Left",     BIT_DIGITAL,   Joy[1] + 3, "p2 left"   },
	{ "P2 Right",    BIT_DIGITAL,   Joy[1] + 4, "p2 right"  },
	{ "P2 Button 1", BIT_DIGITAL,   Joy[1] + 5, "p2 fire 1" },
	{ "P2 Button 2", BIT_DIGITAL,   Joy[1] + 6, "p2 fire 2" },
	{ "P1 Coin",     BIT_DIGITAL,   Joy[2] + 0, "p1 coin"   },
	{ "P2 Coin",     BIT_DIGITAL,   Joy[2] + 1, "p2 coin"   },
	{ "Service",     BIT_DIGITAL,   Joy[2] + 2, "service"   },
	{ "Test",        BIT_DIGITAL,   Joy[2] + 3, "diag"      },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(ddenlovr)

static struct BurnDIPInfo ddenlovrDIPList[] = {
	{ 0x13, 0xff, 0xff, 0xff, NULL              },
	{ 0x14, 0xff, 0xff, 0xff, NULL              },
	{ 0x15, 0xff, 0xff, 0xff, NULL              },
	{ 0x16, 0xff, 0xff, 0xff, NULL              },
	{ 0x17, 0xff, 0xff, 0xff, NULL              },
	{ 0x18, 0xff, 0xff, 0xff, NULL              },
	{ 0x19, 0xff, 0xff, 0xff, NULL              },
	{ 0x1a, 0xff, 0xff, 0xff, NULL              },

	{ 0,    0xfe, 0,    2,    "Unknown 1-1"     },
	{ 0x13, 1,    0x02, 0x02, "Off"             },
	{ 0x13, 1,    0x02, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 1-2"     },
	{ 0x13, 1,    0x04, 0x04, "Off"             },
	{ 0x13, 1,    0x04, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 1-3"     },
	{ 0x13, 1,    0x08, 0x08, "Off"             },
	{ 0x13, 1,    0x08, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 1-4"     },
	{ 0x13, 1,    0x10, 0x10, "Off"             },
	{ 0x13, 1,    0x10, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Comments / Help" },
	{ 0x13, 1,    0x20, 0x00, "No"              },
	{ 0x13, 1,    0x20, 0x20, "Yes"             },

	{ 0,    0xfe, 0,    4,    "Show Girl"       },
	{ 0x13, 1,    0xc0, 0x80, "No"              },
	{ 0x13, 1,    0xc0, 0x40, "Yes, Clothed"    },
	{ 0x13, 1,    0xc0, 0xc0, "Yes, Bikini"     },
	{ 0x13, 1,    0xc0, 0x00, "Yes, Topless"    },
};

STDDIPINFO(ddenlovr)

static struct BurnInputInfo dtoyokenInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "Service",        BIT_DIGITAL,   Joy[2] + 7,          "service"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(dtoyoken)

static struct BurnDIPInfo dtoyokenDIPList[] = {
	{ 0x3e, 0xff, 0xff, 0x47, NULL                            },
	{ 0x3f, 0xff, 0xff, 0x0f, NULL                            },
	{ 0x40, 0xff, 0xff, 0xea, NULL                            },
	{ 0x41, 0xff, 0xff, 0x88, NULL                            },
	{ 0x42, 0xff, 0xff, 0xf6, NULL                            },
	{ 0x43, 0xff, 0xff, 0xff, NULL                            },
	{ 0x44, 0xff, 0xff, 0xff, NULL                            },
	{ 0x45, 0xff, 0xff, 0xff, NULL                            },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                   },
	{ 0x3e, 1,    0x0f, 0x00, "50%"                           },
	{ 0x3e, 1,    0x0f, 0x01, "53%"                           },
	{ 0x3e, 1,    0x0f, 0x02, "56%"                           },
	{ 0x3e, 1,    0x0f, 0x03, "59%"                           },
	{ 0x3e, 1,    0x0f, 0x04, "62%"                           },
	{ 0x3e, 1,    0x0f, 0x05, "65%"                           },
	{ 0x3e, 1,    0x0f, 0x06, "68%"                           },
	{ 0x3e, 1,    0x0f, 0x07, "71%"                           },
	{ 0x3e, 1,    0x0f, 0x08, "75%"                           },
	{ 0x3e, 1,    0x0f, 0x09, "78%"                           },
	{ 0x3e, 1,    0x0f, 0x0a, "81%"                           },
	{ 0x3e, 1,    0x0f, 0x0b, "84%"                           },
	{ 0x3e, 1,    0x0f, 0x0c, "87%"                           },
	{ 0x3e, 1,    0x0f, 0x0d, "90%"                           },
	{ 0x3e, 1,    0x0f, 0x0e, "93%"                           },
	{ 0x3e, 1,    0x0f, 0x0f, "96%"                           },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                     },
	{ 0x3e, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"           },
	{ 0x3e, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"            },
	{ 0x3e, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"           },
	{ 0x3e, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"          },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                   },
	{ 0x3e, 1,    0xc0, 0xc0, "1"                             },
	{ 0x3e, 1,    0xc0, 0x80, "5"                             },
	{ 0x3e, 1,    0xc0, 0x40, "10"                            },
	{ 0x3e, 1,    0xc0, 0x00, "20"                            },

	{ 0,    0xfe, 0,    4,    "Coinage"                       },
	{ 0x3f, 1,    0x03, 0x03, "1C 1C"                         },
	{ 0x3f, 1,    0x03, 0x02, "1C 2C"                         },
	{ 0x3f, 1,    0x03, 0x01, "1C 5C"                         },
	{ 0x3f, 1,    0x03, 0x00, "1C 10C"                        },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                   },
	{ 0x3f, 1,    0x0c, 0x0c, "1"                             },
	{ 0x3f, 1,    0x0c, 0x08, "2"                             },
	{ 0x3f, 1,    0x0c, 0x04, "3"                             },
	{ 0x3f, 1,    0x0c, 0x00, "5"                             },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                  },
	{ 0x3f, 1,    0x30, 0x30, "300"                           },
	{ 0x3f, 1,    0x30, 0x30, "600"                           },
	{ 0x3f, 1,    0x30, 0x30, "1500"                          },
	{ 0x3f, 1,    0x30, 0x30, "3000"                          },
	{ 0x3f, 1,    0x30, 0x20, "500"                           },
	{ 0x3f, 1,    0x30, 0x20, "1000"                          },
	{ 0x3f, 1,    0x30, 0x20, "2500"                          },
	{ 0x3f, 1,    0x30, 0x20, "5000"                          },
	{ 0x3f, 1,    0x30, 0x10, "700"                           },
	{ 0x3f, 1,    0x30, 0x10, "1400"                          },
	{ 0x3f, 1,    0x30, 0x10, "3500"                          },
	{ 0x3f, 1,    0x30, 0x10, "7000"                          },
	{ 0x3f, 1,    0x30, 0x00, "1000"                          },
	{ 0x3f, 1,    0x30, 0x00, "2000"                          },
	{ 0x3f, 1,    0x30, 0x00, "5000"                          },
	{ 0x3f, 1,    0x30, 0x00, "10000"                         },

	{ 0,    0xfe, 0,    2,    "Double Bet"                    },
	{ 0x3f, 1,    0x40, 0x40, "Off"                           },
	{ 0x3f, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Last Chance"                   },
	{ 0x3f, 1,    0x80, 0x80, "Off"                           },
	{ 0x3f, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    8,    "Yakuman Chance Cycle"          },
	{ 0x40, 1,    0x07, 0x07, "None"                          },
	{ 0x40, 1,    0x07, 0x06, "First time only"               },
	{ 0x40, 1,    0x07, 0x05, "Every 300 coins"               },
	{ 0x40, 1,    0x07, 0x04, "Every 500 coins"               },
	{ 0x40, 1,    0x07, 0x03, "Every 700 coins"               },
	{ 0x40, 1,    0x07, 0x02, "Every 1000 coins"              },
	{ 0x40, 1,    0x07, 0x01, "Every 1000 coins"              },
	{ 0x40, 1,    0x07, 0x00, "Every 1000 coins"              },

	{ 0,    0xfe, 0,    2,    "Yakuman Chances Per Cycle"     },
	{ 0x40, 1,    0x08, 0x00, "1"                             },
	{ 0x40, 1,    0x08, 0x08, "2"                             },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                    },
	{ 0x40, 1,    0x10, 0x10, "Off"                           },
	{ 0x40, 1,    0x10, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Don Den Button"                },
	{ 0x40, 1,    0x20, 0x20, "Start"                         },
	{ 0x40, 1,    0x20, 0x00, "Flip Flop"                     },

	{ 0,    0xfe, 0,    2,    "Medal Timer"                   },
	{ 0x40, 1,    0x40, 0x40, "Off"                           },
	{ 0x40, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "DIP-SW4:1"                     },
	{ 0x41, 1,    0x01, 0x01, "Off"                           },
	{ 0x41, 1,    0x01, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "In-Game Music"                 },
	{ 0x41, 1,    0x02, 0x02, "Off"                           },
	{ 0x41, 1,    0x02, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Show Renchan Gal"              },
	{ 0x41, 1,    0x04, 0x04, "No"                            },
	{ 0x41, 1,    0x04, 0x00, "Yes"                           },

	{ 0,    0xfe, 0,    2,    "Renchan Gal Display"           },
	{ 0x41, 1,    0x08, 0x08, "After Every Win"               },
	{ 0x41, 1,    0x08, 0x00, "After 3 Consecutive Wins"      },

	{ 0,    0xfe, 0,    2,    "Game Screen Gal Action"        },
	{ 0x41, 1,    0x10, 0x10, "Off"                           },
	{ 0x41, 1,    0x10, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "In-Game Voice"                 },
	{ 0x41, 1,    0x20, 0x20, "Off"                           },
	{ 0x41, 1,    0x20, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Winning Yakuman Display Voice" },
	{ 0x41, 1,    0x40, 0x40, "Off"                           },
	{ 0x41, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    8,    "Credits Per Note"              },
	{ 0x42, 1,    0x01, 0x01, "5"                             },
	{ 0x42, 1,    0x01, 0x01, "10"                            },
	{ 0x42, 1,    0x01, 0x01, "25"                            },
	{ 0x42, 1,    0x01, 0x01, "50"                            },
	{ 0x42, 1,    0x01, 0x00, "10"                            },
	{ 0x42, 1,    0x01, 0x00, "20"                            },
	{ 0x42, 1,    0x01, 0x00, "50"                            },
	{ 0x42, 1,    0x01, 0x00, "100"                           },

	{ 0,    0xfe, 0,    2,    "DIP-SW1:10"                    },
	{ 0x42, 1,    0x02, 0x02, "Off"                           },
	{ 0x42, 1,    0x02, 0x00, "On"                            },

	{ 0,    0xfe, 0,    4,    "Computer Strength"             },
	{ 0x42, 1,    0x0c, 0x00, "Weak"                          },
	{ 0x42, 1,    0x0c, 0x04, "Normal"                        },
	{ 0x42, 1,    0x0c, 0x08, "Somewhat Strong"               },
	{ 0x42, 1,    0x0c, 0x0c, "Strong"                        },

	{ 0,    0xfe, 0,    2,    "Bets?"                         },
	{ 0x44, 1,    0x40, 0x40, "0"                             },
	{ 0x44, 1,    0x40, 0x00, "1"                             },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x45, 1,    0x40, 0x40, "Off"                           },
	{ 0x45, 1,    0x40, 0x00, "On"                            },
};

STDDIPINFO(dtoyoken)

static struct BurnInputInfo funkyfigInputList[] = {
	{ "P1 Start",    BIT_DIGITAL,   Joy[0] + 0, "p1 start"  },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[0] + 1, "p1 fire 1" },
	{ "P1 Button 2", BIT_DIGITAL,   Joy[0] + 2, "p1 fire 2" },
	{ "P1 Button 3", BIT_DIGITAL,   Joy[0] + 3, "p1 fire 3" },
	{ "P1 Button 4", BIT_DIGITAL,   Joy[0] + 4, "p1 fire 4" },
	{ "P1 Button 5", BIT_DIGITAL,   Joy[0] + 5, "p1 fire 5" },
	{ "P1 Button 6", BIT_DIGITAL,   Joy[0] + 6, "p1 fire 6" },
	{ "P1 Button 7", BIT_DIGITAL,   Joy[0] + 7, "p1 fire 7" },
	{ "P2 Start",    BIT_DIGITAL,   Joy[1] + 0, "p2 start"  },
	{ "P1 Coin",     BIT_DIGITAL,   Joy[2] + 0, "p1 coin"   },
	{ "Service",     BIT_DIGITAL,   Joy[2] + 2, "service"   },
	{ "Test",        BIT_DIGITAL,   Joy[2] + 3, "diag"      },
	{ "P3 Button 8", BIT_DIGITAL,   Joy[2] + 5, "p3 fire 8" },
	{ "P3 Button 9", BIT_DIGITAL,   Joy[2] + 6, "p3 fire 9" },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(funkyfig)

static struct BurnDIPInfo funkyfigDIPList[] = {
	{ 0x0f, 0xff, 0xff, 0xf3, NULL                  },
	{ 0x10, 0xff, 0xff, 0xff, NULL                  },
	{ 0x11, 0xff, 0xff, 0xf7, NULL                  },
	{ 0x12, 0xff, 0xff, 0xff, NULL                  },
	{ 0x13, 0xff, 0xff, 0xff, NULL                  },
	{ 0x14, 0xff, 0xff, 0xff, NULL                  },
	{ 0x15, 0xff, 0xff, 0xff, NULL                  },
	{ 0x16, 0xff, 0xff, 0xff, NULL                  },

	{ 0,    0xfe, 0,    4,    "Coinage"             },
	{ 0x0f, 1,    0x03, 0x00, "3C 1C"               },
	{ 0x0f, 1,    0x03, 0x01, "2C 1C"               },
	{ 0x0f, 1,    0x03, 0x03, "1C 1C"               },
	{ 0x0f, 1,    0x03, 0x02, "1C 2C"               },

	{ 0,    0xfe, 0,    8,    "Difficulty"          },
	{ 0x0f, 1,    0x1c, 0x18, "0"                   },
	{ 0x0f, 1,    0x1c, 0x14, "1"                   },
	{ 0x0f, 1,    0x1c, 0x1c, "2"                   },
	{ 0x0f, 1,    0x1c, 0x10, "3"                   },
	{ 0x0f, 1,    0x1c, 0x0c, "4"                   },
	{ 0x0f, 1,    0x1c, 0x08, "5"                   },
	{ 0x0f, 1,    0x1c, 0x04, "5"                   },
	{ 0x0f, 1,    0x1c, 0x00, "5"                   },

	{ 0,    0xfe, 0,    2,    "Linked Cabinets"     },
	{ 0x0f, 1,    0x20, 0x20, "Off"                 },
	{ 0x0f, 1,    0x20, 0x00, "On"                  },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"         },
	{ 0x0f, 1,    0x40, 0x00, "Off"                 },
	{ 0x0f, 1,    0x40, 0x40, "On"                  },

	{ 0,    0xfe, 0,    2,    "Play Rock Smash"     },
	{ 0x0f, 1,    0x80, 0x00, "No"                  },
	{ 0x0f, 1,    0x80, 0x80, "Yes"                 },

	{ 0,    0xfe, 0,    2,    "2 Player Game"       },
	{ 0x10, 1,    0x01, 0x00, "No"                  },
	{ 0x10, 1,    0x01, 0x01, "Yes"                 },

	{ 0,    0xfe, 0,    2,    "Flip Screen"         },
	{ 0x10, 1,    0x02, 0x02, "Off"                 },
	{ 0x10, 1,    0x02, 0x00, "On"                  },

	{ 0,    0xfe, 0,    4,    "Unknown 2-2&3*"      },
	{ 0x10, 1,    0x0c, 0x0c, "10"                  },
	{ 0x10, 1,    0x0c, 0x08, "20"                  },
	{ 0x10, 1,    0x0c, 0x04, "40"                  },
	{ 0x10, 1,    0x0c, 0x00, "80"                  },

	{ 0,    0xfe, 0,    2,    "Unknown 2-4*"        },
	{ 0x10, 1,    0x10, 0x10, "Off"                 },
	{ 0x10, 1,    0x10, 0x00, "On"                  },

	{ 0,    0xfe, 0,    2,    "Unknown 2-5*"        },
	{ 0x10, 1,    0x20, 0x20, "Off"                 },
	{ 0x10, 1,    0x20, 0x00, "On"                  },

	{ 0,    0xfe, 0,    4,    "Unknown 2-6&7*"      },
	{ 0x10, 1,    0xc0, 0xc0, "0"                   },
	{ 0x10, 1,    0xc0, 0x80, "1"                   },
	{ 0x10, 1,    0xc0, 0x40, "2"                   },
	{ 0x10, 1,    0xc0, 0x00, "3"                   },

	{ 0,    0xfe, 0,    2,    "Continue?"           },
	{ 0x11, 1,    0x01, 0x01, "Off"                 },
	{ 0x11, 1,    0x01, 0x00, "On"                  },

	{ 0,    0xfe, 0,    2,    "Debug Text"          },
	{ 0x11, 1,    0x04, 0x04, "Off"                 },
	{ 0x11, 1,    0x04, 0x00, "On"                  },

	{ 0,    0xfe, 0,    2,    "Country"             },
	{ 0x11, 1,    0x08, 0x08, "Japan"               },
	{ 0x11, 1,    0x08, 0x00, "USA, Canada, Mexico" },

	{ 0,    0xfe, 0,    2,    "Unused"              },
	{ 0x11, 1,    0x10, 0x10, "Off"                 },
	{ 0x11, 1,    0x10, 0x00, "On"                  },

	{ 0,    0xfe, 0,    2,    "Unused"              },
	{ 0x11, 1,    0x20, 0x20, "Off"                 },
	{ 0x11, 1,    0x20, 0x00, "On"                  },

	{ 0,    0xfe, 0,    2,    "Unused"              },
	{ 0x11, 1,    0x40, 0x40, "Off"                 },
	{ 0x11, 1,    0x40, 0x00, "On"                  },

	{ 0,    0xfe, 0,    2,    "Unused"              },
	{ 0x11, 1,    0x80, 0x80, "Off"                 },
	{ 0x11, 1,    0x80, 0x00, "On"                  },
};

STDDIPINFO(funkyfig)

static struct BurnInputInfo hanakanzInputList[] = {
	{ "Gamble Payout", BIT_DIGITAL,   Joy[2] + 0,          "gamble payout" },
	{ "Test",          BIT_DIGITAL,   Joy[2] + 2,          "diag"          },
	{ "Gamble Book",   BIT_DIGITAL,   Joy[2] + 3,          "gamble book"   },
	{ "Memory Reset",  BIT_DIGITAL,   Joy[2] + 4,          "memory reset"  },
	{ "Gamble Keyin",  BIT_DIGITAL,   Joy[2] + 5,          "gamble keyin"  },
	{ "P1 Coin",       BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"       },
	{ "P1 A",          BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"         },
	{ "P1 E",          BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"         },
	{ "P1 M",          BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"         },
	{ "P1 Start",      BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"      },
	{ "P1 B",          BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"         },
	{ "P1 F",          BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"         },
	{ "P1 N",          BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"         },
	{ "P1 Bet",        BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"       },
	{ "P1 C",          BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"         },
	{ "P1 G",          BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"         },
	{ "P1 D",          BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"         },
	{ "P1 H",          BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"         },
	{ "P1 Score",      BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"     },
	{ "P1 Double Up",  BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up" },
	{ "P1 Flip Flop",  BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop" },
	{ "P1 Big",        BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"       },
	{ "P1 Small",      BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"     },
	{ "P2 A",          BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"         },
	{ "P2 E",          BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"         },
	{ "P2 M",          BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"         },
	{ "P2 Start",      BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"      },
	{ "P2 B",          BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"         },
	{ "P2 F",          BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"         },
	{ "P2 N",          BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"         },
	{ "P2 Bet",        BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"       },
	{ "P2 C",          BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"         },
	{ "P2 G",          BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"         },
	{ "P2 D",          BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"         },
	{ "P2 H",          BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"         },
	{ "P2 Score",      BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"     },
	{ "P2 Double Up",  BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up" },
	{ "P2 Flip Flop",  BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop" },
	{ "P2 Big",        BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"       },
	{ "P2 Small",      BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"     },
	{ "Reset",         BIT_DIGITAL,   &DrvReset,           "reset"         },
	{ "Dip A",         BIT_DIPSWITCH, Dips + 0,            "dip"           },
	{ "Dip B",         BIT_DIPSWITCH, Dips + 1,            "dip"           },
	{ "Dip C",         BIT_DIPSWITCH, Dips + 2,            "dip"           },
	{ "Dip D",         BIT_DIPSWITCH, Dips + 3,            "dip"           },
	{ "Dip E",         BIT_DIPSWITCH, Dips + 4,            "dip"           },
	{ "Dip F",         BIT_DIPSWITCH, Dips + 5,            "dip"           },
	{ "Dip G",         BIT_DIPSWITCH, Dips + 6,            "dip"           },
	{ "Dip H",         BIT_DIPSWITCH, Dips + 7,            "dip"           },
};

STDINPUTINFO(hanakanz)

static struct BurnDIPInfo hanakanzDIPList[] = {
	{ 0x29, 0xff, 0xff, 0xff, NULL                   },
	{ 0x2a, 0xff, 0xff, 0xff, NULL                   },
	{ 0x2b, 0xff, 0xff, 0xff, NULL                   },
	{ 0x2c, 0xff, 0xff, 0xff, NULL                   },
	{ 0x2d, 0xff, 0xff, 0xff, NULL                   },
	{ 0x2e, 0xff, 0xff, 0xff, NULL                   },
	{ 0x2f, 0xff, 0xff, 0xff, NULL                   },
	{ 0x30, 0xff, 0xff, 0xff, NULL                   },

	{ 0,    0xfe, 0,    8,    "Unknown 1-0&1&2"      },
	{ 0x29, 1,    0x07, 0x07, "0"                    },
	{ 0x29, 1,    0x07, 0x06, "1"                    },
	{ 0x29, 1,    0x07, 0x05, "2"                    },
	{ 0x29, 1,    0x07, 0x04, "3"                    },
	{ 0x29, 1,    0x07, 0x03, "4"                    },
	{ 0x29, 1,    0x07, 0x02, "5"                    },
	{ 0x29, 1,    0x07, 0x01, "6"                    },
	{ 0x29, 1,    0x07, 0x00, "7"                    },

	{ 0,    0xfe, 0,    2,    "Unknown 1-3"          },
	{ 0x29, 1,    0x08, 0x08, "Off"                  },
	{ 0x29, 1,    0x08, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown 1-4"          },
	{ 0x29, 1,    0x10, 0x10, "Off"                  },
	{ 0x29, 1,    0x10, 0x00, "On"                   },

	{ 0,    0xfe, 0,    4,    "Unknown 1-5&6"        },
	{ 0x29, 1,    0x60, 0x60, "0"                    },
	{ 0x29, 1,    0x60, 0x40, "1"                    },
	{ 0x29, 1,    0x60, 0x20, "2"                    },
	{ 0x29, 1,    0x60, 0x00, "3"                    },

	{ 0,    0xfe, 0,    8,    "Key-In Rate"          },
	{ 0x29, 1,    0x80, 0x00, "5"                    },
	{ 0x29, 1,    0x80, 0x00, "10"                   },
	{ 0x29, 1,    0x80, 0x00, "25"                   },
	{ 0x29, 1,    0x80, 0x00, "50"                   },
	{ 0x29, 1,    0x80, 0x80, "10"                   },
	{ 0x29, 1,    0x80, 0x80, "20"                   },
	{ 0x29, 1,    0x80, 0x80, "50"                   },
	{ 0x29, 1,    0x80, 0x80, "100"                  },

	{ 0,    0xfe, 0,    4,    "SW2:1,2"              },
	{ 0x2a, 1,    0x03, 0x03, "1C 1C"                },
	{ 0x2a, 1,    0x03, 0x02, "1C 2C"                },
	{ 0x2a, 1,    0x03, 0x01, "1C 5C"                },
	{ 0x2a, 1,    0x03, 0x00, "1C 10C"               },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"          },
	{ 0x2a, 1,    0x0c, 0x0c, "1"                    },
	{ 0x2a, 1,    0x0c, 0x08, "2"                    },
	{ 0x2a, 1,    0x0c, 0x04, "3"                    },
	{ 0x2a, 1,    0x0c, 0x00, "5"                    },

	{ 0,    0xfe, 0,    4,    "Gokou Odds"           },
	{ 0x2a, 1,    0x30, 0x30, "100"                  },
	{ 0x2a, 1,    0x30, 0x20, "200"                  },
	{ 0x2a, 1,    0x30, 0x10, "250"                  },
	{ 0x2a, 1,    0x30, 0x00, "300"                  },

	{ 0,    0xfe, 0,    4,    "Shikou Odds"          },
	{ 0x2a, 1,    0xc0, 0xc0, "50"                   },
	{ 0x2a, 1,    0xc0, 0x80, "60"                   },
	{ 0x2a, 1,    0xc0, 0x40, "70"                   },
	{ 0x2a, 1,    0xc0, 0x00, "80"                   },

	{ 0,    0xfe, 0,    4,    "Game Type"            },
	{ 0x2b, 1,    0x03, 0x03, "8 Cards"              },
	{ 0x2b, 1,    0x03, 0x02, "6 Cards (Bets)"       },
	{ 0x2b, 1,    0x03, 0x01, "6 Cards (Bets)?"      },
	{ 0x2b, 1,    0x03, 0x00, "6 Cards (Bets)??"     },

	{ 0,    0xfe, 0,    2,    "(C) Nihon (Censored)" },
	{ 0x2b, 1,    0x04, 0x04, "Off"                  },
	{ 0x2b, 1,    0x04, 0x00, "On"                   },

	{ 0,    0xfe, 0,    4,    "Unknown 3-3&4"        },
	{ 0x2b, 1,    0x18, 0x18, "10"                   },
	{ 0x2b, 1,    0x18, 0x10, "20"                   },
	{ 0x2b, 1,    0x18, 0x08, "30"                   },
	{ 0x2b, 1,    0x18, 0x00, "50"                   },

	{ 0,    0xfe, 0,    4,    "Unknown 3-5&6"        },
	{ 0x2b, 1,    0x60, 0x60, "0"                    },
	{ 0x2b, 1,    0x60, 0x40, "1"                    },
	{ 0x2b, 1,    0x60, 0x20, "2"                    },
	{ 0x2b, 1,    0x60, 0x00, "3"                    },

	{ 0,    0xfe, 0,    2,    "Girl"                 },
	{ 0x2b, 1,    0x80, 0x00, "Off"                  },
	{ 0x2b, 1,    0x80, 0x80, "On"                   },

	{ 0,    0xfe, 0,    2,    "SW4:1"                },
	{ 0x2c, 1,    0x01, 0x00, "Off"                  },
	{ 0x2c, 1,    0x01, 0x01, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown 4-1"          },
	{ 0x2c, 1,    0x02, 0x02, "Off"                  },
	{ 0x2c, 1,    0x02, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown 4-2"          },
	{ 0x2c, 1,    0x04, 0x04, "Off"                  },
	{ 0x2c, 1,    0x04, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown 4-3"          },
	{ 0x2c, 1,    0x08, 0x08, "Off"                  },
	{ 0x2c, 1,    0x08, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Hand Lesson"          },
	{ 0x2c, 1,    0x10, 0x00, "Off"                  },
	{ 0x2c, 1,    0x10, 0x10, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown 4-5"          },
	{ 0x2c, 1,    0x20, 0x20, "Off"                  },
	{ 0x2c, 1,    0x20, 0x00, "On"                   },

	{ 0,    0xfe, 0,    4,    "SW4:7,8"              },
	{ 0x2c, 1,    0xc0, 0xc0, "Hanafuda Panel"       },
	{ 0x2c, 1,    0xc0, 0x80, "Mahjong Panel"        },
	{ 0x2c, 1,    0xc0, 0x40, "Hanafuda Amusement"   },
	{ 0x2c, 1,    0xc0, 0x00, "Mahjong Amusement"    },

	{ 0,    0xfe, 0,    2,    "Maximum Bet"          },
	{ 0x2d, 1,    0x01, 0x00, "5"                    },
	{ 0x2d, 1,    0x01, 0x01, "10"                   },

	{ 0,    0xfe, 0,    2,    "Unknown 1-9"          },
	{ 0x2d, 1,    0x02, 0x02, "Off"                  },
	{ 0x2d, 1,    0x02, 0x00, "On"                   },

	{ 0,    0xfe, 0,    4,    "Ameshikou Odds"       },
	{ 0x2d, 1,    0x0c, 0x0c, "20"                   },
	{ 0x2d, 1,    0x0c, 0x08, "30"                   },
	{ 0x2d, 1,    0x0c, 0x04, "40"                   },
	{ 0x2d, 1,    0x0c, 0x00, "50"                   },

	{ 0,    0xfe, 0,    2,    "Unknown 3-8"          },
	{ 0x2d, 1,    0x10, 0x10, "Off"                  },
	{ 0x2d, 1,    0x10, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Gal Voice"            },
	{ 0x2d, 1,    0x20, 0x00, "Off"                  },
	{ 0x2d, 1,    0x20, 0x20, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown 4-8"          },
	{ 0x2d, 1,    0x40, 0x40, "Off"                  },
	{ 0x2d, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "SW4:10"               },
	{ 0x2d, 1,    0x80, 0x80, "Off"                  },
	{ 0x2d, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Auto Bet"             },
	{ 0x2f, 1,    0x40, 0x40, "Off"                  },
	{ 0x2f, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "? Hopper M."          },
	{ 0x30, 1,    0x40, 0x40, "Off"                  },
	{ 0x30, 1,    0x40, 0x00, "On"                   },
};

STDDIPINFO(hanakanz)

static struct BurnInputInfo hgingaInputList[] = {
	{ "Gamble Keyout", BIT_DIGITAL,   Joy[2] + 0,          "gamble keyout" },
	{ "Test",          BIT_DIGITAL,   Joy[2] + 2,          "diag"          },
	{ "Gamble Book",   BIT_DIGITAL,   Joy[2] + 3,          "gamble book"   },
	{ "Memory Reset",  BIT_DIGITAL,   Joy[2] + 4,          "memory reset"  },
	{ "Gamble Keyin",  BIT_DIGITAL,   Joy[2] + 5,          "gamble keyin"  },
	{ "P1 Coin",       BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"       },
	{ "P1 A",          BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"         },
	{ "P1 E",          BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"         },
	{ "P1 M",          BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"         },
	{ "P1 Start",      BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"      },
	{ "P1 B",          BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"         },
	{ "P1 F",          BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"         },
	{ "P1 N",          BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"         },
	{ "P1 Bet",        BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"       },
	{ "P1 C",          BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"         },
	{ "P1 G",          BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"         },
	{ "P1 D",          BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"         },
	{ "P1 H",          BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"         },
	{ "P1 Score",      BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"     },
	{ "P1 Double Up",  BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up" },
	{ "P1 Flip Flop",  BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop" },
	{ "P1 Big",        BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"       },
	{ "P1 Small",      BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"     },
	{ "P2 A",          BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"         },
	{ "P2 E",          BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"         },
	{ "P2 M",          BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"         },
	{ "P2 Start",      BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"      },
	{ "P2 B",          BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"         },
	{ "P2 F",          BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"         },
	{ "P2 N",          BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"         },
	{ "P2 Bet",        BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"       },
	{ "P2 C",          BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"         },
	{ "P2 G",          BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"         },
	{ "P2 D",          BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"         },
	{ "P2 H",          BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"         },
	{ "P2 Score",      BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"     },
	{ "P2 Double Up",  BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up" },
	{ "P2 Flip Flop",  BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop" },
	{ "P2 Big",        BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"       },
	{ "P2 Small",      BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"     },
	{ "Reset",         BIT_DIGITAL,   &DrvReset,           "reset"         },
	{ "Dip A",         BIT_DIPSWITCH, Dips + 0,            "dip"           },
	{ "Dip B",         BIT_DIPSWITCH, Dips + 1,            "dip"           },
	{ "Dip C",         BIT_DIPSWITCH, Dips + 2,            "dip"           },
	{ "Dip D",         BIT_DIPSWITCH, Dips + 3,            "dip"           },
	{ "Dip E",         BIT_DIPSWITCH, Dips + 4,            "dip"           },
	{ "Dip F",         BIT_DIPSWITCH, Dips + 5,            "dip"           },
	{ "Dip G",         BIT_DIPSWITCH, Dips + 6,            "dip"           },
	{ "Dip H",         BIT_DIPSWITCH, Dips + 7,            "dip"           },
};

STDINPUTINFO(hginga)

static struct BurnDIPInfo hgingaDIPList[] = {
	{ 0x29, 0xff, 0xff, 0xd4, NULL                               },
	{ 0x2a, 0xff, 0xff, 0xff, NULL                               },
	{ 0x2b, 0xff, 0xff, 0xef, NULL                               },
	{ 0x2c, 0xff, 0xff, 0xff, NULL                               },
	{ 0x2d, 0xff, 0xff, 0xef, NULL                               },
	{ 0x2e, 0xff, 0xff, 0xff, NULL                               },
	{ 0x2f, 0xff, 0xff, 0xff, NULL                               },
	{ 0x30, 0xff, 0xff, 0xff, NULL                               },

	{ 0,    0xfe, 0,    8,    "Hanafuda Game Payout Rate"        },
	{ 0x29, 1,    0x07, 0x00, "8 (Low)"                          },
	{ 0x29, 1,    0x07, 0x01, "7"                                },
	{ 0x29, 1,    0x07, 0x02, "6"                                },
	{ 0x29, 1,    0x07, 0x03, "5"                                },
	{ 0x29, 1,    0x07, 0x04, "4"                                },
	{ 0x29, 1,    0x07, 0x05, "3"                                },
	{ 0x29, 1,    0x07, 0x06, "2"                                },
	{ 0x29, 1,    0x07, 0x07, "1 (High)"                         },

	{ 0,    0xfe, 0,    2,    "Payout Variation"                 },
	{ 0x29, 1,    0x08, 0x00, "Small"                            },
	{ 0x29, 1,    0x08, 0x08, "Large"                            },

	{ 0,    0xfe, 0,    2,    "Gokou Yaku"                       },
	{ 0x29, 1,    0x10, 0x00, "Off"                              },
	{ 0x29, 1,    0x10, 0x10, "On"                               },

	{ 0,    0xfe, 0,    4,    "Double-Up Game Win Rate"          },
	{ 0x29, 1,    0x60, 0x00, "65%"                              },
	{ 0x29, 1,    0x60, 0x20, "70%"                              },
	{ 0x29, 1,    0x60, 0x40, "75%"                              },
	{ 0x29, 1,    0x60, 0x60, "80%"                              },

	{ 0,    0xfe, 0,    8,    "Key-In Rate"                      },
	{ 0x29, 1,    0x80, 0x00, "5"                                },
	{ 0x29, 1,    0x80, 0x00, "10"                               },
	{ 0x29, 1,    0x80, 0x00, "25"                               },
	{ 0x29, 1,    0x80, 0x00, "50"                               },
	{ 0x29, 1,    0x80, 0x80, "10"                               },
	{ 0x29, 1,    0x80, 0x80, "20"                               },
	{ 0x29, 1,    0x80, 0x80, "50"                               },
	{ 0x29, 1,    0x80, 0x80, "100"                              },

	{ 0,    0xfe, 0,    4,    "SW2:1,2"                          },
	{ 0x2a, 1,    0x03, 0x03, "1C 1C"                            },
	{ 0x2a, 1,    0x03, 0x02, "1C 2C"                            },
	{ 0x2a, 1,    0x03, 0x01, "1C 5C"                            },
	{ 0x2a, 1,    0x03, 0x00, "1C 10C"                           },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                      },
	{ 0x2a, 1,    0x0c, 0x0c, "1"                                },
	{ 0x2a, 1,    0x0c, 0x08, "2"                                },
	{ 0x2a, 1,    0x0c, 0x04, "3"                                },
	{ 0x2a, 1,    0x0c, 0x00, "5"                                },

	{ 0,    0xfe, 0,    4,    "Gokou Odds"                       },
	{ 0x2a, 1,    0x30, 0x30, "100"                              },
	{ 0x2a, 1,    0x30, 0x20, "200"                              },
	{ 0x2a, 1,    0x30, 0x10, "250"                              },
	{ 0x2a, 1,    0x30, 0x00, "300"                              },

	{ 0,    0xfe, 0,    4,    "Shikou Odds"                      },
	{ 0x2a, 1,    0xc0, 0xc0, "50"                               },
	{ 0x2a, 1,    0xc0, 0x80, "60"                               },
	{ 0x2a, 1,    0xc0, 0x40, "70"                               },
	{ 0x2a, 1,    0xc0, 0x00, "80"                               },

	{ 0,    0xfe, 0,    4,    "Game Mode"                        },
	{ 0x2b, 1,    0x03, 0x03, "Key-out"                          },
	{ 0x2b, 1,    0x03, 0x02, "Hopper payout"                    },
	{ 0x2b, 1,    0x03, 0x01, "Points with credit timer"         },
	{ 0x2b, 1,    0x03, 0x00, "Points with credit timer"         },

	{ 0,    0xfe, 0,    2,    "Hopper Polarity"                  },
	{ 0x2b, 1,    0x04, 0x04, "Active Low"                       },
	{ 0x2b, 1,    0x04, 0x00, "Active High"                      },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                     },
	{ 0x2b, 1,    0x18, 0x18, "1,000"                            },
	{ 0x2b, 1,    0x18, 0x18, "2,000"                            },
	{ 0x2b, 1,    0x18, 0x18, "5,000"                            },
	{ 0x2b, 1,    0x18, 0x18, "10,000"                           },
	{ 0x2b, 1,    0x18, 0x10, "2,000"                            },
	{ 0x2b, 1,    0x18, 0x10, "4,000"                            },
	{ 0x2b, 1,    0x18, 0x10, "10,000"                           },
	{ 0x2b, 1,    0x18, 0x10, "20,000"                           },
	{ 0x2b, 1,    0x18, 0x08, "3,000"                            },
	{ 0x2b, 1,    0x18, 0x08, "6,000"                            },
	{ 0x2b, 1,    0x18, 0x08, "15,000"                           },
	{ 0x2b, 1,    0x18, 0x08, "30,000"                           },
	{ 0x2b, 1,    0x18, 0x00, "5,000"                            },
	{ 0x2b, 1,    0x18, 0x00, "10,000"                           },
	{ 0x2b, 1,    0x18, 0x00, "25,000"                           },
	{ 0x2b, 1,    0x18, 0x00, "50,000"                           },

	{ 0,    0xfe, 0,    2,    "Hanaroku Hopper"                  },
	{ 0x2b, 1,    0x20, 0x20, "Manual"                           },
	{ 0x2b, 1,    0x20, 0x00, "Automatic"                        },

	{ 0,    0xfe, 0,    4,    "Gokou/Shikou Generation Interval" },
	{ 0x2b, 1,    0xc0, 0xc0, "Short"                            },
	{ 0x2b, 1,    0xc0, 0x80, "A Little Short"                   },
	{ 0x2b, 1,    0xc0, 0x40, "A Little Long"                    },
	{ 0x2b, 1,    0xc0, 0x00, "Long"                             },

	{ 0,    0xfe, 0,    2,    "SW4:1"                            },
	{ 0x2c, 1,    0x01, 0x00, "Off"                              },
	{ 0x2c, 1,    0x01, 0x01, "On"                               },

	{ 0,    0xfe, 0,    2,    "In-Game Music"                    },
	{ 0x2c, 1,    0x02, 0x00, "Off"                              },
	{ 0x2c, 1,    0x02, 0x02, "On"                               },

	{ 0,    0xfe, 0,    2,    "Show Gals"                        },
	{ 0x2c, 1,    0x04, 0x00, "Off"                              },
	{ 0x2c, 1,    0x04, 0x04, "On"                               },

	{ 0,    0xfe, 0,    2,    "Higi"                             },
	{ 0x2c, 1,    0x08, 0x00, "Off"                              },
	{ 0x2c, 1,    0x08, 0x08, "On"                               },

	{ 0,    0xfe, 0,    2,    "Hand Lesson"                      },
	{ 0x2c, 1,    0x10, 0x00, "Off"                              },
	{ 0x2c, 1,    0x10, 0x10, "On"                               },

	{ 0,    0xfe, 0,    2,    "Renchan Bonus"                    },
	{ 0x2c, 1,    0x20, 0x00, "Off"                              },
	{ 0x2c, 1,    0x20, 0x20, "On"                               },

	{ 0,    0xfe, 0,    2,    "SW4:10"                           },
	{ 0x2d, 1,    0x02, 0x02, "Off"                              },
	{ 0x2d, 1,    0x02, 0x00, "On"                               },

	{ 0,    0xfe, 0,    2,    "Show Girl in Milky Way Animation" },
	{ 0x2d, 1,    0x04, 0x00, "Off"                              },
	{ 0x2d, 1,    0x04, 0x04, "On"                               },

	{ 0,    0xfe, 0,    4,    "Ameshikou Odds"                   },
	{ 0x2d, 1,    0x30, 0x30, "20"                               },
	{ 0x2d, 1,    0x30, 0x20, "30"                               },
	{ 0x2d, 1,    0x30, 0x10, "40"                               },
	{ 0x2d, 1,    0x30, 0x00, "50"                               },

	{ 0,    0xfe, 0,    2,    "Maximum Bet"                      },
	{ 0x2d, 1,    0x40, 0x00, "5"                                },
	{ 0x2d, 1,    0x40, 0x40, "10"                               },

	{ 0,    0xfe, 0,    2,    "SW1:10"                           },
	{ 0x2d, 1,    0x80, 0x80, "Off"                              },
	{ 0x2d, 1,    0x80, 0x00, "On"                               },

	{ 0,    0xfe, 0,    4,    "SW4:8,9"                          },
	{ 0x2e, 1,    0x03, 0x03, "Hanafuda Panel"                   },
	{ 0x2e, 1,    0x03, 0x02, "Mahjong Panel"                    },
	{ 0x2e, 1,    0x03, 0x01, "Hanaroku Panel"                   },
	{ 0x2e, 1,    0x03, 0x00, "Hanaroku Panel"                   },

	{ 0,    0xfe, 0,    2,    "Auto Bet"                         },
	{ 0x2f, 1,    0x40, 0x40, "Off"                              },
	{ 0x2f, 1,    0x40, 0x00, "On"                               },
};

STDDIPINFO(hginga)

static struct BurnInputInfo hgokouInputList[] = {
	{ "Gamble Keyout", BIT_DIGITAL,   Joy[2] + 0,          "gamble keyout" },
	{ "Test",          BIT_DIGITAL,   Joy[2] + 2,          "diag"          },
	{ "Gamble Book",   BIT_DIGITAL,   Joy[2] + 3,          "gamble book"   },
	{ "Memory Reset",  BIT_DIGITAL,   Joy[2] + 4,          "memory reset"  },
	{ "Gamble Keyin",  BIT_DIGITAL,   Joy[2] + 5,          "gamble keyin"  },
	{ "P1 Coin",       BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"       },
	{ "P1 A",          BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"         },
	{ "P1 E",          BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"         },
	{ "P1 M",          BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"         },
	{ "P1 Start",      BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"      },
	{ "P1 B",          BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"         },
	{ "P1 F",          BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"         },
	{ "P1 N",          BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"         },
	{ "P1 Bet",        BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"       },
	{ "P1 C",          BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"         },
	{ "P1 G",          BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"         },
	{ "P1 D",          BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"         },
	{ "P1 H",          BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"         },
	{ "P1 Score",      BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"     },
	{ "P1 Double Up",  BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up" },
	{ "P1 Flip Flop",  BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop" },
	{ "P1 Big",        BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"       },
	{ "P1 Small",      BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"     },
	{ "P2 A",          BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"         },
	{ "P2 E",          BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"         },
	{ "P2 M",          BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"         },
	{ "P2 Start",      BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"      },
	{ "P2 B",          BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"         },
	{ "P2 F",          BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"         },
	{ "P2 N",          BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"         },
	{ "P2 Bet",        BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"       },
	{ "P2 C",          BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"         },
	{ "P2 G",          BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"         },
	{ "P2 D",          BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"         },
	{ "P2 H",          BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"         },
	{ "P2 Score",      BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"     },
	{ "P2 Double Up",  BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up" },
	{ "P2 Flip Flop",  BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop" },
	{ "P2 Big",        BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"       },
	{ "P2 Small",      BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"     },
	{ "Reset",         BIT_DIGITAL,   &DrvReset,           "reset"         },
	{ "Dip A",         BIT_DIPSWITCH, Dips + 0,            "dip"           },
	{ "Dip B",         BIT_DIPSWITCH, Dips + 1,            "dip"           },
	{ "Dip C",         BIT_DIPSWITCH, Dips + 2,            "dip"           },
	{ "Dip D",         BIT_DIPSWITCH, Dips + 3,            "dip"           },
	{ "Dip E",         BIT_DIPSWITCH, Dips + 4,            "dip"           },
	{ "Dip F",         BIT_DIPSWITCH, Dips + 5,            "dip"           },
	{ "Dip G",         BIT_DIPSWITCH, Dips + 6,            "dip"           },
	{ "Dip H",         BIT_DIPSWITCH, Dips + 7,            "dip"           },
};

STDINPUTINFO(hgokou)

static struct BurnDIPInfo hgokouDIPList[] = {
	{ 0x29, 0xff, 0xff, 0xd4, NULL                                },
	{ 0x2a, 0xff, 0xff, 0xff, NULL                                },
	{ 0x2b, 0xff, 0xff, 0xef, NULL                                },
	{ 0x2c, 0xff, 0xff, 0xff, NULL                                },
	{ 0x2d, 0xff, 0xff, 0xef, NULL                                },
	{ 0x2e, 0xff, 0xff, 0xff, NULL                                },
	{ 0x2f, 0xff, 0xff, 0xff, NULL                                },
	{ 0x30, 0xff, 0xff, 0xff, NULL                                },

	{ 0,    0xfe, 0,    8,    "Hanafuda Game Payout Rate"         },
	{ 0x29, 1,    0x07, 0x00, "8 (low)"                           },
	{ 0x29, 1,    0x07, 0x01, "7"                                 },
	{ 0x29, 1,    0x07, 0x02, "6"                                 },
	{ 0x29, 1,    0x07, 0x03, "5"                                 },
	{ 0x29, 1,    0x07, 0x04, "4"                                 },
	{ 0x29, 1,    0x07, 0x05, "3"                                 },
	{ 0x29, 1,    0x07, 0x06, "2"                                 },
	{ 0x29, 1,    0x07, 0x07, "1 (high)"                          },

	{ 0,    0xfe, 0,    2,    "Payout Variation"                  },
	{ 0x29, 1,    0x08, 0x00, "Small"                             },
	{ 0x29, 1,    0x08, 0x08, "Large"                             },

	{ 0,    0xfe, 0,    2,    "Gokou Yaku"                        },
	{ 0x29, 1,    0x10, 0x00, "Off"                               },
	{ 0x29, 1,    0x10, 0x10, "On"                                },

	{ 0,    0xfe, 0,    4,    "Double Up Game Win Rate"           },
	{ 0x29, 1,    0x60, 0x00, "65%"                               },
	{ 0x29, 1,    0x60, 0x20, "70%"                               },
	{ 0x29, 1,    0x60, 0x40, "75%"                               },
	{ 0x29, 1,    0x60, 0x60, "80%"                               },

	{ 0,    0xfe, 0,    8,    "Key-In Rate"                       },
	{ 0x29, 1,    0x80, 0x00, "5"                                 },
	{ 0x29, 1,    0x80, 0x00, "10"                                },
	{ 0x29, 1,    0x80, 0x00, "25"                                },
	{ 0x29, 1,    0x80, 0x00, "50"                                },
	{ 0x29, 1,    0x80, 0x80, "10"                                },
	{ 0x29, 1,    0x80, 0x80, "20"                                },
	{ 0x29, 1,    0x80, 0x80, "50"                                },
	{ 0x29, 1,    0x80, 0x80, "100"                               },

	{ 0,    0xfe, 0,    4,    "SW2:1,2"                           },
	{ 0x2a, 1,    0x03, 0x03, "1C 1C"                             },
	{ 0x2a, 1,    0x03, 0x02, "1C 2C"                             },
	{ 0x2a, 1,    0x03, 0x01, "1C 5C"                             },
	{ 0x2a, 1,    0x03, 0x00, "1C 10C"                            },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                       },
	{ 0x2a, 1,    0x0c, 0x0c, "1"                                 },
	{ 0x2a, 1,    0x0c, 0x08, "2"                                 },
	{ 0x2a, 1,    0x0c, 0x04, "3"                                 },
	{ 0x2a, 1,    0x0c, 0x00, "5"                                 },

	{ 0,    0xfe, 0,    4,    "Gokou Odds"                        },
	{ 0x2a, 1,    0x30, 0x30, "100"                               },
	{ 0x2a, 1,    0x30, 0x20, "200"                               },
	{ 0x2a, 1,    0x30, 0x10, "250"                               },
	{ 0x2a, 1,    0x30, 0x00, "300"                               },

	{ 0,    0xfe, 0,    4,    "Shikou Odds"                       },
	{ 0x2a, 1,    0xc0, 0xc0, "50"                                },
	{ 0x2a, 1,    0xc0, 0x80, "60"                                },
	{ 0x2a, 1,    0xc0, 0x40, "70"                                },
	{ 0x2a, 1,    0xc0, 0x00, "80"                                },

	{ 0,    0xfe, 0,    3,    "Game Type"                         },
	{ 0x2b, 1,    0x03, 0x03, "Credit Type"                       },
	{ 0x2b, 1,    0x03, 0x02, "Hopper Type"                       },
	{ 0x2b, 1,    0x03, 0x01, "Credit Timer Type"                 },

	{ 0,    0xfe, 0,    2,    "Hopper Polarity"                   },
	{ 0x2b, 1,    0x04, 0x04, "Active Low"                        },
	{ 0x2b, 1,    0x04, 0x00, "Active High"                       },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                      },
	{ 0x2b, 1,    0x18, 0x18, "1,000"                             },
	{ 0x2b, 1,    0x18, 0x18, "2,000"                             },
	{ 0x2b, 1,    0x18, 0x18, "5,000"                             },
	{ 0x2b, 1,    0x18, 0x18, "10,000"                            },
	{ 0x2b, 1,    0x18, 0x10, "2,000"                             },
	{ 0x2b, 1,    0x18, 0x10, "4,000"                             },
	{ 0x2b, 1,    0x18, 0x10, "10,000"                            },
	{ 0x2b, 1,    0x18, 0x10, "20,000"                            },
	{ 0x2b, 1,    0x18, 0x08, "3,000"                             },
	{ 0x2b, 1,    0x18, 0x08, "6,000"                             },
	{ 0x2b, 1,    0x18, 0x08, "15,000"                            },
	{ 0x2b, 1,    0x18, 0x08, "30,000"                            },
	{ 0x2b, 1,    0x18, 0x00, "5,000"                             },
	{ 0x2b, 1,    0x18, 0x00, "10,000"                            },
	{ 0x2b, 1,    0x18, 0x00, "25,000"                            },
	{ 0x2b, 1,    0x18, 0x00, "50,000"                            },

	{ 0,    0xfe, 0,    2,    "Hanaroku Hopper Type"              },
	{ 0x2b, 1,    0x20, 0x20, "Manual"                            },
	{ 0x2b, 1,    0x20, 0x00, "Automatic"                         },

	{ 0,    0xfe, 0,    4,    "Gokou/Shikou Generation Limit"     },
	{ 0x2b, 1,    0xc0, 0xc0, "Setting 1"                         },
	{ 0x2b, 1,    0xc0, 0x80, "Setting 2"                         },
	{ 0x2b, 1,    0xc0, 0x40, "Setting 3"                         },
	{ 0x2b, 1,    0xc0, 0x00, "Setting 4"                         },

	{ 0,    0xfe, 0,    2,    "SW4:1"                             },
	{ 0x2c, 1,    0x01, 0x00, "Off"                               },
	{ 0x2c, 1,    0x01, 0x01, "On"                                },

	{ 0,    0xfe, 0,    2,    "In-Game Music"                     },
	{ 0x2c, 1,    0x02, 0x00, "Off"                               },
	{ 0x2c, 1,    0x02, 0x02, "On"                                },

	{ 0,    0xfe, 0,    2,    "Show Renso Gal"                    },
	{ 0x2c, 1,    0x04, 0x00, "Off"                               },
	{ 0x2c, 1,    0x04, 0x04, "On"                                },

	{ 0,    0xfe, 0,    2,    "Higi"                              },
	{ 0x2c, 1,    0x08, 0x00, "Off"                               },
	{ 0x2c, 1,    0x08, 0x08, "On"                                },

	{ 0,    0xfe, 0,    2,    "Hand Lesson"                       },
	{ 0x2c, 1,    0x10, 0x00, "Off"                               },
	{ 0x2c, 1,    0x10, 0x10, "On"                                },

	{ 0,    0xfe, 0,    2,    "Renso Bonus"                       },
	{ 0x2c, 1,    0x20, 0x00, "Off"                               },
	{ 0x2c, 1,    0x20, 0x20, "On"                                },

	{ 0,    0xfe, 0,    2,    "SW4:10"                            },
	{ 0x2d, 1,    0x02, 0x02, "Off"                               },
	{ 0x2d, 1,    0x02, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Show Double-Up Girl at Game Start" },
	{ 0x2d, 1,    0x04, 0x00, "Off"                               },
	{ 0x2d, 1,    0x04, 0x04, "On"                                },

	{ 0,    0xfe, 0,    2,    "In-Game Voice"                     },
	{ 0x2d, 1,    0x08, 0x00, "Off"                               },
	{ 0x2d, 1,    0x08, 0x08, "On"                                },

	{ 0,    0xfe, 0,    4,    "Ameshikou Odds"                    },
	{ 0x2d, 1,    0x30, 0x30, "20"                                },
	{ 0x2d, 1,    0x30, 0x20, "30"                                },
	{ 0x2d, 1,    0x30, 0x10, "40"                                },
	{ 0x2d, 1,    0x30, 0x00, "50"                                },

	{ 0,    0xfe, 0,    3,    "Maximum Bet"                       },
	{ 0x2d, 1,    0x40, 0x40, "1"                                 },
	{ 0x2d, 1,    0x40, 0x00, "5"                                 },
	{ 0x2d, 1,    0x40, 0x40, "10"                                },

	{ 0,    0xfe, 0,    2,    "SW1:10"                            },
	{ 0x2d, 1,    0x80, 0x80, "Off"                               },
	{ 0x2d, 1,    0x80, 0x00, "On"                                },

	{ 0,    0xfe, 0,    6,    "SW4:7,8,9"                         },
	{ 0x2e, 1,    0x07, 0x07, "Hanafuda Panel"                    },
	{ 0x2e, 1,    0x07, 0x06, "Mahjong Panel"                     },
	{ 0x2e, 1,    0x07, 0x05, "Hanaroku Panel"                    },
	{ 0x2e, 1,    0x07, 0x04, "Mahjong Amusement"                 },
	{ 0x2e, 1,    0x07, 0x03, "Joystick"                          },
	{ 0x2e, 1,    0x07, 0x02, "Hanafuda Amusement"                },
};

STDDIPINFO(hgokou)

static struct BurnInputInfo hkagerouInputList[] = {
	{ "Gamble Payout", BIT_DIGITAL,   Joy[2] + 0,          "gamble payout" },
	{ "Test",          BIT_DIGITAL,   Joy[2] + 2,          "diag"          },
	{ "Gamble Book",   BIT_DIGITAL,   Joy[2] + 3,          "gamble book"   },
	{ "Memory Reset",  BIT_DIGITAL,   Joy[2] + 4,          "memory reset"  },
	{ "Gamble Keyin",  BIT_DIGITAL,   Joy[2] + 5,          "gamble keyin"  },
	{ "P1 Coin",       BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"       },
	{ "P1 A",          BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"         },
	{ "P1 E",          BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"         },
	{ "P1 M",          BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"         },
	{ "P1 Start",      BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"      },
	{ "P1 B",          BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"         },
	{ "P1 F",          BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"         },
	{ "P1 N",          BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"         },
	{ "P1 Bet",        BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"       },
	{ "P1 C",          BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"         },
	{ "P1 G",          BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"         },
	{ "P1 D",          BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"         },
	{ "P1 H",          BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"         },
	{ "P1 Score",      BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"     },
	{ "P1 Double Up",  BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up" },
	{ "P1 Flip Flop",  BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop" },
	{ "P1 Big",        BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"       },
	{ "P1 Small",      BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"     },
	{ "P2 A",          BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"         },
	{ "P2 E",          BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"         },
	{ "P2 M",          BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"         },
	{ "P2 Start",      BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"      },
	{ "P2 B",          BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"         },
	{ "P2 F",          BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"         },
	{ "P2 N",          BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"         },
	{ "P2 Bet",        BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"       },
	{ "P2 C",          BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"         },
	{ "P2 G",          BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"         },
	{ "P2 D",          BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"         },
	{ "P2 H",          BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"         },
	{ "P2 Score",      BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"     },
	{ "P2 Double Up",  BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up" },
	{ "P2 Flip Flop",  BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop" },
	{ "P2 Big",        BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"       },
	{ "P2 Small",      BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"     },
	{ "Reset",         BIT_DIGITAL,   &DrvReset,           "reset"         },
	{ "Dip A",         BIT_DIPSWITCH, Dips + 0,            "dip"           },
	{ "Dip B",         BIT_DIPSWITCH, Dips + 1,            "dip"           },
	{ "Dip C",         BIT_DIPSWITCH, Dips + 2,            "dip"           },
	{ "Dip D",         BIT_DIPSWITCH, Dips + 3,            "dip"           },
	{ "Dip E",         BIT_DIPSWITCH, Dips + 4,            "dip"           },
	{ "Dip F",         BIT_DIPSWITCH, Dips + 5,            "dip"           },
	{ "Dip G",         BIT_DIPSWITCH, Dips + 6,            "dip"           },
	{ "Dip H",         BIT_DIPSWITCH, Dips + 7,            "dip"           },
};

STDINPUTINFO(hkagerou)

static struct BurnDIPInfo hkagerouDIPList[] = {
	{ 0x29, 0xff, 0xff, 0xd4, NULL                            },
	{ 0x2a, 0xff, 0xff, 0xff, NULL                            },
	{ 0x2b, 0xff, 0xff, 0xef, NULL                            },
	{ 0x2c, 0xff, 0xff, 0xff, NULL                            },
	{ 0x2d, 0xff, 0xff, 0xfb, NULL                            },
	{ 0x2e, 0xff, 0xff, 0xff, NULL                            },
	{ 0x2f, 0xff, 0xff, 0xff, NULL                            },
	{ 0x30, 0xff, 0xff, 0xff, NULL                            },

	{ 0,    0xfe, 0,    8,    "Hanafuda Game Payout Rate"     },
	{ 0x29, 1,    0x07, 0x00, "8 (Low)"                       },
	{ 0x29, 1,    0x07, 0x01, "7"                             },
	{ 0x29, 1,    0x07, 0x02, "6"                             },
	{ 0x29, 1,    0x07, 0x03, "5"                             },
	{ 0x29, 1,    0x07, 0x04, "4"                             },
	{ 0x29, 1,    0x07, 0x05, "3"                             },
	{ 0x29, 1,    0x07, 0x06, "2"                             },
	{ 0x29, 1,    0x07, 0x07, "1 (High)"                      },

	{ 0,    0xfe, 0,    2,    "Payout Variation"              },
	{ 0x29, 1,    0x08, 0x00, "Small"                         },
	{ 0x29, 1,    0x08, 0x08, "Large"                         },

	{ 0,    0xfe, 0,    2,    "Gokou Yaku"                    },
	{ 0x29, 1,    0x10, 0x00, "Off"                           },
	{ 0x29, 1,    0x10, 0x10, "On"                            },

	{ 0,    0xfe, 0,    4,    "Double-Up Game Win Rate"       },
	{ 0x29, 1,    0x60, 0x00, "65%"                           },
	{ 0x29, 1,    0x60, 0x20, "70%"                           },
	{ 0x29, 1,    0x60, 0x40, "75%"                           },
	{ 0x29, 1,    0x60, 0x60, "80%"                           },

	{ 0,    0xfe, 0,    8,    "Key-In Rate"                   },
	{ 0x29, 1,    0x80, 0x00, "5"                             },
	{ 0x29, 1,    0x80, 0x00, "10"                            },
	{ 0x29, 1,    0x80, 0x00, "25"                            },
	{ 0x29, 1,    0x80, 0x00, "50"                            },
	{ 0x29, 1,    0x80, 0x80, "10"                            },
	{ 0x29, 1,    0x80, 0x80, "20"                            },
	{ 0x29, 1,    0x80, 0x80, "50"                            },
	{ 0x29, 1,    0x80, 0x80, "100"                           },

	{ 0,    0xfe, 0,    4,    "SW2:1,2"                       },
	{ 0x2a, 1,    0x03, 0x03, "1C 1C"                         },
	{ 0x2a, 1,    0x03, 0x02, "1C 2C"                         },
	{ 0x2a, 1,    0x03, 0x01, "1C 5C"                         },
	{ 0x2a, 1,    0x03, 0x00, "1C 10C"                        },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                   },
	{ 0x2a, 1,    0x0c, 0x0c, "1"                             },
	{ 0x2a, 1,    0x0c, 0x08, "2"                             },
	{ 0x2a, 1,    0x0c, 0x04, "3"                             },
	{ 0x2a, 1,    0x0c, 0x00, "5"                             },

	{ 0,    0xfe, 0,    4,    "Gokou Odds"                    },
	{ 0x2a, 1,    0x30, 0x30, "100"                           },
	{ 0x2a, 1,    0x30, 0x20, "200"                           },
	{ 0x2a, 1,    0x30, 0x10, "250"                           },
	{ 0x2a, 1,    0x30, 0x00, "300"                           },

	{ 0,    0xfe, 0,    4,    "Shikou Odds"                   },
	{ 0x2a, 1,    0xc0, 0xc0, "50"                            },
	{ 0x2a, 1,    0xc0, 0x80, "60"                            },
	{ 0x2a, 1,    0xc0, 0x40, "70"                            },
	{ 0x2a, 1,    0xc0, 0x00, "80"                            },

	{ 0,    0xfe, 0,    2,    "Game Mode"                     },
	{ 0x2b, 1,    0x01, 0x01, "Medal Type"                    },
	{ 0x2b, 1,    0x01, 0x00, "Medal Timer"                   },

	{ 0,    0xfe, 0,    2,    "Distributor"                   },
	{ 0x2b, 1,    0x04, 0x04, "Dynax Soft (more explicit)"    },
	{ 0x2b, 1,    0x04, 0x00, "Nihon System (less explicit)"  },

	{ 0,    0xfe, 0,    4,    "Credit Limit"                  },
	{ 0x2b, 1,    0x18, 0x18, "1000"                          },
	{ 0x2b, 1,    0x18, 0x10, "2000"                          },
	{ 0x2b, 1,    0x18, 0x08, "3000"                          },
	{ 0x2b, 1,    0x18, 0x00, "5000"                          },

	{ 0,    0xfe, 0,    4,    "Gokou/Shikou Generation Limit" },
	{ 0x2b, 1,    0x60, 0x60, "Setting 1 (Normal)"            },
	{ 0x2b, 1,    0x60, 0x40, "Setting 2"                     },
	{ 0x2b, 1,    0x60, 0x20, "Setting 3"                     },
	{ 0x2b, 1,    0x60, 0x00, "Setting 4"                     },

	{ 0,    0xfe, 0,    2,    "Koi-Koi Gal Action"            },
	{ 0x2b, 1,    0x80, 0x00, "Off"                           },
	{ 0x2b, 1,    0x80, 0x80, "On"                            },

	{ 0,    0xfe, 0,    2,    "SW4:1"                         },
	{ 0x2c, 1,    0x01, 0x00, "Off"                           },
	{ 0x2c, 1,    0x01, 0x01, "On"                            },

	{ 0,    0xfe, 0,    2,    "In-Game Music"                 },
	{ 0x2c, 1,    0x02, 0x00, "Off"                           },
	{ 0x2c, 1,    0x02, 0x02, "On"                            },

	{ 0,    0xfe, 0,    2,    "Show Renso Gal"                },
	{ 0x2c, 1,    0x04, 0x00, "Off"                           },
	{ 0x2c, 1,    0x04, 0x04, "On"                            },

	{ 0,    0xfe, 0,    2,    "Higi"                          },
	{ 0x2c, 1,    0x08, 0x00, "Off"                           },
	{ 0x2c, 1,    0x08, 0x08, "On"                            },

	{ 0,    0xfe, 0,    2,    "Hand Lesson"                   },
	{ 0x2c, 1,    0x10, 0x00, "Off"                           },
	{ 0x2c, 1,    0x10, 0x10, "On"                            },

	{ 0,    0xfe, 0,    2,    "Renso Bonus"                   },
	{ 0x2c, 1,    0x20, 0x00, "Off"                           },
	{ 0x2c, 1,    0x20, 0x20, "On"                            },

	{ 0,    0xfe, 0,    4,    "SW4:7,8"                       },
	{ 0x2c, 1,    0xc0, 0xc0, "Hanafuda Panel"                },
	{ 0x2c, 1,    0xc0, 0x80, "Mahjong Panel"                 },
	{ 0x2c, 1,    0xc0, 0x40, "Hanafuda Amusement"            },
	{ 0x2c, 1,    0xc0, 0x00, "Mahjong Amusement"             },

	{ 0,    0xfe, 0,    2,    "Maximum Bet"                   },
	{ 0x2d, 1,    0x01, 0x00, "5"                             },
	{ 0x2d, 1,    0x01, 0x01, "10"                            },

	{ 0,    0xfe, 0,    2,    "SW1:10"                        },
	{ 0x2d, 1,    0x02, 0x02, "Off"                           },
	{ 0x2d, 1,    0x02, 0x00, "On"                            },

	{ 0,    0xfe, 0,    4,    "Ameshikou Odds"                },
	{ 0x2d, 1,    0x0c, 0x0c, "20"                            },
	{ 0x2d, 1,    0x0c, 0x08, "30"                            },
	{ 0x2d, 1,    0x0c, 0x04, "40"                            },
	{ 0x2d, 1,    0x0c, 0x00, "50"                            },

	{ 0,    0xfe, 0,    2,    "Gal Voice"                     },
	{ 0x2d, 1,    0x20, 0x00, "Off"                           },
	{ 0x2d, 1,    0x20, 0x20, "On"                            },

	{ 0,    0xfe, 0,    2,    "SW4:10"                        },
	{ 0x2d, 1,    0x80, 0x80, "Off"                           },
	{ 0x2d, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Auto Bet"                      },
	{ 0x2f, 1,    0x40, 0x40, "Off"                           },
	{ 0x2f, 1,    0x40, 0x00, "On"                            },
};

STDDIPINFO(hkagerou)

static struct BurnInputInfo hnroseInputList[] = {
	{ "Gamble Payout", BIT_DIGITAL,   Joy[2] + 0,          "gamble payout" },
	{ "Test",          BIT_DIGITAL,   Joy[2] + 2,          "diag"          },
	{ "Gamble Book",   BIT_DIGITAL,   Joy[2] + 3,          "gamble book"   },
	{ "Memory Reset",  BIT_DIGITAL,   Joy[2] + 4,          "memory reset"  },
	{ "Bill1",         BIT_DIGITAL,   Joy[2] + 5,          "bill1"         },
	{ "P1 Coin",       BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"       },
	{ "P1 A",          BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"         },
	{ "P1 E",          BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"         },
	{ "P1 M",          BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"         },
	{ "P1 Start",      BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"      },
	{ "P1 B",          BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"         },
	{ "P1 F",          BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"         },
	{ "P1 N",          BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"         },
	{ "P1 Bet",        BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"       },
	{ "P1 C",          BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"         },
	{ "P1 G",          BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"         },
	{ "P1 D",          BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"         },
	{ "P1 H",          BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"         },
	{ "P1 Score",      BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"     },
	{ "P1 Double Up",  BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up" },
	{ "P1 Flip Flop",  BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop" },
	{ "P1 Big",        BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"       },
	{ "P1 Small",      BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"     },
	{ "P2 A",          BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"         },
	{ "P2 E",          BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"         },
	{ "P2 M",          BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"         },
	{ "P2 Start",      BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"      },
	{ "P2 B",          BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"         },
	{ "P2 F",          BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"         },
	{ "P2 N",          BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"         },
	{ "P2 Bet",        BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"       },
	{ "P2 C",          BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"         },
	{ "P2 G",          BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"         },
	{ "P2 D",          BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"         },
	{ "P2 H",          BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"         },
	{ "P2 Score",      BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"     },
	{ "P2 Double Up",  BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up" },
	{ "P2 Flip Flop",  BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop" },
	{ "P2 Big",        BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"       },
	{ "P2 Small",      BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"     },
	{ "Reset",         BIT_DIGITAL,   &DrvReset,           "reset"         },
	{ "Dip A",         BIT_DIPSWITCH, Dips + 0,            "dip"           },
	{ "Dip B",         BIT_DIPSWITCH, Dips + 1,            "dip"           },
	{ "Dip C",         BIT_DIPSWITCH, Dips + 2,            "dip"           },
	{ "Dip D",         BIT_DIPSWITCH, Dips + 3,            "dip"           },
	{ "Dip E",         BIT_DIPSWITCH, Dips + 4,            "dip"           },
	{ "Dip F",         BIT_DIPSWITCH, Dips + 5,            "dip"           },
	{ "Dip G",         BIT_DIPSWITCH, Dips + 6,            "dip"           },
	{ "Dip H",         BIT_DIPSWITCH, Dips + 7,            "dip"           },
};

STDINPUTINFO(hnrose)

static struct BurnDIPInfo hnroseDIPList[] = {
	{ 0x29, 0xff, 0xff, 0xd4, NULL                               },
	{ 0x2a, 0xff, 0xff, 0x7f, NULL                               },
	{ 0x2b, 0xff, 0xff, 0xff, NULL                               },
	{ 0x2c, 0xff, 0xff, 0xff, NULL                               },
	{ 0x2d, 0xff, 0xff, 0xff, NULL                               },
	{ 0x2e, 0xff, 0xff, 0xff, NULL                               },
	{ 0x2f, 0xff, 0xff, 0xff, NULL                               },
	{ 0x30, 0xff, 0xff, 0xff, NULL                               },

	{ 0,    0xfe, 0,    8,    "Hanafuda Game Payout Rate"        },
	{ 0x29, 1,    0x07, 0x00, "8 (Low)"                          },
	{ 0x29, 1,    0x07, 0x01, "7"                                },
	{ 0x29, 1,    0x07, 0x02, "6"                                },
	{ 0x29, 1,    0x07, 0x03, "5"                                },
	{ 0x29, 1,    0x07, 0x04, "4"                                },
	{ 0x29, 1,    0x07, 0x05, "3"                                },
	{ 0x29, 1,    0x07, 0x06, "2"                                },
	{ 0x29, 1,    0x07, 0x07, "1 (High)"                         },

	{ 0,    0xfe, 0,    2,    "Payout Variation"                 },
	{ 0x29, 1,    0x08, 0x00, "Small"                            },
	{ 0x29, 1,    0x08, 0x08, "Large"                            },

	{ 0,    0xfe, 0,    2,    "Gokou Yaku"                       },
	{ 0x29, 1,    0x10, 0x00, "Off"                              },
	{ 0x29, 1,    0x10, 0x10, "On"                               },

	{ 0,    0xfe, 0,    4,    "Double-Up Game Win Rate"          },
	{ 0x29, 1,    0x60, 0x00, "55%"                              },
	{ 0x29, 1,    0x60, 0x20, "60%"                              },
	{ 0x29, 1,    0x60, 0x40, "65%"                              },
	{ 0x29, 1,    0x60, 0x60, "70%"                              },

	{ 0,    0xfe, 0,    2,    "Maximum Bet"                      },
	{ 0x29, 1,    0x80, 0x00, "5"                                },
	{ 0x29, 1,    0x80, 0x80, "10"                               },

	{ 0,    0xfe, 0,    4,    "Coinage"                          },
	{ 0x2a, 1,    0x03, 0x03, "1C 1C"                            },
	{ 0x2a, 1,    0x03, 0x02, "1C 2C"                            },
	{ 0x2a, 1,    0x03, 0x01, "1C 5C"                            },
	{ 0x2a, 1,    0x03, 0x00, "1C 10C"                           },

	{ 0,    0xfe, 0,    8,    "Minimum Bet"                      },
	{ 0x2a, 1,    0x38, 0x38, "1"                                },
	{ 0x2a, 1,    0x38, 0x30, "2"                                },
	{ 0x2a, 1,    0x38, 0x28, "3"                                },
	{ 0x2a, 1,    0x38, 0x20, "4"                                },
	{ 0x2a, 1,    0x38, 0x18, "5"                                },
	{ 0x2a, 1,    0x38, 0x10, "6"                                },
	{ 0x2a, 1,    0x38, 0x08, "7"                                },
	{ 0x2a, 1,    0x38, 0x00, "8"                                },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                     },
	{ 0x2a, 1,    0xc0, 0xc0, "1000"                             },
	{ 0x2a, 1,    0xc0, 0xc0, "2000"                             },
	{ 0x2a, 1,    0xc0, 0xc0, "5000"                             },
	{ 0x2a, 1,    0xc0, 0xc0, "10000"                            },
	{ 0x2a, 1,    0xc0, 0x80, "2000"                             },
	{ 0x2a, 1,    0xc0, 0x80, "4000"                             },
	{ 0x2a, 1,    0xc0, 0x80, "10000"                            },
	{ 0x2a, 1,    0xc0, 0x80, "20000"                            },
	{ 0x2a, 1,    0xc0, 0x40, "3000"                             },
	{ 0x2a, 1,    0xc0, 0x40, "6000"                             },
	{ 0x2a, 1,    0xc0, 0x40, "15000"                            },
	{ 0x2a, 1,    0xc0, 0x40, "30000"                            },
	{ 0x2a, 1,    0xc0, 0x00, "5000"                             },
	{ 0x2a, 1,    0xc0, 0x00, "10000"                            },
	{ 0x2a, 1,    0xc0, 0x00, "25000"                            },
	{ 0x2a, 1,    0xc0, 0x00, "50000"                            },

	{ 0,    0xfe, 0,    2,    "Gokou Odds"                       },
	{ 0x2b, 1,    0x20, 0x20, "100"                              },
	{ 0x2b, 1,    0x20, 0x00, "200"                              },

	{ 0,    0xfe, 0,    2,    "Shikou Odds"                      },
	{ 0x2b, 1,    0x40, 0x40, "50"                               },
	{ 0x2b, 1,    0x40, 0x00, "60"                               },

	{ 0,    0xfe, 0,    2,    "Ameshikou Odds"                   },
	{ 0x2b, 1,    0x80, 0x80, "20"                               },
	{ 0x2b, 1,    0x80, 0x00, "30"                               },

	{ 0,    0xfe, 0,    2,    "DIP-SW4:1"                        },
	{ 0x2c, 1,    0x01, 0x00, "Off"                              },
	{ 0x2c, 1,    0x01, 0x01, "On"                               },

	{ 0,    0xfe, 0,    2,    "In-Game Music"                    },
	{ 0x2c, 1,    0x02, 0x00, "Off"                              },
	{ 0x2c, 1,    0x02, 0x02, "On"                               },

	{ 0,    0xfe, 0,    2,    "Gal Voice"                        },
	{ 0x2c, 1,    0x04, 0x00, "Off"                              },
	{ 0x2c, 1,    0x04, 0x04, "On"                               },

	{ 0,    0xfe, 0,    2,    "Show Winning Streak Gal"          },
	{ 0x2c, 1,    0x08, 0x00, "Off"                              },
	{ 0x2c, 1,    0x08, 0x08, "On"                               },

	{ 0,    0xfe, 0,    2,    "Odds Type"                        },
	{ 0x2c, 1,    0x10, 0x10, "Punch Board"                      },
	{ 0x2c, 1,    0x10, 0x00, "Slots"                            },

	{ 0,    0xfe, 0,    2,    "Winning Streak Bonus"             },
	{ 0x2c, 1,    0x20, 0x00, "Off"                              },
	{ 0x2c, 1,    0x20, 0x20, "On"                               },

	{ 0,    0xfe, 0,    4,    "Game Mode"                        },
	{ 0x2c, 1,    0xc0, 0xc0, "Credit"                           },
	{ 0x2c, 1,    0xc0, 0x80, "Medal timer"                      },
	{ 0x2c, 1,    0xc0, 0x40, "Credit timer"                     },
	{ 0x2c, 1,    0xc0, 0x00, "Credit"                           },

	{ 0,    0xfe, 0,    8,    "Credits Per Note"                 },
	{ 0x2d, 1,    0x01, 0x00, "5"                                },
	{ 0x2d, 1,    0x01, 0x00, "10"                               },
	{ 0x2d, 1,    0x01, 0x00, "25"                               },
	{ 0x2d, 1,    0x01, 0x00, "50"                               },
	{ 0x2d, 1,    0x01, 0x01, "10"                               },
	{ 0x2d, 1,    0x01, 0x01, "20"                               },
	{ 0x2d, 1,    0x01, 0x01, "50"                               },
	{ 0x2d, 1,    0x01, 0x01, "100"                              },

	{ 0,    0xfe, 0,    2,    "DIP-SW1:10"                       },
	{ 0x2d, 1,    0x02, 0x02, "Off"                              },
	{ 0x2d, 1,    0x02, 0x00, "On"                               },

	{ 0,    0xfe, 0,    4,    "Gokou/Shikou Generation Interval" },
	{ 0x2d, 1,    0x0c, 0x00, "4 (longer)"                       },
	{ 0x2d, 1,    0x0c, 0x04, "3"                                },
	{ 0x2d, 1,    0x0c, 0x08, "2"                                },
	{ 0x2d, 1,    0x0c, 0x0c, "1 (shorter)"                      },

	{ 0,    0xfe, 0,    2,    "Maximum Bonus Points"             },
	{ 0x2d, 1,    0x20, 0x20, "99"                               },
	{ 0x2d, 1,    0x20, 0x00, "999"                              },

	{ 0,    0xfe, 0,    4,    "DIP-SW4:9,10"                     },
	{ 0x2d, 1,    0xc0, 0xc0, "Hanafuda Panel"                   },
	{ 0x2d, 1,    0xc0, 0x80, "Mahjong Panel"                    },
	{ 0x2d, 1,    0xc0, 0x40, "Hanafuda Amusement"               },
	{ 0x2d, 1,    0xc0, 0x00, "Mahjong Amusement"                },

	{ 0,    0xfe, 0,    2,    "Auto Bet"                         },
	{ 0x2f, 1,    0x40, 0x40, "Off"                              },
	{ 0x2f, 1,    0x40, 0x00, "On"                               },

	{ 0,    0xfe, 0,    2,    "? Hopper M."                      },
	{ 0x30, 1,    0x40, 0x40, "Off"                              },
	{ 0x30, 1,    0x40, 0x00, "On"                               },
};

STDDIPINFO(hnrose)

static struct BurnInputInfo hparadisInputList[] = {
	{ "P1 Start",     BIT_DIGITAL,   Joy[0] + 0,          "p1 start"      },
	{ "P1 Up",        BIT_DIGITAL,   Joy[0] + 1,          "p1 up"         },
	{ "P1 Down",      BIT_DIGITAL,   Joy[0] + 2,          "p1 down"       },
	{ "P1 Left",      BIT_DIGITAL,   Joy[0] + 3,          "p1 left"       },
	{ "P1 Right",     BIT_DIGITAL,   Joy[0] + 4,          "p1 right"      },
	{ "P1 Button 1",  BIT_DIGITAL,   Joy[0] + 5,          "p1 fire 1"     },
	{ "P1 Coin",      BIT_DIGITAL,   Joy[2] + 0,          "p1 coin"       },
	{ "P2 Coin",      BIT_DIGITAL,   Joy[2] + 1,          "p2 coin"       },
	{ "Service",      BIT_DIGITAL,   Joy[2] + 2,          "service"       },
	{ "Test",         BIT_DIGITAL,   Joy[2] + 3,          "diag"          },
	{ "Gamble Book",  BIT_DIGITAL,   Joy[2] + 4,          "gamble book"   },
	{ "Memory Reset", BIT_DIGITAL,   Joy[2] + 5,          "memory reset"  },
	{ "P1 A",         BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"         },
	{ "P1 E",         BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"         },
	{ "P1 M",         BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"         },
	{ "P1 Start",     BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"      },
	{ "P1 B",         BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"         },
	{ "P1 F",         BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"         },
	{ "P1 N",         BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"         },
	{ "P1 Bet",       BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"       },
	{ "P1 C",         BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"         },
	{ "P1 G",         BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"         },
	{ "P1 D",         BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"         },
	{ "P1 H",         BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"         },
	{ "P1 Score",     BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"     },
	{ "P1 Double Up", BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up" },
	{ "P1 Flip Flop", BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop" },
	{ "P1 Big",       BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"       },
	{ "P1 Small",     BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"     },
	{ "P2 A",         BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"         },
	{ "P2 E",         BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"         },
	{ "P2 M",         BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"         },
	{ "P2 Start",     BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"      },
	{ "P2 B",         BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"         },
	{ "P2 F",         BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"         },
	{ "P2 N",         BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"         },
	{ "P2 Bet",       BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"       },
	{ "P2 C",         BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"         },
	{ "P2 G",         BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"         },
	{ "P2 D",         BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"         },
	{ "P2 H",         BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"         },
	{ "P2 Score",     BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"     },
	{ "P2 Double Up", BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up" },
	{ "P2 Flip Flop", BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop" },
	{ "P2 Big",       BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"       },
	{ "P2 Small",     BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"     },
	{ "Reset",        BIT_DIGITAL,   &DrvReset,           "reset"         },
	{ "Dip A",        BIT_DIPSWITCH, Dips + 0,            "dip"           },
	{ "Dip B",        BIT_DIPSWITCH, Dips + 1,            "dip"           },
	{ "Dip C",        BIT_DIPSWITCH, Dips + 2,            "dip"           },
	{ "Dip D",        BIT_DIPSWITCH, Dips + 3,            "dip"           },
	{ "Dip E",        BIT_DIPSWITCH, Dips + 4,            "dip"           },
	{ "Dip F",        BIT_DIPSWITCH, Dips + 5,            "dip"           },
	{ "Dip G",        BIT_DIPSWITCH, Dips + 6,            "dip"           },
	{ "Dip H",        BIT_DIPSWITCH, Dips + 7,            "dip"           },
};

STDINPUTINFO(hparadis)

static struct BurnDIPInfo hparadisDIPList[] = {
	{ 0x2f, 0xff, 0xff, 0xff, NULL           },
	{ 0x30, 0xff, 0xff, 0xff, NULL           },
	{ 0x31, 0xff, 0xff, 0xff, NULL           },
	{ 0x32, 0xff, 0xff, 0xff, NULL           },
	{ 0x33, 0xff, 0xff, 0xff, NULL           },
	{ 0x34, 0xff, 0xff, 0xff, NULL           },
	{ 0x35, 0xff, 0xff, 0xff, NULL           },
	{ 0x36, 0xff, 0xff, 0xff, NULL           },

	{ 0,    0xfe, 0,    4,    "DIP-SW1:1,2"  },
	{ 0x2f, 1,    0x03, 0x00, "3C 1C"        },
	{ 0x2f, 1,    0x03, 0x01, "2C 1C"        },
	{ 0x2f, 1,    0x03, 0x03, "1C 1C"        },
	{ 0x2f, 1,    0x03, 0x02, "1C 2C"        },

	{ 0,    0xfe, 0,    4,    "DIP-SW1:3,4"  },
	{ 0x2f, 1,    0x0c, 0x08, "Easy"         },
	{ 0x2f, 1,    0x0c, 0x0c, "Normal"       },
	{ 0x2f, 1,    0x0c, 0x04, "Hard"         },
	{ 0x2f, 1,    0x0c, 0x00, "Hardest"      },

	{ 0,    0xfe, 0,    2,    "DIP-SW1:5"    },
	{ 0x2f, 1,    0x10, 0x00, "Off"          },
	{ 0x2f, 1,    0x10, 0x10, "On"           },

	{ 0,    0xfe, 0,    4,    "DIP-SW1:6,7"  },
	{ 0x2f, 1,    0x60, 0x60, "Hanafuda"     },
	{ 0x2f, 1,    0x60, 0x40, "Mahjong"      },
	{ 0x2f, 1,    0x60, 0x20, "Joystick"     },
	{ 0x2f, 1,    0x60, 0x00, "Joystick"     },

	{ 0,    0xfe, 0,    2,    "First Chance" },
	{ 0x2f, 1,    0x80, 0x00, "Off"          },
	{ 0x2f, 1,    0x80, 0x80, "On"           },

	{ 0,    0xfe, 0,    2,    "DIP-SW1:9"    },
	{ 0x31, 1,    0x01, 0x01, "Off"          },
	{ 0x31, 1,    0x01, 0x00, "On"           },

	{ 0,    0xfe, 0,    2,    "DIP-SW1:10"   },
	{ 0x31, 1,    0x02, 0x02, "Off"          },
	{ 0x31, 1,    0x02, 0x00, "On"           },
};

STDDIPINFO(hparadis)

static struct BurnInputInfo htengokuInputList[] = {
	{ "Gamble Keyout", BIT_DIGITAL,   Joy[2] + 0,          "gamble keyout" },
	{ "Test",          BIT_DIGITAL,   Joy[2] + 2,          "diag"          },
	{ "Gamble Book",   BIT_DIGITAL,   Joy[2] + 3,          "gamble book"   },
	{ "Memory Reset",  BIT_DIGITAL,   Joy[2] + 4,          "memory reset"  },
	{ "Gamble Keyin",  BIT_DIGITAL,   Joy[2] + 5,          "gamble keyin"  },
	{ "P1 Coin",       BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"       },
	{ "P1 A",          BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"         },
	{ "P1 E",          BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"         },
	{ "P1 M",          BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"         },
	{ "P1 Start",      BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"      },
	{ "P1 B",          BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"         },
	{ "P1 F",          BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"         },
	{ "P1 N",          BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"         },
	{ "P1 Bet",        BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"       },
	{ "P1 C",          BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"         },
	{ "P1 G",          BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"         },
	{ "P1 D",          BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"         },
	{ "P1 H",          BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"         },
	{ "P1 Score",      BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"     },
	{ "P1 Double Up",  BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up" },
	{ "P1 Flip Flop",  BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop" },
	{ "P1 Big",        BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"       },
	{ "P1 Small",      BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"     },
	{ "P2 A",          BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"         },
	{ "P2 E",          BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"         },
	{ "P2 M",          BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"         },
	{ "P2 Start",      BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"      },
	{ "P2 B",          BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"         },
	{ "P2 F",          BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"         },
	{ "P2 N",          BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"         },
	{ "P2 Bet",        BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"       },
	{ "P2 C",          BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"         },
	{ "P2 G",          BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"         },
	{ "P2 D",          BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"         },
	{ "P2 H",          BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"         },
	{ "P2 Score",      BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"     },
	{ "P2 Double Up",  BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up" },
	{ "P2 Flip Flop",  BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop" },
	{ "P2 Big",        BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"       },
	{ "P2 Small",      BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"     },
	{ "Reset",         BIT_DIGITAL,   &DrvReset,           "reset"         },
	{ "Dip A",         BIT_DIPSWITCH, Dips + 0,            "dip"           },
	{ "Dip B",         BIT_DIPSWITCH, Dips + 1,            "dip"           },
	{ "Dip C",         BIT_DIPSWITCH, Dips + 2,            "dip"           },
	{ "Dip D",         BIT_DIPSWITCH, Dips + 3,            "dip"           },
	{ "Dip E",         BIT_DIPSWITCH, Dips + 4,            "dip"           },
	{ "Dip F",         BIT_DIPSWITCH, Dips + 5,            "dip"           },
	{ "Dip G",         BIT_DIPSWITCH, Dips + 6,            "dip"           },
	{ "Dip H",         BIT_DIPSWITCH, Dips + 7,            "dip"           },
};

STDINPUTINFO(htengoku)

static struct BurnDIPInfo htengokuDIPList[] = {
	{ 0x29, 0xff, 0xff, 0xf5, NULL                        },
	{ 0x2a, 0xff, 0xff, 0xd5, NULL                        },
	{ 0x2b, 0xff, 0xff, 0x77, NULL                        },
	{ 0x2c, 0xff, 0xff, 0xff, NULL                        },
	{ 0x2d, 0xff, 0xff, 0xff, NULL                        },
	{ 0x2e, 0xff, 0xff, 0xff, NULL                        },
	{ 0x2f, 0xff, 0xff, 0xff, NULL                        },
	{ 0x30, 0xff, 0xff, 0xff, NULL                        },

	{ 0,    0xfe, 0,    2,    "Show Gals"                 },
	{ 0x29, 1,    0x01, 0x00, "Off"                       },
	{ 0x29, 1,    0x01, 0x01, "On"                        },

	{ 0,    0xfe, 0,    2,    "Stage Select"              },
	{ 0x29, 1,    0x02, 0x00, "Off"                       },
	{ 0x29, 1,    0x02, 0x02, "On"                        },

	{ 0,    0xfe, 0,    2,    "Higi"                      },
	{ 0x29, 1,    0x04, 0x00, "Off"                       },
	{ 0x29, 1,    0x04, 0x04, "On"                        },

	{ 0,    0xfe, 0,    2,    "Higi Character"            },
	{ 0x29, 1,    0x08, 0x00, "Off"                       },
	{ 0x29, 1,    0x08, 0x08, "On"                        },

	{ 0,    0xfe, 0,    2,    "In-Game Music"             },
	{ 0x29, 1,    0x10, 0x00, "Off"                       },
	{ 0x29, 1,    0x10, 0x10, "On"                        },

	{ 0,    0xfe, 0,    2,    "SW 1:6"                    },
	{ 0x29, 1,    0x20, 0x00, "Off"                       },
	{ 0x29, 1,    0x20, 0x20, "On"                        },

	{ 0,    0xfe, 0,    2,    "Hand Lesson"               },
	{ 0x29, 1,    0x40, 0x00, "Off"                       },
	{ 0x29, 1,    0x40, 0x40, "On"                        },

	{ 0,    0xfe, 0,    2,    "SW 1:8"                    },
	{ 0x29, 1,    0x80, 0x80, "Off"                       },
	{ 0x29, 1,    0x80, 0x00, "On"                        },

	{ 0,    0xfe, 0,    8,    "Hanafuda Game Payout Rate" },
	{ 0x2a, 1,    0x07, 0x00, "8 (Low)"                   },
	{ 0x2a, 1,    0x07, 0x01, "7"                         },
	{ 0x2a, 1,    0x07, 0x02, "6"                         },
	{ 0x2a, 1,    0x07, 0x03, "5"                         },
	{ 0x2a, 1,    0x07, 0x04, "4"                         },
	{ 0x2a, 1,    0x07, 0x05, "3"                         },
	{ 0x2a, 1,    0x07, 0x06, "2"                         },
	{ 0x2a, 1,    0x07, 0x07, "1 (High)"                  },

	{ 0,    0xfe, 0,    2,    "Payout Variation"          },
	{ 0x2a, 1,    0x08, 0x00, "Small"                     },
	{ 0x2a, 1,    0x08, 0x08, "Large"                     },

	{ 0,    0xfe, 0,    2,    "Gokou Yaku"                },
	{ 0x2a, 1,    0x10, 0x00, "Off"                       },
	{ 0x2a, 1,    0x10, 0x10, "On"                        },

	{ 0,    0xfe, 0,    4,    "Double-Up Game Win Rate"   },
	{ 0x2a, 1,    0x60, 0x00, "65%"                       },
	{ 0x2a, 1,    0x60, 0x20, "70%"                       },
	{ 0x2a, 1,    0x60, 0x40, "75%"                       },
	{ 0x2a, 1,    0x60, 0x60, "80%"                       },

	{ 0,    0xfe, 0,    4,    "SW 3:1,2"                  },
	{ 0x2b, 1,    0x03, 0x03, "1C 1C"                     },
	{ 0x2b, 1,    0x03, 0x02, "1C 2C"                     },
	{ 0x2b, 1,    0x03, 0x01, "1C 5C"                     },
	{ 0x2b, 1,    0x03, 0x00, "1C 10C"                    },

	{ 0,    0xfe, 0,    8,    "Key-In Rate"               },
	{ 0x2b, 1,    0x04, 0x00, "5"                         },
	{ 0x2b, 1,    0x04, 0x00, "10"                        },
	{ 0x2b, 1,    0x04, 0x00, "25"                        },
	{ 0x2b, 1,    0x04, 0x00, "50"                        },
	{ 0x2b, 1,    0x04, 0x04, "10"                        },
	{ 0x2b, 1,    0x04, 0x04, "20"                        },
	{ 0x2b, 1,    0x04, 0x04, "50"                        },
	{ 0x2b, 1,    0x04, 0x04, "100"                       },

	{ 0,    0xfe, 0,    2,    "Maximum Bet"               },
	{ 0x2b, 1,    0x08, 0x08, "5"                         },
	{ 0x2b, 1,    0x08, 0x00, "10"                        },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"               },
	{ 0x2b, 1,    0x30, 0x30, "1"                         },
	{ 0x2b, 1,    0x30, 0x20, "2"                         },
	{ 0x2b, 1,    0x30, 0x10, "3"                         },
	{ 0x2b, 1,    0x30, 0x00, "5"                         },

	{ 0,    0xfe, 0,    4,    "Credit Limit"              },
	{ 0x2b, 1,    0xc0, 0xc0, "1000"                      },
	{ 0x2b, 1,    0xc0, 0x80, "2000"                      },
	{ 0x2b, 1,    0xc0, 0x40, "3000"                      },
	{ 0x2b, 1,    0xc0, 0x00, "5000"                      },

	{ 0,    0xfe, 0,    4,    "Gokou Odds"                },
	{ 0x2c, 1,    0x03, 0x03, "100"                       },
	{ 0x2c, 1,    0x03, 0x02, "200"                       },
	{ 0x2c, 1,    0x03, 0x01, "250"                       },
	{ 0x2c, 1,    0x03, 0x00, "300"                       },

	{ 0,    0xfe, 0,    4,    "Shikou Odds"               },
	{ 0x2c, 1,    0x0c, 0x0c, "50"                        },
	{ 0x2c, 1,    0x0c, 0x08, "60"                        },
	{ 0x2c, 1,    0x0c, 0x04, "70"                        },
	{ 0x2c, 1,    0x0c, 0x00, "80"                        },

	{ 0,    0xfe, 0,    4,    "Ameshikou Odds"            },
	{ 0x2c, 1,    0x30, 0x30, "20"                        },
	{ 0x2c, 1,    0x30, 0x20, "30"                        },
	{ 0x2c, 1,    0x30, 0x10, "40"                        },
	{ 0x2c, 1,    0x30, 0x00, "50"                        },

	{ 0,    0xfe, 0,    2,    "SW 1:9"                    },
	{ 0x2d, 1,    0x01, 0x01, "Off"                       },
	{ 0x2d, 1,    0x01, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Time Settings Mode"        },
	{ 0x2d, 1,    0x02, 0x02, "Off"                       },
	{ 0x2d, 1,    0x02, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Payout Mode"               },
	{ 0x2d, 1,    0x10, 0x10, "Key-out"                   },
	{ 0x2d, 1,    0x10, 0x00, "Hopper"                    },

	{ 0,    0xfe, 0,    2,    "Hopper Polarity"           },
	{ 0x2d, 1,    0x20, 0x20, "Active Low"                },
	{ 0x2d, 1,    0x20, 0x00, "Active High"               },
};

STDDIPINFO(htengoku)

static struct BurnInputInfo janshinpInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "Service",        BIT_DIGITAL,   Joy[2] + 7,          "service"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(janshinp)

static struct BurnDIPInfo janshinpDIPList[] = {
	{ 0x3e, 0xff, 0xff, 0x47, NULL                        },
	{ 0x3f, 0xff, 0xff, 0x1f, NULL                        },
	{ 0x40, 0xff, 0xff, 0x0a, NULL                        },
	{ 0x41, 0xff, 0xff, 0xf0, NULL                        },
	{ 0x42, 0xff, 0xff, 0xe6, NULL                        },
	{ 0x43, 0xff, 0xff, 0xff, NULL                        },
	{ 0x44, 0xff, 0xff, 0xff, NULL                        },
	{ 0x45, 0xff, 0xff, 0xff, NULL                        },

	{ 0,    0xfe, 0,    16,   "Payout Rate"               },
	{ 0x3e, 1,    0x0f, 0x00, "50%"                       },
	{ 0x3e, 1,    0x0f, 0x01, "53%"                       },
	{ 0x3e, 1,    0x0f, 0x02, "56%"                       },
	{ 0x3e, 1,    0x0f, 0x03, "59%"                       },
	{ 0x3e, 1,    0x0f, 0x04, "62%"                       },
	{ 0x3e, 1,    0x0f, 0x05, "65%"                       },
	{ 0x3e, 1,    0x0f, 0x06, "68%"                       },
	{ 0x3e, 1,    0x0f, 0x07, "71%"                       },
	{ 0x3e, 1,    0x0f, 0x08, "75%"                       },
	{ 0x3e, 1,    0x0f, 0x09, "78%"                       },
	{ 0x3e, 1,    0x0f, 0x0a, "81%"                       },
	{ 0x3e, 1,    0x0f, 0x0b, "84%"                       },
	{ 0x3e, 1,    0x0f, 0x0c, "87%"                       },
	{ 0x3e, 1,    0x0f, 0x0d, "90%"                       },
	{ 0x3e, 1,    0x0f, 0x0e, "93%"                       },
	{ 0x3e, 1,    0x0f, 0x0f, "96%"                       },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                 },
	{ 0x3e, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"       },
	{ 0x3e, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"        },
	{ 0x3e, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"       },
	{ 0x3e, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"      },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"               },
	{ 0x3e, 1,    0xc0, 0xc0, "1"                         },
	{ 0x3e, 1,    0xc0, 0x80, "5"                         },
	{ 0x3e, 1,    0xc0, 0x40, "10"                        },
	{ 0x3e, 1,    0xc0, 0x00, "20"                        },

	{ 0,    0xfe, 0,    4,    "Coinage"                   },
	{ 0x3f, 1,    0x03, 0x03, "1C 1C"                     },
	{ 0x3f, 1,    0x03, 0x02, "1C 2C"                     },
	{ 0x3f, 1,    0x03, 0x01, "1C 5C"                     },
	{ 0x3f, 1,    0x03, 0x00, "1C 10C"                    },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"               },
	{ 0x3f, 1,    0x0c, 0x0c, "1"                         },
	{ 0x3f, 1,    0x0c, 0x08, "2"                         },
	{ 0x3f, 1,    0x0c, 0x04, "3"                         },
	{ 0x3f, 1,    0x0c, 0x00, "5"                         },

	{ 0,    0xfe, 0,    16,   "Credit Limit"              },
	{ 0x3f, 1,    0x30, 0x30, "300"                       },
	{ 0x3f, 1,    0x30, 0x30, "600"                       },
	{ 0x3f, 1,    0x30, 0x30, "1500"                      },
	{ 0x3f, 1,    0x30, 0x30, "3000"                      },
	{ 0x3f, 1,    0x30, 0x20, "500"                       },
	{ 0x3f, 1,    0x30, 0x20, "1000"                      },
	{ 0x3f, 1,    0x30, 0x20, "2500"                      },
	{ 0x3f, 1,    0x30, 0x20, "5000"                      },
	{ 0x3f, 1,    0x30, 0x10, "700"                       },
	{ 0x3f, 1,    0x30, 0x10, "1400"                      },
	{ 0x3f, 1,    0x30, 0x10, "3500"                      },
	{ 0x3f, 1,    0x30, 0x10, "7000"                      },
	{ 0x3f, 1,    0x30, 0x00, "1000"                      },
	{ 0x3f, 1,    0x30, 0x00, "2000"                      },
	{ 0x3f, 1,    0x30, 0x00, "5000"                      },
	{ 0x3f, 1,    0x30, 0x00, "10000"                     },

	{ 0,    0xfe, 0,    2,    "Double Bet"                },
	{ 0x3f, 1,    0x40, 0x40, "Off"                       },
	{ 0x3f, 1,    0x40, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Last Chance"               },
	{ 0x3f, 1,    0x80, 0x80, "Off"                       },
	{ 0x3f, 1,    0x80, 0x00, "On"                        },

	{ 0,    0xfe, 0,    8,    "Yakuman Chance Cycle"      },
	{ 0x40, 1,    0x07, 0x07, "None"                      },
	{ 0x40, 1,    0x07, 0x06, "First time only"           },
	{ 0x40, 1,    0x07, 0x05, "Every 300 coins"           },
	{ 0x40, 1,    0x07, 0x04, "Every 500 coins"           },
	{ 0x40, 1,    0x07, 0x03, "Every 700 coins"           },
	{ 0x40, 1,    0x07, 0x02, "Every 1000 coins"          },
	{ 0x40, 1,    0x07, 0x01, "Every 1000 coins"          },
	{ 0x40, 1,    0x07, 0x00, "Every 1000 coins"          },

	{ 0,    0xfe, 0,    2,    "Yakuman Chances Per Cycle" },
	{ 0x40, 1,    0x08, 0x00, "1"                         },
	{ 0x40, 1,    0x08, 0x08, "2"                         },

	{ 0,    0xfe, 0,    4,    "Fever Chance"              },
	{ 0x40, 1,    0x30, 0x30, "0"                         },
	{ 0x40, 1,    0x30, 0x20, "None?"                     },
	{ 0x40, 1,    0x30, 0x10, "Many?"                     },
	{ 0x40, 1,    0x30, 0x00, "Only One?"                 },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                },
	{ 0x40, 1,    0x40, 0x40, "Off"                       },
	{ 0x40, 1,    0x40, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Don Den Button"            },
	{ 0x40, 1,    0x80, 0x80, "Start"                     },
	{ 0x40, 1,    0x80, 0x00, "Flip Flop"                 },

	{ 0,    0xfe, 0,    2,    "DIP4:1"                    },
	{ 0x41, 1,    0x01, 0x01, "Off"                       },
	{ 0x41, 1,    0x01, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "In-Game Music"             },
	{ 0x41, 1,    0x02, 0x02, "Off"                       },
	{ 0x41, 1,    0x02, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Girls In Demo Mode"        },
	{ 0x41, 1,    0x04, 0x04, "Off"                       },
	{ 0x41, 1,    0x04, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Gal Select"                },
	{ 0x41, 1,    0x08, 0x08, "Off"                       },
	{ 0x41, 1,    0x08, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Unknown"                   },
	{ 0x41, 1,    0x10, 0x10, "Off"                       },
	{ 0x41, 1,    0x10, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Unknown"                   },
	{ 0x41, 1,    0x20, 0x20, "Off"                       },
	{ 0x41, 1,    0x20, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Unknown"                   },
	{ 0x41, 1,    0x40, 0x40, "Off"                       },
	{ 0x41, 1,    0x40, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Unknown"                   },
	{ 0x41, 1,    0x80, 0x80, "Off"                       },
	{ 0x41, 1,    0x80, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Credits Per Note"          },
	{ 0x42, 1,    0x01, 0x01, "5"                         },
	{ 0x42, 1,    0x01, 0x00, "10"                        },

	{ 0,    0xfe, 0,    2,    "Flip Screen"               },
	{ 0x42, 1,    0x02, 0x02, "Off"                       },
	{ 0x42, 1,    0x02, 0x00, "On"                        },

	{ 0,    0xfe, 0,    4,    "Computer Strength"         },
	{ 0x42, 1,    0x0c, 0x00, "Weak"                      },
	{ 0x42, 1,    0x0c, 0x04, "Normal"                    },
	{ 0x42, 1,    0x0c, 0x0c, "Strong"                    },
	{ 0x42, 1,    0x0c, 0x08, "Very Strong"               },

	{ 0,    0xfe, 0,    2,    "Action Game"               },
	{ 0x42, 1,    0x10, 0x10, "Off"                       },
	{ 0x42, 1,    0x10, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Debug After Bet"           },
	{ 0x42, 1,    0x20, 0x20, "Off"                       },
	{ 0x42, 1,    0x20, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Unknown"                   },
	{ 0x42, 1,    0x40, 0x40, "Off"                       },
	{ 0x42, 1,    0x40, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Adjust Clock"              },
	{ 0x42, 1,    0x80, 0x80, "Off"                       },
	{ 0x42, 1,    0x80, 0x00, "On"                        },

	{ 0,    0xfe, 0,    2,    "Bets?"                     },
	{ 0x44, 1,    0x40, 0x40, "0"                         },
	{ 0x44, 1,    0x40, 0x00, "1"                         },

	{ 0,    0xfe, 0,    2,    "Unknown"                   },
	{ 0x45, 1,    0x40, 0x40, "Off"                       },
	{ 0x45, 1,    0x40, 0x00, "On"                        },
};

STDDIPINFO(janshinp)

static struct BurnInputInfo jongohInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P3 Coin",        BIT_DIGITAL,   Joy[2] + 7,          "p3 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
	// Board service switch (MAME SYSTEM bit 2); keep existing DIP indices stable.
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
};

STDINPUTINFO(jongoh)

static struct BurnDIPInfo jongohDIPList[] = {
	{ 0x3d, 0xff, 0xff, 0x47, NULL                             },
	{ 0x3e, 0xff, 0xff, 0x0f, NULL                             },
	{ 0x3f, 0xff, 0xff, 0x84, NULL                             },
	{ 0x40, 0xff, 0xff, 0x00, NULL                             },
	{ 0x41, 0xff, 0xff, 0xfa, NULL                             },
	{ 0x42, 0xff, 0xff, 0xff, NULL                             },
	{ 0x43, 0xff, 0xff, 0xff, NULL                             },
	{ 0x44, 0xff, 0xff, 0xff, NULL                             },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                    },
	{ 0x3d, 1,    0x0f, 0x00, "50%"                            },
	{ 0x3d, 1,    0x0f, 0x01, "53%"                            },
	{ 0x3d, 1,    0x0f, 0x02, "56%"                            },
	{ 0x3d, 1,    0x0f, 0x03, "59%"                            },
	{ 0x3d, 1,    0x0f, 0x04, "62%"                            },
	{ 0x3d, 1,    0x0f, 0x05, "65%"                            },
	{ 0x3d, 1,    0x0f, 0x06, "68%"                            },
	{ 0x3d, 1,    0x0f, 0x07, "71%"                            },
	{ 0x3d, 1,    0x0f, 0x08, "75%"                            },
	{ 0x3d, 1,    0x0f, 0x09, "78%"                            },
	{ 0x3d, 1,    0x0f, 0x0a, "81%"                            },
	{ 0x3d, 1,    0x0f, 0x0b, "84%"                            },
	{ 0x3d, 1,    0x0f, 0x0c, "87%"                            },
	{ 0x3d, 1,    0x0f, 0x0d, "90%"                            },
	{ 0x3d, 1,    0x0f, 0x0e, "93%"                            },
	{ 0x3d, 1,    0x0f, 0x0f, "96%"                            },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                      },
	{ 0x3d, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"            },
	{ 0x3d, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"             },
	{ 0x3d, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"            },
	{ 0x3d, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"           },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                    },
	{ 0x3d, 1,    0xc0, 0xc0, "1"                              },
	{ 0x3d, 1,    0xc0, 0x80, "5"                              },
	{ 0x3d, 1,    0xc0, 0x40, "10"                             },
	{ 0x3d, 1,    0xc0, 0x00, "20"                             },

	{ 0,    0xfe, 0,    4,    "Coinage"                        },
	{ 0x3e, 1,    0x03, 0x03, "1C 1C"                          },
	{ 0x3e, 1,    0x03, 0x02, "1C 2C"                          },
	{ 0x3e, 1,    0x03, 0x01, "1C 5C"                          },
	{ 0x3e, 1,    0x03, 0x00, "1C 10C"                         },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                    },
	{ 0x3e, 1,    0x0c, 0x0c, "1"                              },
	{ 0x3e, 1,    0x0c, 0x08, "2"                              },
	{ 0x3e, 1,    0x0c, 0x04, "3"                              },
	{ 0x3e, 1,    0x0c, 0x00, "5"                              },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                   },
	{ 0x3e, 1,    0x30, 0x30, "300"                            },
	{ 0x3e, 1,    0x30, 0x30, "600"                            },
	{ 0x3e, 1,    0x30, 0x30, "1500"                           },
	{ 0x3e, 1,    0x30, 0x30, "3000"                           },
	{ 0x3e, 1,    0x30, 0x20, "500"                            },
	{ 0x3e, 1,    0x30, 0x20, "1000"                           },
	{ 0x3e, 1,    0x30, 0x20, "2500"                           },
	{ 0x3e, 1,    0x30, 0x20, "5000"                           },
	{ 0x3e, 1,    0x30, 0x10, "700"                            },
	{ 0x3e, 1,    0x30, 0x10, "1400"                           },
	{ 0x3e, 1,    0x30, 0x10, "3500"                           },
	{ 0x3e, 1,    0x30, 0x10, "7000"                           },
	{ 0x3e, 1,    0x30, 0x00, "1000"                           },
	{ 0x3e, 1,    0x30, 0x00, "2000"                           },
	{ 0x3e, 1,    0x30, 0x00, "5000"                           },
	{ 0x3e, 1,    0x30, 0x00, "10000"                          },

	{ 0,    0xfe, 0,    2,    "Double Bet"                     },
	{ 0x3e, 1,    0x40, 0x40, "Off"                            },
	{ 0x3e, 1,    0x40, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Last Chance"                    },
	{ 0x3e, 1,    0x80, 0x80, "Off"                            },
	{ 0x3e, 1,    0x80, 0x00, "On"                             },

	{ 0,    0xfe, 0,    8,    "Yakuman Chance Cycle"           },
	{ 0x3f, 1,    0x07, 0x07, "None"                           },
	{ 0x3f, 1,    0x07, 0x06, "First time only"                },
	{ 0x3f, 1,    0x07, 0x05, "Every 300 coins"                },
	{ 0x3f, 1,    0x07, 0x04, "Every 500 coins"                },
	{ 0x3f, 1,    0x07, 0x03, "Every 700 coins"                },
	{ 0x3f, 1,    0x07, 0x02, "Every 1000 coins"               },
	{ 0x3f, 1,    0x07, 0x01, "Every 1000 coins"               },
	{ 0x3f, 1,    0x07, 0x00, "Every 1000 coins"               },

	{ 0,    0xfe, 0,    2,    "Yakuman Chances Per Cycle"      },
	{ 0x3f, 1,    0x08, 0x00, "1"                              },
	{ 0x3f, 1,    0x08, 0x08, "2"                              },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                     },
	{ 0x3f, 1,    0x10, 0x10, "Off"                            },
	{ 0x3f, 1,    0x10, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Don Den Button"                 },
	{ 0x3f, 1,    0x20, 0x20, "Start"                          },
	{ 0x3f, 1,    0x20, 0x00, "Flip Flop"                      },

	{ 0,    0xfe, 0,    2,    "Medal Timer"                    },
	{ 0x3f, 1,    0x40, 0x40, "Off"                            },
	{ 0x3f, 1,    0x40, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Game Style"                     },
	{ 0x3f, 1,    0x80, 0x80, "Credit"                         },
	{ 0x3f, 1,    0x80, 0x00, "Credit timer"                   },

	{ 0,    0xfe, 0,    2,    "DIP-SW4:1"                      },
	{ 0x40, 1,    0x01, 0x01, "Off"                            },
	{ 0x40, 1,    0x01, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "In-Game Music"                  },
	{ 0x40, 1,    0x02, 0x02, "Off"                            },
	{ 0x40, 1,    0x02, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Show Renchan Gal"               },
	{ 0x40, 1,    0x04, 0x04, "No"                             },
	{ 0x40, 1,    0x04, 0x00, "Yes"                            },

	{ 0,    0xfe, 0,    2,    "Renchan Gal Display"            },
	{ 0x40, 1,    0x08, 0x08, "After every win"                },
	{ 0x40, 1,    0x08, 0x00, "After 3 consecutive wins"       },

	{ 0,    0xfe, 0,    2,    "Game Screen Gal Action"         },
	{ 0x40, 1,    0x10, 0x10, "Off"                            },
	{ 0x40, 1,    0x10, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "In-Game Voice"                  },
	{ 0x40, 1,    0x20, 0x20, "Off"                            },
	{ 0x40, 1,    0x20, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Winning Yakuman Display Voice"  },
	{ 0x40, 1,    0x40, 0x40, "Off"                            },
	{ 0x40, 1,    0x40, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Hopper Polarity"                },
	{ 0x40, 1,    0x80, 0x80, "Normal"                         },
	{ 0x40, 1,    0x80, 0x00, "Inverted"                       },

	{ 0,    0xfe, 0,    8,    "Credits Per Note"               },
	{ 0x41, 1,    0x01, 0x01, "5"                              },
	{ 0x41, 1,    0x01, 0x01, "10"                             },
	{ 0x41, 1,    0x01, 0x01, "25"                             },
	{ 0x41, 1,    0x01, 0x01, "50"                             },
	{ 0x41, 1,    0x01, 0x00, "10"                             },
	{ 0x41, 1,    0x01, 0x00, "20"                             },
	{ 0x41, 1,    0x01, 0x00, "50"                             },
	{ 0x41, 1,    0x01, 0x00, "100"                            },

	{ 0,    0xfe, 0,    2,    "DIP-SW1:10"                     },
	{ 0x41, 1,    0x02, 0x02, "Off"                            },
	{ 0x41, 1,    0x02, 0x00, "On"                             },

	{ 0,    0xfe, 0,    4,    "Computer Strength"              },
	{ 0x41, 1,    0x0c, 0x00, "Weak"                           },
	{ 0x41, 1,    0x0c, 0x04, "Normal"                         },
	{ 0x41, 1,    0x0c, 0x08, "Somewhat strong"                },
	{ 0x41, 1,    0x0c, 0x0c, "Strong"                         },

	{ 0,    0xfe, 0,    2,    "Start Method With Credit Timer" },
	{ 0x41, 1,    0x10, 0x10, "Normal"                         },
	{ 0x41, 1,    0x10, 0x00, "Always minimum bet"             },

	{ 0,    0xfe, 0,    2,    "Bets?"                          },
	{ 0x43, 1,    0x40, 0x40, "0"                              },
	{ 0x43, 1,    0x40, 0x00, "1"                              },

	{ 0,    0xfe, 0,    2,    "Unknown"                        },
	{ 0x44, 1,    0x40, 0x40, "Off"                            },
	{ 0x44, 1,    0x40, 0x00, "On"                             },

	{ 0,    0xfe, 0,    8,    "Bonus Point Cycle"              },
	{ 0x3f, 1,    0x07, 0x07, "None"                           },
	{ 0x3f, 1,    0x07, 0x06, "First time only"                },
	{ 0x3f, 1,    0x07, 0x05, "Every 300 coins"                },
	{ 0x3f, 1,    0x07, 0x04, "Every 500 coins"                },
	{ 0x3f, 1,    0x07, 0x03, "Every 700 coins"                },
	{ 0x3f, 1,    0x07, 0x02, "Every 1000 coins"               },
	{ 0x3f, 1,    0x07, 0x01, "Every 1000 coins"               },
	{ 0x3f, 1,    0x07, 0x00, "Every 1000 coins"               },

	{ 0,    0xfe, 0,    2,    "Yakuman Chances Per Cycle"      },
	{ 0x3f, 1,    0x08, 0x00, "1"                              },
	{ 0x3f, 1,    0x08, 0x08, "2"                              },

	{ 0,    0xfe, 0,    2,    "Flip Flop Button"               },
	{ 0x3f, 1,    0x10, 0x10, "Start"                          },
	{ 0x3f, 1,    0x10, 0x00, "Flip Flop"                      },

	{ 0,    0xfe, 0,    2,    "Last Chance"                    },
	{ 0x3f, 1,    0x20, 0x20, "Off"                            },
	{ 0x3f, 1,    0x20, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Double Bet"                     },
	{ 0x3f, 1,    0x40, 0x40, "Off"                            },
	{ 0x3f, 1,    0x40, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Medal Timer"                    },
	{ 0x3f, 1,    0x80, 0x80, "Off"                            },
	{ 0x3f, 1,    0x80, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "DIP-SW4:1"                      },
	{ 0x40, 1,    0x01, 0x01, "Off"                            },
	{ 0x40, 1,    0x01, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "In-Game Music"                  },
	{ 0x40, 1,    0x02, 0x02, "Off"                            },
	{ 0x40, 1,    0x02, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "In-Game Voice"                  },
	{ 0x40, 1,    0x04, 0x04, "Off"                            },
	{ 0x40, 1,    0x04, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Winning Yakuman Display Voice"  },
	{ 0x40, 1,    0x08, 0x08, "Off"                            },
	{ 0x40, 1,    0x08, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Show Renchan Gal"               },
	{ 0x40, 1,    0x10, 0x10, "No"                             },
	{ 0x40, 1,    0x10, 0x00, "Yes"                            },

	{ 0,    0xfe, 0,    2,    "Renchan Gal Display"            },
	{ 0x40, 1,    0x20, 0x20, "After every win"                },
	{ 0x40, 1,    0x20, 0x00, "After 3 consecutive wins"       },

	{ 0,    0xfe, 0,    2,    "Game Screen Gal Action"         },
	{ 0x40, 1,    0x40, 0x40, "Off"                            },
	{ 0x40, 1,    0x40, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                     },
	{ 0x40, 1,    0x80, 0x80, "Off"                            },
	{ 0x40, 1,    0x80, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Game Style"                     },
	{ 0x41, 1,    0x10, 0x10, "Credit"                         },
	{ 0x41, 1,    0x10, 0x00, "Credit timer"                   },

	{ 0,    0xfe, 0,    2,    "Start Method With Credit Timer" },
	{ 0x41, 1,    0x20, 0x20, "Normal"                         },
	{ 0x41, 1,    0x20, 0x00, "Always minimum bet"             },
};

STDDIPINFO(jongoh)

static struct BurnInputInfo jongteiInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(jongtei)

static struct BurnDIPInfo jongteiDIPList[] = {
	{ 0x3d, 0xff, 0xff, 0x47, NULL                            },
	{ 0x3e, 0xff, 0xff, 0x0f, NULL                            },
	{ 0x3f, 0xff, 0xff, 0xff, NULL                            },
	{ 0x40, 0xff, 0xff, 0xe8, NULL                            },
	{ 0x41, 0xff, 0xff, 0xff, NULL                            },
	{ 0x42, 0xff, 0xff, 0xff, NULL                            },
	{ 0x43, 0xff, 0xff, 0xff, NULL                            },
	{ 0x44, 0xff, 0xff, 0xff, NULL                            },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                   },
	{ 0x3d, 1,    0x0f, 0x00, "50%"                           },
	{ 0x3d, 1,    0x0f, 0x01, "53%"                           },
	{ 0x3d, 1,    0x0f, 0x02, "56%"                           },
	{ 0x3d, 1,    0x0f, 0x03, "59%"                           },
	{ 0x3d, 1,    0x0f, 0x04, "62%"                           },
	{ 0x3d, 1,    0x0f, 0x05, "65%"                           },
	{ 0x3d, 1,    0x0f, 0x06, "68%"                           },
	{ 0x3d, 1,    0x0f, 0x07, "71%"                           },
	{ 0x3d, 1,    0x0f, 0x08, "75%"                           },
	{ 0x3d, 1,    0x0f, 0x09, "78%"                           },
	{ 0x3d, 1,    0x0f, 0x0a, "81%"                           },
	{ 0x3d, 1,    0x0f, 0x0b, "84%"                           },
	{ 0x3d, 1,    0x0f, 0x0c, "87%"                           },
	{ 0x3d, 1,    0x0f, 0x0d, "90%"                           },
	{ 0x3d, 1,    0x0f, 0x0e, "93%"                           },
	{ 0x3d, 1,    0x0f, 0x0f, "96%"                           },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                     },
	{ 0x3d, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"           },
	{ 0x3d, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"            },
	{ 0x3d, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"           },
	{ 0x3d, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"          },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                   },
	{ 0x3d, 1,    0xc0, 0xc0, "1"                             },
	{ 0x3d, 1,    0xc0, 0x80, "5"                             },
	{ 0x3d, 1,    0xc0, 0x40, "10"                            },
	{ 0x3d, 1,    0xc0, 0x00, "20"                            },

	{ 0,    0xfe, 0,    4,    "Coinage"                       },
	{ 0x3e, 1,    0x03, 0x03, "1C 1C"                         },
	{ 0x3e, 1,    0x03, 0x02, "1C 2C"                         },
	{ 0x3e, 1,    0x03, 0x01, "1C 5C"                         },
	{ 0x3e, 1,    0x03, 0x00, "1C 10C"                        },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                   },
	{ 0x3e, 1,    0x0c, 0x0c, "1"                             },
	{ 0x3e, 1,    0x0c, 0x08, "2"                             },
	{ 0x3e, 1,    0x0c, 0x04, "3"                             },
	{ 0x3e, 1,    0x0c, 0x00, "5"                             },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                  },
	{ 0x3e, 1,    0x30, 0x30, "300"                           },
	{ 0x3e, 1,    0x30, 0x30, "600"                           },
	{ 0x3e, 1,    0x30, 0x30, "1500"                          },
	{ 0x3e, 1,    0x30, 0x30, "3000"                          },
	{ 0x3e, 1,    0x30, 0x20, "500"                           },
	{ 0x3e, 1,    0x30, 0x20, "1000"                          },
	{ 0x3e, 1,    0x30, 0x20, "2500"                          },
	{ 0x3e, 1,    0x30, 0x20, "5000"                          },
	{ 0x3e, 1,    0x30, 0x10, "700"                           },
	{ 0x3e, 1,    0x30, 0x10, "1400"                          },
	{ 0x3e, 1,    0x30, 0x10, "3500"                          },
	{ 0x3e, 1,    0x30, 0x10, "7000"                          },
	{ 0x3e, 1,    0x30, 0x00, "1000"                          },
	{ 0x3e, 1,    0x30, 0x00, "2000"                          },
	{ 0x3e, 1,    0x30, 0x00, "5000"                          },
	{ 0x3e, 1,    0x30, 0x00, "10000"                         },

	{ 0,    0xfe, 0,    2,    "Double Bet"                    },
	{ 0x3e, 1,    0x40, 0x40, "Off"                           },
	{ 0x3e, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Last Chance"                   },
	{ 0x3e, 1,    0x80, 0x80, "Off"                           },
	{ 0x3e, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    8,    "YAKUMAN Bonus"                 },
	{ 0x3f, 1,    0x07, 0x07, "Cut"                           },
	{ 0x3f, 1,    0x07, 0x06, "1 T"                           },
	{ 0x3f, 1,    0x07, 0x05, "300"                           },
	{ 0x3f, 1,    0x07, 0x04, "500"                           },
	{ 0x3f, 1,    0x07, 0x03, "700"                           },
	{ 0x3f, 1,    0x07, 0x02, "1000"                          },
	{ 0x3f, 1,    0x07, 0x01, "1500"                          },
	{ 0x3f, 1,    0x07, 0x00, "2000"                          },

	{ 0,    0xfe, 0,    4,    "YAKUMAN Times"                 },
	{ 0x3f, 1,    0x18, 0x00, "1"                             },
	{ 0x3f, 1,    0x18, 0x18, "1"                             },
	{ 0x3f, 1,    0x18, 0x10, "2"                             },
	{ 0x3f, 1,    0x18, 0x08, "3"                             },

	{ 0,    0xfe, 0,    2,    "3 BAI In YAKUMAN Bonus Chance" },
	{ 0x3f, 1,    0x20, 0x20, "Off"                           },
	{ 0x3f, 1,    0x20, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Auto Tsumo"                    },
	{ 0x3f, 1,    0x40, 0x40, "Off"                           },
	{ 0x3f, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Credit Timing"                 },
	{ 0x3f, 1,    0x80, 0x80, "Off"                           },
	{ 0x3f, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"                   },
	{ 0x40, 1,    0x01, 0x01, "Off"                           },
	{ 0x40, 1,    0x01, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "In-Game Music"                 },
	{ 0x40, 1,    0x02, 0x02, "Off"                           },
	{ 0x40, 1,    0x02, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Girls"                         },
	{ 0x40, 1,    0x04, 0x04, "Off"                           },
	{ 0x40, 1,    0x04, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Girls Show After 3 Renso"      },
	{ 0x40, 1,    0x08, 0x08, "Off"                           },
	{ 0x40, 1,    0x08, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x10, 0x10, "Off"                           },
	{ 0x40, 1,    0x10, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x20, 0x20, "Off"                           },
	{ 0x40, 1,    0x20, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x40, 0x40, "Off"                           },
	{ 0x40, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x80, 0x80, "Off"                           },
	{ 0x40, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Credits Per Note"              },
	{ 0x41, 1,    0x01, 0x01, "5"                             },
	{ 0x41, 1,    0x01, 0x00, "10"                            },

	{ 0,    0xfe, 0,    2,    "Flip Screen"                   },
	{ 0x41, 1,    0x02, 0x02, "Off"                           },
	{ 0x41, 1,    0x02, 0x00, "On"                            },

	{ 0,    0xfe, 0,    4,    "Computer Strength"             },
	{ 0x41, 1,    0x0c, 0x0c, "Weak"                          },
	{ 0x41, 1,    0x0c, 0x08, "Normal"                        },
	{ 0x41, 1,    0x0c, 0x04, "Strong"                        },
	{ 0x41, 1,    0x0c, 0x00, "Very Strong"                   },

	{ 0,    0xfe, 0,    2,    "Game Style"                    },
	{ 0x41, 1,    0x10, 0x10, "Credit"                        },
	{ 0x41, 1,    0x10, 0x00, "Credit Time"                   },

	{ 0,    0xfe, 0,    2,    "Start Method (Credit Time)"    },
	{ 0x41, 1,    0x20, 0x20, "?"                             },
	{ 0x41, 1,    0x20, 0x00, "Rate"                          },

	{ 0,    0xfe, 0,    2,    "Unknown 4-8"                   },
	{ 0x41, 1,    0x40, 0x40, "Off"                           },
	{ 0x41, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown 4-9"                   },
	{ 0x41, 1,    0x80, 0x80, "Off"                           },
	{ 0x41, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Bets?"                         },
	{ 0x43, 1,    0x40, 0x40, "0"                             },
	{ 0x43, 1,    0x40, 0x00, "1"                             },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x44, 1,    0x40, 0x40, "Off"                           },
	{ 0x44, 1,    0x40, 0x00, "On"                            },
};

STDDIPINFO(jongtei)

static struct BurnInputInfo kotbinspInputList[] = {
	{ "P1 Coin",     BIT_DIGITAL,   Joy[2] + 0, "p1 coin"   },
	{ "P1 Start",    BIT_DIGITAL,   Joy[0] + 0, "p1 start"  },
	{ "P1 Up",       BIT_DIGITAL,   Joy[0] + 1, "p1 up"     },
	{ "P1 Down",     BIT_DIGITAL,   Joy[0] + 2, "p1 down"   },
	{ "P1 Left",     BIT_DIGITAL,   Joy[0] + 3, "p1 left"   },
	{ "P1 Right",    BIT_DIGITAL,   Joy[0] + 4, "p1 right"  },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[0] + 5, "p1 fire 1" },
	{ "P2 Yes",      BIT_DIGITAL,   Joy[1] + 3, "mah yes"   },
	{ "P2 No",       BIT_DIGITAL,   Joy[1] + 4, "mah no"    },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(kotbinsp)

static struct BurnDIPInfo kotbinspDIPList[] = {
	{ 0x0a, 0xff, 0xff, 0xff, NULL            },
	{ 0x0b, 0xff, 0xff, 0xff, NULL            },
	{ 0x0c, 0xff, 0xff, 0xff, NULL            },
	{ 0x0d, 0xff, 0xff, 0xff, NULL            },
	{ 0x0e, 0xff, 0xff, 0xff, NULL            },
	{ 0x0f, 0xff, 0xff, 0xff, NULL            },
	{ 0x10, 0xff, 0xff, 0xff, NULL            },
	{ 0x11, 0xff, 0xff, 0xff, NULL            },

	{ 0,    0xfe, 0,    4,    "Unknown 2-0&1" },
	{ 0x0a, 1,    0x03, 0x03, "0"             },
	{ 0x0a, 1,    0x03, 0x02, "1"             },
	{ 0x0a, 1,    0x03, 0x01, "2"             },
	{ 0x0a, 1,    0x03, 0x00, "3"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-2"   },
	{ 0x0a, 1,    0x04, 0x04, "Off"           },
	{ 0x0a, 1,    0x04, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-3"   },
	{ 0x0a, 1,    0x08, 0x08, "Off"           },
	{ 0x0a, 1,    0x08, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-4"   },
	{ 0x0a, 1,    0x10, 0x10, "Off"           },
	{ 0x0a, 1,    0x10, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-5"   },
	{ 0x0a, 1,    0x20, 0x20, "Off"           },
	{ 0x0a, 1,    0x20, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-6"   },
	{ 0x0a, 1,    0x40, 0x40, "Off"           },
	{ 0x0a, 1,    0x40, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-7"   },
	{ 0x0a, 1,    0x80, 0x80, "Off"           },
	{ 0x0a, 1,    0x80, 0x00, "On"            },

	{ 0,    0xfe, 0,    8,    "Difficulty?"   },
	{ 0x0b, 1,    0x07, 0x07, "0"             },
	{ 0x0b, 1,    0x07, 0x06, "1"             },
	{ 0x0b, 1,    0x07, 0x05, "2"             },
	{ 0x0b, 1,    0x07, 0x04, "3"             },
	{ 0x0b, 1,    0x07, 0x03, "4"             },
	{ 0x0b, 1,    0x07, 0x02, "5"             },
	{ 0x0b, 1,    0x07, 0x01, "6"             },
	{ 0x0b, 1,    0x07, 0x00, "7"             },

	{ 0,    0xfe, 0,    8,    "Coinage"       },
	{ 0x0b, 1,    0x38, 0x00, "5C 1C"         },
	{ 0x0b, 1,    0x38, 0x08, "4C 1C"         },
	{ 0x0b, 1,    0x38, 0x10, "3C 1C"         },
	{ 0x0b, 1,    0x38, 0x18, "2C 1C"         },
	{ 0x0b, 1,    0x38, 0x38, "1C 1C"         },
	{ 0x0b, 1,    0x38, 0x30, "1C 2C"         },
	{ 0x0b, 1,    0x38, 0x28, "1C 5C"         },
	{ 0x0b, 1,    0x38, 0x20, "1C 50C"        },

	{ 0,    0xfe, 0,    2,    "Unknown 1-6"   },
	{ 0x0b, 1,    0x40, 0x40, "Off"           },
	{ 0x0b, 1,    0x40, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 1-7"   },
	{ 0x0b, 1,    0x80, 0x80, "Off"           },
	{ 0x0b, 1,    0x80, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-8"   },
	{ 0x0e, 1,    0x01, 0x01, "Off"           },
	{ 0x0e, 1,    0x01, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-9"   },
	{ 0x0e, 1,    0x02, 0x02, "Off"           },
	{ 0x0e, 1,    0x02, 0x00, "Unknown 2-0&1" },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"   },
	{ 0x0e, 1,    0x04, 0x00, "Off"           },
	{ 0x0e, 1,    0x04, 0x04, "On"            },

	{ 0,    0xfe, 0,    2,    "Flip Screen"   },
	{ 0x0e, 1,    0x08, 0x08, "Off"           },
	{ 0x0e, 1,    0x08, 0x00, "On"            },
};

STDDIPINFO(kotbinsp)

static struct BurnInputInfo kotbinyoInputList[] = {
	{ "P1 Coin",  BIT_DIGITAL,   Joy[2] + 0, "p1 coin"  },
	{ "P3 G",     BIT_DIGITAL,   Joy[2] + 4, "mah g"    },
	{ "P3 H",     BIT_DIGITAL,   Joy[2] + 5, "mah h"    },
	{ "Service",  BIT_DIGITAL,   Joy[2] + 6, "service"  },
	{ "P1 Start", BIT_DIGITAL,   Joy[0] + 0, "p1 start" },
	{ "P1 A",     BIT_DIGITAL,   Joy[0] + 1, "mah a"    },
	{ "P1 B",     BIT_DIGITAL,   Joy[0] + 2, "mah b"    },
	{ "P1 C",     BIT_DIGITAL,   Joy[0] + 3, "mah c"    },
	{ "P1 D",     BIT_DIGITAL,   Joy[0] + 4, "mah d"    },
	{ "P1 E",     BIT_DIGITAL,   Joy[0] + 5, "mah e"    },
	{ "P1 F",     BIT_DIGITAL,   Joy[0] + 6, "mah f"    },
	{ "P2 Yes",   BIT_DIGITAL,   Joy[1] + 3, "mah yes"  },
	{ "P2 No",    BIT_DIGITAL,   Joy[1] + 4, "mah no"   },
	{ "Reset",    BIT_DIGITAL,   &DrvReset,  "reset"    },
	{ "Dip A",    BIT_DIPSWITCH, Dips + 0,   "dip"      },
	{ "Dip B",    BIT_DIPSWITCH, Dips + 1,   "dip"      },
	{ "Dip C",    BIT_DIPSWITCH, Dips + 2,   "dip"      },
	{ "Dip D",    BIT_DIPSWITCH, Dips + 3,   "dip"      },
	{ "Dip E",    BIT_DIPSWITCH, Dips + 4,   "dip"      },
	{ "Dip F",    BIT_DIPSWITCH, Dips + 5,   "dip"      },
	{ "Dip G",    BIT_DIPSWITCH, Dips + 6,   "dip"      },
	{ "Dip H",    BIT_DIPSWITCH, Dips + 7,   "dip"      },
};

STDINPUTINFO(kotbinyo)

static struct BurnDIPInfo kotbinyoDIPList[] = {
	{ 0x0e, 0xff, 0xff, 0xff, NULL            },
	{ 0x0f, 0xff, 0xff, 0xff, NULL            },
	{ 0x10, 0xff, 0xff, 0xff, NULL            },
	{ 0x11, 0xff, 0xff, 0xff, NULL            },
	{ 0x12, 0xff, 0xff, 0xff, NULL            },
	{ 0x13, 0xff, 0xff, 0xff, NULL            },
	{ 0x14, 0xff, 0xff, 0xff, NULL            },
	{ 0x15, 0xff, 0xff, 0xff, NULL            },

	{ 0,    0xfe, 0,    8,    "Difficulty?"   },
	{ 0x0e, 1,    0x07, 0x07, "0"             },
	{ 0x0e, 1,    0x07, 0x06, "1"             },
	{ 0x0e, 1,    0x07, 0x05, "2"             },
	{ 0x0e, 1,    0x07, 0x04, "3"             },
	{ 0x0e, 1,    0x07, 0x03, "4"             },
	{ 0x0e, 1,    0x07, 0x02, "5"             },
	{ 0x0e, 1,    0x07, 0x01, "6"             },
	{ 0x0e, 1,    0x07, 0x00, "7"             },

	{ 0,    0xfe, 0,    8,    "Coinage"       },
	{ 0x0e, 1,    0x38, 0x00, "5C 1C"         },
	{ 0x0e, 1,    0x38, 0x08, "4C 1C"         },
	{ 0x0e, 1,    0x38, 0x10, "3C 1C"         },
	{ 0x0e, 1,    0x38, 0x18, "2C 1C"         },
	{ 0x0e, 1,    0x38, 0x38, "1C 1C"         },
	{ 0x0e, 1,    0x38, 0x30, "1C 2C"         },
	{ 0x0e, 1,    0x38, 0x28, "1C 5C"         },
	{ 0x0e, 1,    0x38, 0x20, "1C 50C"        },

	{ 0,    0xfe, 0,    2,    "Controls"      },
	{ 0x0e, 1,    0x40, 0x00, "Keyboard"      },
	{ 0x0e, 1,    0x40, 0x40, "Joystick"      },

	{ 0,    0xfe, 0,    2,    "Unknown 1-7"   },
	{ 0x0e, 1,    0x80, 0x80, "Off"           },
	{ 0x0e, 1,    0x80, 0x00, "On"            },

	{ 0,    0xfe, 0,    4,    "Unknown 2-0&1" },
	{ 0x0f, 1,    0x03, 0x03, "0"             },
	{ 0x0f, 1,    0x03, 0x02, "1"             },
	{ 0x0f, 1,    0x03, 0x01, "2"             },
	{ 0x0f, 1,    0x03, 0x00, "3"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-2"   },
	{ 0x0f, 1,    0x04, 0x04, "Off"           },
	{ 0x0f, 1,    0x04, 0x00, "On"            },

	{ 0,    0xfe, 0,    4,    "Unknown 2-3&4" },
	{ 0x0f, 1,    0x18, 0x18, "0"             },
	{ 0x0f, 1,    0x18, 0x10, "1"             },
	{ 0x0f, 1,    0x18, 0x08, "2"             },
	{ 0x0f, 1,    0x18, 0x00, "3"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-5"   },
	{ 0x0f, 1,    0x20, 0x20, "Off"           },
	{ 0x0f, 1,    0x20, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-6"   },
	{ 0x0f, 1,    0x40, 0x40, "Off"           },
	{ 0x0f, 1,    0x40, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-7"   },
	{ 0x0f, 1,    0x80, 0x80, "Off"           },
	{ 0x0f, 1,    0x80, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"   },
	{ 0x12, 1,    0x01, 0x00, "Off"           },
	{ 0x12, 1,    0x01, 0x01, "On"            },

	{ 0,    0xfe, 0,    2,    "Flip Screen"   },
	{ 0x12, 1,    0x02, 0x02, "Off"           },
	{ 0x12, 1,    0x02, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-8"   },
	{ 0x12, 1,    0x04, 0x04, "Off"           },
	{ 0x12, 1,    0x04, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-9"   },
	{ 0x12, 1,    0x08, 0x08, "Off"           },
	{ 0x12, 1,    0x08, 0x00, "On"            },
};

STDDIPINFO(kotbinyo)

static struct BurnInputInfo kotbinyosuInputList[] = {
	{ "P1 Coin",  BIT_DIGITAL,   Joy[2] + 0, "p1 coin"  },
	{ "P3 G",     BIT_DIGITAL,   Joy[2] + 4, "mah g"    },
	{ "P3 H",     BIT_DIGITAL,   Joy[2] + 5, "mah h"    },
	{ "Service",  BIT_DIGITAL,   Joy[2] + 6, "service"  },
	{ "P1 Start", BIT_DIGITAL,   Joy[0] + 0, "p1 start" },
	{ "P1 A",     BIT_DIGITAL,   Joy[0] + 1, "mah a"    },
	{ "P1 B",     BIT_DIGITAL,   Joy[0] + 2, "mah b"    },
	{ "P1 C",     BIT_DIGITAL,   Joy[0] + 3, "mah c"    },
	{ "P1 D",     BIT_DIGITAL,   Joy[0] + 4, "mah d"    },
	{ "P1 E",     BIT_DIGITAL,   Joy[0] + 5, "mah e"    },
	{ "P1 F",     BIT_DIGITAL,   Joy[0] + 6, "mah f"    },
	{ "P2 Yes",   BIT_DIGITAL,   Joy[1] + 3, "mah yes"  },
	{ "P2 No",    BIT_DIGITAL,   Joy[1] + 4, "mah no"   },
	{ "Reset",    BIT_DIGITAL,   &DrvReset,  "reset"    },
	{ "Dip A",    BIT_DIPSWITCH, Dips + 0,   "dip"      },
	{ "Dip B",    BIT_DIPSWITCH, Dips + 1,   "dip"      },
	{ "Dip C",    BIT_DIPSWITCH, Dips + 2,   "dip"      },
	{ "Dip D",    BIT_DIPSWITCH, Dips + 3,   "dip"      },
	{ "Dip E",    BIT_DIPSWITCH, Dips + 4,   "dip"      },
	{ "Dip F",    BIT_DIPSWITCH, Dips + 5,   "dip"      },
	{ "Dip G",    BIT_DIPSWITCH, Dips + 6,   "dip"      },
	{ "Dip H",    BIT_DIPSWITCH, Dips + 7,   "dip"      },
};

STDINPUTINFO(kotbinyosu)

static struct BurnDIPInfo kotbinyosuDIPList[] = {
	{ 0x0e, 0xff, 0xff, 0xff, NULL            },
	{ 0x0f, 0xff, 0xff, 0xef, NULL            },
	{ 0x10, 0xff, 0xff, 0xff, NULL            },
	{ 0x11, 0xff, 0xff, 0xff, NULL            },
	{ 0x12, 0xff, 0xff, 0xf7, NULL            },
	{ 0x13, 0xff, 0xff, 0xff, NULL            },
	{ 0x14, 0xff, 0xff, 0xff, NULL            },
	{ 0x15, 0xff, 0xff, 0xff, NULL            },

	{ 0,    0xfe, 0,    8,    "Difficulty?"   },
	{ 0x0e, 1,    0x07, 0x07, "0"             },
	{ 0x0e, 1,    0x07, 0x06, "1"             },
	{ 0x0e, 1,    0x07, 0x05, "2"             },
	{ 0x0e, 1,    0x07, 0x04, "3"             },
	{ 0x0e, 1,    0x07, 0x03, "4"             },
	{ 0x0e, 1,    0x07, 0x02, "5"             },
	{ 0x0e, 1,    0x07, 0x01, "6"             },
	{ 0x0e, 1,    0x07, 0x00, "7"             },

	{ 0,    0xfe, 0,    8,    "Coinage"       },
	{ 0x0e, 1,    0x38, 0x00, "5C 1C"         },
	{ 0x0e, 1,    0x38, 0x08, "4C 1C"         },
	{ 0x0e, 1,    0x38, 0x10, "3C 1C"         },
	{ 0x0e, 1,    0x38, 0x18, "2C 1C"         },
	{ 0x0e, 1,    0x38, 0x38, "1C 1C"         },
	{ 0x0e, 1,    0x38, 0x30, "1C 2C"         },
	{ 0x0e, 1,    0x38, 0x28, "1C 5C"         },
	{ 0x0e, 1,    0x38, 0x20, "1C 50C"        },

	{ 0,    0xfe, 0,    2,    "Controls"      },
	{ 0x0e, 1,    0x40, 0x00, "Keyboard"      },
	{ 0x0e, 1,    0x40, 0x40, "Joystick"      },

	{ 0,    0xfe, 0,    2,    "Unknown 1-7"   },
	{ 0x0e, 1,    0x80, 0x80, "Off"           },
	{ 0x0e, 1,    0x80, 0x00, "On"            },

	{ 0,    0xfe, 0,    4,    "Unknown 2-0&1" },
	{ 0x0f, 1,    0x03, 0x03, "0"             },
	{ 0x0f, 1,    0x03, 0x02, "1"             },
	{ 0x0f, 1,    0x03, 0x01, "2"             },
	{ 0x0f, 1,    0x03, 0x00, "3"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-2"   },
	{ 0x0f, 1,    0x04, 0x04, "Off"           },
	{ 0x0f, 1,    0x04, 0x00, "On"            },

	{ 0,    0xfe, 0,    4,    "Unknown 2-3&4" },
	{ 0x0f, 1,    0x18, 0x18, "0"             },
	{ 0x0f, 1,    0x18, 0x10, "1"             },
	{ 0x0f, 1,    0x18, 0x08, "2"             },
	{ 0x0f, 1,    0x18, 0x00, "3"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-5"   },
	{ 0x0f, 1,    0x20, 0x20, "Off"           },
	{ 0x0f, 1,    0x20, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-6"   },
	{ 0x0f, 1,    0x40, 0x40, "Off"           },
	{ 0x0f, 1,    0x40, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-7"   },
	{ 0x0f, 1,    0x80, 0x80, "Off"           },
	{ 0x0f, 1,    0x80, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"   },
	{ 0x12, 1,    0x01, 0x00, "Off"           },
	{ 0x12, 1,    0x01, 0x01, "On"            },

	{ 0,    0xfe, 0,    2,    "Flip Screen"   },
	{ 0x12, 1,    0x02, 0x02, "Off"           },
	{ 0x12, 1,    0x02, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-8"   },
	{ 0x12, 1,    0x04, 0x04, "Off"           },
	{ 0x12, 1,    0x04, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-9"   },
	{ 0x12, 1,    0x08, 0x08, "Off"           },
	{ 0x12, 1,    0x08, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-3"   },
	{ 0x0f, 1,    0x08, 0x08, "Off"           },
	{ 0x0f, 1,    0x08, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-4"   },
	{ 0x0f, 1,    0x10, 0x10, "Off"           },
	{ 0x0f, 1,    0x10, 0x00, "On"            },

	{ 0,    0xfe, 0,    2,    "Unknown 2-9"   },
	{ 0x12, 1,    0x08, 0x08, "Off"           },
	{ 0x12, 1,    0x08, 0x00, "On"            },
};

STDDIPINFO(kotbinyosu)

static struct BurnInputInfo mjchuukaInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjchuuka)

static struct BurnDIPInfo mjchuukaDIPList[] = {
	{ 0x3d, 0xff, 0xff, 0xf7, NULL                   },
	{ 0x3e, 0xff, 0xff, 0xff, NULL                   },
	{ 0x3f, 0xff, 0xff, 0xff, NULL                   },
	{ 0x40, 0xff, 0xff, 0xe0, NULL                   },
	{ 0x41, 0xff, 0xff, 0xff, NULL                   },
	{ 0x42, 0xff, 0xff, 0xff, NULL                   },
	{ 0x43, 0xff, 0xff, 0xff, NULL                   },
	{ 0x44, 0xff, 0xff, 0xff, NULL                   },

	{ 0,    0xfe, 0,    16,   "Pay Out Rate (%)"     },
	{ 0x3d, 1,    0x0f, 0x00, "50"                   },
	{ 0x3d, 1,    0x0f, 0x01, "53"                   },
	{ 0x3d, 1,    0x0f, 0x02, "56"                   },
	{ 0x3d, 1,    0x0f, 0x03, "59"                   },
	{ 0x3d, 1,    0x0f, 0x04, "62"                   },
	{ 0x3d, 1,    0x0f, 0x05, "65"                   },
	{ 0x3d, 1,    0x0f, 0x06, "68"                   },
	{ 0x3d, 1,    0x0f, 0x07, "71"                   },
	{ 0x3d, 1,    0x0f, 0x08, "75"                   },
	{ 0x3d, 1,    0x0f, 0x09, "78"                   },
	{ 0x3d, 1,    0x0f, 0x0a, "81"                   },
	{ 0x3d, 1,    0x0f, 0x0b, "84"                   },
	{ 0x3d, 1,    0x0f, 0x0c, "87"                   },
	{ 0x3d, 1,    0x0f, 0x0d, "90"                   },
	{ 0x3d, 1,    0x0f, 0x0e, "93"                   },
	{ 0x3d, 1,    0x0f, 0x0f, "96"                   },

	{ 0,    0xfe, 0,    4,    "Odds Rate"            },
	{ 0x3d, 1,    0x30, 0x20, "1 2 3 4 6 8 10 15"    },
	{ 0x3d, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"  },
	{ 0x3d, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"   },
	{ 0x3d, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100" },

	{ 0,    0xfe, 0,    4,    "Max Rate"             },
	{ 0x3d, 1,    0xc0, 0xc0, "1"                    },
	{ 0x3d, 1,    0xc0, 0x80, "5"                    },
	{ 0x3d, 1,    0xc0, 0x40, "10"                   },
	{ 0x3d, 1,    0xc0, 0x00, "20"                   },

	{ 0,    0xfe, 0,    4,    "Coin A"               },
	{ 0x3e, 1,    0x03, 0x03, "1C 1C"                },
	{ 0x3e, 1,    0x03, 0x02, "1C 2C"                },
	{ 0x3e, 1,    0x03, 0x01, "1C 5C"                },
	{ 0x3e, 1,    0x03, 0x00, "1C 10C"               },

	{ 0,    0xfe, 0,    4,    "Min Rate To Play"     },
	{ 0x3e, 1,    0x0c, 0x0c, "1"                    },
	{ 0x3e, 1,    0x0c, 0x08, "2"                    },
	{ 0x3e, 1,    0x0c, 0x04, "3"                    },
	{ 0x3e, 1,    0x0c, 0x00, "5"                    },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3e, 1,    0x10, 0x10, "10"                   },
	{ 0x3e, 1,    0x10, 0x00, "255"                  },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3e, 1,    0x20, 0x20, "Off"                  },
	{ 0x3e, 1,    0x20, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3e, 1,    0x40, 0x40, "Off"                  },
	{ 0x3e, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3e, 1,    0x80, 0x80, "Off"                  },
	{ 0x3e, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    8,    "YAKUMAN Bonus"        },
	{ 0x3f, 1,    0x07, 0x07, "Cut"                  },
	{ 0x3f, 1,    0x07, 0x06, "1 T"                  },
	{ 0x3f, 1,    0x07, 0x05, "300"                  },
	{ 0x3f, 1,    0x07, 0x04, "500"                  },
	{ 0x3f, 1,    0x07, 0x03, "700"                  },
	{ 0x3f, 1,    0x07, 0x02, "1000"                 },
	{ 0x3f, 1,    0x07, 0x01, "1000?"                },
	{ 0x3f, 1,    0x07, 0x00, "1000?"                },

	{ 0,    0xfe, 0,    4,    "YAKUMAN Times"        },
	{ 0x3f, 1,    0x18, 0x00, "1"                    },
	{ 0x3f, 1,    0x18, 0x18, "1"                    },
	{ 0x3f, 1,    0x18, 0x10, "2"                    },
	{ 0x3f, 1,    0x18, 0x08, "3?"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3f, 1,    0x20, 0x20, "Off"                  },
	{ 0x3f, 1,    0x20, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "DonDen Key"           },
	{ 0x3f, 1,    0x40, 0x40, "Start"                },
	{ 0x3f, 1,    0x40, 0x00, "Flip Flop"            },

	{ 0,    0xfe, 0,    2,    "Flip Screen"          },
	{ 0x3f, 1,    0x80, 0x80, "Off"                  },
	{ 0x3f, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"          },
	{ 0x40, 1,    0x01, 0x01, "Off"                  },
	{ 0x40, 1,    0x01, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "In Game Music"        },
	{ 0x40, 1,    0x02, 0x02, "Off"                  },
	{ 0x40, 1,    0x02, 0x00, "On"                   },

	{ 0,    0xfe, 0,    4,    "Girls"                },
	{ 0x40, 1,    0x0c, 0x0c, "No"                   },
	{ 0x40, 1,    0x0c, 0x08, "Dressed"              },
	{ 0x40, 1,    0x0c, 0x04, "Underwear"            },
	{ 0x40, 1,    0x0c, 0x00, "Nude"                 },

	{ 0,    0xfe, 0,    2,    "Girls Speech"         },
	{ 0x40, 1,    0x10, 0x10, "Off"                  },
	{ 0x40, 1,    0x10, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x40, 1,    0x20, 0x20, "Off"                  },
	{ 0x40, 1,    0x20, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x40, 1,    0x40, 0x40, "Off"                  },
	{ 0x40, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x40, 1,    0x80, 0x80, "Off"                  },
	{ 0x40, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x41, 1,    0x01, 0x01, "Off"                  },
	{ 0x41, 1,    0x01, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Credits Per Note"     },
	{ 0x41, 1,    0x02, 0x02, "5"                    },
	{ 0x41, 1,    0x02, 0x00, "10"                   },

	{ 0,    0xfe, 0,    4,    "Computer Strength?"   },
	{ 0x41, 1,    0x0c, 0x0c, "Weak"                 },
	{ 0x41, 1,    0x0c, 0x08, "Normal"               },
	{ 0x41, 1,    0x0c, 0x04, "Strong"               },
	{ 0x41, 1,    0x0c, 0x00, "Very Strong"          },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x41, 1,    0x10, 0x10, "Off"                  },
	{ 0x41, 1,    0x10, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x41, 1,    0x20, 0x20, "Off"                  },
	{ 0x41, 1,    0x20, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown 4-8"          },
	{ 0x41, 1,    0x40, 0x40, "Off"                  },
	{ 0x41, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown 4-9"          },
	{ 0x41, 1,    0x80, 0x80, "Off"                  },
	{ 0x41, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x43, 1,    0x40, 0x40, "Off"                  },
	{ 0x43, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x44, 1,    0x40, 0x40, "Off"                  },
	{ 0x44, 1,    0x40, 0x00, "On"                   },
};

STDDIPINFO(mjchuuka)

static struct BurnInputInfo mjfloveInputList[] = {
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 0,          "p1 coin"         },
	{ "Service",        BIT_DIGITAL,   Joy[2] + 2,          "service"         },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 3,          "diag"            },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjflove)

static struct BurnDIPInfo mjfloveDIPList[] = {
	{ 0x3a, 0xff, 0xff, 0xff, NULL         },
	{ 0x3b, 0xff, 0xff, 0xff, NULL         },
	{ 0x3c, 0xff, 0xff, 0xff, NULL         },
	{ 0x3d, 0xff, 0xff, 0xff, NULL         },
	{ 0x3e, 0xff, 0xff, 0xff, NULL         },
	{ 0x3f, 0xff, 0xff, 0xff, NULL         },
	{ 0x40, 0xff, 0xff, 0xff, NULL         },
	{ 0x41, 0xff, 0xff, 0xff, NULL         },

	{ 0,    0xfe, 0,    4,    "SW1:1,2"    },
	{ 0x3b, 1,    0x03, 0x00, "3C 1C"      },
	{ 0x3b, 1,    0x03, 0x01, "2C 1C"      },
	{ 0x3b, 1,    0x03, 0x03, "1C 1C"      },
	{ 0x3b, 1,    0x03, 0x02, "1C 2C"      },

	{ 0,    0xfe, 0,    8,    "SW1:3,4,5"  },
	{ 0x3b, 1,    0x1c, 0x18, "1 (Weak)"   },
	{ 0x3b, 1,    0x1c, 0x14, "2"          },
	{ 0x3b, 1,    0x1c, 0x10, "3"          },
	{ 0x3b, 1,    0x1c, 0x1c, "4 (Normal)" },
	{ 0x3b, 1,    0x1c, 0x0c, "5"          },
	{ 0x3b, 1,    0x1c, 0x08, "6"          },
	{ 0x3b, 1,    0x1c, 0x04, "7"          },
	{ 0x3b, 1,    0x1c, 0x00, "8 (Strong)" },

	{ 0,    0xfe, 0,    2,    "SW1:6"      },
	{ 0x3b, 1,    0x20, 0x00, "Off"        },
	{ 0x3b, 1,    0x20, 0x20, "On"         },

	{ 0,    0xfe, 0,    2,    "SW1:7"      },
	{ 0x3b, 1,    0x40, 0x40, "Off"        },
	{ 0x3b, 1,    0x40, 0x00, "On"         },

	{ 0,    0xfe, 0,    2,    "SW1:8"      },
	{ 0x3b, 1,    0x80, 0x80, "Off"        },
	{ 0x3b, 1,    0x80, 0x00, "On"         },
};

STDDIPINFO(mjflove)

static struct BurnInputInfo mjgnightInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjgnight)

static struct BurnDIPInfo mjgnightDIPList[] = {
	{ 0x3d, 0xff, 0xff, 0x47, NULL                            },
	{ 0x3e, 0xff, 0xff, 0x3f, NULL                            },
	{ 0x3f, 0xff, 0xff, 0xff, NULL                            },
	{ 0x40, 0xff, 0xff, 0xfc, NULL                            },
	{ 0x41, 0xff, 0xff, 0xff, NULL                            },
	{ 0x42, 0xff, 0xff, 0xff, NULL                            },
	{ 0x43, 0xff, 0xff, 0xff, NULL                            },
	{ 0x44, 0xff, 0xff, 0xff, NULL                            },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                   },
	{ 0x3d, 1,    0x0f, 0x00, "50%"                           },
	{ 0x3d, 1,    0x0f, 0x01, "53%"                           },
	{ 0x3d, 1,    0x0f, 0x02, "56%"                           },
	{ 0x3d, 1,    0x0f, 0x03, "59%"                           },
	{ 0x3d, 1,    0x0f, 0x04, "62%"                           },
	{ 0x3d, 1,    0x0f, 0x05, "65%"                           },
	{ 0x3d, 1,    0x0f, 0x06, "68%"                           },
	{ 0x3d, 1,    0x0f, 0x07, "71%"                           },
	{ 0x3d, 1,    0x0f, 0x08, "75%"                           },
	{ 0x3d, 1,    0x0f, 0x09, "78%"                           },
	{ 0x3d, 1,    0x0f, 0x0a, "81%"                           },
	{ 0x3d, 1,    0x0f, 0x0b, "84%"                           },
	{ 0x3d, 1,    0x0f, 0x0c, "87%"                           },
	{ 0x3d, 1,    0x0f, 0x0d, "90%"                           },
	{ 0x3d, 1,    0x0f, 0x0e, "93%"                           },
	{ 0x3d, 1,    0x0f, 0x0f, "96%"                           },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                     },
	{ 0x3d, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"           },
	{ 0x3d, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"            },
	{ 0x3d, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"           },
	{ 0x3d, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"          },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                   },
	{ 0x3d, 1,    0xc0, 0xc0, "1"                             },
	{ 0x3d, 1,    0xc0, 0x80, "5"                             },
	{ 0x3d, 1,    0xc0, 0x40, "10"                            },
	{ 0x3d, 1,    0xc0, 0x00, "20"                            },

	{ 0,    0xfe, 0,    4,    "Coinage"                       },
	{ 0x3e, 1,    0x03, 0x03, "1C 1C"                         },
	{ 0x3e, 1,    0x03, 0x02, "1C 2C"                         },
	{ 0x3e, 1,    0x03, 0x01, "1C 5C"                         },
	{ 0x3e, 1,    0x03, 0x00, "1C 10C"                        },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                   },
	{ 0x3e, 1,    0x0c, 0x0c, "1"                             },
	{ 0x3e, 1,    0x0c, 0x08, "2"                             },
	{ 0x3e, 1,    0x0c, 0x04, "3"                             },
	{ 0x3e, 1,    0x0c, 0x00, "5"                             },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                  },
	{ 0x3e, 1,    0x30, 0x30, "300"                           },
	{ 0x3e, 1,    0x30, 0x30, "600"                           },
	{ 0x3e, 1,    0x30, 0x30, "1500"                          },
	{ 0x3e, 1,    0x30, 0x30, "3000"                          },
	{ 0x3e, 1,    0x30, 0x20, "500"                           },
	{ 0x3e, 1,    0x30, 0x20, "1000"                          },
	{ 0x3e, 1,    0x30, 0x20, "2500"                          },
	{ 0x3e, 1,    0x30, 0x20, "5000"                          },
	{ 0x3e, 1,    0x30, 0x10, "700"                           },
	{ 0x3e, 1,    0x30, 0x10, "1400"                          },
	{ 0x3e, 1,    0x30, 0x10, "3500"                          },
	{ 0x3e, 1,    0x30, 0x10, "7000"                          },
	{ 0x3e, 1,    0x30, 0x00, "1000"                          },
	{ 0x3e, 1,    0x30, 0x00, "2000"                          },
	{ 0x3e, 1,    0x30, 0x00, "5000"                          },
	{ 0x3e, 1,    0x30, 0x00, "10000"                         },

	{ 0,    0xfe, 0,    2,    "Double Bet"                    },
	{ 0x3e, 1,    0x40, 0x40, "Off"                           },
	{ 0x3e, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Last Chance"                   },
	{ 0x3e, 1,    0x80, 0x80, "Off"                           },
	{ 0x3e, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    8,    "YAKUMAN Bonus"                 },
	{ 0x3f, 1,    0x07, 0x07, "Cut"                           },
	{ 0x3f, 1,    0x07, 0x06, "1 T"                           },
	{ 0x3f, 1,    0x07, 0x05, "300"                           },
	{ 0x3f, 1,    0x07, 0x04, "500"                           },
	{ 0x3f, 1,    0x07, 0x03, "700"                           },
	{ 0x3f, 1,    0x07, 0x02, "1000"                          },
	{ 0x3f, 1,    0x07, 0x01, "1500"                          },
	{ 0x3f, 1,    0x07, 0x00, "2000"                          },

	{ 0,    0xfe, 0,    4,    "YAKUMAN Times"                 },
	{ 0x3f, 1,    0x18, 0x00, "1"                             },
	{ 0x3f, 1,    0x18, 0x18, "1"                             },
	{ 0x3f, 1,    0x18, 0x10, "2"                             },
	{ 0x3f, 1,    0x18, 0x08, "3"                             },

	{ 0,    0xfe, 0,    2,    "3 BAI In YAKUMAN Bonus Chance" },
	{ 0x3f, 1,    0x20, 0x20, "Off"                           },
	{ 0x3f, 1,    0x20, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Auto Tsumo"                    },
	{ 0x3f, 1,    0x40, 0x40, "Off"                           },
	{ 0x3f, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Credit Timing"                 },
	{ 0x3f, 1,    0x80, 0x80, "Off"                           },
	{ 0x3f, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"                   },
	{ 0x40, 1,    0x01, 0x01, "Off"                           },
	{ 0x40, 1,    0x01, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "In-Game Music"                 },
	{ 0x40, 1,    0x02, 0x02, "Off"                           },
	{ 0x40, 1,    0x02, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Girls"                         },
	{ 0x40, 1,    0x04, 0x04, "Off"                           },
	{ 0x40, 1,    0x04, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Girls Show After 3 Renso"      },
	{ 0x40, 1,    0x08, 0x08, "Off"                           },
	{ 0x40, 1,    0x08, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x10, 0x10, "Off"                           },
	{ 0x40, 1,    0x10, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x20, 0x20, "Off"                           },
	{ 0x40, 1,    0x20, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x40, 0x40, "Off"                           },
	{ 0x40, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x80, 0x80, "Off"                           },
	{ 0x40, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Credits Per Note"              },
	{ 0x41, 1,    0x01, 0x01, "5"                             },
	{ 0x41, 1,    0x01, 0x00, "10"                            },

	{ 0,    0xfe, 0,    2,    "Flip Screen"                   },
	{ 0x41, 1,    0x02, 0x02, "Off"                           },
	{ 0x41, 1,    0x02, 0x00, "On"                            },

	{ 0,    0xfe, 0,    4,    "Computer Strength"             },
	{ 0x41, 1,    0x0c, 0x0c, "Weak"                          },
	{ 0x41, 1,    0x0c, 0x08, "Normal"                        },
	{ 0x41, 1,    0x0c, 0x04, "Strong"                        },
	{ 0x41, 1,    0x0c, 0x00, "Very Strong"                   },

	{ 0,    0xfe, 0,    2,    "Game Style"                    },
	{ 0x41, 1,    0x10, 0x10, "Credit"                        },
	{ 0x41, 1,    0x10, 0x00, "Credit Time"                   },

	{ 0,    0xfe, 0,    2,    "Start Method (Credit Time)"    },
	{ 0x41, 1,    0x20, 0x20, "?"                             },
	{ 0x41, 1,    0x20, 0x00, "Rate"                          },

	{ 0,    0xfe, 0,    2,    "Unknown 4-8"                   },
	{ 0x41, 1,    0x40, 0x40, "Off"                           },
	{ 0x41, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown 4-9"                   },
	{ 0x41, 1,    0x80, 0x80, "Off"                           },
	{ 0x41, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Bets?"                         },
	{ 0x43, 1,    0x40, 0x40, "0"                             },
	{ 0x43, 1,    0x40, 0x00, "1"                             },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x44, 1,    0x40, 0x40, "Off"                           },
	{ 0x44, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "DIP2:3"                        },
	{ 0x3e, 1,    0x04, 0x04, "Off"                           },
	{ 0x3e, 1,    0x04, 0x00, "On"                            },

	{ 0,    0xfe, 0,    8,    "Minimum Bet"                   },
	{ 0x3e, 1,    0x38, 0x38, "1"                             },
	{ 0x3e, 1,    0x38, 0x30, "2"                             },
	{ 0x3e, 1,    0x38, 0x28, "3"                             },
	{ 0x3e, 1,    0x38, 0x20, "4"                             },
	{ 0x3e, 1,    0x38, 0x18, "5"                             },
	{ 0x3e, 1,    0x38, 0x10, "6"                             },
	{ 0x3e, 1,    0x38, 0x08, "7"                             },
	{ 0x3e, 1,    0x38, 0x00, "8"                             },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                  },
	{ 0x3e, 1,    0xc0, 0xc0, "300"                           },
	{ 0x3e, 1,    0xc0, 0xc0, "600"                           },
	{ 0x3e, 1,    0xc0, 0xc0, "1500"                          },
	{ 0x3e, 1,    0xc0, 0xc0, "3000"                          },
	{ 0x3e, 1,    0xc0, 0x80, "500"                           },
	{ 0x3e, 1,    0xc0, 0x80, "1000"                          },
	{ 0x3e, 1,    0xc0, 0x80, "2500"                          },
	{ 0x3e, 1,    0xc0, 0x80, "5000"                          },
	{ 0x3e, 1,    0xc0, 0x40, "700"                           },
	{ 0x3e, 1,    0xc0, 0x40, "1400"                          },
	{ 0x3e, 1,    0xc0, 0x40, "3500"                          },
	{ 0x3e, 1,    0xc0, 0x40, "7000"                          },
	{ 0x3e, 1,    0xc0, 0x00, "1000"                          },
	{ 0x3e, 1,    0xc0, 0x00, "2000"                          },
	{ 0x3e, 1,    0xc0, 0x00, "5000"                          },
	{ 0x3e, 1,    0xc0, 0x00, "10000"                         },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x04, 0x04, "Off"                           },
	{ 0x40, 1,    0x04, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x08, 0x08, "Off"                           },
	{ 0x40, 1,    0x08, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x10, 0x10, "Off"                           },
	{ 0x40, 1,    0x10, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x20, 0x20, "Off"                           },
	{ 0x40, 1,    0x20, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x40, 0x40, "Off"                           },
	{ 0x40, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x40, 1,    0x80, 0x80, "Off"                           },
	{ 0x40, 1,    0x80, 0x00, "On"                            },
};

STDDIPINFO(mjgnight)

static struct BurnInputInfo mjkokryuInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P3 Coin",        BIT_DIGITAL,   Joy[2] + 7,          "p3 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjkokryu)

static struct BurnDIPInfo mjkokryuDIPList[] = {
	{ 0x3e, 0xff, 0xff, 0x46, NULL                                },
	{ 0x3f, 0xff, 0xff, 0x3f, NULL                                },
	{ 0x40, 0xff, 0xff, 0xdc, NULL                                },
	{ 0x41, 0xff, 0xff, 0xe1, NULL                                },
	{ 0x42, 0xff, 0xff, 0xfa, NULL                                },
	{ 0x43, 0xff, 0xff, 0xff, NULL                                },
	{ 0x44, 0xff, 0xff, 0xff, NULL                                },
	{ 0x45, 0xff, 0xff, 0xff, NULL                                },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                       },
	{ 0x3e, 1,    0x0f, 0x00, "50%"                               },
	{ 0x3e, 1,    0x0f, 0x01, "53%"                               },
	{ 0x3e, 1,    0x0f, 0x02, "56%"                               },
	{ 0x3e, 1,    0x0f, 0x03, "59%"                               },
	{ 0x3e, 1,    0x0f, 0x04, "62%"                               },
	{ 0x3e, 1,    0x0f, 0x05, "65%"                               },
	{ 0x3e, 1,    0x0f, 0x06, "68%"                               },
	{ 0x3e, 1,    0x0f, 0x07, "71%"                               },
	{ 0x3e, 1,    0x0f, 0x08, "75%"                               },
	{ 0x3e, 1,    0x0f, 0x09, "78%"                               },
	{ 0x3e, 1,    0x0f, 0x0a, "81%"                               },
	{ 0x3e, 1,    0x0f, 0x0b, "84%"                               },
	{ 0x3e, 1,    0x0f, 0x0c, "87%"                               },
	{ 0x3e, 1,    0x0f, 0x0d, "90%"                               },
	{ 0x3e, 1,    0x0f, 0x0e, "93%"                               },
	{ 0x3e, 1,    0x0f, 0x0f, "96%"                               },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                         },
	{ 0x3e, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"               },
	{ 0x3e, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"                },
	{ 0x3e, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"               },
	{ 0x3e, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"              },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                       },
	{ 0x3e, 1,    0xc0, 0xc0, "1"                                 },
	{ 0x3e, 1,    0xc0, 0x80, "5"                                 },
	{ 0x3e, 1,    0xc0, 0x40, "10"                                },
	{ 0x3e, 1,    0xc0, 0x00, "20"                                },

	{ 0,    0xfe, 0,    8,    "DIP-SW2:1,2,3"                     },
	{ 0x3f, 1,    0x07, 0x07, "1C 1C"                             },
	{ 0x3f, 1,    0x07, 0x06, "1C 2C"                             },
	{ 0x3f, 1,    0x07, 0x05, "1C 5C"                             },
	{ 0x3f, 1,    0x07, 0x04, "1C 10C"                            },
	{ 0x3f, 1,    0x07, 0x03, "1C 1C"                             },
	{ 0x3f, 1,    0x07, 0x02, "1C 2C"                             },
	{ 0x3f, 1,    0x07, 0x01, "1C 5C"                             },
	{ 0x3f, 1,    0x07, 0x00, "Free Play"                         },

	{ 0,    0xfe, 0,    8,    "Minimum Bet"                       },
	{ 0x3f, 1,    0x38, 0x38, "1"                                 },
	{ 0x3f, 1,    0x38, 0x30, "2"                                 },
	{ 0x3f, 1,    0x38, 0x28, "3"                                 },
	{ 0x3f, 1,    0x38, 0x20, "4"                                 },
	{ 0x3f, 1,    0x38, 0x18, "5"                                 },
	{ 0x3f, 1,    0x38, 0x10, "6"                                 },
	{ 0x3f, 1,    0x38, 0x08, "7"                                 },
	{ 0x3f, 1,    0x38, 0x00, "8"                                 },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                      },
	{ 0x3f, 1,    0xc0, 0xc0, "300"                               },
	{ 0x3f, 1,    0xc0, 0xc0, "600"                               },
	{ 0x3f, 1,    0xc0, 0xc0, "1500"                              },
	{ 0x3f, 1,    0xc0, 0xc0, "3000"                              },
	{ 0x3f, 1,    0xc0, 0x80, "500"                               },
	{ 0x3f, 1,    0xc0, 0x80, "1000"                              },
	{ 0x3f, 1,    0xc0, 0x80, "2500"                              },
	{ 0x3f, 1,    0xc0, 0x80, "5000"                              },
	{ 0x3f, 1,    0xc0, 0x40, "700"                               },
	{ 0x3f, 1,    0xc0, 0x40, "1400"                              },
	{ 0x3f, 1,    0xc0, 0x40, "3500"                              },
	{ 0x3f, 1,    0xc0, 0x40, "7000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "1000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "2000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "5000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "10000"                             },

	{ 0,    0xfe, 0,    8,    "Yakuman Chance Cycle"              },
	{ 0x40, 1,    0x07, 0x07, "None"                              },
	{ 0x40, 1,    0x07, 0x06, "First time only"                   },
	{ 0x40, 1,    0x07, 0x05, "Every 300 coins"                   },
	{ 0x40, 1,    0x07, 0x04, "Every 500 coins"                   },
	{ 0x40, 1,    0x07, 0x03, "Every 700 coins"                   },
	{ 0x40, 1,    0x07, 0x02, "Every 1000 coins"                  },
	{ 0x40, 1,    0x07, 0x01, "Every 1500 coins"                  },
	{ 0x40, 1,    0x07, 0x00, "Every 2000 coins"                  },

	{ 0,    0xfe, 0,    3,    "Yakuman Chances Per Cycle"         },
	{ 0x40, 1,    0x18, 0x18, "1"                                 },
	{ 0x40, 1,    0x18, 0x10, "2"                                 },
	{ 0x40, 1,    0x18, 0x18, "3"                                 },

	{ 0,    0xfe, 0,    2,    "Allow Sanbaiman as Yakuman Chance" },
	{ 0x40, 1,    0x20, 0x20, "No"                                },
	{ 0x40, 1,    0x20, 0x00, "Yes"                               },

	{ 0,    0xfe, 0,    2,    "Medal Timer"                       },
	{ 0x40, 1,    0x80, 0x80, "Off"                               },
	{ 0x40, 1,    0x80, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "DIP-SW4:1"                         },
	{ 0x41, 1,    0x01, 0x01, "Off"                               },
	{ 0x41, 1,    0x01, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Don Den Button"                    },
	{ 0x41, 1,    0x02, 0x02, "Start"                             },
	{ 0x41, 1,    0x02, 0x00, "Flip Flop"                         },

	{ 0,    0xfe, 0,    2,    "Last Chance"                       },
	{ 0x41, 1,    0x04, 0x04, "Off"                               },
	{ 0x41, 1,    0x04, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                        },
	{ 0x41, 1,    0x08, 0x08, "Off"                               },
	{ 0x41, 1,    0x08, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Double Bet"                        },
	{ 0x41, 1,    0x10, 0x10, "Off"                               },
	{ 0x41, 1,    0x10, 0x00, "On"                                },

	{ 0,    0xfe, 0,    8,    "Credits Per Note 1 / 2"            },
	{ 0x42, 1,    0x01, 0x01, "5 / 10"                            },
	{ 0x42, 1,    0x01, 0x01, "10 / 5"                            },
	{ 0x42, 1,    0x01, 0x01, "25 / 50"                           },
	{ 0x42, 1,    0x01, 0x01, "50 / 25"                           },
	{ 0x42, 1,    0x01, 0x00, "10 / 20"                           },
	{ 0x42, 1,    0x01, 0x00, "20 / 10"                           },
	{ 0x42, 1,    0x01, 0x00, "50 / 100"                          },
	{ 0x42, 1,    0x01, 0x00, "100 / 50"                          },

	{ 0,    0xfe, 0,    2,    "DIP-SW1:10"                        },
	{ 0x42, 1,    0x02, 0x02, "Off"                               },
	{ 0x42, 1,    0x02, 0x00, "On"                                },

	{ 0,    0xfe, 0,    4,    "Computer Strength"                 },
	{ 0x42, 1,    0x0c, 0x00, "Weak"                              },
	{ 0x42, 1,    0x0c, 0x04, "Normal"                            },
	{ 0x42, 1,    0x0c, 0x08, "Somewhat strong"                   },
	{ 0x42, 1,    0x0c, 0x0c, "Strong"                            },

	{ 0,    0xfe, 0,    2,    "Game Style"                        },
	{ 0x42, 1,    0x10, 0x10, "Credit"                            },
	{ 0x42, 1,    0x10, 0x00, "Credit timer"                      },

	{ 0,    0xfe, 0,    2,    "Credit Timer Start Method"         },
	{ 0x42, 1,    0x20, 0x20, "Normal"                            },
	{ 0x42, 1,    0x20, 0x00, "Fixed minimum bet"                 },

	{ 0,    0xfe, 0,    2,    "Bets?"                             },
	{ 0x44, 1,    0x40, 0x40, "0"                                 },
	{ 0x44, 1,    0x40, 0x00, "1"                                 },

	{ 0,    0xfe, 0,    2,    "Unknown"                           },
	{ 0x45, 1,    0x40, 0x40, "Off"                               },
	{ 0x45, 1,    0x40, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Event Mode"                        },
	{ 0x41, 1,    0x20, 0x20, "Off"                               },
	{ 0x41, 1,    0x20, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Challenge Mode"                    },
	{ 0x41, 1,    0x40, 0x40, "Off"                               },
	{ 0x41, 1,    0x40, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Slump Support"                     },
	{ 0x41, 1,    0x80, 0x80, "Off"                               },
	{ 0x41, 1,    0x80, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Payout Rate Setting"               },
	{ 0x42, 1,    0x80, 0x80, "Off"                               },
	{ 0x42, 1,    0x80, 0x00, "On"                                },
};

STDDIPINFO(mjkokryu)

static struct BurnInputInfo mjmyorn2InputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "Bill1",          BIT_DIGITAL,   Joy[2] + 5,          "bill1"           },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "Service",        BIT_DIGITAL,   Joy[2] + 7,          "service"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjmyorn2)

static struct BurnDIPInfo mjmyorn2DIPList[] = {
	{ 0x3e, 0xff, 0xff, 0x47, NULL                         },
	{ 0x3f, 0xff, 0xff, 0x4f, NULL                         },
	{ 0x40, 0xff, 0xff, 0x07, NULL                         },
	{ 0x41, 0xff, 0xff, 0xc0, NULL                         },
	{ 0x42, 0xff, 0xff, 0x83, NULL                         },
	{ 0x43, 0xff, 0xff, 0xff, NULL                         },
	{ 0x44, 0xff, 0xff, 0xff, NULL                         },
	{ 0x45, 0xff, 0xff, 0xff, NULL                         },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                },
	{ 0x3e, 1,    0x0f, 0x00, "50%"                        },
	{ 0x3e, 1,    0x0f, 0x01, "53%"                        },
	{ 0x3e, 1,    0x0f, 0x02, "56%"                        },
	{ 0x3e, 1,    0x0f, 0x03, "59%"                        },
	{ 0x3e, 1,    0x0f, 0x04, "62%"                        },
	{ 0x3e, 1,    0x0f, 0x05, "65%"                        },
	{ 0x3e, 1,    0x0f, 0x06, "68%"                        },
	{ 0x3e, 1,    0x0f, 0x07, "71%"                        },
	{ 0x3e, 1,    0x0f, 0x08, "75%"                        },
	{ 0x3e, 1,    0x0f, 0x09, "78%"                        },
	{ 0x3e, 1,    0x0f, 0x0a, "81%"                        },
	{ 0x3e, 1,    0x0f, 0x0b, "84%"                        },
	{ 0x3e, 1,    0x0f, 0x0c, "87%"                        },
	{ 0x3e, 1,    0x0f, 0x0d, "90%"                        },
	{ 0x3e, 1,    0x0f, 0x0e, "93%"                        },
	{ 0x3e, 1,    0x0f, 0x0f, "96%"                        },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                  },
	{ 0x3e, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"        },
	{ 0x3e, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"         },
	{ 0x3e, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"        },
	{ 0x3e, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"       },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                },
	{ 0x3e, 1,    0xc0, 0xc0, "1"                          },
	{ 0x3e, 1,    0xc0, 0x80, "5"                          },
	{ 0x3e, 1,    0xc0, 0x40, "10"                         },
	{ 0x3e, 1,    0xc0, 0x00, "20"                         },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                },
	{ 0x3f, 1,    0x0c, 0x0c, "1"                          },
	{ 0x3f, 1,    0x0c, 0x08, "2"                          },
	{ 0x3f, 1,    0x0c, 0x04, "3"                          },
	{ 0x3f, 1,    0x0c, 0x00, "5"                          },

	{ 0,    0xfe, 0,    8,    "Bonus Chance Cycle"         },
	{ 0x3f, 1,    0x70, 0x70, "None"                       },
	{ 0x3f, 1,    0x70, 0x60, "First time only"            },
	{ 0x3f, 1,    0x70, 0x50, "Every 300 coins"            },
	{ 0x3f, 1,    0x70, 0x40, "Every 500 coins"            },
	{ 0x3f, 1,    0x70, 0x30, "Every 700 coins"            },
	{ 0x3f, 1,    0x70, 0x20, "Every 1000 coins"           },
	{ 0x3f, 1,    0x70, 0x10, "Every 1000 coins"           },
	{ 0x3f, 1,    0x70, 0x00, "Every 1000 coins"           },

	{ 0,    0xfe, 0,    2,    "Chances Per Cycle"          },
	{ 0x3f, 1,    0x80, 0x00, "1"                          },
	{ 0x3f, 1,    0x80, 0x80, "2"                          },

	{ 0,    0xfe, 0,    3,    "Game Mode"                  },
	{ 0x40, 1,    0x03, 0x03, "Key-out"                    },
	{ 0x40, 1,    0x03, 0x02, "Hopper payout"              },
	{ 0x40, 1,    0x03, 0x01, "Key-out with credit timer"  },

	{ 0,    0xfe, 0,    2,    "Hopper Polarity"            },
	{ 0x40, 1,    0x04, 0x04, "Normal"                     },
	{ 0x40, 1,    0x04, 0x00, "Inverted"                   },

	{ 0,    0xfe, 0,    4,    "Credit Limit"               },
	{ 0x40, 1,    0x18, 0x18, "300"                        },
	{ 0x40, 1,    0x18, 0x10, "500"                        },
	{ 0x40, 1,    0x18, 0x08, "700"                        },
	{ 0x40, 1,    0x18, 0x00, "1000"                       },

	{ 0,    0xfe, 0,    2,    "Computer Strength"          },
	{ 0x40, 1,    0x20, 0x00, "Normal"                     },
	{ 0x40, 1,    0x20, 0x20, "Strong"                     },

	{ 0,    0xfe, 0,    2,    "Double Bet"                 },
	{ 0x40, 1,    0x40, 0x40, "Off"                        },
	{ 0x40, 1,    0x40, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Last Chance"                },
	{ 0x40, 1,    0x80, 0x80, "Off"                        },
	{ 0x40, 1,    0x80, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "SW 4:1"                     },
	{ 0x41, 1,    0x01, 0x01, "Off"                        },
	{ 0x41, 1,    0x01, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "In-Game Music"              },
	{ 0x41, 1,    0x02, 0x02, "Off"                        },
	{ 0x41, 1,    0x02, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Show Gals"                  },
	{ 0x41, 1,    0x04, 0x04, "Off"                        },
	{ 0x41, 1,    0x04, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Gal Select"                 },
	{ 0x41, 1,    0x08, 0x08, "Off"                        },
	{ 0x41, 1,    0x08, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Show Game Action"           },
	{ 0x41, 1,    0x10, 0x10, "Off"                        },
	{ 0x41, 1,    0x10, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "In-Game Mat"                },
	{ 0x41, 1,    0x20, 0x00, "Plain"                      },
	{ 0x41, 1,    0x20, 0x20, "With Character"             },

	{ 0,    0xfe, 0,    2,    "Time Settings Mode"         },
	{ 0x41, 1,    0x40, 0x40, "Off"                        },
	{ 0x41, 1,    0x40, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                 },
	{ 0x42, 1,    0x04, 0x04, "Off"                        },
	{ 0x42, 1,    0x04, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Don Den Button"             },
	{ 0x42, 1,    0x08, 0x08, "Start"                      },
	{ 0x42, 1,    0x08, 0x00, "Flip Flop"                  },

	{ 0,    0xfe, 0,    4,    "Yakuman/Fever Chance Ratio" },
	{ 0x42, 1,    0x30, 0x30, "Yakuman Chance Only"        },
	{ 0x42, 1,    0x30, 0x20, "Equal Yakuman/Fever Chance" },
	{ 0x42, 1,    0x30, 0x10, "Frequent Fever Chance"      },
	{ 0x42, 1,    0x30, 0x00, "Fever Chance Only"          },

	{ 0,    0xfe, 0,    12,   "Credits Per Note"           },
	{ 0x42, 1,    0x40, 0x40, "5"                          },
	{ 0x42, 1,    0x40, 0x40, "10"                         },
	{ 0x42, 1,    0x40, 0x40, "25"                         },
	{ 0x42, 1,    0x40, 0x40, "50"                         },
	{ 0x42, 1,    0x40, 0x40, "10"                         },
	{ 0x42, 1,    0x40, 0x40, "15"                         },
	{ 0x42, 1,    0x40, 0x00, "10"                         },
	{ 0x42, 1,    0x40, 0x00, "20"                         },
	{ 0x42, 1,    0x40, 0x00, "50"                         },
	{ 0x42, 1,    0x40, 0x00, "100"                        },
	{ 0x42, 1,    0x40, 0x00, "20"                         },
	{ 0x42, 1,    0x40, 0x00, "30"                         },

	{ 0,    0xfe, 0,    2,    "SW 1:10"                    },
	{ 0x42, 1,    0x80, 0x80, "Off"                        },
	{ 0x42, 1,    0x80, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Alternate Coinage"          },
	{ 0x43, 1,    0x01, 0x01, "Off"                        },
	{ 0x43, 1,    0x01, 0x00, "On"                         },

	{ 0,    0xfe, 0,    8,    "SW 2:1,2"                   },
	{ 0x43, 1,    0x06, 0x06, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x04, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x02, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x00, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x02, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x06, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x00, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x04, "DSW5"                       },

	{ 0,    0xfe, 0,    14,   "Credits Per Note"           },
	{ 0x42, 1,    0x40, 0x40, "5"                          },
	{ 0x42, 1,    0x40, 0x40, "10"                         },
	{ 0x42, 1,    0x40, 0x40, "25"                         },
	{ 0x42, 1,    0x40, 0x40, "50"                         },
	{ 0x42, 1,    0x40, 0x40, "5"                          },
	{ 0x42, 1,    0x40, 0x40, "10"                         },
	{ 0x42, 1,    0x40, 0x40, "15"                         },
	{ 0x42, 1,    0x40, 0x00, "10"                         },
	{ 0x42, 1,    0x40, 0x00, "20"                         },
	{ 0x42, 1,    0x40, 0x00, "50"                         },
	{ 0x42, 1,    0x40, 0x00, "100"                        },
	{ 0x42, 1,    0x40, 0x00, "10"                         },
	{ 0x42, 1,    0x40, 0x00, "20"                         },
	{ 0x42, 1,    0x40, 0x00, "30"                         },

	{ 0,    0xfe, 0,    8,    "SW 2:1,2"                   },
	{ 0x43, 1,    0x06, 0x06, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x04, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x02, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x00, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x04, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x02, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x06, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x00, "DSW5"                       },
};

STDDIPINFO(mjmyorn2)

static struct BurnInputInfo mjmyorntInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "Bill1",          BIT_DIGITAL,   Joy[2] + 5,          "bill1"           },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "Service",        BIT_DIGITAL,   Joy[2] + 7,          "service"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjmyornt)

static struct BurnDIPInfo mjmyorntDIPList[] = {
	{ 0x3e, 0xff, 0xff, 0x47, NULL                         },
	{ 0x3f, 0xff, 0xff, 0x4f, NULL                         },
	{ 0x40, 0xff, 0xff, 0x07, NULL                         },
	{ 0x41, 0xff, 0xff, 0xc0, NULL                         },
	{ 0x42, 0xff, 0xff, 0x83, NULL                         },
	{ 0x43, 0xff, 0xff, 0xff, NULL                         },
	{ 0x44, 0xff, 0xff, 0xff, NULL                         },
	{ 0x45, 0xff, 0xff, 0xff, NULL                         },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                },
	{ 0x3e, 1,    0x0f, 0x00, "50%"                        },
	{ 0x3e, 1,    0x0f, 0x01, "53%"                        },
	{ 0x3e, 1,    0x0f, 0x02, "56%"                        },
	{ 0x3e, 1,    0x0f, 0x03, "59%"                        },
	{ 0x3e, 1,    0x0f, 0x04, "62%"                        },
	{ 0x3e, 1,    0x0f, 0x05, "65%"                        },
	{ 0x3e, 1,    0x0f, 0x06, "68%"                        },
	{ 0x3e, 1,    0x0f, 0x07, "71%"                        },
	{ 0x3e, 1,    0x0f, 0x08, "75%"                        },
	{ 0x3e, 1,    0x0f, 0x09, "78%"                        },
	{ 0x3e, 1,    0x0f, 0x0a, "81%"                        },
	{ 0x3e, 1,    0x0f, 0x0b, "84%"                        },
	{ 0x3e, 1,    0x0f, 0x0c, "87%"                        },
	{ 0x3e, 1,    0x0f, 0x0d, "90%"                        },
	{ 0x3e, 1,    0x0f, 0x0e, "93%"                        },
	{ 0x3e, 1,    0x0f, 0x0f, "96%"                        },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                  },
	{ 0x3e, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"        },
	{ 0x3e, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"         },
	{ 0x3e, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"        },
	{ 0x3e, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"       },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                },
	{ 0x3e, 1,    0xc0, 0xc0, "1"                          },
	{ 0x3e, 1,    0xc0, 0x80, "5"                          },
	{ 0x3e, 1,    0xc0, 0x40, "10"                         },
	{ 0x3e, 1,    0xc0, 0x00, "20"                         },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                },
	{ 0x3f, 1,    0x0c, 0x0c, "1"                          },
	{ 0x3f, 1,    0x0c, 0x08, "2"                          },
	{ 0x3f, 1,    0x0c, 0x04, "3"                          },
	{ 0x3f, 1,    0x0c, 0x00, "5"                          },

	{ 0,    0xfe, 0,    8,    "Bonus Chance Cycle"         },
	{ 0x3f, 1,    0x70, 0x70, "None"                       },
	{ 0x3f, 1,    0x70, 0x60, "First time only"            },
	{ 0x3f, 1,    0x70, 0x50, "Every 300 coins"            },
	{ 0x3f, 1,    0x70, 0x40, "Every 500 coins"            },
	{ 0x3f, 1,    0x70, 0x30, "Every 700 coins"            },
	{ 0x3f, 1,    0x70, 0x20, "Every 1000 coins"           },
	{ 0x3f, 1,    0x70, 0x10, "Every 1000 coins"           },
	{ 0x3f, 1,    0x70, 0x00, "Every 1000 coins"           },

	{ 0,    0xfe, 0,    2,    "Chances Per Cycle"          },
	{ 0x3f, 1,    0x80, 0x00, "1"                          },
	{ 0x3f, 1,    0x80, 0x80, "2"                          },

	{ 0,    0xfe, 0,    3,    "Game Mode"                  },
	{ 0x40, 1,    0x03, 0x03, "Key-out"                    },
	{ 0x40, 1,    0x03, 0x02, "Hopper payout"              },
	{ 0x40, 1,    0x03, 0x01, "Key-out with credit timer"  },

	{ 0,    0xfe, 0,    2,    "Hopper Polarity"            },
	{ 0x40, 1,    0x04, 0x04, "Normal"                     },
	{ 0x40, 1,    0x04, 0x00, "Inverted"                   },

	{ 0,    0xfe, 0,    4,    "Credit Limit"               },
	{ 0x40, 1,    0x18, 0x18, "300"                        },
	{ 0x40, 1,    0x18, 0x10, "500"                        },
	{ 0x40, 1,    0x18, 0x08, "700"                        },
	{ 0x40, 1,    0x18, 0x00, "1000"                       },

	{ 0,    0xfe, 0,    2,    "Computer Strength"          },
	{ 0x40, 1,    0x20, 0x00, "Normal"                     },
	{ 0x40, 1,    0x20, 0x20, "Strong"                     },

	{ 0,    0xfe, 0,    2,    "Double Bet"                 },
	{ 0x40, 1,    0x40, 0x40, "Off"                        },
	{ 0x40, 1,    0x40, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Last Chance"                },
	{ 0x40, 1,    0x80, 0x80, "Off"                        },
	{ 0x40, 1,    0x80, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "SW 4:1"                     },
	{ 0x41, 1,    0x01, 0x01, "Off"                        },
	{ 0x41, 1,    0x01, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "In-Game Music"              },
	{ 0x41, 1,    0x02, 0x02, "Off"                        },
	{ 0x41, 1,    0x02, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Show Gals"                  },
	{ 0x41, 1,    0x04, 0x04, "Off"                        },
	{ 0x41, 1,    0x04, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Gal Select"                 },
	{ 0x41, 1,    0x08, 0x08, "Off"                        },
	{ 0x41, 1,    0x08, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Show Game Action"           },
	{ 0x41, 1,    0x10, 0x10, "Off"                        },
	{ 0x41, 1,    0x10, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "In-Game Mat"                },
	{ 0x41, 1,    0x20, 0x00, "Plain"                      },
	{ 0x41, 1,    0x20, 0x20, "With Character"             },

	{ 0,    0xfe, 0,    2,    "Time Settings Mode"         },
	{ 0x41, 1,    0x40, 0x40, "Off"                        },
	{ 0x41, 1,    0x40, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                 },
	{ 0x42, 1,    0x04, 0x04, "Off"                        },
	{ 0x42, 1,    0x04, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Don Den Button"             },
	{ 0x42, 1,    0x08, 0x08, "Start"                      },
	{ 0x42, 1,    0x08, 0x00, "Flip Flop"                  },

	{ 0,    0xfe, 0,    4,    "Yakuman/Fever Chance Ratio" },
	{ 0x42, 1,    0x30, 0x30, "Yakuman Chance Only"        },
	{ 0x42, 1,    0x30, 0x20, "Equal Yakuman/Fever Chance" },
	{ 0x42, 1,    0x30, 0x10, "Frequent Fever Chance"      },
	{ 0x42, 1,    0x30, 0x00, "Fever Chance Only"          },

	{ 0,    0xfe, 0,    12,   "Credits Per Note"           },
	{ 0x42, 1,    0x40, 0x40, "5"                          },
	{ 0x42, 1,    0x40, 0x40, "10"                         },
	{ 0x42, 1,    0x40, 0x40, "25"                         },
	{ 0x42, 1,    0x40, 0x40, "50"                         },
	{ 0x42, 1,    0x40, 0x40, "10"                         },
	{ 0x42, 1,    0x40, 0x40, "15"                         },
	{ 0x42, 1,    0x40, 0x00, "10"                         },
	{ 0x42, 1,    0x40, 0x00, "20"                         },
	{ 0x42, 1,    0x40, 0x00, "50"                         },
	{ 0x42, 1,    0x40, 0x00, "100"                        },
	{ 0x42, 1,    0x40, 0x00, "20"                         },
	{ 0x42, 1,    0x40, 0x00, "30"                         },

	{ 0,    0xfe, 0,    2,    "SW 1:10"                    },
	{ 0x42, 1,    0x80, 0x80, "Off"                        },
	{ 0x42, 1,    0x80, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Alternate Coinage"          },
	{ 0x43, 1,    0x01, 0x01, "Off"                        },
	{ 0x43, 1,    0x01, 0x00, "On"                         },

	{ 0,    0xfe, 0,    8,    "SW 2:1,2"                   },
	{ 0x43, 1,    0x06, 0x06, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x04, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x02, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x00, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x02, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x06, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x00, "DSW5"                       },
	{ 0x43, 1,    0x06, 0x04, "DSW5"                       },
};

STDDIPINFO(mjmyornt)

static struct BurnInputInfo mjmysterInputList[] = {
	{ "Gamble Keyout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble keyout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "Bill1",          BIT_DIGITAL,   Joy[2] + 5,          "bill1"           },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjmyster)

static struct BurnDIPInfo mjmysterDIPList[] = {
	{ 0x3d, 0xff, 0xff, 0x47, NULL                         },
	{ 0x3e, 0xff, 0xff, 0x4f, NULL                         },
	{ 0x3f, 0xff, 0xff, 0x07, NULL                         },
	{ 0x40, 0xff, 0xff, 0xe0, NULL                         },
	{ 0x41, 0xff, 0xff, 0xa3, NULL                         },
	{ 0x42, 0xff, 0xff, 0xff, NULL                         },
	{ 0x43, 0xff, 0xff, 0xff, NULL                         },
	{ 0x44, 0xff, 0xff, 0xff, NULL                         },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                },
	{ 0x3d, 1,    0x0f, 0x00, "50%"                        },
	{ 0x3d, 1,    0x0f, 0x01, "53%"                        },
	{ 0x3d, 1,    0x0f, 0x02, "56%"                        },
	{ 0x3d, 1,    0x0f, 0x03, "59%"                        },
	{ 0x3d, 1,    0x0f, 0x04, "62%"                        },
	{ 0x3d, 1,    0x0f, 0x05, "65%"                        },
	{ 0x3d, 1,    0x0f, 0x06, "68%"                        },
	{ 0x3d, 1,    0x0f, 0x07, "71%"                        },
	{ 0x3d, 1,    0x0f, 0x08, "75%"                        },
	{ 0x3d, 1,    0x0f, 0x09, "78%"                        },
	{ 0x3d, 1,    0x0f, 0x0a, "81%"                        },
	{ 0x3d, 1,    0x0f, 0x0b, "84%"                        },
	{ 0x3d, 1,    0x0f, 0x0c, "87%"                        },
	{ 0x3d, 1,    0x0f, 0x0d, "90%"                        },
	{ 0x3d, 1,    0x0f, 0x0e, "93%"                        },
	{ 0x3d, 1,    0x0f, 0x0f, "96%"                        },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                  },
	{ 0x3d, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"        },
	{ 0x3d, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"         },
	{ 0x3d, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"        },
	{ 0x3d, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"       },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                },
	{ 0x3d, 1,    0xc0, 0xc0, "1"                          },
	{ 0x3d, 1,    0xc0, 0x80, "5"                          },
	{ 0x3d, 1,    0xc0, 0x40, "10"                         },
	{ 0x3d, 1,    0xc0, 0x00, "20"                         },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                },
	{ 0x3e, 1,    0x0c, 0x0c, "1"                          },
	{ 0x3e, 1,    0x0c, 0x08, "2"                          },
	{ 0x3e, 1,    0x0c, 0x04, "3"                          },
	{ 0x3e, 1,    0x0c, 0x00, "5"                          },

	{ 0,    0xfe, 0,    8,    "Bonus Chance Cycle"         },
	{ 0x3e, 1,    0x70, 0x70, "None"                       },
	{ 0x3e, 1,    0x70, 0x60, "First time only"            },
	{ 0x3e, 1,    0x70, 0x50, "Every 300 coins"            },
	{ 0x3e, 1,    0x70, 0x40, "Every 500 coins"            },
	{ 0x3e, 1,    0x70, 0x30, "Every 700 coins"            },
	{ 0x3e, 1,    0x70, 0x20, "Every 1000 coins"           },
	{ 0x3e, 1,    0x70, 0x10, "Every 1000 coins"           },
	{ 0x3e, 1,    0x70, 0x00, "Every 1000 coins"           },

	{ 0,    0xfe, 0,    2,    "Chances Per Cycle"          },
	{ 0x3e, 1,    0x80, 0x00, "1"                          },
	{ 0x3e, 1,    0x80, 0x80, "2"                          },

	{ 0,    0xfe, 0,    3,    "Game Mode"                  },
	{ 0x3f, 1,    0x03, 0x03, "Key-out"                    },
	{ 0x3f, 1,    0x03, 0x02, "Hopper payout"              },
	{ 0x3f, 1,    0x03, 0x01, "Points with credit timer"   },

	{ 0,    0xfe, 0,    2,    "Hopper Polarity"            },
	{ 0x3f, 1,    0x04, 0x04, "Normal"                     },
	{ 0x3f, 1,    0x04, 0x00, "Inverted"                   },

	{ 0,    0xfe, 0,    4,    "Credit Limit"               },
	{ 0x3f, 1,    0x18, 0x18, "300"                        },
	{ 0x3f, 1,    0x18, 0x10, "500"                        },
	{ 0x3f, 1,    0x18, 0x08, "700"                        },
	{ 0x3f, 1,    0x18, 0x00, "1000"                       },

	{ 0,    0xfe, 0,    2,    "Computer Strength"          },
	{ 0x3f, 1,    0x20, 0x00, "Normal"                     },
	{ 0x3f, 1,    0x20, 0x20, "Strong"                     },

	{ 0,    0xfe, 0,    2,    "Double Bet"                 },
	{ 0x3f, 1,    0x40, 0x40, "Off"                        },
	{ 0x3f, 1,    0x40, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Last Chance"                },
	{ 0x3f, 1,    0x80, 0x80, "Off"                        },
	{ 0x3f, 1,    0x80, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "SW4:1"                      },
	{ 0x40, 1,    0x01, 0x01, "Off"                        },
	{ 0x40, 1,    0x01, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "In-Game Music"              },
	{ 0x40, 1,    0x02, 0x02, "Off"                        },
	{ 0x40, 1,    0x02, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Show Gals"                  },
	{ 0x40, 1,    0x04, 0x04, "Off"                        },
	{ 0x40, 1,    0x04, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Show Game Action"           },
	{ 0x40, 1,    0x08, 0x08, "Off"                        },
	{ 0x40, 1,    0x08, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Fever Payout Rate"          },
	{ 0x40, 1,    0x10, 0x10, "Low"                        },
	{ 0x40, 1,    0x10, 0x00, "Normal"                     },

	{ 0,    0xfe, 0,    2,    "SW4:7"                      },
	{ 0x40, 1,    0x40, 0x40, "Off"                        },
	{ 0x40, 1,    0x40, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "SW4:8"                      },
	{ 0x40, 1,    0x80, 0x80, "Japan"                      },
	{ 0x40, 1,    0x80, 0x00, "Hong Kong"                  },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                 },
	{ 0x41, 1,    0x04, 0x04, "Off"                        },
	{ 0x41, 1,    0x04, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Don Den Button"             },
	{ 0x41, 1,    0x08, 0x08, "Start"                      },
	{ 0x41, 1,    0x08, 0x00, "Flip Flop"                  },

	{ 0,    0xfe, 0,    4,    "Yakuman/Fever Chance Ratio" },
	{ 0x41, 1,    0x30, 0x30, "Yakuman Chance Only"        },
	{ 0x41, 1,    0x30, 0x20, "Equal Yakuman/Fever Chance" },
	{ 0x41, 1,    0x30, 0x10, "Frequent Fever Chance"      },
	{ 0x41, 1,    0x30, 0x00, "Fever Chance Only"          },

	{ 0,    0xfe, 0,    14,   "Credits Per Note"           },
	{ 0x41, 1,    0x40, 0x40, "5"                          },
	{ 0x41, 1,    0x40, 0x40, "10"                         },
	{ 0x41, 1,    0x40, 0x40, "25"                         },
	{ 0x41, 1,    0x40, 0x40, "50"                         },
	{ 0x41, 1,    0x40, 0x40, "5"                          },
	{ 0x41, 1,    0x40, 0x40, "10"                         },
	{ 0x41, 1,    0x40, 0x40, "15"                         },
	{ 0x41, 1,    0x40, 0x00, "10"                         },
	{ 0x41, 1,    0x40, 0x00, "20"                         },
	{ 0x41, 1,    0x40, 0x00, "50"                         },
	{ 0x41, 1,    0x40, 0x00, "100"                        },
	{ 0x41, 1,    0x40, 0x00, "10"                         },
	{ 0x41, 1,    0x40, 0x00, "20"                         },
	{ 0x41, 1,    0x40, 0x00, "30"                         },

	{ 0,    0xfe, 0,    2,    "SW1:10"                     },
	{ 0x41, 1,    0x80, 0x80, "Off"                        },
	{ 0x41, 1,    0x80, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Alternate Coinage"          },
	{ 0x42, 1,    0x01, 0x01, "Off"                        },
	{ 0x42, 1,    0x01, 0x00, "On"                         },

	{ 0,    0xfe, 0,    8,    "SW2:1,2"                    },
	{ 0x42, 1,    0x06, 0x06, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x04, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x02, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x00, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x04, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x02, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x06, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x00, "FAKE"                       },
};

STDDIPINFO(mjmyster)

static struct BurnInputInfo mjmyunivhInputList[] = {
	{ "Gamble Keyout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble keyout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "Bill1",          BIT_DIGITAL,   Joy[2] + 5,          "bill1"           },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjmyunivh)

static struct BurnDIPInfo mjmyunivhDIPList[] = {
	{ 0x3d, 0xff, 0xff, 0x47, NULL                         },
	{ 0x3e, 0xff, 0xff, 0x4f, NULL                         },
	{ 0x3f, 0xff, 0xff, 0x07, NULL                         },
	{ 0x40, 0xff, 0xff, 0x60, NULL                         },
	{ 0x41, 0xff, 0xff, 0xa3, NULL                         },
	{ 0x42, 0xff, 0xff, 0xff, NULL                         },
	{ 0x43, 0xff, 0xff, 0xff, NULL                         },
	{ 0x44, 0xff, 0xff, 0xff, NULL                         },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                },
	{ 0x3d, 1,    0x0f, 0x00, "50%"                        },
	{ 0x3d, 1,    0x0f, 0x01, "53%"                        },
	{ 0x3d, 1,    0x0f, 0x02, "56%"                        },
	{ 0x3d, 1,    0x0f, 0x03, "59%"                        },
	{ 0x3d, 1,    0x0f, 0x04, "62%"                        },
	{ 0x3d, 1,    0x0f, 0x05, "65%"                        },
	{ 0x3d, 1,    0x0f, 0x06, "68%"                        },
	{ 0x3d, 1,    0x0f, 0x07, "71%"                        },
	{ 0x3d, 1,    0x0f, 0x08, "75%"                        },
	{ 0x3d, 1,    0x0f, 0x09, "78%"                        },
	{ 0x3d, 1,    0x0f, 0x0a, "81%"                        },
	{ 0x3d, 1,    0x0f, 0x0b, "84%"                        },
	{ 0x3d, 1,    0x0f, 0x0c, "87%"                        },
	{ 0x3d, 1,    0x0f, 0x0d, "90%"                        },
	{ 0x3d, 1,    0x0f, 0x0e, "93%"                        },
	{ 0x3d, 1,    0x0f, 0x0f, "96%"                        },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                  },
	{ 0x3d, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"        },
	{ 0x3d, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"         },
	{ 0x3d, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"        },
	{ 0x3d, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"       },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                },
	{ 0x3d, 1,    0xc0, 0xc0, "1"                          },
	{ 0x3d, 1,    0xc0, 0x80, "5"                          },
	{ 0x3d, 1,    0xc0, 0x40, "10"                         },
	{ 0x3d, 1,    0xc0, 0x00, "20"                         },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                },
	{ 0x3e, 1,    0x0c, 0x0c, "1"                          },
	{ 0x3e, 1,    0x0c, 0x08, "2"                          },
	{ 0x3e, 1,    0x0c, 0x04, "3"                          },
	{ 0x3e, 1,    0x0c, 0x00, "5"                          },

	{ 0,    0xfe, 0,    8,    "Bonus Chance Cycle"         },
	{ 0x3e, 1,    0x70, 0x70, "None"                       },
	{ 0x3e, 1,    0x70, 0x60, "First time only"            },
	{ 0x3e, 1,    0x70, 0x50, "Every 300 coins"            },
	{ 0x3e, 1,    0x70, 0x40, "Every 500 coins"            },
	{ 0x3e, 1,    0x70, 0x30, "Every 700 coins"            },
	{ 0x3e, 1,    0x70, 0x20, "Every 1000 coins"           },
	{ 0x3e, 1,    0x70, 0x10, "Every 1000 coins"           },
	{ 0x3e, 1,    0x70, 0x00, "Every 1000 coins"           },

	{ 0,    0xfe, 0,    2,    "Chances Per Cycle"          },
	{ 0x3e, 1,    0x80, 0x00, "1"                          },
	{ 0x3e, 1,    0x80, 0x80, "2"                          },

	{ 0,    0xfe, 0,    3,    "Game Mode"                  },
	{ 0x3f, 1,    0x03, 0x03, "Key-out"                    },
	{ 0x3f, 1,    0x03, 0x02, "Hopper payout"              },
	{ 0x3f, 1,    0x03, 0x01, "Points with credit timer"   },

	{ 0,    0xfe, 0,    2,    "Hopper Polarity"            },
	{ 0x3f, 1,    0x04, 0x04, "Normal"                     },
	{ 0x3f, 1,    0x04, 0x00, "Inverted"                   },

	{ 0,    0xfe, 0,    4,    "Credit Limit"               },
	{ 0x3f, 1,    0x18, 0x18, "300"                        },
	{ 0x3f, 1,    0x18, 0x10, "500"                        },
	{ 0x3f, 1,    0x18, 0x08, "700"                        },
	{ 0x3f, 1,    0x18, 0x00, "1000"                       },

	{ 0,    0xfe, 0,    2,    "Computer Strength"          },
	{ 0x3f, 1,    0x20, 0x00, "Normal"                     },
	{ 0x3f, 1,    0x20, 0x20, "Strong"                     },

	{ 0,    0xfe, 0,    2,    "Double Bet"                 },
	{ 0x3f, 1,    0x40, 0x40, "Off"                        },
	{ 0x3f, 1,    0x40, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Last Chance"                },
	{ 0x3f, 1,    0x80, 0x80, "Off"                        },
	{ 0x3f, 1,    0x80, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "SW4:1"                      },
	{ 0x40, 1,    0x01, 0x01, "Off"                        },
	{ 0x40, 1,    0x01, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "In-Game Music"              },
	{ 0x40, 1,    0x02, 0x02, "Off"                        },
	{ 0x40, 1,    0x02, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Show Gals"                  },
	{ 0x40, 1,    0x04, 0x04, "Off"                        },
	{ 0x40, 1,    0x04, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Show Game Action"           },
	{ 0x40, 1,    0x08, 0x08, "Off"                        },
	{ 0x40, 1,    0x08, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Fever Payout Rate"          },
	{ 0x40, 1,    0x10, 0x10, "Low"                        },
	{ 0x40, 1,    0x10, 0x00, "Normal"                     },

	{ 0,    0xfe, 0,    2,    "SW4:7"                      },
	{ 0x40, 1,    0x40, 0x40, "Off"                        },
	{ 0x40, 1,    0x40, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "SW4:8"                      },
	{ 0x40, 1,    0x80, 0x80, "Japan"                      },
	{ 0x40, 1,    0x80, 0x00, "Hong Kong"                  },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                 },
	{ 0x41, 1,    0x04, 0x04, "Off"                        },
	{ 0x41, 1,    0x04, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Don Den Button"             },
	{ 0x41, 1,    0x08, 0x08, "Start"                      },
	{ 0x41, 1,    0x08, 0x00, "Flip Flop"                  },

	{ 0,    0xfe, 0,    4,    "Yakuman/Fever Chance Ratio" },
	{ 0x41, 1,    0x30, 0x30, "Yakuman Chance Only"        },
	{ 0x41, 1,    0x30, 0x20, "Equal Yakuman/Fever Chance" },
	{ 0x41, 1,    0x30, 0x10, "Frequent Fever Chance"      },
	{ 0x41, 1,    0x30, 0x00, "Fever Chance Only"          },

	{ 0,    0xfe, 0,    14,   "Credits Per Note"           },
	{ 0x41, 1,    0x40, 0x40, "5"                          },
	{ 0x41, 1,    0x40, 0x40, "10"                         },
	{ 0x41, 1,    0x40, 0x40, "25"                         },
	{ 0x41, 1,    0x40, 0x40, "50"                         },
	{ 0x41, 1,    0x40, 0x40, "5"                          },
	{ 0x41, 1,    0x40, 0x40, "10"                         },
	{ 0x41, 1,    0x40, 0x40, "15"                         },
	{ 0x41, 1,    0x40, 0x00, "10"                         },
	{ 0x41, 1,    0x40, 0x00, "20"                         },
	{ 0x41, 1,    0x40, 0x00, "50"                         },
	{ 0x41, 1,    0x40, 0x00, "100"                        },
	{ 0x41, 1,    0x40, 0x00, "10"                         },
	{ 0x41, 1,    0x40, 0x00, "20"                         },
	{ 0x41, 1,    0x40, 0x00, "30"                         },

	{ 0,    0xfe, 0,    2,    "SW1:10"                     },
	{ 0x41, 1,    0x80, 0x80, "Off"                        },
	{ 0x41, 1,    0x80, 0x00, "On"                         },

	{ 0,    0xfe, 0,    2,    "Alternate Coinage"          },
	{ 0x42, 1,    0x01, 0x01, "Off"                        },
	{ 0x42, 1,    0x01, 0x00, "On"                         },

	{ 0,    0xfe, 0,    8,    "SW2:1,2"                    },
	{ 0x42, 1,    0x06, 0x06, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x04, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x02, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x00, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x04, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x02, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x06, "FAKE"                       },
	{ 0x42, 1,    0x06, 0x00, "FAKE"                       },

	{ 0,    0xfe, 0,    2,    "SW4:8"                      },
	{ 0x40, 1,    0x80, 0x80, "Japan"                      },
	{ 0x40, 1,    0x80, 0x00, "Hong Kong/Taiwan"           },
};

STDDIPINFO(mjmyunivh)

static struct BurnInputInfo mjnigiriInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P3 Coin",        BIT_DIGITAL,   Joy[2] + 7,          "p3 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjnigiri)

static struct BurnDIPInfo mjnigiriDIPList[] = {
	{ 0x3e, 0xff, 0xff, 0x46, NULL                                },
	{ 0x3f, 0xff, 0xff, 0x3f, NULL                                },
	{ 0x40, 0xff, 0xff, 0xdc, NULL                                },
	{ 0x41, 0xff, 0xff, 0xe1, NULL                                },
	{ 0x42, 0xff, 0xff, 0xfa, NULL                                },
	{ 0x43, 0xff, 0xff, 0xff, NULL                                },
	{ 0x44, 0xff, 0xff, 0xff, NULL                                },
	{ 0x45, 0xff, 0xff, 0xff, NULL                                },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                       },
	{ 0x3e, 1,    0x0f, 0x00, "50%"                               },
	{ 0x3e, 1,    0x0f, 0x01, "53%"                               },
	{ 0x3e, 1,    0x0f, 0x02, "56%"                               },
	{ 0x3e, 1,    0x0f, 0x03, "59%"                               },
	{ 0x3e, 1,    0x0f, 0x04, "62%"                               },
	{ 0x3e, 1,    0x0f, 0x05, "65%"                               },
	{ 0x3e, 1,    0x0f, 0x06, "68%"                               },
	{ 0x3e, 1,    0x0f, 0x07, "71%"                               },
	{ 0x3e, 1,    0x0f, 0x08, "75%"                               },
	{ 0x3e, 1,    0x0f, 0x09, "78%"                               },
	{ 0x3e, 1,    0x0f, 0x0a, "81%"                               },
	{ 0x3e, 1,    0x0f, 0x0b, "84%"                               },
	{ 0x3e, 1,    0x0f, 0x0c, "87%"                               },
	{ 0x3e, 1,    0x0f, 0x0d, "90%"                               },
	{ 0x3e, 1,    0x0f, 0x0e, "93%"                               },
	{ 0x3e, 1,    0x0f, 0x0f, "96%"                               },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                         },
	{ 0x3e, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"               },
	{ 0x3e, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"                },
	{ 0x3e, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"               },
	{ 0x3e, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"              },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                       },
	{ 0x3e, 1,    0xc0, 0xc0, "1"                                 },
	{ 0x3e, 1,    0xc0, 0x80, "5"                                 },
	{ 0x3e, 1,    0xc0, 0x40, "10"                                },
	{ 0x3e, 1,    0xc0, 0x00, "20"                                },

	{ 0,    0xfe, 0,    8,    "DIP-SW2:1,2,3"                     },
	{ 0x3f, 1,    0x07, 0x07, "1C 1C"                             },
	{ 0x3f, 1,    0x07, 0x06, "1C 2C"                             },
	{ 0x3f, 1,    0x07, 0x05, "1C 5C"                             },
	{ 0x3f, 1,    0x07, 0x04, "1C 10C"                            },
	{ 0x3f, 1,    0x07, 0x03, "1C 1C"                             },
	{ 0x3f, 1,    0x07, 0x02, "1C 2C"                             },
	{ 0x3f, 1,    0x07, 0x01, "1C 5C"                             },
	{ 0x3f, 1,    0x07, 0x00, "Free Play"                         },

	{ 0,    0xfe, 0,    8,    "Minimum Bet"                       },
	{ 0x3f, 1,    0x38, 0x38, "1"                                 },
	{ 0x3f, 1,    0x38, 0x30, "2"                                 },
	{ 0x3f, 1,    0x38, 0x28, "3"                                 },
	{ 0x3f, 1,    0x38, 0x20, "4"                                 },
	{ 0x3f, 1,    0x38, 0x18, "5"                                 },
	{ 0x3f, 1,    0x38, 0x10, "6"                                 },
	{ 0x3f, 1,    0x38, 0x08, "7"                                 },
	{ 0x3f, 1,    0x38, 0x00, "8"                                 },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                      },
	{ 0x3f, 1,    0xc0, 0xc0, "300"                               },
	{ 0x3f, 1,    0xc0, 0xc0, "600"                               },
	{ 0x3f, 1,    0xc0, 0xc0, "1500"                              },
	{ 0x3f, 1,    0xc0, 0xc0, "3000"                              },
	{ 0x3f, 1,    0xc0, 0x80, "500"                               },
	{ 0x3f, 1,    0xc0, 0x80, "1000"                              },
	{ 0x3f, 1,    0xc0, 0x80, "2500"                              },
	{ 0x3f, 1,    0xc0, 0x80, "5000"                              },
	{ 0x3f, 1,    0xc0, 0x40, "700"                               },
	{ 0x3f, 1,    0xc0, 0x40, "1400"                              },
	{ 0x3f, 1,    0xc0, 0x40, "3500"                              },
	{ 0x3f, 1,    0xc0, 0x40, "7000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "1000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "2000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "5000"                              },
	{ 0x3f, 1,    0xc0, 0x00, "10000"                             },

	{ 0,    0xfe, 0,    8,    "Yakuman Chance Cycle"              },
	{ 0x40, 1,    0x07, 0x07, "None"                              },
	{ 0x40, 1,    0x07, 0x06, "First time only"                   },
	{ 0x40, 1,    0x07, 0x05, "Every 300 coins"                   },
	{ 0x40, 1,    0x07, 0x04, "Every 500 coins"                   },
	{ 0x40, 1,    0x07, 0x03, "Every 700 coins"                   },
	{ 0x40, 1,    0x07, 0x02, "Every 1000 coins"                  },
	{ 0x40, 1,    0x07, 0x01, "Every 1500 coins"                  },
	{ 0x40, 1,    0x07, 0x00, "Every 2000 coins"                  },

	{ 0,    0xfe, 0,    3,    "Yakuman Chances Per Cycle"         },
	{ 0x40, 1,    0x18, 0x18, "1"                                 },
	{ 0x40, 1,    0x18, 0x10, "2"                                 },
	{ 0x40, 1,    0x18, 0x18, "3"                                 },

	{ 0,    0xfe, 0,    2,    "Allow Sanbaiman as Yakuman Chance" },
	{ 0x40, 1,    0x20, 0x20, "No"                                },
	{ 0x40, 1,    0x20, 0x00, "Yes"                               },

	{ 0,    0xfe, 0,    2,    "Medal Timer"                       },
	{ 0x40, 1,    0x80, 0x80, "Off"                               },
	{ 0x40, 1,    0x80, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "DIP-SW4:1"                         },
	{ 0x41, 1,    0x01, 0x01, "Off"                               },
	{ 0x41, 1,    0x01, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Don Den Button"                    },
	{ 0x41, 1,    0x02, 0x02, "Start"                             },
	{ 0x41, 1,    0x02, 0x00, "Flip Flop"                         },

	{ 0,    0xfe, 0,    2,    "Last Chance"                       },
	{ 0x41, 1,    0x04, 0x04, "Off"                               },
	{ 0x41, 1,    0x04, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                        },
	{ 0x41, 1,    0x08, 0x08, "Off"                               },
	{ 0x41, 1,    0x08, 0x00, "On"                                },

	{ 0,    0xfe, 0,    2,    "Double Bet"                        },
	{ 0x41, 1,    0x10, 0x10, "Off"                               },
	{ 0x41, 1,    0x10, 0x00, "On"                                },

	{ 0,    0xfe, 0,    8,    "Credits Per Note 1 / 2"            },
	{ 0x42, 1,    0x01, 0x01, "5 / 10"                            },
	{ 0x42, 1,    0x01, 0x01, "10 / 5"                            },
	{ 0x42, 1,    0x01, 0x01, "25 / 50"                           },
	{ 0x42, 1,    0x01, 0x01, "50 / 25"                           },
	{ 0x42, 1,    0x01, 0x00, "10 / 20"                           },
	{ 0x42, 1,    0x01, 0x00, "20 / 10"                           },
	{ 0x42, 1,    0x01, 0x00, "50 / 100"                          },
	{ 0x42, 1,    0x01, 0x00, "100 / 50"                          },

	{ 0,    0xfe, 0,    2,    "DIP-SW1:10"                        },
	{ 0x42, 1,    0x02, 0x02, "Off"                               },
	{ 0x42, 1,    0x02, 0x00, "On"                                },

	{ 0,    0xfe, 0,    4,    "Computer Strength"                 },
	{ 0x42, 1,    0x0c, 0x00, "Weak"                              },
	{ 0x42, 1,    0x0c, 0x04, "Normal"                            },
	{ 0x42, 1,    0x0c, 0x08, "Somewhat strong"                   },
	{ 0x42, 1,    0x0c, 0x0c, "Strong"                            },

	{ 0,    0xfe, 0,    2,    "Game Style"                        },
	{ 0x42, 1,    0x10, 0x10, "Credit"                            },
	{ 0x42, 1,    0x10, 0x00, "Credit timer"                      },

	{ 0,    0xfe, 0,    2,    "Credit Timer Start Method"         },
	{ 0x42, 1,    0x20, 0x20, "Normal"                            },
	{ 0x42, 1,    0x20, 0x00, "Fixed minimum bet"                 },

	{ 0,    0xfe, 0,    2,    "Bets?"                             },
	{ 0x44, 1,    0x40, 0x40, "0"                                 },
	{ 0x44, 1,    0x40, 0x00, "1"                                 },

	{ 0,    0xfe, 0,    2,    "Unknown"                           },
	{ 0x45, 1,    0x40, 0x40, "Off"                               },
	{ 0x45, 1,    0x40, 0x00, "On"                                },
};

STDDIPINFO(mjnigiri)

static struct BurnInputInfo mjreach1InputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjreach1)

static struct BurnDIPInfo mjreach1DIPList[] = {
	{ 0x3d, 0xff, 0xff, 0xf7, NULL                            },
	{ 0x3e, 0xff, 0xff, 0xff, NULL                            },
	{ 0x3f, 0xff, 0xff, 0xff, NULL                            },
	{ 0x40, 0xff, 0xff, 0xe8, NULL                            },
	{ 0x41, 0xff, 0xff, 0xff, NULL                            },
	{ 0x42, 0xff, 0xff, 0xff, NULL                            },
	{ 0x43, 0xff, 0xff, 0xff, NULL                            },
	{ 0x44, 0xff, 0xff, 0xff, NULL                            },

	{ 0,    0xfe, 0,    16,   "Pay Out Rate (%)"              },
	{ 0x3d, 1,    0x0f, 0x00, "50"                            },
	{ 0x3d, 1,    0x0f, 0x01, "53"                            },
	{ 0x3d, 1,    0x0f, 0x02, "56"                            },
	{ 0x3d, 1,    0x0f, 0x03, "59"                            },
	{ 0x3d, 1,    0x0f, 0x04, "62"                            },
	{ 0x3d, 1,    0x0f, 0x05, "65"                            },
	{ 0x3d, 1,    0x0f, 0x06, "68"                            },
	{ 0x3d, 1,    0x0f, 0x07, "71"                            },
	{ 0x3d, 1,    0x0f, 0x08, "75"                            },
	{ 0x3d, 1,    0x0f, 0x09, "78"                            },
	{ 0x3d, 1,    0x0f, 0x0a, "81"                            },
	{ 0x3d, 1,    0x0f, 0x0b, "84"                            },
	{ 0x3d, 1,    0x0f, 0x0c, "87"                            },
	{ 0x3d, 1,    0x0f, 0x0d, "90"                            },
	{ 0x3d, 1,    0x0f, 0x0e, "93"                            },
	{ 0x3d, 1,    0x0f, 0x0f, "96"                            },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                     },
	{ 0x3d, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"           },
	{ 0x3d, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"            },
	{ 0x3d, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"           },
	{ 0x3d, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"          },

	{ 0,    0xfe, 0,    4,    "Max Rate"                      },
	{ 0x3d, 1,    0xc0, 0xc0, "1"                             },
	{ 0x3d, 1,    0xc0, 0x80, "5"                             },
	{ 0x3d, 1,    0xc0, 0x40, "10"                            },
	{ 0x3d, 1,    0xc0, 0x00, "20"                            },

	{ 0,    0xfe, 0,    4,    "Coin A"                        },
	{ 0x3e, 1,    0x03, 0x03, "1C 1C"                         },
	{ 0x3e, 1,    0x03, 0x02, "1C 2C"                         },
	{ 0x3e, 1,    0x03, 0x01, "1C 5C"                         },
	{ 0x3e, 1,    0x03, 0x00, "1C 10C"                        },

	{ 0,    0xfe, 0,    4,    "Min Rate To Play"              },
	{ 0x3e, 1,    0x0c, 0x0c, "1"                             },
	{ 0x3e, 1,    0x0c, 0x08, "2"                             },
	{ 0x3e, 1,    0x0c, 0x04, "3"                             },
	{ 0x3e, 1,    0x0c, 0x00, "5"                             },

	{ 0,    0xfe, 0,    4,    "Payout"                        },
	{ 0x3e, 1,    0x30, 0x30, "300"                           },
	{ 0x3e, 1,    0x30, 0x20, "500"                           },
	{ 0x3e, 1,    0x30, 0x10, "700"                           },
	{ 0x3e, 1,    0x30, 0x00, "1000"                          },

	{ 0,    0xfe, 0,    2,    "W-BET"                         },
	{ 0x3e, 1,    0x40, 0x40, "Off"                           },
	{ 0x3e, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Last Chance"                   },
	{ 0x3e, 1,    0x80, 0x80, "Off"                           },
	{ 0x3e, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    8,    "YAKUMAN Bonus"                 },
	{ 0x3f, 1,    0x07, 0x07, "Cut"                           },
	{ 0x3f, 1,    0x07, 0x06, "1 T"                           },
	{ 0x3f, 1,    0x07, 0x05, "300"                           },
	{ 0x3f, 1,    0x07, 0x04, "500"                           },
	{ 0x3f, 1,    0x07, 0x03, "700"                           },
	{ 0x3f, 1,    0x07, 0x02, "1000"                          },
	{ 0x3f, 1,    0x07, 0x01, "1500"                          },
	{ 0x3f, 1,    0x07, 0x00, "2000"                          },

	{ 0,    0xfe, 0,    4,    "YAKUMAN Times"                 },
	{ 0x3f, 1,    0x18, 0x00, "1"                             },
	{ 0x3f, 1,    0x18, 0x18, "1"                             },
	{ 0x3f, 1,    0x18, 0x10, "2"                             },
	{ 0x3f, 1,    0x18, 0x08, "3"                             },

	{ 0,    0xfe, 0,    2,    "3 BAI In YAKUMAN Bonus Chance" },
	{ 0x3f, 1,    0x20, 0x20, "Off"                           },
	{ 0x3f, 1,    0x20, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Auto Tsumo"                    },
	{ 0x3f, 1,    0x40, 0x40, "Off"                           },
	{ 0x3f, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Credit Timing"                 },
	{ 0x3f, 1,    0x80, 0x80, "Off"                           },
	{ 0x3f, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"                   },
	{ 0x40, 1,    0x01, 0x01, "Off"                           },
	{ 0x40, 1,    0x01, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "In Game Music"                 },
	{ 0x40, 1,    0x02, 0x02, "Off"                           },
	{ 0x40, 1,    0x02, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Girls (Demo)"                  },
	{ 0x40, 1,    0x04, 0x04, "Off"                           },
	{ 0x40, 1,    0x04, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Girls Show After 3 Renso"      },
	{ 0x40, 1,    0x08, 0x08, "Off"                           },
	{ 0x40, 1,    0x08, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Girls (Play)"                  },
	{ 0x40, 1,    0x10, 0x10, "Off"                           },
	{ 0x40, 1,    0x10, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Boys In Game"                  },
	{ 0x40, 1,    0x20, 0x20, "Off"                           },
	{ 0x40, 1,    0x20, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Boys"                          },
	{ 0x40, 1,    0x40, 0x40, "Off"                           },
	{ 0x40, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Don Den Key"                   },
	{ 0x40, 1,    0x80, 0x80, "Start"                         },
	{ 0x40, 1,    0x80, 0x00, "Flip/Flop"                     },

	{ 0,    0xfe, 0,    2,    "Credits Per Note"              },
	{ 0x41, 1,    0x01, 0x01, "5"                             },
	{ 0x41, 1,    0x01, 0x00, "10"                            },

	{ 0,    0xfe, 0,    2,    "Flip Screen"                   },
	{ 0x41, 1,    0x02, 0x02, "Off"                           },
	{ 0x41, 1,    0x02, 0x00, "On"                            },

	{ 0,    0xfe, 0,    4,    "Computer Strength"             },
	{ 0x41, 1,    0x0c, 0x0c, "Weak"                          },
	{ 0x41, 1,    0x0c, 0x08, "Normal"                        },
	{ 0x41, 1,    0x0c, 0x04, "Strong"                        },
	{ 0x41, 1,    0x0c, 0x00, "Very Strong"                   },

	{ 0,    0xfe, 0,    2,    "Game Style"                    },
	{ 0x41, 1,    0x10, 0x10, "Credit"                        },
	{ 0x41, 1,    0x10, 0x00, "Credit Time"                   },

	{ 0,    0xfe, 0,    2,    "Start Method (Credit Time)"    },
	{ 0x41, 1,    0x20, 0x20, "?"                             },
	{ 0x41, 1,    0x20, 0x00, "Rate"                          },

	{ 0,    0xfe, 0,    2,    "Unknown 4-8"                   },
	{ 0x41, 1,    0x40, 0x40, "Off"                           },
	{ 0x41, 1,    0x40, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Unknown 4-9"                   },
	{ 0x41, 1,    0x80, 0x80, "Off"                           },
	{ 0x41, 1,    0x80, 0x00, "On"                            },

	{ 0,    0xfe, 0,    2,    "Bets?"                         },
	{ 0x43, 1,    0x40, 0x40, "0"                             },
	{ 0x43, 1,    0x40, 0x00, "1"                             },

	{ 0,    0xfe, 0,    2,    "Unknown"                       },
	{ 0x44, 1,    0x40, 0x40, "Off"                           },
	{ 0x44, 1,    0x40, 0x00, "On"                            },
};

STDDIPINFO(mjreach1)

static struct BurnInputInfo mjschukaInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjschuka)

static struct BurnDIPInfo mjschukaDIPList[] = {
	{ 0x3d, 0xff, 0xff, 0xf7, NULL                   },
	{ 0x3e, 0xff, 0xff, 0xff, NULL                   },
	{ 0x3f, 0xff, 0xff, 0xcc, NULL                   },
	{ 0x40, 0xff, 0xff, 0xff, NULL                   },
	{ 0x41, 0xff, 0xff, 0xff, NULL                   },
	{ 0x42, 0xff, 0xff, 0xff, NULL                   },
	{ 0x43, 0xff, 0xff, 0xff, NULL                   },
	{ 0x44, 0xff, 0xff, 0xff, NULL                   },

	{ 0,    0xfe, 0,    16,   "Pay Out Rate (%)"     },
	{ 0x3d, 1,    0x0f, 0x00, "50"                   },
	{ 0x3d, 1,    0x0f, 0x01, "53"                   },
	{ 0x3d, 1,    0x0f, 0x02, "56"                   },
	{ 0x3d, 1,    0x0f, 0x03, "59"                   },
	{ 0x3d, 1,    0x0f, 0x04, "62"                   },
	{ 0x3d, 1,    0x0f, 0x05, "65"                   },
	{ 0x3d, 1,    0x0f, 0x06, "68"                   },
	{ 0x3d, 1,    0x0f, 0x07, "71"                   },
	{ 0x3d, 1,    0x0f, 0x08, "75"                   },
	{ 0x3d, 1,    0x0f, 0x09, "78"                   },
	{ 0x3d, 1,    0x0f, 0x0a, "81"                   },
	{ 0x3d, 1,    0x0f, 0x0b, "84"                   },
	{ 0x3d, 1,    0x0f, 0x0c, "87"                   },
	{ 0x3d, 1,    0x0f, 0x0d, "90"                   },
	{ 0x3d, 1,    0x0f, 0x0e, "93"                   },
	{ 0x3d, 1,    0x0f, 0x0f, "96"                   },

	{ 0,    0xfe, 0,    4,    "Odds Rate"            },
	{ 0x3d, 1,    0x30, 0x20, "1 2 3 4 6 8 10 15"    },
	{ 0x3d, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"  },
	{ 0x3d, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"   },
	{ 0x3d, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100" },

	{ 0,    0xfe, 0,    4,    "Max Rate"             },
	{ 0x3d, 1,    0xc0, 0xc0, "1"                    },
	{ 0x3d, 1,    0xc0, 0x80, "5"                    },
	{ 0x3d, 1,    0xc0, 0x40, "10"                   },
	{ 0x3d, 1,    0xc0, 0x00, "20"                   },

	{ 0,    0xfe, 0,    4,    "Coin A"               },
	{ 0x3e, 1,    0x03, 0x03, "1C 1C"                },
	{ 0x3e, 1,    0x03, 0x02, "1C 2C"                },
	{ 0x3e, 1,    0x03, 0x01, "1C 5C"                },
	{ 0x3e, 1,    0x03, 0x00, "1C 10C"               },

	{ 0,    0xfe, 0,    4,    "Min Rate To Play"     },
	{ 0x3e, 1,    0x0c, 0x0c, "1"                    },
	{ 0x3e, 1,    0x0c, 0x08, "2"                    },
	{ 0x3e, 1,    0x0c, 0x04, "3"                    },
	{ 0x3e, 1,    0x0c, 0x00, "5"                    },

	{ 0,    0xfe, 0,    4,    "Payout"               },
	{ 0x3e, 1,    0x30, 0x30, "300"                  },
	{ 0x3e, 1,    0x30, 0x20, "500"                  },
	{ 0x3e, 1,    0x30, 0x10, "700"                  },
	{ 0x3e, 1,    0x30, 0x00, "1000"                 },

	{ 0,    0xfe, 0,    2,    "W-BET"                },
	{ 0x3e, 1,    0x40, 0x40, "Off"                  },
	{ 0x3e, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Last Chance"          },
	{ 0x3e, 1,    0x80, 0x80, "Off"                  },
	{ 0x3e, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"          },
	{ 0x3f, 1,    0x01, 0x01, "Off"                  },
	{ 0x3f, 1,    0x01, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "In Game Music"        },
	{ 0x3f, 1,    0x02, 0x02, "Off"                  },
	{ 0x3f, 1,    0x02, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3f, 1,    0x04, 0x04, "Off"                  },
	{ 0x3f, 1,    0x04, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3f, 1,    0x08, 0x08, "Off"                  },
	{ 0x3f, 1,    0x08, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Girls"                },
	{ 0x3f, 1,    0x10, 0x10, "Off"                  },
	{ 0x3f, 1,    0x10, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Girls Speech"         },
	{ 0x3f, 1,    0x20, 0x20, "Off"                  },
	{ 0x3f, 1,    0x20, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3f, 1,    0x40, 0x40, "Off"                  },
	{ 0x3f, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3f, 1,    0x80, 0x80, "Off"                  },
	{ 0x3f, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    8,    "YAKUMAN Bonus"        },
	{ 0x40, 1,    0x07, 0x07, "Cut"                  },
	{ 0x40, 1,    0x07, 0x06, "1 T"                  },
	{ 0x40, 1,    0x07, 0x05, "300"                  },
	{ 0x40, 1,    0x07, 0x04, "500"                  },
	{ 0x40, 1,    0x07, 0x03, "700"                  },
	{ 0x40, 1,    0x07, 0x02, "1000"                 },
	{ 0x40, 1,    0x07, 0x01, "1000?"                },
	{ 0x40, 1,    0x07, 0x00, "1000?"                },

	{ 0,    0xfe, 0,    2,    "YAKUMAN Times?"       },
	{ 0x40, 1,    0x08, 0x00, "1"                    },
	{ 0x40, 1,    0x08, 0x08, "2"                    },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x40, 1,    0x10, 0x00, "140"                  },
	{ 0x40, 1,    0x10, 0x10, "160"                  },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x40, 1,    0x20, 0x20, "Off"                  },
	{ 0x40, 1,    0x20, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "DonDen Key"           },
	{ 0x40, 1,    0x40, 0x40, "Start"                },
	{ 0x40, 1,    0x40, 0x00, "Flip Flop"            },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x40, 1,    0x80, 0x80, "Off"                  },
	{ 0x40, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Credits Per Note"     },
	{ 0x41, 1,    0x01, 0x01, "5"                    },
	{ 0x41, 1,    0x01, 0x00, "10"                   },

	{ 0,    0xfe, 0,    2,    "Flip Screen"          },
	{ 0x41, 1,    0x02, 0x02, "Off"                  },
	{ 0x41, 1,    0x02, 0x00, "On"                   },

	{ 0,    0xfe, 0,    4,    "Computer Strength?"   },
	{ 0x41, 1,    0x0c, 0x0c, "Weak"                 },
	{ 0x41, 1,    0x0c, 0x08, "Normal"               },
	{ 0x41, 1,    0x0c, 0x04, "Strong"               },
	{ 0x41, 1,    0x0c, 0x00, "Very Strong"          },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x41, 1,    0x10, 0x10, "Off"                  },
	{ 0x41, 1,    0x10, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x41, 1,    0x20, 0x20, "Off"                  },
	{ 0x41, 1,    0x20, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x41, 1,    0x40, 0x40, "Off"                  },
	{ 0x41, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x41, 1,    0x80, 0x80, "Off"                  },
	{ 0x41, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x43, 1,    0x40, 0x40, "Off"                  },
	{ 0x43, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x44, 1,    0x40, 0x40, "Off"                  },
	{ 0x44, 1,    0x40, 0x00, "On"                   },
};

STDDIPINFO(mjschuka)

static struct BurnInputInfo mjxyslInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(mjxysl)

static struct BurnDIPInfo mjxyslDIPList[] = {
	{ 0x3d, 0xff, 0xff, 0x77, NULL                   },
	{ 0x3e, 0xff, 0xff, 0x7f, NULL                   },
	{ 0x3f, 0xff, 0xff, 0x9f, NULL                   },
	{ 0x40, 0xff, 0xff, 0xe0, NULL                   },
	{ 0x41, 0xff, 0xff, 0xfd, NULL                   },
	{ 0x42, 0xff, 0xff, 0xff, NULL                   },
	{ 0x43, 0xff, 0xff, 0xff, NULL                   },
	{ 0x44, 0xff, 0xff, 0xff, NULL                   },

	{ 0,    0xfe, 0,    16,   "Payout Rate"          },
	{ 0x3d, 1,    0x0f, 0x00, "50%"                  },
	{ 0x3d, 1,    0x0f, 0x01, "53%"                  },
	{ 0x3d, 1,    0x0f, 0x02, "56%"                  },
	{ 0x3d, 1,    0x0f, 0x03, "59%"                  },
	{ 0x3d, 1,    0x0f, 0x04, "62%"                  },
	{ 0x3d, 1,    0x0f, 0x05, "65%"                  },
	{ 0x3d, 1,    0x0f, 0x06, "68%"                  },
	{ 0x3d, 1,    0x0f, 0x07, "71%"                  },
	{ 0x3d, 1,    0x0f, 0x08, "75%"                  },
	{ 0x3d, 1,    0x0f, 0x09, "78%"                  },
	{ 0x3d, 1,    0x0f, 0x0a, "81%"                  },
	{ 0x3d, 1,    0x0f, 0x0b, "84%"                  },
	{ 0x3d, 1,    0x0f, 0x0c, "87%"                  },
	{ 0x3d, 1,    0x0f, 0x0d, "90%"                  },
	{ 0x3d, 1,    0x0f, 0x0e, "93%"                  },
	{ 0x3d, 1,    0x0f, 0x0f, "96%"                  },

	{ 0,    0xfe, 0,    4,    "Odds Rate"            },
	{ 0x3d, 1,    0x30, 0x20, "1 2 3 4 6 8 10 15"    },
	{ 0x3d, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"  },
	{ 0x3d, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"   },
	{ 0x3d, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100" },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"          },
	{ 0x3d, 1,    0xc0, 0xc0, "1"                    },
	{ 0x3d, 1,    0xc0, 0x80, "5"                    },
	{ 0x3d, 1,    0xc0, 0x40, "10"                   },
	{ 0x3d, 1,    0xc0, 0x00, "20"                   },

	{ 0,    0xfe, 0,    4,    "Coinage"              },
	{ 0x3e, 1,    0x03, 0x03, "1C 1C"                },
	{ 0x3e, 1,    0x03, 0x02, "1C 2C"                },
	{ 0x3e, 1,    0x03, 0x01, "1C 5C"                },
	{ 0x3e, 1,    0x03, 0x00, "1C 10C"               },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"          },
	{ 0x3e, 1,    0x0c, 0x0c, "1"                    },
	{ 0x3e, 1,    0x0c, 0x08, "2"                    },
	{ 0x3e, 1,    0x0c, 0x04, "3"                    },
	{ 0x3e, 1,    0x0c, 0x00, "5"                    },

	{ 0,    0xfe, 0,    2,    "Last Chance"          },
	{ 0x3e, 1,    0x80, 0x80, "Off"                  },
	{ 0x3e, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Auto Reach"           },
	{ 0x3f, 1,    0x20, 0x20, "Off"                  },
	{ 0x3f, 1,    0x20, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Don Den Button"       },
	{ 0x3f, 1,    0x40, 0x40, "Start"                },
	{ 0x3f, 1,    0x40, 0x00, "Flip Flop"            },

	{ 0,    0xfe, 0,    2,    "DIP4:1"               },
	{ 0x40, 1,    0x01, 0x01, "Off"                  },
	{ 0x40, 1,    0x01, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "In-Game Music"        },
	{ 0x40, 1,    0x02, 0x02, "Off"                  },
	{ 0x40, 1,    0x02, 0x00, "On"                   },

	{ 0,    0xfe, 0,    4,    "Gals"                 },
	{ 0x40, 1,    0x0c, 0x0c, "Off"                  },
	{ 0x40, 1,    0x0c, 0x08, "Dressed"              },
	{ 0x40, 1,    0x0c, 0x04, "Underwear"            },
	{ 0x40, 1,    0x0c, 0x00, "Nude"                 },

	{ 0,    0xfe, 0,    2,    "Gal Voice"            },
	{ 0x40, 1,    0x10, 0x10, "Off"                  },
	{ 0x40, 1,    0x10, 0x00, "On"                   },

	{ 0,    0xfe, 0,    8,    "Credits Per Note"     },
	{ 0x41, 1,    0x02, 0x02, "5"                    },
	{ 0x41, 1,    0x02, 0x02, "10"                   },
	{ 0x41, 1,    0x02, 0x02, "25"                   },
	{ 0x41, 1,    0x02, 0x02, "50"                   },
	{ 0x41, 1,    0x02, 0x00, "10"                   },
	{ 0x41, 1,    0x02, 0x00, "20"                   },
	{ 0x41, 1,    0x02, 0x00, "50"                   },
	{ 0x41, 1,    0x02, 0x00, "100"                  },

	{ 0,    0xfe, 0,    2,    "Game Type"            },
	{ 0x41, 1,    0x10, 0x10, "Credits"              },
	{ 0x41, 1,    0x10, 0x00, "Points"               },

	{ 0,    0xfe, 0,    2,    "DIP4:10"              },
	{ 0x41, 1,    0x80, 0x80, "Mahjong Panel"        },
	{ 0x41, 1,    0x80, 0x80, "Joystick"             },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x43, 1,    0x40, 0x40, "Off"                  },
	{ 0x43, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x44, 1,    0x40, 0x40, "Off"                  },
	{ 0x44, 1,    0x40, 0x00, "On"                   },
};

STDDIPINFO(mjxysl)

static struct BurnInputInfo mmpanicInputList[] = {
	{ "P1 Coin",     BIT_DIGITAL,   Joy[0] + 0, "p1 coin"   },
	{ "Service",     BIT_DIGITAL,   Joy[0] + 2, "service"   },
	{ "Test",        BIT_DIGITAL,   Joy[0] + 3, "diag"      },
	{ "P1 Button 8", BIT_DIGITAL,   Joy[0] + 5, "p1 fire 8" },
	{ "P1 Button 9", BIT_DIGITAL,   Joy[0] + 6, "p1 fire 9" },
	{ "P1 Start",    BIT_DIGITAL,   Joy[1] + 0, "p1 start"  },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[1] + 1, "p1 fire 1" },
	{ "P1 Button 2", BIT_DIGITAL,   Joy[1] + 2, "p1 fire 2" },
	{ "P1 Button 3", BIT_DIGITAL,   Joy[1] + 3, "p1 fire 3" },
	{ "P1 Button 4", BIT_DIGITAL,   Joy[1] + 4, "p1 fire 4" },
	{ "P1 Button 5", BIT_DIGITAL,   Joy[1] + 5, "p1 fire 5" },
	{ "P1 Button 6", BIT_DIGITAL,   Joy[1] + 6, "p1 fire 6" },
	{ "P1 Button 7", BIT_DIGITAL,   Joy[1] + 7, "p1 fire 7" },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(mmpanic)

static struct BurnDIPInfo mmpanicDIPList[] = {
	{ 0x0e, 0xff, 0xff, 0xf3, NULL              },
	{ 0x0f, 0xff, 0xff, 0xff, NULL              },
	{ 0x10, 0xff, 0xff, 0xff, NULL              },
	{ 0x11, 0xff, 0xff, 0xff, NULL              },
	{ 0x12, 0xff, 0xff, 0xff, NULL              },
	{ 0x13, 0xff, 0xff, 0xff, NULL              },
	{ 0x14, 0xff, 0xff, 0xff, NULL              },
	{ 0x15, 0xff, 0xff, 0xff, NULL              },

	{ 0,    0xfe, 0,    4,    "Coinage"         },
	{ 0x0e, 1,    0x03, 0x00, "3C 1C"           },
	{ 0x0e, 1,    0x03, 0x01, "2C 1C"           },
	{ 0x0e, 1,    0x03, 0x03, "1C 1C"           },
	{ 0x0e, 1,    0x03, 0x02, "1C 2C"           },

	{ 0,    0xfe, 0,    8,    "Difficulty"      },
	{ 0x0e, 1,    0x1c, 0x1c, "0"               },
	{ 0x0e, 1,    0x1c, 0x18, "1"               },
	{ 0x0e, 1,    0x1c, 0x14, "2"               },
	{ 0x0e, 1,    0x1c, 0x10, "3"               },
	{ 0x0e, 1,    0x1c, 0x0c, "4"               },
	{ 0x0e, 1,    0x1c, 0x08, "5"               },
	{ 0x0e, 1,    0x1c, 0x04, "5 (duplicate 1)" },
	{ 0x0e, 1,    0x1c, 0x00, "5 (duplicate 2)" },

	{ 0,    0xfe, 0,    2,    "Linked Cabinets" },
	{ 0x0e, 1,    0x20, 0x20, "Off"             },
	{ 0x0e, 1,    0x20, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"     },
	{ 0x0e, 1,    0x40, 0x00, "Off"             },
	{ 0x0e, 1,    0x40, 0x40, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 1-7*"    },
	{ 0x0e, 1,    0x80, 0x80, "Off"             },
	{ 0x0e, 1,    0x80, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 2-0*"    },
	{ 0x0f, 1,    0x01, 0x01, "Off"             },
	{ 0x0f, 1,    0x01, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Flip Screen"     },
	{ 0x0f, 1,    0x02, 0x02, "Off"             },
	{ 0x0f, 1,    0x02, 0x00, "On"              },

	{ 0,    0xfe, 0,    4,    "Unknown 2-2&3*"  },
	{ 0x0f, 1,    0x0c, 0x0c, "10"              },
	{ 0x0f, 1,    0x0c, 0x08, "20"              },
	{ 0x0f, 1,    0x0c, 0x04, "40"              },
	{ 0x0f, 1,    0x0c, 0x00, "80"              },

	{ 0,    0xfe, 0,    2,    "Unknown 2-4*"    },
	{ 0x0f, 1,    0x10, 0x10, "Off"             },
	{ 0x0f, 1,    0x10, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 2-5*"    },
	{ 0x0f, 1,    0x20, 0x20, "Off"             },
	{ 0x0f, 1,    0x20, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 2-6*"    },
	{ 0x0f, 1,    0x40, 0x40, "Off"             },
	{ 0x0f, 1,    0x40, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 2-7*"    },
	{ 0x0f, 1,    0x80, 0x80, "Off"             },
	{ 0x0f, 1,    0x80, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Set Date"        },
	{ 0x10, 1,    0x01, 0x01, "Off"             },
	{ 0x10, 1,    0x01, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 3-2*"    },
	{ 0x10, 1,    0x04, 0x04, "Off"             },
	{ 0x10, 1,    0x04, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 3-3*"    },
	{ 0x10, 1,    0x08, 0x08, "Off"             },
	{ 0x10, 1,    0x08, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 3-4"     },
	{ 0x10, 1,    0x10, 0x10, "Off"             },
	{ 0x10, 1,    0x10, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 3-5"     },
	{ 0x10, 1,    0x20, 0x20, "Off"             },
	{ 0x10, 1,    0x20, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 3-6*"    },
	{ 0x10, 1,    0x40, 0x40, "Off"             },
	{ 0x10, 1,    0x40, 0x00, "On"              },

	{ 0,    0xfe, 0,    2,    "Unknown 3-7*"    },
	{ 0x10, 1,    0x80, 0x80, "Off"             },
	{ 0x10, 1,    0x80, 0x00, "On"              },
};

STDDIPINFO(mmpanic)

static struct BurnInputInfo nettoqcInputList[] = {
	{ "P1 Start",    BIT_DIGITAL,   Joy[0] + 0, "p1 start"  },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[0] + 1, "p1 fire 1" },
	{ "P1 Button 2", BIT_DIGITAL,   Joy[0] + 2, "p1 fire 2" },
	{ "P1 Button 3", BIT_DIGITAL,   Joy[0] + 3, "p1 fire 3" },
	{ "P1 Button 4", BIT_DIGITAL,   Joy[0] + 4, "p1 fire 4" },
	{ "P2 Start",    BIT_DIGITAL,   Joy[1] + 0, "p2 start"  },
	{ "P2 Button 1", BIT_DIGITAL,   Joy[1] + 1, "p2 fire 1" },
	{ "P2 Button 2", BIT_DIGITAL,   Joy[1] + 2, "p2 fire 2" },
	{ "P2 Button 3", BIT_DIGITAL,   Joy[1] + 3, "p2 fire 3" },
	{ "P2 Button 4", BIT_DIGITAL,   Joy[1] + 4, "p2 fire 4" },
	{ "P1 Coin",     BIT_DIGITAL,   Joy[2] + 0, "p1 coin"   },
	{ "P2 Coin",     BIT_DIGITAL,   Joy[2] + 1, "p2 coin"   },
	{ "Service",     BIT_DIGITAL,   Joy[2] + 2, "service"   },
	{ "Test",        BIT_DIGITAL,   Joy[2] + 3, "diag"      },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(nettoqc)

static struct BurnDIPInfo nettoqcDIPList[] = {
	{ 0x0f, 0xff, 0xff, 0xff, NULL             },
	{ 0x10, 0xff, 0xff, 0xff, NULL             },
	{ 0x11, 0xff, 0xff, 0x7f, NULL             },
	{ 0x12, 0xff, 0xff, 0xff, NULL             },
	{ 0x13, 0xff, 0xff, 0xff, NULL             },
	{ 0x14, 0xff, 0xff, 0xff, NULL             },
	{ 0x15, 0xff, 0xff, 0xff, NULL             },
	{ 0x16, 0xff, 0xff, 0xff, NULL             },

	{ 0,    0xfe, 0,    4,    "Coin A"         },
	{ 0x0f, 1,    0x03, 0x00, "3C 1C"          },
	{ 0x0f, 1,    0x03, 0x01, "2C 1C"          },
	{ 0x0f, 1,    0x03, 0x03, "1C 1C"          },
	{ 0x0f, 1,    0x03, 0x02, "1C 2C"          },

	{ 0,    0xfe, 0,    4,    "Unknown"        },
	{ 0x0f, 1,    0x0c, 0x0c, "1"              },
	{ 0x0f, 1,    0x0c, 0x08, "2"              },
	{ 0x0f, 1,    0x0c, 0x04, "3"              },
	{ 0x0f, 1,    0x0c, 0x00, "5"              },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"    },
	{ 0x0f, 1,    0x10, 0x00, "Off"            },
	{ 0x0f, 1,    0x10, 0x10, "On"             },

	{ 0,    0xfe, 0,    2,    "Free Play"      },
	{ 0x0f, 1,    0x20, 0x20, "Off"            },
	{ 0x0f, 1,    0x20, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 1-6*"   },
	{ 0x0f, 1,    0x40, 0x40, "Off"            },
	{ 0x0f, 1,    0x40, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 1-7*"   },
	{ 0x0f, 1,    0x80, 0x80, "Off"            },
	{ 0x0f, 1,    0x80, 0x00, "On"             },

	{ 0,    0xfe, 0,    4,    "Unknown 2-0&1*" },
	{ 0x10, 1,    0x03, 0x02, "0"              },
	{ 0x10, 1,    0x03, 0x03, "1"              },
	{ 0x10, 1,    0x03, 0x01, "2"              },
	{ 0x10, 1,    0x03, 0x00, "3"              },

	{ 0,    0xfe, 0,    2,    "Unknown 2-2"    },
	{ 0x10, 1,    0x04, 0x04, "Off"            },
	{ 0x10, 1,    0x04, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-3*"   },
	{ 0x10, 1,    0x08, 0x08, "Off"            },
	{ 0x10, 1,    0x08, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-4*"   },
	{ 0x10, 1,    0x10, 0x10, "Off"            },
	{ 0x10, 1,    0x10, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-5*"   },
	{ 0x10, 1,    0x20, 0x20, "Off"            },
	{ 0x10, 1,    0x20, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-6*"   },
	{ 0x10, 1,    0x40, 0x40, "Off"            },
	{ 0x10, 1,    0x40, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-7*"   },
	{ 0x10, 1,    0x80, 0x80, "Off"            },
	{ 0x10, 1,    0x80, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 1-8*"   },
	{ 0x11, 1,    0x01, 0x01, "Off"            },
	{ 0x11, 1,    0x01, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-8*"   },
	{ 0x11, 1,    0x04, 0x04, "Off"            },
	{ 0x11, 1,    0x04, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-9*"   },
	{ 0x11, 1,    0x08, 0x08, "Off"            },
	{ 0x11, 1,    0x08, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Detailed Tests" },
	{ 0x11, 1,    0x80, 0x80, "Off"            },
	{ 0x11, 1,    0x80, 0x00, "On"             },
};

STDDIPINFO(nettoqc)

static struct BurnInputInfo quiz365InputList[] = {
	{ "P1 Start",    BIT_DIGITAL,   Joy[0] + 0, "p1 start"  },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[0] + 1, "p1 fire 1" },
	{ "P1 Button 2", BIT_DIGITAL,   Joy[0] + 2, "p1 fire 2" },
	{ "P1 Button 3", BIT_DIGITAL,   Joy[0] + 3, "p1 fire 3" },
	{ "P1 Button 4", BIT_DIGITAL,   Joy[0] + 4, "p1 fire 4" },
	{ "P2 Start",    BIT_DIGITAL,   Joy[1] + 0, "p2 start"  },
	{ "P2 Button 1", BIT_DIGITAL,   Joy[1] + 1, "p2 fire 1" },
	{ "P2 Button 2", BIT_DIGITAL,   Joy[1] + 2, "p2 fire 2" },
	{ "P2 Button 3", BIT_DIGITAL,   Joy[1] + 3, "p2 fire 3" },
	{ "P2 Button 4", BIT_DIGITAL,   Joy[1] + 4, "p2 fire 4" },
	{ "P1 Coin",     BIT_DIGITAL,   Joy[2] + 0, "p1 coin"   },
	{ "P2 Coin",     BIT_DIGITAL,   Joy[2] + 1, "p2 coin"   },
	{ "Service",     BIT_DIGITAL,   Joy[2] + 2, "service"   },
	{ "Test",        BIT_DIGITAL,   Joy[2] + 3, "diag"      },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(quiz365)

static struct BurnDIPInfo quiz365DIPList[] = {
	{ 0x0f, 0xff, 0xff, 0xff, NULL             },
	{ 0x10, 0xff, 0xff, 0xff, NULL             },
	{ 0x11, 0xff, 0xff, 0xfb, NULL             },
	{ 0x12, 0xff, 0xff, 0xff, NULL             },
	{ 0x13, 0xff, 0xff, 0xff, NULL             },
	{ 0x14, 0xff, 0xff, 0xff, NULL             },
	{ 0x15, 0xff, 0xff, 0xff, NULL             },
	{ 0x16, 0xff, 0xff, 0xff, NULL             },

	{ 0,    0xfe, 0,    4,    "Coin A"         },
	{ 0x0f, 1,    0x03, 0x00, "3C 1C"          },
	{ 0x0f, 1,    0x03, 0x01, "2C 1C"          },
	{ 0x0f, 1,    0x03, 0x03, "1C 1C"          },
	{ 0x0f, 1,    0x03, 0x02, "1C 2C"          },

	{ 0,    0xfe, 0,    4,    "Coin B"         },
	{ 0x0f, 1,    0x0c, 0x00, "3C 1C"          },
	{ 0x0f, 1,    0x0c, 0x04, "2C 1C"          },
	{ 0x0f, 1,    0x0c, 0x0c, "1C 1C"          },
	{ 0x0f, 1,    0x0c, 0x08, "1C 2C"          },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"    },
	{ 0x0f, 1,    0x10, 0x00, "Off"            },
	{ 0x0f, 1,    0x10, 0x10, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 1-5*"   },
	{ 0x0f, 1,    0x20, 0x20, "Off"            },
	{ 0x0f, 1,    0x20, 0x00, "On"             },

	{ 0,    0xfe, 0,    4,    "Unknown 1-6&7"  },
	{ 0x0f, 1,    0xc0, 0x40, "0"              },
	{ 0x0f, 1,    0xc0, 0x80, "1"              },
	{ 0x0f, 1,    0xc0, 0xc0, "2"              },
	{ 0x0f, 1,    0xc0, 0x00, "2"              },

	{ 0,    0xfe, 0,    4,    "Unknown 2-0&1"  },
	{ 0x10, 1,    0x03, 0x02, "0"              },
	{ 0x10, 1,    0x03, 0x03, "1"              },
	{ 0x10, 1,    0x03, 0x01, "2"              },
	{ 0x10, 1,    0x03, 0x00, "3"              },

	{ 0,    0xfe, 0,    4,    "Unknown 2-2&3"  },
	{ 0x10, 1,    0x0c, 0x08, "0"              },
	{ 0x10, 1,    0x0c, 0x0c, "1"              },
	{ 0x10, 1,    0x0c, 0x04, "2"              },
	{ 0x10, 1,    0x0c, 0x00, "3"              },

	{ 0,    0xfe, 0,    2,    "Unknown 2-4"    },
	{ 0x10, 1,    0x10, 0x10, "Off"            },
	{ 0x10, 1,    0x10, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-5"    },
	{ 0x10, 1,    0x20, 0x20, "Off"            },
	{ 0x10, 1,    0x20, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-6"    },
	{ 0x10, 1,    0x40, 0x40, "Off"            },
	{ 0x10, 1,    0x40, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 2-7"    },
	{ 0x10, 1,    0x80, 0x80, "Off"            },
	{ 0x10, 1,    0x80, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 3-0"    },
	{ 0x11, 1,    0x01, 0x01, "Off"            },
	{ 0x11, 1,    0x01, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Detailed Tests" },
	{ 0x11, 1,    0x04, 0x04, "Off"            },
	{ 0x11, 1,    0x04, 0x00, "On"             },

	{ 0,    0xfe, 0,    2,    "Unknown 3-3"    },
	{ 0x11, 1,    0x08, 0x08, "Off"            },
	{ 0x11, 1,    0x08, 0x00, "On"             },
};

STDDIPINFO(quiz365)

static struct BurnInputInfo quizchqInputList[] = {
	{ "P1 Start",    BIT_DIGITAL,   Joy[0] + 0, "p1 start"  },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[0] + 1, "p1 fire 1" },
	{ "P1 Button 2", BIT_DIGITAL,   Joy[0] + 2, "p1 fire 2" },
	{ "P1 Button 3", BIT_DIGITAL,   Joy[0] + 3, "p1 fire 3" },
	{ "P1 Button 4", BIT_DIGITAL,   Joy[0] + 4, "p1 fire 4" },
	{ "P2 Start",    BIT_DIGITAL,   Joy[1] + 0, "p2 start"  },
	{ "P2 Button 1", BIT_DIGITAL,   Joy[1] + 1, "p2 fire 1" },
	{ "P2 Button 2", BIT_DIGITAL,   Joy[1] + 2, "p2 fire 2" },
	{ "P2 Button 3", BIT_DIGITAL,   Joy[1] + 3, "p2 fire 3" },
	{ "P2 Button 4", BIT_DIGITAL,   Joy[1] + 4, "p2 fire 4" },
	{ "P1 Coin",     BIT_DIGITAL,   Joy[2] + 0, "p1 coin"   },
	{ "P2 Coin",     BIT_DIGITAL,   Joy[2] + 1, "p2 coin"   },
	{ "Service",     BIT_DIGITAL,   Joy[2] + 2, "service"   },
	{ "Test",        BIT_DIGITAL,   Joy[2] + 3, "diag"      },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(quizchq)

static struct BurnDIPInfo quizchqDIPList[] = {
	{ 0x0f, 0xff, 0xff, 0xff, NULL          },
	{ 0x10, 0xff, 0xff, 0xff, NULL          },
	{ 0x11, 0xff, 0xff, 0xff, NULL          },
	{ 0x12, 0xff, 0xff, 0xff, NULL          },
	{ 0x13, 0xff, 0xff, 0xff, NULL          },
	{ 0x14, 0xff, 0xff, 0xff, NULL          },
	{ 0x15, 0xff, 0xff, 0xff, NULL          },
	{ 0x16, 0xff, 0xff, 0xff, NULL          },

	{ 0,    0xfe, 0,    4,    "Coin A"      },
	{ 0x0f, 1,    0x03, 0x00, "3C 1C"       },
	{ 0x0f, 1,    0x03, 0x01, "2C 1C"       },
	{ 0x0f, 1,    0x03, 0x03, "1C 1C"       },
	{ 0x0f, 1,    0x03, 0x02, "1C 2C"       },

	{ 0,    0xfe, 0,    4,    "Coin B"      },
	{ 0x0f, 1,    0x0c, 0x00, "3C 1C"       },
	{ 0x0f, 1,    0x0c, 0x04, "2C 1C"       },
	{ 0x0f, 1,    0x0c, 0x0c, "1C 1C"       },
	{ 0x0f, 1,    0x0c, 0x08, "1C 2C"       },

	{ 0,    0xfe, 0,    2,    "Unknown 1-4" },
	{ 0x0f, 1,    0x10, 0x10, "Off"         },
	{ 0x0f, 1,    0x10, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Demo Sounds" },
	{ 0x0f, 1,    0x20, 0x00, "Off"         },
	{ 0x0f, 1,    0x20, 0x20, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 1-6" },
	{ 0x0f, 1,    0x40, 0x40, "Off"         },
	{ 0x0f, 1,    0x40, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Set Date"    },
	{ 0x0f, 1,    0x80, 0x80, "Off"         },
	{ 0x0f, 1,    0x80, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-0" },
	{ 0x10, 1,    0x01, 0x01, "Off"         },
	{ 0x10, 1,    0x01, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-1" },
	{ 0x10, 1,    0x02, 0x02, "Off"         },
	{ 0x10, 1,    0x02, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-2" },
	{ 0x10, 1,    0x04, 0x04, "Off"         },
	{ 0x10, 1,    0x04, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-3" },
	{ 0x10, 1,    0x08, 0x08, "Off"         },
	{ 0x10, 1,    0x08, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-4" },
	{ 0x10, 1,    0x10, 0x10, "Off"         },
	{ 0x10, 1,    0x10, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-5" },
	{ 0x10, 1,    0x20, 0x20, "Off"         },
	{ 0x10, 1,    0x20, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 2-6" },
	{ 0x10, 1,    0x40, 0x40, "Off"         },
	{ 0x10, 1,    0x40, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 1-0" },
	{ 0x11, 1,    0x01, 0x01, "Off"         },
	{ 0x11, 1,    0x01, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 1-1" },
	{ 0x11, 1,    0x02, 0x02, "Off"         },
	{ 0x11, 1,    0x02, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 1-2" },
	{ 0x11, 1,    0x04, 0x04, "Off"         },
	{ 0x11, 1,    0x04, 0x00, "On"          },

	{ 0,    0xfe, 0,    2,    "Unknown 1-3" },
	{ 0x11, 1,    0x08, 0x08, "Off"         },
	{ 0x11, 1,    0x08, 0x00, "On"          },
};

STDDIPINFO(quizchq)

static struct BurnInputInfo rongrongInputList[] = {
	{ "P1 Start",    BIT_DIGITAL,   Joy[0] + 0, "p1 start"  },
	{ "P1 Up",       BIT_DIGITAL,   Joy[0] + 1, "p1 up"     },
	{ "P1 Down",     BIT_DIGITAL,   Joy[0] + 2, "p1 down"   },
	{ "P1 Left",     BIT_DIGITAL,   Joy[0] + 3, "p1 left"   },
	{ "P1 Right",    BIT_DIGITAL,   Joy[0] + 4, "p1 right"  },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[0] + 5, "p1 fire 1" },
	{ "P1 Button 2", BIT_DIGITAL,   Joy[0] + 6, "p1 fire 2" },
	{ "P1 Button 3", BIT_DIGITAL,   Joy[0] + 7, "p1 fire 3" },
	{ "P2 Start",    BIT_DIGITAL,   Joy[1] + 0, "p2 start"  },
	{ "P2 Up",       BIT_DIGITAL,   Joy[1] + 1, "p2 up"     },
	{ "P2 Down",     BIT_DIGITAL,   Joy[1] + 2, "p2 down"   },
	{ "P2 Left",     BIT_DIGITAL,   Joy[1] + 3, "p2 left"   },
	{ "P2 Right",    BIT_DIGITAL,   Joy[1] + 4, "p2 right"  },
	{ "P2 Button 1", BIT_DIGITAL,   Joy[1] + 5, "p2 fire 1" },
	{ "P2 Button 2", BIT_DIGITAL,   Joy[1] + 6, "p2 fire 2" },
	{ "P2 Button 3", BIT_DIGITAL,   Joy[1] + 7, "p2 fire 3" },
	{ "P1 Coin",     BIT_DIGITAL,   Joy[2] + 0, "p1 coin"   },
	{ "P2 Coin",     BIT_DIGITAL,   Joy[2] + 1, "p2 coin"   },
	{ "Service",     BIT_DIGITAL,   Joy[2] + 2, "service"   },
	{ "Test",        BIT_DIGITAL,   Joy[2] + 3, "diag"      },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(rongrong)

static struct BurnDIPInfo rongrongDIPList[] = {
	{ 0x15, 0xff, 0xff, 0xff, NULL                           },
	{ 0x16, 0xff, 0xff, 0xff, NULL                           },
	{ 0x17, 0xff, 0xff, 0xff, NULL                           },
	{ 0x18, 0xff, 0xff, 0xff, NULL                           },
	{ 0x19, 0xff, 0xff, 0xff, NULL                           },
	{ 0x1a, 0xff, 0xff, 0xff, NULL                           },
	{ 0x1b, 0xff, 0xff, 0xff, NULL                           },
	{ 0x1c, 0xff, 0xff, 0xff, NULL                           },

	{ 0,    0xfe, 0,    4,    "SW1:1,2"                      },
	{ 0x15, 1,    0x03, 0x00, "3C 1C"                        },
	{ 0x15, 1,    0x03, 0x01, "2C 1C"                        },
	{ 0x15, 1,    0x03, 0x03, "1C 1C"                        },
	{ 0x15, 1,    0x03, 0x02, "1C 2C"                        },

	{ 0,    0xfe, 0,    4,    "SW1:3,4"                      },
	{ 0x15, 1,    0x0c, 0x00, "3C 1C"                        },
	{ 0x15, 1,    0x0c, 0x04, "2C 1C"                        },
	{ 0x15, 1,    0x0c, 0x0c, "1C 1C"                        },
	{ 0x15, 1,    0x0c, 0x08, "1C 2C"                        },

	{ 0,    0xfe, 0,    2,    "SW1:5"                        },
	{ 0x15, 1,    0x10, 0x10, "Off"                          },
	{ 0x15, 1,    0x10, 0x00, "On"                           },

	{ 0,    0xfe, 0,    2,    "Helps"                        },
	{ 0x15, 1,    0x20, 0x00, "2"                            },
	{ 0x15, 1,    0x20, 0x20, "3"                            },

	{ 0,    0xfe, 0,    2,    "SW1:7"                        },
	{ 0x15, 1,    0x40, 0x00, "Off"                          },
	{ 0x15, 1,    0x40, 0x40, "On"                           },

	{ 0,    0xfe, 0,    2,    "Blinking on Matching Pieces"  },
	{ 0x15, 1,    0x80, 0x80, "First Level and on Continues" },
	{ 0x15, 1,    0x80, 0x00, "Always On"                    },

	{ 0,    0xfe, 0,    4,    "SW2:1,2"                      },
	{ 0x16, 1,    0x03, 0x02, "Easy"                         },
	{ 0x16, 1,    0x03, 0x03, "Normal"                       },
	{ 0x16, 1,    0x03, 0x01, "Hard"                         },
	{ 0x16, 1,    0x03, 0x00, "Hardest"                      },

	{ 0,    0xfe, 0,    4,    "Base Time"                    },
	{ 0x16, 1,    0x0c, 0x0c, "80 Seconds"                   },
	{ 0x16, 1,    0x0c, 0x08, "90 Seconds"                   },
	{ 0x16, 1,    0x0c, 0x04, "70 Seconds"                   },
	{ 0x16, 1,    0x0c, 0x00, "60 Seconds"                   },

	{ 0,    0xfe, 0,    4,    "VS Rounds"                    },
	{ 0x16, 1,    0x30, 0x30, "1"                            },
	{ 0x16, 1,    0x30, 0x20, "2"                            },
	{ 0x16, 1,    0x30, 0x10, "3"                            },
	{ 0x16, 1,    0x30, 0x00, "3 (duplicate)"                },

	{ 0,    0xfe, 0,    2,    "SW2:7"                        },
	{ 0x16, 1,    0x40, 0x40, "Off"                          },
	{ 0x16, 1,    0x40, 0x00, "On"                           },

	{ 0,    0xfe, 0,    2,    "SW2:8"                        },
	{ 0x16, 1,    0x80, 0x80, "Off"                          },
	{ 0x16, 1,    0x80, 0x00, "On"                           },

	{ 0,    0xfe, 0,    2,    "SW1:9"                        },
	{ 0x17, 1,    0x01, 0x01, "Off"                          },
	{ 0x17, 1,    0x01, 0x00, "On"                           },

	{ 0,    0xfe, 0,    2,    "SW2:9"                        },
	{ 0x17, 1,    0x04, 0x04, "Off"                          },
	{ 0x17, 1,    0x04, 0x00, "On"                           },

	{ 0,    0xfe, 0,    2,    "Select Round (Cheat)"         },
	{ 0x17, 1,    0x08, 0x08, "Off"                          },
	{ 0x17, 1,    0x08, 0x00, "On"                           },
};

STDDIPINFO(rongrong)

static struct BurnInputInfo rselectInputList[] = {
	{ "Gamble Payout", BIT_DIGITAL,   Joy[2] + 0,          "gamble payout" },
	{ "Test",          BIT_DIGITAL,   Joy[2] + 2,          "diag"          },
	{ "Gamble Book",   BIT_DIGITAL,   Joy[2] + 3,          "gamble book"   },
	{ "Memory Reset",  BIT_DIGITAL,   Joy[2] + 4,          "memory reset"  },
	{ "P2 Coin",       BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"       },
	{ "P1 Coin",       BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"       },
	{ "P1 A",          BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"         },
	{ "P1 E",          BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"         },
	{ "P1 Double Up",  BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah double up" },
	{ "P1 B",          BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"         },
	{ "P1 F",          BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"         },
	{ "P1 Start",      BIT_DIGITAL,   MatrixJoy[0][1] + 2, "p1 start"      },
	{ "P1 C",          BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"         },
	{ "P1 G",          BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"         },
	{ "P1 Small",      BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah small"     },
	{ "P1 D",          BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"         },
	{ "P1 H",          BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"         },
	{ "P1 Big",        BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah big"       },
	{ "P1 Score",      BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah score"     },
	{ "P1 Button 1",   BIT_DIGITAL,   MatrixJoy[0][4] + 1, "p1 fire 1"     },
	{ "P1 Button 2",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "p1 fire 2"     },
	{ "P1 Flip Flop",  BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop" },
	{ "P1 Button 3",   BIT_DIGITAL,   MatrixJoy[0][4] + 4, "p1 fire 3"     },
	{ "P1 Button 4",   BIT_DIGITAL,   MatrixJoy[0][4] + 5, "p1 fire 4"     },
	{ "P2 A",          BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"         },
	{ "P2 E",          BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"         },
	{ "P2 Double Up",  BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah double up" },
	{ "P2 B",          BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"         },
	{ "P2 F",          BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"         },
	{ "P2 Start",      BIT_DIGITAL,   MatrixJoy[1][1] + 2, "p2 start"      },
	{ "P2 C",          BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"         },
	{ "P2 G",          BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"         },
	{ "P2 Small",      BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah small"     },
	{ "P2 D",          BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"         },
	{ "P2 H",          BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"         },
	{ "P2 Big",        BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah big"       },
	{ "P2 Score",      BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah score"     },
	{ "P2 Button 1",   BIT_DIGITAL,   MatrixJoy[1][4] + 1, "p2 fire 1"     },
	{ "P2 Button 2",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "p2 fire 2"     },
	{ "P2 Flip Flop",  BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop" },
	{ "P2 Button 3",   BIT_DIGITAL,   MatrixJoy[1][4] + 4, "p2 fire 3"     },
	{ "P2 Button 4",   BIT_DIGITAL,   MatrixJoy[1][4] + 5, "p2 fire 4"     },
	{ "Reset",         BIT_DIGITAL,   &DrvReset,           "reset"         },
	{ "Dip A",         BIT_DIPSWITCH, Dips + 0,            "dip"           },
	{ "Dip B",         BIT_DIPSWITCH, Dips + 1,            "dip"           },
	{ "Dip C",         BIT_DIPSWITCH, Dips + 2,            "dip"           },
	{ "Dip D",         BIT_DIPSWITCH, Dips + 3,            "dip"           },
	{ "Dip E",         BIT_DIPSWITCH, Dips + 4,            "dip"           },
	{ "Dip F",         BIT_DIPSWITCH, Dips + 5,            "dip"           },
	{ "Dip G",         BIT_DIPSWITCH, Dips + 6,            "dip"           },
	{ "Dip H",         BIT_DIPSWITCH, Dips + 7,            "dip"           },
};

STDINPUTINFO(rselect)

static struct BurnDIPInfo rselectDIPList[] = {
	{ 0x2b, 0xff, 0xff, 0x4d, NULL                       },
	{ 0x2c, 0xff, 0xff, 0x00, NULL                       },
	{ 0x2d, 0xff, 0xff, 0xff, NULL                       },
	{ 0x2e, 0xff, 0xff, 0xe0, NULL                       },
	{ 0x2f, 0xff, 0xff, 0xff, NULL                       },
	{ 0x30, 0xff, 0xff, 0xff, NULL                       },
	{ 0x31, 0xff, 0xff, 0xff, NULL                       },
	{ 0x32, 0xff, 0xff, 0xff, NULL                       },

	{ 0,    0xfe, 0,    8,    "Payout Rate"              },
	{ 0x2b, 1,    0x07, 0x00, "8 (low)"                  },
	{ 0x2b, 1,    0x07, 0x01, "7"                        },
	{ 0x2b, 1,    0x07, 0x02, "6"                        },
	{ 0x2b, 1,    0x07, 0x03, "5"                        },
	{ 0x2b, 1,    0x07, 0x04, "4"                        },
	{ 0x2b, 1,    0x07, 0x05, "3"                        },
	{ 0x2b, 1,    0x07, 0x06, "2"                        },
	{ 0x2b, 1,    0x07, 0x07, "1 (high)"                 },

	{ 0,    0xfe, 0,    4,    "Maximum Bet (per symbol)" },
	{ 0x2b, 1,    0x18, 0x18, "1"                        },
	{ 0x2b, 1,    0x18, 0x10, "5"                        },
	{ 0x2b, 1,    0x18, 0x08, "10"                       },
	{ 0x2b, 1,    0x18, 0x00, "20"                       },

	{ 0,    0xfe, 0,    8,    "DIP-SW1:6,7,8"            },
	{ 0x2b, 1,    0xe0, 0xe0, "1C 1C"                    },
	{ 0x2b, 1,    0xe0, 0xc0, "1C 2C"                    },
	{ 0x2b, 1,    0xe0, 0xa0, "1C 4C"                    },
	{ 0x2b, 1,    0xe0, 0x80, "1C 5C"                    },
	{ 0x2b, 1,    0xe0, 0x60, "1C 8C"                    },
	{ 0x2b, 1,    0xe0, 0x40, "1C 10C"                   },
	{ 0x2b, 1,    0xe0, 0x20, "1C 20C"                   },
	{ 0x2b, 1,    0xe0, 0x00, "1C 50C"                   },

	{ 0,    0xfe, 0,    8,    "Coin Limit"               },
	{ 0x2c, 1,    0x07, 0x07, "150"                      },
	{ 0x2c, 1,    0x07, 0x06, "300"                      },
	{ 0x2c, 1,    0x07, 0x05, "500"                      },
	{ 0x2c, 1,    0x07, 0x04, "1,000"                    },
	{ 0x2c, 1,    0x07, 0x03, "1,500"                    },
	{ 0x2c, 1,    0x07, 0x02, "2,000"                    },
	{ 0x2c, 1,    0x07, 0x01, "5,000"                    },
	{ 0x2c, 1,    0x07, 0x00, "Unlimited"                },

	{ 0,    0xfe, 0,    4,    "Note Limit"               },
	{ 0x2c, 1,    0x18, 0x18, "300"                      },
	{ 0x2c, 1,    0x18, 0x10, "1,000"                    },
	{ 0x2c, 1,    0x18, 0x08, "3,000"                    },
	{ 0x2c, 1,    0x18, 0x00, "Unlimited"                },

	{ 0,    0xfe, 0,    8,    "Credit Limit"             },
	{ 0x2c, 1,    0xe0, 0xe0, "500"                      },
	{ 0x2c, 1,    0xe0, 0xc0, "1,000"                    },
	{ 0x2c, 1,    0xe0, 0xa0, "2,000"                    },
	{ 0x2c, 1,    0xe0, 0x80, "3,000"                    },
	{ 0x2c, 1,    0xe0, 0x60, "5,000"                    },
	{ 0x2c, 1,    0xe0, 0x40, "10,000"                   },
	{ 0x2c, 1,    0xe0, 0x20, "20,000"                   },
	{ 0x2c, 1,    0xe0, 0x00, "Unlimited"                },

	{ 0,    0xfe, 0,    2,    "Game Type"                },
	{ 0x2d, 1,    0x01, 0x01, "Credit Type"              },
	{ 0x2d, 1,    0x01, 0x00, "Hopper Type"              },

	{ 0,    0xfe, 0,    2,    "Hopper Polarity"          },
	{ 0x2d, 1,    0x04, 0x04, "Normal"                   },
	{ 0x2d, 1,    0x04, 0x00, "Inverted"                 },

	{ 0,    0xfe, 0,    2,    "DIP-SW4:1"                },
	{ 0x2e, 1,    0x01, 0x01, "Off"                      },
	{ 0x2e, 1,    0x01, 0x00, "On"                       },

	{ 0,    0xfe, 0,    2,    "In-Game Music"            },
	{ 0x2e, 1,    0x02, 0x02, "Off"                      },
	{ 0x2e, 1,    0x02, 0x00, "On"                       },

	{ 0,    0xfe, 0,    2,    "Double Up Game"           },
	{ 0x2e, 1,    0x04, 0x04, "Off"                      },
	{ 0x2e, 1,    0x04, 0x00, "On"                       },

	{ 0,    0xfe, 0,    2,    "Quiz/Coin Toss"           },
	{ 0x2e, 1,    0x08, 0x08, "Quiz"                     },
	{ 0x2e, 1,    0x08, 0x00, "Coin Toss"                },

	{ 0,    0xfe, 0,    2,    "Panel Lamps"              },
	{ 0x2e, 1,    0x10, 0x10, "Off"                      },
	{ 0x2e, 1,    0x10, 0x00, "On"                       },

	{ 0,    0xfe, 0,    2,    "Clock Set"                },
	{ 0x2e, 1,    0x20, 0x20, "Off"                      },
	{ 0x2e, 1,    0x20, 0x00, "On"                       },

	{ 0,    0xfe, 0,    2,    "DIP-SW4:7"                },
	{ 0x2e, 1,    0x40, 0x40, "Off"                      },
	{ 0x2e, 1,    0x40, 0x00, "On"                       },

	{ 0,    0xfe, 0,    2,    "Bonus Game Difficulty"    },
	{ 0x2f, 1,    0x10, 0x00, "Easy"                     },
	{ 0x2f, 1,    0x10, 0x10, "Normal"                   },

	{ 0,    0xfe, 0,    2,    "Fever Odds"               },
	{ 0x2f, 1,    0x20, 0x00, "Low"                      },
	{ 0x2f, 1,    0x20, 0x20, "Normal"                   },

	{ 0,    0xfe, 0,    16,   "Credits Per Note"         },
	{ 0x2f, 1,    0x40, 0x00, "5"                        },
	{ 0x2f, 1,    0x40, 0x00, "10"                       },
	{ 0x2f, 1,    0x40, 0x00, "20"                       },
	{ 0x2f, 1,    0x40, 0x00, "25"                       },
	{ 0x2f, 1,    0x40, 0x00, "40"                       },
	{ 0x2f, 1,    0x40, 0x00, "50"                       },
	{ 0x2f, 1,    0x40, 0x00, "100"                      },
	{ 0x2f, 1,    0x40, 0x00, "250"                      },
	{ 0x2f, 1,    0x40, 0x40, "10"                       },
	{ 0x2f, 1,    0x40, 0x40, "20"                       },
	{ 0x2f, 1,    0x40, 0x40, "40"                       },
	{ 0x2f, 1,    0x40, 0x40, "50"                       },
	{ 0x2f, 1,    0x40, 0x40, "80"                       },
	{ 0x2f, 1,    0x40, 0x40, "100"                      },
	{ 0x2f, 1,    0x40, 0x40, "200"                      },
	{ 0x2f, 1,    0x40, 0x40, "500"                      },

	{ 0,    0xfe, 0,    2,    "DIP-SW1:10"               },
	{ 0x2f, 1,    0x80, 0x80, "Off"                      },
	{ 0x2f, 1,    0x80, 0x00, "On"                       },
};

STDDIPINFO(rselect)

static struct BurnInputInfo seljan2InputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P3 Coin",        BIT_DIGITAL,   Joy[2] + 7,          "p3 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
	// Board service switch (MAME SYSTEM bit 2); keep existing DIP indices stable.
	{ "Test",           BIT_DIGITAL,   Joy[2] + 2,          "diag"            },
};

STDINPUTINFO(seljan2)

static struct BurnDIPInfo seljan2DIPList[] = {
	{ 0x3d, 0xff, 0xff, 0x47, NULL                             },
	{ 0x3e, 0xff, 0xff, 0x0f, NULL                             },
	{ 0x3f, 0xff, 0xff, 0xd4, NULL                             },
	{ 0x40, 0xff, 0xff, 0xc0, NULL                             },
	{ 0x41, 0xff, 0xff, 0xfa, NULL                             },
	{ 0x42, 0xff, 0xff, 0xff, NULL                             },
	{ 0x43, 0xff, 0xff, 0xff, NULL                             },
	{ 0x44, 0xff, 0xff, 0xff, NULL                             },

	{ 0,    0xfe, 0,    16,   "Payout Rate"                    },
	{ 0x3d, 1,    0x0f, 0x00, "50%"                            },
	{ 0x3d, 1,    0x0f, 0x01, "53%"                            },
	{ 0x3d, 1,    0x0f, 0x02, "56%"                            },
	{ 0x3d, 1,    0x0f, 0x03, "59%"                            },
	{ 0x3d, 1,    0x0f, 0x04, "62%"                            },
	{ 0x3d, 1,    0x0f, 0x05, "65%"                            },
	{ 0x3d, 1,    0x0f, 0x06, "68%"                            },
	{ 0x3d, 1,    0x0f, 0x07, "71%"                            },
	{ 0x3d, 1,    0x0f, 0x08, "75%"                            },
	{ 0x3d, 1,    0x0f, 0x09, "78%"                            },
	{ 0x3d, 1,    0x0f, 0x0a, "81%"                            },
	{ 0x3d, 1,    0x0f, 0x0b, "84%"                            },
	{ 0x3d, 1,    0x0f, 0x0c, "87%"                            },
	{ 0x3d, 1,    0x0f, 0x0d, "90%"                            },
	{ 0x3d, 1,    0x0f, 0x0e, "93%"                            },
	{ 0x3d, 1,    0x0f, 0x0f, "96%"                            },

	{ 0,    0xfe, 0,    4,    "Odds Rate"                      },
	{ 0x3d, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"            },
	{ 0x3d, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"             },
	{ 0x3d, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"            },
	{ 0x3d, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100"           },

	{ 0,    0xfe, 0,    4,    "Maximum Bet"                    },
	{ 0x3d, 1,    0xc0, 0xc0, "1"                              },
	{ 0x3d, 1,    0xc0, 0x80, "5"                              },
	{ 0x3d, 1,    0xc0, 0x40, "10"                             },
	{ 0x3d, 1,    0xc0, 0x00, "20"                             },

	{ 0,    0xfe, 0,    4,    "Coinage"                        },
	{ 0x3e, 1,    0x03, 0x03, "1C 1C"                          },
	{ 0x3e, 1,    0x03, 0x02, "1C 2C"                          },
	{ 0x3e, 1,    0x03, 0x01, "1C 5C"                          },
	{ 0x3e, 1,    0x03, 0x00, "1C 10C"                         },

	{ 0,    0xfe, 0,    4,    "Minimum Bet"                    },
	{ 0x3e, 1,    0x0c, 0x0c, "1"                              },
	{ 0x3e, 1,    0x0c, 0x08, "2"                              },
	{ 0x3e, 1,    0x0c, 0x04, "3"                              },
	{ 0x3e, 1,    0x0c, 0x00, "5"                              },

	{ 0,    0xfe, 0,    16,   "Credit Limit"                   },
	{ 0x3e, 1,    0x30, 0x30, "300"                            },
	{ 0x3e, 1,    0x30, 0x30, "600"                            },
	{ 0x3e, 1,    0x30, 0x30, "1500"                           },
	{ 0x3e, 1,    0x30, 0x30, "3000"                           },
	{ 0x3e, 1,    0x30, 0x20, "500"                            },
	{ 0x3e, 1,    0x30, 0x20, "1000"                           },
	{ 0x3e, 1,    0x30, 0x20, "2500"                           },
	{ 0x3e, 1,    0x30, 0x20, "5000"                           },
	{ 0x3e, 1,    0x30, 0x10, "700"                            },
	{ 0x3e, 1,    0x30, 0x10, "1400"                           },
	{ 0x3e, 1,    0x30, 0x10, "3500"                           },
	{ 0x3e, 1,    0x30, 0x10, "7000"                           },
	{ 0x3e, 1,    0x30, 0x00, "1000"                           },
	{ 0x3e, 1,    0x30, 0x00, "2000"                           },
	{ 0x3e, 1,    0x30, 0x00, "5000"                           },
	{ 0x3e, 1,    0x30, 0x00, "10000"                          },

	{ 0,    0xfe, 0,    2,    "Double Bet"                     },
	{ 0x3e, 1,    0x40, 0x40, "Off"                            },
	{ 0x3e, 1,    0x40, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Last Chance"                    },
	{ 0x3e, 1,    0x80, 0x80, "Off"                            },
	{ 0x3e, 1,    0x80, 0x00, "On"                             },

	{ 0,    0xfe, 0,    8,    "Yakuman Chance Cycle"           },
	{ 0x3f, 1,    0x07, 0x07, "None"                           },
	{ 0x3f, 1,    0x07, 0x06, "First time only"                },
	{ 0x3f, 1,    0x07, 0x05, "Every 300 coins"                },
	{ 0x3f, 1,    0x07, 0x04, "Every 500 coins"                },
	{ 0x3f, 1,    0x07, 0x03, "Every 700 coins"                },
	{ 0x3f, 1,    0x07, 0x02, "Every 1000 coins"               },
	{ 0x3f, 1,    0x07, 0x01, "Every 1000 coins"               },
	{ 0x3f, 1,    0x07, 0x00, "Every 1000 coins"               },

	{ 0,    0xfe, 0,    2,    "Yakuman Chances Per Cycle"      },
	{ 0x3f, 1,    0x08, 0x00, "1"                              },
	{ 0x3f, 1,    0x08, 0x08, "2"                              },

	{ 0,    0xfe, 0,    2,    "Auto Reach"                     },
	{ 0x3f, 1,    0x10, 0x10, "Off"                            },
	{ 0x3f, 1,    0x10, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Don Den Button"                 },
	{ 0x3f, 1,    0x20, 0x20, "Start"                          },
	{ 0x3f, 1,    0x20, 0x00, "Flip Flop"                      },

	{ 0,    0xfe, 0,    2,    "Medal Timer"                    },
	{ 0x3f, 1,    0x40, 0x40, "Off"                            },
	{ 0x3f, 1,    0x40, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Game Style"                     },
	{ 0x3f, 1,    0x80, 0x80, "Credit"                         },
	{ 0x3f, 1,    0x80, 0x00, "Credit timer"                   },

	{ 0,    0xfe, 0,    2,    "DIP-SW4:1"                      },
	{ 0x40, 1,    0x01, 0x01, "Off"                            },
	{ 0x40, 1,    0x01, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "In-Game Music"                  },
	{ 0x40, 1,    0x02, 0x02, "Off"                            },
	{ 0x40, 1,    0x02, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Show Renchan Gal"               },
	{ 0x40, 1,    0x04, 0x04, "No"                             },
	{ 0x40, 1,    0x04, 0x00, "Yes"                            },

	{ 0,    0xfe, 0,    2,    "Renchan Gal Display"            },
	{ 0x40, 1,    0x08, 0x08, "After every win"                },
	{ 0x40, 1,    0x08, 0x00, "After 3 consecutive wins"       },

	{ 0,    0xfe, 0,    2,    "Game Screen Gal Action"         },
	{ 0x40, 1,    0x10, 0x10, "Off"                            },
	{ 0x40, 1,    0x10, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "In-Game Voice"                  },
	{ 0x40, 1,    0x20, 0x20, "Off"                            },
	{ 0x40, 1,    0x20, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Winning Yakuman Display Voice"  },
	{ 0x40, 1,    0x40, 0x40, "Off"                            },
	{ 0x40, 1,    0x40, 0x00, "On"                             },

	{ 0,    0xfe, 0,    2,    "Hopper Polarity"                },
	{ 0x40, 1,    0x80, 0x80, "Normal"                         },
	{ 0x40, 1,    0x80, 0x00, "Inverted"                       },

	{ 0,    0xfe, 0,    8,    "Credits Per Note"               },
	{ 0x41, 1,    0x01, 0x01, "5"                              },
	{ 0x41, 1,    0x01, 0x01, "10"                             },
	{ 0x41, 1,    0x01, 0x01, "25"                             },
	{ 0x41, 1,    0x01, 0x01, "50"                             },
	{ 0x41, 1,    0x01, 0x00, "10"                             },
	{ 0x41, 1,    0x01, 0x00, "20"                             },
	{ 0x41, 1,    0x01, 0x00, "50"                             },
	{ 0x41, 1,    0x01, 0x00, "100"                            },

	{ 0,    0xfe, 0,    2,    "DIP-SW1:10"                     },
	{ 0x41, 1,    0x02, 0x02, "Off"                            },
	{ 0x41, 1,    0x02, 0x00, "On"                             },

	{ 0,    0xfe, 0,    4,    "Computer Strength"              },
	{ 0x41, 1,    0x0c, 0x00, "Weak"                           },
	{ 0x41, 1,    0x0c, 0x04, "Normal"                         },
	{ 0x41, 1,    0x0c, 0x08, "Somewhat strong"                },
	{ 0x41, 1,    0x0c, 0x0c, "Strong"                         },

	{ 0,    0xfe, 0,    2,    "Start Method With Credit Timer" },
	{ 0x41, 1,    0x10, 0x10, "Normal"                         },
	{ 0x41, 1,    0x10, 0x00, "Always minimum bet"             },

	{ 0,    0xfe, 0,    2,    "Bets?"                          },
	{ 0x43, 1,    0x40, 0x40, "0"                              },
	{ 0x43, 1,    0x40, 0x00, "1"                              },

	{ 0,    0xfe, 0,    2,    "Unknown"                        },
	{ 0x44, 1,    0x40, 0x40, "Off"                            },
	{ 0x44, 1,    0x40, 0x00, "On"                             },
};

STDDIPINFO(seljan2)

static struct BurnInputInfo sryudensInputList[] = {
	{ "Gamble Payout",  BIT_DIGITAL,   Joy[2] + 0,          "gamble payout"   },
	{ "Gamble Book",    BIT_DIGITAL,   Joy[2] + 3,          "gamble book"     },
	{ "Memory Reset",   BIT_DIGITAL,   Joy[2] + 4,          "memory reset"    },
	{ "P2 Coin",        BIT_DIGITAL,   Joy[2] + 5,          "p2 coin"         },
	{ "P1 Coin",        BIT_DIGITAL,   Joy[2] + 6,          "p1 coin"         },
	{ "P3 Coin",        BIT_DIGITAL,   Joy[2] + 7,          "p3 coin"         },
	{ "P1 A",           BIT_DIGITAL,   MatrixJoy[0][0] + 0, "mah a"           },
	{ "P1 E",           BIT_DIGITAL,   MatrixJoy[0][0] + 1, "mah e"           },
	{ "P1 I",           BIT_DIGITAL,   MatrixJoy[0][0] + 2, "mah i"           },
	{ "P1 M",           BIT_DIGITAL,   MatrixJoy[0][0] + 3, "mah m"           },
	{ "P1 Kan",         BIT_DIGITAL,   MatrixJoy[0][0] + 4, "mah kan"         },
	{ "P1 Start",       BIT_DIGITAL,   MatrixJoy[0][0] + 5, "p1 start"        },
	{ "P1 B",           BIT_DIGITAL,   MatrixJoy[0][1] + 0, "mah b"           },
	{ "P1 F",           BIT_DIGITAL,   MatrixJoy[0][1] + 1, "mah f"           },
	{ "P1 J",           BIT_DIGITAL,   MatrixJoy[0][1] + 2, "mah j"           },
	{ "P1 N",           BIT_DIGITAL,   MatrixJoy[0][1] + 3, "mah n"           },
	{ "P1 Reach",       BIT_DIGITAL,   MatrixJoy[0][1] + 4, "mah reach"       },
	{ "P1 Bet",         BIT_DIGITAL,   MatrixJoy[0][1] + 5, "mah bet"         },
	{ "P1 C",           BIT_DIGITAL,   MatrixJoy[0][2] + 0, "mah c"           },
	{ "P1 G",           BIT_DIGITAL,   MatrixJoy[0][2] + 1, "mah g"           },
	{ "P1 K",           BIT_DIGITAL,   MatrixJoy[0][2] + 2, "mah k"           },
	{ "P1 Chi",         BIT_DIGITAL,   MatrixJoy[0][2] + 3, "mah chi"         },
	{ "P1 Ron",         BIT_DIGITAL,   MatrixJoy[0][2] + 4, "mah ron"         },
	{ "P1 D",           BIT_DIGITAL,   MatrixJoy[0][3] + 0, "mah d"           },
	{ "P1 H",           BIT_DIGITAL,   MatrixJoy[0][3] + 1, "mah h"           },
	{ "P1 L",           BIT_DIGITAL,   MatrixJoy[0][3] + 2, "mah l"           },
	{ "P1 Pon",         BIT_DIGITAL,   MatrixJoy[0][3] + 3, "mah pon"         },
	{ "P1 Last Chance", BIT_DIGITAL,   MatrixJoy[0][4] + 0, "mah last chance" },
	{ "P1 Score",       BIT_DIGITAL,   MatrixJoy[0][4] + 1, "mah score"       },
	{ "P1 Double Up",   BIT_DIGITAL,   MatrixJoy[0][4] + 2, "mah double up"   },
	{ "P1 Flip Flop",   BIT_DIGITAL,   MatrixJoy[0][4] + 3, "mah flip flop"   },
	{ "P1 Big",         BIT_DIGITAL,   MatrixJoy[0][4] + 4, "mah big"         },
	{ "P1 Small",       BIT_DIGITAL,   MatrixJoy[0][4] + 5, "mah small"       },
	{ "P2 A",           BIT_DIGITAL,   MatrixJoy[1][0] + 0, "mah a"           },
	{ "P2 E",           BIT_DIGITAL,   MatrixJoy[1][0] + 1, "mah e"           },
	{ "P2 I",           BIT_DIGITAL,   MatrixJoy[1][0] + 2, "mah i"           },
	{ "P2 M",           BIT_DIGITAL,   MatrixJoy[1][0] + 3, "mah m"           },
	{ "P2 Kan",         BIT_DIGITAL,   MatrixJoy[1][0] + 4, "mah kan"         },
	{ "P2 Start",       BIT_DIGITAL,   MatrixJoy[1][0] + 5, "p2 start"        },
	{ "P2 B",           BIT_DIGITAL,   MatrixJoy[1][1] + 0, "mah b"           },
	{ "P2 F",           BIT_DIGITAL,   MatrixJoy[1][1] + 1, "mah f"           },
	{ "P2 J",           BIT_DIGITAL,   MatrixJoy[1][1] + 2, "mah j"           },
	{ "P2 N",           BIT_DIGITAL,   MatrixJoy[1][1] + 3, "mah n"           },
	{ "P2 Reach",       BIT_DIGITAL,   MatrixJoy[1][1] + 4, "mah reach"       },
	{ "P2 Bet",         BIT_DIGITAL,   MatrixJoy[1][1] + 5, "mah bet"         },
	{ "P2 C",           BIT_DIGITAL,   MatrixJoy[1][2] + 0, "mah c"           },
	{ "P2 G",           BIT_DIGITAL,   MatrixJoy[1][2] + 1, "mah g"           },
	{ "P2 K",           BIT_DIGITAL,   MatrixJoy[1][2] + 2, "mah k"           },
	{ "P2 Chi",         BIT_DIGITAL,   MatrixJoy[1][2] + 3, "mah chi"         },
	{ "P2 Ron",         BIT_DIGITAL,   MatrixJoy[1][2] + 4, "mah ron"         },
	{ "P2 D",           BIT_DIGITAL,   MatrixJoy[1][3] + 0, "mah d"           },
	{ "P2 H",           BIT_DIGITAL,   MatrixJoy[1][3] + 1, "mah h"           },
	{ "P2 L",           BIT_DIGITAL,   MatrixJoy[1][3] + 2, "mah l"           },
	{ "P2 Pon",         BIT_DIGITAL,   MatrixJoy[1][3] + 3, "mah pon"         },
	{ "P2 Last Chance", BIT_DIGITAL,   MatrixJoy[1][4] + 0, "mah last chance" },
	{ "P2 Score",       BIT_DIGITAL,   MatrixJoy[1][4] + 1, "mah score"       },
	{ "P2 Double Up",   BIT_DIGITAL,   MatrixJoy[1][4] + 2, "mah double up"   },
	{ "P2 Flip Flop",   BIT_DIGITAL,   MatrixJoy[1][4] + 3, "mah flip flop"   },
	{ "P2 Big",         BIT_DIGITAL,   MatrixJoy[1][4] + 4, "mah big"         },
	{ "P2 Small",       BIT_DIGITAL,   MatrixJoy[1][4] + 5, "mah small"       },
	{ "Reset",          BIT_DIGITAL,   &DrvReset,           "reset"           },
	{ "Dip A",          BIT_DIPSWITCH, Dips + 0,            "dip"             },
	{ "Dip B",          BIT_DIPSWITCH, Dips + 1,            "dip"             },
	{ "Dip C",          BIT_DIPSWITCH, Dips + 2,            "dip"             },
	{ "Dip D",          BIT_DIPSWITCH, Dips + 3,            "dip"             },
	{ "Dip E",          BIT_DIPSWITCH, Dips + 4,            "dip"             },
	{ "Dip F",          BIT_DIPSWITCH, Dips + 5,            "dip"             },
	{ "Dip G",          BIT_DIPSWITCH, Dips + 6,            "dip"             },
	{ "Dip H",          BIT_DIPSWITCH, Dips + 7,            "dip"             },
};

STDINPUTINFO(sryudens)

static struct BurnDIPInfo sryudensDIPList[] = {
	{ 0x3d, 0xff, 0xff, 0xf7, NULL                   },
	{ 0x3e, 0xff, 0xff, 0xff, NULL                   },
	{ 0x3f, 0xff, 0xff, 0xff, NULL                   },
	{ 0x40, 0xff, 0xff, 0xfc, NULL                   },
	{ 0x41, 0xff, 0xff, 0xef, NULL                   },
	{ 0x42, 0xff, 0xff, 0xff, NULL                   },
	{ 0x43, 0xff, 0xff, 0xff, NULL                   },
	{ 0x44, 0xff, 0xff, 0xff, NULL                   },

	{ 0,    0xfe, 0,    16,   "Pay Out Rate (%)"     },
	{ 0x3d, 1,    0x0f, 0x00, "50"                   },
	{ 0x3d, 1,    0x0f, 0x01, "53"                   },
	{ 0x3d, 1,    0x0f, 0x02, "56"                   },
	{ 0x3d, 1,    0x0f, 0x03, "59"                   },
	{ 0x3d, 1,    0x0f, 0x04, "62"                   },
	{ 0x3d, 1,    0x0f, 0x05, "65"                   },
	{ 0x3d, 1,    0x0f, 0x06, "68"                   },
	{ 0x3d, 1,    0x0f, 0x07, "71"                   },
	{ 0x3d, 1,    0x0f, 0x08, "75"                   },
	{ 0x3d, 1,    0x0f, 0x09, "78"                   },
	{ 0x3d, 1,    0x0f, 0x0a, "81"                   },
	{ 0x3d, 1,    0x0f, 0x0b, "84"                   },
	{ 0x3d, 1,    0x0f, 0x0c, "87"                   },
	{ 0x3d, 1,    0x0f, 0x0d, "90"                   },
	{ 0x3d, 1,    0x0f, 0x0e, "93"                   },
	{ 0x3d, 1,    0x0f, 0x0f, "96"                   },

	{ 0,    0xfe, 0,    4,    "Odds Rate"            },
	{ 0x3d, 1,    0x30, 0x30, "1 2 4 8 12 16 24 32"  },
	{ 0x3d, 1,    0x30, 0x00, "1 2 3 5 8 15 30 50"   },
	{ 0x3d, 1,    0x30, 0x20, "2 3 6 8 12 15 30 50"  },
	{ 0x3d, 1,    0x30, 0x10, "1 2 3 5 10 25 50 100" },

	{ 0,    0xfe, 0,    4,    "Max Rate"             },
	{ 0x3d, 1,    0xc0, 0xc0, "1"                    },
	{ 0x3d, 1,    0xc0, 0x80, "5"                    },
	{ 0x3d, 1,    0xc0, 0x40, "10"                   },
	{ 0x3d, 1,    0xc0, 0x00, "20"                   },

	{ 0,    0xfe, 0,    4,    "Coin A"               },
	{ 0x3e, 1,    0x03, 0x03, "1C 1C"                },
	{ 0x3e, 1,    0x03, 0x02, "1C 2C"                },
	{ 0x3e, 1,    0x03, 0x01, "1C 5C"                },
	{ 0x3e, 1,    0x03, 0x00, "1C 10C"               },

	{ 0,    0xfe, 0,    4,    "Min Rate To Play"     },
	{ 0x3e, 1,    0x0c, 0x0c, "1"                    },
	{ 0x3e, 1,    0x0c, 0x08, "2"                    },
	{ 0x3e, 1,    0x0c, 0x04, "3"                    },
	{ 0x3e, 1,    0x0c, 0x00, "5"                    },

	{ 0,    0xfe, 0,    4,    "Payout"               },
	{ 0x3e, 1,    0x30, 0x30, "300"                  },
	{ 0x3e, 1,    0x30, 0x20, "500"                  },
	{ 0x3e, 1,    0x30, 0x10, "700"                  },
	{ 0x3e, 1,    0x30, 0x00, "1000"                 },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3e, 1,    0x40, 0x40, "Off"                  },
	{ 0x3e, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3e, 1,    0x80, 0x80, "Off"                  },
	{ 0x3e, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    8,    "YAKUMAN Bonus"        },
	{ 0x3f, 1,    0x07, 0x07, "Cut"                  },
	{ 0x3f, 1,    0x07, 0x06, "1 T"                  },
	{ 0x3f, 1,    0x07, 0x05, "300"                  },
	{ 0x3f, 1,    0x07, 0x04, "500"                  },
	{ 0x3f, 1,    0x07, 0x03, "700"                  },
	{ 0x3f, 1,    0x07, 0x02, "1000"                 },
	{ 0x3f, 1,    0x07, 0x01, "1000"                 },
	{ 0x3f, 1,    0x07, 0x00, "1000"                 },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3f, 1,    0x08, 0x08, "Off"                  },
	{ 0x3f, 1,    0x08, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3f, 1,    0x10, 0x10, "Off"                  },
	{ 0x3f, 1,    0x10, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "DonDen Key"           },
	{ 0x3f, 1,    0x20, 0x20, "Start"                },
	{ 0x3f, 1,    0x20, 0x00, "Flip Flop"            },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3f, 1,    0x40, 0x40, "Off"                  },
	{ 0x3f, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x3f, 1,    0x80, 0x80, "Off"                  },
	{ 0x3f, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Demo Sounds"          },
	{ 0x40, 1,    0x01, 0x01, "Off"                  },
	{ 0x40, 1,    0x01, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "In Game Music"        },
	{ 0x40, 1,    0x02, 0x02, "Off"                  },
	{ 0x40, 1,    0x02, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x40, 1,    0x04, 0x04, "Off"                  },
	{ 0x40, 1,    0x04, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x40, 1,    0x08, 0x08, "Off"                  },
	{ 0x40, 1,    0x08, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Undress Girl"         },
	{ 0x40, 1,    0x10, 0x10, "Off"                  },
	{ 0x40, 1,    0x10, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x40, 1,    0x20, 0x20, "Off"                  },
	{ 0x40, 1,    0x20, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x40, 1,    0x40, 0x40, "Off"                  },
	{ 0x40, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x40, 1,    0x80, 0x80, "Off"                  },
	{ 0x40, 1,    0x80, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Credits Per Note"     },
	{ 0x41, 1,    0x01, 0x01, "5"                    },
	{ 0x41, 1,    0x01, 0x00, "10"                   },

	{ 0,    0xfe, 0,    2,    "Flip Screen"          },
	{ 0x41, 1,    0x02, 0x02, "Off"                  },
	{ 0x41, 1,    0x02, 0x00, "On"                   },

	{ 0,    0xfe, 0,    4,    "Unknown"              },
	{ 0x41, 1,    0x0c, 0x0c, "0"                    },
	{ 0x41, 1,    0x0c, 0x00, "1"                    },
	{ 0x41, 1,    0x0c, 0x04, "3"                    },
	{ 0x41, 1,    0x0c, 0x08, "7"                    },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x41, 1,    0x10, 0x10, "Off"                  },
	{ 0x41, 1,    0x10, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x41, 1,    0x20, 0x20, "Off"                  },
	{ 0x41, 1,    0x20, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x41, 1,    0x40, 0x40, "Off"                  },
	{ 0x41, 1,    0x40, 0x00, "On"                   },

	{ 0,    0xfe, 0,    2,    "Controls"             },
	{ 0x41, 1,    0x80, 0x80, "Keyboard"             },
	{ 0x41, 1,    0x80, 0x00, "Joystick"             },

	{ 0,    0xfe, 0,    2,    "Bets?"                },
	{ 0x43, 1,    0x40, 0x40, "0"                    },
	{ 0x43, 1,    0x40, 0x00, "1"                    },

	{ 0,    0xfe, 0,    2,    "Unknown"              },
	{ 0x44, 1,    0x40, 0x40, "Off"                  },
	{ 0x44, 1,    0x40, 0x00, "On"                   },
};

STDDIPINFO(sryudens)

static struct BurnInputInfo ultrchmpInputList[] = {
	{ "P1 Start",    BIT_DIGITAL,   Joy[0] + 0, "p1 start"  },
	{ "P1 Button 1", BIT_DIGITAL,   Joy[0] + 1, "p1 fire 1" },
	{ "P1 Button 2", BIT_DIGITAL,   Joy[0] + 2, "p1 fire 2" },
	{ "P1 Button 3", BIT_DIGITAL,   Joy[0] + 3, "p1 fire 3" },
	{ "P1 Button 4", BIT_DIGITAL,   Joy[0] + 4, "p1 fire 4" },
	{ "P2 Start",    BIT_DIGITAL,   Joy[1] + 0, "p2 start"  },
	{ "P2 Button 1", BIT_DIGITAL,   Joy[1] + 1, "p2 fire 1" },
	{ "P2 Button 2", BIT_DIGITAL,   Joy[1] + 2, "p2 fire 2" },
	{ "P2 Button 3", BIT_DIGITAL,   Joy[1] + 3, "p2 fire 3" },
	{ "P2 Button 4", BIT_DIGITAL,   Joy[1] + 4, "p2 fire 4" },
	{ "P1 Coin",     BIT_DIGITAL,   Joy[2] + 0, "p1 coin"   },
	{ "P2 Coin",     BIT_DIGITAL,   Joy[2] + 1, "p2 coin"   },
	{ "Service",     BIT_DIGITAL,   Joy[2] + 2, "service"   },
	{ "Test",        BIT_DIGITAL,   Joy[2] + 3, "diag"      },
	{ "Reset",       BIT_DIGITAL,   &DrvReset,  "reset"     },
	{ "Dip A",       BIT_DIPSWITCH, Dips + 0,   "dip"       },
	{ "Dip B",       BIT_DIPSWITCH, Dips + 1,   "dip"       },
	{ "Dip C",       BIT_DIPSWITCH, Dips + 2,   "dip"       },
	{ "Dip D",       BIT_DIPSWITCH, Dips + 3,   "dip"       },
	{ "Dip E",       BIT_DIPSWITCH, Dips + 4,   "dip"       },
	{ "Dip F",       BIT_DIPSWITCH, Dips + 5,   "dip"       },
	{ "Dip G",       BIT_DIPSWITCH, Dips + 6,   "dip"       },
	{ "Dip H",       BIT_DIPSWITCH, Dips + 7,   "dip"       },
};

STDINPUTINFO(ultrchmp)

static struct BurnDIPInfo ultrchmpDIPList[] = {
	{ 0x0f, 0xff, 0xff, 0xff, NULL },
	{ 0x10, 0xff, 0xff, 0xff, NULL },
	{ 0x11, 0xff, 0xff, 0xff, NULL },
	{ 0x12, 0xff, 0xff, 0xff, NULL },
	{ 0x13, 0xff, 0xff, 0xff, NULL },
	{ 0x14, 0xff, 0xff, 0xff, NULL },
	{ 0x15, 0xff, 0xff, 0xff, NULL },
	{ 0x16, 0xff, 0xff, 0xff, NULL },
};

STDDIPINFO(ultrchmp)

static struct BurnRomInfo htengokuRomDesc[] = {
	{ "6501.4b",  0x40000, 0x29a7fc83, 1 | BRF_PRG | BRF_ESS },

	{ "6506.4c",  0x80000, 0x7de17b26, 2 | BRF_GRA           },
	{ "6507.5c",  0x20000, 0xced3155b, 2 | BRF_GRA           },
	{ "6508.6c",  0x20000, 0xca46ed48, 2 | BRF_GRA           },
	{ "6505.2b",  0x20000, 0x161058fd, 2 | BRF_GRA           },
	{ "6504.1b",  0x20000, 0xb2ca9838, 2 | BRF_GRA           },
	{ "6503.2a",  0x20000, 0x6ac42304, 2 | BRF_GRA           },
	{ "6502.1a",  0x20000, 0x9276a10a, 2 | BRF_GRA           },
	{ "6509.10b", 0x80000, 0xf8524c28, 2 | BRF_GRA           },
	{ "6510.11b", 0x20000, 0x0fdd6edf, 2 | BRF_GRA           },
};

STD_ROM_PICK(htengoku)

STD_ROM_FN(htengoku)

static const RomLoad htengokuLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x100000, 1, 0x0, 0x20000 },
	{ 3, 1, 0x120000, 1, 0x0, 0x20000 },
	{ 4, 1, 0x140000, 1, 0x0, 0x20000 },
	{ 5, 1, 0x160000, 1, 0x0, 0x20000 },
	{ 6, 1, 0x180000, 1, 0x0, 0x20000 },
	{ 7, 1, 0x1a0000, 1, 0x0, 0x20000 },
	{ 8, 1, 0x200000, 1, 0x0, 0x80000 },
	{ 9, 1, 0x280000, 1, 0x0, 0x20000 },
};

static struct BurnRomInfo mmpanicRomDesc[] = {
	{ "nwc7002a", 0x40000,  0x725b337f, 1 | BRF_PRG | BRF_ESS },

	{ "nwc7003",  0x20000,  0x4f02ce44, 4 | BRF_PRG | BRF_ESS },

	{ "nwc7004",  0x100000, 0x5b4ad8c5, 2 | BRF_GRA           },
	{ "nwc7005",  0x100000, 0x9ec41956, 2 | BRF_GRA           },
	{ "nwc7006a", 0x80000,  0x9099c571, 2 | BRF_GRA           },

	{ "nwc7001a", 0x40000,  0x1ae3660d, 3 | BRF_SND           },
};

STD_ROM_PICK(mmpanic)

STD_ROM_FN(mmpanic)

static const RomLoad mmpanicLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000  },
	{ 1, 3, 0x0,      1, 0x0, 0x20000  },
	{ 2, 1, 0x0,      1, 0x0, 0x100000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x100000 },
	{ 4, 1, 0x200000, 1, 0x0, 0x80000  },
	{ 5, 2, 0x0,      1, 0x0, 0x40000  },
};

static struct BurnRomInfo mjmyorn2RomDesc[] = {
	{ "3d",  0x40000, 0x7ef07c1e, 1 | BRF_PRG | BRF_ESS },

	{ "12b", 0x80000, 0x1debae36, 2 | BRF_GRA           },
	{ "11b", 0x80000, 0xfe9953f7, 2 | BRF_GRA           },
	{ "10b", 0x80000, 0x6d0fd29a, 2 | BRF_GRA           },
	{ "9b",  0x40000, 0x36228e6a, 2 | BRF_GRA           },

	{ "1e",  0x20000, 0x09b7a9b2, 3 | BRF_SND           },
};

STD_ROM_PICK(mjmyorn2)

STD_ROM_FN(mjmyorn2)

static const RomLoad mjmyorn2Loads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x180000, 1, 0x0, 0x40000 },
	{ 5, 2, 0x0,      1, 0x0, 0x20000 },
	{ 5, 2, 0x20000,  1, 0x0, 0x20000 },
};

static struct BurnRomInfo mjmyorntRomDesc[] = {
	{ "7102.3d",  0x40000,  0x058f779b, 1 | BRF_PRG | BRF_ESS },

	{ "7105.12b", 0x100000, 0x35ca0079, 2 | BRF_GRA           },
	{ "7104.11b", 0x80000,  0x6d0fd29a, 2 | BRF_GRA           },
	{ "7103.10b", 0x20000,  0x88511487, 2 | BRF_GRA           },

	{ "7101.1e",  0x20000,  0x09b7a9b2, 3 | BRF_SND           },
};

STD_ROM_PICK(mjmyornt)

STD_ROM_FN(mjmyornt)

static const RomLoad mjmyorntLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000  },
	{ 1, 1, 0x0,      1, 0x0, 0x100000 },
	{ 2, 1, 0x100000, 1, 0x0, 0x80000  },
	{ 3, 1, 0x180000, 1, 0x0, 0x20000  },
	{ 4, 2, 0x0,      1, 0x0, 0x20000  },
};

static struct BurnRomInfo funkyfigRomDesc[] = {
	{ "7403.3c",  0x80000,  0xad0f5e14, 1 | BRF_PRG | BRF_ESS },

	{ "7401.1h",  0x20000,  0x0f47d785, 4 | BRF_PRG | BRF_ESS },

	{ "7404.8b",  0x80000,  0xaa4ddf32, 2 | BRF_GRA           },
	{ "7405.9b",  0x80000,  0xfc125bd8, 2 | BRF_GRA           },
	{ "7406.10b", 0x80000,  0x04a214b1, 2 | BRF_GRA           },
	{ "7407.11b", 0x80000,  0x7c794189, 2 | BRF_GRA           },
	{ "7409.4b",  0x100000, 0x064082c3, 2 | BRF_GRA           },
	{ "7410.3b",  0x100000, 0x0ba67874, 2 | BRF_GRA           },
	{ "7408.13b", 0x100000, 0x9efe4c60, 2 | BRF_GRA           },
	{ "7411.2b",  0x100000, 0x1e9c73dc, 2 | BRF_GRA           },

	{ "7402.1e",  0x40000,  0x5038cc34, 3 | BRF_SND           },
};

STD_ROM_PICK(funkyfig)

STD_ROM_FN(funkyfig)

static const RomLoad funkyfigLoads[] = {
	{ 0,  0, 0x0,      1, 0x0, 0x80000  },
	{ 0,  0, 0x10000,  1, 0x0, 0x80000  },
	{ 1,  3, 0x0,      1, 0x0, 0x20000  },
	{ 2,  1, 0x0,      1, 0x0, 0x80000  },
	{ 3,  1, 0x80000,  1, 0x0, 0x80000  },
	{ 4,  1, 0x100000, 1, 0x0, 0x80000  },
	{ 5,  1, 0x180000, 1, 0x0, 0x80000  },
	{ 6,  1, 0x200000, 1, 0x0, 0x100000 },
	{ 7,  1, 0x280000, 1, 0x0, 0x100000 },
	{ 8,  1, 0x300000, 1, 0x0, 0x100000 },
	{ 9,  1, 0x400000, 1, 0x0, 0x100000 },
	{ 10, 2, 0x0,      1, 0x0, 0x40000  },
};

static struct BurnRomInfo funkyfigaRomDesc[] = {
	{ "7403.3c",  0x80000,  0x2e68c8a0, 1 | BRF_PRG | BRF_ESS },

	{ "7401.1h",  0x20000,  0x0f47d785, 4 | BRF_PRG | BRF_ESS },

	{ "7404.8b",  0x100000, 0x5e60f3f5, 2 | BRF_GRA           },
	{ "7405.9b",  0x100000, 0xb100b696, 2 | BRF_GRA           },
	{ "7406.10b", 0x100000, 0x6a00492a, 2 | BRF_GRA           },
	{ "7407.11b", 0x100000, 0x9efe4c60, 2 | BRF_GRA           },
	{ "7408.13b", 0x80000,  0x1a947f3b, 2 | BRF_GRA           },

	{ "7402.1e",  0x40000,  0x5038cc34, 3 | BRF_SND           },
};

STD_ROM_PICK(funkyfiga)

STD_ROM_FN(funkyfiga)

static const RomLoad funkyfigaLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000  },
	{ 1, 3, 0x0,      1, 0x0, 0x20000  },
	{ 2, 1, 0x0,      1, 0x0, 0x100000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x100000 },
	{ 4, 1, 0x200000, 1, 0x0, 0x100000 },
	{ 5, 1, 0x300000, 1, 0x0, 0x100000 },
	{ 6, 1, 0x400000, 1, 0x0, 0x80000  },
	{ 7, 2, 0x0,      1, 0x0, 0x40000  },
};

static struct BurnRomInfo quizchqRomDesc[] = {
	{ "nwc7302.3e",  0x80000, 0x14217f2d, 1 | BRF_PRG | BRF_ESS },
	{ "nwc7303.4e",  0x80000, 0xffc77601, 1 | BRF_PRG | BRF_ESS },

	{ "nwc7307.s4b", 0x80000, 0xa09d1dbe, 2 | BRF_GRA           },
	{ "nwc7306.s3b", 0x80000, 0x52d27aac, 2 | BRF_GRA           },
	{ "nwc7305.s2b", 0x80000, 0x5f50914e, 2 | BRF_GRA           },
	{ "nwc7304.s1b", 0x80000, 0x72866919, 2 | BRF_GRA           },
	{ "nwc7310.s4a", 0x80000, 0x5939aeab, 2 | BRF_GRA           },
	{ "nwc7309.s3a", 0x80000, 0x88c863b2, 2 | BRF_GRA           },
	{ "nwc7308.s2a", 0x20000, 0x6eb5c81d, 2 | BRF_GRA           },

	{ "nwc7301.1f",  0x80000, 0x52c672e8, 3 | BRF_SND           },
};

STD_ROM_PICK(quizchq)

STD_ROM_FN(quizchq)

static const RomLoad quizchqLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000 },
	{ 1, 0, 0x90000,  1, 0x0, 0x80000 },
	{ 2, 1, 0x0,      1, 0x0, 0x80000 },
	{ 3, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 4, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 5, 1, 0x180000, 1, 0x0, 0x80000 },
	{ 6, 1, 0x200000, 1, 0x0, 0x80000 },
	{ 7, 1, 0x280000, 1, 0x0, 0x80000 },
	{ 8, 1, 0x300000, 1, 0x0, 0x20000 },
	{ 9, 2, 0x0,      1, 0x0, 0x80000 },
};

static struct BurnRomInfo quizchqlRomDesc[] = {
	{ "2.rom",    0x80000, 0x1bf8fb25, 1 | BRF_PRG | BRF_ESS },
	{ "3.rom",    0x80000, 0x6028198f, 1 | BRF_PRG | BRF_ESS },

	{ "4.rom",    0x80000, 0xe6bdea31, 2 | BRF_GRA           },
	{ "5.rom",    0x80000, 0xc243f10a, 2 | BRF_GRA           },
	{ "11.rom",   0x80000, 0xc9ae5880, 2 | BRF_GRA           },
	{ "7.rom",    0x80000, 0xa490aa4e, 2 | BRF_GRA           },
	{ "6.rom",    0x80000, 0xfbf713b6, 2 | BRF_GRA           },
	{ "8.rom",    0x80000, 0x68d4b79f, 2 | BRF_GRA           },
	{ "10.rom",   0x80000, 0xd56eaf0e, 2 | BRF_GRA           },
	{ "9.rom",    0x80000, 0xa11d535a, 2 | BRF_GRA           },
	{ "12.rom",   0x20000, 0x43f8e5c7, 2 | BRF_GRA           },

	{ "1snd.rom", 0x80000, 0xcebb9220, 3 | BRF_SND           },
};

STD_ROM_PICK(quizchql)

STD_ROM_FN(quizchql)

static const RomLoad quizchqlLoads[] = {
	{ 0,  0, 0x0,      1, 0x0, 0x80000 },
	{ 0,  0, 0x10000,  1, 0x0, 0x80000 },
	{ 1,  0, 0x90000,  1, 0x0, 0x80000 },
	{ 2,  1, 0x0,      1, 0x0, 0x80000 },
	{ 3,  1, 0x80000,  1, 0x0, 0x80000 },
	{ 4,  1, 0x100000, 1, 0x0, 0x80000 },
	{ 5,  1, 0x180000, 1, 0x0, 0x80000 },
	{ 6,  1, 0x200000, 1, 0x0, 0x80000 },
	{ 7,  1, 0x280000, 1, 0x0, 0x80000 },
	{ 8,  1, 0x300000, 1, 0x0, 0x80000 },
	{ 9,  1, 0x380000, 1, 0x0, 0x80000 },
	{ 10, 1, 0x400000, 1, 0x0, 0x20000 },
	{ 11, 2, 0x0,      1, 0x0, 0x80000 },
};

static struct BurnRomInfo quizchqkRomDesc[] = {
	{ "kmi002q.bin", 0x80000,  0xee15687c, 1 | BRF_PRG | BRF_ESS },
	{ "kmi003q.bin", 0x80000,  0xcf6be97f, 1 | BRF_PRG | BRF_ESS },

	{ "kmi004q.bin", 0x100000, 0xa4c888c3, 2 | BRF_GRA           },
	{ "kmi005q.bin", 0x100000, 0x83c87805, 2 | BRF_GRA           },
	{ "kmi006q.bin", 0x100000, 0xcf88c42b, 2 | BRF_GRA           },

	{ "kmi001q.bin", 0x80000,  0x6a9cd570, 3 | BRF_SND           },
};

STD_ROM_PICK(quizchqk)

STD_ROM_FN(quizchqk)

static const RomLoad quizchqkLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000  },
	{ 1, 0, 0x90000,  1, 0x0, 0x80000  },
	{ 2, 1, 0x0,      1, 0x0, 0x100000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x100000 },
	{ 4, 1, 0x200000, 1, 0x0, 0x100000 },
	{ 5, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo animaljrRomDesc[] = {
	{ "7502a.2e",  0x40000,  0x78aa0f24, 1 | BRF_PRG | BRF_ESS },

	{ "7503a.8e",  0x20000,  0xa7032aae, 4 | BRF_PRG | BRF_ESS },

	{ "7504.17b",  0x100000, 0xb62de6a3, 2 | BRF_GRA           },
	{ "7505.17d",  0x80000,  0x729b073f, 2 | BRF_GRA           },
	{ "7506a.17f", 0x80000,  0x21fb7d86, 2 | BRF_GRA           },

	{ "7501a.1h",  0x40000,  0x52174727, 3 | BRF_SND           },
};

STD_ROM_PICK(animaljr)

STD_ROM_FN(animaljr)

static const RomLoad animaljrLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000  },
	{ 1, 3, 0x0,      1, 0x0, 0x20000  },
	{ 2, 1, 0x0,      1, 0x0, 0x100000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x80000  },
	{ 4, 1, 0x180000, 1, 0x0, 0x80000  },
	{ 5, 2, 0x0,      1, 0x0, 0x40000  },
};

static struct BurnRomInfo animaljrsRomDesc[] = {
	{ "7502s.2e",  0x40000,  0x4b14a4be, 1 | BRF_PRG | BRF_ESS },

	{ "7503s.8e",  0x20000,  0xd1fac899, 4 | BRF_PRG | BRF_ESS },

	{ "7504.17b",  0x100000, 0xb62de6a3, 2 | BRF_GRA           },
	{ "7505.17d",  0x80000,  0x729b073f, 2 | BRF_GRA           },
	{ "7506s.17f", 0x80000,  0x1be1ae17, 2 | BRF_GRA           },

	{ "7501s.1h",  0x40000,  0x59debb66, 3 | BRF_SND           },
};

STD_ROM_PICK(animaljrs)

STD_ROM_FN(animaljrs)

static const RomLoad animaljrsLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000  },
	{ 1, 3, 0x0,      1, 0x0, 0x20000  },
	{ 2, 1, 0x0,      1, 0x0, 0x100000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x80000  },
	{ 4, 1, 0x180000, 1, 0x0, 0x80000  },
	{ 5, 2, 0x0,      1, 0x0, 0x40000  },
};

static struct BurnRomInfo animaljrjRomDesc[] = {
	{ "nwc_7502.2e",  0x40000,  0xc526cf56, 1 | BRF_PRG | BRF_ESS },

	{ "nwc_7503.8e",  0x20000,  0x9c27e0b6, 4 | BRF_PRG | BRF_ESS },

	{ "nwc_7504.17b", 0x100000, 0xb62de6a3, 2 | BRF_GRA           },
	{ "nwc_7505.17d", 0x80000,  0x729b073f, 2 | BRF_GRA           },

	{ "nwc_7501.1h",  0x40000,  0xc821e589, 3 | BRF_SND           },
};

STD_ROM_PICK(animaljrj)

STD_ROM_FN(animaljrj)

static const RomLoad animaljrjLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000  },
	{ 1, 3, 0x0,      1, 0x0, 0x20000  },
	{ 2, 1, 0x0,      1, 0x0, 0x100000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x80000  },
	{ 4, 2, 0x0,      1, 0x0, 0x40000  },
};

static struct BurnRomInfo mjmysterRomDesc[] = {
	{ "77t2.c3",  0x40000, 0xb1427cce, 1 | BRF_PRG | BRF_ESS },

	{ "77t6.b12", 0x80000, 0xa287589a, 2 | BRF_GRA           },
	{ "77t5.b11", 0x80000, 0xa3475059, 2 | BRF_GRA           },
	{ "77t4.b10", 0x80000, 0xf45c24d6, 2 | BRF_GRA           },
	{ "77t3.b9",  0x20000, 0x8671165b, 2 | BRF_GRA           },

	{ "77t1.d1",  0x20000, 0x09b7a9b2, 3 | BRF_SND           },
};

STD_ROM_PICK(mjmyster)

STD_ROM_FN(mjmyster)

static const RomLoad mjmysterLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x180000, 1, 0x0, 0x20000 },
	{ 5, 2, 0x0,      1, 0x0, 0x20000 },
};

static struct BurnRomInfo mjmywrldRomDesc[] = {
	{ "2.3d",  0x40000,  0xa1ff31fa, 1 | BRF_PRG | BRF_ESS },

	{ "5.12b", 0x100000, 0xa1f26722, 2 | BRF_GRA           },
	{ "4.11b", 0x80000,  0xf45c24d6, 2 | BRF_GRA           },
	{ "3.10b", 0x20000,  0x8671165b, 2 | BRF_GRA           },

	{ "1.1e",  0x20000,  0x09b7a9b2, 3 | BRF_SND           },
};

STD_ROM_PICK(mjmywrld)

STD_ROM_FN(mjmywrld)

static const RomLoad mjmywrldLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000  },
	{ 1, 1, 0x0,      1, 0x0, 0x100000 },
	{ 2, 1, 0x100000, 1, 0x0, 0x80000  },
	{ 3, 1, 0x180000, 1, 0x0, 0x20000  },
	{ 4, 2, 0x0,      1, 0x0, 0x20000  },
};

static struct BurnRomInfo hgingaRomDesc[] = {
	{ "8101-1.2b", 0x40000, 0x923c70c5, 1 | BRF_PRG | BRF_ESS },

	{ "8102.9a",   0x80000, 0x0074af23, 2 | BRF_GRA           },
	{ "8103.10a",  0x80000, 0xa3a4ecb5, 2 | BRF_GRA           },
	{ "8104.11a",  0x20000, 0x24513af9, 2 | BRF_GRA           },
};

STD_ROM_PICK(hginga)

STD_ROM_FN(hginga)

static const RomLoad hgingaLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x20000 },
	{ 3, 1, 0x120000, 1, 0x0, 0x20000 },
	{ 3, 1, 0x140000, 1, 0x0, 0x20000 },
	{ 3, 1, 0x160000, 1, 0x0, 0x20000 },
};

static struct BurnRomInfo hgingaaRomDesc[] = {
	{ "8101.2b",  0x40000, 0x77a64b71, 1 | BRF_PRG | BRF_ESS },

	{ "8102.9a",  0x80000, 0x0074af23, 2 | BRF_GRA           },
	{ "8103.10a", 0x80000, 0xa3a4ecb5, 2 | BRF_GRA           },
	{ "8104.11a", 0x20000, 0x24513af9, 2 | BRF_GRA           },
};

STD_ROM_PICK(hgingaa)

STD_ROM_FN(hgingaa)

static const RomLoad hgingaaLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x20000 },
	{ 3, 1, 0x120000, 1, 0x0, 0x20000 },
	{ 3, 1, 0x140000, 1, 0x0, 0x20000 },
	{ 3, 1, 0x160000, 1, 0x0, 0x20000 },
};

static struct BurnRomInfo mjmyunivRomDesc[] = {
	{ "2.3d",  0x40000, 0x3284d714, 1 | BRF_PRG | BRF_ESS },

	{ "5.11b", 0x80000, 0xa287589a, 2 | BRF_GRA           },
	{ "6.12b", 0x80000, 0xa3475059, 2 | BRF_GRA           },
	{ "3.9b",  0x80000, 0xf45c24d6, 2 | BRF_GRA           },
	{ "4.10b", 0x80000, 0x57fb94cc, 2 | BRF_GRA           },

	{ "1.1e",  0x20000, 0x09b7a9b2, 3 | BRF_SND           },
};

STD_ROM_PICK(mjmyuniv)

STD_ROM_FN(mjmyuniv)

static const RomLoad mjmyunivLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x400000, 1, 0x0, 0x80000 },
	{ 2, 1, 0x480000, 1, 0x0, 0x80000 },
	{ 3, 1, 0x500000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x580000, 1, 0x0, 0x80000 },
	{ 5, 2, 0x0,      1, 0x0, 0x20000 },
};

static struct BurnRomInfo mjmyunivhRomDesc[] = {
	{ "rom.u42", 0x40000, 0x8000eec3, 1 | BRF_PRG | BRF_ESS },

	{ "rom.u59", 0x80000, 0xa287589a, 2 | BRF_GRA           },
	{ "rom.u60", 0x80000, 0x33dc6d59, 2 | BRF_GRA           },
	{ "rom.u57", 0x80000, 0xf45c24d6, 2 | BRF_GRA           },
	{ "rom.u58", 0x80000, 0x9af9a939, 2 | BRF_GRA           },

	{ "rom.u30", 0x40000, 0x30e496a6, 3 | BRF_SND           },
};

STD_ROM_PICK(mjmyunivh)

STD_ROM_FN(mjmyunivh)

static const RomLoad mjmyunivhLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x800000, 1, 0x0, 0x80000 },
	{ 2, 1, 0x880000, 1, 0x0, 0x80000 },
	{ 3, 1, 0x900000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x980000, 1, 0x0, 0x80000 },
	{ 5, 2, 0x0,      1, 0x0, 0x40000 },
};

static struct BurnRomInfo mjmywrldtRomDesc[] = {
	{ "romb.u47", 0x40000, 0xeb447ce4, 1 | BRF_PRG | BRF_ESS },

	{ "romf.u60", 0x80000, 0xa287589a, 2 | BRF_GRA           },
	{ "rome.u59", 0x80000, 0xa3475059, 2 | BRF_GRA           },
	{ "romd.u58", 0x80000, 0xf45c24d6, 2 | BRF_GRA           },
	{ "romc.u57", 0x80000, 0xd9e4a01e, 2 | BRF_GRA           },

	{ "roma.u30", 0x40000, 0xf9bf11fa, 3 | BRF_SND           },
};

STD_ROM_PICK(mjmywrldt)

STD_ROM_FN(mjmywrldt)

static const RomLoad mjmywrldtLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x200000, 1, 0x0, 0x80000 },
	{ 2, 1, 0x280000, 1, 0x0, 0x80000 },
	{ 3, 1, 0x300000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x380000, 1, 0x0, 0x80000 },
	{ 5, 2, 0x0,      1, 0x0, 0x40000 },
};

static struct BurnRomInfo quiz365RomDesc[] = {
	{ "7805.4b",  0x80000,  0x70f93543, 1 | BRF_PRG | BRF_ESS },
	{ "7804.4d",  0x80000,  0x2ae003f4, 1 | BRF_PRG | BRF_ESS },
	{ "7803.3b",  0x40000,  0x10d315b1, 1 | BRF_PRG | BRF_ESS },
	{ "7802.3d",  0x40000,  0x6616caa3, 1 | BRF_PRG | BRF_ESS },

	{ "7810.14b", 0x100000, 0x4b1a4984, 2 | BRF_GRA           },
	{ "7809.13b", 0x100000, 0x139d52ab, 2 | BRF_GRA           },
	{ "7808.12b", 0x80000,  0xa09fd4a4, 2 | BRF_GRA           },
	{ "7807.11b", 0x80000,  0x988b3e84, 2 | BRF_GRA           },
	{ "7806.10b", 0x80000,  0x7f9aa228, 2 | BRF_GRA           },

	{ "7801.1fu", 0x80000,  0x53519d67, 3 | BRF_SND           },
	{ "7801.1fd", 0x80000,  0x448c58dd, 3 | BRF_SND           },
};

STD_ROM_PICK(quiz365)

STD_ROM_FN(quiz365)

static const RomLoad quiz365Loads[] = {
	{ 0,  0, 0x1,      2, 0x0, 0x80000  },
	{ 1,  0, 0x0,      2, 0x0, 0x80000  },
	{ 2,  0, 0x100001, 2, 0x0, 0x40000  },
	{ 3,  0, 0x100000, 2, 0x0, 0x40000  },
	{ 4,  1, 0x0,      1, 0x0, 0x100000 },
	{ 5,  1, 0x100000, 1, 0x0, 0x100000 },
	{ 6,  1, 0x200000, 1, 0x0, 0x80000  },
	{ 7,  1, 0x280000, 1, 0x0, 0x80000  },
	{ 8,  1, 0x300000, 1, 0x0, 0x80000  },
	{ 9,  2, 0x0,      1, 0x0, 0x80000  },
	{ 10, 2, 0x80000,  1, 0x0, 0x80000  },
};

static struct BurnRomInfo quiz365tRomDesc[] = {
	{ "7805.rom", 0x80000,  0x6db33222, 1 | BRF_PRG | BRF_ESS },
	{ "7804.rom", 0x80000,  0x46d04ace, 1 | BRF_PRG | BRF_ESS },
	{ "7803.rom", 0x40000,  0x5b7a78d3, 1 | BRF_PRG | BRF_ESS },
	{ "7802.rom", 0x40000,  0xc3238a9d, 1 | BRF_PRG | BRF_ESS },

	{ "7810.rom", 0x100000, 0x4b1a4984, 2 | BRF_GRA           },
	{ "7809.rom", 0x100000, 0x139d52ab, 2 | BRF_GRA           },
	{ "7808.rom", 0x80000,  0xa09fd4a4, 2 | BRF_GRA           },
	{ "7806.rom", 0x100000, 0x75767c6f, 2 | BRF_GRA           },
	{ "7807.rom", 0x80000,  0x60fb1dfe, 2 | BRF_GRA           },

	{ "7801.rom", 0x100000, 0x285cc62a, 3 | BRF_SND           },
};

STD_ROM_PICK(quiz365t)

STD_ROM_FN(quiz365t)

static const RomLoad quiz365tLoads[] = {
	{ 0, 0, 0x1,      2, 0x0,     0x80000  },
	{ 1, 0, 0x0,      2, 0x0,     0x80000  },
	{ 2, 0, 0x100001, 2, 0x0,     0x40000  },
	{ 3, 0, 0x100000, 2, 0x0,     0x40000  },
	{ 4, 1, 0x0,      1, 0x0,     0x100000 },
	{ 5, 1, 0x100000, 1, 0x0,     0x100000 },
	{ 6, 1, 0x200000, 1, 0x0,     0x80000  },
	{ 7, 1, 0x280000, 1, 0x0,     0x100000 },
	{ 8, 1, 0x380000, 1, 0x0,     0x80000  },
	{ 9, 2, 0x80000,  1, 0x0,     0x80000  },
	{ 9, 2, 0x0,      1, 0x80000, 0x80000  },
};

static struct BurnRomInfo rongrongRomDesc[] = {
	{ "8002e.3e",  0x80000, 0x062fa1b6, 1 | BRF_PRG | BRF_ESS },

	{ "8003.8c",   0x80000, 0xf57192e5, 2 | BRF_GRA           },
	{ "8004.9c",   0x80000, 0xc8c0b5cb, 2 | BRF_GRA           },
	{ "8005e.10c", 0x80000, 0x11c7a23c, 2 | BRF_GRA           },
	{ "8006e.11c", 0x80000, 0x137e9b83, 2 | BRF_GRA           },
	{ "8007e.12c", 0x80000, 0x374a1d50, 2 | BRF_GRA           },

	{ "8001w.2f",  0x40000, 0x8edc87a2, 3 | BRF_SND           },
};

STD_ROM_PICK(rongrong)

STD_ROM_FN(rongrong)

static const RomLoad rongrongLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x180000, 1, 0x0, 0x80000 },
	{ 5, 1, 0x200000, 1, 0x0, 0x80000 },
	{ 6, 2, 0x0,      1, 0x0, 0x40000 },
};

static struct BurnRomInfo rongrongjRomDesc[] = {
	{ "8002.3e",  0x80000, 0x27142684, 1 | BRF_PRG | BRF_ESS },

	{ "8003.8c",  0x80000, 0xf57192e5, 2 | BRF_GRA           },
	{ "8004.9c",  0x80000, 0xc8c0b5cb, 2 | BRF_GRA           },
	{ "8005.10c", 0x80000, 0xd1e5f74c, 2 | BRF_GRA           },
	{ "8006.11c", 0x80000, 0xbcbd1b0b, 2 | BRF_GRA           },
	{ "8007.12c", 0x40000, 0xc76cbb69, 2 | BRF_GRA           },

	{ "8001.2f",  0x40000, 0x9fc8a367, 3 | BRF_SND           },
};

STD_ROM_PICK(rongrongj)

STD_ROM_FN(rongrongj)

static const RomLoad rongrongjLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x180000, 1, 0x0, 0x80000 },
	{ 5, 1, 0x200000, 1, 0x0, 0x40000 },
	{ 6, 2, 0x0,      1, 0x0, 0x40000 },
};

static struct BurnRomInfo rongronggRomDesc[] = {
	{ "rr_8002g.rom", 0x80000, 0x9a5d2885, 1 | BRF_PRG | BRF_ESS },

	{ "rr_8003.rom",  0x80000, 0xf57192e5, 2 | BRF_GRA           },
	{ "rr_8004.rom",  0x80000, 0xc8c0b5cb, 2 | BRF_GRA           },
	{ "rr_8005g.rom", 0x80000, 0x11c7a23c, 2 | BRF_GRA           },
	{ "rr_8006g.rom", 0x80000, 0xf3de77e6, 2 | BRF_GRA           },
	{ "rr_8007g.rom", 0x80000, 0x38a8caa3, 2 | BRF_GRA           },

	{ "rr_8001w.rom", 0x40000, 0x8edc87a2, 3 | BRF_SND           },
};

STD_ROM_PICK(rongrongg)

STD_ROM_FN(rongrongg)

static const RomLoad rongronggLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x180000, 1, 0x0, 0x80000 },
	{ 5, 1, 0x200000, 1, 0x0, 0x80000 },
	{ 6, 2, 0x0,      1, 0x0, 0x40000 },
};

static struct BurnRomInfo hparadisRomDesc[] = {
	{ "1012.3e", 0x40000, 0xbd3a3a8f, 1 | BRF_PRG | BRF_ESS },

	{ "1015.3a", 0x80000, 0xea7b282e, 2 | BRF_GRA           },
	{ "1014.2a", 0x80000, 0x5f057c13, 2 | BRF_GRA           },
	{ "1016.4a", 0x80000, 0xc8e7ffb6, 2 | BRF_GRA           },
	{ "1013.1a", 0x80000, 0xc26b0563, 2 | BRF_GRA           },

	{ "1011.2f", 0x40000, 0x8c852b1a, 3 | BRF_SND           },
};

STD_ROM_PICK(hparadis)

STD_ROM_FN(hparadis)

static const RomLoad hparadisLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x400000, 1, 0x0, 0x80000 },
	{ 2, 1, 0x480000, 1, 0x0, 0x80000 },
	{ 3, 1, 0x500000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x580000, 1, 0x0, 0x80000 },
	{ 5, 2, 0x0,      1, 0x0, 0x40000 },
};

static struct BurnRomInfo rselectRomDesc[] = {
	{ "8302.2b",  0x80000,  0x941df21e, 1 | BRF_PRG | BRF_ESS },

	{ "8303.9a",  0x100000, 0x5e22b02f, 2 | BRF_GRA           },
	{ "8304.10a", 0x80000,  0x39b9dc72, 2 | BRF_GRA           },

	{ "8301.2d",  0x40000,  0x428e3c15, 3 | BRF_SND           },
};

STD_ROM_PICK(rselect)

STD_ROM_FN(rselect)

static const RomLoad rselectLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x100000 },
	{ 2, 1, 0x100000, 1, 0x0, 0x80000  },
	{ 3, 2, 0x0,      1, 0x0, 0x40000  },
};

static struct BurnRomInfo hgokouRomDesc[] = {
	{ "1082b.2b", 0x40000, 0xe33bc5a5, 1 | BRF_PRG | BRF_ESS },

	{ "1083.9a",  0x80000, 0x054200c3, 2 | BRF_GRA           },
	{ "1084.10a", 0x80000, 0x49f657e8, 2 | BRF_GRA           },

	{ "1081.2d",  0x40000, 0x74dede40, 3 | BRF_SND           },
};

STD_ROM_PICK(hgokou)

STD_ROM_FN(hgokou)

static const RomLoad hgokouLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 2, 1, 0x180000, 1, 0x0, 0x80000 },
	{ 3, 2, 0x0,      1, 0x0, 0x40000 },
};

static struct BurnRomInfo hgokouaRomDesc[] = {
	{ "1082a.2b", 0x40000, 0x3ddd4a5f, 1 | BRF_PRG | BRF_ESS },

	{ "1083.9a",  0x80000, 0x054200c3, 2 | BRF_GRA           },
	{ "1084.10a", 0x80000, 0x49f657e8, 2 | BRF_GRA           },

	{ "1081.2d",  0x40000, 0x74dede40, 3 | BRF_SND           },
};

STD_ROM_PICK(hgokoua)

STD_ROM_FN(hgokoua)

static const RomLoad hgokouaLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 2, 1, 0x180000, 1, 0x0, 0x80000 },
	{ 3, 2, 0x0,      1, 0x0, 0x40000 },
};

static struct BurnRomInfo hgokbangRomDesc[] = {
	{ "1162.2b",  0x40000, 0x02414b42, 1 | BRF_PRG | BRF_ESS },

	{ "1163.9a",  0x80000, 0x054200c3, 2 | BRF_GRA           },
	{ "1164.10a", 0x80000, 0x25b40754, 2 | BRF_GRA           },

	{ "1161.2d",  0x40000, 0x74dede40, 3 | BRF_SND           },
};

STD_ROM_PICK(hgokbang)

STD_ROM_FN(hgokbang)

static const RomLoad hgokbangLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x400000, 1, 0x0, 0x80000 },
	{ 2, 1, 0x480000, 1, 0x0, 0x80000 },
	{ 3, 2, 0x0,      1, 0x0, 0x40000 },
};

static struct BurnRomInfo mjdchukaRomDesc[] = {
	{ "2.5b",  0x80000, 0x7957b4e7, 1 | BRF_PRG | BRF_ESS },

	{ "3.11c", 0x80000, 0xc66553c3, 2 | BRF_GRA           },
	{ "4.11a", 0x80000, 0x972852fb, 2 | BRF_GRA           },

	{ "1.1b",  0x80000, 0x9759c65e, 3 | BRF_SND           },
};

STD_ROM_PICK(mjdchuka)

STD_ROM_FN(mjdchuka)

static const RomLoad mjdchukaLoads[] = {
	{ 0, 0, 0x0, 1, 0x0, 0x80000 },
	{ 1, 1, 0x0, 2, 0x0, 0x80000 },
	{ 2, 1, 0x1, 2, 0x0, 0x80000 },
	{ 3, 2, 0x0, 1, 0x0, 0x80000 },
};

static struct BurnRomInfo mjschukaRomDesc[] = {
	{ "1153.5c",  0x80000, 0x67cf10db, 1 | BRF_PRG | BRF_ESS },

	{ "1154.11b", 0x80000, 0xb8d04189, 2 | BRF_GRA           },
	{ "1155.12b", 0x80000, 0x4208edcf, 2 | BRF_GRA           },

	{ "1151.1c",  0x80000, 0xc92065a9, 3 | BRF_SND           },
	{ "1152.2c",  0x20000, 0xf9244532, 3 | BRF_SND           },
};

STD_ROM_PICK(mjschuka)

STD_ROM_FN(mjschuka)

static const RomLoad mjschukaLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 3, 2, 0x0,      1, 0x0, 0x80000 },
	{ 3, 2, 0x100000, 1, 0x0, 0x20000 },
	{ 4, 2, 0x80000,  1, 0x0, 0x20000 },
	{ 4, 2, 0x180000, 1, 0x0, 0x20000 },
	{ 4, 2, 0xa0000,  1, 0x0, 0x20000 },
	{ 4, 2, 0x1a0000, 1, 0x0, 0x20000 },
	{ 4, 2, 0xc0000,  1, 0x0, 0x20000 },
	{ 4, 2, 0x1c0000, 1, 0x0, 0x20000 },
};

static struct BurnRomInfo nettoqcRomDesc[] = {
	{ "10305.rom", 0x80000,  0xebb14a1f, 1 | BRF_PRG | BRF_ESS },
	{ "10303.rom", 0x80000,  0x30c114c3, 1 | BRF_PRG | BRF_ESS },
	{ "10306.rom", 0x40000,  0xf19fe827, 1 | BRF_PRG | BRF_ESS },
	{ "10304.rom", 0x80000,  0xda1f56e5, 1 | BRF_PRG | BRF_ESS },

	{ "10307.rom", 0x100000, 0xc7a3b05f, 2 | BRF_GRA           },
	{ "10308.rom", 0x100000, 0x416807a1, 2 | BRF_GRA           },
	{ "10309.rom", 0x100000, 0x81841272, 2 | BRF_GRA           },
	{ "10310.rom", 0x80000,  0x0f790cda, 2 | BRF_GRA           },
	{ "10311.rom", 0x80000,  0x41109231, 2 | BRF_GRA           },

	{ "10301.rom", 0x80000,  0x52afd952, 3 | BRF_SND           },
	{ "10302.rom", 0x80000,  0x6e2d4660, 3 | BRF_SND           },
};

STD_ROM_PICK(nettoqc)

STD_ROM_FN(nettoqc)

static const RomLoad nettoqcLoads[] = {
	{ 0,  0, 0x1,      2, 0x0,     0x80000  },
	{ 1,  0, 0x0,      2, 0x0,     0x80000  },
	{ 2,  0, 0x100001, 2, 0x0,     0x40000  },
	{ 3,  0, 0x100000, 2, 0x0,     0x40000  },
	{ 3,  0, 0x100000, 2, 0x40000, 0x40000  },
	{ 4,  1, 0x0,      1, 0x0,     0x100000 },
	{ 5,  1, 0x100000, 1, 0x0,     0x100000 },
	{ 6,  1, 0x200000, 1, 0x0,     0x100000 },
	{ 7,  1, 0x300000, 1, 0x0,     0x80000  },
	{ 8,  1, 0x380000, 1, 0x0,     0x80000  },
	{ 9,  2, 0x0,      1, 0x0,     0x80000  },
	{ 10, 2, 0x80000,  1, 0x0,     0x80000  },
};

static struct BurnRomInfo ultrchmpRomDesc[] = {
	{ "5.1a",     0x80000,  0x23afa373, 1 | BRF_PRG | BRF_ESS },
	{ "3.1c",     0x80000,  0x073e1959, 1 | BRF_PRG | BRF_ESS },
	{ "4.1b",     0x40000,  0xe99ad8b6, 1 | BRF_PRG | BRF_ESS },
	{ "2.1d",     0x40000,  0x90e3ee61, 1 | BRF_PRG | BRF_ESS },

	{ "10402.2b", 0x200000, 0x6414d46f, 2 | BRF_GRA           },
	{ "10403.2a", 0x200000, 0xb646fa00, 2 | BRF_GRA           },
	{ "6.2c",     0x40000,  0x28171d0f, 2 | BRF_GRA           },

	{ "10401.2h", 0x200000, 0x7ea88e86, 3 | BRF_SND           },
	{ "1.1h",     0x20000,  0x65f3df4c, 3 | BRF_SND           },
};

STD_ROM_PICK(ultrchmp)

STD_ROM_FN(ultrchmp)

static const RomLoad ultrchmpLoads[] = {
	{ 0, 0, 0x1,      2, 0x0, 0x80000  },
	{ 1, 0, 0x0,      2, 0x0, 0x80000  },
	{ 2, 0, 0x100001, 2, 0x0, 0x40000  },
	{ 3, 0, 0x100000, 2, 0x0, 0x40000  },
	{ 4, 1, 0x0,      1, 0x0, 0x200000 },
	{ 5, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 6, 1, 0x400000, 1, 0x0, 0x40000  },
	{ 7, 2, 0x0,      1, 0x0, 0x200000 },
	{ 7, 2, 0x200000, 1, 0x0, 0x200000 },
	{ 8, 2, 0x200000, 1, 0x0, 0x20000  },
};

static struct BurnRomInfo ultrchmphRomDesc[] = {
	{ "1145h.1a", 0x80000,  0xaec83a82, 1 | BRF_PRG | BRF_ESS },
	{ "1143h.1c", 0x80000,  0x7ab05a4c, 1 | BRF_PRG | BRF_ESS },
	{ "1144h.1b", 0x40000,  0xbc413cce, 1 | BRF_PRG | BRF_ESS },
	{ "1142h.1d", 0x40000,  0xd4a70092, 1 | BRF_PRG | BRF_ESS },

	{ "1148h.2b", 0x200000, 0x6414d46f, 2 | BRF_GRA           },
	{ "1149h.2a", 0x200000, 0xb646fa00, 2 | BRF_GRA           },
	{ "1147h.2c", 0x40000,  0x3ed82868, 2 | BRF_GRA           },

	{ "1146h.2h", 0x200000, 0x7ea88e86, 3 | BRF_SND           },
	{ "1141h.1h", 0x20000,  0x98b7501c, 3 | BRF_SND           },
};

STD_ROM_PICK(ultrchmph)

STD_ROM_FN(ultrchmph)

static const RomLoad ultrchmphLoads[] = {
	{ 0, 0, 0x1,      2, 0x0, 0x80000  },
	{ 1, 0, 0x0,      2, 0x0, 0x80000  },
	{ 2, 0, 0x100001, 2, 0x0, 0x40000  },
	{ 3, 0, 0x100000, 2, 0x0, 0x40000  },
	{ 4, 1, 0x0,      1, 0x0, 0x200000 },
	{ 5, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 6, 1, 0x400000, 1, 0x0, 0x40000  },
	{ 7, 2, 0x0,      1, 0x0, 0x200000 },
	{ 7, 2, 0x200000, 1, 0x0, 0x200000 },
	{ 8, 2, 0x200000, 1, 0x0, 0x20000  },
};

static struct BurnRomInfo ddenlovjRomDesc[] = {
	{ "1124.2d", 0x40000, 0x6a9ec557, 1 | BRF_PRG | BRF_ESS },
	{ "1123.2h", 0x40000, 0xd41cbed0, 1 | BRF_PRG | BRF_ESS },

	{ "1125.2b", 0x80000, 0x0181f53c, 2 | BRF_GRA           },
	{ "1126.2a", 0x80000, 0x17ff2df4, 2 | BRF_GRA           },
	{ "1127.3d", 0x80000, 0x9c136914, 2 | BRF_GRA           },
	{ "1128.3c", 0x80000, 0x757c9941, 2 | BRF_GRA           },
	{ "1129.3b", 0x80000, 0x957bc57e, 2 | BRF_GRA           },

	{ "1122.2m", 0x80000, 0xe8619d66, 3 | BRF_SND           },
	{ "1121.2n", 0x80000, 0xe2b8359e, 3 | BRF_SND           },
};

STD_ROM_PICK(ddenlovj)

STD_ROM_FN(ddenlovj)

static const RomLoad ddenlovjLoads[] = {
	{ 0, 0, 0x1,      2, 0x0, 0x40000 },
	{ 1, 0, 0x0,      2, 0x0, 0x40000 },
	{ 2, 1, 0xc00000, 1, 0x0, 0x80000 },
	{ 3, 1, 0xc80000, 1, 0x0, 0x80000 },
	{ 4, 1, 0xd00000, 1, 0x0, 0x80000 },
	{ 5, 1, 0xd80000, 1, 0x0, 0x80000 },
	{ 6, 1, 0xe00000, 1, 0x0, 0x80000 },
	{ 7, 2, 0x80000,  1, 0x0, 0x80000 },
	{ 8, 2, 0x100000, 1, 0x0, 0x80000 },
};

static struct BurnRomInfo ddenlovrkRomDesc[] = {
	{ "rom.1a", 0x40000, 0x868c45f8, 1 | BRF_PRG | BRF_ESS },
	{ "rom.1b", 0x40000, 0x4fab3c90, 1 | BRF_PRG | BRF_ESS },

	{ "rom.2a", 0x80000, 0xee143d8e, 2 | BRF_GRA           },
	{ "rom.2b", 0x80000, 0x58a662be, 2 | BRF_GRA           },
	{ "rom.2c", 0x80000, 0xf96e0708, 2 | BRF_GRA           },
	{ "rom.2d", 0x80000, 0xb47e27ec, 2 | BRF_GRA           },
	{ "rom.2e", 0x80000, 0x7c7beef6, 2 | BRF_GRA           },

	{ "rom.1e", 0x80000, 0xa49318df, 3 | BRF_SND           },
	{ "rom.1f", 0x80000, 0x9df4f029, 3 | BRF_SND           },
};

STD_ROM_PICK(ddenlovrk)

STD_ROM_FN(ddenlovrk)

static const RomLoad ddenlovrkLoads[] = {
	{ 0, 0, 0x1,      2, 0x0,     0x40000 },
	{ 1, 0, 0x0,      2, 0x0,     0x40000 },
	{ 2, 1, 0x0,      1, 0x0,     0x80000 },
	{ 3, 1, 0x80000,  1, 0x0,     0x80000 },
	{ 4, 1, 0x100000, 1, 0x0,     0x80000 },
	{ 5, 1, 0x180000, 1, 0x0,     0x80000 },
	{ 6, 1, 0x200000, 1, 0x0,     0x80000 },
	{ 7, 2, 0x80000,  1, 0x0,     0x40000 },
	{ 7, 2, 0x180000, 1, 0x40000, 0x40000 },
	{ 8, 2, 0x40000,  1, 0x0,     0x40000 },
	{ 8, 2, 0x140000, 1, 0x40000, 0x40000 },
};

static struct BurnRomInfo ddenlovrbRomDesc[] = {
	{ "rom2", 0x40000, 0xcabdf78f, 1 | BRF_PRG | BRF_ESS },
	{ "rom3", 0x40000, 0x36f8d05e, 1 | BRF_PRG | BRF_ESS },

	{ "rom4", 0x80000, 0xee143d8e, 2 | BRF_GRA           },
	{ "rom5", 0x80000, 0x58a662be, 2 | BRF_GRA           },
	{ "rom6", 0x80000, 0xf96e0708, 2 | BRF_GRA           },
	{ "rom7", 0x80000, 0xb47e27ec, 2 | BRF_GRA           },
	{ "rom8", 0x80000, 0x7c7beef6, 2 | BRF_GRA           },

	{ "rom1", 0x80000, 0xba4723e8, 3 | BRF_SND           },
};

STD_ROM_PICK(ddenlovrb)

STD_ROM_FN(ddenlovrb)

static const RomLoad ddenlovrbLoads[] = {
	{ 0, 0, 0x1,      2, 0x0, 0x40000 },
	{ 1, 0, 0x0,      2, 0x0, 0x40000 },
	{ 2, 1, 0x0,      1, 0x0, 0x80000 },
	{ 3, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 4, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 5, 1, 0x180000, 1, 0x0, 0x80000 },
	{ 6, 1, 0x200000, 1, 0x0, 0x80000 },
	{ 7, 2, 0x0,      1, 0x0, 0x80000 },
};

static struct BurnRomInfo ddenlovrRomDesc[] = {
	{ "1134h.1a", 0x40000, 0x43accdff, 1 | BRF_PRG | BRF_ESS },
	{ "1133h.1c", 0x40000, 0x361bf7b6, 1 | BRF_PRG | BRF_ESS },

	{ "1135h.3h", 0x80000, 0xee143d8e, 2 | BRF_GRA           },
	{ "1136h.3f", 0x80000, 0x58a662be, 2 | BRF_GRA           },
	{ "1137h.3e", 0x80000, 0xf96e0708, 2 | BRF_GRA           },
	{ "1138h.3d", 0x80000, 0x633cff33, 2 | BRF_GRA           },
	{ "1139h.3c", 0x80000, 0xbe1189ca, 2 | BRF_GRA           },

	{ "1131h.1f", 0x80000, 0x32f68241, 3 | BRF_SND           },
	{ "1132h.1e", 0x80000, 0x2de6363d, 3 | BRF_SND           },
};

STD_ROM_PICK(ddenlovr)

STD_ROM_FN(ddenlovr)

static const RomLoad ddenlovrLoads[] = {
	{ 0, 0, 0x1,      2, 0x0, 0x40000 },
	{ 1, 0, 0x0,      2, 0x0, 0x40000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x80000 },
	{ 3, 1, 0x280000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x300000, 1, 0x0, 0x80000 },
	{ 5, 1, 0x380000, 1, 0x0, 0x80000 },
	{ 6, 1, 0x400000, 1, 0x0, 0x80000 },
	{ 7, 2, 0x80000,  1, 0x0, 0x80000 },
	{ 8, 2, 0x100000, 1, 0x0, 0x80000 },
};

static struct BurnRomInfo mjxyslRomDesc[] = {
	{ "b.5b",  0x80000, 0x3b0761c9, 1 | BRF_PRG | BRF_ESS },

	{ "c.11c", 0x80000, 0x83bfc841, 2 | BRF_GRA           },
	{ "d.11a", 0x80000, 0x1bf6220a, 2 | BRF_GRA           },
	{ "e.12c", 0x20000, 0xbec823ac, 2 | BRF_GRA           },
	{ "f.12a", 0x20000, 0xa1030162, 2 | BRF_GRA           },

	{ "a.1b",  0x80000, 0xce32dcbc, 3 | BRF_SND           },
};

STD_ROM_PICK(mjxysl)

STD_ROM_FN(mjxysl)

static const RomLoad mjxyslLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 1, 1, 0x0,      2, 0x0, 0x80000 },
	{ 2, 1, 0x1,      2, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 2, 0x0, 0x20000 },
	{ 4, 1, 0x100001, 2, 0x0, 0x20000 },
	{ 5, 2, 0x0,      1, 0x0, 0x80000 },
};

static struct BurnRomInfo hanakanzRomDesc[] = {
	{ "50720.5b",  0x80000, 0xdc40fcfc, 1 | BRF_PRG | BRF_ESS },

	{ "50740.8b",  0x80000, 0x999e70ce, 2 | BRF_GRA           },
	{ "50730.8c",  0x80000, 0x54e1731d, 2 | BRF_GRA           },
	{ "50760.10b", 0x80000, 0x8fcb5da3, 2 | BRF_GRA           },
	{ "50750.10c", 0x80000, 0x0e58bf9e, 2 | BRF_GRA           },
	{ "50780.12b", 0x80000, 0x6dfd8a86, 2 | BRF_GRA           },
	{ "50770.12c", 0x80000, 0x118e6baf, 2 | BRF_GRA           },

	{ "50710.1c",  0x80000, 0x72ae072f, 3 | BRF_SND           },
};

STD_ROM_PICK(hanakanz)

STD_ROM_FN(hanakanz)

static const RomLoad hanakanzLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 1, 1, 0x0,      2, 0x0, 0x80000 },
	{ 2, 1, 0x1,      2, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 2, 0x0, 0x80000 },
	{ 4, 1, 0x100001, 2, 0x0, 0x80000 },
	{ 5, 1, 0x200000, 2, 0x0, 0x80000 },
	{ 6, 1, 0x200001, 2, 0x0, 0x80000 },
	{ 7, 2, 0x0,      1, 0x0, 0x80000 },
};

static struct BurnRomInfo kotbinyoRomDesc[] = {
	{ "prg.5b",  0x80000, 0x673c90d5, 1 | BRF_PRG | BRF_ESS },

	{ "gfx.8b",  0x80000, 0x126f3591, 2 | BRF_GRA           },
	{ "gfx.8c",  0x80000, 0xab52b33d, 2 | BRF_GRA           },
	{ "gfx.10b", 0x80000, 0x2e9d35f9, 2 | BRF_GRA           },
	{ "gfx.10c", 0x80000, 0x83851ae1, 2 | BRF_GRA           },
	{ "gfx.12b", 0x40000, 0xbf5ae6c2, 2 | BRF_GRA           },
	{ "gfx.12c", 0x40000, 0x2f476026, 2 | BRF_GRA           },

	{ "snd.1c",  0x40000, 0xd3a739a7, 3 | BRF_SND           },
};

STD_ROM_PICK(kotbinyo)

STD_ROM_FN(kotbinyo)

static const RomLoad kotbinyoLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 1, 1, 0x0,      2, 0x0, 0x80000 },
	{ 2, 1, 0x1,      2, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 2, 0x0, 0x80000 },
	{ 4, 1, 0x100001, 2, 0x0, 0x80000 },
	{ 5, 1, 0x200000, 2, 0x0, 0x40000 },
	{ 6, 1, 0x200001, 2, 0x0, 0x40000 },
	{ 7, 2, 0x0,      1, 0x0, 0x40000 },
	{ 7, 2, 0x40000,  1, 0x0, 0x40000 },
};

static struct BurnRomInfo kotbinyosuRomDesc[] = {
	{ "prg.5b",  0x80000, 0xce35efe7, 1 | BRF_PRG | BRF_ESS },

	{ "gfx.8b",  0x80000, 0x126f3591, 2 | BRF_GRA           },
	{ "gfx.8c",  0x80000, 0xab52b33d, 2 | BRF_GRA           },
	{ "gfx.10b", 0x80000, 0x2e9d35f9, 2 | BRF_GRA           },
	{ "gfx.10c", 0x80000, 0x83851ae1, 2 | BRF_GRA           },
	{ "gfx.12b", 0x40000, 0xbf5ae6c2, 2 | BRF_GRA           },
	{ "gfx.12c", 0x40000, 0x2f476026, 2 | BRF_GRA           },

	{ "snd.1c",  0x40000, 0xd3a739a7, 3 | BRF_SND           },
};

STD_ROM_PICK(kotbinyosu)

STD_ROM_FN(kotbinyosu)

static const RomLoad kotbinyosuLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 1, 1, 0x0,      2, 0x0, 0x80000 },
	{ 2, 1, 0x1,      2, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 2, 0x0, 0x80000 },
	{ 4, 1, 0x100001, 2, 0x0, 0x80000 },
	{ 5, 1, 0x200000, 2, 0x0, 0x40000 },
	{ 6, 1, 0x200001, 2, 0x0, 0x40000 },
	{ 7, 2, 0x0,      1, 0x0, 0x40000 },
	{ 7, 2, 0x40000,  1, 0x0, 0x40000 },
};

static struct BurnRomInfo kotbinspRomDesc[] = {
	{ "prg.5c",    0x80000,  0xc917f791, 1 | BRF_PRG | BRF_ESS },

	{ "909036.8b", 0x100000, 0xc468bdda, 2 | BRF_GRA           },
	{ "909035.8c", 0x100000, 0xcea4dbfa, 2 | BRF_GRA           },
	{ "909034.6b", 0x80000,  0x9f366a2a, 2 | BRF_GRA           },
	{ "909033.6c", 0x80000,  0x9388b85d, 2 | BRF_GRA           },

	{ "909031.1c", 0x80000,  0x9f20a531, 3 | BRF_SND           },
};

STD_ROM_PICK(kotbinsp)

STD_ROM_FN(kotbinsp)

static const RomLoad kotbinspLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      2, 0x0, 0x100000 },
	{ 2, 1, 0x1,      2, 0x0, 0x100000 },
	{ 3, 1, 0x200000, 2, 0x0, 0x80000  },
	{ 4, 1, 0x200001, 2, 0x0, 0x80000  },
	{ 5, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo akamaruRomDesc[] = {
	{ "50107.1b", 0x80000,  0x8364d627, 1 | BRF_PRG | BRF_ESS },
	{ "50105.1d", 0x80000,  0x42ff4bec, 1 | BRF_PRG | BRF_ESS },
	{ "50108.1a", 0x80000,  0x1520ecad, 1 | BRF_PRG | BRF_ESS },
	{ "50106.1c", 0x80000,  0x8e081747, 1 | BRF_PRG | BRF_ESS },

	{ "50109.2h", 0x100000, 0xcdff154d, 2 | BRF_GRA           },
	{ "50112.2d", 0x100000, 0x7fdd9853, 2 | BRF_GRA           },
	{ "50110.3h", 0x100000, 0x06ca4d87, 2 | BRF_GRA           },
	{ "50113.3d", 0x80000,  0xc9a571cd, 2 | BRF_GRA           },
	{ "50111.4h", 0x80000,  0xb2de7a3c, 2 | BRF_GRA           },
	{ "50114.4d", 0x80000,  0x2af39dfd, 2 | BRF_GRA           },

	{ "50101.1h", 0x80000,  0x6bef6cab, 3 | BRF_SND           },
	{ "50102.1g", 0x80000,  0x056ac348, 3 | BRF_SND           },
	{ "50104.1e", 0x80000,  0x790f18c1, 3 | BRF_SND           },
	{ "50103.1f", 0x80000,  0xaff3a753, 3 | BRF_SND           },
};

STD_ROM_PICK(akamaru)

STD_ROM_FN(akamaru)

static const RomLoad akamaruLoads[] = {
	{ 0,  0, 0x1,      2, 0x0, 0x80000  },
	{ 1,  0, 0x0,      2, 0x0, 0x80000  },
	{ 2,  0, 0x100001, 2, 0x0, 0x80000  },
	{ 3,  0, 0x100000, 2, 0x0, 0x80000  },
	{ 4,  1, 0x0,      1, 0x0, 0x100000 },
	{ 5,  1, 0x100000, 1, 0x0, 0x100000 },
	{ 6,  1, 0x200000, 1, 0x0, 0x100000 },
	{ 7,  1, 0x300000, 1, 0x0, 0x80000  },
	{ 8,  1, 0x380000, 1, 0x0, 0x80000  },
	{ 9,  1, 0x400000, 1, 0x0, 0x80000  },
	{ 10, 2, 0x80000,  1, 0x0, 0x80000  },
	{ 11, 2, 0x100000, 1, 0x0, 0x80000  },
	{ 12, 2, 0x200000, 1, 0x0, 0x80000  },
	{ 13, 2, 0x280000, 1, 0x0, 0x80000  },
};

static struct BurnRomInfo janshinpRomDesc[] = {
	{ "700j-2.5c",  0x80000, 0x188bae18, 1 | BRF_PRG | BRF_ESS },

	{ "700j-3.13b", 0x80000, 0xd7289433, 2 | BRF_GRA           },
	{ "700j-4.14b", 0x80000, 0x881e1f91, 2 | BRF_GRA           },
	{ "700j-5.13d", 0x80000, 0x5a6bb6a9, 2 | BRF_GRA           },

	{ "700j-1.1c",  0x20000, 0x09b7a9b2, 3 | BRF_SND           },
};

STD_ROM_PICK(janshinp)

STD_ROM_FN(janshinp)

static const RomLoad janshinpLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 4, 2, 0x0,      1, 0x0, 0x20000 },
};

static struct BurnRomInfo dtoyokenRomDesc[] = {
	{ "700d-2.5c",  0x80000, 0xf92a70ad, 1 | BRF_PRG | BRF_ESS },

	{ "700d-3.13b", 0x80000, 0x6215aed3, 2 | BRF_GRA           },
	{ "700d-4.14b", 0x80000, 0x06d2ee0b, 2 | BRF_GRA           },
	{ "700d-5.13d", 0x80000, 0xbe4bc975, 2 | BRF_GRA           },
	{ "700d-6.14d", 0x80000, 0x62487321, 2 | BRF_GRA           },

	{ "700d-1.1c",  0x80000, 0x70e630e6, 3 | BRF_SND           },
};

STD_ROM_PICK(dtoyoken)

STD_ROM_FN(dtoyoken)

static const RomLoad dtoyokenLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x180000, 1, 0x0, 0x80000 },
	{ 5, 2, 0x0,      1, 0x0, 0x80000 },
};

static struct BurnRomInfo sryudensRomDesc[] = {
	{ "50202.5c",  0x80000, 0x7072d3d9, 1 | BRF_PRG | BRF_ESS },

	{ "50203.13b", 0x80000, 0x4da60d60, 2 | BRF_GRA           },
	{ "50204.14b", 0x80000, 0x43d1d705, 2 | BRF_GRA           },
	{ "50205.15b", 0x80000, 0x7c727fc0, 2 | BRF_GRA           },
	{ "50206.13d", 0x80000, 0xc52396dd, 2 | BRF_GRA           },
	{ "50207.14d", 0x80000, 0xcb600774, 2 | BRF_GRA           },
	{ "50208.15d", 0x80000, 0x0b30c780, 2 | BRF_GRA           },
	{ "50209.13f", 0x80000, 0x8f34a31c, 2 | BRF_GRA           },
	{ "50210.14f", 0x80000, 0x2fdd3b49, 2 | BRF_GRA           },
	{ "50211.15f", 0x80000, 0x39ad357a, 2 | BRF_GRA           },

	{ "50201.1c",  0x80000, 0x5a8cd45c, 3 | BRF_SND           },
};

STD_ROM_PICK(sryudens)

STD_ROM_FN(sryudens)

static const RomLoad sryudensLoads[] = {
	{ 0,  0, 0x0,      1, 0x0, 0x80000 },
	{ 0,  0, 0x10000,  1, 0x0, 0x80000 },
	{ 1,  1, 0x0,      1, 0x0, 0x80000 },
	{ 2,  1, 0x80000,  1, 0x0, 0x80000 },
	{ 3,  1, 0x100000, 1, 0x0, 0x80000 },
	{ 4,  1, 0x180000, 1, 0x0, 0x80000 },
	{ 5,  1, 0x200000, 1, 0x0, 0x80000 },
	{ 6,  1, 0x280000, 1, 0x0, 0x80000 },
	{ 7,  1, 0x300000, 1, 0x0, 0x80000 },
	{ 8,  1, 0x380000, 1, 0x0, 0x80000 },
	{ 9,  1, 0x400000, 1, 0x0, 0x80000 },
	{ 10, 2, 0x0,      1, 0x0, 0x80000 },
};

static struct BurnRomInfo seljan2RomDesc[] = {
	{ "5572.4c",  0x80000,  0xfb99be5a, 1 | BRF_PRG | BRF_ESS },

	{ "5573.11c", 0x200000, 0x917ef80e, 2 | BRF_GRA           },
	{ "5574.13c", 0x100000, 0x260fb823, 2 | BRF_GRA           },

	{ "5571.1c",  0x80000,  0x5a8cd45c, 3 | BRF_SND           },
};

STD_ROM_PICK(seljan2)

STD_ROM_FN(seljan2)

static const RomLoad seljan2Loads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x400000, 1, 0x0, 0x100000 },
	{ 3, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo seljan2aRomDesc[] = {
	{ "50802.5c",  0x80000,  0x2ccc37ad, 1 | BRF_PRG | BRF_ESS },

	{ "50808.13f", 0x100000, 0x97daddfc, 2 | BRF_GRA           },
	{ "50803.13b", 0x100000, 0x5c7ffbdf, 2 | BRF_GRA           },
	{ "50809.15j", 0x100000, 0xe2f7b62a, 2 | BRF_GRA           },
	{ "50806.13d", 0x80000,  0x8f34a31c, 2 | BRF_GRA           },
	{ "50807.15d", 0x80000,  0x2fdd3b49, 2 | BRF_GRA           },
	{ "50805.15b", 0x80000,  0x39ad357a, 2 | BRF_GRA           },
	{ "50804.14b", 0x40000,  0x073b52a7, 2 | BRF_GRA           },

	{ "50801.1c",  0x80000,  0x5a8cd45c, 3 | BRF_SND           },
};

STD_ROM_PICK(seljan2a)

STD_ROM_FN(seljan2a)

static const RomLoad seljan2aLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x100000 },
	{ 2, 1, 0x100000, 1, 0x0, 0x100000 },
	{ 3, 1, 0x200000, 1, 0x0, 0x100000 },
	{ 4, 1, 0x300000, 1, 0x0, 0x80000  },
	{ 5, 1, 0x380000, 1, 0x0, 0x80000  },
	{ 6, 1, 0x400000, 1, 0x0, 0x80000  },
	{ 7, 1, 0x480000, 1, 0x0, 0x40000  },
	{ 8, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo mjfloveRomDesc[] = {
	{ "50004.2e", 0x80000, 0x20afcdec, 1 | BRF_PRG | BRF_ESS },

	{ "50005.2d", 0x80000, 0x6494d5ad, 2 | BRF_GRA           },
	{ "50006.2c", 0x80000, 0xa1d61eb7, 2 | BRF_GRA           },
	{ "50007.2b", 0x80000, 0xd79ea2f5, 2 | BRF_GRA           },
	{ "50009.3h", 0x80000, 0xcba17351, 2 | BRF_GRA           },
	{ "50013.3a", 0x80000, 0xebb7c8bd, 2 | BRF_GRA           },
	{ "50010.3d", 0x80000, 0x62c05df9, 2 | BRF_GRA           },
	{ "50011.3c", 0x80000, 0x7e05f586, 2 | BRF_GRA           },
	{ "50012.3b", 0x40000, 0x9853e5e4, 2 | BRF_GRA           },

	{ "50002.2h", 0x80000, 0x955034db, 3 | BRF_SND           },
	{ "50003.2f", 0x80000, 0x8845734a, 3 | BRF_SND           },
	{ "50001.2j", 0x80000, 0x6c0a93fd, 3 | BRF_SND           },
	{ "50008.3j", 0x40000, 0x4a95b5eb, 3 | BRF_SND           },
};

STD_ROM_PICK(mjflove)

STD_ROM_FN(mjflove)

static const RomLoad mjfloveLoads[] = {
	{ 0,  0, 0x0,      1, 0x0, 0x80000 },
	{ 0,  0, 0x10000,  1, 0x0, 0x80000 },
	{ 1,  1, 0x0,      1, 0x0, 0x80000 },
	{ 2,  1, 0x80000,  1, 0x0, 0x80000 },
	{ 3,  1, 0x100000, 1, 0x0, 0x80000 },
	{ 4,  1, 0x180000, 1, 0x0, 0x80000 },
	{ 5,  1, 0x200000, 1, 0x0, 0x80000 },
	{ 6,  1, 0x280000, 1, 0x0, 0x80000 },
	{ 7,  1, 0x300000, 1, 0x0, 0x80000 },
	{ 8,  1, 0x380000, 1, 0x0, 0x40000 },
	{ 9,  2, 0x0,      1, 0x0, 0x80000 },
	{ 10, 2, 0x80000,  1, 0x0, 0x80000 },
	{ 11, 2, 0x100000, 1, 0x0, 0x80000 },
	{ 12, 2, 0x180000, 1, 0x0, 0x40000 },
	{ 12, 2, 0x1c0000, 1, 0x0, 0x40000 },
};

static struct BurnRomInfo mjmyorntrRomDesc[] = {
	{ "71s2.5b",  0x40000, 0x5a283aca, 1 | BRF_PRG | BRF_ESS },

	{ "71s3.11a", 0x80000, 0x1debae36, 2 | BRF_GRA           },
	{ "71s4.12a", 0x80000, 0xfe9953f7, 2 | BRF_GRA           },
	{ "71s5.11c", 0x80000, 0x6d0fd29a, 2 | BRF_GRA           },
	{ "71s6.12c", 0x40000, 0xbe7c49e3, 2 | BRF_GRA           },

	{ "71s1.1b",  0x20000, 0x09b7a9b2, 3 | BRF_SND           },
};

STD_ROM_PICK(mjmyorntr)

STD_ROM_FN(mjmyorntr)

static const RomLoad mjmyorntrLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x0,      1, 0x0, 0x80000 },
	{ 2, 1, 0x80000,  1, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x180000, 1, 0x0, 0x40000 },
	{ 5, 2, 0x0,      1, 0x0, 0x20000 },
};

static struct BurnRomInfo mjmysterrRomDesc[] = {
	{ "77s2.5c",  0x40000, 0xa2b3a480, 1 | BRF_PRG | BRF_ESS },

	{ "77s3.11a", 0x80000, 0xa287589a, 2 | BRF_GRA           },
	{ "77s4.12a", 0x80000, 0xa3475059, 2 | BRF_GRA           },
	{ "77s5.11c", 0x80000, 0xf45c24d6, 2 | BRF_GRA           },
	{ "77s6.12c", 0x40000, 0xebe64e5a, 2 | BRF_GRA           },

	{ "71s1.1b",  0x20000, 0x09b7a9b2, 3 | BRF_SND           },
};

STD_ROM_PICK(mjmysterr)

STD_ROM_FN(mjmysterr)

static const RomLoad mjmysterrLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x40000 },
	{ 0, 0, 0x10000,  1, 0x0, 0x40000 },
	{ 1, 1, 0x400000, 1, 0x0, 0x80000 },
	{ 2, 1, 0x480000, 1, 0x0, 0x80000 },
	{ 3, 1, 0x500000, 1, 0x0, 0x80000 },
	{ 4, 1, 0x580000, 1, 0x0, 0x20000 },
	{ 5, 2, 0x0,      1, 0x0, 0x20000 },
};

static struct BurnRomInfo hkagerouRomDesc[] = {
	{ "nm5102.5b",  0x80000, 0xc56c0856, 1 | BRF_PRG | BRF_ESS },

	{ "nm5104.8b",  0x80000, 0xe91dd92b, 2 | BRF_GRA           },
	{ "nm5103.8c",  0x80000, 0x4d4e248b, 2 | BRF_GRA           },
	{ "nm5106.10b", 0x80000, 0x0853c32d, 2 | BRF_GRA           },
	{ "nm5105.10c", 0x80000, 0xf109ec10, 2 | BRF_GRA           },
	{ "nm5108.12b", 0x40000, 0xd0a99b19, 2 | BRF_GRA           },
	{ "nm5107.12c", 0x40000, 0x65a0ebbd, 2 | BRF_GRA           },

	{ "nm5101.1c",  0x80000, 0xbf7a397e, 3 | BRF_SND           },
};

STD_ROM_PICK(hkagerou)

STD_ROM_FN(hkagerou)

static const RomLoad hkagerouLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 1, 1, 0xc00000, 2, 0x0, 0x80000 },
	{ 2, 1, 0xc00001, 2, 0x0, 0x80000 },
	{ 3, 1, 0xd00000, 2, 0x0, 0x80000 },
	{ 4, 1, 0xd00001, 2, 0x0, 0x80000 },
	{ 5, 1, 0xe00000, 2, 0x0, 0x40000 },
	{ 6, 1, 0xe00001, 2, 0x0, 0x40000 },
	{ 7, 2, 0x0,      1, 0x0, 0x80000 },
};

static struct BurnRomInfo mjchuukaRomDesc[] = {
	{ "d12102.5b",  0x80000, 0x585a0a8e, 1 | BRF_PRG | BRF_ESS },

	{ "d12103.11c", 0x80000, 0x83bfc841, 2 | BRF_GRA           },
	{ "d12104.11a", 0x80000, 0x1bf6220a, 2 | BRF_GRA           },
	{ "d12105.12c", 0x80000, 0x3424c8ac, 2 | BRF_GRA           },
	{ "d12106.12a", 0x80000, 0x9052bd09, 2 | BRF_GRA           },
	{ "d12107.13c", 0x20000, 0x184afa94, 2 | BRF_GRA           },
	{ "d12108.13a", 0x20000, 0xf8e8558a, 2 | BRF_GRA           },

	{ "d12101.1b",  0x80000, 0x9759c65e, 3 | BRF_SND           },
};

STD_ROM_PICK(mjchuuka)

STD_ROM_FN(mjchuuka)

static const RomLoad mjchuukaLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000 },
	{ 1, 1, 0x0,      2, 0x0, 0x80000 },
	{ 2, 1, 0x1,      2, 0x0, 0x80000 },
	{ 3, 1, 0x100000, 2, 0x0, 0x80000 },
	{ 4, 1, 0x100001, 2, 0x0, 0x80000 },
	{ 5, 1, 0x280000, 2, 0x0, 0x20000 },
	{ 6, 1, 0x280001, 2, 0x0, 0x20000 },
	{ 7, 2, 0x0,      1, 0x0, 0x80000 },
};

static struct BurnRomInfo mjreach1RomDesc[] = {
	{ "52602-n.5b", 0x80000,  0x6bef7978, 1 | BRF_PRG | BRF_ESS },

	{ "52604.8b",   0x100000, 0x6ce01bb4, 2 | BRF_GRA           },
	{ "52603.8c",   0x100000, 0x16d2c169, 2 | BRF_GRA           },
	{ "52606.10b",  0x100000, 0x07fe5dae, 2 | BRF_GRA           },
	{ "52605.10c",  0x100000, 0xb5d57163, 2 | BRF_GRA           },
	{ "52608.12b",  0x80000,  0x2f93dde4, 2 | BRF_GRA           },
	{ "52607.12c",  0x80000,  0x5e685c4d, 2 | BRF_GRA           },

	{ "52601.1c",   0x80000,  0x52666107, 3 | BRF_SND           },
};

STD_ROM_PICK(mjreach1)

STD_ROM_FN(mjreach1)

static const RomLoad mjreach1Loads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      2, 0x0, 0x100000 },
	{ 2, 1, 0x1,      2, 0x0, 0x100000 },
	{ 3, 1, 0x200000, 2, 0x0, 0x100000 },
	{ 4, 1, 0x200001, 2, 0x0, 0x100000 },
	{ 5, 1, 0x400000, 2, 0x0, 0x80000  },
	{ 6, 1, 0x400001, 2, 0x0, 0x80000  },
	{ 7, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo mjreach1aRomDesc[] = {
	{ "52602-1.5b", 0x80000,  0x493cb0ff, 1 | BRF_PRG | BRF_ESS },

	{ "52604.8b",   0x100000, 0x6ce01bb4, 2 | BRF_GRA           },
	{ "52603.8c",   0x100000, 0x16d2c169, 2 | BRF_GRA           },
	{ "52606.10b",  0x100000, 0x07fe5dae, 2 | BRF_GRA           },
	{ "52605.10c",  0x100000, 0xb5d57163, 2 | BRF_GRA           },
	{ "52608.12b",  0x80000,  0x2f93dde4, 2 | BRF_GRA           },
	{ "52607.12c",  0x80000,  0x5e685c4d, 2 | BRF_GRA           },

	{ "52601.1c",   0x80000,  0x52666107, 3 | BRF_SND           },
};

STD_ROM_PICK(mjreach1a)

STD_ROM_FN(mjreach1a)

static const RomLoad mjreach1aLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      2, 0x0, 0x100000 },
	{ 2, 1, 0x1,      2, 0x0, 0x100000 },
	{ 3, 1, 0x200000, 2, 0x0, 0x100000 },
	{ 4, 1, 0x200001, 2, 0x0, 0x100000 },
	{ 5, 1, 0x400000, 2, 0x0, 0x80000  },
	{ 6, 1, 0x400001, 2, 0x0, 0x80000  },
	{ 7, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo jongteiRomDesc[] = {
	{ "53202.5b",  0x80000,  0xfa95a7f2, 1 | BRF_PRG | BRF_ESS },

	{ "53203.7b",  0x200000, 0x55d6522a, 2 | BRF_GRA           },
	{ "53204.8b",  0x200000, 0x4f58a303, 2 | BRF_GRA           },
	{ "53205.10b", 0x200000, 0xd69e0355, 2 | BRF_GRA           },
	{ "53206.12b", 0x200000, 0xf0652395, 2 | BRF_GRA           },

	{ "53201.2a",  0x200000, 0xc53d840c, 3 | BRF_SND           },
};

STD_ROM_PICK(jongtei)

STD_ROM_FN(jongtei)

static const RomLoad jongteiLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 1, 0x400000, 1, 0x0, 0x200000 },
	{ 4, 1, 0x600000, 1, 0x0, 0x200000 },
	{ 5, 2, 0x0,      1, 0x0, 0x200000 },
};

static struct BurnRomInfo jongteiaRomDesc[] = {
	{ "830722.6b",  0x80000,  0x2e2c15b8, 1 | BRF_PRG | BRF_ESS },

	{ "t83073.7b",  0x200000, 0x55d6522a, 2 | BRF_GRA           },
	{ "t83074.8b",  0x200000, 0x4f58a303, 2 | BRF_GRA           },
	{ "t83075.9b",  0x200000, 0xd69e0355, 2 | BRF_GRA           },
	{ "t83076.11b", 0x200000, 0x8b7d5ca2, 2 | BRF_GRA           },

	{ "t83071.2a",  0x200000, 0xc53d840c, 3 | BRF_SND           },
};

STD_ROM_PICK(jongteia)

STD_ROM_FN(jongteia)

static const RomLoad jongteiaLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 1, 0x400000, 1, 0x0, 0x200000 },
	{ 4, 1, 0x600000, 1, 0x0, 0x200000 },
	{ 5, 2, 0x0,      1, 0x0, 0x200000 },
};

static struct BurnRomInfo htsubakiRomDesc[] = {
	{ "53302.5b",  0x80000,  0x91ea64e6, 1 | BRF_PRG | BRF_ESS },

	{ "53303.7b",  0x200000, 0x5be92db3, 2 | BRF_GRA           },
	{ "53304.8b",  0x200000, 0xe6d426ea, 2 | BRF_GRA           },
	{ "53305.10b", 0x200000, 0x9ff82238, 2 | BRF_GRA           },
	{ "53306.12b", 0x200000, 0x54b2a248, 2 | BRF_GRA           },

	{ "53301.2a",  0x200000, 0xe1ae834a, 3 | BRF_SND           },
};

STD_ROM_PICK(htsubaki)

STD_ROM_FN(htsubaki)

static const RomLoad htsubakiLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 1, 0x400000, 1, 0x0, 0x200000 },
	{ 4, 1, 0x600000, 1, 0x0, 0x200000 },
	{ 5, 2, 0x0,      1, 0x0, 0x200000 },
};

static struct BurnRomInfo mjgnightRomDesc[] = {
	{ "00302.5b",  0x80000,  0x7169611a, 1 | BRF_PRG | BRF_ESS },

	{ "00303.7b",  0x200000, 0x5b2f28a5, 2 | BRF_GRA           },
	{ "00304.8b",  0x200000, 0x82624fb6, 2 | BRF_GRA           },
	{ "00305.10b", 0x200000, 0x4a5a6ac5, 2 | BRF_GRA           },
	{ "00306.12b", 0x200000, 0x143c4d24, 2 | BRF_GRA           },

	{ "00301.2a",  0x100000, 0xf5a0953a, 3 | BRF_SND           },
};

STD_ROM_PICK(mjgnight)

STD_ROM_FN(mjgnight)

static const RomLoad mjgnightLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 1, 0x400000, 1, 0x0, 0x200000 },
	{ 4, 1, 0x600000, 1, 0x0, 0x200000 },
	{ 5, 2, 0x0,      1, 0x0, 0x100000 },
	{ 5, 2, 0x100000, 1, 0x0, 0x100000 },
};

static struct BurnRomInfo hnroseRomDesc[] = {
	{ "00802a4.6b", 0x80000,  0xf08f7d21, 1 | BRF_PRG | BRF_ESS },

	{ "00803.7b",   0x200000, 0x93060501, 2 | BRF_GRA           },
	{ "00804.8b",   0x200000, 0x7deb12ab, 2 | BRF_GRA           },
	{ "00805.9b",   0x200000, 0x6f026843, 2 | BRF_GRA           },
	{ "00806.11b",  0x200000, 0xab006626, 2 | BRF_GRA           },

	{ "00801.2a",   0x200000, 0x3e9f7a5a, 3 | BRF_SND           },
};

STD_ROM_PICK(hnrose)

STD_ROM_FN(hnrose)

static const RomLoad hnroseLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 1, 0x400000, 1, 0x0, 0x200000 },
	{ 4, 1, 0x600000, 1, 0x0, 0x200000 },
	{ 5, 2, 0x0,      1, 0x0, 0x200000 },
};

static struct BurnRomInfo mjswacadRomDesc[] = {
	{ "01002_2.5b", 0x80000,  0xdfb1e304, 1 | BRF_PRG | BRF_ESS },

	{ "01003.7b",   0x200000, 0x1df7a355, 2 | BRF_GRA           },
	{ "01004.8b",   0x200000, 0x198904d4, 2 | BRF_GRA           },
	{ "01005.9b",   0x200000, 0x4dbcf7cf, 2 | BRF_GRA           },
	{ "01006.11b",  0x200000, 0xd407e19c, 2 | BRF_GRA           },

	{ "01001.2b",   0x200000, 0x4b84f45c, 3 | BRF_SND           },
};

STD_ROM_PICK(mjswacad)

STD_ROM_FN(mjswacad)

static const RomLoad mjswacadLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 1, 0x400000, 1, 0x0, 0x200000 },
	{ 4, 1, 0x600000, 1, 0x0, 0x200000 },
	{ 5, 2, 0x0,      1, 0x0, 0x200000 },
};

static struct BurnRomInfo jongohRomDesc[] = {
	{ "t80324.4c",  0x80000,  0x31c5864b, 1 | BRF_PRG | BRF_ESS },

	{ "t80334.11c", 0x200000, 0x530aeea8, 2 | BRF_GRA           },
	{ "t8034.13c",  0x100000, 0x260fb823, 2 | BRF_GRA           },

	{ "t8031.1c",   0x80000,  0x5a8cd45c, 3 | BRF_SND           },
};

STD_ROM_PICK(jongoh)

STD_ROM_FN(jongoh)

static const RomLoad jongohLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x400000, 1, 0x0, 0x100000 },
	{ 3, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo daireachRomDesc[] = {
	{ "01202.5c",  0x80000,  0x2d1cc94d, 1 | BRF_PRG | BRF_ESS },

	{ "01203.11c", 0x200000, 0xda054dbf, 2 | BRF_GRA           },
	{ "01204.11a", 0x200000, 0xac96b1fc, 2 | BRF_GRA           },

	{ "01201.1c",  0x80000,  0x9a638bc8, 3 | BRF_SND           },
};

STD_ROM_PICK(daireach)

STD_ROM_FN(daireach)

static const RomLoad daireachLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x400000, 1, 0x0, 0x200000 },
	{ 3, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo daichukaRomDesc[] = {
	// MAME 0.289: BAD_DUMP; offsets 0x92 and 0xa2 in every 0x100-byte block are 0xff.
	{ "3062a5.4c",   0x80000,  0x40fbf627, 1 | BRF_PRG | BRF_ESS },

	{ "t83063.11c",  0x200000, 0x2c0a7cf6, 2 | BRF_GRA           },
	{ "83604_2.sub", 0x200000, 0x0faeaf86, 2 | BRF_GRA           },

	{ "t83061.1c",   0x80000,  0x52666107, 3 | BRF_SND           },
};

STD_ROM_PICK(daichuka)

STD_ROM_FN(daichuka)

static const RomLoad daichukaLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x400000, 1, 0x0, 0x200000 },
	{ 3, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo daichukaaRomDesc[] = {
	{ "p3042a4.4c",  0x80000,  0x04155ff0, 1 | BRF_PRG | BRF_ESS },

	{ "83043_2.sub", 0x200000, 0x7d1cdaf4, 2 | BRF_GRA           },

	{ "t83041.1c",   0x80000,  0xc92065a9, 3 | BRF_SND           },
};

STD_ROM_PICK(daichukaa)

STD_ROM_FN(daichukaa)

static const RomLoad daichukaaLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 0, 0, 0x10000,  1, 0x0, 0x80000  },
	{ 1, 1, 0x400000, 1, 0x0, 0x200000 },
	{ 2, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo daimyojnRomDesc[] = {
	{ "p0172.6b", 0x80000,  0x478442bd, 1 | BRF_PRG | BRF_ESS },

	{ "t0173.7b", 0x200000, 0xb54c7b02, 2 | BRF_GRA           },
	{ "p0174.8b", 0x200000, 0x861de43f, 2 | BRF_GRA           },

	{ "t0171.2b", 0x80000,  0x464be04c, 3 | BRF_SND           },
};

STD_ROM_PICK(daimyojn)

STD_ROM_FN(daimyojn)

static const RomLoad daimyojnLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo mjswacadsRomDesc[] = {
	{ "p0102y5.5b",  0x80000,  0x8ae679ad, 1 | BRF_PRG | BRF_ESS },

	{ "01003.7b",    0x200000, 0x1df7a355, 2 | BRF_GRA           },
	{ "01004.8b",    0x200000, 0x198904d4, 2 | BRF_GRA           },
	{ "01005.9b",    0x200000, 0x4dbcf7cf, 2 | BRF_GRA           },
	{ "01006_2.11b", 0x200000, 0x7c646f5c, 2 | BRF_GRA           },

	{ "01001.2b",    0x100000, 0xee30d20a, 3 | BRF_SND           },
};

STD_ROM_PICK(mjswacads)

STD_ROM_FN(mjswacads)

static const RomLoad mjswacadsLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 1, 0x400000, 1, 0x0, 0x200000 },
	{ 4, 1, 0x600000, 1, 0x0, 0x200000 },
	{ 5, 2, 0x0,      1, 0x0, 0x100000 },
};

static struct BurnRomInfo mjtenhoRomDesc[] = {
	{ "p016021.6b", 0x80000,  0x8acd16f1, 1 | BRF_PRG | BRF_ESS },

	{ "t01063.7b",  0x200000, 0xead305a8, 2 | BRF_GRA           },
	{ "t01064.8b",  0x200000, 0x9d3240d8, 2 | BRF_GRA           },

	{ "t01601.2b",  0x80000,  0xe5678713, 3 | BRF_SND           },
};

STD_ROM_PICK(mjtenho)

STD_ROM_FN(mjtenho)

static const RomLoad mjtenhoLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo momotaroRomDesc[] = {
	{ "r0272m1.6e", 0x80000,  0x71c83332, 1 | BRF_PRG | BRF_ESS },
	// MAME 0.289: both graphics ROMs are BAD_DUMP (fixed bits xxxxx1xxxxxxxxx1).
	{ "t0273.7b",   0x200000, 0x5ae90ae2, 2 | BRF_GRA           },
	{ "t0274.8b",   0x200000, 0x78209778, 2 | BRF_GRA           },

	{ "t0271.2b",   0x80000,  0xc850d7b2, 3 | BRF_SND           },
};

STD_ROM_PICK(momotaro)

STD_ROM_FN(momotaro)

static const RomLoad momotaroLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo mjnigiriRomDesc[] = {
	{ "p0382-2.6b", 0x80000,  0x6474a421, 1 | BRF_PRG | BRF_ESS },

	{ "t0383.7b",   0x200000, 0xe948ec2e, 2 | BRF_GRA           },
	{ "t0384.8b",   0x200000, 0xcbfa4713, 2 | BRF_GRA           },

	{ "t0381.2b",   0x80000,  0xd7705dc6, 3 | BRF_SND           },
};

STD_ROM_PICK(mjnigiri)

STD_ROM_FN(mjnigiri)

static const RomLoad mjnigiriLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 2, 0x0,      1, 0x0, 0x80000  },
};

static struct BurnRomInfo mjkokryuRomDesc[] = {
	{ "r0402-3.6e",  0x80000,  0x775f1dcf, 1 | BRF_PRG | BRF_ESS },

	{ "t0403.7b",    0x200000, 0x43f531db, 2 | BRF_GRA           },
	{ "t0404.8b",    0x200000, 0x340e36d3, 2 | BRF_GRA           },
	{ "t0405.9b",    0x200000, 0x20abe74f, 2 | BRF_GRA           },
	{ "t0406-1.11b", 0x200000, 0xc06b00a4, 2 | BRF_GRA           },

	{ "t0401.2b",    0x100000, 0x320953cb, 3 | BRF_SND           },

	{ "nvram",       0x8000,   0xc2590a33, 4 | BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(mjkokryu)

STD_ROM_FN(mjkokryu)

static const RomLoad mjkokryuLoads[] = {
	{ 0, 0, 0x0,      1, 0x0, 0x80000  },
	{ 1, 1, 0x0,      1, 0x0, 0x200000 },
	{ 2, 1, 0x200000, 1, 0x0, 0x200000 },
	{ 3, 1, 0x400000, 1, 0x0, 0x200000 },
	{ 4, 1, 0x600000, 1, 0x0, 0x200000 },
	{ 5, 2, 0x0,      1, 0x0, 0x100000 },
};

static INT32 htengokuInit()
{
	return DrvInit(BOARD_HTENGOKU, 0x40000, 0x300000, 0x40000, htengokuLoads, 10);
}

struct BurnDriver BurnDrvHtengoku = {
	"htengoku", NULL, NULL, NULL, "1992",
	"Hanafuda Hana Tengoku (Japan)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, htengokuRomInfo, htengokuRomName, NULL, NULL, NULL, NULL, htengokuInputInfo, htengokuDIPInfo,
	htengokuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mmpanicInit()
{
	return DrvInit(BOARD_MMPANIC, 0x50000, 0x280000, 0x40000, mmpanicLoads, 7);
}

struct BurnDriver BurnDrvMmpanic = {
	"mmpanic", NULL, NULL, NULL, "1992",
	"Monkey Mole Panic (USA)\0", NULL, "Nakanihon / East Technology (Taito license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mmpanicRomInfo, mmpanicRomName, NULL, NULL, NULL, NULL, mmpanicInputInfo, mmpanicDIPInfo,
	mmpanicInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mjmyorn2Init()
{
	return DrvInit(BOARD_MJMYORNT, 0x98000, 0x1c0000, 0x40000, mjmyorn2Loads, 8);
}

struct BurnDriver BurnDrvMjmyorn2 = {
	"mjmyorn2", NULL, NULL, NULL, "1993",
	"Mahjong The Mysterious Orient Part 2 ~ Exotic Dream ~ (Japan, v1.00)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjmyorn2RomInfo, mjmyorn2RomName, NULL, NULL, NULL, NULL, mjmyorn2InputInfo, mjmyorn2DIPInfo,
	mjmyorn2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mjmyorntInit()
{
	return DrvInit(BOARD_MJMYORNT, 0x98000, 0x1a0000, 0x40000, mjmyorntLoads, 6);
}

struct BurnDriver BurnDrvMjmyornt = {
	"mjmyornt", "mjmyorn2", NULL, NULL, "1992",
	"Mahjong The Mysterious Orient (Japan, v1.00)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjmyorntRomInfo, mjmyorntRomName, NULL, NULL, NULL, NULL, mjmyorntInputInfo, mjmyorntDIPInfo,
	mjmyorntInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 funkyfigInit()
{
	return DrvInit(BOARD_FUNKYFIG, 0x98000, 0x500000, 0x40000, funkyfigLoads, 12);
}

struct BurnDriver BurnDrvFunkyfig = {
	"funkyfig", NULL, NULL, NULL, "1993",
	"The First Funky Fighter (USA, Canada, Mexico / Japan, set 1)\0", NULL, "Nakanihon / East Technology (Taito license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, funkyfigRomInfo, funkyfigRomName, NULL, NULL, NULL, NULL, funkyfigInputInfo, funkyfigDIPInfo,
	funkyfigInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 funkyfigaInit()
{
	return DrvInit(BOARD_FUNKYFIG, 0x98000, 0x500000, 0x40000, funkyfigaLoads, 9);
}

struct BurnDriver BurnDrvFunkyfiga = {
	"funkyfiga", "funkyfig", NULL, NULL, "1993",
	"The First Funky Fighter (USA, Canada, Mexico / Japan, set 2)\0", NULL, "Nakanihon / East Technology (Taito license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, funkyfigaRomInfo, funkyfigaRomName, NULL, NULL, NULL, NULL, funkyfigInputInfo, funkyfigDIPInfo,
	funkyfigaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 quizchqInit()
{
	return DrvInit(BOARD_QUIZCHQ, 0x118000, 0x320000, 0x80000, quizchqLoads, 11);
}

struct BurnDriver BurnDrvQuizchq = {
	"quizchq", NULL, NULL, NULL, "1993",
	"Quiz Channel Question (Japan, Ver 1.00)\0", NULL, "Nakanihon", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, quizchqRomInfo, quizchqRomName, NULL, NULL, NULL, NULL, quizchqInputInfo, quizchqDIPInfo,
	quizchqInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 quizchqlInit()
{
	return DrvInit(BOARD_QUIZCHQ, 0x118000, 0x420000, 0x80000, quizchqlLoads, 13);
}

struct BurnDriver BurnDrvQuizchql = {
	"quizchql", "quizchq", NULL, NULL, "1993",
	"Quiz Channel Question (Taiwan?, Ver 1.23)\0", NULL, "Nakanihon (Laxan license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, quizchqlRomInfo, quizchqlRomName, NULL, NULL, NULL, NULL, quizchqInputInfo, quizchqDIPInfo,
	quizchqlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 quizchqkInit()
{
	return DrvInit(BOARD_QUIZCHQ, 0x118000, 0x300000, 0x80000, quizchqkLoads, 7);
}

struct BurnDriver BurnDrvQuizchqk = {
	"quizchqk", "quizchq", NULL, NULL, "1993",
	"Quiz Channel Question (Korea, Ver 1.10)\0", NULL, "KM Korea Co, Ltd (official license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, quizchqkRomInfo, quizchqkRomName, NULL, NULL, NULL, NULL, quizchqInputInfo, quizchqDIPInfo,
	quizchqkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 animaljrInit()
{
	return DrvInit(BOARD_MMPANIC, 0x50000, 0x200000, 0x40000, animaljrLoads, 7);
}

struct BurnDriver BurnDrvAnimaljr = {
	"animaljr", NULL, NULL, NULL, "1993",
	"Exciting Animal Land Jr. (USA, Canada, Mexico)\0", NULL, "Nakanihon / East Technology (Taito license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, animaljrRomInfo, animaljrRomName, NULL, NULL, NULL, NULL, animaljrInputInfo, animaljrDIPInfo,
	animaljrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 animaljrsInit()
{
	return DrvInit(BOARD_MMPANIC, 0x50000, 0x200000, 0x40000, animaljrsLoads, 7);
}

struct BurnDriver BurnDrvAnimaljrs = {
	"animaljrs", "animaljr", NULL, NULL, "1993",
	"Animalandia Jr. (Spanish)\0", NULL, "Nakanihon / East Technology (Taito license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, animaljrsRomInfo, animaljrsRomName, NULL, NULL, NULL, NULL, animaljrInputInfo, animaljrDIPInfo,
	animaljrsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 animaljrjInit()
{
	return DrvInit(BOARD_MMPANIC, 0x50000, 0x200000, 0x40000, animaljrjLoads, 6);
}

struct BurnDriver BurnDrvAnimaljrj = {
	"animaljrj", "animaljr", NULL, NULL, "1993",
	"Waiwai Animal Land Jr. (Japan)\0", NULL, "Nakanihon / East Technology (Taito license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, animaljrjRomInfo, animaljrjRomName, NULL, NULL, NULL, NULL, animaljrInputInfo, animaljrDIPInfo,
	animaljrjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mjmysterInit()
{
	return DrvInit(BOARD_MJMYSTER, 0x98000, 0x1a0000, 0x40000, mjmysterLoads, 7);
}

struct BurnDriver BurnDrvMjmyster = {
	"mjmyster", NULL, NULL, NULL, "1994",
	"Mahjong The Mysterious World (Japan, set 1)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjmysterRomInfo, mjmysterRomName, NULL, NULL, NULL, NULL, mjmysterInputInfo, mjmysterDIPInfo,
	mjmysterInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mjmywrldInit()
{
	return DrvInit(BOARD_MJMYWRLD, 0x98000, 0x1a0000, 0x40000, mjmywrldLoads, 6);
}

struct BurnDriver BurnDrvMjmywrld = {
	"mjmywrld", "mjmyster", NULL, NULL, "1994",
	"Mahjong The Mysterious World (Japan, set 2)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjmywrldRomInfo, mjmywrldRomName, NULL, NULL, NULL, NULL, mjmysterInputInfo, mjmysterDIPInfo,
	mjmywrldInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 hgingaInit()
{
	return DrvInit(BOARD_HGINGA, 0x98000, 0x180000, 0x40000, hgingaLoads, 8);
}

struct BurnDriver BurnDrvHginga = {
	"hginga", NULL, NULL, NULL, "1994",
	"Hanafuda Hana Ginga (Japan, ver 1.00, rev 1)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hgingaRomInfo, hgingaRomName, NULL, NULL, NULL, NULL, hgingaInputInfo, hgingaDIPInfo,
	hgingaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 hgingaaInit()
{
	return DrvInit(BOARD_HGINGA, 0x98000, 0x180000, 0x40000, hgingaaLoads, 8);
}

struct BurnDriver BurnDrvHgingaa = {
	"hgingaa", "hginga", NULL, NULL, "1994",
	"Hanafuda Hana Ginga (Japan, ver 1.00)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hgingaaRomInfo, hgingaaRomName, NULL, NULL, NULL, NULL, hgingaInputInfo, hgingaDIPInfo,
	hgingaaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mjmyunivInit()
{
	return DrvInit(BOARD_MJMYUNIV, 0x98000, 0x600000, 0x40000, mjmyunivLoads, 7);
}

struct BurnDriver BurnDrvMjmyuniv = {
	"mjmyuniv", NULL, NULL, NULL, "1994",
	"Mahjong The Mysterious Universe (Japan, D85)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjmyunivRomInfo, mjmyunivRomName, NULL, NULL, NULL, NULL, mjmysterInputInfo, mjmysterDIPInfo,
	mjmyunivInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mjmyunivhInit()
{
	return DrvInit(BOARD_MJMYUNIV, 0x98000, 0xc00000, 0x40000, mjmyunivhLoads, 7);
}

struct BurnDriver BurnDrvMjmyunivh = {
	"mjmyunivh", "mjmyuniv", NULL, NULL, "1995",
	"Maa Zoek Lung Hing Fu Dai (Hong Kong, D106T)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjmyunivhRomInfo, mjmyunivhRomName, NULL, NULL, NULL, NULL, mjmyunivhInputInfo, mjmyunivhDIPInfo,
	mjmyunivhInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mjmywrldtInit()
{
	return DrvInit(BOARD_MJMYWRLDT, 0x98000, 0x400000, 0x40000, mjmywrldtLoads, 7);
}

struct BurnDriver BurnDrvMjmywrldt = {
	"mjmywrldt", "mjmyster", NULL, NULL, "1996",
	"Maque Caihui Ying (Taiwan, ver 2.20)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjmywrldtRomInfo, mjmywrldtRomName, NULL, NULL, NULL, NULL, mjmyunivhInputInfo, mjmyunivhDIPInfo,
	mjmywrldtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 quiz365Init()
{
	return DrvInit(BOARD_QUIZ365, 0x180000, 0x380000, 0x100000, quiz365Loads, 11);
}

struct BurnDriver BurnDrvQuiz365 = {
	"quiz365", NULL, NULL, NULL, "1994",
	"Quiz 365 (Japan)\0", NULL, "Nakanihon", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, quiz365RomInfo, quiz365RomName, NULL, NULL, NULL, NULL, quiz365InputInfo, quiz365DIPInfo,
	quiz365Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 quiz365tInit()
{
	return DrvInit(BOARD_QUIZ365, 0x180000, 0x400000, 0x100000, quiz365tLoads, 11);
}

struct BurnDriver BurnDrvQuiz365t = {
	"quiz365t", "quiz365", NULL, NULL, "1994",
	"Quiz 365 (Hong Kong & Taiwan)\0", NULL, "Nakanihon / Taito", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, quiz365tRomInfo, quiz365tRomName, NULL, NULL, NULL, NULL, quiz365InputInfo, quiz365DIPInfo,
	quiz365tInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 rongrongInit()
{
	return DrvInit(BOARD_RONGRONG, 0x118000, 0x280000, 0x40000, rongrongLoads, 8);
}

struct BurnDriver BurnDrvRongrong = {
	"rongrong", NULL, NULL, NULL, "1994",
	"Puzzle Game Rong Rong (Europe)\0", NULL, "Nakanihon (Activision license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rongrongRomInfo, rongrongRomName, NULL, NULL, NULL, NULL, rongrongInputInfo, rongrongDIPInfo,
	rongrongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 rongrongjInit()
{
	return DrvInit(BOARD_RONGRONG, 0x118000, 0x240000, 0x40000, rongrongjLoads, 8);
}

struct BurnDriver BurnDrvRongrongj = {
	"rongrongj", "rongrong", NULL, NULL, "1994",
	"Puzzle Game Rong Rong (Japan)\0", NULL, "Nakanihon (Activision license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rongrongjRomInfo, rongrongjRomName, NULL, NULL, NULL, NULL, rongrongInputInfo, rongrongDIPInfo,
	rongrongjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 rongronggInit()
{
	return DrvInit(BOARD_RONGRONG, 0x118000, 0x280000, 0x40000, rongronggLoads, 8);
}

struct BurnDriver BurnDrvRongrongg = {
	"rongrongg", "rongrong", NULL, NULL, "1994",
	"Puzzle Game Rong Rong (Germany)\0", NULL, "Nakanihon (Activision license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rongronggRomInfo, rongronggRomName, NULL, NULL, NULL, NULL, rongrongInputInfo, rongrongDIPInfo,
	rongronggInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 hparadisInit()
{
	return DrvInit(BOARD_HPARADIS, 0x58000, 0x600000, 0x40000, hparadisLoads, 7);
}

struct BurnDriver BurnDrvHparadis = {
	"hparadis", NULL, NULL, NULL, "1994",
	"Super Hana Paradise (Japan)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hparadisRomInfo, hparadisRomName, NULL, NULL, NULL, NULL, hparadisInputInfo, hparadisDIPInfo,
	hparadisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 rselectInit()
{
	return DrvInit(BOARD_RSELECT, 0x98000, 0x200000, 0x40000, rselectLoads, 5);
}

struct BurnDriver BurnDrvRselect = {
	"rselect", NULL, NULL, NULL, "1994",
	"Royal Selection (Japan, ver. 1.01)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rselectRomInfo, rselectRomName, NULL, NULL, NULL, NULL, rselectInputInfo, rselectDIPInfo,
	rselectInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 hgokouInit()
{
	return DrvInit(BOARD_HGOKOU, 0x98000, 0x200000, 0x40000, hgokouLoads, 5);
}

struct BurnDriver BurnDrvHgokou = {
	"hgokou", NULL, NULL, NULL, "1995",
	"Hanafuda Hana Gokou (Japan, ver. B)\0", NULL, "Dynax (Alba license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hgokouRomInfo, hgokouRomName, NULL, NULL, NULL, NULL, hgokouInputInfo, hgokouDIPInfo,
	hgokouInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 hgokouaInit()
{
	return DrvInit(BOARD_HGOKBANG, 0x98000, 0x200000, 0x40000, hgokouaLoads, 5);
}

struct BurnDriver BurnDrvHgokoua = {
	"hgokoua", "hgokou", NULL, NULL, "1995",
	"Hanafuda Hana Gokou (Japan, ver. A)\0", NULL, "Dynax (Alba license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hgokouaRomInfo, hgokouaRomName, NULL, NULL, NULL, NULL, hgokouInputInfo, hgokouDIPInfo,
	hgokouaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 hgokbangInit()
{
	return DrvInit(BOARD_HGOKBANG, 0x98000, 0x500000, 0x40000, hgokbangLoads, 5);
}

struct BurnDriver BurnDrvHgokbang = {
	"hgokbang", "hgokou", NULL, NULL, "1995",
	"Hanafuda Hana Gokou Bangaihen (Japan)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hgokbangRomInfo, hgokbangRomName, NULL, NULL, NULL, NULL, hgokouInputInfo, hgokouDIPInfo,
	hgokbangInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mjdchukaInit()
{
	return DrvInit(BOARD_MJCHUUKA, 0x80000, 0x100000, 0x80000, mjdchukaLoads, 4);
}

struct BurnDriver BurnDrvMjdchuka = {
	"mjdchuka", NULL, NULL, NULL, "1995",
	"Maque Da Zhonghua Quan (Taiwan, D111)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjdchukaRomInfo, mjdchukaRomName, NULL, NULL, NULL, NULL, mjchuukaInputInfo, mjchuukaDIPInfo,
	mjdchukaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 mjschukaInit()
{
	return DrvInit(BOARD_MJSCHUKA, 0xa0000, 0x100000, 0x200000, mjschukaLoads, 12);
}

struct BurnDriver BurnDrvMjschuka = {
	"mjschuka", NULL, NULL, NULL, "1995",
	"Mahjong Super Dai Chuuka Ken (Japan, D115)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjschukaRomInfo, mjschukaRomName, NULL, NULL, NULL, NULL, mjschukaInputInfo, mjschukaDIPInfo,
	mjschukaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 nettoqcInit()
{
	return DrvInit(BOARD_NETTOQC, 0x180000, 0x400000, 0x100000, nettoqcLoads, 12);
}

struct BurnDriver BurnDrvNettoqc = {
	"nettoqc", NULL, NULL, NULL, "1995",
	"Nettoh Quiz Champion (Japan)\0", NULL, "Nakanihon", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, nettoqcRomInfo, nettoqcRomName, NULL, NULL, NULL, NULL, nettoqcInputInfo, nettoqcDIPInfo,
	nettoqcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 ultrchmpInit()
{
	return DrvInit(BOARD_ULTRCHMP, 0x180000, 0x440000, 0x400000, ultrchmpLoads, 10);
}

struct BurnDriver BurnDrvUltrchmp = {
	"ultrchmp", "nettoqc", NULL, NULL, "1995",
	"Se Gye Hweng Dan Ultra Champion (Korea)\0", NULL, "Nakanihon", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, ultrchmpRomInfo, ultrchmpRomName, NULL, NULL, NULL, NULL, ultrchmpInputInfo, ultrchmpDIPInfo,
	ultrchmpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 ultrchmphInit()
{
	return DrvInit(BOARD_ULTRCHMP, 0x180000, 0x440000, 0x400000, ultrchmphLoads, 10);
}

struct BurnDriver BurnDrvUltrchmph = {
	"ultrchmph", "nettoqc", NULL, NULL, "1995",
	"Cheng Ba Shi Jie - Chao Shi Kong Guan Jun (Taiwan)\0", NULL, "Nakanihon", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, ultrchmphRomInfo, ultrchmphRomName, NULL, NULL, NULL, NULL, ultrchmpInputInfo, ultrchmpDIPInfo,
	ultrchmphInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 ddenlovjInit()
{
	return DrvInit(BOARD_DDENLOVJ, 0x80000, 0xe80000, 0x200000, ddenlovjLoads, 9);
}

struct BurnDriver BurnDrvDdenlovj = {
	"ddenlovj", NULL, NULL, NULL, "1995",
	"Don Den Lover Vol. 1 - Shiro Kuro Tsukeyo! (Japan)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, ddenlovjRomInfo, ddenlovjRomName, NULL, NULL, NULL, NULL, ddenlovjInputInfo, ddenlovjDIPInfo,
	ddenlovjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 ddenlovrkInit()
{
	return DrvInit(BOARD_DDENLOVRK, 0x80000, 0x280000, 0x200000, ddenlovrkLoads, 11);
}

struct BurnDriver BurnDrvDdenlovrk = {
	"ddenlovrk", "ddenlovj", NULL, NULL, "1995",
	"Don Den Lover Vol. 1 - Heukbaeg-euro Jeonghaja (Korea)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, ddenlovrkRomInfo, ddenlovrkRomName, NULL, NULL, NULL, NULL, ddenlovrInputInfo, ddenlovrDIPInfo,
	ddenlovrkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 ddenlovrbInit()
{
	return DrvInit(BOARD_DDENLOVR, 0x80000, 0x280000, 0x200000, ddenlovrbLoads, 8);
}

struct BurnDriver BurnDrvDdenlovrb = {
	"ddenlovrb", "ddenlovj", NULL, NULL, "1995",
	"Don Den Lover Vol. 1 - Heukbaeg-euro Jeonghaja (Korea, bootleg)\0", NULL, "bootleg", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, ddenlovrbRomInfo, ddenlovrbRomName, NULL, NULL, NULL, NULL, ddenlovrInputInfo, ddenlovrDIPInfo,
	ddenlovrbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 ddenlovrInit()
{
	return DrvInit(BOARD_DDENLOVR, 0x80000, 0x480000, 0x200000, ddenlovrLoads, 9);
}

struct BurnDriver BurnDrvDdenlovr = {
	"ddenlovr", "ddenlovj", NULL, NULL, "1996",
	"Don Den Lover Vol. 1 (Hong Kong)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, ddenlovrRomInfo, ddenlovrRomName, NULL, NULL, NULL, NULL, ddenlovrInputInfo, ddenlovrDIPInfo,
	ddenlovrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mjxyslInit()
{
	return DrvInit(BOARD_MJCHUUKA, 0x80000, 0x200000, 0x80000, mjxyslLoads, 6);
}

struct BurnDriver BurnDrvMjxysl = {
	"mjxysl", NULL, NULL, NULL, "1996",
	"Majiang Xingyun Shenlong (China, D121)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjxyslRomInfo, mjxyslRomName, NULL, NULL, NULL, NULL, mjxyslInputInfo, mjxyslDIPInfo,
	mjxyslInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 hanakanzInit()
{
	return DrvInit(BOARD_HANAKANZ, 0x80000, 0x300000, 0x80000, hanakanzLoads, 8);
}

struct BurnDriver BurnDrvHanakanz = {
	"hanakanz", NULL, NULL, NULL, "1996",
	"Hana Kanzashi (Japan)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hanakanzRomInfo, hanakanzRomName, NULL, NULL, NULL, NULL, hanakanzInputInfo, hanakanzDIPInfo,
	hanakanzInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 kotbinyoInit()
{
	return DrvInit(BOARD_KOTBINYO, 0x80000, 0x280000, 0x80000, kotbinyoLoads, 9);
}

struct BurnDriver BurnDrvKotbinyo = {
	"kotbinyo", "hanakanz", NULL, NULL, "1997",
	"Kkotbinyeo (Korea)\0", NULL, "Dynax / Shinwhajin", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, kotbinyoRomInfo, kotbinyoRomName, NULL, NULL, NULL, NULL, kotbinyoInputInfo, kotbinyoDIPInfo,
	kotbinyoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	335, 240, 4, 3
};

static INT32 kotbinyosuInit()
{
	return DrvInit(BOARD_KOTBINYO, 0x80000, 0x280000, 0x80000, kotbinyosuLoads, 9);
}

struct BurnDriver BurnDrvKotbinyosu = {
	"kotbinyosu", "hanakanz", NULL, NULL, "1997",
	"Speed Up / Kkotbinyeo (Korea)\0", NULL, "Dynax / Shinwhajin", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, kotbinyosuRomInfo, kotbinyosuRomName, NULL, NULL, NULL, NULL, kotbinyosuInputInfo, kotbinyosuDIPInfo,
	kotbinyosuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	335, 240, 4, 3
};

static INT32 kotbinspInit()
{
	return DrvInit(BOARD_KOTBINSP, 0x80000, 0x2000000, 0x80000, kotbinspLoads, 6);
}

struct BurnDriver BurnDrvKotbinsp = {
	"kotbinsp", NULL, NULL, NULL, "1997",
	"Kkotbinyeo Special (Korea)\0", NULL, "Dynax / Shinwhajin", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, kotbinspRomInfo, kotbinspRomName, NULL, NULL, NULL, NULL, kotbinspInputInfo, kotbinspDIPInfo,
	kotbinspInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	335, 240, 4, 3
};

static INT32 akamaruInit()
{
	return DrvInit(BOARD_AKAMARU, 0x200000, 0x480000, 0x300000, akamaruLoads, 14);
}

struct BurnDriver BurnDrvAkamaru = {
	"akamaru", NULL, NULL, NULL, "1996",
	"Panel & Variety Akamaru Q Joushou Dont-R\0", NULL, "Dynax (Nakanihon license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, akamaruRomInfo, akamaruRomName, NULL, NULL, NULL, NULL, akamaruInputInfo, akamaruDIPInfo,
	akamaruInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 janshinpInit()
{
	return DrvInit(BOARD_JANSHINP, 0xa0000, 0x180000, 0x80000, janshinpLoads, 6);
}

struct BurnDriver BurnDrvJanshinp = {
	"janshinp", NULL, NULL, NULL, "1996",
	"Mahjong Janshin Plus (Japan)\0", NULL, "Dynax / Sigma", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, janshinpRomInfo, janshinpRomName, NULL, NULL, NULL, NULL, janshinpInputInfo, janshinpDIPInfo,
	janshinpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 239, 4, 3
};

static INT32 dtoyokenInit()
{
	return DrvInit(BOARD_DTOYOKEN, 0xa0000, 0x200000, 0x80000, dtoyokenLoads, 7);
}

struct BurnDriver BurnDrvDtoyoken = {
	"dtoyoken", NULL, NULL, NULL, "1996",
	"Mahjong Dai Touyouken (Japan)\0", NULL, "Dynax / Sigma", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, dtoyokenRomInfo, dtoyokenRomName, NULL, NULL, NULL, NULL, dtoyokenInputInfo, dtoyokenDIPInfo,
	dtoyokenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 239, 4, 3
};

static INT32 sryudensInit()
{
	return DrvInit(BOARD_SRYUDENS, 0xa0000, 0x480000, 0x80000, sryudensLoads, 12);
}

struct BurnDriver BurnDrvSryudens = {
	"sryudens", NULL, NULL, NULL, "1996",
	"Mahjong Seiryu Densetsu (Japan, NM502)\0", NULL, "Dynax / Face", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, sryudensRomInfo, sryudensRomName, NULL, NULL, NULL, NULL, sryudensInputInfo, sryudensDIPInfo,
	sryudensInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 239, 4, 3
};

static INT32 seljan2Init()
{
	return DrvInit(BOARD_SELJAN2, 0xa8000, 0x500000, 0x80000, seljan2Loads, 5);
}

struct BurnDriver BurnDrvSeljan2 = {
	"seljan2", NULL, NULL, NULL, "1996",
	"Return of Sel Jan II (Japan, NM557)\0", NULL, "Dynax / Face", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, seljan2RomInfo, seljan2RomName, NULL, NULL, NULL, NULL, seljan2InputInfo, seljan2DIPInfo,
	seljan2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 239, 4, 3
};

static INT32 seljan2aInit()
{
	return DrvInit(BOARD_SRYUDENS, 0xa0000, 0x4c0000, 0x80000, seljan2aLoads, 10);
}

struct BurnDriver BurnDrvSeljan2a = {
	"seljan2a", "seljan2", NULL, NULL, "1996",
	"Return of Sel Jan II (Japan, NM508)\0", NULL, "Dynax / Face", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, seljan2aRomInfo, seljan2aRomName, NULL, NULL, NULL, NULL, seljan2InputInfo, seljan2DIPInfo,
	seljan2aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 239, 4, 3
};

static INT32 mjfloveInit()
{
	return DrvInit(BOARD_MJFLOVE, 0xa0000, 0x3c0000, 0x200000, mjfloveLoads, 15);
}

struct BurnDriver BurnDrvMjflove = {
	"mjflove", NULL, NULL, NULL, "1996",
	"Mahjong Fantasic Love (Japan)\0", NULL, "Nakanihon", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjfloveRomInfo, mjfloveRomName, NULL, NULL, NULL, NULL, mjfloveInputInfo, mjfloveDIPInfo,
	mjfloveInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mjmyorntrInit()
{
	return DrvInit(BOARD_MJMYORNTR, 0x58000, 0x1c0000, 0x40000, mjmyorntrLoads, 7);
}

struct BurnDriver BurnDrvMjmyorntr = {
	"mjmyorntr", NULL, NULL, NULL, "1997",
	"Mahjong The Mysterious Orient Returns (Japan, v1.00)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjmyorntrRomInfo, mjmyorntrRomName, NULL, NULL, NULL, NULL, mjschukaInputInfo, mjschukaDIPInfo,
	mjmyorntrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 mjmysterrInit()
{
	return DrvInit(BOARD_MJMYORNTR, 0x58000, 0x5a0000, 0x40000, mjmysterrLoads, 7);
}

struct BurnDriver BurnDrvMjmysterr = {
	"mjmysterr", NULL, NULL, NULL, "1997",
	"Mahjong The Mysterious World Returns (Japan)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjmysterrRomInfo, mjmysterrRomName, NULL, NULL, NULL, NULL, mjschukaInputInfo, mjschukaDIPInfo,
	mjmysterrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};

static INT32 hkagerouInit()
{
	return DrvInit(BOARD_HKAGEROU, 0x80000, 0xe80000, 0x80000, hkagerouLoads, 8);
}

struct BurnDriver BurnDrvHkagerou = {
	"hkagerou", NULL, NULL, NULL, "1997",
	"Hana Kagerou (Japan)\0", NULL, "Nakanihon / Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hkagerouRomInfo, hkagerouRomName, NULL, NULL, NULL, NULL, hkagerouInputInfo, hkagerouDIPInfo,
	hkagerouInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 mjchuukaInit()
{
	return DrvInit(BOARD_MJCHUUKA, 0x80000, 0x300000, 0x80000, mjchuukaLoads, 8);
}

struct BurnDriver BurnDrvMjchuuka = {
	"mjchuuka", NULL, NULL, NULL, "1998",
	"Maque Zhonghua Ernu (Taiwan)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjchuukaRomInfo, mjchuukaRomName, NULL, NULL, NULL, NULL, mjchuukaInputInfo, mjchuukaDIPInfo,
	mjchuukaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 mjreach1Init()
{
	return DrvInit(BOARD_MJREACH1, 0x80000, 0x500000, 0x80000, mjreach1Loads, 8);
}

struct BurnDriver BurnDrvMjreach1 = {
	"mjreach1", NULL, NULL, NULL, "1998",
	"Mahjong Reach Ippatsu (Japan, NM526-NSI)\0", NULL, "Nihon System", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjreach1RomInfo, mjreach1RomName, NULL, NULL, NULL, NULL, mjreach1InputInfo, mjreach1DIPInfo,
	mjreach1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 mjreach1aInit()
{
	return DrvInit(BOARD_MJREACH1, 0x80000, 0x500000, 0x80000, mjreach1aLoads, 8);
}

struct BurnDriver BurnDrvMjreach1a = {
	"mjreach1a", "mjreach1", NULL, NULL, "1998",
	"Mahjong Reach Ippatsu (Japan, NM526-01)\0", NULL, "Nihon System", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjreach1aRomInfo, mjreach1aRomName, NULL, NULL, NULL, NULL, mjreach1InputInfo, mjreach1DIPInfo,
	mjreach1aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 jongteiInit()
{
	return DrvInit(BOARD_JONGTEI, 0x80000, 0x800000, 0x200000, jongteiLoads, 6);
}

struct BurnDriver BurnDrvJongtei = {
	"jongtei", NULL, NULL, NULL, "1999",
	"Mahjong Jong-Tei (Japan, NM532-01)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, jongteiRomInfo, jongteiRomName, NULL, NULL, NULL, NULL, jongteiInputInfo, jongteiDIPInfo,
	jongteiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 jongteiaInit()
{
	return DrvInit(BOARD_JONGTEIA, 0x80000, 0x800000, 0x200000, jongteiaLoads, 6);
}

struct BurnDriver BurnDrvJongteia = {
	"jongteia", "jongtei", NULL, NULL, "2000",
	"Mahjong Jong-Tei (Japan, Techno-Top license)\0", NULL, "Dynax (Techno-Top license)", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, jongteiaRomInfo, jongteiaRomName, NULL, NULL, NULL, NULL, jongteiInputInfo, jongteiDIPInfo,
	jongteiaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 htsubakiInit()
{
	return DrvInit(BOARD_HTSUBAKI, 0x80000, 0x800000, 0x200000, htsubakiLoads, 6);
}

struct BurnDriver BurnDrvHtsubaki = {
	"htsubaki", NULL, NULL, NULL, "1999",
	"Hanafuda Hana Tsubaki (Japan)\0", NULL, "Dynax", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, htsubakiRomInfo, htsubakiRomName, NULL, NULL, NULL, NULL, hnroseInputInfo, hnroseDIPInfo,
	htsubakiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 mjgnightInit()
{
	return DrvInit(BOARD_MJGNIGHT, 0x80000, 0x800000, 0x200000, mjgnightLoads, 7);
}

struct BurnDriver BurnDrvMjgnight = {
	"mjgnight", NULL, NULL, NULL, "2000",
	"Mahjong Gorgeous Night (Japan, TSM003-01)\0", NULL, "Techno-Top", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjgnightRomInfo, mjgnightRomName, NULL, NULL, NULL, NULL, mjgnightInputInfo, mjgnightDIPInfo,
	mjgnightInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 hnroseInit()
{
	return DrvInit(BOARD_HNROSE, 0x80000, 0x800000, 0x200000, hnroseLoads, 6);
}

struct BurnDriver BurnDrvHnrose = {
	"hnrose", NULL, NULL, NULL, "2000",
	"Hanafuda Night Rose (Japan, TSM008-04)\0", NULL, "Techno-Top", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hnroseRomInfo, hnroseRomName, NULL, NULL, NULL, NULL, hnroseInputInfo, hnroseDIPInfo,
	hnroseInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	335, 240, 4, 3
};

static INT32 mjswacadInit()
{
	return DrvInit(BOARD_MJGNIGHT, 0x80000, 0x800000, 0x200000, mjswacadLoads, 6);
}

struct BurnDriver BurnDrvMjswacad = {
	"mjswacad", NULL, NULL, NULL, "2000",
	"Mahjong Sweet Academy (Japan, TSM010-002)\0", NULL, "Techno-Top", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjswacadRomInfo, mjswacadRomName, NULL, NULL, NULL, NULL, mjgnightInputInfo, mjgnightDIPInfo,
	mjswacadInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};

static INT32 jongohInit()
{
	return DrvInit(BOARD_JONGOH, 0xa8000, 0x500000, 0x80000, jongohLoads, 5);
}

struct BurnDriver BurnDrvJongoh = {
	"jongoh", NULL, NULL, NULL, "2000",
	"Mahjong Jongoh (Japan, TTL800-03-04)\0", NULL, "Techno-Top", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, jongohRomInfo, jongohRomName, NULL, NULL, NULL, NULL, jongohInputInfo, jongohDIPInfo,
	jongohInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 239, 4, 3
};

static INT32 daireachInit()
{
	return DrvInit(BOARD_DAIREACH, 0xa8000, 0x600000, 0x80000, daireachLoads, 5);
}

struct BurnDriver BurnDrvDaireach = {
	"daireach", NULL, NULL, NULL, "2001",
	"Mahjong Dai-Reach (Japan, TSM012-C01)\0", NULL, "Techno-Top", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, daireachRomInfo, daireachRomName, NULL, NULL, NULL, NULL, seljan2InputInfo, seljan2DIPInfo,
	daireachInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 239, 4, 3
};

static INT32 daichukaInit()
{
	return DrvInit(BOARD_DAICHUKA, 0xa8000, 0x600000, 0x80000, daichukaLoads, 5);
}

struct BurnDriver BurnDrvDaichuka = {
	"daichuka", NULL, NULL, NULL, "2002",
	"Mahjong Dai Chuuka Ken (Japan, P830 006A-005)\0", "Bad program ROM dump (use daichukaa)", "Techno-Top", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, daichukaRomInfo, daichukaRomName, NULL, NULL, NULL, NULL, jongohInputInfo, jongohDIPInfo,
	daichukaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 239, 4, 3
};

static INT32 daichukaaInit()
{
	return DrvInit(BOARD_DAICHUKAA, 0xa8000, 0x600000, 0x80000, daichukaaLoads, 4);
}

struct BurnDriver BurnDrvDaichukaa = {
	"daichukaa", "daichuka", NULL, NULL, "2002",
	"Mahjong Dai Chuuka Ken (Japan, P830-004A-004)\0", "Set inspection period in service menu on first run", "Techno-Top", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, daichukaaRomInfo, daichukaaRomName, NULL, NULL, NULL, NULL, jongohInputInfo, jongohDIPInfo,
	daichukaaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 239, 4, 3
};

static INT32 daimyojnInit()
{
	return DrvInit(BOARD_DAIMYOJN, 0x80000, 0x400000, 0x80000, daimyojnLoads, 4);
}

struct BurnDriver BurnDrvDaimyojn = {
	"daimyojn", NULL, NULL, NULL, "2002",
	"Mahjong Daimyojin (Japan, T017-PB-00)\0", NULL, "Dynax / Techno-Top / Techno-Planning", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, daimyojnRomInfo, daimyojnRomName, NULL, NULL, NULL, NULL, daimyojnInputInfo, daimyojnDIPInfo,
	daimyojnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	335, 240, 4, 3
};

static INT32 mjswacadsInit()
{
	return DrvInit(BOARD_MJSWACADS, 0x80000, 0x800000, 0x100000, mjswacadsLoads, 6);
}

struct BurnDriver BurnDrvMjswacads = {
	"mjswacads", NULL, NULL, NULL, "2002",
	"Mahjong Sweet Academy Special (Japan, P010B-Y05)\0", NULL, "Techno-Top / Long Stone", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjswacadsRomInfo, mjswacadsRomName, NULL, NULL, NULL, NULL, daimyojnInputInfo, daimyojnDIPInfo,
	mjswacadsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	335, 240, 4, 3
};

static INT32 mjtenhoInit()
{
	return DrvInit(BOARD_DAIMYOJN, 0x80000, 0x400000, 0x80000, mjtenhoLoads, 4);
}

struct BurnDriver BurnDrvMjtenho = {
	"mjtenho", NULL, NULL, NULL, "2002",
	"Mahjong Tenho (Japan, P016B-000)\0", NULL, "Techno-Top", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjtenhoRomInfo, mjtenhoRomName, NULL, NULL, NULL, NULL, daimyojnInputInfo, daimyojnDIPInfo,
	mjtenhoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	335, 240, 4, 3
};

static INT32 momotaroInit()
{
	return DrvInit(BOARD_MOMOTARO, 0x80000, 0x400000, 0x80000, momotaroLoads, 4);
}

struct BurnDriver BurnDrvMomotaro = {
	"momotaro", NULL, NULL, NULL, "2004",
	"Mahjong Momotarou (Japan, T027-RB-01)\0", "Bad graphics ROM dumps (redump required)", "Techno-Top", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	0, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, momotaroRomInfo, momotaroRomName, NULL, NULL, NULL, NULL, daimyojnInputInfo, daimyojnDIPInfo,
	momotaroInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	335, 240, 4, 3
};

static INT32 mjnigiriInit()
{
	return DrvInit(BOARD_MJNIGIRI, 0x80000, 0x400000, 0x80000, mjnigiriLoads, 4);
}

struct BurnDriver BurnDrvMjnigiri = {
	"mjnigiri", NULL, NULL, NULL, "2007",
	"Mahjong Nigiri Itcho!! (Japan, T038-PB-002)\0", NULL, "Techno-Top", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjnigiriRomInfo, mjnigiriRomName, NULL, NULL, NULL, NULL, mjnigiriInputInfo, mjnigiriDIPInfo,
	mjnigiriInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	335, 240, 4, 3
};

static INT32 mjkokryuInit()
{
	INT32 ret = DrvInit(BOARD_MOMOTARO, 0x80000, 0x800000, 0x100000, mjkokryuLoads, 6);
	if (ret)
		return ret;

	// MAME supplies factory NVRAM with 36 months until the next inspection.
	// The frontend restores the user's battery RAM after driver initialization.
	return BurnLoadRom(DrvRAM, 6, 1);
}

struct BurnDriver BurnDrvMjkokryu = {
	"mjkokryu", NULL, NULL, NULL, "2008",
	"Mahjong Kokuryu (Japan, T040-RB-03)\0", NULL, "Techno-Top", "Dynax blitter rev. 3",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mjkokryuRomInfo, mjkokryuRomName, NULL, NULL, NULL, NULL, mjkokryuInputInfo, mjkokryuDIPInfo,
	mjkokryuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	335, 240, 4, 3
};
