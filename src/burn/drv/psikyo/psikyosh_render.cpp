// Video rendering module for Psikyo SH2 games
// Lots of code here and there ripped directly from MAME
// Thanks to David Haywood for the initial MAME driver
// as well as some other valuable pointers.

#include "tiles_generic.h" // nScreenWidth & nScreenHeight
#include "psikyosh_render.h" // contains loads of macros
#include "psikyosh_threads.h"

UINT8 *pPsikyoshTiles;
UINT32  *pPsikyoshSpriteBuffer;
UINT32  *pPsikyoshBgRAM;
UINT32  *pPsikyoshVidRegs;
UINT32  *pPsikyoshPalRAM;
UINT32  *pPsikyoshZoomRAM;

static UINT8 *DrvTransTab;
static UINT8 alphatable[0x100];
static UINT16 nibbleExpand[0x100];

static UINT16 *DrvPriBmp;
static UINT8 *DrvZoomBmp;
static INT32 nDrvZoomPrev = -1;
static INT32 nDrvZoomPrevGfx = -1;
static INT32 nDrvZoomPrevHigh = -1;
static INT32 nDrvZoomPrevWide = -1;
static UINT32  *DrvTmpDraw;
static UINT32  *DrvTmpDraw_ptr;
static PsikyoshThreadPool PsikyoshThreads;

static UINT32 sprite_priority_list[8][0x400];
static UINT16 sprite_priority_count[8];

static INT32 nGraphicsMin0;  // minimum tile number 4bpp
static INT32 nGraphicsMin1;  // for 8bpp
static INT32 nGraphicsSize;  // normal
static INT32 nGraphicsSize0; // for 4bpp
static INT32 nGraphicsSize1; // for 8bpp

//--------------------------------------------------------------------------------

static inline UINT32 alpha_blend(UINT32 d, UINT32 s, UINT32 p)
{
	if (p == 0) return d;

	INT32 a = 256 - p;

	return (((((s & 0xff00ff) * p) + ((d & 0xff00ff) * a)) & 0xff00ff00) |
		((((s & 0x00ff00) * p) + ((d & 0x00ff00) * a)) & 0x00ff0000)) >> 8;
}

//--------------------------------------------------------------------------------

