/* Super Kaneko Nova System Sprites

   "CG24173 6186" & "CG24143 4181" (always used as a pair?)

  - used by suprnova.c
            galpani3.c
            jchan.c

  - ToDo:
    Get rid of sprite position kludges
    Fix zooming precision/rounding (most noticeable on jchan backgrounds)

	Ported from MAME 0.144u4
	based on MAME sources by David Haywood
*/

#include "tiles_generic.h"

#define cliprect_min_y 0
#define cliprect_max_y (nScreenHeight-1)
#define cliprect_min_x 0
#define cliprect_max_x (nScreenWidth - 1)

#define SUPRNOVA_DECODE_BUFFER_SIZE	0x2000

static INT32 sprite_kludge_x, sprite_kludge_y;
static UINT8 decodebuffer[0x2000];

static INT32 skns_rle_decode ( INT32 romoffset, INT32 size, UINT8*gfx_source, INT32 gfx_length, bool *hasPixels = NULL )
{
	bool visible = false;
	INT32 srcpos = romoffset % gfx_length;
	INT32 dstpos = 0;
	while (size > 0) {
		const UINT8 code = gfx_source[srcpos];
		if (++srcpos == gfx_length) srcpos = 0;
		INT32 remaining = (code & 0x7f) + 1;
		// Decode the complete final packet, even when it exceeds requested size.
		size -= remaining;
		if (remaining == 1) {
			decodebuffer[dstpos] = gfx_source[srcpos];
			visible = true; // Keep the single-byte fast path free of value tests.
			if (++srcpos == gfx_length) srcpos = 0;
			dstpos = (dstpos + 1) & (SUPRNOVA_DECODE_BUFFER_SIZE - 1);
			continue;
		}
		if (code & 0x80) {
			// Literal packets may contain visible pixels; avoid scanning them twice.
			visible = true;
			while (remaining > 0) {
				INT32 count = remaining;
				if (count > gfx_length - srcpos) count = gfx_length - srcpos;
				if (count > SUPRNOVA_DECODE_BUFFER_SIZE - dstpos) count = SUPRNOVA_DECODE_BUFFER_SIZE - dstpos;
				memcpy(decodebuffer + dstpos, gfx_source + srcpos, count);
				srcpos += count;
				if (srcpos == gfx_length) srcpos = 0;
				dstpos = (dstpos + count) & (SUPRNOVA_DECODE_BUFFER_SIZE - 1);
				remaining -= count;
			}
		} else {
			const UINT8 value = gfx_source[srcpos];
			visible |= value != 0;
			if (++srcpos == gfx_length) srcpos = 0;
			while (remaining > 0) {
				INT32 count = remaining;
				if (count > SUPRNOVA_DECODE_BUFFER_SIZE - dstpos) count = SUPRNOVA_DECODE_BUFFER_SIZE - dstpos;
				memset(decodebuffer + dstpos, value, count);
				dstpos = (dstpos + count) & (SUPRNOVA_DECODE_BUFFER_SIZE - 1);
				remaining -= count;
			}
		}
	}
	if (hasPixels) *hasPixels = visible;
	return srcpos;
}

static void skns_blit_unscaled(UINT16 *bitmap, INT32 sx, INT32 sy, INT32 xsize, INT32 ysize, INT32 xflip, INT32 yflip, INT32 colour)
{
	if (xflip) sx -= xsize;
	if (yflip) sy -= ysize;
	const INT32 left = sx < 0 ? 0 : sx;
	const INT32 top = sy < 0 ? 0 : sy;
	const INT32 right = sx + xsize < nScreenWidth ? sx + xsize : nScreenWidth;
	const INT32 bottom = sy + ysize < nScreenHeight ? sy + ysize : nScreenHeight;
	if (left >= right || top >= bottom) return;
	const INT32 firstx = xflip ? xsize - 1 - (left - sx) : left - sx;
	const INT32 step = xflip ? -1 : 1;
	for (INT32 y = top; y < bottom; y++) {
		const INT32 srcy = yflip ? ysize - 1 - (y - sy) : y - sy;
		const UINT8 *src = decodebuffer + srcy * xsize;
		UINT16 *dst = bitmap + y * nScreenWidth;
		INT32 srcx = firstx;
		for (INT32 x = left; x < right; x++, srcx += step) {
			const UINT8 pixel = src[srcx];
			if (pixel) dst[x] = pixel + colour;
		}
	}
}

