/* ported from mame 0.155 */
/*
Copyright (c) 1997-2015 Nicola Salmoria and the MAME team
Redistributions may not be sold, nor may they be used in a commercial product or activity.
*/
/* emulation of Altera Cyclone EPIC12 FPGA programmed as a blitter */
/* code by David Haywood, Luca Elia, MetalliC */

#include "burnint.h"
#include "tiles_generic.h"
#include "sh4_intf.h"
#include "thready.h"
#include "rectangle.h"
#include "epic12_fast.h"
#include "epic12_packed.h"
#include <chrono>
#include <atomic>
#include <math.h> // floor()

static const int EP1C_VRAM_CLK_NANOSEC = 13;
static const int EP1C_SRAM_CLK_NANOSEC = 20;
static const int EP1C_VRAM_H_LINE_PERIOD_NANOSEC = 63600;
static const int EP1C_VRAM_H_LINE_DURATION_NANOSEC = 2160;
static const int EP1C_FRAME_DURATION_NANOSEC = 16666666;
static const int EP1C_DRAW_OPERATION_SIZE_BYTES = 20;
static const int EP1C_CLIP_OPERATION_SIZE_BYTES = 2;

// When looking at VRAM viewer in Special mode in Muchi Muchi Pork, draws 32 pixels outside of
// the "clip area" is visible. This is likely why the frame buffers will have at least a 32 pixel offset
// from the VRAM borders or other buffers in all games.
static int EP1C_CLIP_MARGIN = 32;
// note: configurable via dipsetting  -dink may 2023

// Number of bytes that are read each time Blitter fetches operations from SRAM.
static const int OPERATION_CHUNK_SIZE_BYTES = 64;

// Approximate time it takes to fetch a chunk of operations.
// This is composed of the time that the Blitter holds the Bus Request (BREQ) signal
// of the SH-3, as well as the overhead between requests.
static const int OPERATION_READ_CHUNK_INTERVAL_NS = 700;

static UINT64 m_blit_delay_ns = 0;
static UINT16 m_blit_idle_op_bytes = 0;

static void idle_blitter(UINT8 operation_size_bytes)
{
	m_blit_idle_op_bytes += operation_size_bytes;
	if (m_blit_idle_op_bytes >= OPERATION_CHUNK_SIZE_BYTES)
	{
		m_blit_idle_op_bytes -= OPERATION_CHUNK_SIZE_BYTES;
		m_blit_delay_ns += OPERATION_READ_CHUNK_INTERVAL_NS;
	}
}

static UINT16* m_ram16;
static UINT16* m_ram16_copy;
static UINT32 m_gfx_addr;
static UINT32 m_gfx_addr_shadowcopy;
static UINT32 m_gfx_scroll_x, m_gfx_scroll_y;
static UINT32 m_gfx_clip_x, m_gfx_clip_y;
static UINT32 m_gfx_clip_x_shadowcopy, m_gfx_clip_y_shadowcopy;
static int m_clip_margin_shadowcopy;

static int m_gfx_size;
static UINT32 *m_bitmaps;
static UINT16 *m_bitmaps16; // optional native RGB555 + flag storage
static rectangle m_clip;

static UINT64 epic12_device_blit_delay;
static int m_blitter_busy;

static int m_main_ramsize; // type D has double the main ram
static int m_main_rammask;

static int m_delay_method;
static int m_delay_scale;
static int m_burn_cycles;

static INT32 sleep_on_busy = 1;

static UINT8 epic12_device_colrtable[0x20][0x40];
static UINT8 epic12_device_colrtable_rev[0x20][0x40];
static UINT8 epic12_device_colrtable_add[0x20][0x20];

#if EPIC12_NEON
static bool epic12_blend_neon = true;
#endif
static UINT16 *pal16 = NULL; // palette lut for 16bpp video emulation

#include "epic12.h"

struct _clr_t
{
	UINT8 b,g,r,t;
};

typedef struct _clr_t clr_t;

union colour_t
{
	clr_t trgb;
	UINT32 u32;
};

typedef const void (*epic12_device_blitfunction)(
						const rectangle *,
						UINT32 *, /* gfx */
						int , /* src_x */
						int , /* src_y */
						const int , /* dst_x_start */
						const int , /* dst_y_start */
						int , /* dimx */
						int , /* dimy */
						const int , /* flipy */
						const UINT8 , /* s_alpha */
						const UINT8 , /* d_alpha */
						//int , /* tint */
						const clr_t *, UINT64 * );