static void draw_blendy_tile(INT32 gfx, INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 alpha, INT32 z)
{
	if (gfx == 0) {
		code &= 0x7ffff;
		code -= nGraphicsMin0;
		if (code < 0 || code > nGraphicsSize0) return;

		if (DrvTransTab[code >> 3] & (1 << (code & 7))) return;

		UINT32 *pal = pBurnDrvPalette + (color << 4);
		UINT8 *src = pPsikyoshTiles + (code << 7);
	
		INT32 inc = 8;
		if (fy) {
			inc = -8;
			src += 0x78;
		}

		if (sx >= 0 && sx < (nScreenWidth-16) && sy >= 0 && sy <= (nScreenHeight-16)) {
			if (z > 0) {
				if (fx) {
					if (alpha == 0xff) {
						PUTPIXEL_4BPP_NORMAL_PRIO_FLIPX()
					} else if (alpha >= 0) {
						PUTPIXEL_4BPP_ALPHA_PRIO_FLIPX()
					} else {
						PUTPIXEL_4BPP_ALPHATAB_PRIO_FLIPX()
					}
				} else {
					if (alpha == 0xff) {
						PUTPIXEL_4BPP_NORMAL_PRIO()
					} else if (alpha >= 0) {
						PUTPIXEL_4BPP_ALPHA_PRIO()
					} else {
						PUTPIXEL_4BPP_ALPHATAB_PRIO()
					}
				}
			} else {
				if (fx) {
					if (alpha == 0xff) {
						PUTPIXEL_4BPP_NORMAL_FLIPX()
					} else if (alpha >= 0) {
						PUTPIXEL_4BPP_ALPHA_FLIPX()
					} else {
						PUTPIXEL_4BPP_ALPHATAB_FLIPX()
					}
				} else {
					if (alpha == 0xff) {
						PUTPIXEL_4BPP_NORMAL()
					} else if (alpha >= 0) {
						PUTPIXEL_4BPP_ALPHA()
					} else {
						PUTPIXEL_4BPP_ALPHATAB()
					}
				}
			}
		} else {
			if (z > 0) {
				if (fx) {
					if (alpha == 0xff) {
						PUTPIXEL_4BPP_NORMAL_PRIO_FLIPX_CLIP()
					} else if (alpha >= 0) {
						PUTPIXEL_4BPP_ALPHA_PRIO_FLIPX_CLIP()
					} else {
						PUTPIXEL_4BPP_ALPHATAB_PRIO_FLIPX_CLIP()
					}
				} else {
					if (alpha == 0xff) {
						PUTPIXEL_4BPP_NORMAL_PRIO_CLIP()
					} else if (alpha >= 0) {
						PUTPIXEL_4BPP_ALPHA_PRIO_CLIP()
					} else {
						PUTPIXEL_4BPP_ALPHATAB_PRIO_CLIP()
					}
				}
			} else {
				if (fx) {
					if (alpha == 0xff) {
						PUTPIXEL_4BPP_NORMAL_FLIPX_CLIP()
					} else if (alpha >= 0) {
						PUTPIXEL_4BPP_ALPHA_FLIPX_CLIP()
					} else {
						PUTPIXEL_4BPP_ALPHATAB_FLIPX_CLIP()
					}
				} else {
					if (alpha == 0xff) {
						PUTPIXEL_4BPP_NORMAL_CLIP()
					} else if (alpha >= 0) {
						PUTPIXEL_4BPP_ALPHA_CLIP()
					} else {
						PUTPIXEL_4BPP_ALPHATAB_CLIP()
					}
				}
			}
		}
	} else {
		code &= 0x3ffff;
		code -= nGraphicsMin1;
		if (code < 0 || code > nGraphicsSize0) return;

		if (DrvTransTab[(code >> 3) + 0x10000] & (1 << (code & 7))) return;

		UINT32 *pal = pBurnDrvPalette + (color << 4);
		UINT8 *src = pPsikyoshTiles + (code << 8);

		INT32 inc = 16;
		if (fy) {
			inc = -16;
			src += 0xf0;
		}

		if (sx >= 0 && sx < (nScreenWidth-16) && sy >= 0 && sy < (nScreenHeight-16)) {
			if (z > 0) {
				if (fx) {
					if (alpha == 0xff) {
						PUTPIXEL_8BPP_NORMAL_PRIO_FLIPX()
					} else if (alpha >= 0) {
						PUTPIXEL_8BPP_ALPHA_PRIO_FLIPX()
					} else {
						PUTPIXEL_8BPP_ALPHATAB_PRIO_FLIPX()
					}
				} else {
					if (alpha == 0xff) {
						PUTPIXEL_8BPP_NORMAL_PRIO()
					} else if (alpha >= 0) {
						PUTPIXEL_8BPP_ALPHA_PRIO()
					} else {
						PUTPIXEL_8BPP_ALPHATAB_PRIO()
					}
				}
			} else {
				if (fx) {
					if (alpha == 0xff) {
						PUTPIXEL_8BPP_NORMAL_FLIPX()
					} else if (alpha >= 0) {
						PUTPIXEL_8BPP_ALPHA_FLIPX()
					} else {
						PUTPIXEL_8BPP_ALPHATAB_FLIPX()
					}
				} else {
					if (alpha == 0xff) {
						PUTPIXEL_8BPP_NORMAL()
					} else if (alpha >= 0) {
						PUTPIXEL_8BPP_ALPHA()
					} else {
						PUTPIXEL_8BPP_ALPHATAB()
					}
				}
			}
		} else {
			if (z > 0) {
				if (fx) {
					if (alpha == 0xff) {
						PUTPIXEL_8BPP_NORMAL_PRIO_FLIPX_CLIP()
					} else if (alpha >= 0) {
						PUTPIXEL_8BPP_ALPHA_PRIO_FLIPX_CLIP()
					} else {
						PUTPIXEL_8BPP_ALPHATAB_PRIO_FLIPX_CLIP()
					}
				} else {
					if (alpha == 0xff) {
						PUTPIXEL_8BPP_NORMAL_PRIO_CLIP()
					} else if (alpha >= 0) {
						PUTPIXEL_8BPP_ALPHA_PRIO_CLIP()
					} else {
						PUTPIXEL_8BPP_ALPHATAB_PRIO_CLIP()
					}
				}
			} else {
				if (fx) {
					if (alpha == 0xff) {
						PUTPIXEL_8BPP_NORMAL_FLIPX_CLIP()
					} else if (alpha >= 0) {
						PUTPIXEL_8BPP_ALPHA_FLIPX_CLIP()
					} else {
						PUTPIXEL_8BPP_ALPHATAB_FLIPX_CLIP()
					}
				} else {
					if (alpha == 0xff) {
						PUTPIXEL_8BPP_NORMAL_CLIP()
					} else if (alpha >= 0) {
						PUTPIXEL_8BPP_ALPHA_CLIP()
					} else {
						PUTPIXEL_8BPP_ALPHATAB_CLIP()
					}
				}
			}

		}
	}
}