void skns_sprite_kludge(INT32 x, INT32 y)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_SknsSprInitted) bprintf(PRINT_ERROR, _T("skns_sprite_kludge called without init\n"));
#endif

	sprite_kludge_x = x;
	sprite_kludge_y = y;
}

/* Zooming blitter, zoom is by way of both source and destination offsets */
/* We are working in .6 fixed point if you hadn't guessed */

#define z_decls(step)				\
	UINT16 zxs = 0x40-(zx_m>>2);			\
	UINT16 zxd = 0x40-(zx_s>>2);		\
	UINT16 zys = 0x40-(zy_m>>2);			\
	UINT16 zyd = 0x40-(zy_s>>2);		\
	INT32 xs, ys, xd, yd, old, old2;		\
	INT32 step_spr = step;				\
	INT32 bxs = 0, bys = 0;				\
	INT32 clip_min_x = cliprect_min_x<<6;		\
	INT32 clip_max_x = (cliprect_max_x+1)<<6;	\
	INT32 clip_min_y = cliprect_min_y<<6;		\
	INT32 clip_max_y = (cliprect_max_y+1)<<6;	\
	sx <<= 6;					\
	sy <<= 6;					\
	x <<= 6;					\
	y <<= 6;

#define z_clamp_x_min() \
	if (x < clip_min_x) { \
		const INT32 steps = (clip_min_x - x + zxd - 1) / zxd; \
		bxs += steps * zxs; \
		x += steps * zxd; \
	}

#define z_clamp_x_max() \
	if (x > clip_max_x) { \
		const INT32 steps = (x - clip_max_x + zxd - 1) / zxd; \
		bxs += steps * zxs; \
		x -= steps * zxd; \
	}

#define z_clamp_y_min() \
	if (y < clip_min_y) { \
		const INT32 steps = (clip_min_y - y + zyd - 1) / zyd; \
		bys += steps * zys; \
		y += steps * zyd; \
		src += (bys >> 6) * step_spr; \
	}

#define z_clamp_y_max() \
	if (y > clip_max_y) { \
		const INT32 steps = (y - clip_max_y + zyd - 1) / zyd; \
		bys += steps * zys; \
		y -= steps * zyd; \
		src += (bys >> 6) * step_spr; \
	}

#define z_loop_x()			\
	xs = bxs;					\
	xd = x;					\
	while(xs < sx && xd <= clip_max_x)

#define z_loop_x_flip()			\
	xs = bxs;					\
	xd = x;					\
	while(xs < sx && xd >= clip_min_x)

#define z_loop_y()			\
	ys = bys;					\
	yd = y;					\
	while(ys < sy && yd <= clip_max_y)

#define z_loop_y_flip()			\
	ys = bys;					\
	yd = y;					\
	while(ys < sy && yd >= clip_min_y)

#define z_row() \
	UINT16 *const dst_row = (yd >> 6) < nScreenHeight ? bitmap + (yd >> 6) * nScreenWidth : NULL;

#define z_draw_pixel()				\
	UINT8 val = src[xs >> 6];			\
	if(val)					\
		if (dst_row && (xd>>6) < nScreenWidth)	\
			dst_row[xd>>6] = val + colour;

#define z_x_dst(op)			\
	old = xd;					\
	do {						\
		xs += zxs;					\
		xd op zxd;					\
	} while(!((xd^old) & ~0x3f));

#define z_y_dst(op)			\
	old = yd;					\
	old2 = ys;					\
	do {						\
		ys += zys;					\
		yd op zyd;					\
	} while(!((yd^old) & ~0x3f));			\
	while((ys^old2) & ~0x3f) {			\
		src += step_spr;				\
		old2 += 0x40;				\
	}

