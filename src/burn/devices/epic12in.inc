/* blitter function */

static const void FUNCNAME(BLIT_PARAMS)
{
	UINT32* gfx2;
	int y, yf;

#if REALLY_SIMPLE == 0
	colour_t s_clr;
#endif

#if BLENDED == 1
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
		*delay_pixels += (dimy - starty)*(dimx - startx);

		//printf("delay is now %d\n", epic12_device_blit_delay);
	}

#if BLENDED == 1
#if _SMODE == 0
#if _DMODE == 0
	const UINT8* salpha_table = epic12_device_colrtable[s_alpha];
	const UINT8* dalpha_table = epic12_device_colrtable[d_alpha];
#endif

#if _DMODE == 5
	const UINT8* salpha_table = epic12_device_colrtable[s_alpha];
#endif
#if _DMODE == 1
	const UINT8* salpha_table = epic12_device_colrtable[s_alpha];
#endif

#endif

#if _SMODE == 2
#if _DMODE == 0

	const UINT8* dalpha_table = epic12_device_colrtable[d_alpha];
#endif
#endif
#endif



	// Use precisely the same clipping and delay accounting as the
	// original generated path, including flip and source wrap rules.
	if (m_bitmaps16) {
		if(dimx<=startx || dimy<=starty)return;
#if EPIC12_NEON
		const bool neon=epic12_blend_neon;
#else
		const bool neon=false;
#endif
		for (y=starty;y<dimy;y++) {
			UINT16 *d=m_bitmaps16+(dst_y_start+y)*0x2000+dst_x_start+startx;
			const UINT16 *s=m_bitmaps16+((src_y+yf*y)&0x0fff)*0x2000+
				(FLIPX?src_x-startx:src_x+startx);
#if BLENDED == 1
			Epic12Packed::Row<FLIPX,TRANSPARENT>(d,s,dimx>startx?dimx-startx:0,
				_SMODE,_DMODE,s_alpha,d_alpha,tint_clr->r,tint_clr->g,tint_clr->b,TINT,true,neon);
#else
			Epic12Packed::Row<FLIPX,TRANSPARENT>(d,s,dimx>startx?dimx-startx:0,
				0,0,s_alpha,d_alpha,tint_clr->r,tint_clr->g,tint_clr->b,
#if REALLY_SIMPLE == 1
				false,
#else
				TINT,
#endif
				false,neon);
#endif
		}
		return;
	}

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
#if REALLY_SIMPLE == 1
		Epic12Fast::CopyRow<FLIPX>(bmp, gfx2, dimx-startx > 0 ? dimx-startx : 0, TRANSPARENT != 0);
#else
#if EPIC12_NEON && REALLY_SIMPLE == 0
		if (epic12_blend_neon && dimx-startx >= 4) {
#if BLENDED == 1
			const size_t n = Epic12Fast::BlendRow<_SMODE,_DMODE,TINT,true,FLIPX,TRANSPARENT>(
#else
			const size_t n = Epic12Fast::BlendRow<0,0,TINT,false,FLIPX,TRANSPARENT>(
#endif
				bmp,gfx2,dimx-startx,s_alpha,d_alpha,tint_clr->r,tint_clr->g,tint_clr->b,epic12_device_colrtable);
			bmp+=n;
#if FLIPX == 1
			gfx2-=n;
#else
			gfx2+=n;
#endif
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