static void draw_prezoom(INT32 gfx, INT32 code, INT32 high, INT32 wide)
{
	// these probably aren't the safest routines, but they should be pretty fast.

	if (gfx) {
		INT32 tileno = (code & 0x3ffff) - nGraphicsMin1;
		if (tileno < 0 || tileno > nGraphicsSize1) tileno = 0;
		if (nDrvZoomPrev == tileno && nDrvZoomPrevGfx == gfx &&
			nDrvZoomPrevHigh == high && nDrvZoomPrevWide == wide) return;
		nDrvZoomPrev = tileno;
		nDrvZoomPrevGfx = gfx;
		nDrvZoomPrevHigh = high;
		nDrvZoomPrevWide = wide;
		UINT32 *gfxptr = (UINT32*)(pPsikyoshTiles + (tileno << 8));

		for (INT32 ytile = 0; ytile < high; ytile++)
		{
			for (INT32 xtile = 0; xtile < wide; xtile++)
			{
				UINT32 *dest = (UINT32*)(DrvZoomBmp + (ytile << 12) + (xtile << 4));

				for (INT32 ypixel = 0; ypixel < 16; ypixel++, gfxptr += 4) {

					dest[0] = gfxptr[0];
					dest[1] = gfxptr[1];
					dest[2] = gfxptr[2];
					dest[3] = gfxptr[3];

					dest += 64;
				}
			}
		}
	} else {
		INT32 tileno = (code & 0x7ffff) - nGraphicsMin0;
		if (tileno < 0 || tileno > nGraphicsSize0) tileno = 0;
		if (nDrvZoomPrev == tileno && nDrvZoomPrevGfx == gfx &&
			nDrvZoomPrevHigh == high && nDrvZoomPrevWide == wide) return;
		nDrvZoomPrev = tileno;
		nDrvZoomPrevGfx = gfx;
		nDrvZoomPrevHigh = high;
		nDrvZoomPrevWide = wide;
		UINT8 *gfxptr = pPsikyoshTiles + (tileno << 7);
		for (INT32 ytile = 0; ytile < high; ytile++)
		{
			for (INT32 xtile = 0; xtile < wide; xtile++)
			{
				UINT8 *dest = DrvZoomBmp + (ytile << 12) + (xtile << 4);

				for (INT32 ypixel = 0; ypixel < 16; ypixel++, gfxptr += 8)
				{
					UINT16 *dest16 = (UINT16*)dest;
					for (INT32 xbyte = 0; xbyte < 8; xbyte++) {
						dest16[xbyte] = nibbleExpand[gfxptr[xbyte]];
					}

					dest += 256;
				}
			}
		}
	}
}

struct PsikyoshZoomContext {
	UINT32 *draw;
	UINT16 *priority;
	const UINT8 *source;
	const UINT32 *palette;
	INT32 screenWidth;
	INT32 sx;
	INT32 sy;
	INT32 ex;
	INT32 yIndex;
	INT32 dy;
	INT32 alpha;
	INT32 z;
	const UINT16 *sourceX;
};

template<INT32 UsePriority, INT32 BlendMode>
static void draw_zoom_rows(void *opaque, INT32 begin, INT32 end)
{
	PsikyoshZoomContext *context = (PsikyoshZoomContext*)opaque;
	INT32 yIndex = context->yIndex + begin * context->dy;

	for (INT32 row = begin; row < end; row++, yIndex += context->dy) {
		const UINT8 *source = context->source + (yIndex >> 10) * 256;
		UINT32 *dest = context->draw + (context->sy + row) * context->screenWidth;
		UINT16 *priority = context->priority + (context->sy + row) * context->screenWidth;
		const UINT16 *sourceX = context->sourceX;

		for (INT32 x = context->sx; x < context->ex; x++, sourceX++) {
			if (UsePriority && context->z < priority[x]) continue;

			const INT32 color = source[*sourceX];
			if (color == 0) continue;

			if (BlendMode == 0) {
				dest[x] = context->palette[color];
			} else if (BlendMode == 1) {
				dest[x] = alpha_blend(dest[x], context->palette[color], context->alpha);
			} else {
				const INT32 pixelAlpha = alphatable[color];
				if (pixelAlpha == 0xff) {
					dest[x] = context->palette[color];
				} else {
					dest[x] = alpha_blend(dest[x], context->palette[color], pixelAlpha);
				}
			}

			if (UsePriority) priority[x] = context->z;
		}
	}
}

struct PsikyoshUnscaledSpriteContext {
	INT32 gfx;
	UINT32 code;
	INT32 color;
	INT32 flipx;
	INT32 flipy;
	INT32 offsx;
	INT32 offsy;
	INT32 alpha;
	INT32 wide;
	INT32 high;
	INT32 z;
};

static void draw_unscaled_sprite_rows(void *opaque, INT32 begin, INT32 end)
{
	PsikyoshUnscaledSpriteContext *context = (PsikyoshUnscaledSpriteContext*)opaque;

	for (INT32 row = begin; row < end; row++) {
		const INT32 ytile = context->flipy ? context->high - 1 - row : row;
		const INT32 sy = context->offsy + (ytile << 4);
		UINT32 code = context->code + row * context->wide;

		for (INT32 column = 0; column < context->wide; column++, code++) {
			const INT32 xtile = context->flipx ? context->wide - 1 - column : column;
			const INT32 sx = context->offsx + (xtile << 4);

			draw_blendy_tile(context->gfx, code, context->color, sx, sy,
				context->flipx, context->flipy, context->alpha, context->z);
		}
	}
}