static void blit_nf_z(UINT16 *bitmap, const UINT8 *src, INT32 x, INT32 y, INT32 sx, INT32 sy, UINT16 zx_m, UINT16 zx_s, UINT16 zy_m, UINT16 zy_s, INT32 colour)
{
	z_decls(sx);
	z_clamp_x_min();
	z_clamp_y_min();
	z_loop_y() {
		z_row();
		z_loop_x() {
			z_draw_pixel();
			z_x_dst(+=);
		}
		z_y_dst(+=);
	}
}

static void blit_fy_z(UINT16 *bitmap, const UINT8 *src, INT32 x, INT32 y, INT32 sx, INT32 sy, UINT16 zx_m, UINT16 zx_s, UINT16 zy_m, UINT16 zy_s, INT32 colour)
{
	z_decls(sx);
	z_clamp_x_min();
	z_clamp_y_max();
	z_loop_y_flip() {
		z_row();
		z_loop_x() {
			z_draw_pixel();
			z_x_dst(+=);
		}
		z_y_dst(-=);
	}
}

static void blit_fx_z(UINT16 *bitmap, const UINT8 *src, INT32 x, INT32 y, INT32 sx, INT32 sy, UINT16 zx_m, UINT16 zx_s, UINT16 zy_m, UINT16 zy_s, INT32 colour)
{
	z_decls(sx);
	z_clamp_x_max();
	z_clamp_y_min();
	z_loop_y() {
		z_row();
		z_loop_x_flip() {
			z_draw_pixel();
			z_x_dst(-=);
		}
		z_y_dst(+=);
	}
}

static void blit_fxy_z(UINT16 *bitmap, const UINT8 *src, INT32 x, INT32 y, INT32 sx, INT32 sy, UINT16 zx_m, UINT16 zx_s, UINT16 zy_m, UINT16 zy_s, INT32 colour)
{
	z_decls(sx);
	z_clamp_x_max();
	z_clamp_y_max();
	z_loop_y_flip() {
		z_row();
		z_loop_x_flip() {
			z_draw_pixel();
			z_x_dst(-=);
		}
		z_y_dst(-=);
	}
}

static void (*const blit_z[4])(UINT16 *bitmap, const UINT8 *src, INT32 x, INT32 y, INT32 sx, INT32 sy, UINT16 zx_m, UINT16 zx_s, UINT16 zy_m, UINT16 zy_s, INT32 colour) = {
	blit_nf_z,
	blit_fy_z,
	blit_fx_z,
	blit_fxy_z,
};

