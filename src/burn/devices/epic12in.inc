/* blitter function */

static const void FUNCNAME(BLIT_PARAMS)
{
	UINT32* gfx2;
	int y, yf;

#if BLENDED == 1
#if TINT == 0 && ((_SMODE == 0 && (_DMODE == 0 || _DMODE == 1 || _DMODE == 5)) || (_SMODE == 2 && _DMODE == 0))
#define FAST_BLEND_LUT 1
#else
#define FAST_BLEND_LUT 0
#endif
#else
#define FAST_BLEND_LUT 0
#endif

#if REALLY_SIMPLE == 0 && FAST_BLEND_LUT == 0
	colour_t s_clr;
#endif

#if BLENDED == 1 && FAST_BLEND_LUT == 0
	colour_t d_clr;

#if _SMODE == 2
#if _DMODE != 0
	colour_t clr0;
#endif
#elif _SMODE == 0
#if _DMODE != 0
#if _DMODE != 5
#if _DMODE != 1
	colour_t clr0;
#endif
#endif
#endif
#else
	colour_t clr0;
#endif


#endif

#if REALLY_SIMPLE == 1
#if TRANSPARENT == 1
	UINT32 pen;
#endif
#else
	UINT32 pen;
#endif
	UINT32 *bmp;

#if FLIPX == 1
	src_x += (dimx-1);
#endif

	if (flipy)  {   yf = -1;    src_y += (dimy-1);  }
	else        {   yf = +1;                        }

	int starty = 0;
	const int dst_y_end = dst_y_start+dimy;

	if (dst_y_start < clip->min_y)
		starty = clip->min_y - dst_y_start;

	if (dst_y_end > clip->max_y)
		dimy -= (dst_y_end-1) - clip->max_y;

	// check things are safe to draw (note, if the source would wrap round an edge of the 0x2000*0x1000 vram we don't draw.. not sure what the hw does anyway)
	// ddpdfk triggers this on boss explosions so it needs fixing
#if FLIPX == 1
	if ((src_x &0x1fff) < ((src_x-(dimx-1))&0x1fff))
	{
	//  popmessage("sprite gets clipped off src_x %04x dimx %04x\n", src_x, dimx);
		return;
	}
#else
	if ((src_x &0x1fff) > ((src_x+(dimx-1))&0x1fff))
	{
	//  popmessage("sprite gets clipped off src_x %04x dimx %04x\n", src_x, dimx);
		return;
	}
#endif

	int startx = 0;
	const int dst_x_end = dst_x_start+dimx;

	if (dst_x_start < clip->min_x)
		startx = clip->min_x - dst_x_start;

	if (dst_x_end > clip->max_x)
		dimx -= (dst_x_end-1) - clip->max_x;

// wrong/unsafe slowdown sim
	if (dimy > starty && dimx > startx)
	{
		epic12_device_blit_delay += (dimy - starty)*(dimx - startx);

		//printf("delay is now %d\n", epic12_device_blit_delay);
	}

#if BLENDED == 1
#if _SMODE == 0
#if _DMODE == 0
	const UINT8* blend_table = epic12_blend_s0_d0 + ((s_alpha * 0x20 + d_alpha) * 0x400);
#endif

#if _DMODE == 5
	const UINT8* blend_table = epic12_blend_s0_d5 + s_alpha * 0x400;
#endif
#if _DMODE == 1
	const UINT8* blend_table = epic12_blend_s0_d1 + s_alpha * 0x400;
#endif

#endif

#if _SMODE == 2
#if _DMODE == 0
	const UINT8* blend_table = epic12_blend_s2_d0 + d_alpha * 0x400;
#endif
#endif
#endif



	for (y = starty; y < dimy; y++)
	{
		//bmp = &bitmap->pix(dst_y_start + y, dst_x_start+startx);
		bmp = m_bitmaps + (dst_y_start + y) * 0x2000 + (dst_x_start+startx);

		const int ysrc_index =  ((src_y + yf * y) & 0x0fff) * 0x2000;
		gfx2 = gfx + ysrc_index;

		#if FLIPX == 1
			gfx2 += (src_x-startx);
		#else
			gfx2 += (src_x+startx);
		#endif

#if REALLY_SIMPLE == 1 && TRANSPARENT == 0 && FLIPX == 0
		const int copy_width = dimx - startx;
		if (copy_width > 0) {
			if (bmp + copy_width <= gfx2 || gfx2 + copy_width <= bmp) {
				memcpy(bmp, gfx2, copy_width * sizeof(UINT32));
			} else {
				const UINT32 *end = bmp + copy_width;
				while (bmp < end) {
					*bmp++ = *gfx2++;
				}
			}
		}
#elif REALLY_SIMPLE == 1 && TRANSPARENT == 1 && FLIPX == 0
		const int copy_width = dimx - startx;
		if (copy_width > 0) {
			if (bmp + copy_width <= gfx2 || gfx2 + copy_width <= bmp) {
				const UINT32 *end4 = gfx2 + (copy_width & ~3);
				while (gfx2 < end4) {
					const UINT32 p0 = gfx2[0];
					const UINT32 p1 = gfx2[1];
					const UINT32 p2 = gfx2[2];
					const UINT32 p3 = gfx2[3];
					const UINT32 opaque = (p0 & p1 & p2 & p3) & 0x20000000;
					const UINT32 visible = (p0 | p1 | p2 | p3) & 0x20000000;

					if (opaque) {
						memcpy(bmp, gfx2, 4 * sizeof(UINT32));
					} else if (visible) {
						if (p0 & 0x20000000) bmp[0] = p0;
						if (p1 & 0x20000000) bmp[1] = p1;
						if (p2 & 0x20000000) bmp[2] = p2;
						if (p3 & 0x20000000) bmp[3] = p3;
					}

					gfx2 += 4;
					bmp += 4;
				}

				const UINT32 *end = gfx2 + (copy_width & 3);
				while (gfx2 < end) {
					const UINT32 value = *gfx2++;
					if (value & 0x20000000) *bmp = value;
					bmp++;
				}
			} else {
				const UINT32 *end = bmp + copy_width;
				while (bmp < end) {
					#include "epic12pixel.inc"
				}
			}
		}
#else
#if 1
		const UINT32* end = bmp+(dimx-startx);
#else
		// maybe we can do some SSE type optimizations on larger blocks? right now this just results in more code and slower compiling tho.

		const int width = dimx-startx;
		const UINT32* end = bmp+(width);

		if (width<0) return;

		int bigblocks = width>>3;

		while (bigblocks)
		{
			#include "epic12pixel.inc"
			#include "epic12pixel.inc"
			#include "epic12pixel.inc"
			#include "epic12pixel.inc"
			#include "epic12pixel.inc"
			#include "epic12pixel.inc"
			#include "epic12pixel.inc"
			#include "epic12pixel.inc"

			bigblocks--;
		}
#endif
		while (bmp<end)
		{
			#include "epic12pixel.inc"
		}
#endif

	}

//  g_profiler.stop();
}

#undef LOOP_INCREMENTS
#undef FAST_BLEND_LUT