static void psikyosh_drawgfxzoom(INT32 gfx, UINT32 code, INT32 color, INT32 flipx, INT32 flipy, INT32 offsx, 
				 INT32 offsy, INT32 alpha, INT32 zoomx, INT32 zoomy, INT32 wide, INT32 high, INT32 z)
{
	if (~nBurnLayer & 8) return;
	if (!zoomx || !zoomy) return;

	if (zoomx == 0x400 && zoomy == 0x400)
	{
		if (PsikyoshThreads.IsParallel() && wide * high >= 32 && high >= 4) {
			PsikyoshUnscaledSpriteContext context = {
				gfx, code, color, flipx, flipy, offsx, offsy, alpha, wide, high, z
			};
			PsikyoshThreads.ParallelFor(high, 1, draw_unscaled_sprite_rows, &context);
			return;
		}

		INT32 xstart, ystart, xend, yend, xinc, yinc, code_offset = 0;

		if (flipx)	{ xstart = wide-1; xend = -1;   xinc = -1; }
		else		{ xstart = 0;      xend = wide; xinc = +1; }

		if (flipy)	{ ystart = high-1; yend = -1;   yinc = -1; }
		else		{ ystart = 0;      yend = high; yinc = +1; }

		for (INT32 ytile = ystart; ytile != yend; ytile += yinc )
		{
			for (INT32 xtile = xstart; xtile != xend; xtile += xinc )
			{
				INT32 sx = offsx + (xtile << 4);
				INT32 sy = offsy + (ytile << 4);

				draw_blendy_tile(gfx, code + code_offset++, color, sx, sy, flipx, flipy, alpha, z);
			}
		}
	}
	else
	{
		draw_prezoom(gfx, code, high, wide);

		{
			UINT32 *pal = pBurnDrvPalette + (color << 4);

			INT32 sprite_screen_height = ((high << 24) / zoomy + 0x200) >> 10;
			INT32 sprite_screen_width  = ((wide << 24) / zoomx + 0x200) >> 10;

			if (sprite_screen_width && sprite_screen_height)
			{
				INT32 sx = offsx;
				INT32 sy = offsy;
				INT32 ex = sx + sprite_screen_width;
				INT32 ey = sy + sprite_screen_height;

				INT32 x_index_base;
				INT32 y_index;

				INT32 dx, dy;

				if (flipx) { x_index_base = (sprite_screen_width-1)*zoomx; dx = -zoomx; }
				else	   { x_index_base = 0; dx = zoomx; }

				if (flipy) { y_index = (sprite_screen_height-1)*zoomy; dy = -zoomy; }
				else	   { y_index = 0; dy = zoomy; }

				{
					if (sx < 0) {
						INT32 pixels = 0-sx;
						sx += pixels;
						x_index_base += pixels*dx;
					}
					if (sy < 0 ) {
						INT32 pixels = 0-sy;
						sy += pixels;
						y_index += pixels*dy;
					}
					if (ex > nScreenWidth) {
						INT32 pixels = ex-(nScreenWidth-1)-1;
						ex -= pixels;
					}
					if (ey > nScreenHeight)	{
						INT32 pixels = ey-(nScreenHeight-1)-1;
						ey -= pixels;
					}
				}

				if (ex > sx)
				{
					const INT32 zoomArea = (ex - sx) * (ey - sy);
					if (PsikyoshThreads.IsParallel() && zoomArea >= 0x2000 && (ey - sy) >= 16) {
						UINT16 sourceX[320];
						INT32 xIndex = x_index_base;
						for (INT32 x = 0; x < ex - sx; x++, xIndex += dx) {
							sourceX[x] = (UINT16)(xIndex >> 10);
						}
						PsikyoshZoomContext context = {
							DrvTmpDraw, DrvPriBmp, DrvZoomBmp, pal, nScreenWidth,
							sx, sy, ex, y_index, dy, alpha, z, sourceX
						};
						PsikyoshThreadCallback callback;

						if (alpha == 0xff) {
							callback = (z > 0) ? draw_zoom_rows<1, 0> : draw_zoom_rows<0, 0>;
						} else if (alpha >= 0) {
							callback = (z > 0) ? draw_zoom_rows<1, 1> : draw_zoom_rows<0, 1>;
						} else {
							callback = (z > 0) ? draw_zoom_rows<1, 2> : draw_zoom_rows<0, 2>;
						}

						PsikyoshThreads.ParallelFor(ey - sy, 8, callback, &context);
						return;
					}

					if (alpha == 0xff) {
						if (z > 0) {
							PUTPIXEL_ZOOM_NORMAL_PRIO()
						} else {
							PUTPIXEL_ZOOM_NORMAL()
						}
					} else if (alpha >= 0) {
						if (z > 0) {
							PUTPIXEL_ZOOM_ALPHA_PRIO()
						} else {
							PUTPIXEL_ZOOM_ALPHA()
						}
					} else {
						if (z > 0) {
							PUTPIXEL_ZOOM_ALPHATAB_PRIO()
						} else {
							PUTPIXEL_ZOOM_ALPHATAB()
						}
					}
				}
			}
		}
	}
}

static void build_sprite_priority_list()
{
	UINT32 *src = pPsikyoshSpriteBuffer;
	UINT16 *list = (UINT16 *)src + 0x3800/2;
	const UINT32 priorityRegister = pPsikyoshVidRegs[2];
	const UINT8 priorityMap[4] = {
		(UINT8)(priorityRegister >> 28),
		(UINT8)((priorityRegister >> 24) & 0x0f),
		(UINT8)((priorityRegister >> 20) & 0x0f),
		(UINT8)((priorityRegister >> 16) & 0x0f)
	};

	memset(sprite_priority_count, 0, sizeof(sprite_priority_count));

	for (UINT16 listcntr = 0; listcntr < 0x400; listcntr++)
	{
#ifdef LSB_FIRST
		UINT32 listdat = list[listcntr ^ 1];
#else
		UINT32 listdat = list[listcntr];
#endif
		UINT32 sprnum = (listdat & 0x03ff) << 2;
		UINT32 pri = priorityMap[(src[sprnum+1] >> 12) & 3];

		if (pri < 8) {
			sprite_priority_list[pri][sprite_priority_count[pri]++] =
				((UINT32)listcntr << 10) | (sprnum >> 2);
		}

		if (listdat & 0x4000) break;
	}
}