// disable_priority is a hack to make jchan drawing a bit quicker (rather than moving the sprites around different bitmaps and adding colors
void skns_draw_sprites(UINT16 *bitmap, UINT32* spriteram_source, INT32 spriteram_size, UINT8* gfx_source, INT32 gfx_length, UINT32* sprite_regs, INT32 disable_priority)
{
#if defined FBNEO_DEBUG
	if (!DebugDev_SknsSprInitted) bprintf(PRINT_ERROR, _T("skns_draw_sprites called without init\n"));
#endif

	/*- SPR RAM Format -**

      16 bytes per sprite

	0x00  --ss --SS  z--- ----  jjjg g-ff  ppcc cccc

      s = y size
      S = x size
      j = joint
      g = group sprite is part of (if groups are enabled)
      f = flip
      p = priority
      c = palette

	0x04  ---- -aaa  aaaa aaaa  aaaa aaaa  aaaa aaaa

      a = ROM address of sprite data

	0x08  ZZZZ ZZ--  zzzz zz--  xxxx xxxx  xx-- ----

      Z = horizontal zoom table
      z = horizontal zoom subtable
      x = x position

	0x0C  ZZZZ ZZ--  zzzz zz--  yyyy yyyy  yy-- ----

      Z = vertical zoom table
      z = vertical zoom subtable
      x = y position

	**- End of Comments -*/

	UINT32 *source = spriteram_source;
	UINT32 *finish = source + spriteram_size/4;

	INT32 group_x_offset[4];
	INT32 group_y_offset[4];
	INT32 group_enable;
	INT32 group_number;
	INT32 sprite_flip;
	INT32 sprite_x_scroll;
	INT32 sprite_y_scroll;
	INT32 disabled = sprite_regs[0x04/4] & 0x08; // RWR1
	INT32 xsize,ysize, size, xpos=0,ypos=0, pri=0, romoffset, colour=0, xflip,yflip, joint;
	INT32 sx,sy;
	INT32 endromoffs=0, gfxlen;
	INT32 grow;
	UINT16 zoomx_m, zoomx_s, zoomy_m, zoomy_s;

	if ((!disabled)){

		group_enable    = (sprite_regs[0x00/4] & 0x0040) >> 6; // RWR0

		/* Sengekis uses global flip */
		sprite_flip = (sprite_regs[0x04/4] & 0x03); // RWR1

		sprite_y_scroll = ((sprite_regs[0x08/4] & 0x7fc0) >> 6); // RWR2
		sprite_x_scroll = ((sprite_regs[0x10/4] & 0x7fc0) >> 6); // RWR4
		if (sprite_y_scroll&0x100) sprite_y_scroll -= 0x200; // Signed
		if (sprite_x_scroll&0x100) sprite_x_scroll -= 0x200; // Signed

		group_x_offset[0] = (sprite_regs[0x18/4] & 0xffc0) >> 6; // RWR6
		group_y_offset[0] = (sprite_regs[0x1c/4] & 0xffc0) >> 6; // RWR7
		if (group_x_offset[0]&0x200) group_x_offset[0] -= 0x400; // Signed
		if (group_y_offset[0]&0x200) group_y_offset[0] -= 0x400; // Signed

		group_x_offset[1] = (sprite_regs[0x20/4] & 0xffc0) >> 6; // RWR8
		group_y_offset[1] = (sprite_regs[0x24/4] & 0xffc0) >> 6; // RWR9
		if (group_x_offset[1]&0x200) group_x_offset[1] -= 0x400; // Signed
		if (group_y_offset[1]&0x200) group_y_offset[1] -= 0x400; // Signed

		group_x_offset[2] = (sprite_regs[0x28/4] & 0xffc0) >> 6; // RWR10
		group_y_offset[2] = (sprite_regs[0x2c/4] & 0xffc0) >> 6; // RWR11
		if (group_x_offset[2]&0x200) group_x_offset[2] -= 0x400; // Signed
		if (group_y_offset[2]&0x200) group_y_offset[2] -= 0x400; // Signed

		group_x_offset[3] = (sprite_regs[0x30/4] & 0xffc0) >> 6; // RWR12
		group_y_offset[3] = (sprite_regs[0x34/4] & 0xffc0) >> 6; // RWR13
		if (group_x_offset[3]&0x200) group_x_offset[3] -= 0x400; // Signed
		if (group_y_offset[3]&0x200) group_y_offset[3] -= 0x400; // Signed

		/* Seems that sprites are consistently off by a fixed no. of pixels in different games
           (Patterns emerge through Manufacturer/Date/Orientation) */
		sprite_x_scroll += sprite_kludge_x;
		sprite_y_scroll += sprite_kludge_y;


		gfxlen = gfx_length;
		while( source<finish )
		{
			xflip = (source[0] & 0x00000200) >> 9;
			yflip = (source[0] & 0x00000100) >> 8;

			ysize = (source[0] & 0x30000000) >> 28;
			xsize = (source[0] & 0x03000000) >> 24;
			xsize ++;
			ysize ++;

			xsize *= 16;
			ysize *= 16;

			size = xsize * ysize;

			joint = (source[0] & 0x0000e000) >> 13;

			if (!(joint & 1))
			{
				xpos =  (source[2] & 0x0000ffc0) >> 6;
				ypos =  (source[3] & 0x0000ffc0) >> 6;

				xpos += sprite_x_scroll; // Global offset
				ypos += sprite_y_scroll;

				if (group_enable)
				{
					group_number = (source[0] & 0x00001800) >> 11;

					/* the group positioning doesn't seem to be working as i'd expect,
					if I apply the x position the cursor on galpani4 ends up moving
					from the correct position to too far right, also the y offset
					seems to cause the position to be off by one in galpans2 even if
					it fixes the position in galpani4?

					even if I take into account the global sprite scroll registers
					it isn't right

					global offset kludged using game specific offset -pjp */

					xpos += group_x_offset[group_number];
					ypos += group_y_offset[group_number];
				}
			}
			else
			{
				xpos +=  (source[2] & 0x0000ffc0) >> 6;
				ypos +=  (source[3] & 0x0000ffc0) >> 6;
			}

			if (xpos > 0x1ff) xpos -= 0x400;
			if (ypos > 0x1ff) ypos -= 0x400;

			/* Local sprite offset (for taking flip into account and drawing offset) */
			sx = xpos;
			sy = ypos;

			/* Global Sprite Flip (sengekis) */
			if (sprite_flip&2)
			{
				xflip ^= 1;
				sx = nScreenWidth - sx;
			}
			if (sprite_flip&1)
			{
				yflip ^= 1;
				sy = nScreenHeight - sy;
			}

			/* Palette linking */
			if (!(joint & 2))
			{
				colour = (source[0] & 0x0000003f) >> 0;
			}

			/* Priority and Tile linking */
			if (!(joint & 4))
			{
				romoffset = (source[1] & 0x07ffffff) >> 0;
				pri = (source[0] & 0x000000c0) >> 6;
			} else {
				romoffset = endromoffs;
			}

			grow = (source[0]>>23) & 1;

			if (!grow)
			{
				zoomx_m = (source[2] >> 24)&0x00fc;
				zoomx_s = (source[2] >> 16)&0x00fc;
				zoomy_m = (source[3] >> 24)&0x00fc;
				zoomy_s = (source[3] >> 16)&0x00fc;
			}
			else
			{
				// sengekis uses this on sprites which are shrinking as they head towards the ground
				// it's also used on the input test of Gals Panic S2
				//
				// it appears to offer a higher precision 'shrink' mode (although I'm not entirely
				//  convinced this implementation is correct because we simply end up ignoring
				//  part of the data)
				zoomx_m = 0;
				zoomx_s = (source[2] >> 24)&0x00fc;
				zoomy_m = 0;
				zoomy_s = (source[3] >> 24)&0x00fc;


			}

			romoffset &= gfxlen-1;

			bool hasPixels;
			endromoffs = skns_rle_decode ( romoffset, size, gfx_source, gfx_length, &hasPixels );

			// in Cyvern

			//  train in tunnel pri = 0x00
			//  nothing?         = 0x01
			//  players etc. pri = 0x02
			//  pickups etc. pri = 0x03

			// Decode/link state is already updated even for transparent repeat-only sprites.
			if (hasPixels) {
				INT32 NewColour = (colour<<8);
				if (disable_priority) {
					NewColour += disable_priority; // jchan hack
				} else {
					NewColour += (pri << 14);
				}

				if(zoomx_m || zoomx_s || zoomy_m || zoomy_s)
				{
					blit_z[ (xflip<<1) | yflip ](bitmap, decodebuffer, sx, sy, xsize, ysize, zoomx_m, zoomx_s, zoomy_m, zoomy_s, NewColour);
				}
				else
				{
					skns_blit_unscaled(bitmap, sx, sy, xsize, ysize, xflip, yflip, NewColour);
				}
			}

			source+=4;
		}
	}
}

void skns_init()
{
	DebugDev_SknsSprInitted = 1;
}

void skns_exit()
{
#if defined FBNEO_DEBUG
	if (!DebugDev_SknsSprInitted) bprintf(PRINT_ERROR, _T("skns_exit called without init\n"));
#endif

	DebugDev_SknsSprInitted = 0;
}