#define BLIT_PARAMS const rectangle *clip, UINT32 *gfx, int src_x, int src_y, const int dst_x_start, const int dst_y_start, int dimx, int dimy, const int flipy, const UINT8 s_alpha, const UINT8 d_alpha, const clr_t *tint_clr, UINT64 *delay_pixels

	static inline void pen_to_clr(UINT32 pen, clr_t *clr)
	{
	// --t- ---- rrrr r--- gggg g--- bbbb b---  format
		clr->r = (pen >> (16+3));// & 0x1f;
		clr->g = (pen >>  (8+3));// & 0x1f;
		clr->b = (pen >>   3);// & 0x1f;

	// --t- ---- ---r rrrr ---g gggg ---b bbbb  format
	//  clr->r = (pen >> 16) & 0x1f;
	//  clr->g = (pen >> 8) & 0x1f;
	//  clr->b = (pen >> 0) & 0x1f;

	}


	// convert separate r,g,b biases (0..80..ff) to clr_t (-1f..0..1f)
	static inline void tint_to_clr(UINT8 r, UINT8 g, UINT8 b, clr_t *clr)
	{
		clr->r  =   r>>2;
		clr->g  =   g>>2;
		clr->b  =   b>>2;
	}

	// clr_t to r5g5b5
	static inline UINT32 clr_to_pen(const clr_t *clr)
	{
	// --t- ---- rrrr r--- gggg g--- bbbb b---  format
		return (clr->r << (16+3)) | (clr->g << (8+3)) | (clr->b << 3);

	// --t- ---- ---r rrrr ---g gggg ---b bbbb  format
	//  return (clr->r << (16)) | (clr->g << (8)) | (clr->b);
	}


	static inline void clr_add_with_clr_mul_fixed(clr_t *clr, const clr_t *clr0, const UINT8 mulfixed_val, const clr_t *mulfixed_clr0)
	{
		clr->r = epic12_device_colrtable_add[clr0->r][epic12_device_colrtable[(mulfixed_clr0->r)][mulfixed_val]];
		clr->g = epic12_device_colrtable_add[clr0->g][epic12_device_colrtable[(mulfixed_clr0->g)][mulfixed_val]];
		clr->b = epic12_device_colrtable_add[clr0->b][epic12_device_colrtable[(mulfixed_clr0->b)][mulfixed_val]];
	}

	static inline  void clr_add_with_clr_mul_3param(clr_t *clr, const clr_t *clr0, const clr_t *clr1, const clr_t *clr2)
	{
		clr->r = epic12_device_colrtable_add[clr0->r][epic12_device_colrtable[(clr2->r)][(clr1->r)]];
		clr->g = epic12_device_colrtable_add[clr0->g][epic12_device_colrtable[(clr2->g)][(clr1->g)]];
		clr->b = epic12_device_colrtable_add[clr0->b][epic12_device_colrtable[(clr2->b)][(clr1->b)]];
	}

	static inline  void clr_add_with_clr_square(clr_t *clr, const clr_t *clr0, const clr_t *clr1)
	{
		clr->r = epic12_device_colrtable_add[clr0->r][epic12_device_colrtable[(clr1->r)][(clr1->r)]];
		clr->g = epic12_device_colrtable_add[clr0->r][epic12_device_colrtable[(clr1->g)][(clr1->g)]];
		clr->b = epic12_device_colrtable_add[clr0->r][epic12_device_colrtable[(clr1->b)][(clr1->b)]];
	}

	static inline  void clr_add_with_clr_mul_fixed_rev(clr_t *clr, const clr_t *clr0, const UINT8 val, const clr_t *clr1)
	{
		clr->r =  epic12_device_colrtable_add[clr0->r][epic12_device_colrtable_rev[val][(clr1->r)]];
		clr->g =  epic12_device_colrtable_add[clr0->g][epic12_device_colrtable_rev[val][(clr1->g)]];
		clr->b =  epic12_device_colrtable_add[clr0->b][epic12_device_colrtable_rev[val][(clr1->b)]];
	}

	static inline  void clr_add_with_clr_mul_rev_3param(clr_t *clr, const clr_t *clr0, const clr_t *clr1, const clr_t *clr2)
	{
		clr->r =  epic12_device_colrtable_add[clr0->r][epic12_device_colrtable_rev[(clr2->r)][(clr1->r)]];
		clr->g =  epic12_device_colrtable_add[clr0->g][epic12_device_colrtable_rev[(clr2->g)][(clr1->g)]];
		clr->b =  epic12_device_colrtable_add[clr0->b][epic12_device_colrtable_rev[(clr2->b)][(clr1->b)]];
	}

	static inline  void clr_add_with_clr_mul_rev_square(clr_t *clr, const clr_t *clr0, const clr_t *clr1)
	{
		clr->r =  epic12_device_colrtable_add[clr0->r][epic12_device_colrtable_rev[(clr1->r)][(clr1->r)]];
		clr->g =  epic12_device_colrtable_add[clr0->g][epic12_device_colrtable_rev[(clr1->g)][(clr1->g)]];
		clr->b =  epic12_device_colrtable_add[clr0->b][epic12_device_colrtable_rev[(clr1->b)][(clr1->b)]];
	}


	static inline  void clr_add(clr_t *clr, const clr_t *clr0, const clr_t *clr1)
	{
	/*
	    clr->r = clr0->r + clr1->r;
	    clr->g = clr0->g + clr1->g;
	    clr->b = clr0->b + clr1->b;
	*/
		// use pre-clamped lookup table
		clr->r =  epic12_device_colrtable_add[clr0->r][clr1->r];
		clr->g =  epic12_device_colrtable_add[clr0->g][clr1->g];
		clr->b =  epic12_device_colrtable_add[clr0->b][clr1->b];

	}


	static inline void clr_mul(clr_t *clr0, const clr_t *clr1)
	{
		clr0->r = epic12_device_colrtable[(clr0->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable[(clr0->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable[(clr0->b)][(clr1->b)];
	}

	static inline void clr_square(clr_t *clr0, const clr_t *clr1)
	{
		clr0->r = epic12_device_colrtable[(clr1->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable[(clr1->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable[(clr1->b)][(clr1->b)];
	}

	static inline void clr_mul_3param(clr_t *clr0, const clr_t *clr1, const clr_t *clr2)
	{
		clr0->r = epic12_device_colrtable[(clr2->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable[(clr2->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable[(clr2->b)][(clr1->b)];
	}

	static inline void clr_mul_rev(clr_t *clr0, const clr_t *clr1)
	{
		clr0->r = epic12_device_colrtable_rev[(clr0->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable_rev[(clr0->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable_rev[(clr0->b)][(clr1->b)];
	}

	static inline void clr_mul_rev_square(clr_t *clr0, const clr_t *clr1)
	{
		clr0->r = epic12_device_colrtable_rev[(clr1->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable_rev[(clr1->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable_rev[(clr1->b)][(clr1->b)];
	}


	static inline void clr_mul_rev_3param(clr_t *clr0, const clr_t *clr1, const clr_t *clr2)
	{
		clr0->r = epic12_device_colrtable_rev[(clr2->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable_rev[(clr2->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable_rev[(clr2->b)][(clr1->b)];
	}

	static inline void clr_mul_fixed(clr_t *clr, const UINT8 val, const clr_t *clr0)
	{
		clr->r = epic12_device_colrtable[val][(clr0->r)];
		clr->g = epic12_device_colrtable[val][(clr0->g)];
		clr->b = epic12_device_colrtable[val][(clr0->b)];
	}

	static inline void clr_mul_fixed_rev(clr_t *clr, const UINT8 val, const clr_t *clr0)
	{
		clr->r = epic12_device_colrtable_rev[val][(clr0->r)];
		clr->g = epic12_device_colrtable_rev[val][(clr0->g)];
		clr->b = epic12_device_colrtable_rev[val][(clr0->b)];
	}

	static inline void clr_copy(clr_t *clr, const clr_t *clr0)
	{
		clr->r = clr0->r;
		clr->g = clr0->g;
		clr->b = clr0->b;
	}



	// (1|s|d) * s_factor * s + (1|s|d) * d_factor * d
	// 0: +alpha
	// 1: +source
	// 2: +dest
	// 3: *
	// 4: -alpha
	// 5: -source
	// 6: -dest
	// 7: *

#include "epic12_blit0.inc"
#include "epic12_blit1.inc"
#include "epic12_blit2.inc"
#include "epic12_blit3.inc"
#include "epic12_blit4.inc"
#include "epic12_blit5.inc"
#include "epic12_blit6.inc"
#include "epic12_blit7.inc"
#include "epic12_blit8.inc"

static UINT8 *dips; // pointer to cv1k's dips

static void blitter_delay_callback(int)
{
	m_blitter_busy = 0;
	//bprintf(0, _T("cyc @ blitdelay callback: %d\n"), Sh3TotalCycles());
}

// Profile only at whole-list boundaries. Worker wall time overlaps CPU
// time and must not be added to frame total. Counters are monotonic,
// with completion of a job attributed to the interval where it finishes.
static bool blitProfile = false;
static UINT64 blitSubmitWaitNs, blitShadowNs, blitSyncNs, blitJobs, blitAsyncJobs;
static std::atomic<UINT64> blitWorkerNs{0}, blitParallelJobs{0}, blitParallelFallback{0};
static std::atomic<UINT64> blitSplitReject[EPIC12_SPLIT_REASONS];
static std::atomic<UINT64> blitSegmentedJobs{0}, blitSegmentedRanges{0}, blitLaneRanges{0}, blitTileJobs{0}, blitTilePhases{0}, blitTileDraws{0};
static std::atomic<UINT64> blitTileFirstHits{0},blitPlanNs{0},blitTilePlanNs{0};
#if EPIC12_NEON && THREADY == THREADY_PTHREAD
static threadystruct blitHelper;
static bool blitParallelReady, blitParallelAllowed;
static bool blitSegmentsEnabled=true, blitPreciseEnabled=true, blitTilesEnabled=false;
static bool blitTileFirstEnabled=true,blitTilePreferred=false;
static int blitSplitY;
static UINT64 blitHelperDelay;
static void gfx_helper_cb();
#endif
static bool blitJobAsync;
static UINT64 BlitNow()
{
	return (UINT64)std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

void epic12_set_perf_enabled(bool enabled)
{
	// Called only during driver initialization, before submitting work.
	blitProfile=enabled;
	blitSubmitWaitNs=blitShadowNs=blitSyncNs=blitJobs=blitAsyncJobs=0;
	blitWorkerNs.store(0,std::memory_order_relaxed);
	blitParallelJobs.store(0,std::memory_order_relaxed);
	blitParallelFallback.store(0,std::memory_order_relaxed);
	for (auto &n:blitSplitReject) n.store(0,std::memory_order_relaxed);
	blitSegmentedJobs.store(0,std::memory_order_relaxed);
	blitSegmentedRanges.store(0,std::memory_order_relaxed);
	blitLaneRanges.store(0,std::memory_order_relaxed);
	blitTileJobs.store(0,std::memory_order_relaxed);
	blitTilePhases.store(0,std::memory_order_relaxed);
	blitTileDraws.store(0,std::memory_order_relaxed);
	blitTileFirstHits.store(0,std::memory_order_relaxed);
	blitPlanNs.store(0,std::memory_order_relaxed);
	blitTilePlanNs.store(0,std::memory_order_relaxed);
	blitJobAsync=false;
}

void epic12_get_perf_stats(Epic12PerfStats *stats)
{
	*stats={blitSubmitWaitNs,blitShadowNs,blitSyncNs,
		blitWorkerNs.load(std::memory_order_relaxed),blitJobs,blitAsyncJobs,
		blitParallelJobs.load(std::memory_order_relaxed),blitParallelFallback.load(std::memory_order_relaxed),{},
		blitSegmentedJobs.load(std::memory_order_relaxed),blitSegmentedRanges.load(std::memory_order_relaxed),blitLaneRanges.load(std::memory_order_relaxed),
		blitTileJobs.load(std::memory_order_relaxed),blitTilePhases.load(std::memory_order_relaxed),blitTileDraws.load(std::memory_order_relaxed),
		blitTileFirstHits.load(std::memory_order_relaxed),blitPlanNs.load(std::memory_order_relaxed),blitTilePlanNs.load(std::memory_order_relaxed)};
	for (unsigned i=0;i<EPIC12_SPLIT_REASONS;i++) stats->splitReject[i]=blitSplitReject[i].load(std::memory_order_relaxed);
}

static void gfx_exec(); // forward
#if EPIC12_NEON && THREADY == THREADY_PTHREAD
static bool gfx_exec_parallel();
#endif

static void run_blitter_cb()
{
	epic12_device_blit_delay = 0;
	const UINT64 start = blitProfile ? BlitNow() : 0;
#if EPIC12_NEON && THREADY == THREADY_PTHREAD
	if (blitParallelReady && blitParallelAllowed) {
		const bool used=gfx_exec_parallel();
		if (blitProfile) {
			if (used) blitParallelJobs.fetch_add(1,std::memory_order_relaxed);
			else blitParallelFallback.fetch_add(1,std::memory_order_relaxed);
		}
		if (!used) gfx_exec();
	} else
#endif
		gfx_exec();
	if (blitProfile) {
		const UINT64 elapsed = BlitNow()-start;
		if (blitJobAsync) blitWorkerNs.fetch_add(elapsed,std::memory_order_relaxed);
		else blitSyncNs+=elapsed;
	}
}

void epic12_exit()
{
	thready.notify_wait();
	thready.exit();
#if EPIC12_NEON && THREADY == THREADY_PTHREAD
	if (blitParallelReady) blitHelper.exit();
	blitParallelReady=false;
#endif

	BurnFree(m_bitmaps);
	BurnFree(m_bitmaps16);
	BurnFree(m_ram16_copy);

	if (pal16) {
		BurnFree(pal16);
		pal16 = NULL;
	}
}

void epic12_init(INT32 ram_size, UINT16 *ram, UINT8 *dippy)
{
#if EPIC12_NEON
	const char *blend = getenv("FBNEO_CV1K_NEON_BLEND");
	epic12_blend_neon = !blend || strcmp(blend,"0") != 0;
	bprintf(PRINT_UI,_T("CV1000 EPIC12 NEON blend: %s\n"),epic12_blend_neon?_T("enabled"):_T("disabled"));
#endif
	m_main_ramsize = ram_size;
	m_main_rammask = ram_size - 1;

	m_ram16 = ram;

	m_ram16_copy = (UINT16*)BurnMalloc(ram_size);

	dips = dippy;

	m_gfx_size = 0x2000 * 0x1000;
	// Compact VRAM is opt-in until an RK3566 A/B test establishes
	// speed and real-game correctness. Limit to libretro: its state
	// callback consumes a byte stream, not named standalone areas.
#if defined(__LIBRETRO__) && EPIC12_NEON
	const char *packed=getenv("FBNEO_CV1K_VRAM16");
	const char *special=getenv("FBNEO_CV1K_PACKED_FAST");
	Epic12Packed::specialized=!special || strcmp(special,"0")!=0;
	if (packed && !strcmp(packed,"1"))
		m_bitmaps16=(UINT16*)BurnMalloc(m_gfx_size*2);
#endif
	if (!m_bitmaps16) m_bitmaps=(UINT32*)BurnMalloc(m_gfx_size*4);
	bprintf(PRINT_UI,_T("CV1000 EPIC12 VRAM: %s\n"),m_bitmaps16?_T("16-bit packed (experimental)"):_T("32-bit expanded"));
	if(m_bitmaps16)bprintf(PRINT_UI,_T("CV1000 EPIC12 packed specialized rows: %s\n"),Epic12Packed::specialized?_T("enabled"):_T("disabled"));

	m_clip.set(0, 0x2000-1, 0, 0x1000-1);

	m_delay_method = 0; // accurate
	m_delay_scale = 50;
	m_blitter_busy = 0;
	m_gfx_addr = 0;
	m_gfx_addr_shadowcopy = 0;
	m_gfx_scroll_x = 0;
	m_gfx_scroll_y = 0;
	m_gfx_clip_x = 0;
	m_gfx_clip_y = 0;
	m_gfx_clip_x_shadowcopy = 0;
	m_gfx_clip_y_shadowcopy = 0;
	epic12_device_blit_delay = 0;

	m_blit_delay_ns = 0;
	m_blit_idle_op_bytes = 0;

	epic12_set_blitter_clipping_margin(1);
	epic12_set_blitter_sleep_on_busy(1);

	thready.init(run_blitter_cb);
#if EPIC12_NEON && THREADY == THREADY_PTHREAD
	const char *parallel=getenv("FBNEO_CV1K_BLIT_PARALLEL");
	const char *segments=getenv("FBNEO_CV1K_BLIT_SEGMENTS");
	const char *precise=getenv("FBNEO_CV1K_BLIT_PRECISE");
	const char *tiles=getenv("FBNEO_CV1K_BLIT_TILES");
	blitTilesEnabled=tiles && !strcmp(tiles,"1");
	const char *first=getenv("FBNEO_CV1K_TILE_FIRST");
	blitTileFirstEnabled=!first || strcmp(first,"0")!=0;
	blitTilePreferred=false;
	blitPreciseEnabled=!precise || strcmp(precise,"0")!=0;
	blitSegmentsEnabled=!segments || strcmp(segments,"0")!=0;
	blitParallelReady=false;
	blitParallelAllowed=false;
	if (thready_cpu_count()>=4 && (!parallel || strcmp(parallel,"0")!=0)) {
		blitHelper.init(gfx_helper_cb);
		blitParallelReady=blitHelper.is_threaded()!=0;
	}
	bprintf(PRINT_UI,_T("CV1000 EPIC12 safe row parallel: %s\n"),blitParallelReady?_T("enabled"):_T("disabled"));
	bprintf(PRINT_UI,_T("CV1000 EPIC12 segmented parallel: %s\n"),(blitParallelReady && blitSegmentsEnabled)?_T("enabled"):_T("disabled"));
	bprintf(PRINT_UI,_T("CV1000 EPIC12 exact cross-lane hazards: %s\n"),(blitParallelReady && blitPreciseEnabled)?_T("enabled"):_T("disabled"));
	bprintf(PRINT_UI,_T("CV1000 EPIC12 tile command scheduling: %s\n"),(blitParallelReady && blitTilesEnabled)?_T("enabled (experimental)"):_T("disabled"));
	if(blitTilesEnabled)bprintf(PRINT_UI,_T("CV1000 EPIC12 adaptive tile-first planning: %s\n"),blitTileFirstEnabled?_T("enabled"):_T("disabled"));
#endif

	sh4_set_cave_blitter_delay_func(blitter_delay_callback);
}

void epic12_set_blitterthreading(INT32 value)
{
	thready.set_threading(value);
}

void epic12_set_blitterdelay(INT32 delay, INT32 burn_cycles)
{
	m_delay_scale = delay;
	m_burn_cycles = burn_cycles;
}

void epic12_set_blitterdelay_method(INT32 delay_method) // 0 = accurate, !0 = ancient
{
	m_delay_method = delay_method;
}

void epic12_set_blitter_clipping_margin(INT32 c_margin_on) // ??? not sure.
{
	EP1C_CLIP_MARGIN = (c_margin_on) ? 32 : 0;
}

void epic12_set_blitter_sleep_on_busy(INT32 busysleep_on)
{
	sleep_on_busy = (busysleep_on) ? 1 : 0;
}

void epic12_reset()
{
	thready.notify_wait();
	// cache table to avoid divides in blit code, also pre-clamped
	int x,y;
	for (y=0;y<0x40;y++)
	{
		for (x=0;x<0x20;x++)
		{
			epic12_device_colrtable[x][y] = (x*y) / 0x1f;
			if (epic12_device_colrtable[x][y]>0x1f) epic12_device_colrtable[x][y] = 0x1f;

			epic12_device_colrtable_rev[x^0x1f][y] = (x*y) / 0x1f;
			if (epic12_device_colrtable_rev[x^0x1f][y]>0x1f) epic12_device_colrtable_rev[x^0x1f][y] = 0x1f;
		}
	}

	// preclamped add table
	for (y=0;y<0x20;y++)
	{
		for (x=0;x<0x20;x++)
		{
			epic12_device_colrtable_add[x][y] = (x+y);
			if (epic12_device_colrtable_add[x][y]>0x1f) epic12_device_colrtable_add[x][y] = 0x1f;
		}
	}

	m_blitter_busy = 0;
	m_gfx_addr = 0;
	m_gfx_addr_shadowcopy = 0;
	m_gfx_scroll_x = 0;
	m_gfx_scroll_y = 0;
	m_gfx_clip_x = 0;
	m_gfx_clip_y = 0;
	m_gfx_clip_x_shadowcopy = 0;
	m_gfx_clip_y_shadowcopy = 0;
	epic12_device_blit_delay = 0;
	m_blit_delay_ns = 0;
	m_blit_idle_op_bytes = 0;

	thready.reset();
}

static inline UINT16 READ_NEXT_WORD(UINT32 *addr)
{
	const UINT16 data = m_ram16_copy[((*addr & m_main_rammask) >> 1)];

	*addr += 2;

	return data;
}

static inline UINT16 COPY_NEXT_WORD(UINT32 *addr)
{
	const UINT16 data = m_ram16[((*addr & m_main_rammask) >> 1)];
	m_ram16_copy[((*addr & m_main_rammask) >> 1)] = data;

	*addr += 2;

	return data;
}

static void gfx_upload_shadow_copy(UINT32 *addr)
{
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);

	const UINT32 dimx = (COPY_NEXT_WORD(addr) & 0x1fff) + 1;
	const UINT32 dimy = (COPY_NEXT_WORD(addr) & 0x0fff) + 1;

	// Copy contiguous pieces, splitting exactly at the original RAM wrap.
	UINT32 left = dimx * dimy;
	while (left) {
		const UINT32 at = (*addr & m_main_rammask) >> 1;
		const UINT32 available = (m_main_ramsize >> 1) - at;
		const UINT32 n = left < available ? left : available;
		memcpy(m_ram16_copy + at, m_ram16 + at, n * sizeof(UINT16));
		*addr += n * 2;
		left -= n;
	}

	// Time spent on uploads is mostly due to Main RAM accesses.
	// The Blitter will send BREQ requests to the SH-3, to access Main RAM
	// and then write it to VRAM.
	// The number of bytes to read are the sum of a 16b fixed header and the pixel
	// data (2 byte per pixel). RAM accesses are 32bit, so divide by four for clocks.
	//
	// TODO: There's additional overhead to these request thats are not included. The BREQ
	// assertion also puts CPU into WAIT, if it needs uncached RAM accesses.
	int num_sram_clk = (16 + dimx * dimy * 2 ) / 4;
	m_blit_delay_ns += num_sram_clk * EP1C_SRAM_CLK_NANOSEC;
	m_blit_idle_op_bytes = 0;
}

static void gfx_upload(UINT32 *addr)
{
	UINT32 x,y, dst_p,dst_x_start,dst_y_start, dimx,dimy;
	UINT32 *dst;

	// 0x20000000
	READ_NEXT_WORD(addr);
	READ_NEXT_WORD(addr);

	// 0x99999999
	READ_NEXT_WORD(addr);
	READ_NEXT_WORD(addr);

	dst_x_start = READ_NEXT_WORD(addr);
	dst_y_start = READ_NEXT_WORD(addr);

	dst_p = 0;
	dst_x_start &= 0x1fff;
	dst_y_start &= 0x0fff;

	dimx = (READ_NEXT_WORD(addr) & 0x1fff) + 1;
	dimy = (READ_NEXT_WORD(addr) & 0x0fff) + 1;

	//bprintf(0, _T("GFX COPY: DST %02X,%02X,%03X DIM %02X,%03X\n"), dst_p,dst_x_start,dst_y_start, dimx,dimy);

	for (y = 0; y < dimy; y++)
	{
		//dst = &m_bitmaps->pix(dst_y_start + y, 0);
		const size_t offset=size_t(dst_y_start+y)*0x2000+dst_x_start;
		dst=m_bitmaps16?nullptr:m_bitmaps+offset;
		UINT16 *packed=m_bitmaps16?m_bitmaps16+offset:nullptr;

		UINT32 left = dimx;
		while (left) {
			const UINT32 at = (*addr & m_main_rammask) >> 1;
			const UINT32 available = (m_main_ramsize >> 1) - at;
			const UINT32 n = left < available ? left : available;
			if (packed) {memcpy(packed,m_ram16_copy+at,n*sizeof(UINT16));packed+=n;}
			else {Epic12Fast::Upload(dst,m_ram16_copy+at,n);dst+=n;}
			*addr += n * 2;
			left -= n;
		}
	}
}

#define draw_params clip, m_bitmaps,src_x,src_y, x,y, dimx,dimy, flipy, s_alpha, d_alpha, &tint_clr, delay_pixels



static epic12_device_blitfunction epic12_device_f0_ti1_tr1_blit_funcs[] =
{
	draw_sprite_f0_ti1_tr1_s0_d0, draw_sprite_f0_ti1_tr1_s1_d0, draw_sprite_f0_ti1_tr1_s2_d0, draw_sprite_f0_ti1_tr1_s3_d0, draw_sprite_f0_ti1_tr1_s4_d0, draw_sprite_f0_ti1_tr1_s5_d0, draw_sprite_f0_ti1_tr1_s6_d0, draw_sprite_f0_ti1_tr1_s7_d0,
	draw_sprite_f0_ti1_tr1_s0_d1, draw_sprite_f0_ti1_tr1_s1_d1, draw_sprite_f0_ti1_tr1_s2_d1, draw_sprite_f0_ti1_tr1_s3_d1, draw_sprite_f0_ti1_tr1_s4_d1, draw_sprite_f0_ti1_tr1_s5_d1, draw_sprite_f0_ti1_tr1_s6_d1, draw_sprite_f0_ti1_tr1_s7_d1,
	draw_sprite_f0_ti1_tr1_s0_d2, draw_sprite_f0_ti1_tr1_s1_d2, draw_sprite_f0_ti1_tr1_s2_d2, draw_sprite_f0_ti1_tr1_s3_d2, draw_sprite_f0_ti1_tr1_s4_d2, draw_sprite_f0_ti1_tr1_s5_d2, draw_sprite_f0_ti1_tr1_s6_d2, draw_sprite_f0_ti1_tr1_s7_d2,
	draw_sprite_f0_ti1_tr1_s0_d3, draw_sprite_f0_ti1_tr1_s1_d3, draw_sprite_f0_ti1_tr1_s2_d3, draw_sprite_f0_ti1_tr1_s3_d3, draw_sprite_f0_ti1_tr1_s4_d3, draw_sprite_f0_ti1_tr1_s5_d3, draw_sprite_f0_ti1_tr1_s6_d3, draw_sprite_f0_ti1_tr1_s7_d3,
	draw_sprite_f0_ti1_tr1_s0_d4, draw_sprite_f0_ti1_tr1_s1_d4, draw_sprite_f0_ti1_tr1_s2_d4, draw_sprite_f0_ti1_tr1_s3_d4, draw_sprite_f0_ti1_tr1_s4_d4, draw_sprite_f0_ti1_tr1_s5_d4, draw_sprite_f0_ti1_tr1_s6_d4, draw_sprite_f0_ti1_tr1_s7_d4,
	draw_sprite_f0_ti1_tr1_s0_d5, draw_sprite_f0_ti1_tr1_s1_d5, draw_sprite_f0_ti1_tr1_s2_d5, draw_sprite_f0_ti1_tr1_s3_d5, draw_sprite_f0_ti1_tr1_s4_d5, draw_sprite_f0_ti1_tr1_s5_d5, draw_sprite_f0_ti1_tr1_s6_d5, draw_sprite_f0_ti1_tr1_s7_d5,
	draw_sprite_f0_ti1_tr1_s0_d6, draw_sprite_f0_ti1_tr1_s1_d6, draw_sprite_f0_ti1_tr1_s2_d6, draw_sprite_f0_ti1_tr1_s3_d6, draw_sprite_f0_ti1_tr1_s4_d6, draw_sprite_f0_ti1_tr1_s5_d6, draw_sprite_f0_ti1_tr1_s6_d6, draw_sprite_f0_ti1_tr1_s7_d6,
	draw_sprite_f0_ti1_tr1_s0_d7, draw_sprite_f0_ti1_tr1_s1_d7, draw_sprite_f0_ti1_tr1_s2_d7, draw_sprite_f0_ti1_tr1_s3_d7, draw_sprite_f0_ti1_tr1_s4_d7, draw_sprite_f0_ti1_tr1_s5_d7, draw_sprite_f0_ti1_tr1_s6_d7, draw_sprite_f0_ti1_tr1_s7_d7,
};

static epic12_device_blitfunction epic12_device_f0_ti1_tr0_blit_funcs[] =
{
	draw_sprite_f0_ti1_tr0_s0_d0, draw_sprite_f0_ti1_tr0_s1_d0, draw_sprite_f0_ti1_tr0_s2_d0, draw_sprite_f0_ti1_tr0_s3_d0, draw_sprite_f0_ti1_tr0_s4_d0, draw_sprite_f0_ti1_tr0_s5_d0, draw_sprite_f0_ti1_tr0_s6_d0, draw_sprite_f0_ti1_tr0_s7_d0,
	draw_sprite_f0_ti1_tr0_s0_d1, draw_sprite_f0_ti1_tr0_s1_d1, draw_sprite_f0_ti1_tr0_s2_d1, draw_sprite_f0_ti1_tr0_s3_d1, draw_sprite_f0_ti1_tr0_s4_d1, draw_sprite_f0_ti1_tr0_s5_d1, draw_sprite_f0_ti1_tr0_s6_d1, draw_sprite_f0_ti1_tr0_s7_d1,
	draw_sprite_f0_ti1_tr0_s0_d2, draw_sprite_f0_ti1_tr0_s1_d2, draw_sprite_f0_ti1_tr0_s2_d2, draw_sprite_f0_ti1_tr0_s3_d2, draw_sprite_f0_ti1_tr0_s4_d2, draw_sprite_f0_ti1_tr0_s5_d2, draw_sprite_f0_ti1_tr0_s6_d2, draw_sprite_f0_ti1_tr0_s7_d2,
	draw_sprite_f0_ti1_tr0_s0_d3, draw_sprite_f0_ti1_tr0_s1_d3, draw_sprite_f0_ti1_tr0_s2_d3, draw_sprite_f0_ti1_tr0_s3_d3, draw_sprite_f0_ti1_tr0_s4_d3, draw_sprite_f0_ti1_tr0_s5_d3, draw_sprite_f0_ti1_tr0_s6_d3, draw_sprite_f0_ti1_tr0_s7_d3,
	draw_sprite_f0_ti1_tr0_s0_d4, draw_sprite_f0_ti1_tr0_s1_d4, draw_sprite_f0_ti1_tr0_s2_d4, draw_sprite_f0_ti1_tr0_s3_d4, draw_sprite_f0_ti1_tr0_s4_d4, draw_sprite_f0_ti1_tr0_s5_d4, draw_sprite_f0_ti1_tr0_s6_d4, draw_sprite_f0_ti1_tr0_s7_d4,
	draw_sprite_f0_ti1_tr0_s0_d5, draw_sprite_f0_ti1_tr0_s1_d5, draw_sprite_f0_ti1_tr0_s2_d5, draw_sprite_f0_ti1_tr0_s3_d5, draw_sprite_f0_ti1_tr0_s4_d5, draw_sprite_f0_ti1_tr0_s5_d5, draw_sprite_f0_ti1_tr0_s6_d5, draw_sprite_f0_ti1_tr0_s7_d5,
	draw_sprite_f0_ti1_tr0_s0_d6, draw_sprite_f0_ti1_tr0_s1_d6, draw_sprite_f0_ti1_tr0_s2_d6, draw_sprite_f0_ti1_tr0_s3_d6, draw_sprite_f0_ti1_tr0_s4_d6, draw_sprite_f0_ti1_tr0_s5_d6, draw_sprite_f0_ti1_tr0_s6_d6, draw_sprite_f0_ti1_tr0_s7_d6,
	draw_sprite_f0_ti1_tr0_s0_d7, draw_sprite_f0_ti1_tr0_s1_d7, draw_sprite_f0_ti1_tr0_s2_d7, draw_sprite_f0_ti1_tr0_s3_d7, draw_sprite_f0_ti1_tr0_s4_d7, draw_sprite_f0_ti1_tr0_s5_d7, draw_sprite_f0_ti1_tr0_s6_d7, draw_sprite_f0_ti1_tr0_s7_d7,
};

static epic12_device_blitfunction epic12_device_f1_ti1_tr1_blit_funcs[] =
{
	draw_sprite_f1_ti1_tr1_s0_d0, draw_sprite_f1_ti1_tr1_s1_d0, draw_sprite_f1_ti1_tr1_s2_d0, draw_sprite_f1_ti1_tr1_s3_d0, draw_sprite_f1_ti1_tr1_s4_d0, draw_sprite_f1_ti1_tr1_s5_d0, draw_sprite_f1_ti1_tr1_s6_d0, draw_sprite_f1_ti1_tr1_s7_d0,
	draw_sprite_f1_ti1_tr1_s0_d1, draw_sprite_f1_ti1_tr1_s1_d1, draw_sprite_f1_ti1_tr1_s2_d1, draw_sprite_f1_ti1_tr1_s3_d1, draw_sprite_f1_ti1_tr1_s4_d1, draw_sprite_f1_ti1_tr1_s5_d1, draw_sprite_f1_ti1_tr1_s6_d1, draw_sprite_f1_ti1_tr1_s7_d1,
	draw_sprite_f1_ti1_tr1_s0_d2, draw_sprite_f1_ti1_tr1_s1_d2, draw_sprite_f1_ti1_tr1_s2_d2, draw_sprite_f1_ti1_tr1_s3_d2, draw_sprite_f1_ti1_tr1_s4_d2, draw_sprite_f1_ti1_tr1_s5_d2, draw_sprite_f1_ti1_tr1_s6_d2, draw_sprite_f1_ti1_tr1_s7_d2,
	draw_sprite_f1_ti1_tr1_s0_d3, draw_sprite_f1_ti1_tr1_s1_d3, draw_sprite_f1_ti1_tr1_s2_d3, draw_sprite_f1_ti1_tr1_s3_d3, draw_sprite_f1_ti1_tr1_s4_d3, draw_sprite_f1_ti1_tr1_s5_d3, draw_sprite_f1_ti1_tr1_s6_d3, draw_sprite_f1_ti1_tr1_s7_d3,
	draw_sprite_f1_ti1_tr1_s0_d4, draw_sprite_f1_ti1_tr1_s1_d4, draw_sprite_f1_ti1_tr1_s2_d4, draw_sprite_f1_ti1_tr1_s3_d4, draw_sprite_f1_ti1_tr1_s4_d4, draw_sprite_f1_ti1_tr1_s5_d4, draw_sprite_f1_ti1_tr1_s6_d4, draw_sprite_f1_ti1_tr1_s7_d4,
	draw_sprite_f1_ti1_tr1_s0_d5, draw_sprite_f1_ti1_tr1_s1_d5, draw_sprite_f1_ti1_tr1_s2_d5, draw_sprite_f1_ti1_tr1_s3_d5, draw_sprite_f1_ti1_tr1_s4_d5, draw_sprite_f1_ti1_tr1_s5_d5, draw_sprite_f1_ti1_tr1_s6_d5, draw_sprite_f1_ti1_tr1_s7_d5,
	draw_sprite_f1_ti1_tr1_s0_d6, draw_sprite_f1_ti1_tr1_s1_d6, draw_sprite_f1_ti1_tr1_s2_d6, draw_sprite_f1_ti1_tr1_s3_d6, draw_sprite_f1_ti1_tr1_s4_d6, draw_sprite_f1_ti1_tr1_s5_d6, draw_sprite_f1_ti1_tr1_s6_d6, draw_sprite_f1_ti1_tr1_s7_d6,
	draw_sprite_f1_ti1_tr1_s0_d7, draw_sprite_f1_ti1_tr1_s1_d7, draw_sprite_f1_ti1_tr1_s2_d7, draw_sprite_f1_ti1_tr1_s3_d7, draw_sprite_f1_ti1_tr1_s4_d7, draw_sprite_f1_ti1_tr1_s5_d7, draw_sprite_f1_ti1_tr1_s6_d7, draw_sprite_f1_ti1_tr1_s7_d7,
};

static epic12_device_blitfunction epic12_device_f1_ti1_tr0_blit_funcs[] =
{
	draw_sprite_f1_ti1_tr0_s0_d0, draw_sprite_f1_ti1_tr0_s1_d0, draw_sprite_f1_ti1_tr0_s2_d0, draw_sprite_f1_ti1_tr0_s3_d0, draw_sprite_f1_ti1_tr0_s4_d0, draw_sprite_f1_ti1_tr0_s5_d0, draw_sprite_f1_ti1_tr0_s6_d0, draw_sprite_f1_ti1_tr0_s7_d0,
	draw_sprite_f1_ti1_tr0_s0_d1, draw_sprite_f1_ti1_tr0_s1_d1, draw_sprite_f1_ti1_tr0_s2_d1, draw_sprite_f1_ti1_tr0_s3_d1, draw_sprite_f1_ti1_tr0_s4_d1, draw_sprite_f1_ti1_tr0_s5_d1, draw_sprite_f1_ti1_tr0_s6_d1, draw_sprite_f1_ti1_tr0_s7_d1,
	draw_sprite_f1_ti1_tr0_s0_d2, draw_sprite_f1_ti1_tr0_s1_d2, draw_sprite_f1_ti1_tr0_s2_d2, draw_sprite_f1_ti1_tr0_s3_d2, draw_sprite_f1_ti1_tr0_s4_d2, draw_sprite_f1_ti1_tr0_s5_d2, draw_sprite_f1_ti1_tr0_s6_d2, draw_sprite_f1_ti1_tr0_s7_d2,
	draw_sprite_f1_ti1_tr0_s0_d3, draw_sprite_f1_ti1_tr0_s1_d3, draw_sprite_f1_ti1_tr0_s2_d3, draw_sprite_f1_ti1_tr0_s3_d3, draw_sprite_f1_ti1_tr0_s4_d3, draw_sprite_f1_ti1_tr0_s5_d3, draw_sprite_f1_ti1_tr0_s6_d3, draw_sprite_f1_ti1_tr0_s7_d3,
	draw_sprite_f1_ti1_tr0_s0_d4, draw_sprite_f1_ti1_tr0_s1_d4, draw_sprite_f1_ti1_tr0_s2_d4, draw_sprite_f1_ti1_tr0_s3_d4, draw_sprite_f1_ti1_tr0_s4_d4, draw_sprite_f1_ti1_tr0_s5_d4, draw_sprite_f1_ti1_tr0_s6_d4, draw_sprite_f1_ti1_tr0_s7_d4,
	draw_sprite_f1_ti1_tr0_s0_d5, draw_sprite_f1_ti1_tr0_s1_d5, draw_sprite_f1_ti1_tr0_s2_d5, draw_sprite_f1_ti1_tr0_s3_d5, draw_sprite_f1_ti1_tr0_s4_d5, draw_sprite_f1_ti1_tr0_s5_d5, draw_sprite_f1_ti1_tr0_s6_d5, draw_sprite_f1_ti1_tr0_s7_d5,
	draw_sprite_f1_ti1_tr0_s0_d6, draw_sprite_f1_ti1_tr0_s1_d6, draw_sprite_f1_ti1_tr0_s2_d6, draw_sprite_f1_ti1_tr0_s3_d6, draw_sprite_f1_ti1_tr0_s4_d6, draw_sprite_f1_ti1_tr0_s5_d6, draw_sprite_f1_ti1_tr0_s6_d6, draw_sprite_f1_ti1_tr0_s7_d6,
	draw_sprite_f1_ti1_tr0_s0_d7, draw_sprite_f1_ti1_tr0_s1_d7, draw_sprite_f1_ti1_tr0_s2_d7, draw_sprite_f1_ti1_tr0_s3_d7, draw_sprite_f1_ti1_tr0_s4_d7, draw_sprite_f1_ti1_tr0_s5_d7, draw_sprite_f1_ti1_tr0_s6_d7, draw_sprite_f1_ti1_tr0_s7_d7,
};



static epic12_device_blitfunction epic12_device_f0_ti0_tr1_blit_funcs[] =
{
	draw_sprite_f0_ti0_tr1_s0_d0, draw_sprite_f0_ti0_tr1_s1_d0, draw_sprite_f0_ti0_tr1_s2_d0, draw_sprite_f0_ti0_tr1_s3_d0, draw_sprite_f0_ti0_tr1_s4_d0, draw_sprite_f0_ti0_tr1_s5_d0, draw_sprite_f0_ti0_tr1_s6_d0, draw_sprite_f0_ti0_tr1_s7_d0,
	draw_sprite_f0_ti0_tr1_s0_d1, draw_sprite_f0_ti0_tr1_s1_d1, draw_sprite_f0_ti0_tr1_s2_d1, draw_sprite_f0_ti0_tr1_s3_d1, draw_sprite_f0_ti0_tr1_s4_d1, draw_sprite_f0_ti0_tr1_s5_d1, draw_sprite_f0_ti0_tr1_s6_d1, draw_sprite_f0_ti0_tr1_s7_d1,
	draw_sprite_f0_ti0_tr1_s0_d2, draw_sprite_f0_ti0_tr1_s1_d2, draw_sprite_f0_ti0_tr1_s2_d2, draw_sprite_f0_ti0_tr1_s3_d2, draw_sprite_f0_ti0_tr1_s4_d2, draw_sprite_f0_ti0_tr1_s5_d2, draw_sprite_f0_ti0_tr1_s6_d2, draw_sprite_f0_ti0_tr1_s7_d2,
	draw_sprite_f0_ti0_tr1_s0_d3, draw_sprite_f0_ti0_tr1_s1_d3, draw_sprite_f0_ti0_tr1_s2_d3, draw_sprite_f0_ti0_tr1_s3_d3, draw_sprite_f0_ti0_tr1_s4_d3, draw_sprite_f0_ti0_tr1_s5_d3, draw_sprite_f0_ti0_tr1_s6_d3, draw_sprite_f0_ti0_tr1_s7_d3,
	draw_sprite_f0_ti0_tr1_s0_d4, draw_sprite_f0_ti0_tr1_s1_d4, draw_sprite_f0_ti0_tr1_s2_d4, draw_sprite_f0_ti0_tr1_s3_d4, draw_sprite_f0_ti0_tr1_s4_d4, draw_sprite_f0_ti0_tr1_s5_d4, draw_sprite_f0_ti0_tr1_s6_d4, draw_sprite_f0_ti0_tr1_s7_d4,
	draw_sprite_f0_ti0_tr1_s0_d5, draw_sprite_f0_ti0_tr1_s1_d5, draw_sprite_f0_ti0_tr1_s2_d5, draw_sprite_f0_ti0_tr1_s3_d5, draw_sprite_f0_ti0_tr1_s4_d5, draw_sprite_f0_ti0_tr1_s5_d5, draw_sprite_f0_ti0_tr1_s6_d5, draw_sprite_f0_ti0_tr1_s7_d5,
	draw_sprite_f0_ti0_tr1_s0_d6, draw_sprite_f0_ti0_tr1_s1_d6, draw_sprite_f0_ti0_tr1_s2_d6, draw_sprite_f0_ti0_tr1_s3_d6, draw_sprite_f0_ti0_tr1_s4_d6, draw_sprite_f0_ti0_tr1_s5_d6, draw_sprite_f0_ti0_tr1_s6_d6, draw_sprite_f0_ti0_tr1_s7_d6,
	draw_sprite_f0_ti0_tr1_s0_d7, draw_sprite_f0_ti0_tr1_s1_d7, draw_sprite_f0_ti0_tr1_s2_d7, draw_sprite_f0_ti0_tr1_s3_d7, draw_sprite_f0_ti0_tr1_s4_d7, draw_sprite_f0_ti0_tr1_s5_d7, draw_sprite_f0_ti0_tr1_s6_d7, draw_sprite_f0_ti0_tr1_s7_d7,
};

static epic12_device_blitfunction epic12_device_f0_ti0_tr0_blit_funcs[] =
{
	draw_sprite_f0_ti0_tr0_s0_d0, draw_sprite_f0_ti0_tr0_s1_d0, draw_sprite_f0_ti0_tr0_s2_d0, draw_sprite_f0_ti0_tr0_s3_d0, draw_sprite_f0_ti0_tr0_s4_d0, draw_sprite_f0_ti0_tr0_s5_d0, draw_sprite_f0_ti0_tr0_s6_d0, draw_sprite_f0_ti0_tr0_s7_d0,
	draw_sprite_f0_ti0_tr0_s0_d1, draw_sprite_f0_ti0_tr0_s1_d1, draw_sprite_f0_ti0_tr0_s2_d1, draw_sprite_f0_ti0_tr0_s3_d1, draw_sprite_f0_ti0_tr0_s4_d1, draw_sprite_f0_ti0_tr0_s5_d1, draw_sprite_f0_ti0_tr0_s6_d1, draw_sprite_f0_ti0_tr0_s7_d1,
	draw_sprite_f0_ti0_tr0_s0_d2, draw_sprite_f0_ti0_tr0_s1_d2, draw_sprite_f0_ti0_tr0_s2_d2, draw_sprite_f0_ti0_tr0_s3_d2, draw_sprite_f0_ti0_tr0_s4_d2, draw_sprite_f0_ti0_tr0_s5_d2, draw_sprite_f0_ti0_tr0_s6_d2, draw_sprite_f0_ti0_tr0_s7_d2,
	draw_sprite_f0_ti0_tr0_s0_d3, draw_sprite_f0_ti0_tr0_s1_d3, draw_sprite_f0_ti0_tr0_s2_d3, draw_sprite_f0_ti0_tr0_s3_d3, draw_sprite_f0_ti0_tr0_s4_d3, draw_sprite_f0_ti0_tr0_s5_d3, draw_sprite_f0_ti0_tr0_s6_d3, draw_sprite_f0_ti0_tr0_s7_d3,
	draw_sprite_f0_ti0_tr0_s0_d4, draw_sprite_f0_ti0_tr0_s1_d4, draw_sprite_f0_ti0_tr0_s2_d4, draw_sprite_f0_ti0_tr0_s3_d4, draw_sprite_f0_ti0_tr0_s4_d4, draw_sprite_f0_ti0_tr0_s5_d4, draw_sprite_f0_ti0_tr0_s6_d4, draw_sprite_f0_ti0_tr0_s7_d4,
	draw_sprite_f0_ti0_tr0_s0_d5, draw_sprite_f0_ti0_tr0_s1_d5, draw_sprite_f0_ti0_tr0_s2_d5, draw_sprite_f0_ti0_tr0_s3_d5, draw_sprite_f0_ti0_tr0_s4_d5, draw_sprite_f0_ti0_tr0_s5_d5, draw_sprite_f0_ti0_tr0_s6_d5, draw_sprite_f0_ti0_tr0_s7_d5,
	draw_sprite_f0_ti0_tr0_s0_d6, draw_sprite_f0_ti0_tr0_s1_d6, draw_sprite_f0_ti0_tr0_s2_d6, draw_sprite_f0_ti0_tr0_s3_d6, draw_sprite_f0_ti0_tr0_s4_d6, draw_sprite_f0_ti0_tr0_s5_d6, draw_sprite_f0_ti0_tr0_s6_d6, draw_sprite_f0_ti0_tr0_s7_d6,
	draw_sprite_f0_ti0_tr0_s0_d7, draw_sprite_f0_ti0_tr0_s1_d7, draw_sprite_f0_ti0_tr0_s2_d7, draw_sprite_f0_ti0_tr0_s3_d7, draw_sprite_f0_ti0_tr0_s4_d7, draw_sprite_f0_ti0_tr0_s5_d7, draw_sprite_f0_ti0_tr0_s6_d7, draw_sprite_f0_ti0_tr0_s7_d7,
};

static epic12_device_blitfunction epic12_device_f1_ti0_tr1_blit_funcs[] =
{
	draw_sprite_f1_ti0_tr1_s0_d0, draw_sprite_f1_ti0_tr1_s1_d0, draw_sprite_f1_ti0_tr1_s2_d0, draw_sprite_f1_ti0_tr1_s3_d0, draw_sprite_f1_ti0_tr1_s4_d0, draw_sprite_f1_ti0_tr1_s5_d0, draw_sprite_f1_ti0_tr1_s6_d0, draw_sprite_f1_ti0_tr1_s7_d0,
	draw_sprite_f1_ti0_tr1_s0_d1, draw_sprite_f1_ti0_tr1_s1_d1, draw_sprite_f1_ti0_tr1_s2_d1, draw_sprite_f1_ti0_tr1_s3_d1, draw_sprite_f1_ti0_tr1_s4_d1, draw_sprite_f1_ti0_tr1_s5_d1, draw_sprite_f1_ti0_tr1_s6_d1, draw_sprite_f1_ti0_tr1_s7_d1,
	draw_sprite_f1_ti0_tr1_s0_d2, draw_sprite_f1_ti0_tr1_s1_d2, draw_sprite_f1_ti0_tr1_s2_d2, draw_sprite_f1_ti0_tr1_s3_d2, draw_sprite_f1_ti0_tr1_s4_d2, draw_sprite_f1_ti0_tr1_s5_d2, draw_sprite_f1_ti0_tr1_s6_d2, draw_sprite_f1_ti0_tr1_s7_d2,
	draw_sprite_f1_ti0_tr1_s0_d3, draw_sprite_f1_ti0_tr1_s1_d3, draw_sprite_f1_ti0_tr1_s2_d3, draw_sprite_f1_ti0_tr1_s3_d3, draw_sprite_f1_ti0_tr1_s4_d3, draw_sprite_f1_ti0_tr1_s5_d3, draw_sprite_f1_ti0_tr1_s6_d3, draw_sprite_f1_ti0_tr1_s7_d3,
	draw_sprite_f1_ti0_tr1_s0_d4, draw_sprite_f1_ti0_tr1_s1_d4, draw_sprite_f1_ti0_tr1_s2_d4, draw_sprite_f1_ti0_tr1_s3_d4, draw_sprite_f1_ti0_tr1_s4_d4, draw_sprite_f1_ti0_tr1_s5_d4, draw_sprite_f1_ti0_tr1_s6_d4, draw_sprite_f1_ti0_tr1_s7_d4,
	draw_sprite_f1_ti0_tr1_s0_d5, draw_sprite_f1_ti0_tr1_s1_d5, draw_sprite_f1_ti0_tr1_s2_d5, draw_sprite_f1_ti0_tr1_s3_d5, draw_sprite_f1_ti0_tr1_s4_d5, draw_sprite_f1_ti0_tr1_s5_d5, draw_sprite_f1_ti0_tr1_s6_d5, draw_sprite_f1_ti0_tr1_s7_d5,
	draw_sprite_f1_ti0_tr1_s0_d6, draw_sprite_f1_ti0_tr1_s1_d6, draw_sprite_f1_ti0_tr1_s2_d6, draw_sprite_f1_ti0_tr1_s3_d6, draw_sprite_f1_ti0_tr1_s4_d6, draw_sprite_f1_ti0_tr1_s5_d6, draw_sprite_f1_ti0_tr1_s6_d6, draw_sprite_f1_ti0_tr1_s7_d6,
	draw_sprite_f1_ti0_tr1_s0_d7, draw_sprite_f1_ti0_tr1_s1_d7, draw_sprite_f1_ti0_tr1_s2_d7, draw_sprite_f1_ti0_tr1_s3_d7, draw_sprite_f1_ti0_tr1_s4_d7, draw_sprite_f1_ti0_tr1_s5_d7, draw_sprite_f1_ti0_tr1_s6_d7, draw_sprite_f1_ti0_tr1_s7_d7,
};

static epic12_device_blitfunction epic12_device_f1_ti0_tr0_blit_funcs[] =
{
	draw_sprite_f1_ti0_tr0_s0_d0, draw_sprite_f1_ti0_tr0_s1_d0, draw_sprite_f1_ti0_tr0_s2_d0, draw_sprite_f1_ti0_tr0_s3_d0, draw_sprite_f1_ti0_tr0_s4_d0, draw_sprite_f1_ti0_tr0_s5_d0, draw_sprite_f1_ti0_tr0_s6_d0, draw_sprite_f1_ti0_tr0_s7_d0,
	draw_sprite_f1_ti0_tr0_s0_d1, draw_sprite_f1_ti0_tr0_s1_d1, draw_sprite_f1_ti0_tr0_s2_d1, draw_sprite_f1_ti0_tr0_s3_d1, draw_sprite_f1_ti0_tr0_s4_d1, draw_sprite_f1_ti0_tr0_s5_d1, draw_sprite_f1_ti0_tr0_s6_d1, draw_sprite_f1_ti0_tr0_s7_d1,
	draw_sprite_f1_ti0_tr0_s0_d2, draw_sprite_f1_ti0_tr0_s1_d2, draw_sprite_f1_ti0_tr0_s2_d2, draw_sprite_f1_ti0_tr0_s3_d2, draw_sprite_f1_ti0_tr0_s4_d2, draw_sprite_f1_ti0_tr0_s5_d2, draw_sprite_f1_ti0_tr0_s6_d2, draw_sprite_f1_ti0_tr0_s7_d2,
	draw_sprite_f1_ti0_tr0_s0_d3, draw_sprite_f1_ti0_tr0_s1_d3, draw_sprite_f1_ti0_tr0_s2_d3, draw_sprite_f1_ti0_tr0_s3_d3, draw_sprite_f1_ti0_tr0_s4_d3, draw_sprite_f1_ti0_tr0_s5_d3, draw_sprite_f1_ti0_tr0_s6_d3, draw_sprite_f1_ti0_tr0_s7_d3,
	draw_sprite_f1_ti0_tr0_s0_d4, draw_sprite_f1_ti0_tr0_s1_d4, draw_sprite_f1_ti0_tr0_s2_d4, draw_sprite_f1_ti0_tr0_s3_d4, draw_sprite_f1_ti0_tr0_s4_d4, draw_sprite_f1_ti0_tr0_s5_d4, draw_sprite_f1_ti0_tr0_s6_d4, draw_sprite_f1_ti0_tr0_s7_d4,
	draw_sprite_f1_ti0_tr0_s0_d5, draw_sprite_f1_ti0_tr0_s1_d5, draw_sprite_f1_ti0_tr0_s2_d5, draw_sprite_f1_ti0_tr0_s3_d5, draw_sprite_f1_ti0_tr0_s4_d5, draw_sprite_f1_ti0_tr0_s5_d5, draw_sprite_f1_ti0_tr0_s6_d5, draw_sprite_f1_ti0_tr0_s7_d5,
	draw_sprite_f1_ti0_tr0_s0_d6, draw_sprite_f1_ti0_tr0_s1_d6, draw_sprite_f1_ti0_tr0_s2_d6, draw_sprite_f1_ti0_tr0_s3_d6, draw_sprite_f1_ti0_tr0_s4_d6, draw_sprite_f1_ti0_tr0_s5_d6, draw_sprite_f1_ti0_tr0_s6_d6, draw_sprite_f1_ti0_tr0_s7_d6,
	draw_sprite_f1_ti0_tr0_s0_d7, draw_sprite_f1_ti0_tr0_s1_d7, draw_sprite_f1_ti0_tr0_s2_d7, draw_sprite_f1_ti0_tr0_s3_d7, draw_sprite_f1_ti0_tr0_s4_d7, draw_sprite_f1_ti0_tr0_s5_d7, draw_sprite_f1_ti0_tr0_s6_d7, draw_sprite_f1_ti0_tr0_s7_d7,
};

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/*
    Calculate number of VRAM row accesses a draw will perform.
    Source data will typically be aligned well with VRAM, but this is not the case for the destination.
    As an example, drawing a 64x32 pixel image will usually read from two VRAM rows for source data,
    but if the destination start coordinate is (x=10, y=10), each of the 32x32px chunks of source data will
    touch 4 rows of destination VRAM, leading to a total of 8 destination VRAM accesses.
*/
inline UINT16 calculate_vram_accesses(UINT16 start_x, UINT16 start_y, UINT16 dimx, UINT16 dimy)
{
	int x_rows = 0;
	int num_vram_rows = 0;
	for (int x_pixels = dimx; x_pixels > 0; x_pixels -= 32)
	{
		x_rows++;
		if (((start_x & 31) + MIN(32, x_pixels)) > 32)
			x_rows++;  // Drawing across multiple horizontal VRAM row boundaries.
	}
	for (int y_pixels = dimy; y_pixels > 0; y_pixels -= 32)
	{
		num_vram_rows += x_rows;
		if (((start_y & 31) + MIN(32, y_pixels)) > 32)
			num_vram_rows += x_rows;  // Drawing across multiple vertical VRAM row boundaries.	
	}
	return num_vram_rows;
}

static void gfx_draw_shadow_copy(UINT32 *addr)
{
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);
	UINT16 src_x_start = COPY_NEXT_WORD(addr);
	UINT16 src_y_start = COPY_NEXT_WORD(addr);
	UINT16 dst_x_start = COPY_NEXT_WORD(addr);
	UINT16 dst_y_start = COPY_NEXT_WORD(addr);
	UINT16 src_dimx = (COPY_NEXT_WORD(addr) & 0x1fff) + 1;
	UINT16 src_dimy = (COPY_NEXT_WORD(addr) & 0x0fff) + 1;
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);

	// Calculate Blitter delay for the Draw operation.
	// On real hardware, the Blitter will read operations into a FIFO queue
	// by asserting BREQ on the SH3 and then reading from Main RAM.
	// Since the reads are done concurrently to executions of operations, its
	// ok to estimate the delay all at once instead for emulation purposes.
	
	UINT16 dst_x_end = dst_x_start + src_dimx - 1;
	UINT16 dst_y_end = dst_y_start + src_dimy - 1;

	// Sprites fully outside of clipping area should not be drawn.
	if (dst_x_start > m_clip.max_x || dst_x_end < m_clip.min_x || dst_y_start > m_clip.max_y || dst_y_end < m_clip.min_y)
	{
		idle_blitter(EP1C_DRAW_OPERATION_SIZE_BYTES);
		return;
	}

	// Clip the blitter operations, to have the calculations only respect the area being written.
	// It's not 100% clear this is how this is performed, but it is clear that there should be some amount of clipping
	// applied here to match the hardware. This way seems most likely, and maps well to the delays seen on hardware.
	// One example of this being utilized heavily is the transparent fog in Mushihimesama Futari Stage 1. This is drawn as
	// 256x256 sprites, with large parts clipped away.

	dst_x_start = MAX(dst_x_start, (UINT16)m_clip.min_x);
	dst_y_start = MAX(dst_y_start, (UINT16)m_clip.min_y);
	dst_x_end = MIN(dst_x_end, (UINT16)m_clip.max_x);
	dst_y_end = MIN(dst_y_end, (UINT16)m_clip.max_y);
	src_dimx = dst_x_end - dst_x_start + 1;
	src_dimy = dst_y_end - dst_y_start + 1;

	m_blit_idle_op_bytes = 0;  // Blitter no longer idle.

	// VRAM data is laid out in 32x32 pixel rows. Calculate amount of rows accessed. 
	int src_num_vram_rows = calculate_vram_accesses(src_x_start, src_y_start, src_dimx, src_dimy);
	int dst_num_vram_rows = calculate_vram_accesses(dst_x_start, dst_y_start, src_dimx, src_dimy);

	// Since draws are done 4 pixels at the time, extend the draw area to coordinates aligned for this.
	// Doing this after VRAM calculations simplify things a bit, and these extensions will never make the
	// destination area span additional VRAM rows. 
	dst_x_start -= dst_x_start & 3;
	dst_x_end += (4 - ((dst_x_end + 1) & 3)) & 3;
	UINT16 dst_dimx = dst_x_end - dst_x_start + 1;
	UINT16 dst_dimy = dst_y_end - dst_y_start + 1;

	// Number of VRAM CLK cycles needed to draw a sprite is sum of:
	// - Number of pixels read from source divided by 4 (Each CLK reads 4 pixels, since 32bit DDR).
	// - Number of pixels read from destination divided by 4.
	// - Pixels written to destination divided by 4.
	// - VRAM access overhead:
	//   - 6 CLK of overhead after each read from a source VRAM row.
	//   - 20 CLK of overhead between read and write of each destination VRAM row.
	//   - 11 CLK of overhead after each write to a destination VRAM row.
	// - 12 CLK of additional overhead per sprite at the end of writing.
	// Note: Details are from https://buffis.com/docs/CV1000_Blitter_Research_by_buffi.pdf
	//       There may be mistakes.	
	UINT32 num_vram_clk = src_dimx * src_dimy / 4 + dst_dimx * dst_dimy / 2 + src_num_vram_rows * 6 + dst_num_vram_rows * (20 + 11) + 12;
	m_blit_delay_ns += num_vram_clk * EP1C_VRAM_CLK_NANOSEC;
}

static void gfx_draw(UINT32 *addr, const rectangle *clip, UINT64 *delay_pixels)
{
	int x,y, dimx,dimy, flipx,flipy;//, src_p;
	int trans,blend, s_mode, d_mode;
	clr_t tint_clr;
	int tinted = 0;

	UINT16 attr     =   READ_NEXT_WORD(addr);
	UINT16 alpha    =   READ_NEXT_WORD(addr);
	UINT16 src_x    =   READ_NEXT_WORD(addr);
	UINT16 src_y    =   READ_NEXT_WORD(addr);
	UINT16 dst_x_start  =   READ_NEXT_WORD(addr);
	UINT16 dst_y_start  =   READ_NEXT_WORD(addr);
	UINT16 w        =   READ_NEXT_WORD(addr);
	UINT16 h        =   READ_NEXT_WORD(addr);
	UINT16 tint_r   =   READ_NEXT_WORD(addr);
	UINT16 tint_gb  =   READ_NEXT_WORD(addr);

	// 0: +alpha
	// 1: +source
	// 2: +dest
	// 3: *
	// 4: -alpha
	// 5: -source
	// 6: -dest
	// 7: *

	d_mode  =    attr & 0x0007;
	s_mode  =   (attr & 0x0070) >> 4;

	trans   =    attr & 0x0100;
	blend   =      attr & 0x0200;

	flipy   =    attr & 0x0400;
	flipx   =    attr & 0x0800;

	const UINT8 d_alpha =   ((alpha & 0x00ff)       )>>3;
	const UINT8 s_alpha =   ((alpha & 0xff00) >> 8  )>>3;

//  src_p   =   0;
	src_x   =   src_x & 0x1fff;
	src_y   =   src_y & 0x0fff;


	x       =   (dst_x_start & 0x7fff) - (dst_x_start & 0x8000);
	y       =   (dst_y_start & 0x7fff) - (dst_y_start & 0x8000);

	dimx    =   (w & 0x1fff) + 1;
	dimy    =   (h & 0x0fff) + 1;

	// convert parameters to clr


	tint_to_clr(tint_r & 0x00ff, (tint_gb >>  8) & 0xff, tint_gb & 0xff, &tint_clr);

	/* interestingly this gets set to 0x20 for 'normal' not 0x1f */

	if (tint_clr.r!=0x20)
		tinted = 1;

	if (tint_clr.g!=0x20)
		tinted = 1;

	if (tint_clr.b!=0x20)
		tinted = 1;


	// surprisingly frequent, need to verify if it produces a worthwhile speedup tho.
	if ((s_mode==0 && s_alpha==0x1f) && (d_mode==4 && d_alpha==0x1f))
		blend = 0;

	if (tinted)
	{
		if (!flipx)
		{
			if (trans)
			{
				if (!blend)
				{
					draw_sprite_f0_ti1_tr1_plain(draw_params);
				}
				else
				{
					epic12_device_f0_ti1_tr1_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
			else
			{
			if (!blend)
				{
					draw_sprite_f0_ti1_tr0_plain(draw_params);
				}
				else
				{
					epic12_device_f0_ti1_tr0_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
		}
		else // flipx
		{
			if (trans)
			{
				if (!blend)
				{
					draw_sprite_f1_ti1_tr1_plain(draw_params);
				}
				else
				{
					epic12_device_f1_ti1_tr1_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
			else
			{
			if (!blend)
				{
					draw_sprite_f1_ti1_tr0_plain(draw_params);
				}
				else
				{
					epic12_device_f1_ti1_tr0_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
		}
	}
	else
	{
		if (blend==0 && tinted==0)
		{
			if (!flipx)
			{
				if (trans)
				{
					draw_sprite_f0_ti0_tr1_simple(draw_params);
				}
				else
				{
					draw_sprite_f0_ti0_tr0_simple(draw_params);
				}
			}
			else
			{
				if (trans)
				{
					draw_sprite_f1_ti0_tr1_simple(draw_params);
				}
				else
				{
					draw_sprite_f1_ti0_tr0_simple(draw_params);
				}

			}

			return;
		}



		//printf("smode %d dmode %d\n", s_mode, d_mode);

		if (!flipx)
		{
			if (trans)
			{
				if (!blend)
				{
					draw_sprite_f0_ti0_plain(draw_params);
				}
				else
				{
					epic12_device_f0_ti0_tr1_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
			else
			{
			if (!blend)
				{
					draw_sprite_f0_ti0_tr0_plain(draw_params);
				}
				else
				{
					epic12_device_f0_ti0_tr0_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
		}
		else // flipx
		{
			if (trans)
			{
				if (!blend)
				{
					draw_sprite_f1_ti0_plain(draw_params);
				}
				else
				{
					epic12_device_f1_ti0_tr1_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
			else
			{
			if (!blend)
				{
					draw_sprite_f1_ti0_tr0_plain(draw_params);
				}
				else
				{
					epic12_device_f1_ti0_tr0_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
		}
	}
}

static void gfx_create_shadow_copy()
{
	UINT32 addr = m_gfx_addr & 0x1fffffff;

	m_clip.set(m_gfx_clip_x_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_x_shadowcopy + 320 - 1 + EP1C_CLIP_MARGIN,
			   m_gfx_clip_y_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_y_shadowcopy + 240 - 1 + EP1C_CLIP_MARGIN);

	while (1)
	{
		// request commands from main CPU RAM
		const UINT16 data = COPY_NEXT_WORD(&addr);

		switch (data & 0xf000)
		{
			case 0x0000:
			case 0xf000:
				return;

			case 0xc000:
				if (COPY_NEXT_WORD(&addr)) // cliptype
					m_clip.set(m_gfx_clip_x_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_x_shadowcopy + 320 - 1 + EP1C_CLIP_MARGIN,
							   m_gfx_clip_y_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_y_shadowcopy + 240 - 1 + EP1C_CLIP_MARGIN);
				else
					m_clip.set(0, 0x2000 - 1, 0, 0x1000 - 1);
				idle_blitter(EP1C_CLIP_OPERATION_SIZE_BYTES);
				break;

			case 0x2000:
				addr -= 2;
				gfx_upload_shadow_copy(&addr);
				break;

			case 0x1000:
				addr -= 2;
				gfx_draw_shadow_copy(&addr);
				break;

			default:
				//popmessage("GFX op = %04X", data);
				return;
		}
	}
}


// Snapshot the same clipping state for each lane. Intersect only Y, so
// source X wrap checks and all blend/flip calculations remain unchanged.
static rectangle gfx_initial_clip()
{
	return rectangle(m_gfx_clip_x_shadowcopy - m_clip_margin_shadowcopy,
		m_gfx_clip_x_shadowcopy + 320 - 1 + m_clip_margin_shadowcopy,
		m_gfx_clip_y_shadowcopy - m_clip_margin_shadowcopy,
		m_gfx_clip_y_shadowcopy + 240 - 1 + m_clip_margin_shadowcopy);
}

static void gfx_exec_range(UINT32 addr, UINT32 end, rectangle clip,
	int min_y, int max_y, UINT64 *delay_pixels, bool split)
{
	while (addr != end) {
		UINT16 data=READ_NEXT_WORD(&addr);
		switch(data & 0xf000) {
			case 0x0000:
			case 0xf000:
				return;
			case 0xc000:
				if (READ_NEXT_WORD(&addr)) clip=gfx_initial_clip();
				else clip.set(0,0x2000-1,0,0x1000-1);
				break;
			case 0x2000:
				// Parallel preflight never admits uploads.
				if (split) return;
				addr-=2;
				gfx_upload(&addr);
				break;
			case 0x1000: {
				addr-=2;
				rectangle band=clip;
				if (split) {
					band.min_y=MAX(band.min_y,min_y);
					band.max_y=MIN(band.max_y,max_y);
				}
				if (band.min_y<=band.max_y) gfx_draw(&addr,&band,delay_pixels);
				else addr+=20;
				break;
			}
			default:
				return;
		}
	}
}

static void gfx_exec_band(int min_y, int max_y, UINT64 *delay_pixels, bool split)
{
	gfx_exec_range(m_gfx_addr_shadowcopy & 0x1fffffff, ~UINT32(0), gfx_initial_clip(),
		min_y, max_y, delay_pixels, split);
}

static void gfx_exec()
{
	gfx_exec_band(0,0x0fff,&epic12_device_blit_delay,false);
}

#if EPIC12_NEON && THREADY == THREADY_PTHREAD
// A 32x32 conservative tile map. Reject *any* overlap of a source
// rectangle with any destination tile in the list. With no VRAM
// read-after-write dependencies, two disjoint destination Y bands
// can execute the original draw order independently and bit-exactly.
struct Epic12TileMap {
	UINT64 bits[128][4] = {};
	void add(int x0,int x1,int y0,int y1) {
		const unsigned lo=x0>>5,hi=x1>>5;
		for (unsigned i=lo/64;i<=hi/64;i++) {
			const unsigned a=i==lo/64?lo%64:0, b=i==hi/64?hi%64:63;
			const UINT64 mask=(~UINT64(0)<<a)&(~UINT64(0)>>(63-b));
			for (int y=y0>>5;y<=(y1>>5);y++) bits[y][i]|=mask;
		}
	}
	bool intersects(const Epic12TileMap &other) const {
		for (int y=0;y<128;y++) for (int x=0;x<4;x++)
			if (bits[y][x]&other.bits[y][x]) return true;
		return false;
	}
	bool intersects(int x0,int x1,int y0,int y1) const {
		const unsigned lo=x0>>5,hi=x1>>5;
		for (unsigned i=lo/64;i<=hi/64;i++) {
			const unsigned a=i==lo/64?lo%64:0,b=i==hi/64?hi%64:63;
			const UINT64 mask=(~UINT64(0)<<a)&(~UINT64(0)>>(63-b));
			for (int y=y0>>5;y<=(y1>>5);y++) if (bits[y][i]&mask) return true;
		}
		return false;
	}
};

// Decode exactly the same clipped/flip origins as gfx_draw. Empty draws
// have no footprint; wrapped/unsafe draws must retain the serial clip.
struct Epic12Footprint {
	int x0,x1,y0,y1,rx0,rx1,ry0,ry1;
	unsigned weight;
	bool empty, flip_y;
};

static bool gfx_draw_footprint(UINT32 &addr, UINT16 attr, const rectangle &clip, Epic12Footprint &f)
{
	const UINT16 alpha=READ_NEXT_WORD(&addr);
	const int sx=READ_NEXT_WORD(&addr)&0x1fff, sy=READ_NEXT_WORD(&addr)&0x0fff;
	const int dx=(INT16)READ_NEXT_WORD(&addr),dy=(INT16)READ_NEXT_WORD(&addr);
	const int w=(READ_NEXT_WORD(&addr)&0x1fff)+1,h=(READ_NEXT_WORD(&addr)&0x0fff)+1;
	const UINT16 tr=READ_NEXT_WORD(&addr),tgb=READ_NEXT_WORD(&addr);
	const int x0=MAX(dx,clip.min_x),x1=MIN(dx+w-1,clip.max_x);
	const int y0=MAX(dy,clip.min_y),y1=MIN(dy+h-1,clip.max_y);
	f.empty=x0>x1 || y0>y1;
	if (f.empty) return true;
	// Keep wrapped or invalid geometry on the original path.
	if (sx+w>0x2000 || sy+h>0x1000 || x0<0 || y0<0 || x1>=0x2000 || y1>=0x1000)
		return false;
	// Only pixels actually read by the clipped draw count.
	// The original flip origin uses the uncut width/height.
	const bool fx=attr&0x800,fy=attr&0x400;
	const int rx0=sx+(fx?w-1-(x1-dx):x0-dx);
	const int rx1=sx+(fx?w-1-(x0-dx):x1-dx);
	const int ry0=sy+(fy?h-1-(y1-dy):y0-dy);
	const int ry1=sy+(fy?h-1-(y0-dy):y1-dy);
	const bool tint=((tr&0xff)>>2)!=32 || ((tgb>>8)>>2)!=32 || ((tgb&0xff)>>2)!=32;
	const bool blend=(attr&0x200) && !((attr&0x77)==4 && (alpha&0xf8f8)==0xf8f8);
	const unsigned weight=1+((attr&0x100)!=0)+2*blend+tint;
	f={x0,x1,y0,y1,rx0,rx1,ry0,ry1,weight,false,fy};
	return true;
}

// Two workers preserve draw order inside their own destination row bands.
// A read/write dependency within a band is harmless: it is resolved by
// that one worker in original order. Only cross-band RAW/WAR hazards,
// including within a single self-overlapping draw, require serialization.
// The coarse tile map remains the fast broad phase; exact rectangles
// are consulted only on a hit. Work and scratch memory are bounded.
struct Epic12ExactRect {
	UINT16 x0,x1,y0,y1;
	bool overlaps(const Epic12ExactRect &r) const {
		return x0<=r.x1 && r.x0<=x1 && y0<=r.y1 && r.y0<=y1;
	}
};
struct Epic12ExactSet {
	static const unsigned Limit=2048;
	Epic12TileMap tiles;
	Epic12ExactRect rects[Limit];
	unsigned count=0;
	bool add(const Epic12ExactRect &r) {
		if (count==Limit) return false;
		rects[count++]=r;
		tiles.add(r.x0,r.x1,r.y0,r.y1);
		return true;
	}
	bool clear_of(const Epic12ExactRect &r,unsigned &budget) const {
		if (!tiles.intersects(r.x0,r.x1,r.y0,r.y1)) return true;
		for (unsigned i=0;i<count;i++) {
			if (!budget) return false;
			--budget;
			if (r.overlaps(rects[i])) return false;
		}
		return true;
	}
};
static bool gfx_lanes_independent(UINT32 addr,UINT32 end,rectangle clip,int split)
{
	Epic12ExactSet reads[2],writes[2];
	unsigned budget=131072;
	const UINT32 start=addr;
	for (unsigned cmd=0;cmd<8192;cmd++) {
		if (addr==end) return true;
		if (addr-start+2>UINT32(m_main_ramsize)) return false;
		const UINT16 attr=READ_NEXT_WORD(&addr);
		switch (attr&0xf000) {
			case 0: case 0xf000: return true;
			case 0xc000:
				if (addr-start+2>UINT32(m_main_ramsize)) return false;
				if (READ_NEXT_WORD(&addr)) clip=gfx_initial_clip();
				else clip.set(0,0x1fff,0,0x0fff);
				break;
			case 0x1000: {
				if (addr-start+18>UINT32(m_main_ramsize)) return false;
				Epic12Footprint f;
				if (!gfx_draw_footprint(addr,attr,clip,f)) return false;
				if (f.empty) break;
				for (unsigned lane=0;lane<2;lane++) {
					const int y0=lane?MAX(f.y0,split):f.y0;
					const int y1=lane?f.y1:MIN(f.y1,split-1);
					if (y0>y1) continue;
					const Epic12ExactRect w={UINT16(f.x0),UINT16(f.x1),UINT16(y0),UINT16(y1)};
					const unsigned other=1-lane;
					if (!reads[other].clear_of(w,budget) || !writes[lane].add(w)) return false;
					int sy0=f.flip_y?f.ry1-(y1-f.y0):f.ry0+(y0-f.y0);
					int sy1=f.flip_y?f.ry1-(y0-f.y0):f.ry0+(y1-f.y0);
					// Source rows owned by this lane cannot race with
					// another lane; keep only the other band's rows.
					if (lane) sy1=MIN(sy1,split-1);
					else sy0=MAX(sy0,split);
					if (sy0<=sy1) {
						const Epic12ExactRect r={UINT16(f.rx0),UINT16(f.rx1),UINT16(sy0),UINT16(sy1)};
						if (!writes[other].clear_of(r,budget) || !reads[lane].add(r)) return false;
					}
				}
				break;
			}
			default: return false;
		}
	}
	return false;
}

static bool gfx_split_range(UINT32 addr, UINT32 end, rectangle clip, int &split_y, Epic12SplitReject *why=nullptr, bool *precise=nullptr)
{
	auto reject=[&](Epic12SplitReject reason) {if (why) *why=reason; return false;};
	if (precise) *precise=false;
	const rectangle initial=clip;
	const UINT32 start=addr;
	Epic12TileMap reads,writes;
	// Fine Y bands and relative pixel cost, rather than raw area alone.
	// Blend/tint is more costly than a plain copy; these weights guide
	// host scheduling only. They never affect emulated delay accounting.
	UINT32 work[512]={};
	UINT64 total=0,pixels=0;
	for (unsigned cmd=0;cmd<8192;cmd++) {
		if (addr-start+2>UINT32(m_main_ramsize)) return reject(EPIC12_SPLIT_OTHER);
		const UINT16 attr=addr==end ? 0 : READ_NEXT_WORD(&addr);
		const unsigned bytes=(attr&0xf000)==0x1000 ? 18 : (attr&0xf000)==0xc000 ? 2 : 0;
		if (addr-start+bytes>UINT32(m_main_ramsize)) return reject(EPIC12_SPLIT_OTHER);
		switch(attr&0xf000) {
			case 0x0000:
			case 0xf000: {
				if (pixels<180000) return reject(EPIC12_SPLIT_SMALL);
				const bool collision=reads.intersects(writes);
				if (collision && !blitPreciseEnabled) return reject(EPIC12_SPLIT_DEPENDENCY);
				UINT64 upper=0;
				// Try at most three near-balanced cuts, in increasing
				// estimated imbalance order. A different cut can avoid
				// a cross-lane hazard without discarding a whole list.
				UINT64 best[3]={~UINT64(0),~UINT64(0),~UINT64(0)};
				int best_y[3]={};
				for (int i=0;i<511;i++) {
					upper+=work[i];
					// Keep each worker busy and choose the closest cut,
					// including the boundary before the midpoint.
					if (upper<total/4 || total-upper<total/4) continue;
					const UINT64 lower=total-upper;
					const UINT64 diff=upper>lower?upper-lower:lower-upper;
					for (unsigned k=0;k<3;k++) if (diff<best[k]) {
						for (unsigned j=2;j>k;j--) {best[j]=best[j-1];best_y[j]=best_y[j-1];}
						best[k]=diff;best_y[k]=(i+1)*8;break;
					}
					if (!collision && upper>=lower) break;
				}
				if (!best_y[0]) return reject(EPIC12_SPLIT_BALANCE);
				for (unsigned k=0;k<3 && best_y[k];k++) {
					if (!collision || gfx_lanes_independent(start,end,initial,best_y[k])) {
						split_y=best_y[k];
						if (precise) *precise=collision;
						return true;
					}
				}
				return reject(EPIC12_SPLIT_DEPENDENCY);
			}
			case 0xc000:
				if (READ_NEXT_WORD(&addr)) clip=gfx_initial_clip();
				else clip.set(0,0x1fff,0,0x0fff);
				break;
			case 0x1000: {
				Epic12Footprint f;
				if (!gfx_draw_footprint(addr,attr,clip,f)) return reject(EPIC12_SPLIT_GEOMETRY);
				if (f.empty) break;
				reads.add(f.rx0,f.rx1,f.ry0,f.ry1);
				writes.add(f.x0,f.x1,f.y0,f.y1);
				const int x0=f.x0,x1=f.x1,y0=f.y0,y1=f.y1;
				const unsigned weight=f.weight;
				for (int i=y0/8;i<=y1/8;i++) {
					const UINT32 n=(x1-x0+1)*(MIN(y1,i*8+7)-MAX(y0,i*8)+1);
					const UINT32 cost=n*weight;
					if (work[i]>0xffffffffu-cost) return reject(EPIC12_SPLIT_OTHER);
					work[i]+=cost;
					total+=cost;
					pixels+=n;
				}
				break;
			}
			case 0x2000:
				return reject(EPIC12_SPLIT_UPLOAD);
			default:
				return reject(EPIC12_SPLIT_OTHER);
		}
	}
	return reject(EPIC12_SPLIT_OTHER);
}

static bool gfx_split_plan(int &split_y, Epic12SplitReject *why=nullptr, bool *precise=nullptr)
{
	return gfx_split_range(m_gfx_addr_shadowcopy & 0x1fffffff, ~UINT32(0), gfx_initial_clip(), split_y, why, precise);
}

struct Epic12Segment {
	UINT32 start,end;
	rectangle clip;
	int split_y; // zero: original serial execution, including uploads
	bool precise;
};
static const unsigned EPIC12_MAX_SEGMENTS=64;
static const Epic12Segment *blitHelperSegment;

// Second pass only for lists rejected for uploads/dependencies/geometry.
// Preflight the ENTIRE list before writing VRAM: failure cannot leave a
// partially executed list for the caller to replay. No allocation, at most
// 8192 commands / one RAM traversal / 64 ranges (and helper submissions).
static bool gfx_segment_plan(Epic12Segment *segments, unsigned &count, bool relaxed=false)
{
	count=0;
	const UINT32 start=m_gfx_addr_shadowcopy & 0x1fffffff;
	UINT32 addr=start,run=start;
	rectangle clip=gfx_initial_clip(),run_clip=clip;
	Epic12TileMap reads,writes;
	bool any_parallel=false;
	UINT64 run_pixels=0;
	auto append=[&](UINT32 first,UINT32 last,const rectangle &initial,bool candidate) {
		if (first==last) return true;
		int split=0;
		bool precise=false;
		if (candidate && gfx_split_range(first,last,initial,split,nullptr,&precise)) any_parallel=true;
		// Adjacent serial ranges can share an execution pass. Their clip
		// commands remain in the stream, so only the first snapshot matters.
		if (!split && count && !segments[count-1].split_y && segments[count-1].end==first) {
			segments[count-1].end=last;
			return true;
		}
		if (count==EPIC12_MAX_SEGMENTS) return false;
		segments[count++]={first,last,initial,split,precise};
		return true;
	};
	auto flush=[&](UINT32 end) {
		if (!append(run,end,run_clip,run_pixels>=180000)) return false;
		if (run_pixels) {
			if (!relaxed) {reads=Epic12TileMap{};writes=Epic12TileMap{};}
			run_pixels=0;
		}
		run=end;
		run_clip=clip;
		return true;
	};
	for (unsigned cmd=0;cmd<8192;cmd++) {
		const UINT32 at=addr;
		if (addr-start+2>UINT32(m_main_ramsize)) return false;
		const UINT16 attr=READ_NEXT_WORD(&addr);
		const unsigned type=attr&0xf000;
		const unsigned bytes=type==0x1000 ? 18 : type==0x2000 ? 14 : type==0xc000 ? 2 : 0;
		if (addr-start+bytes>UINT32(m_main_ramsize)) return false;
		switch (type) {
			case 0x0000:
			case 0xf000:
				return flush(at) && any_parallel;
			case 0xc000:
				if (READ_NEXT_WORD(&addr)) clip=gfx_initial_clip();
				else clip.set(0,0x1fff,0,0x0fff);
				break;
			case 0x2000: {
				// Skip inline data, never interpret pixels as commands. The
				// original uploader runs once after both preceding lanes join.
				addr+=6;
				const unsigned x=READ_NEXT_WORD(&addr)&0x1fff,y=READ_NEXT_WORD(&addr)&0x0fff;
				const unsigned w=(READ_NEXT_WORD(&addr)&0x1fff)+1,h=(READ_NEXT_WORD(&addr)&0x0fff)+1;
				if (x+w>0x2000 || y+h>0x1000 || addr-start+w*h*2>UINT32(m_main_ramsize)) return false;
				addr+=w*h*2;
				if (!flush(at) || !append(at,addr,clip,false)) return false;
				run=addr;
				run_clip=clip;
				break;
			}
			case 0x1000: {
				Epic12Footprint f;
				const bool safe=gfx_draw_footprint(addr,attr,clip,f);
				if (f.empty) break;
				// Self overlap (including conservative tile alias) and unsafe
				// geometry retain full serial pixel order and original clip.
				const bool self=safe && (f.rx0>>5)<=(f.x1>>5) && (f.x0>>5)<=(f.rx1>>5)
					&& (f.ry0>>5)<=(f.y1>>5) && (f.y0>>5)<=(f.ry1>>5);
				if (!safe || (!relaxed && self)) {
					if (!flush(at) || !append(at,addr,clip,false)) return false;
					run=addr;
					run_clip=clip;
					break;
				}
				// Both RAW and WAR need a barrier. Destination/destination
				// overlap is safe: each lane keeps the original draw order.
				if (!relaxed) {
					if (writes.intersects(f.rx0,f.rx1,f.ry0,f.ry1) || reads.intersects(f.x0,f.x1,f.y0,f.y1))
						if (!flush(at)) return false;
					reads.add(f.rx0,f.rx1,f.ry0,f.ry1);
					writes.add(f.x0,f.x1,f.y0,f.y1);
				}
				run_pixels+=UINT64(f.x1-f.x0+1)*(f.y1-f.y0+1);
				break;
			}
			default:
				return false;
		}
	}
	return false;
}

// An alternative to slicing every draw into the same horizontal bands.
// Assign COMPLETE draws to two lanes based on 32x32 tile broad-phase
// and exact rectangle dependencies (RAW, WAR, WAW). A draw with
// self-overlap/flip remains in one lane in the original pixel order.
// Only join when neither lane can accept the next command. This is
// command-level tile ownership, not a speculative pixel reorder.
struct Epic12TileDraw {
	UINT32 addr;
	rectangle clip;
	unsigned lane;
};
struct Epic12TilePhase {
	UINT32 start,end;
	rectangle clip;
	unsigned first,last;
	bool parallel;
};
struct Epic12TilePlan {
	static const unsigned MaxDraws=8192,MaxPhases=64;
	Epic12TileDraw draws[MaxDraws];
	Epic12TilePhase phases[MaxPhases];
	unsigned drawCount=0,phaseCount=0;
};
static const Epic12TilePlan *blitHelperTiles;
static const Epic12TilePhase *blitHelperTilePhase;

static bool gfx_tile_plan(Epic12TilePlan &plan)
{
	plan.drawCount=plan.phaseCount=0;
	const UINT32 start=m_gfx_addr_shadowcopy&0x1fffffff;
	UINT32 addr=start,run=start;
	rectangle clip=gfx_initial_clip(),runClip=clip;
	Epic12ExactSet reads[2],writes[2];
	UINT64 work[2]={};
	unsigned first=0,budget=262144;
	bool profitable=false;
	auto append=[&](UINT32 a,UINT32 b,const rectangle &c,unsigned f,unsigned l,bool parallel){
		if(a==b)return true;
		if(!parallel && plan.phaseCount && !plan.phases[plan.phaseCount-1].parallel &&
			plan.phases[plan.phaseCount-1].end==a) {
			plan.phases[plan.phaseCount-1].end=b;return true;
		}
		if(plan.phaseCount==Epic12TilePlan::MaxPhases)return false;
		plan.phases[plan.phaseCount++]={a,b,c,f,l,parallel};
		profitable|=parallel;
		return true;
	};
	auto flush=[&](UINT32 end){
		const UINT64 sum=work[0]+work[1],small=MIN(work[0],work[1]);
		const bool parallel=sum>=90000 && small>=24000 && small>=sum/4;
		if(!append(run,end,runClip,first,plan.drawCount,parallel))return false;
		run=end;runClip=clip;first=plan.drawCount;
		for(unsigned lane=0;lane<2;lane++){
			work[lane]=0;
			reads[lane].count=writes[lane].count=0;
			reads[lane].tiles=Epic12TileMap{};writes[lane].tiles=Epic12TileMap{};
		}
		return true;
	};
	for(unsigned cmd=0;cmd<8192;cmd++){
		const UINT32 at=addr;
		if(addr-start+2>UINT32(m_main_ramsize))return false;
		const UINT16 attr=READ_NEXT_WORD(&addr);
		const unsigned type=attr&0xf000;
		const unsigned bytes=type==0x1000?18:type==0x2000?14:type==0xc000?2:0;
		if(addr-start+bytes>UINT32(m_main_ramsize))return false;
		switch(type){
			case 0:case 0xf000:return flush(at)&&profitable;
			case 0xc000:
				if(READ_NEXT_WORD(&addr))clip=gfx_initial_clip();
				else clip.set(0,0x1fff,0,0x0fff);
				break;
			case 0x2000:{
				addr+=6;
				const unsigned x=READ_NEXT_WORD(&addr)&0x1fff,y=READ_NEXT_WORD(&addr)&0xfff;
				const unsigned w=(READ_NEXT_WORD(&addr)&0x1fff)+1,h=(READ_NEXT_WORD(&addr)&0xfff)+1;
				if(x+w>0x2000 || y+h>0x1000 || addr-start+w*h*2>UINT32(m_main_ramsize))return false;
				addr+=w*h*2;
				if(!flush(at)||!append(at,addr,clip,0,0,false))return false;
				run=addr;runClip=clip;break;
			}
			case 0x1000:{
				Epic12Footprint f;
				const bool safe=gfx_draw_footprint(addr,attr,clip,f);
				if(f.empty)break;
				if(!safe){
					if(!flush(at)||!append(at,addr,clip,0,0,false))return false;
					run=addr;runClip=clip;break;
				}
				const Epic12ExactRect r={UINT16(f.rx0),UINT16(f.rx1),UINT16(f.ry0),UINT16(f.ry1)};
				const Epic12ExactRect w={UINT16(f.x0),UINT16(f.x1),UINT16(f.y0),UINT16(f.y1)};
				auto allowed=[&](unsigned lane){
					const unsigned other=1-lane;
					return reads[lane].count<Epic12ExactSet::Limit && writes[lane].count<Epic12ExactSet::Limit &&
						writes[other].clear_of(r,budget) && reads[other].clear_of(w,budget) &&
						writes[other].clear_of(w,budget);
				};
				unsigned lane=work[0]<=work[1]?0:1;
				if(!allowed(lane)){
					lane=1-lane;
					if(!allowed(lane)){
						if(!budget||!flush(at))return false;
						lane=0;
					}
				}
				if(!budget||plan.drawCount==Epic12TilePlan::MaxDraws ||
					!reads[lane].add(r)||!writes[lane].add(w))return false;
				plan.draws[plan.drawCount++]={at,clip,lane};
				work[lane]+=UINT64(f.x1-f.x0+1)*(f.y1-f.y0+1)*f.weight;
				break;
			}
			default:return false;
		}
	}
	return false;
}

static void gfx_tile_execute_lane(const Epic12TilePlan &plan,const Epic12TilePhase &phase,
	unsigned lane,UINT64 *delay)
{
	for(unsigned i=phase.first;i<phase.last;i++){
		const auto &d=plan.draws[i];
		if(d.lane!=lane)continue;
		UINT32 addr=d.addr;
		gfx_draw(&addr,&d.clip,delay);
	}
}

static void gfx_helper_cb()
{
	blitHelperDelay=0;
	if(blitHelperTiles){
		gfx_tile_execute_lane(*blitHelperTiles,*blitHelperTilePhase,1,&blitHelperDelay);
		return;
	}
	if (blitHelperSegment) {
		const Epic12Segment &s=*blitHelperSegment;
		gfx_exec_range(s.start,s.end,s.clip,s.split_y,0x0fff,&blitHelperDelay,true);
	} else gfx_exec_band(blitSplitY,0x0fff,&blitHelperDelay,true);
}

static bool gfx_exec_tiles()
{
	Epic12TilePlan plan;
	const UINT64 begin=blitProfile?BlitNow():0;
	const bool planned=gfx_tile_plan(plan);
	if(blitProfile){
		const UINT64 elapsed=BlitNow()-begin;
		blitPlanNs.fetch_add(elapsed,std::memory_order_relaxed);
		blitTilePlanNs.fetch_add(elapsed,std::memory_order_relaxed);
	}
	if(!planned)return false;
	unsigned phases=0,draws=0;
	for(unsigned i=0;i<plan.phaseCount;i++){
		const auto &p=plan.phases[i];
		if(!p.parallel){
			gfx_exec_range(p.start,p.end,p.clip,0,0x0fff,&epic12_device_blit_delay,false);
			continue;
		}
		phases++;draws+=p.last-p.first;
		blitHelperTiles=&plan;blitHelperTilePhase=&p;
		blitHelper.notify();
		gfx_tile_execute_lane(plan,p,0,&epic12_device_blit_delay);
		blitHelper.notify_wait();
		epic12_device_blit_delay+=blitHelperDelay;
		blitHelperTiles=nullptr;blitHelperTilePhase=nullptr;
	}
	if(blitProfile){
		blitTileJobs.fetch_add(1,std::memory_order_relaxed);
		blitTilePhases.fetch_add(phases,std::memory_order_relaxed);
		blitTileDraws.fetch_add(draws,std::memory_order_relaxed);
	}
	return true;
}

static bool gfx_exec_segmented()
{
	Epic12Segment segments[EPIC12_MAX_SEGMENTS];
	unsigned count;
	const UINT64 begin=blitProfile?BlitNow():0;
	bool planned=gfx_segment_plan(segments,count);
	if(!planned && blitPreciseEnabled)planned=gfx_segment_plan(segments,count,true);
	if(blitProfile)blitPlanNs.fetch_add(BlitNow()-begin,std::memory_order_relaxed);
	if(!planned)return false;
	unsigned splitCount=0, preciseCount=0;
	for (unsigned i=0;i<count;i++) {
		const Epic12Segment &s=segments[i];
		if (!s.split_y) {
			gfx_exec_range(s.start,s.end,s.clip,0,0x0fff,&epic12_device_blit_delay,false);
			continue;
		}
		// notify publishes this immutable range; join before reusing it,
		// uploading, or allowing a dependent draw to read the results.
		++splitCount;
		preciseCount+=s.precise;
		blitHelperSegment=&s;
		blitHelper.notify();
		gfx_exec_range(s.start,s.end,s.clip,0,s.split_y-1,&epic12_device_blit_delay,true);
		blitHelper.notify_wait();
		epic12_device_blit_delay+=blitHelperDelay;
	}
	blitHelperSegment=nullptr;
	if (blitProfile) {
		blitSegmentedJobs.fetch_add(1,std::memory_order_relaxed);
		blitSegmentedRanges.fetch_add(splitCount,std::memory_order_relaxed);
		blitLaneRanges.fetch_add(preciseCount,std::memory_order_relaxed);
	}
	return true;
}

static bool gfx_exec_parallel()
{
	// mmpork used tile scheduling for >94% of async lists. Reuse
	// the previous successful choice, not its stale command plan.
	// Every list is still preflighted from current shadow RAM.
	bool triedTiles=false;
	auto tryTiles=[&](){
		triedTiles=true;
		const bool ok=gfx_exec_tiles();
		blitTilePreferred=ok;
		return ok;
	};
	if(blitTilesEnabled && blitTileFirstEnabled && blitTilePreferred){
		if(tryTiles()){
			if(blitProfile)blitTileFirstHits.fetch_add(1,std::memory_order_relaxed);
			return true;
		}
	}
	Epic12SplitReject why=EPIC12_SPLIT_OTHER;
	bool precise=false;
	const UINT64 begin=blitProfile?BlitNow():0;
	const bool split=gfx_split_plan(blitSplitY,&why,&precise);
	if(blitProfile)blitPlanNs.fetch_add(BlitNow()-begin,std::memory_order_relaxed);
	if(!split){
		if(blitTilesEnabled && !triedTiles && (why==EPIC12_SPLIT_DEPENDENCY || why==EPIC12_SPLIT_BALANCE)
			&& tryTiles())return true;
		blitTilePreferred=false;
		if(blitSegmentsEnabled && (why==EPIC12_SPLIT_UPLOAD || why==EPIC12_SPLIT_DEPENDENCY || why==EPIC12_SPLIT_GEOMETRY)
			&& gfx_exec_segmented())return true;
		if(blitTilesEnabled && !triedTiles && (why==EPIC12_SPLIT_UPLOAD || why==EPIC12_SPLIT_SMALL)
			&& tryTiles())return true;
		if(blitProfile)blitSplitReject[why].fetch_add(1,std::memory_order_relaxed);
		return false;
	}
	blitTilePreferred=false;
	blitHelper.notify();
	gfx_exec_band(0,blitSplitY-1,&epic12_device_blit_delay,true);
	blitHelper.notify_wait();
	epic12_device_blit_delay+=blitHelperDelay;
	if(precise && blitProfile)blitLaneRanges.fetch_add(1,std::memory_order_relaxed);
	return true;
}
#endif


void epic12_wait_blitterthread()
{
	thready.notify_wait();
}

static void gfx_exec_write(UINT32 data)
{
//	if ( ACCESSING_BITS_0_7 )
	{
		if (data & 1)
		{
			const UINT64 waitStart = blitProfile ? BlitNow() : 0;
			thready.notify_wait();
			if (blitProfile) blitSubmitWaitNs+=BlitNow()-waitStart;

			m_gfx_clip_x_shadowcopy = m_gfx_clip_x;
			m_gfx_clip_y_shadowcopy = m_gfx_clip_y;
			m_clip_margin_shadowcopy = EP1C_CLIP_MARGIN;

			// Create a copy of the blit list so we can safely thread it.
			// Copying the Blitter operations will also estimate the delay needed for processing.
			m_blit_delay_ns = 0;
			const UINT64 shadowStart = blitProfile ? BlitNow() : 0;
			gfx_create_shadow_copy();
			if (blitProfile) blitShadowNs+=BlitNow()-shadowStart;

			m_gfx_addr_shadowcopy = m_gfx_addr;

			if (m_delay_method) // old method
			{
				if (epic12_device_blit_delay && m_delay_scale)
				{
					m_blitter_busy = 1;
					int delay = epic12_device_blit_delay*(15 * m_delay_scale / 50);
					INT32 cycles = (INT32)((double)((double)delay / 1000000000) * sh4_get_cpu_speed());

					//bprintf(0, _T("old_blitter_delay  %d   cycles  %d\n"),delay, cycles);

					sh4_set_cave_blitter_delay_timer(cycles);
				}
				else
				{
					m_blitter_busy = 0;
				}
			} else {  // new method (buffis)
				// Every EP1C_VRAM_H_LINE_PERIOD_NANOSEC, the Blitter will block other operations, due
				// to fetching a horizontal line from VRAM for output.
				m_blit_delay_ns += floor( m_blit_delay_ns / EP1C_VRAM_H_LINE_PERIOD_NANOSEC ) * EP1C_VRAM_H_LINE_DURATION_NANOSEC;

				// Check if Blitter takes longer than a frame to render.
				// In practice, there's a bit less time than this to allow for lack of slowdown but
				// for debugging purposes this is an ok approximation.
				//if (m_blit_delay_ns > EP1C_FRAME_DURATION_NANOSEC)
				//	LOGDBG("Blitter delay! Blit duration %lld ns.\n", m_blit_delay_ns);

				m_blitter_busy = 1;

				INT32 cycles = (INT32)((double)((double)m_blit_delay_ns / 1000000000) * sh4_get_cpu_speed());

				//bprintf(0, _T("new blit_delay  %I64d   cycles  %d\n"),m_blit_delay_ns, cycles);

				sh4_set_cave_blitter_delay_timer(cycles);
			}

			epic12_device_blit_delay = 0;
			// Previous list has been joined; notify publishes the snapshot.
			blitJobAsync=thready.is_threaded()!=0;
#if EPIC12_NEON && THREADY == THREADY_PTHREAD
			blitParallelAllowed=blitJobAsync && !m_delay_method && (dips[1]&4);
#endif
			if (blitProfile) {
				blitJobs++;
				if (blitJobAsync) blitAsyncJobs++;
			}
			thready.notify(); // run gfx_exec(), w/ threading if set (via dip option)
		}
	}
}

static void pal16_check_init()
{
	if (nBurnBpp < 3 && !pal16) {
		// Valid device colors are RGB555, not arbitrary RGB888.
		pal16=(UINT16*)BurnMalloc(32768*sizeof(UINT16));
		for (INT32 i=0;i<32768;i++) {
			const UINT32 p=Epic12Fast::Expand(UINT16(i));
			pal16[i]=BurnHighCol((p>>16)&255,(p>>8)&255,p&255,0);
		}
	}
}

static void epic12_draw_screen16_24bpp()
{
	INT32 scrollx = -m_gfx_scroll_x;
	INT32 scrolly = -m_gfx_scroll_y;

	UINT8  *dst = (UINT8  *)pBurnDraw;
	UINT32 *src = (UINT32 *)m_bitmaps;
	const INT32 heightmask = 0x1000 - 1;
	const INT32 widthmask  = 0x2000 - 1;

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		const size_t row=((y-scrolly)&heightmask)*0x2000;
		const UINT32 *s0=m_bitmaps16?nullptr:src+row;
		const UINT16 *p0=m_bitmaps16?m_bitmaps16+row:nullptr;
		INT32 sx;

		switch (nBurnBpp) {
			case 2: // 16bpp
				for (INT32 x = 0; x < nScreenWidth; x++, dst += nBurnBpp)
				{
					sx = x - scrollx;
					PutPix(dst,pal16[(p0?p0[sx&widthmask]:Epic12Packed::Pack(s0[sx&widthmask]))&0x7fff]);
				}
				break;
			case 3: // 24bpp
				for (INT32 x = 0; x < nScreenWidth; x++, dst += nBurnBpp)
				{
					sx = x - scrollx;
					PutPix(dst,p0?Epic12Fast::Expand(p0[sx&widthmask]):s0[sx&widthmask]);
				}
				break;
		}
	}
}

void epic12_draw_screen(UINT8 &recalc_palette)
{
	INT32 scrollx = -m_gfx_scroll_x;
	INT32 scrolly = -m_gfx_scroll_y;

	if (nBurnBpp != 4) {
		if (recalc_palette) {
			pal16_check_init();
			recalc_palette = 0;
		}

		epic12_draw_screen16_24bpp();
		return;
	}

	UINT32 *dst = (UINT32 *)pBurnDraw;
	UINT32 *src = (UINT32 *)m_bitmaps;
	const INT32 heightmask = 0x1000 - 1;
	const INT32 widthmask  = 0x2000 - 1;

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		const size_t row=((y-scrolly)&heightmask)*0x2000;
		const UINT32 *s0=m_bitmaps16?nullptr:src+row;
		const UINT16 *p0=m_bitmaps16?m_bitmaps16+row:nullptr;
		UINT32 *d0 = dst + (y * nScreenWidth);
		UINT32 sx = UINT32(-scrollx) & widthmask;
		INT32 left = nScreenWidth;
		while (left > 0) {
			const INT32 available = 0x2000 - sx;
			const INT32 n = left < available ? left : available;
			if (p0) Epic12Packed::ExpandRow(d0,p0+sx,n);
			else memcpy(d0,s0+sx,n*sizeof(UINT32));
			d0 += n;
			left -= n;
			sx = 0;
		}
	}
}



// 0x18000000 - 0x18000057

UINT32 epic12_blitter_read(UINT32 offset)
{
	switch (offset)
	{
		case 0x10:
			if (m_blitter_busy)
			{
				//m_maincpu->spin_until_time(attotime::from_usec(10));
				if (sleep_on_busy) {
					Sh3BurnCycles(m_burn_cycles); // 0x400 @ (12800000*8)
				}
				//bprintf(0, _T("%d frame - blitter busy read....."), nCurrentFrame);

				return 0x00000000;
			}
			else
				return 0x00000010;

		case 0x24:
			return 0xffffffff;

		case 0x28:
			return 0xffffffff;

		case 0x50:
			return *dips;

		default:
			//logerror("unknownblitter_r %08x %08x\n", offset*4, mem_mask);
			break;

	}
	return 0;
}


void epic12_blitter_write(UINT32 offset, UINT32 data)
{
	switch (offset)
	{
		case 0x04:
			gfx_exec_write(data);
			break;

		case 0x08:
			m_gfx_addr = data & 0xffffff;
			break;

		case 0x14:
			m_gfx_scroll_x = data;
			break;

		case 0x18:
			m_gfx_scroll_y = data;
			break;

		case 0x40:
			m_gfx_clip_x = data;
			break;

		case 0x44:
			m_gfx_clip_y = data;
			break;
	}
}

// Libretro state callbacks concatenate areas in order. Emit the same
// expanded byte stream in bounded chunks, not a 128 MiB temporary.
static void epic12_scan_vram(INT32 nAction)
{
	if (!m_bitmaps16) {ScanVar(m_bitmaps,m_gfx_size*4,"epic12 vram");return;}
	// Metadata-only callbacks (e.g. libretro achievement RAM discovery)
	// neither serialize nor restore data. Expose the real allocation.
	if (!(nAction&ACB_ACCESSMASK)) {ScanVar(m_bitmaps16,m_gfx_size*2,"epic12 vram");return;}
	UINT32 chunk[8192];
	for (int at=0;at<m_gfx_size;at+=8192) {
		const int n=MIN(8192,m_gfx_size-at);
		if (nAction&ACB_READ) Epic12Packed::ExpandRow(chunk,m_bitmaps16+at,n);
		BurnArea area={chunk,UINT32(n*4),0,(char*)"epic12 vram"};
		if (BurnAcb(&area)!=0) return;
		if (nAction&ACB_WRITE)
			for(int i=0;i<n;i++)m_bitmaps16[at+i]=Epic12Packed::Pack(chunk[i]);
	}
}

void epic12_scan(INT32 nAction, INT32 *pnMin)
{
	thready.notify_wait();
	SCAN_VAR(m_gfx_addr);
//	SCAN_VAR(m_gfx_addr_shadowcopy); // probably not needed!
	SCAN_VAR(m_gfx_scroll_x);
	SCAN_VAR(m_gfx_scroll_y);
	SCAN_VAR(m_gfx_clip_x);
	SCAN_VAR(m_gfx_clip_y);
//	SCAN_VAR(m_gfx_clip_x_shadowcopy);
//	SCAN_VAR(m_gfx_clip_y_shadowcopy);
	SCAN_VAR(epic12_device_blit_delay);
	SCAN_VAR(m_delay_scale);
	SCAN_VAR(m_blitter_busy);
	SCAN_VAR(m_blit_delay_ns);
	SCAN_VAR(m_blit_idle_op_bytes);

	if (~nAction & ACB_RUNAHEAD) {
		epic12_scan_vram(nAction);
	}
	thready.scan();
}