static void draw_sprites(UINT8 req_pri)
{
	UINT32   *src = pPsikyoshSpriteBuffer;
	UINT16 *zoom_table = (UINT16 *)pPsikyoshZoomRAM;
	UINT8  *alpha_table = (UINT8 *)pPsikyoshVidRegs;

	for (UINT16 listindex = 0; listindex < sprite_priority_count[req_pri]; listindex++)
	{
		UINT32 xpos, ypos, high, wide, flpx, flpy, zoomx, zoomy, tnum, colr, dpth;
		INT32 alpha;
		const UINT32 entry = sprite_priority_list[req_pri][listindex];
		const UINT16 listcntr = (UINT16)(entry >> 10);
		const UINT32 sprnum = (entry & 0x03ff) << 2;

		{
			const UINT32 position = src[sprnum+0];
			const UINT32 attribute = src[sprnum+1];
			const UINT32 graphics = src[sprnum+2];
			ypos  = (position & 0x03ff0000) >> 16;
			xpos  = (position & 0x000003ff);
			high  =((attribute & 0x0f000000) >> 24) + 1;
			wide  =((attribute & 0x00000f00) >>  8) + 1;
			flpy  = (attribute & 0x80000000) >> 31;
			flpx  = (attribute & 0x00008000) >> 15;
			zoomy = (attribute & 0x00ff0000) >> 16;
			zoomx = (attribute & 0x000000ff);
			tnum  = (graphics & 0x0007ffff);
			dpth  = (graphics & 0x00800000) >> 23;
			colr  = (graphics & 0xff000000) >> 24;
			alpha = (graphics & 0x00700000) >> 20;

			if (ypos & 0x200) ypos -= 0x400;
			if (xpos & 0x200) xpos -= 0x400;

#ifdef LSB_FIRST
			alpha = alpha_table[alpha ^ 3];
#else
			alpha = alpha_table[alpha];
#endif

			if (alpha & 0x80) {
				alpha = -1;
			} else {
				alpha = alphatable[alpha | 0xc0];
			}

#ifdef LSB_FIRST
			const UINT32 zoomyValue = zoom_table[zoomy ^ 1];
			const UINT32 zoomxValue = zoom_table[zoomx ^ 1];
#else
			const UINT32 zoomyValue = zoom_table[zoomy];
			const UINT32 zoomxValue = zoom_table[zoomx];
#endif
			if (zoomyValue && zoomxValue)
			{
				psikyosh_drawgfxzoom(dpth, tnum, colr, flpx, flpy, xpos, ypos, alpha, 
					zoomxValue, zoomyValue, wide, high, listcntr);
			}
		}
	}
}

struct PsikyoshLayerContext {
	INT32 gfx;
	INT32 alpha;
	INT32 codeBase;
	INT32 visibleColumns;
	const INT32 *visibleColumn;
	const INT32 *visibleColumnX;
	const INT32 *visibleRow;
	const INT32 *visibleRowY;
};

static void draw_layer_rows(void *opaque, INT32 begin, INT32 end)
{
	PsikyoshLayerContext *context = (PsikyoshLayerContext*)opaque;

	for (INT32 y = begin; y < end; y++) {
		for (INT32 x = 0; x < context->visibleColumns; x++) {
			const INT32 offs = (context->visibleRow[y] << 5) | context->visibleColumn[x];
			UINT32 code = pPsikyoshBgRAM[context->codeBase + offs];

			draw_blendy_tile(context->gfx, code & 0x7ffff, code >> 24,
				context->visibleColumnX[x], context->visibleRowY[y],
				0, 0, context->alpha, 0);
		}
	}
}

static void draw_layer(INT32 layer, INT32 bank, INT32 alpha, INT32 scrollx, INT32 scrolly)
{
	if ((bank < 0x0c) || (bank > 0x1f)) return;

	if (alpha & 0x80) {
		alpha = -1;
	} else {
		alpha = alphatable[alpha | 0xc0];
	}

	INT32 attr = pPsikyoshVidRegs[7] << (layer << 2);
	INT32 gfx  = attr & 0x00004000;
	INT32 size =(attr & 0x00001000) ? 32 : 16;
	INT32 wide = size * 16;
	INT32 visibleColumn[32];
	INT32 visibleColumnX[32];
	INT32 visibleRow[32];
	INT32 visibleRowY[32];
	INT32 visibleColumns = 0;
	INT32 visibleRows = 0;

	for (INT32 column = 0; column < 32; column++) {
		INT32 sx = ((column << 4) + scrollx) & 0x1ff;
		if (sx >= nScreenWidth) sx -= 0x200;
		if (sx < -15) continue;

		visibleColumn[visibleColumns] = column;
		visibleColumnX[visibleColumns++] = sx;
	}

	for (INT32 row = 0; row < size; row++) {
		INT32 sy = ((row << 4) + scrolly) & (wide - 1);
		if (sy >= nScreenHeight) sy -= wide;
		if (sy < -15) continue;

		visibleRow[visibleRows] = row;
		visibleRowY[visibleRows++] = sy;
	}

	if (PsikyoshThreads.IsParallel() && visibleRows >= 4 &&
		visibleRows * visibleColumns >= 0x100) {
		PsikyoshLayerContext context = {
			gfx, alpha, (bank * 0x800) / 4 - 0x4000 / 4, visibleColumns,
			visibleColumn, visibleColumnX, visibleRow, visibleRowY
		};
		PsikyoshThreads.ParallelFor(visibleRows, 1, draw_layer_rows, &context);
		return;
	}

	for (INT32 y = 0; y < visibleRows; y++) {
		for (INT32 x = 0; x < visibleColumns; x++) {
			const INT32 offs = (visibleRow[y] << 5) | visibleColumn[x];
			UINT32 code = pPsikyoshBgRAM[(bank*0x800)/4 + offs - 0x4000/4];

			draw_blendy_tile(gfx, code & 0x7ffff, (code >> 24),
				visibleColumnX[x], visibleRowY[y], 0, 0, alpha, 0);
		}
	}
}

static void draw_bglayer(INT32 layer)
{
	if (!(nBurnLayer & 1)) return;

	INT32 scrollx, scrolly, bank, alpha;
	INT32 scrollbank = ((pPsikyoshVidRegs[6] << (layer << 3)) >> 24) & 0x7f;
	INT32 offset = (scrollbank == 0x0b) ? 0x200 : 0;

	bank    = (pPsikyoshBgRAM[0x17f0/4 + offset + layer] & 0x000000ff);
	alpha   = (pPsikyoshBgRAM[0x17f0/4 + offset + layer] & 0x0000bf00) >> 8;
	scrollx = (pPsikyoshBgRAM[0x13f0/4 + offset + layer] & 0x000001ff);
	scrolly = (pPsikyoshBgRAM[0x13f0/4 + offset + layer] & 0x03ff0000) >> 16;

	if (scrollbank == 0x0d) scrollx += 0x08;

	draw_layer(layer, bank, alpha, scrollx, scrolly);
}

static void draw_bglayertext(INT32 layer)
{
	if (~nBurnLayer & 2) return;

	INT32 scrollx, scrolly, bank, alpha;
	INT32 scrollbank = ((pPsikyoshVidRegs[6] << (layer << 3)) >> 24) & 0x7f;

	bank    = (pPsikyoshBgRAM[(scrollbank*0x800)/4 + 0x0400/4 - 0x4000/4] & 0x000000ff);
	alpha   = (pPsikyoshBgRAM[(scrollbank*0x800)/4 + 0x0400/4 - 0x4000/4] & 0x0000bf00) >> 8;
	scrollx = (pPsikyoshBgRAM[(scrollbank*0x800)/4 - 0x4000/4           ] & 0x000001ff);
	scrolly = (pPsikyoshBgRAM[(scrollbank*0x800)/4 - 0x4000/4           ] & 0x03ff0000) >> 16;

	draw_layer(layer, bank, alpha, scrollx, scrolly);

	bank    = (pPsikyoshBgRAM[(scrollbank*0x800)/4 + 0x0400/4 + 0x20/4 - 0x4000/4] & 0x000000ff);
	alpha   = (pPsikyoshBgRAM[(scrollbank*0x800)/4 + 0x0400/4 + 0x20/4 - 0x4000/4] & 0x0000bf00) >> 8;
	scrollx = (pPsikyoshBgRAM[(scrollbank*0x800)/4 - 0x4000/4 + 0x20/4           ] & 0x000001ff);
	scrolly = (pPsikyoshBgRAM[(scrollbank*0x800)/4 - 0x4000/4 + 0x20/4           ] & 0x03ff0000) >> 16;

	draw_layer(layer, bank, alpha, scrollx, scrolly);
}

static void draw_bglayerscroll(INT32 layer)
{
	if (!(nBurnLayer & 4)) return;

	INT32 scrollx, bank, alpha;
	INT32 scrollbank = ((pPsikyoshVidRegs[6] << (layer << 3)) >> 24) & 0x7f;

	bank    = (pPsikyoshBgRAM[(scrollbank*0x800)/4 + 0x0400/4 - 0x4000/4] & 0x000000ff);
	alpha   = (pPsikyoshBgRAM[(scrollbank*0x800)/4 + 0x0400/4 - 0x4000/4] & 0x0000bf00) >> 8;
	scrollx = (pPsikyoshBgRAM[(scrollbank*0x800)/4 - 0x4000/4           ] & 0x000001ff);
//	scrolly = (pPsikyoshBgRAM[(scrollbank*0x800)/4 - 0x4000/4           ] & 0x03ff0000) >> 16;

	draw_layer(layer, bank, alpha, scrollx, 0);
}

static void draw_background(UINT8 req_pri)
{
	for (INT32 i = 0; i < 3; i++)
	{
		if (!((pPsikyoshVidRegs[7] << (i << 2)) & 0x8000))
			continue;

		INT32 bgtype = ((pPsikyoshVidRegs[6] << (i << 3)) >> 24) & 0x7f;

		switch (bgtype)
		{
			case 0x0a: // Normal
				if((pPsikyoshBgRAM[0x17f0/4 + (i*0x04)/4] >> 24) == req_pri)
					draw_bglayer(i);
				break;

			case 0x0b: // Alt / Normal
				if((pPsikyoshBgRAM[0x1ff0/4 + (i*0x04)/4] >> 24) == req_pri)
					draw_bglayer(i);
				break;

			case 0x0c: // Using normal for now
			case 0x0d: // Using normal for now
				if((pPsikyoshBgRAM[(bgtype*0x800)/4 + 0x400/4 - 0x4000/4] >> 24) == req_pri)
					draw_bglayertext(i);
				break;

			case 0x0e:
			case 0x10: case 0x11: case 0x12: case 0x13:
			case 0x14: case 0x15: case 0x16: case 0x17:
			case 0x18: case 0x19: case 0x1a: case 0x1b:
			case 0x1c: case 0x1d: case 0x1e: case 0x1f:
				if((pPsikyoshBgRAM[(bgtype*0x800)/4 + 0x400/4 - 0x4000/4] >> 24) == req_pri)
					draw_bglayerscroll(i);
				break;
		}
	}
}

struct PsikyoshFramePrepareContext {
	UINT32 *draw;
	UINT16 *priority;
	UINT32 *palette;
	const UINT32 *paletteRam;
	const UINT32 *line;
	INT32 width;
	INT32 height;
	INT32 paletteEntries;
};

static void prepare_frame_rows(void *opaque, INT32 begin, INT32 end)
{
	PsikyoshFramePrepareContext *context = (PsikyoshFramePrepareContext*)opaque;
	const INT32 paletteBegin = (INT32)(((INT64)begin * context->paletteEntries) / context->height);
	const INT32 paletteEnd = (INT32)(((INT64)end * context->paletteEntries) / context->height);

	for (INT32 y = begin; y < end; y++) {
		UINT32 *draw = context->draw + y * context->width;
		UINT16 *priority = context->priority + y * context->width;
		const UINT32 line = context->line[y];

		if (line & 0xff) {
			const UINT32 color = line >> 8;
			for (INT32 x = 0; x < context->width; x++) {
				draw[x] = color;
			}
		} else {
			memset(draw, 0, context->width * sizeof(UINT32));
		}
		memset(priority, 0, context->width * sizeof(UINT16));
	}

	for (INT32 i = paletteBegin; i < paletteEnd; i++) {
		context->palette[i] = context->paletteRam[i] >> 8;
	}
}

struct PsikyoshLineContext {
	UINT32 *draw;
	const UINT32 *line;
	INT32 width;
};

static void postlineblend_rows(void *opaque, INT32 begin, INT32 end)
{
	PsikyoshLineContext *context = (PsikyoshLineContext*)opaque;

	for (INT32 y = begin; y < end; y++) {
		UINT32 *destline = context->draw + y * context->width;
		if (context->line[y] & 0x80) {
			const UINT32 color = context->line[y] >> 8;
			for (INT32 x = 0; x < context->width; x++) {
				destline[x] = color;
			}
		}
		else if (context->line[y] & 0x7f) {
			const UINT32 color = context->line[y] >> 8;
			const UINT32 alpha = (context->line[y] & 0x7f) << 1;
			for (INT32 x = 0; x < context->width; x++) {
				destline[x] = alpha_blend(destline[x], color, alpha);
			}
		}
	}
}

struct PsikyoshTransferContext {
	const UINT32 *source;
	UINT8 *destination;
	INT32 width;
	INT32 bytesPerPixel;
};

static void transfer_rows(void *opaque, INT32 begin, INT32 end)
{
	PsikyoshTransferContext *context = (PsikyoshTransferContext*)opaque;

	for (INT32 y = begin; y < end; y++) {
		const UINT32 *source = context->source + y * context->width;
		UINT8 *destination = context->destination + y * context->width * context->bytesPerPixel;

		for (INT32 x = 0; x < context->width; x++) {
			const UINT32 color = source[x];
			PutPix(destination + x * context->bytesPerPixel,
				BurnHighCol(color >> 16, color >> 8, color, 0));
		}
	}
}

INT32 PsikyoshDraw()
{
	if (nBurnBpp == 4) {
		DrvTmpDraw = (UINT32*)pBurnDraw;
	} else {
		DrvTmpDraw = DrvTmpDraw_ptr;
	}

	UINT32 *psikyosh_vidregs = pPsikyoshVidRegs;
	PsikyoshLineContext line = { DrvTmpDraw, pPsikyoshBgRAM, nScreenWidth };
	PsikyoshFramePrepareContext prepare = {
		DrvTmpDraw, DrvPriBmp, pBurnDrvPalette, pPsikyoshPalRAM,
		pPsikyoshBgRAM, nScreenWidth, nScreenHeight, 0x5000 / 4
	};
	PsikyoshThreads.ParallelFor(nScreenHeight, 64, prepare_frame_rows, &prepare);
	build_sprite_priority_list();

	for (UINT32 i = 0; i < 8; i++) {
		draw_sprites(i);
		draw_background(i);
		if ((psikyosh_vidregs[2] & 0x0f) == i) {
			line.line = pPsikyoshBgRAM + 0x0400/4;
			PsikyoshThreads.ParallelFor(nScreenHeight, 64, postlineblend_rows, &line);
		}
	}

	if (nBurnBpp < 4) {
		PsikyoshTransferContext transfer = {
			DrvTmpDraw, pBurnDraw, nScreenWidth, nBurnBpp
		};
		PsikyoshThreads.ParallelFor(nScreenHeight, 64, transfer_rows, &transfer);
	}

	return 0;
}

static void fill_alphatable()
{
	for (INT32 i = 0; i < 0xc0; i++)
		alphatable[i] = 0xff;

	for (INT32 i = 0; i < 0x40; i++) {
		alphatable[i | 0xc0] = ((0x3f - i) * 0xff) / 0x3f;
	}

	for (INT32 i = 0; i < 0x100; i++) {
#ifdef LSB_FIRST
		nibbleExpand[i] = (UINT16)((i >> 4) | ((i & 0x0f) << 8));
#else
		nibbleExpand[i] = (UINT16)(((i >> 4) << 8) | (i & 0x0f));
#endif
	}
}

struct PsikyoshTransTabContext {
	UINT8 *destination;
	const UINT8 *graphics;
	INT32 graphicsSize;
	INT32 tileBytes;
};

static void calculate_transtab_range(void *opaque, INT32 begin, INT32 end)
{
	PsikyoshTransTabContext *context = (PsikyoshTransTabContext*)opaque;

	for (INT32 output = begin; output < end; output++) {
		UINT8 transparent = 0xff;

		for (INT32 bit = 0; bit < 8; bit++) {
			const INT32 tileOffset = (output * 8 + bit) * context->tileBytes;
			if (tileOffset >= context->graphicsSize) break;

			const UINT64 *source = (const UINT64*)(context->graphics + tileOffset);
			UINT64 used = 0;
			for (INT32 i = 0; i < context->tileBytes / (INT32)sizeof(UINT64); i++) {
				used |= source[i];
			}
			if (used) transparent &= ~(1 << bit);
		}

		context->destination[output] = transparent;
	}
}

static void calculate_transtab()
{
	DrvTransTab = (UINT8*)BurnMalloc(0x18000);

	memset (DrvTransTab, 0xff, 0x18000);

	PsikyoshTransTabContext context4 = {
		DrvTransTab + 0x00000, pPsikyoshTiles, nGraphicsSize, 0x80
	};
	PsikyoshThreads.ParallelFor((nGraphicsSize + 0x3ff) / 0x400,
		0x400, calculate_transtab_range, &context4);

	PsikyoshTransTabContext context8 = {
		DrvTransTab + 0x10000, pPsikyoshTiles, nGraphicsSize, 0x100
	};
	PsikyoshThreads.ParallelFor((nGraphicsSize + 0x7ff) / 0x800,
		0x200, calculate_transtab_range, &context8);
}

void PsikyoshVideoInit(INT32 gfx_max, INT32 gfx_min)
{
	DrvZoomBmp	= (UINT8 *)BurnMalloc(16 * 16 * 16 * 16);
	DrvPriBmp	= (UINT16*)BurnMalloc(320 * 240 * sizeof(INT16));
	DrvTmpDraw_ptr	= (UINT32  *)BurnMalloc(320 * 240 * sizeof(UINT32));

	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		BurnDrvGetVisibleSize(&nScreenHeight, &nScreenWidth);
	} else {
		BurnDrvGetVisibleSize(&nScreenWidth, &nScreenHeight);
	}

	nGraphicsSize  = gfx_max - gfx_min;
	nGraphicsMin0  = (gfx_min / 128);
	nGraphicsMin1  = (gfx_min / 256);
	nGraphicsSize0 = (nGraphicsSize / 128) - 1;
	nGraphicsSize1 = (nGraphicsSize / 256) - 1;

	PsikyoshThreads.Configure();
	calculate_transtab();
	fill_alphatable();
}

void PsikyoshVideoExit()
{
	PsikyoshThreads.Shutdown();

	BurnFree (DrvZoomBmp);
	BurnFree (DrvPriBmp);
	BurnFree (DrvTmpDraw_ptr);
	DrvTmpDraw = NULL;
	BurnFree (DrvTransTab);
	
	nDrvZoomPrev		= -1;
	nDrvZoomPrevGfx		= -1;
	nDrvZoomPrevHigh		= -1;
	nDrvZoomPrevWide		= -1;
	pPsikyoshTiles		= NULL;
	pPsikyoshSpriteBuffer	= NULL;
	pPsikyoshBgRAM		= NULL;
	pPsikyoshVidRegs	= NULL;
	pPsikyoshPalRAM		= NULL;
	pPsikyoshZoomRAM	= NULL;
	pBurnDrvPalette		= NULL;

	nScreenWidth = nScreenHeight = 0;
}
