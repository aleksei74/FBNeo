/* ported from mame 0.155 */
/*
Copyright (c) 1997-2015 Nicola Salmoria and the MAME team
Redistributions may not be sold, nor may they be used in a commercial product or activity.
*/
/*****************************************************************************
 *
 *   sh4.c
 *   Portable Hitachi SH-4 (SH7750 family) emulator
 *
 *   By R. Belmont, based on sh2.c by Juergen Buchmueller, Mariusz Wojcieszek,
 *      Olivier Galibert, Sylvain Glaize, and James Forshaw.
 *
 *
 *   TODO: FPU
 *         DMA
 *         on-board peripherals
 *
 *   DONE: boot/reset setup
 *         64-bit data bus
 *         banked registers
 *         additional registers for supervisor mode
 *         FPU status and data registers
 *         state save for the new registers
 *         interrupts
 *         store queues
 *
 *****************************************************************************/

#include "burnint.h"
#include "sh4_intf.h"
#include "cv1k_runtime_policy.h"
#include <stddef.h>
#include "sh4.h"
#include "sh4regs.h"
#include "sh4comn.h"
#include "sh3comn.h"
#include "sh4dmac.h"
#include "sh4tmu.h"
#include "math.h"
#include "sh3_arm64_native.h"
#include "sh3_arm64_v2.h"
#include <chrono>

#define COMBINE_DATA(varptr)		(*(varptr) = (*(varptr) & ~mem_mask) | (data & mem_mask))

static int c_md2;
static int c_md1;
static int c_md0;
static int c_md6;
static int c_md4;
static int c_md3;
static int c_md5;
static int c_md7;
static int c_md8;
static int c_clock;

// Keep the frequently accessed integer state under one base address.
// Reference aliases preserve all existing handler and SCAN_VAR names;
// only host placement changes. Never serialize the whole structure.
struct alignas(64) Sh3HotState {
	UINT32 r[16];
	UINT32 sr,pc,ppc,pr,gbr,mach,macl,ea;
	int icount;
	INT32 total_cycles;
	UINT32 delay,test_irq,cpu_off,pending_irq,spc,ssr,vbr;
};
static Sh3HotState sh3Hot;
static UINT32 (&m_r)[16]=sh3Hot.r;
static int &m_sh4_icount=sh3Hot.icount;
static INT32 &sh3_total_cycles=sh3Hot.total_cycles;
static UINT32 &m_ppc=sh3Hot.ppc;
static UINT32 &m_pc=sh3Hot.pc;
static UINT32 &m_spc=sh3Hot.spc;
static UINT32 &m_pr=sh3Hot.pr;
static UINT32 &m_sr=sh3Hot.sr;
static UINT32 &m_ssr=sh3Hot.ssr;
static UINT32 &m_gbr=sh3Hot.gbr;
static UINT32 &m_vbr=sh3Hot.vbr;
static UINT32 &m_mach=sh3Hot.mach;
static UINT32 &m_macl=sh3Hot.macl;
static UINT32  m_rbnk[2][8];
static UINT32  m_sgr;
static UINT32  m_fr[16];
static UINT32  m_xf[16];
static UINT32 &m_ea=sh3Hot.ea;
static UINT32 &m_delay=sh3Hot.delay;
static UINT32 &m_cpu_off=sh3Hot.cpu_off;
static UINT32 &m_pending_irq=sh3Hot.pending_irq;
static UINT32 &m_test_irq=sh3Hot.test_irq;
static UINT32  m_fpscr;
static UINT32  m_fpul;
static UINT32  m_dbr;

static UINT32  m_exception_priority[128];
static int     m_exception_requesting[128];

static UINT32  m_irq_line_state[17];

// sh4 internal
static UINT32 m_m[16384];

// timer regs handled manually for reuse
static UINT32 m_SH4_TSTR;
static UINT32 m_SH4_TCNT0;
static UINT32 m_SH4_TCNT1;
static UINT32 m_SH4_TCNT2;
static UINT32 m_SH4_TCR0;
static UINT32 m_SH4_TCR1;
static UINT32 m_SH4_TCR2;
static UINT32 m_SH4_TCOR0;
static UINT32 m_SH4_TCOR1;
static UINT32 m_SH4_TCOR2;
static UINT32 m_SH4_TOCR;
static UINT32 m_SH4_TCPR2;

// INTC regs
static UINT32 m_SH4_IPRA;

static UINT32 m_SH4_IPRC;

// DMAC regs
static UINT32 m_SH4_SAR0;
static UINT32 m_SH4_SAR1;
static UINT32 m_SH4_SAR2;
static UINT32 m_SH4_SAR3;

static UINT32 m_SH4_DAR0;
static UINT32 m_SH4_DAR1;
static UINT32 m_SH4_DAR2;
static UINT32 m_SH4_DAR3;

static UINT32 m_SH4_CHCR0;
static UINT32 m_SH4_CHCR1;
static UINT32 m_SH4_CHCR2;
static UINT32 m_SH4_CHCR3;

static UINT32 m_SH4_DMATCR0;
static UINT32 m_SH4_DMATCR1;
static UINT32 m_SH4_DMATCR2;
static UINT32 m_SH4_DMATCR3;

static UINT32 m_SH4_DMAOR;

static UINT32  m_nmi_line_state;

static UINT32  m_sleep_mode;

static int     m_frt_input;
static int     m_irln;
static int     m_internal_irq_level;
static int     m_internal_irq_vector;

/*emu_timer *m_dma_timer[4];
emu_timer *m_refresh_timer;
emu_timer *m_rtc_timer;
emu_timer *m_timer[3];*/

static UINT32  m_refresh_timer_base;
static int     m_dma_timer_active[4];
static UINT32  m_dma_source[4];
static UINT32  m_dma_destination[4];
static UINT32  m_dma_count[4];
static int     m_dma_wordsize[4];
static int     m_dma_source_increment[4];
static int     m_dma_destination_increment[4];
static int     m_dma_mode[4];

static int     m_is_slave;
static int     m_cpu_clock;
static int     m_bus_clock;
static int     m_pm_clock;

static int	   m_pm_divider; // clock divider of timer unit

// for scaling up to the prescaler of timer unit when using cpu rate selection
// see notes in Sh3SetClockCV1k
#define ratio_multi 100000 // float -> int, format ii.fffff  (for speed)
static int     m_ratio;
static int     m_ratio_rev;

static int     m_fpu_sz;
static int     m_fpu_pr;
static int     m_ioport16_pullup;
static int     m_ioport16_direction;
static int     m_ioport4_pullup;
static int     m_ioport4_direction;

//static void    (*m_ftcsr_read_callback)(UINT32 data);

/* This MMU simulation is good for the simple remap used on Naomi GD-ROM SQ access *ONLY* */
static UINT32  m_sh4_tlb_address[64];
static UINT32  m_sh4_tlb_data[64];
static UINT8   m_sh4_mmu_enabled;

static int     m_cpu_type;

// sh3 internal
static UINT32  m_sh3internal_upper[0x3000/4];
static UINT32  m_sh3internal_lower[0x1000];


static INT32   sh3_end_run;

static UINT32  area_last = 0;
static UINT32  area_cur = 0;
static UINT32  area_write = 0;

// Wait state config (WCR2 -> AreaWS[7]) - see sh3comn.c
#define EAT(c) { m_sh4_icount -= c; sh3_total_cycles += c; }
static INT32  AreaWS1[7];
static INT32  AreaWS2[7];
//extern int counter;
#define SH3_WAIT_STATE_ACTIVE 0
#if SH3_WAIT_STATE_ACTIVE
static inline void WaitState(UINT32 a, UINT32 is_write) {
	// causes too much game-lag, perhaps I messed up the impl?
	extern int counter; // enabled via-debug counter (m/n keys in fbneo)
	if (counter) {
		area_cur = a & 0xff000000;
		if (area_cur != area_last || area_write != is_write) {
			EAT(AreaWS1[a >> (24+2)]);
		}
		area_last = area_cur;
		area_write = is_write;
	}
}
#else
#define WaitState
#endif

// logging stuff
static TCHAR *tag() { static TCHAR id[10] = _T("sh-3\0"); return &id[0]; }
#define logerror //(s, ...) bprintf(0, _T(s), __VA_ARGS__);
#define fatalerror(s, ...) bprintf(0, _T(s), __VA_ARGS__);
#define INPUT_LINE_NMI 0x20
#define CLEAR_LINE 0
#ifdef LSB_FIRST
#define NATIVE_ENDIAN_VALUE_LE_BE(le, be) (le)
#else
#define NATIVE_ENDIAN_VALUE_LE_BE(le, be) (be)
#endif

// timer granularity speedhack
static INT32 timer_granularity = 0;
static bool cv1kTimerFastEnabled=false;

void Sh3SetTimerGranularity(INT32 timergransh)
{
	timer_granularity = timergransh;
}

//-- timer jibbajabba --------------------+1
// simple timer system -dink 2019, v2 (2022-upgreydde ver.)
struct sh4_dtimer
{
	INT32 running;
	UINT32 time_trig;
	UINT32 time_current;
	INT32 timer_param;
	INT32 timer_prescaler;
	UINT32 prescale_counter;
	INT32 retrig;
	void (*timer_exec)(int);

	void scan() {
		SCAN_VAR(running);
		SCAN_VAR(time_trig);
		SCAN_VAR(time_current);
		SCAN_VAR(timer_param);
		SCAN_VAR(timer_prescaler);
		SCAN_VAR(prescale_counter);
		SCAN_VAR(retrig);
	}

	void start(UINT32 trig_at, INT32 tparam, INT32 run_now, INT32 retrigger) {
		running = run_now;
		if (tparam != -1) timer_param = tparam;
		time_trig = trig_at;
		time_current = 0;
		//prescale_counter = 0; <-- not here!
		retrig = retrigger;
		//if (counter) bprintf(0, _T("timer %d START:  %d  cycles - running: %d\n"), timer_param, time_trig, running);
		//if (counter) bprintf(0, _T("timer %d   running %d - timeleft  %d  time_trig %d  time_current %d\n"), timer_param, running, time_trig - time_current, time_trig, time_current);
	}

	void init(INT32 tparam, void (*callback)(int)) {
		config(tparam, callback);
		reset();
	}

	void config(INT32 tparam, void (*callback)(int)) {
		timer_param = tparam;
		timer_exec = callback;
		timer_prescaler = 1 * ratio_multi;
	}

	void set_prescaler(INT32 prescale) {
		timer_prescaler = prescale;
	}

	void run_prescale(INT32 cyc) {
		prescale_counter += cyc * m_ratio;
		if (timer_prescaler > 0 && prescale_counter / 4 >= (UINT32)timer_prescaler) {
			const UINT32 ticks = prescale_counter / (UINT32)timer_prescaler;
			if (!running) {
				prescale_counter %= (UINT32)timer_prescaler;
				return;
			}
			if (time_current < time_trig && ticks < time_trig - time_current) {
				time_current += ticks;
				prescale_counter %= (UINT32)timer_prescaler;
				return;
			}
		}
		while (prescale_counter >= timer_prescaler) {
			prescale_counter -= timer_prescaler;

			// note: we can't optimize this, f.ex:
			//run(prescale_counter / timer_prescaler); prescale_counter %= timer_prescaler;
			// why? when the timer hits, the prescaler can & will change.

			// note2:
			//run(1);  this is just the contents of run(1) from below
			if (running) {
				time_current += 1;

				if (time_current >= time_trig) { // should be while (retrig needs this, not used by sh4)
					//if (counter) bprintf(0, _T("timer %d hits @ %d\n"), timer_param, time_current);

					if (retrig == 0) {
						running = 0;
						//time_trig = -1;
						//stop();
						//break;
					}
					if (timer_exec) {
						timer_exec(timer_param); // NOTE: this cb _might_ re-start/init the timer!
					}
					//time_current -= time_trig;
				}
			}
		}
	}

	// O(1) advance when the next prescaled timer event cannot fire.
	// Fall back BEFORE mutating anything if an expiry could invoke a
	// callback (which may restart or change this or another timer).
	void run_prescale_fast(INT32 cyc) {
		if(timer_prescaler<=0){run_prescale(cyc);return;}
		const UINT32 p=UINT32(timer_prescaler);
		const UINT32 next=prescale_counter+UINT32(cyc)*UINT32(m_ratio);
		if(next<p){prescale_counter=next;return;}
		UINT32 ticks=1,rem=next-p;
		if(rem>=p){ticks=next/p;rem=next%p;}
		if(running && (time_current>=time_trig || ticks>=time_trig-time_current)){
			run_prescale(cyc);return;
		}
		if(running)time_current+=ticks;
		prescale_counter=rem;
	}

	void run(INT32 cyc) {
		if (running) {
			time_current += cyc;

			if (time_current >= time_trig) { // should be while (retrig needs this, not used by sh4)
				//if (counter) bprintf(0, _T("timer %d hits @ %d\n"), timer_param, time_current);

				if (retrig == 0) {
					running = 0;
					//time_trig = -1;
					//stop();
					//break;
				}
				if (timer_exec) {
					timer_exec(timer_param); // NOTE: this cb _might_ re-start/init the timer!
				}
				//time_current -= time_trig;
			}
		}
	}

	void reset() {
		stop();
		prescale_counter = 0; // this must be free-running (only reset here!)
	}
	void stop() {
		running = 0;
		time_current = 0;
		timer_param = 0;
	}
	INT32 isrunning() {
		return running;
	}
	UINT32 timeleft() {
		return time_trig - time_current;
	}
};

static sh4_dtimer m_timer[3];
static sh4_dtimer m_dma_timer[4];
static sh4_dtimer cave_blitter_delay;

static void sh4_run_timers(INT32 cycles) {
	if(cv1kTimerFastEnabled && timer_granularity){
		m_timer[0].run_prescale_fast(cycles);
		m_timer[1].run_prescale_fast(cycles);
		m_timer[2].run_prescale_fast(cycles);
	}else{
		m_timer[0].run_prescale(cycles);
		m_timer[1].run_prescale(cycles);
		m_timer[2].run_prescale(cycles);
	}

	m_dma_timer[0].run(cycles);
	m_dma_timer[1].run(cycles);
	m_dma_timer[2].run(cycles);
	m_dma_timer[3].run(cycles);

	cave_blitter_delay.run(cycles);
}

static void (*pCaveBlitterDelayFunc)(int);

void sh4_set_cave_blitter_delay_func(void (*pfunc)(int))
{
	pCaveBlitterDelayFunc = pfunc;
}

void sh4_set_cave_blitter_delay_timer(int cycles)
{
	cave_blitter_delay.start(cycles, -1, 1, 0);
}

static void cave_blitter_delay_func(int param)
{
	if (pCaveBlitterDelayFunc) {
		pCaveBlitterDelayFunc(0);
	}
}

void Sh3SetClockCV1k(INT32 clock)
{
	c_clock = clock;
	bprintf(0, _T("Sh3SetClockCV1k:  %d   tmu prescale %d\n"), c_clock, m_pm_divider);

	// cpu rate change
	// scale-up using integer representing floating point (ii.fffff, 1 = 100000 (ratio_multi))
	//m_ratio_needed = m_pm_divider * ratio_multi;
	m_ratio = ((double)(12800000 * 8) / clock) * ratio_multi;
	m_ratio_rev = ((double)clock / (12800000 * 8)) * ratio_multi;
}

INT32 sh4_get_cpu_speed() {
	return c_clock;
}

static void Cv1kV2InvalidateAll();

INT32 Sh3Scan(INT32 nAction)
{
	SCAN_VAR(m_ppc);
	SCAN_VAR(m_pc);
	SCAN_VAR(m_spc);
	SCAN_VAR(m_pr);
	SCAN_VAR(m_sr);
	SCAN_VAR(m_ssr);
	SCAN_VAR(m_gbr);
	SCAN_VAR(m_vbr);
	SCAN_VAR(m_mach);
	SCAN_VAR(m_macl);
	SCAN_VAR(m_r);
	SCAN_VAR(m_rbnk);
	SCAN_VAR(m_sgr);
	SCAN_VAR(m_fr);
	SCAN_VAR(m_xf);
	SCAN_VAR(m_ea);
	SCAN_VAR(m_delay);
	SCAN_VAR(m_cpu_off);
	SCAN_VAR(m_pending_irq);
	SCAN_VAR(m_test_irq);
	SCAN_VAR(m_fpscr);
	SCAN_VAR(m_fpul);
	SCAN_VAR(m_dbr);

	SCAN_VAR(m_exception_priority);
	SCAN_VAR(m_exception_requesting);

	SCAN_VAR(m_irq_line_state);

	SCAN_VAR(m_m);

	SCAN_VAR(m_SH4_TSTR);
	SCAN_VAR(m_SH4_TCNT0);
	SCAN_VAR(m_SH4_TCNT1);
	SCAN_VAR(m_SH4_TCNT2);
	SCAN_VAR(m_SH4_TCR0);
	SCAN_VAR(m_SH4_TCR1);
	SCAN_VAR(m_SH4_TCR2);
	SCAN_VAR(m_SH4_TCOR0);
	SCAN_VAR(m_SH4_TCOR1);
	SCAN_VAR(m_SH4_TCOR2);
	SCAN_VAR(m_SH4_TOCR);
	SCAN_VAR(m_SH4_TCPR2);

	SCAN_VAR(m_SH4_IPRA);

	SCAN_VAR(m_SH4_IPRC);


	SCAN_VAR(m_SH4_SAR0);
	SCAN_VAR(m_SH4_SAR1);
	SCAN_VAR(m_SH4_SAR2);
	SCAN_VAR(m_SH4_SAR3);

	SCAN_VAR(m_SH4_DAR0);
	SCAN_VAR(m_SH4_DAR1);
	SCAN_VAR(m_SH4_DAR2);
	SCAN_VAR(m_SH4_DAR3);

	SCAN_VAR(m_SH4_CHCR0);
	SCAN_VAR(m_SH4_CHCR1);
	SCAN_VAR(m_SH4_CHCR2);
	SCAN_VAR(m_SH4_CHCR3);

	SCAN_VAR(m_SH4_DMATCR0);
	SCAN_VAR(m_SH4_DMATCR1);
	SCAN_VAR(m_SH4_DMATCR2);
	SCAN_VAR(m_SH4_DMATCR3);

	SCAN_VAR(m_SH4_DMAOR);

	SCAN_VAR(m_nmi_line_state);

	SCAN_VAR(m_sleep_mode);

	SCAN_VAR(m_frt_input);
	SCAN_VAR(m_irln);
	SCAN_VAR(m_internal_irq_level);
	SCAN_VAR(m_internal_irq_vector);

	SCAN_VAR(m_refresh_timer_base);
	SCAN_VAR(m_dma_timer_active);
	SCAN_VAR(m_dma_source);
	SCAN_VAR(m_dma_destination);
	SCAN_VAR(m_dma_count);
	SCAN_VAR(m_dma_wordsize);
	SCAN_VAR(m_dma_source_increment);
	SCAN_VAR(m_dma_destination_increment);
	SCAN_VAR(m_dma_mode);

	SCAN_VAR(m_sh4_icount);
	SCAN_VAR(m_is_slave);
	SCAN_VAR(m_cpu_clock);
	SCAN_VAR(m_bus_clock);
	SCAN_VAR(m_pm_clock);

	SCAN_VAR(m_pm_divider); // timer clock prescaler
	SCAN_VAR(m_ratio);

	SCAN_VAR(m_fpu_sz);
	SCAN_VAR(m_fpu_pr);
	SCAN_VAR(m_ioport16_pullup);
	SCAN_VAR(m_ioport16_direction);
	SCAN_VAR(m_ioport4_pullup);
	SCAN_VAR(m_ioport4_direction);

	SCAN_VAR(m_sh4_tlb_address);
	SCAN_VAR(m_sh4_tlb_data);
	SCAN_VAR(m_sh4_mmu_enabled);

	SCAN_VAR(m_sh3internal_upper);
	SCAN_VAR(m_sh3internal_lower);

	SCAN_VAR(sh3_total_cycles);

	SCAN_VAR(sh3_end_run);

	SCAN_VAR(AreaWS1);
	SCAN_VAR(AreaWS2);
	SCAN_VAR(area_last);
	SCAN_VAR(area_write);

	m_timer[0].scan();
	m_timer[1].scan();
	m_timer[2].scan();

	m_dma_timer[0].scan();
	m_dma_timer[1].scan();
	m_dma_timer[2].scan();
	m_dma_timer[3].scan();

	cave_blitter_delay.scan();
	if(nAction&ACB_WRITE)Cv1kV2InvalidateAll();

	return 0;
}

//-- sh3 memory handler for Finalburn Neo ---------------------

#define SH3_BITS		(16)					// 16 = 0x10000 page size
#define SH3_PAGE_COUNT  (1 << (32 - SH3_BITS))	// Number of pages
#define SH3_SHIFT		(SH3_BITS)				// Shift value = page bits
#define SH3_PAGE_SIZE	(1 << SH3_BITS)			// Page size
#define SH3_PAGEM		(SH3_PAGE_SIZE - 1)
#define SH3_WADD		(SH3_PAGE_COUNT)		// Value to add for write section = Number of pages
#define SH3_MASK		(SH3_WADD - 1)

#define	SH3_MAXHANDLER	(8)

static UINT8 *MemMapR[SH3_PAGE_COUNT];
static UINT8 *MemMapRL[SH3_PAGE_COUNT];
static UINT8 *MemMapW[SH3_PAGE_COUNT];
static UINT8 *MemMapF[SH3_PAGE_COUNT];
#include "sh3_cv1k_write_watch.inc"
// Fetch pointers may survive a slow instruction only when no mapping
// API has changed the fetch map. All code words are still read live.
static UINT32 sh3FetchMapEpoch;
static pSh3ReadByteHandler ReadByte[SH3_MAXHANDLER];
static pSh3WriteByteHandler WriteByte[SH3_MAXHANDLER];
static pSh3ReadWordHandler ReadWord[SH3_MAXHANDLER];
static pSh3WriteWordHandler WriteWord[SH3_MAXHANDLER];
static pSh3ReadLongHandler ReadLong[SH3_MAXHANDLER];
static pSh3WriteLongHandler WriteLong[SH3_MAXHANDLER];
// CV1000's long-only idle handler uses aligned reads from this RAM page.
// Inline only the explicitly registered handler; all other handler maps
// retain the generic path, including tests and future drivers.
static UINT8 *cv1kIdlePage;
static UINT32 cv1kIdleAddress, cv1kIdlePC;
static const INT32 *cv1kIdleCycles;
static pSh3ReadLongHandler cv1kIdleHandler;
// Registered only after checking every page against the actual maps.
// Mapping or handler changes revoke this authorization before mutation.
static UINT8 *cv1kFastRamBase,*cv1kFastRomBase;
static UINT32 cv1kFastRamMask,cv1kFastIdleAddress;
static bool cv1kIdleBatchEnabled;
static void Cv1kInvalidateFastMemory()
{
	cv1kFastRamBase=cv1kFastRomBase=NULL;
	cv1kFastRamMask=0;cv1kFastIdleAddress=~UINT32(0);
}
static bool Cv1kIsIdlePage(UINT8 *page)
{
	return (uintptr_t)page==1 && cv1kIdlePage && cv1kIdleCycles &&
		MemMapR[0xc00]==cv1kIdlePage && ReadLong[1]==cv1kIdleHandler;
}
// CV1000 Type B/C mirrors 8 MiB across 16 MiB; Type D has 16 MiB.
// Do not infer these mappings from the guest address alone.
void Sh3SetCv1kCacheRAM(UINT8 *ram,UINT32 size)
{
	Cv1kV2TrustScratch(ram,size);
}
void Sh3SetCv1kFastMemory(UINT8 *ram,UINT32 ramSize,UINT8 *rom)
{
	Cv1kInvalidateFastMemory();
	Cv1kV2TrustMemory(ram,ramSize,rom);
	// Full region shortcuts regressed on the RK3566. Keep them opt-in,
	// independently of the first-page aligned long-load rescue.
	const bool requested=Cv1kPerformanceOption(getenv("FBNEO_CV1K_FAST_RAM"),false);
	cv1kIdleBatchEnabled=!SH3_WAIT_STATE_ACTIVE &&
		Cv1kPerformanceOption(getenv("FBNEO_CV1K_IDLE_BATCH"));
	bool ramOK=requested && !SH3_WAIT_STATE_ACTIVE && ram &&
		(ramSize==0x800000 || ramSize==0x1000000);
	UINT32 idle=~UINT32(0);
	if(ramOK)for(UINT32 off=0;off<0x1000000;off+=SH3_PAGE_SIZE){
		const UINT32 idx=(0xc000000+off)>>SH3_SHIFT;
		UINT8 *p=ram+(off&(ramSize-1));
		if(MemMapR[idx]!=p || MemMapW[idx]!=p){ramOK=false;break;}
		if(MemMapRL[idx]!=p){
			if(idx!=0xc00 || cv1kIdlePage!=ram || !Cv1kIsIdlePage(MemMapRL[idx])){
				ramOK=false;break;
			}
			idle=cv1kIdleAddress;
		}
	}
	if(ramOK){cv1kFastRamBase=ram;cv1kFastRamMask=ramSize-1;cv1kFastIdleAddress=idle;}
	bool romOK=requested && !SH3_WAIT_STATE_ACTIVE && rom;
	if(romOK)for(UINT32 off=0;off<0x400000;off+=SH3_PAGE_SIZE){
		const UINT32 idx=off>>SH3_SHIFT;
		if(MemMapR[idx]!=rom+off || MemMapRL[idx]!=rom+off){romOK=false;break;}
	}
	if(romOK)cv1kFastRomBase=rom;
	bprintf(PRINT_UI,_T("CV1000 SH3 validated direct RAM: %s, ROM: %s, idle exception: %08x\n"),
		ramOK?_T("enabled"):_T("disabled"),romOK?_T("enabled"):_T("disabled"),cv1kFastIdleAddress);
	bprintf(PRINT_UI,_T("CV1000 SH3 batched idle-page rescue: %s\n"),
		(cv1kIdleBatchEnabled && Cv1kIsIdlePage(MemMapRL[0xc00]))?_T("enabled"):_T("disabled"));
}
static pSh3ReadLongHandler cv1kPollReadHandler;
static pSh3Cv1kPollPeek cv1kPollPeek;
void Sh3SetCv1kPollRead(pSh3ReadLongHandler handler,pSh3Cv1kPollPeek peek)
{
	cv1kPollReadHandler=handler;cv1kPollPeek=peek;
}
unsigned Sh3Cv1kPeekCode(UINT32 pc,UINT16 *ops,unsigned count)
{
	unsigned n=0;
	if(pc&1)return 0;
	for(;n<count;n++,pc+=2){
		if(pc>=0xe0000000)break;
		const UINT32 a=pc&AM;
		UINT8 *page=MemMapF[a>>SH3_SHIFT];
		if((uintptr_t)page<SH3_MAXHANDLER)break;
		memcpy(ops+n,page+(a&SH3_PAGEM),2);
	}
	return n;
}


void Sh3SetCv1kIdleRead(UINT8 *ram, UINT32 addr, UINT32 pc,
	const INT32 *burn, pSh3ReadLongHandler handler)
{
	Cv1kInvalidateFastMemory();
	const char *option=getenv("FBNEO_CV1K_IDLE_DIRECT");
	cv1kIdlePage=(ram && burn && handler && (!option || strcmp(option,"0")!=0))?ram:NULL;
	cv1kIdleAddress=addr;
	cv1kIdlePC=pc;
	cv1kIdleCycles=burn;
	cv1kIdleHandler=handler;
	if (ram) bprintf(PRINT_UI,_T("CV1000 SH3 direct idle-page reads: %s\n"),
		cv1kIdlePage?_T("enabled"):_T("disabled"));
}


static pSh3ReadLongHandler ReadPort;
static pSh3WriteLongHandler WritePort;

static UINT32 __fastcall Sh3DummyReadLong(UINT32) { return 0; }
static void __fastcall Sh3DummyWriteLong(UINT32, UINT32) { }

static INT32 Sh3MapInit()
{
	//bprintf(0, _T("Init Sh3 MEMMAP\n"));
	memset(MemMapR, 0, sizeof(MemMapR));
	memset(MemMapRL, 0, sizeof(MemMapRL));
	memset(MemMapW, 0, sizeof(MemMapW));
	memset(MemMapF, 0, sizeof(MemMapF));
	for(unsigned i=0;i<SH3_MAXHANDLER;i++)Cv1kV2SetTracked(i,false);
	Cv1kV2WriteMapChanged();
	++sh3FetchMapEpoch;
	Cv1kInvalidateFastMemory();
	cv1kIdlePage=NULL;cv1kIdleBatchEnabled=false;
	Sh3SetCv1kPollRead(NULL,NULL);

	return 0;
}

INT32 Sh3MapMemory(UINT8* pMemory, UINT32 nStart, UINT32 nEnd, INT32 nType)
{
	Cv1kInvalidateFastMemory();
	if (nType & MAP_FETCHOP) ++sh3FetchMapEpoch;
	UINT8* Ptr = pMemory - nStart;
	UINT8** pMemMapR = MemMapR + (nStart >> SH3_SHIFT);
	UINT8** pMemMapRL = MemMapRL + (nStart >> SH3_SHIFT);
	UINT8** pMemMapW = MemMapW + (nStart >> SH3_SHIFT);
	UINT8** pMemMapF = MemMapF + (nStart >> SH3_SHIFT);

	for (UINT64 i = (nStart & ~SH3_PAGEM); i <= nEnd; i += SH3_PAGE_SIZE, pMemMapR++, pMemMapRL++, pMemMapW++, pMemMapF++) {
		if (nType & MAP_READ) {
			pMemMapR[0] = Ptr + i;
			pMemMapRL[0] = Ptr + i;
		}
		if (nType & MAP_WRITE)	  pMemMapW[0] = Ptr + i;
		if (nType & MAP_FETCHOP)  pMemMapF[0] = Ptr + i;
	}
	Cv1kV2WriteMapChanged();

	return 0;
}

INT32 Sh3MapHandler(uintptr_t nHandler, UINT32 nStart, UINT32 nEnd, INT32 nType)
{
	Cv1kInvalidateFastMemory();
	if (nType & MAP_FETCHOP) ++sh3FetchMapEpoch;
	UINT8** pMemMapR = MemMapR + (nStart >> SH3_SHIFT);
	UINT8** pMemMapRL = MemMapRL + (nStart >> SH3_SHIFT);
	UINT8** pMemMapW = MemMapW + (nStart >> SH3_SHIFT);
	UINT8** pMemMapF = MemMapF + (nStart >> SH3_SHIFT);

	for (UINT64 i = (nStart & ~SH3_PAGEM); i <= nEnd; i += SH3_PAGE_SIZE, pMemMapR++, pMemMapRL++, pMemMapW++, pMemMapF++) {
		if (nType & MAP_READ) {
			pMemMapR[0] = (UINT8*)nHandler;
			pMemMapRL[0] = (UINT8*)nHandler;
		}
		if (nType & MAP_WRITE)	 pMemMapW[0] = (UINT8*)nHandler;
		if (nType & MAP_FETCHOP) pMemMapF[0] = (UINT8*)nHandler;
	}
	Cv1kV2WriteMapChanged();
	return 0;
}

INT32 Sh3MapReadLongHandler(uintptr_t nHandler, UINT32 nStart, UINT32 nEnd)
{
	Cv1kInvalidateFastMemory();
	UINT8** pMemMapRL = MemMapRL + (nStart >> SH3_SHIFT);

	for (UINT64 i = (nStart & ~SH3_PAGEM); i <= nEnd; i += SH3_PAGE_SIZE, pMemMapRL++) {
		pMemMapRL[0] = (UINT8*)nHandler;
	}
	Cv1kV2WriteMapChanged();

	return 0;
}

INT32 Sh3SetReadPortHandler(pSh3ReadLongHandler pHandler)
{
	ReadPort = pHandler;
	Cv1kV2RevokeTrust();
	return 0;
}

INT32 Sh3SetWritePortHandler(pSh3WriteLongHandler pHandler)
{
	WritePort = pHandler;
	Cv1kV2RevokeTrust();
	Cv1kV2SetTracked(SH3_MAXHANDLER-1,false);
	Cv1kV2SetTracked(SH3_MAXHANDLER-2,false);
	return 0;
}

INT32 Sh3SetReadByteHandler(INT32 i, pSh3ReadByteHandler pHandler)
{
	if (i >= SH3_MAXHANDLER) {
		bprintf(PRINT_ERROR, _T("Sh3SetReadByteHandler called with invalid index %x\n"), i);
		return 1;
	}

	ReadByte[i] = pHandler;
	Cv1kV2RevokeTrust();
	return 0;
}

INT32 Sh3SetWriteByteHandler(INT32 i, pSh3WriteByteHandler pHandler)
{
	if (i >= SH3_MAXHANDLER) {
		bprintf(PRINT_ERROR, _T("Sh3SetWriteByteHandler called with invalid index %x\n"), i);
		return 1;
	}

	Cv1kV2SetTracked(i,false);
	WriteByte[i] = pHandler;
	Cv1kV2RevokeTrust();
	return 0;
}

INT32 Sh3SetReadWordHandler(INT32 i, pSh3ReadWordHandler pHandler)
{
	if (i >= SH3_MAXHANDLER) {
		bprintf(PRINT_ERROR, _T("Sh3SetReadWordHandler called with invalid index %x\n"), i);
		return 1;
	}

	ReadWord[i] = pHandler;
	Cv1kV2RevokeTrust();
	return 0;
}

INT32 Sh3SetWriteWordHandler(INT32 i, pSh3WriteWordHandler pHandler)
{
	if (i >= SH3_MAXHANDLER) {
		bprintf(PRINT_ERROR, _T("Sh3SetWriteWordHandler called with invalid index %x\n"), i);
		return 1;
	}

	Cv1kV2SetTracked(i,false);
	WriteWord[i] = pHandler;
	Cv1kV2RevokeTrust();
	return 0;
}

INT32 Sh3SetReadLongHandler(INT32 i, pSh3ReadLongHandler pHandler)
{
	if (i >= SH3_MAXHANDLER) {
		bprintf(PRINT_ERROR, _T("Sh3SetReadLongHandler called with invalid index %x\n"), i);
		return 1;
	}

	Cv1kInvalidateFastMemory();
	ReadLong[i] = pHandler;
	Cv1kV2RevokeTrust();
	return 0;
}

INT32 Sh3SetWriteLongHandler(INT32 i, pSh3WriteLongHandler pHandler)
{
	if (i >= SH3_MAXHANDLER) {
		bprintf(PRINT_ERROR, _T("Sh3SetWriteLongHandler called with invalid index %x\n"), i);
		return 1;
	}

	Cv1kV2SetTracked(i,false);
	WriteLong[i] = pHandler;
	Cv1kV2RevokeTrust();
	return 0;
}

UINT32 Sh3GetPC(int)
{
	return (m_delay) ? (m_delay & AM) : (m_pc & AM);
}

void Sh3BurnUntilInt()
{
	m_cpu_off = 1;
}

void Sh3RunEnd()
{
	bprintf(PRINT_ERROR, _T("Sh3RunEnd() disabled (for speed)\n"));
	sh3_end_run = 1;
}

INT32 Sh3TotalCycles()
{
	return sh3_total_cycles;
}

void Sh3NewFrame()
{
	sh3_total_cycles = 0;
}

void Sh3BurnCycles(INT32 cycles)
{
	m_sh4_icount -= cycles;
	sh3_total_cycles += cycles;
}

INT32 Sh3Idle(INT32 cycles)
{
	m_sh4_icount -= cycles;
	sh3_total_cycles += cycles;

	return cycles;
}

static UINT8 __fastcall Sh3LowerReadByte(UINT32 a)
{
	return sh3_internal_r((a - SH3_LOWER_REGBASE)>>2, (0xff << (((~a) & 3)*8))) >> (((~a) & 3)*8);
}

static UINT16 __fastcall Sh3LowerReadWord(UINT32 a)
{
	return sh3_internal_r((a - SH3_LOWER_REGBASE)>>2, (0xffff << (((~a) & 2)*8))) >> (((~a) & 2)*8);
}

static UINT32 __fastcall Sh3LowerReadLong(UINT32 a)
{
	return sh3_internal_r((a - SH3_LOWER_REGBASE)>>2, 0xffffffff);
}

static void __fastcall Sh3LowerWriteByte(UINT32 a, UINT8 d)
{
	sh3_internal_w((a - SH3_LOWER_REGBASE)>>2, d << (((~a) & 3)*8), (0xff << (((~a) & 3)*8)));
}

static void __fastcall Sh3LowerWriteWord(UINT32 a, UINT16 d)
{
	sh3_internal_w((a - SH3_LOWER_REGBASE)>>2, d << (((~a) & 2)*8), (0xffff << (((~a) & 2)*8)));
}

static void __fastcall Sh3LowerWriteLong(UINT32 a, UINT32 d)
{
	sh3_internal_w((a - SH3_LOWER_REGBASE)>>2, d, 0xffffffff);
}

static UINT8 __fastcall Sh3UpperReadByte(UINT32 a)
{
	return sh3_internal_high_r((a - SH3_UPPER_REGBASE)>>2, (0xff << (((~a) & 3)*8))) >> (((~a) & 3)*8);
}

static UINT16 __fastcall Sh3UpperReadWord(UINT32 a)
{
	return sh3_internal_high_r((a - SH3_UPPER_REGBASE)>>2, (0xffff << (((~a) & 2)*8))) >> (((~a) & 2)*8);
}

static UINT32 __fastcall Sh3UpperReadLong(UINT32 a)
{
	return sh3_internal_high_r((a - SH3_UPPER_REGBASE)>>2, 0xffffffff);
}

static void __fastcall Sh3UpperWriteByte(UINT32 a, UINT8 d)
{
	sh3_internal_high_w((a - SH3_UPPER_REGBASE)>>2, d << (((~a) & 3)*8), (0xff << (((~a) & 3)*8)));
}

static void __fastcall Sh3UpperWriteWord(UINT32 a, UINT16 d)
{
	sh3_internal_high_w((a - SH3_UPPER_REGBASE)>>2, d << (((~a) & 2)*8), (0xffff << (((~a) & 2)*8)));
}

static void __fastcall Sh3UpperWriteLong(UINT32 a, UINT32 d)
{
	sh3_internal_high_w((a - SH3_UPPER_REGBASE)>>2, d, 0xffffffff);
}

// Keep direct mapped memory in the instruction handler on Cortex-A55.
// The bodies (including handler/idle-page semantics) are shared unchanged.
// Build with -DSH3_INLINE_MEMORY=0 for the compiler's original heuristic.
#ifndef SH3_INLINE_MEMORY
#define SH3_INLINE_MEMORY 1
#endif
#if SH3_INLINE_MEMORY && defined(__GNUC__) && (defined(__aarch64__) || defined(SH3_INTERPRETER_TEST))
#define SH3_MEMORY_INLINE static inline __attribute__((always_inline))
#else
#define SH3_MEMORY_INLINE static inline
#endif
SH3_MEMORY_INLINE void WB(UINT32 A, UINT8 V); // forward
SH3_MEMORY_INLINE UINT8 RB(UINT32 A); // forward

static void cheat_write(UINT32 a, UINT8 d)
{
	WB(a, d);
}

static UINT8 cheat_read(UINT32 a)
{
	return RB(a);
}

void Sh3CheatSetIRQLine(INT32 cpunum, INT32 line, INT32 state)
{
	Sh3SetIRQLine(line, state);
}

cpu_core_config sh3Config =
{
	"sh3",
	Sh3Open,
	Sh3Close,
	cheat_read,
	cheat_write,
	Sh3GetActive,
	Sh3TotalCycles,
	Sh3NewFrame,
	Sh3Idle,
	Sh3CheatSetIRQLine,
	Sh3Run,
	Sh3RunEnd,
	Sh3Reset,
	Sh3Scan,
	Sh3Exit,
	0x1000000,
	0
};

void Sh3Init(INT32 num, INT32 hz, char md0, char md1, char md2, char md3, char md4, char md5, char md6, char md7, char md8 )
{
	if (num != 0) {
		bprintf(0, _T("--- Sh3Init(): only supports single cpu.\n"));
		return;
	}

	bprintf(0, _T("--  sh3 init @ %dhz\n"), hz);

	c_clock = hz;

	m_cpu_type = CPU_TYPE_SH3;

	c_md0 = md0; c_md1 = md1; c_md2 = md2; c_md3 = md3;
	c_md4 = md4; c_md5 = md5; c_md6 = md6; c_md7 = md7; c_md8 = md8;

	Sh3SetReadPortHandler(Sh3DummyReadLong);
	Sh3SetWritePortHandler(Sh3DummyWriteLong);

	Sh3MapInit();

	Sh3MapHandler(SH3_MAXHANDLER - 1, SH3_LOWER_REGBASE, SH3_LOWER_REGEND, MAP_READ | MAP_WRITE);
	Sh3MapHandler(SH3_MAXHANDLER - 2, SH3_UPPER_REGBASE, SH3_UPPER_REGEND, MAP_READ | MAP_WRITE);

	Sh3SetReadByteHandler (SH3_MAXHANDLER - 1, Sh3LowerReadByte);
	Sh3SetReadWordHandler (SH3_MAXHANDLER - 1, Sh3LowerReadWord);
	Sh3SetReadLongHandler (SH3_MAXHANDLER - 1, Sh3LowerReadLong);
	Sh3SetWriteByteHandler(SH3_MAXHANDLER - 1, Sh3LowerWriteByte);
	Sh3SetWriteWordHandler(SH3_MAXHANDLER - 1, Sh3LowerWriteWord);
	Sh3SetWriteLongHandler(SH3_MAXHANDLER - 1, Sh3LowerWriteLong);

	Sh3SetReadByteHandler (SH3_MAXHANDLER - 2, Sh3UpperReadByte);
	Sh3SetReadWordHandler (SH3_MAXHANDLER - 2, Sh3UpperReadWord);
	Sh3SetReadLongHandler (SH3_MAXHANDLER - 2, Sh3UpperReadLong);
	Sh3SetWriteByteHandler(SH3_MAXHANDLER - 2, Sh3UpperWriteByte);
	Sh3SetWriteWordHandler(SH3_MAXHANDLER - 2, Sh3UpperWriteWord);
	Sh3SetWriteLongHandler(SH3_MAXHANDLER - 2, Sh3UpperWriteLong);

	for (INT32 i = 0; i < 3; i++) {
		m_timer[i].init(i, sh4_timer_callback);
	}

	for (INT32 i = 0; i < 4; i++) {
		m_dma_timer[i].init(i | 0x10, sh4_dmac_callback); // "| 0x10" for timer logging
	}

	cave_blitter_delay.init(0 | 0x100, cave_blitter_delay_func); // "| 0x100" for timer logging

	sh4_set_cave_blitter_delay_func(NULL);

	sh4_parse_configuration();
	sh4_default_exception_priorities();
	m_irln = 15;
	m_test_irq = 0;

	Sh3SetClockCV1k(c_clock);

	CpuCheatRegister(0, &sh3Config);
}

void Sh3Exit()
{
	Cv1kV2RevokeTrust();
	Cv1kInvalidateFastMemory();
	cv1kIdlePage=NULL;cv1kIdleBatchEnabled=false;
	Sh3Cv1kDmaPerfEnable(false);
	Sh3SetCv1kPollRead(NULL,NULL);
	Sh3SetCv1kNative(0);
}

// The registered CV1000 I/O handlers cannot write executable RAM
// except through the watched DMA/write paths. Replacement revokes this.
void Sh3SetCv1kTrackedIO(){
#if SH3_NATIVE_V2
	Cv1kV2SetTracked(0,true);
	if(WriteByte[SH3_MAXHANDLER-1]==Sh3LowerWriteByte &&
		WriteWord[SH3_MAXHANDLER-1]==Sh3LowerWriteWord &&
		WriteLong[SH3_MAXHANDLER-1]==Sh3LowerWriteLong)Cv1kV2SetTracked(SH3_MAXHANDLER-1,true);
	if(WriteByte[SH3_MAXHANDLER-2]==Sh3UpperWriteByte &&
		WriteWord[SH3_MAXHANDLER-2]==Sh3UpperWriteWord &&
		WriteLong[SH3_MAXHANDLER-2]==Sh3UpperWriteLong)Cv1kV2SetTracked(SH3_MAXHANDLER-2,true);
#endif
}

void Sh3Open(const INT32 i)
{
	if (i != 0) bprintf(0, _T("Sh3Open(%d): only supports single cpu, currently.\n"));
}

void Sh3Close()
{
}

INT32 Sh3GetActive()
{
	return 0;
}

/* Called for unimplemented opcodes */
static void TODO(const UINT16 opcode)
{
}

//static UINT8 *pr = NULL;

static inline UINT16 sh3_cpu_readop16(UINT32 A)
{
	WaitState(A, 0);

	UINT8 *pr = MemMapF[ A >> SH3_SHIFT ]; // fetch

	return *((UINT16 *)(pr + (A & SH3_PAGEM)));
}

SH3_MEMORY_INLINE UINT8 RB(UINT32 A)
{
	if (A < 0xe0000000) {
		A &= AM;
		WaitState(A, 0);
	}

	UINT8 *pr = MemMapR[ A >> SH3_SHIFT ];
	if ( (uintptr_t)pr >= SH3_MAXHANDLER ) {
#ifdef LSB_FIRST
		A ^= 1;
#endif
		return pr[A & SH3_PAGEM];
	}
	return ReadByte[(uintptr_t)pr](A);
}

SH3_MEMORY_INLINE UINT16 RW(UINT32 A)
{
	if (A < 0xe0000000) {
		A &= AM;
		WaitState(A, 0);
	}

	UINT8 *pr = MemMapR[ A >> SH3_SHIFT ];
	// A long-only handler replaces the CV1000 speedhack page's former
	// all-width handler. Retain its unaligned-word behavior, too.
	if ((A & 1) && (A >> SH3_SHIFT) == 0xc00 &&
		(uintptr_t)pr >= SH3_MAXHANDLER &&
		(uintptr_t)MemMapRL[A >> SH3_SHIFT] == 1)
		return ReadWord[1](A);
	if ( (uintptr_t)pr >= SH3_MAXHANDLER ) {
#ifdef LSB_FIRST
	//	A ^= 2;
#endif
		//return (pr[A & SH3_PAGEM] << 8) | pr[(A & SH3_PAGEM) + 1];
		return *((UINT16 *)(pr + (A & SH3_PAGEM)));
	}
	return ReadWord[(uintptr_t)pr](A);
}

// Opt-in host code-size experiment; does not change guest cycle accounting.
// Keep the RAM long-load path inline while sharing its larger idle/MMIO tail.
#ifndef SH3_SMALL_HANDLERS
#define SH3_SMALL_HANDLERS 0
#endif
#ifndef SH3_FAST_ARITHMETIC
#if defined(__aarch64__) || defined(SH3_INTERPRETER_TEST)
#define SH3_FAST_ARITHMETIC 1
#else
#define SH3_FAST_ARITHMETIC 0
#endif
#endif
#if SH3_SMALL_HANDLERS
static __attribute__((noinline)) UINT32 Sh3ReadLongSlow(UINT32 A, UINT8 *pr)
{
	if ((uintptr_t)pr==1 && cv1kIdlePage && (A>>SH3_SHIFT)==0xc00 &&
		MemMapR[0xc00]==cv1kIdlePage && ReadLong[1]==cv1kIdleHandler) {
		// Same address and PC test, alignment and word swap as the
		// driver's speedhack_read_long. Do not bypass another handler.
		if (A==cv1kIdleAddress) {
			const UINT32 pc=(m_delay?m_delay:m_pc)&AM;
			if (pc==cv1kIdlePC || pc==cv1kIdlePC+2) {
				EAT(*cv1kIdleCycles);
			}
		}
		UINT32 V=*((UINT32 *)(cv1kIdlePage+(A&0xfffc)));
#ifdef LSB_FIRST
		V=(V<<16)|(V>>16);
#endif
		return V;
	}
	return ReadLong[(uintptr_t)pr](A);
}
#endif

SH3_MEMORY_INLINE UINT32 RL(UINT32 A)
{
	if (A < 0xe0000000) {
		A &= AM;
		WaitState(A, 0);
	}

	UINT8 *pr = MemMapRL[ A >> SH3_SHIFT ];
	if ( (uintptr_t)pr >= SH3_MAXHANDLER ) {
		UINT32 V = *((UINT32 *)(pr + (A & SH3_PAGEM)));
#ifdef LSB_FIRST
		V = (V << 16) | (V >> 16);
#endif
		return V;
	}
#if SH3_SMALL_HANDLERS
	return Sh3ReadLongSlow(A, pr);
#else
	if ((uintptr_t)pr==1 && cv1kIdlePage && (A>>SH3_SHIFT)==0xc00 &&
		MemMapR[0xc00]==cv1kIdlePage && ReadLong[1]==cv1kIdleHandler) {
		// Same address and PC test, alignment and word swap as the
		// driver's speedhack_read_long. Do not bypass another handler.
		if (A==cv1kIdleAddress) {
			const UINT32 pc=(m_delay?m_delay:m_pc)&AM;
			if (pc==cv1kIdlePC || pc==cv1kIdlePC+2) {
				EAT(*cv1kIdleCycles);
			}
		}
		UINT32 V=*((UINT32 *)(cv1kIdlePage+(A&0xfffc)));
#ifdef LSB_FIRST
		V=(V<<16)|(V>>16);
#endif
		return V;
	}
	return ReadLong[(uintptr_t)pr](A);
#endif
}

SH3_MEMORY_INLINE void WB(UINT32 A, UINT8 V)
{
	if (A < 0xe0000000) {
		A &= AM;
		WaitState(A, 1);
	}

	UINT8 *pr = MemMapW[ A >> SH3_SHIFT ];
	if ((uintptr_t)pr >= SH3_MAXHANDLER) {
#ifdef LSB_FIRST
	    A ^= 1;
#endif
		pr[A & SH3_PAGEM] = (UINT8)V;
		Cv1kV2NoteWrite(A,1);
		return;
	}
	Cv1kV2NoteHandlerWrite((uintptr_t)pr);
	WriteByte[(uintptr_t)pr](A, V);
}

SH3_MEMORY_INLINE void WW(UINT32 A, UINT16 V)
{
	if (A < 0xe0000000) {
		A &= AM;
		WaitState(A, 1);
	}

	UINT8 *pr = MemMapW[ A >> SH3_SHIFT ];
	if ((uintptr_t)pr >= SH3_MAXHANDLER) {
#ifdef LSB_FIRST
	 //   A ^= 2;
#endif
		*((UINT16 *)(pr + (A & SH3_PAGEM))) = (UINT16)V;
		Cv1kV2NoteWrite(A,2);
		return;
	}
	Cv1kV2NoteHandlerWrite((uintptr_t)pr);
	WriteWord[(uintptr_t)pr](A, V);
}

SH3_MEMORY_INLINE void WL(UINT32 A, UINT32 V)
{
	if (A < 0xe0000000) {
		A &= AM;
		WaitState(A, 1);
	}

	UINT8 *pr = MemMapW[ A >> SH3_SHIFT ];
	if ((uintptr_t)pr >= SH3_MAXHANDLER) {
#ifdef LSB_FIRST
		V = (V << 16) | (V >> 16);
#endif
		*((UINT32 *)(pr + (A & SH3_PAGEM))) = (UINT32)V;
		Cv1kV2NoteWrite(A,4);
		return;
	}
	Cv1kV2NoteHandlerWrite((uintptr_t)pr);
	WriteLong[(uintptr_t)pr](A, V);
}

#include "sh4comn.inc"
#include "sh3comn.inc"
#include "sh4tmu.inc"
#include "sh4dmac.inc"

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 1100  1       -
 *  ADD     Rm,Rn
 */
static inline void ADD(const UINT16 opcode)
{
	m_r[Rn] += m_r[Rm];
}

/*  code                 cycles  t-bit
 *  0111 nnnn iiii iiii  1       -
 *  ADD     #imm,Rn
 */
static inline void ADDI(const UINT16 opcode)
{
	m_r[Rn] += (INT32)(INT16)(INT8)(opcode&0xff);
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 1110  1       carry
 *  ADDC    Rm,Rn
 */
static inline void ADDC(const UINT16 opcode)
{
#if SH3_FAST_ARITHMETIC
	const UINT64 sum=UINT64(m_r[Rn])+m_r[Rm]+(m_sr&T);
	m_r[Rn]=UINT32(sum);
	m_sr=(m_sr&~T)|UINT32(sum>>32);
#else
	UINT32 m = Rm; UINT32 n = Rn;
	UINT32 tmp0, tmp1;

	tmp1 = m_r[n] + m_r[m];
	tmp0 = m_r[n];
	m_r[n] = tmp1 + (m_sr & T);
	if (tmp0 > tmp1)
		m_sr |= T;
	else
		m_sr &= ~T;
	if (tmp1 > m_r[n])
		m_sr |= T;
#endif
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 1111  1       overflow
 *  ADDV    Rm,Rn
 */
static inline void ADDV(const UINT16 opcode)
{
#if SH3_FAST_ARITHMETIC
	const UINT32 a = m_r[Rn], b = m_r[Rm];
	const UINT32 result = a + b;
	m_r[Rn] = result;
	m_sr = (m_sr & ~T) | ((~(a ^ b) & (a ^ result)) >> 31);
#else
	UINT32 m = Rm; UINT32 n = Rn;
	INT32 dest, src, ans;

	if ((INT32) m_r[n] >= 0)
		dest = 0;
	else
		dest = 1;
	if ((INT32) m_r[m] >= 0)
		src = 0;
	else
		src = 1;
	src += dest;
	m_r[n] += m_r[m];
	if ((INT32) m_r[n] >= 0)
		ans = 0;
	else
		ans = 1;
	ans += dest;
	if (src == 0 || src == 2)
	{
		if (ans == 1)
			m_sr |= T;
		else
			m_sr &= ~T;
	}
	else
		m_sr &= ~T;
#endif
}

/*  code                 cycles  t-bit
 *  0010 nnnn mmmm 1001  1       -
 *  AND     Rm,Rn
 */
static inline void AND(const UINT16 opcode)
{
	m_r[Rn] &= m_r[Rm];
}


/*  code                 cycles  t-bit
 *  1100 1001 iiii iiii  1       -
 *  AND     #imm,R0
 */
static inline void ANDI(const UINT16 opcode)
{
	m_r[0] &= (opcode&0xff);
}

/*  code                 cycles  t-bit
 *  1100 1101 iiii iiii  1       -
 *  AND.B   #imm,@(R0,GBR)
 */
static inline void ANDM(const UINT16 opcode)
{
	UINT32 temp;

	m_ea = m_gbr + m_r[0];
	temp = (opcode&0xff) & RB( m_ea );
	WB(m_ea, temp );
	m_sh4_icount -= 2;
	sh3_total_cycles += 2;
}

/*  code                 cycles  t-bit
 *  1000 1011 dddd dddd  3/1     -
 *  BF      disp8
 */
static inline void BF(const UINT16 opcode)
{
	if ((m_sr & T) == 0)
	{
		INT32 disp = ((INT32)(opcode&0xff) << 24) >> 24;
		m_pc = m_ea = m_pc + disp * 2 + 2;
		m_sh4_icount -= 2;
		sh3_total_cycles += 2;
	}
}

/*  code                 cycles  t-bit
 *  1000 1111 dddd dddd  3/1     -
 *  BFS     disp8
 */
static inline void BFS(const UINT16 opcode)
{
	if ((m_sr & T) == 0)
	{
		INT32 disp = ((INT32)(opcode&0xff) << 24) >> 24;
		m_delay = m_pc;
		m_pc = m_ea = m_pc + disp * 2 + 2;
		m_sh4_icount--;
		sh3_total_cycles += 1;
	}
}

/*  code                 cycles  t-bit
 *  1010 dddd dddd dddd  2       -
 *  BRA     disp12
 */
static inline void BRA(const UINT16 opcode)
{
	INT32 disp = ((INT32)(opcode&0xfff) << 20) >> 20;

#if BUSY_LOOP_HACKS
	if (disp == -2)
	{
		UINT32 next_opcode = RW(m_ppc & AM);
		/* BRA  $
		 * NOP
		 */
		if (next_opcode == 0x0009) {
			m_sh4_icount %= 3;   /* cycles for BRA $ and NOP taken (3) */
		}
	}
#endif
	m_delay = m_pc;
	m_pc = m_ea = m_pc + disp * 2 + 2;
	m_sh4_icount--;
	sh3_total_cycles += 1;
}

/*  code                 cycles  t-bit
 *  0000 mmmm 0010 0011  2       -
 *  BRAF    Rm
 */
static inline void BRAF(const UINT16 opcode)
{
	m_delay = m_pc;
	m_pc += m_r[Rn] + 2;
	m_sh4_icount--;
	sh3_total_cycles += 1;
}

/*  code                 cycles  t-bit
 *  1011 dddd dddd dddd  2       -
 *  BSR     disp12
 */
static inline void BSR(const UINT16 opcode)
{
	INT32 disp = ((INT32)(opcode&0xfff) << 20) >> 20;

	m_pr = m_pc + 2;
	m_delay = m_pc;
	m_pc = m_ea = m_pc + disp * 2 + 2;
	m_sh4_icount--;
	sh3_total_cycles += 1;
}

/*  code                 cycles  t-bit
 *  0000 mmmm 0000 0011  2       -
 *  BSRF    Rm
 */
static inline void BSRF(const UINT16 opcode)
{
	m_pr = m_pc + 2;
	m_delay = m_pc;
	m_pc += m_r[Rn] + 2;
	m_sh4_icount--;
	sh3_total_cycles += 1;
}

/*  code                 cycles  t-bit
 *  1000 1001 dddd dddd  3/1     -
 *  BT      disp8
 */
static inline void BT(const UINT16 opcode)
{
	if ((m_sr & T) != 0)
	{
		INT32 disp = ((INT32)(opcode&0xff) << 24) >> 24;
		m_pc = m_ea = m_pc + disp * 2 + 2;
		m_sh4_icount -= 2;
		sh3_total_cycles += 2;
	}
}

/*  code                 cycles  t-bit
 *  1000 1101 dddd dddd  2/1     -
 *  BTS     disp8
 */
static inline void BTS(const UINT16 opcode)
{
	if ((m_sr & T) != 0)
	{
		INT32 disp = ((INT32)(opcode&0xff) << 24) >> 24;
		m_delay = m_pc;
		m_pc = m_ea = m_pc + disp * 2 + 2;
		m_sh4_icount--;
		sh3_total_cycles += 1;
	}
}

/*  code                 cycles  t-bit
 *  0000 0000 0010 1000  1       -
 *  CLRMAC
 */
static inline void CLRMAC(const UINT16 opcode)
{
	m_mach = 0;
	m_macl = 0;
}

/*  code                 cycles  t-bit
 *  0000 0000 0000 1000  1       -
 *  CLRT
 */
static inline void CLRT(const UINT16 opcode)
{
	m_sr &= ~T;
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0000  1       comparison result
 *  CMP_EQ  Rm,Rn
 */
static inline void CMPEQ(const UINT16 opcode)
{
	if (m_r[Rn] == m_r[Rm])
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0011  1       comparison result
 *  CMP_GE  Rm,Rn
 */
static inline void CMPGE(const UINT16 opcode)
{
	if ((INT32) m_r[Rn] >= (INT32) m_r[Rm])
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0111  1       comparison result
 *  CMP_GT  Rm,Rn
 */
static inline void CMPGT(const UINT16 opcode)
{
	if ((INT32) m_r[Rn] > (INT32) m_r[Rm])
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0110  1       comparison result
 *  CMP_HI  Rm,Rn
 */
static inline void CMPHI(const UINT16 opcode)
{
	if ((UINT32) m_r[Rn] > (UINT32) m_r[Rm])
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0010  1       comparison result
 *  CMP_HS  Rm,Rn
 */
static inline void CMPHS(const UINT16 opcode)
{
	if ((UINT32) m_r[Rn] >= (UINT32) m_r[Rm])
		m_sr |= T;
	else
		m_sr &= ~T;
}


/*  code                 cycles  t-bit
 *  0100 nnnn 0001 0101  1       comparison result
 *  CMP_PL  Rn
 */
static inline void CMPPL(const UINT16 opcode)
{
	if ((INT32) m_r[Rn] > 0)
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  code                 cycles  t-bit
 *  0100 nnnn 0001 0001  1       comparison result
 *  CMP_PZ  Rn
 */
static inline void CMPPZ(const UINT16 opcode)
{
	if ((INT32) m_r[Rn] >= 0)
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  code                 cycles  t-bit
 *  0010 nnnn mmmm 1100  1       comparison result
 * CMP_STR  Rm,Rn
 */
static inline void CMPSTR(const UINT16 opcode)
{
#if SH3_FAST_ARITHMETIC
	const UINT32 v=m_r[Rn]^m_r[Rm];
	m_sr=(m_sr&~T)|UINT32(((v-0x01010101u)&~v&0x80808080u)!=0);
#else
	UINT32 temp;
	INT32 HH, HL, LH, LL;
	temp = m_r[Rn] ^ m_r[Rm];
	HH = (temp >> 24) & 0xff;
	HL = (temp >> 16) & 0xff;
	LH = (temp >> 8) & 0xff;
	LL = temp & 0xff;
	if (HH && HL && LH && LL)
	m_sr &= ~T;
	else
	m_sr |= T;
#endif
	}


/*  code                 cycles  t-bit
 *  1000 1000 iiii iiii  1       comparison result
 *  CMP/EQ #imm,R0
 */
static inline void CMPIM(const UINT16 opcode)
{
	UINT32 imm = (UINT32)(INT32)(INT16)(INT8)(opcode&0xff);

	if (m_r[0] == imm)
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  code                 cycles  t-bit
 *  0010 nnnn mmmm 0111  1       calculation result
 *  DIV0S   Rm,Rn
 */
static inline void DIV0S(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if ((m_r[n] & 0x80000000) == 0)
		m_sr &= ~Q;
	else
		m_sr |= Q;
	if ((m_r[m] & 0x80000000) == 0)
		m_sr &= ~M;
	else
		m_sr |= M;
	if ((m_r[m] ^ m_r[n]) & 0x80000000)
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  code                 cycles  t-bit
 *  0000 0000 0001 1001  1       0
 *  DIV0U
 */
static inline void DIV0U(const UINT16 opcode)
{
	m_sr &= ~(M | Q | T);
}

/*  code                 cycles  t-bit
 *  0011 nnnn mmmm 0100  1       calculation result
 *  DIV1 Rm,Rn
 */
static inline void DIV1(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	UINT32 tmp0;
	UINT32 old_q;

	old_q = m_sr & Q;
	if (0x80000000 & m_r[n])
		m_sr |= Q;
	else
		m_sr &= ~Q;

	m_r[n] = (m_r[n] << 1) | (m_sr & T);

	if (!old_q)
	{
		if (!(m_sr & M))
		{
			tmp0 = m_r[n];
			m_r[n] -= m_r[m];
			if(!(m_sr & Q))
				if(m_r[n] > tmp0)
					m_sr |= Q;
				else
					m_sr &= ~Q;
			else
				if(m_r[n] > tmp0)
					m_sr &= ~Q;
				else
					m_sr |= Q;
		}
		else
		{
			tmp0 = m_r[n];
			m_r[n] += m_r[m];
			if(!(m_sr & Q))
			{
				if(m_r[n] < tmp0)
					m_sr &= ~Q;
				else
					m_sr |= Q;
			}
			else
			{
				if(m_r[n] < tmp0)
					m_sr |= Q;
				else
					m_sr &= ~Q;
			}
		}
	}
	else
	{
		if (!(m_sr & M))
		{
			tmp0 = m_r[n];
			m_r[n] += m_r[m];
			if(!(m_sr & Q))
				if(m_r[n] < tmp0)
					m_sr |= Q;
				else
					m_sr &= ~Q;
			else
				if(m_r[n] < tmp0)
					m_sr &= ~Q;
				else
					m_sr |= Q;
		}
		else
		{
			tmp0 = m_r[n];
			m_r[n] -= m_r[m];
			if(!(m_sr & Q))
				if(m_r[n] > tmp0)
					m_sr &= ~Q;
				else
					m_sr |= Q;
			else
				if(m_r[n] > tmp0)
					m_sr |= Q;
				else
					m_sr &= ~Q;
		}
	}

	tmp0 = (m_sr & (Q | M));
	if((!tmp0) || (tmp0 == 0x300)) /* if Q == M set T else clear T */
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  DMULS.L Rm,Rn */
static inline void DMULS(const UINT16 opcode)
{
#if SH3_FAST_ARITHMETIC
	const UINT64 product=UINT64(INT64(INT32(m_r[Rn]))*INT64(INT32(m_r[Rm])));
	m_mach=UINT32(product>>32);m_macl=UINT32(product);
	EAT(1);
#else
	UINT32 m = Rm; UINT32 n = Rn;

	UINT32 RnL, RnH, RmL, RmH, Res0, Res1, Res2;
	UINT32 temp0, temp1, temp2, temp3;
	INT32 tempm, tempn, fnLmL;

	tempn = (INT32) m_r[n];
	tempm = (INT32) m_r[m];
	if (tempn < 0)
		tempn = 0 - tempn;
	if (tempm < 0)
		tempm = 0 - tempm;
	if ((INT32) (m_r[n] ^ m_r[m]) < 0)
		fnLmL = -1;
	else
		fnLmL = 0;
	temp1 = (UINT32) tempn;
	temp2 = (UINT32) tempm;
	RnL = temp1 & 0x0000ffff;
	RnH = (temp1 >> 16) & 0x0000ffff;
	RmL = temp2 & 0x0000ffff;
	RmH = (temp2 >> 16) & 0x0000ffff;
	temp0 = RmL * RnL;
	temp1 = RmH * RnL;
	temp2 = RmL * RnH;
	temp3 = RmH * RnH;
	Res2 = 0;
	Res1 = temp1 + temp2;
	if (Res1 < temp1)
		Res2 += 0x00010000;
	temp1 = (Res1 << 16) & 0xffff0000;
	Res0 = temp0 + temp1;
	if (Res0 < temp0)
		Res2++;
	Res2 = Res2 + ((Res1 >> 16) & 0x0000ffff) + temp3;
	if (fnLmL < 0)
	{
		Res2 = ~Res2;
		if (Res0 == 0)
			Res2++;
		else
			Res0 = (~Res0) + 1;
	}
	m_mach = Res2;
	m_macl = Res0;
	m_sh4_icount--;
	sh3_total_cycles += 1;
#endif
}

/*  DMULU.L Rm,Rn */
static inline void DMULU(const UINT16 opcode)
{
#if SH3_FAST_ARITHMETIC
	const UINT64 product=UINT64(m_r[Rn])*m_r[Rm];
	m_mach=UINT32(product>>32);m_macl=UINT32(product);
	EAT(1);
#else
	UINT32 m = Rm; UINT32 n = Rn;

	UINT32 RnL, RnH, RmL, RmH, Res0, Res1, Res2;
	UINT32 temp0, temp1, temp2, temp3;

	RnL = m_r[n] & 0x0000ffff;
	RnH = (m_r[n] >> 16) & 0x0000ffff;
	RmL = m_r[m] & 0x0000ffff;
	RmH = (m_r[m] >> 16) & 0x0000ffff;
	temp0 = RmL * RnL;
	temp1 = RmH * RnL;
	temp2 = RmL * RnH;
	temp3 = RmH * RnH;
	Res2 = 0;
	Res1 = temp1 + temp2;
	if (Res1 < temp1)
		Res2 += 0x00010000;
	temp1 = (Res1 << 16) & 0xffff0000;
	Res0 = temp0 + temp1;
	if (Res0 < temp0)
		Res2++;
	Res2 = Res2 + ((Res1 >> 16) & 0x0000ffff) + temp3;
	m_mach = Res2;
	m_macl = Res0;
	m_sh4_icount--;
	sh3_total_cycles += 1;
#endif
}

/*  DT      Rn */
static inline void DT(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n]--;
	if (m_r[n] == 0)
		m_sr |= T;
	else
		m_sr &= ~T;
#if BUSY_LOOP_HACKS
	{
		UINT32 next_opcode = RW(m_ppc & AM);
		/* DT   Rn
		 * BF   $-2
		 */
		if (next_opcode == 0x8bfd)
		{
			if (m_r[n] > 1 && m_sh4_icount > 4)
			{
				// Same busy-loop speedhack, but advance the repeated
				// DT/BF pairs in O(1), preserving T and cycle overshoot.
				const UINT32 budget = (UINT32(m_sh4_icount) - 1) >> 2;
				const UINT32 remaining = m_r[n] - 1;
				const UINT32 skip = budget < remaining ? budget : remaining;
				m_r[n] -= skip;
				EAT(INT32(skip * 4));
			}
		}
	}
#endif
}

/*  EXTS.B  Rm,Rn */
static inline void EXTSB(const UINT16 opcode)
{
	m_r[Rn] = ((INT32)m_r[Rm] << 24) >> 24;
}

/*  EXTS.W  Rm,Rn */
static inline void EXTSW(const UINT16 opcode)
{
	m_r[Rn] = ((INT32)m_r[Rm] << 16) >> 16;
}

/*  EXTU.B  Rm,Rn */
static inline void EXTUB(const UINT16 opcode)
{
	m_r[Rn] = m_r[Rm] & 0x000000ff;
}

/*  EXTU.W  Rm,Rn */
static inline void EXTUW(const UINT16 opcode)
{
	m_r[Rn] = m_r[Rm] & 0x0000ffff;
}

/*  JMP     @Rm */
static inline void JMP(const UINT16 opcode)
{
	m_delay = m_pc;
	m_pc = m_ea = m_r[Rn];
}

/*  JSR     @Rm */
static inline void JSR(const UINT16 opcode)
{
	m_delay = m_pc;
	m_pr = m_pc + 2;
	m_pc = m_ea = m_r[Rn];
	m_sh4_icount--;
	sh3_total_cycles += 1;
}


/*  LDC     Rm,SR */
static inline void LDCSR(const UINT16 opcode)
{
	UINT32 reg;

	reg = m_r[Rn];
//	if ((machine().debug_flags & DEBUG_FLAG_ENABLED) != 0)
//		sh4_syncronize_register_bank((m_sr & sRB) >> 29);
	if ((m_r[Rn] & sRB) != (m_sr & sRB))
		sh4_change_register_bank(m_r[Rn] & sRB ? 1 : 0);
	m_sr = reg & FLAGS;
	sh4_exception_recompute();
}

/*  LDC     Rm,GBR */
static inline void LDCGBR(const UINT16 opcode)
{
	m_gbr = m_r[Rn];
}

/*  LDC     Rm,VBR */
static inline void LDCVBR(const UINT16 opcode)
{
	m_vbr = m_r[Rn];
}

/*  LDC.L   @Rm+,SR */
static inline void LDCMSR(const UINT16 opcode)
{
	UINT32 old;

	old = m_sr;
	m_ea = m_r[Rn];
	m_sr = RL(m_ea ) & FLAGS;
//	if ((machine().debug_flags & DEBUG_FLAG_ENABLED) != 0)
//		sh4_syncronize_register_bank((old & sRB) >> 29);
	if ((old & sRB) != (m_sr & sRB))
		sh4_change_register_bank(m_sr & sRB ? 1 : 0);
	m_r[Rn] += 4;
	m_sh4_icount -= 2;
	sh3_total_cycles += 2;
	sh4_exception_recompute();
}

/*  LDC.L   @Rm+,GBR */
static inline void LDCMGBR(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	m_gbr = RL(m_ea );
	m_r[Rn] += 4;
	m_sh4_icount -= 2;
	sh3_total_cycles += 2;
}

/*  LDC.L   @Rm+,VBR */
static inline void LDCMVBR(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	m_vbr = RL(m_ea );
	m_r[Rn] += 4;
	m_sh4_icount -= 2;
	sh3_total_cycles += 2;
}

/*  LDS     Rm,MACH */
static inline void LDSMACH(const UINT16 opcode)
{
	m_mach = m_r[Rn];
}

/*  LDS     Rm,MACL */
static inline void LDSMACL(const UINT16 opcode)
{
	m_macl = m_r[Rn];
}

/*  LDS     Rm,PR */
static inline void LDSPR(const UINT16 opcode)
{
	m_pr = m_r[Rn];
}

/*  LDS.L   @Rm+,MACH */
static inline void LDSMMACH(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	m_mach = RL(m_ea );
	m_r[Rn] += 4;
}

/*  LDS.L   @Rm+,MACL */
static inline void LDSMMACL(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	m_macl = RL(m_ea );
	m_r[Rn] += 4;
}

/*  LDS.L   @Rm+,PR */
static inline void LDSMPR(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	m_pr = RL(m_ea );
	m_r[Rn] += 4;
}

/*  MAC.L   @Rm+,@Rn+ */
static inline void MAC_L(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	UINT32 RnL, RnH, RmL, RmH, Res0, Res1, Res2;
	UINT32 temp0, temp1, temp2, temp3;
	INT32 tempm, tempn, fnLmL;

	tempn = (INT32) RL(m_r[n] );
	m_r[n] += 4;
	tempm = (INT32) RL(m_r[m] );
	m_r[m] += 4;
	if ((INT32) (tempn ^ tempm) < 0)
		fnLmL = -1;
	else
		fnLmL = 0;
	if (tempn < 0)
		tempn = 0 - tempn;
	if (tempm < 0)
		tempm = 0 - tempm;
	temp1 = (UINT32) tempn;
	temp2 = (UINT32) tempm;
	RnL = temp1 & 0x0000ffff;
	RnH = (temp1 >> 16) & 0x0000ffff;
	RmL = temp2 & 0x0000ffff;
	RmH = (temp2 >> 16) & 0x0000ffff;
	temp0 = RmL * RnL;
	temp1 = RmH * RnL;
	temp2 = RmL * RnH;
	temp3 = RmH * RnH;
	Res2 = 0;
	Res1 = temp1 + temp2;
	if (Res1 < temp1)
		Res2 += 0x00010000;
	temp1 = (Res1 << 16) & 0xffff0000;
	Res0 = temp0 + temp1;
	if (Res0 < temp0)
		Res2++;
	Res2 = Res2 + ((Res1 >> 16) & 0x0000ffff) + temp3;
	if (fnLmL < 0)
	{
		Res2 = ~Res2;
		if (Res0 == 0)
			Res2++;
		else
			Res0 = (~Res0) + 1;
	}
	if (m_sr & S)
	{
		Res0 = m_macl + Res0;
		if (m_macl > Res0)
			Res2++;
		Res2 += (m_mach & 0x0000ffff);
		if (((INT32) Res2 < 0) && (Res2 < 0xffff8000))
		{
			Res2 = 0x00008000;
			Res0 = 0x00000000;
		}
		else if (((INT32) Res2 > 0) && (Res2 > 0x00007fff))
		{
			Res2 = 0x00007fff;
			Res0 = 0xffffffff;
		}
		m_mach = Res2;
		m_macl = Res0;
	}
	else
	{
		Res0 = m_macl + Res0;
		if (m_macl > Res0)
			Res2++;
		Res2 += m_mach;
		m_mach = Res2;
		m_macl = Res0;
	}
	m_sh4_icount -= 2;
	sh3_total_cycles += 2;
}

/*  MAC.W   @Rm+,@Rn+ */
static inline void MAC_W(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	INT32 tempm, tempn, dest, src, ans;
	UINT32 templ;

	tempn = (INT32) RW(m_r[n] );
	m_r[n] += 2;
	tempm = (INT32) RW(m_r[m] );
	m_r[m] += 2;
	templ = m_macl;
	tempm = ((INT32) (short) tempn * (INT32) (short) tempm);
	if ((INT32) m_macl >= 0)
		dest = 0;
	else
		dest = 1;
	if ((INT32) tempm >= 0)
	{
		src = 0;
		tempn = 0;
	}
	else
	{
		src = 1;
		tempn = 0xffffffff;
	}
	src += dest;
	m_macl += tempm;
	if ((INT32) m_macl >= 0)
		ans = 0;
	else
		ans = 1;
	ans += dest;
	if (m_sr & S)
	{
		if (ans == 1)
			{
				if (src == 0)
					m_macl = 0x7fffffff;
				if (src == 2)
					m_macl = 0x80000000;
			}
	}
	else
	{
		m_mach += tempn;
		if (templ > m_macl)
			m_mach += 1;
	}
	m_sh4_icount -= 2;
	sh3_total_cycles += 2;
}

/*  MOV     Rm,Rn */
static inline void MOV(const UINT16 opcode)
{
	m_r[Rn] = m_r[Rm];
}

/*  MOV.B   Rm,@Rn */
static inline void MOVBS(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	WB(m_ea, m_r[Rm] & 0x000000ff);
}

/*  MOV.W   Rm,@Rn */
static inline void MOVWS(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	WW(m_ea, m_r[Rm] & 0x0000ffff);
}

/*  MOV.L   Rm,@Rn */
static inline void MOVLS(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	WL(m_ea, m_r[Rm] );
}

/*  MOV.B   @Rm,Rn */
static inline void MOVBL(const UINT16 opcode)
{
	m_ea = m_r[Rm];
	m_r[Rn] = (UINT32)(INT32)(INT16)(INT8) RB( m_ea );
}

/*  MOV.W   @Rm,Rn */
static inline void MOVWL(const UINT16 opcode)
{
	m_ea = m_r[Rm];
	m_r[Rn] = (UINT32)(INT32)(INT16) RW(m_ea );
}

/*  MOV.L   @Rm,Rn */
static inline void MOVLL(const UINT16 opcode)
{
	m_ea = m_r[Rm];
	m_r[Rn] = RL(m_ea );
}

/*  MOV.B   Rm,@-Rn */
static inline void MOVBM(const UINT16 opcode)
{
	UINT32 data = m_r[Rm] & 0x000000ff;

	m_r[Rn] -= 1;
	WB(m_r[Rn], data );
}

/*  MOV.W   Rm,@-Rn */
static inline void MOVWM(const UINT16 opcode)
{
	UINT32 data = m_r[Rm] & 0x0000ffff;

	m_r[Rn] -= 2;
	WW(m_r[Rn], data );
}

/*  MOV.L   Rm,@-Rn */
static inline void MOVLM(const UINT16 opcode)
{
	UINT32 data = m_r[Rm];

	m_r[Rn] -= 4;
	WL(m_r[Rn], data );
}

/*  MOV.B   @Rm+,Rn */
static inline void MOVBP(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	m_r[n] = (UINT32)(INT32)(INT16)(INT8) RB( m_r[m] );
	if (n != m)
		m_r[m] += 1;
}

/*  MOV.W   @Rm+,Rn */
static inline void MOVWP(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	m_r[n] = (UINT32)(INT32)(INT16) RW(m_r[m] );
	if (n != m)
		m_r[m] += 2;
}

/*  MOV.L   @Rm+,Rn */
static inline void MOVLP(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	m_r[n] = RL(m_r[m] );
	if (n != m)
		m_r[m] += 4;
}

/*  MOV.B   Rm,@(R0,Rn) */
static inline void MOVBS0(const UINT16 opcode)
{
	m_ea = m_r[Rn] + m_r[0];
	WB(m_ea, m_r[Rm] & 0x000000ff );
}

/*  MOV.W   Rm,@(R0,Rn) */
static inline void MOVWS0(const UINT16 opcode)
{
	m_ea = m_r[Rn] + m_r[0];
	WW(m_ea, m_r[Rm] & 0x0000ffff );
}

/*  MOV.L   Rm,@(R0,Rn) */
static inline void MOVLS0(const UINT16 opcode)
{
	m_ea = m_r[Rn] + m_r[0];
	WL(m_ea, m_r[Rm] );
}

/*  MOV.B   @(R0,Rm),Rn */
static inline void MOVBL0(const UINT16 opcode)
{
	m_ea = m_r[Rm] + m_r[0];
	m_r[Rn] = (UINT32)(INT32)(INT16)(INT8) RB( m_ea );
}

/*  MOV.W   @(R0,Rm),Rn */
static inline void MOVWL0(const UINT16 opcode)
{
	m_ea = m_r[Rm] + m_r[0];
	m_r[Rn] = (UINT32)(INT32)(INT16) RW(m_ea );
}

/*  MOV.L   @(R0,Rm),Rn */
static inline void MOVLL0(const UINT16 opcode)
{
	m_ea = m_r[Rm] + m_r[0];
	m_r[Rn] = RL(m_ea );
}

/*  MOV     #imm,Rn */
static inline void MOVI(const UINT16 opcode)
{
	m_r[Rn] = (UINT32)(INT32)(INT16)(INT8)(opcode&0xff);
}

/*  MOV.W   @(disp8,PC),Rn */
static inline void MOVWI(const UINT16 opcode)
{
	UINT32 disp = opcode & 0xff;
	m_ea = m_pc + disp * 2 + 2;
	m_r[Rn] = (UINT32)(INT32)(INT16) RW(m_ea );
}

/*  MOV.L   @(disp8,PC),Rn */
static inline void MOVLI(const UINT16 opcode)
{
	UINT32 disp = opcode & 0xff;
	m_ea = ((m_pc + 2) & ~3) + disp * 4;
	m_r[Rn] = RL(m_ea );
}

/*  MOV.B   @(disp8,GBR),R0 */
static inline void MOVBLG(const UINT16 opcode)
{
	UINT32 disp = opcode & 0xff;
	m_ea = m_gbr + disp;
	m_r[0] = (UINT32)(INT32)(INT16)(INT8) RB( m_ea );
}

/*  MOV.W   @(disp8,GBR),R0 */
static inline void MOVWLG(const UINT16 opcode)
{
	UINT32 disp = opcode & 0xff;
	m_ea = m_gbr + disp * 2;
	m_r[0] = (INT32)(INT16) RW(m_ea );
}

/*  MOV.L   @(disp8,GBR),R0 */
static inline void MOVLLG(const UINT16 opcode)
{
	UINT32 disp = opcode & 0xff;
	m_ea = m_gbr + disp * 4;
	m_r[0] = RL(m_ea );
}

/*  MOV.B   R0,@(disp8,GBR) */
static inline void MOVBSG(const UINT16 opcode)
{
	UINT32 disp = opcode & 0xff;
	m_ea = m_gbr + disp;
	WB(m_ea, m_r[0] & 0x000000ff );
}

/*  MOV.W   R0,@(disp8,GBR) */
static inline void MOVWSG(const UINT16 opcode)
{
	UINT32 disp = opcode & 0xff;
	m_ea = m_gbr + disp * 2;
	WW(m_ea, m_r[0] & 0x0000ffff );
}

/*  MOV.L   R0,@(disp8,GBR) */
static inline void MOVLSG(const UINT16 opcode)
{
	UINT32 disp = opcode & 0xff;
	m_ea = m_gbr + disp * 4;
	WL(m_ea, m_r[0] );
}

/*  MOV.B   R0,@(disp4,Rm) */
static inline void MOVBS4(const UINT16 opcode)
{
	UINT32 disp = opcode & 0x0f;
	m_ea = m_r[Rm] + disp;
	WB(m_ea, m_r[0] & 0x000000ff );
}

/*  MOV.W   R0,@(disp4,Rm) */
static inline void MOVWS4(const UINT16 opcode)
{
	UINT32 disp = opcode & 0x0f;
	m_ea = m_r[Rm] + disp * 2;
	WW(m_ea, m_r[0] & 0x0000ffff );
}

/* MOV.L Rm,@(disp4,Rn) */
static inline void MOVLS4(const UINT16 opcode)
{
	UINT32 disp = opcode & 0x0f;
	m_ea = m_r[Rn] + disp * 4;
	WL(m_ea, m_r[Rm] );
}

/*  MOV.B   @(disp4,Rm),R0 */
static inline void MOVBL4(const UINT16 opcode)
{
	UINT32 disp = opcode & 0x0f;
	m_ea = m_r[Rm] + disp;
	m_r[0] = (UINT32)(INT32)(INT16)(INT8) RB( m_ea );
}

/*  MOV.W   @(disp4,Rm),R0 */
static inline void MOVWL4(const UINT16 opcode)
{
	UINT32 disp = opcode & 0x0f;
	m_ea = m_r[Rm] + disp * 2;
	m_r[0] = (UINT32)(INT32)(INT16) RW(m_ea );
}

/*  MOV.L   @(disp4,Rm),Rn */
static inline void MOVLL4(const UINT16 opcode)
{
	UINT32 disp = opcode & 0x0f;
	m_ea = m_r[Rm] + disp * 4;
	m_r[Rn] = RL(m_ea );
}

/*  MOVA    @(disp8,PC),R0 */
static inline void MOVA(const UINT16 opcode)
{
	UINT32 disp = opcode & 0xff;
	m_ea = ((m_pc + 2) & ~3) + disp * 4;
	m_r[0] = m_ea;
}

/*  MOVT    Rn */
void MOVT(const UINT16 opcode)
{
	m_r[Rn] = m_sr & T;
}

/*  MUL.L   Rm,Rn */
static inline void MULL(const UINT16 opcode)
{
	m_macl = m_r[Rn] * m_r[Rm];
	m_sh4_icount--;
	sh3_total_cycles += 1;
}

/*  MULS    Rm,Rn */
static inline void MULS(const UINT16 opcode)
{
	m_macl = (INT16) m_r[Rn] * (INT16) m_r[Rm];
}

/*  MULU    Rm,Rn */
static inline void MULU(const UINT16 opcode)
{
	m_macl = (UINT16) m_r[Rn] * (UINT16) m_r[Rm];
}

/*  NEG     Rm,Rn */
static inline void NEG(const UINT16 opcode)
{
	m_r[Rn] = 0 - m_r[Rm];
}

/*  NEGC    Rm,Rn */
static inline void NEGC(const UINT16 opcode)
{
	UINT32 temp;

	temp = m_r[Rm];
	m_r[Rn] = -temp - (m_sr & T);
	if (temp || (m_sr & T))
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  NOP */
static inline void NOP(const UINT16 opcode)
{
}

/*  NOT     Rm,Rn */
static inline void NOT(const UINT16 opcode)
{
	m_r[Rn] = ~m_r[Rm];
}

/*  OR      Rm,Rn */
static inline void OR(const UINT16 opcode)
{
	m_r[Rn] |= m_r[Rm];
}

/*  OR      #imm,R0 */
static inline void ORI(const UINT16 opcode)
{
	m_r[0] |= (opcode&0xff);
	m_sh4_icount -= 2;
	sh3_total_cycles += 2;
}

/*  OR.B    #imm,@(R0,GBR) */
static inline void ORM(const UINT16 opcode)
{
	UINT32 temp;

	m_ea = m_gbr + m_r[0];
	temp = RB( m_ea );
	temp |= (opcode&0xff);
	WB(m_ea, temp );
}

/*  ROTCL   Rn */
static inline void ROTCL(const UINT16 opcode)
{
	UINT32 n = Rn;

	UINT32 temp;

	temp = (m_r[n] >> 31) & T;
	m_r[n] = (m_r[n] << 1) | (m_sr & T);
	m_sr = (m_sr & ~T) | temp;
}

/*  ROTCR   Rn */
static inline void ROTCR(const UINT16 opcode)
{
	UINT32 n = Rn;

	UINT32 temp;
	temp = (m_sr & T) << 31;
	if (m_r[n] & T)
		m_sr |= T;
	else
		m_sr &= ~T;
	m_r[n] = (m_r[n] >> 1) | temp;
}

/*  ROTL    Rn */
static inline void ROTL(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_sr = (m_sr & ~T) | ((m_r[n] >> 31) & T);
	m_r[n] = (m_r[n] << 1) | (m_r[n] >> 31);
}

/*  ROTR    Rn */
static inline void ROTR(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_sr = (m_sr & ~T) | (m_r[n] & T);
	m_r[n] = (m_r[n] >> 1) | (m_r[n] << 31);
}

/*  RTE */
static inline void RTE(const UINT16 opcode)
{
	m_delay = m_pc;
	m_pc = m_ea = m_spc;
//	if ((machine().debug_flags & DEBUG_FLAG_ENABLED) != 0)
//		sh4_syncronize_register_bank((m_sr & sRB) >> 29);
	if ((m_ssr & sRB) != (m_sr & sRB))
		sh4_change_register_bank(m_ssr & sRB ? 1 : 0);
	m_sr = m_ssr;
	m_sh4_icount--;
	sh3_total_cycles += 1;
	sh4_exception_recompute();
}

/*  RTS */
static inline void RTS(const UINT16 opcode)
{
	m_delay = m_pc;
	m_pc = m_ea = m_pr;
	m_sh4_icount--;
	sh3_total_cycles += 1;
}

/*  SETT */
static inline void SETT(const UINT16 opcode)
{
	m_sr |= T;
}

/*  SHAL    Rn      (same as SHLL) */
static inline void SHAL(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_sr = (m_sr & ~T) | ((m_r[n] >> 31) & T);
	m_r[n] <<= 1;
}

/*  SHAR    Rn */
static inline void SHAR(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_sr = (m_sr & ~T) | (m_r[n] & T);
	m_r[n] = (UINT32)((INT32)m_r[n] >> 1);
}

/*  SHLL    Rn      (same as SHAL) */
static inline void SHLL(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_sr = (m_sr & ~T) | ((m_r[n] >> 31) & T);
	m_r[n] <<= 1;
}

/*  SHLL2   Rn */
static inline void SHLL2(const UINT16 opcode)
{
	m_r[Rn] <<= 2;
}

/*  SHLL8   Rn */
static inline void SHLL8(const UINT16 opcode)
{
	m_r[Rn] <<= 8;
}

/*  SHLL16  Rn */
static inline void SHLL16(const UINT16 opcode)
{
	m_r[Rn] <<= 16;
}

/*  SHLR    Rn */
static inline void SHLR(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_sr = (m_sr & ~T) | (m_r[n] & T);
	m_r[n] >>= 1;
}

/*  SHLR2   Rn */
static inline void SHLR2(const UINT16 opcode)
{
	m_r[Rn] >>= 2;
}

/*  SHLR8   Rn */
static inline void SHLR8(const UINT16 opcode)
{
	m_r[Rn] >>= 8;
}

/*  SHLR16  Rn */
static inline void SHLR16(const UINT16 opcode)
{
	m_r[Rn] >>= 16;
}

/*  SLEEP */
static inline void SLEEP(const UINT16 opcode)
{
	/* 0 = normal mode */
	/* 1 = enters into power-down mode */
	/* 2 = go out the power-down mode after an exception */
	if(m_sleep_mode != 2)
		m_pc -= 2;
	m_sh4_icount -= 2;
	sh3_total_cycles += 2;
	/* Wait_for_exception; */
	if(m_sleep_mode == 0)
		m_sleep_mode = 1;
	else if(m_sleep_mode == 2)
		m_sleep_mode = 0;
}

/*  STC     SR,Rn */
static inline void STCSR(const UINT16 opcode)
{
	m_r[Rn] = m_sr;
}

/*  STC     GBR,Rn */
static inline void STCGBR(const UINT16 opcode)
{
	m_r[Rn] = m_gbr;
}

/*  STC     VBR,Rn */
static inline void STCVBR(const UINT16 opcode)
{
	m_r[Rn] = m_vbr;
}

/*  STC.L   SR,@-Rn */
static inline void STCMSR(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_sr );
	m_sh4_icount--;
	sh3_total_cycles += 1;
}

/*  STC.L   GBR,@-Rn */
static inline void STCMGBR(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_gbr );
	m_sh4_icount--;
	sh3_total_cycles += 1;
}

/*  STC.L   VBR,@-Rn */
static inline void STCMVBR(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_vbr );
	m_sh4_icount--;
	sh3_total_cycles += 1;
}

/*  STS     MACH,Rn */
static inline void STSMACH(const UINT16 opcode)
{
	m_r[Rn] = m_mach;
}

/*  STS     MACL,Rn */
static inline void STSMACL(const UINT16 opcode)
{
	m_r[Rn] = m_macl;
}

/*  STS     PR,Rn */
static inline void STSPR(const UINT16 opcode)
{
	m_r[Rn] = m_pr;
}

/*  STS.L   MACH,@-Rn */
static inline void STSMMACH(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_mach );
}

/*  STS.L   MACL,@-Rn */
static inline void STSMMACL(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_macl );
}

/*  STS.L   PR,@-Rn */
static inline void STSMPR(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_pr );
}

/*  SUB     Rm,Rn */
static inline void SUB(const UINT16 opcode)
{
	m_r[Rn] -= m_r[Rm];
}

/*  SUBC    Rm,Rn */
static inline void SUBC(const UINT16 opcode)
{
#if SH3_FAST_ARITHMETIC
	const UINT64 a=m_r[Rn],sub=UINT64(m_r[Rm])+(m_sr&T);
	m_r[Rn]=UINT32(a-sub);
	m_sr=(m_sr&~T)|UINT32(a<sub);
#else
	UINT32 m = Rm; UINT32 n = Rn;

	UINT32 tmp0, tmp1;

	tmp1 = m_r[n] - m_r[m];
	tmp0 = m_r[n];
	m_r[n] = tmp1 - (m_sr & T);
	if (tmp0 < tmp1)
		m_sr |= T;
	else
		m_sr &= ~T;
	if (tmp1 < m_r[n])
		m_sr |= T;
#endif
}

/*  SUBV    Rm,Rn */
static inline void SUBV(const UINT16 opcode)
{
#if SH3_FAST_ARITHMETIC
	const UINT32 a = m_r[Rn], b = m_r[Rm];
	const UINT32 result = a - b;
	m_r[Rn] = result;
	m_sr = (m_sr & ~T) | (((a ^ b) & (a ^ result)) >> 31);
#else
	UINT32 m = Rm; UINT32 n = Rn;

	INT32 dest, src, ans;

	if ((INT32) m_r[n] >= 0)
		dest = 0;
	else
		dest = 1;
	if ((INT32) m_r[m] >= 0)
		src = 0;
	else
		src = 1;
	src += dest;
	m_r[n] -= m_r[m];
	if ((INT32) m_r[n] >= 0)
		ans = 0;
	else
		ans = 1;
	ans += dest;
	if (src == 1)
	{
		if (ans == 1)
			m_sr |= T;
		else
			m_sr &= ~T;
	}
	else
		m_sr &= ~T;
#endif
}

/*  SWAP.B  Rm,Rn */
static inline void SWAPB(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	UINT32 temp0, temp1;

	temp0 = m_r[m] & 0xffff0000;
	temp1 = (m_r[m] & 0x000000ff) << 8;
	m_r[n] = (m_r[m] >> 8) & 0x000000ff;
	m_r[n] = m_r[n] | temp1 | temp0;
}

/*  SWAP.W  Rm,Rn */
static inline void SWAPW(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	UINT32 temp;

	temp = (m_r[m] >> 16) & 0x0000ffff;
	m_r[n] = (m_r[m] << 16) | temp;
}

/*  TAS.B   @Rn */
static inline void TAS(const UINT16 opcode)
{
	UINT32 n = Rn;

	UINT32 temp;
	m_ea = m_r[n];
	/* Bus Lock enable */
	temp = RB( m_ea );
	if (temp == 0)
		m_sr |= T;
	else
		m_sr &= ~T;
	temp |= 0x80;
	/* Bus Lock disable */
	WB(m_ea, temp );
	m_sh4_icount -= 3;
	sh3_total_cycles += 3;
}

/*  TRAPA   #imm */
static inline void TRAPA(const UINT16 opcode)
{
	UINT32 imm = opcode & 0xff;

	if (m_cpu_type == CPU_TYPE_SH4)
	{
		m_m[TRA] = imm << 2;
	}
	else /* SH3 */
	{
		m_sh3internal_upper[SH3_TRA_ADDR] = imm << 2;
	}


	m_ssr = m_sr;
	m_spc = m_pc;
	m_sgr = m_r[15];

	m_sr |= MD;
//	if ((machine().debug_flags & DEBUG_FLAG_ENABLED) != 0)
//		sh4_syncronize_register_bank((m_sr & sRB) >> 29);
	if (!(m_sr & sRB))
		sh4_change_register_bank(1);
	m_sr |= sRB;
	m_sr |= BL;
	sh4_exception_recompute();

	if (m_cpu_type == CPU_TYPE_SH4)
	{
		m_m[EXPEVT] = 0x00000160;
	}
	else /* SH3 */
	{
		m_sh3internal_upper[SH3_EXPEVT_ADDR] = 0x00000160;
	}

	m_pc = m_vbr + 0x00000100;

	m_sh4_icount -= 7;
	sh3_total_cycles += 7;
}

/*  TST     Rm,Rn */
static inline void TST(const UINT16 opcode)
{
	if ((m_r[Rn] & m_r[Rm]) == 0)
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  TST     #imm,R0 */
static inline void TSTI(const UINT16 opcode)
{
	UINT32 imm = opcode & 0xff;

	if ((imm & m_r[0]) == 0)
		m_sr |= T;
	else
		m_sr &= ~T;
}

/*  TST.B   #imm,@(R0,GBR) */
static inline void TSTM(const UINT16 opcode)
{
	UINT32 imm = opcode & 0xff;

	m_ea = m_gbr + m_r[0];
	if ((imm & RB( m_ea )) == 0)
		m_sr |= T;
	else
		m_sr &= ~T;
	m_sh4_icount -= 2;
	sh3_total_cycles += 2;
}

/*  XOR     Rm,Rn */
static inline void XOR(const UINT16 opcode)
{
	m_r[Rn] ^= m_r[Rm];
}

/*  XOR     #imm,R0 */
static inline void XORI(const UINT16 opcode)
{
	UINT32 imm = opcode & 0xff;
	m_r[0] ^= imm;
}

/*  XOR.B   #imm,@(R0,GBR) */
static inline void XORM(const UINT16 opcode)
{
	UINT32 imm = opcode & 0xff;
	UINT32 temp;

	m_ea = m_gbr + m_r[0];
	temp = RB( m_ea );
	temp ^= imm;
	WB(m_ea, temp );
	m_sh4_icount -= 2;
	sh3_total_cycles += 2;
}

/*  XTRCT   Rm,Rn */
static inline void XTRCT(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	UINT32 temp;

	temp = (m_r[m] << 16) & 0xffff0000;
	m_r[n] = (m_r[n] >> 16) & 0x0000ffff;
	m_r[n] |= temp;
}

/*  STC     SSR,Rn */
static inline void STCSSR(const UINT16 opcode)
{
	m_r[Rn] = m_ssr;
}

/*  STC     SPC,Rn */
static inline void STCSPC(const UINT16 opcode)
{
	m_r[Rn] = m_spc;
}

/*  STC     SGR,Rn */
static inline void STCSGR(const UINT16 opcode)
{
	m_r[Rn] = m_sgr;
}

/*  STS     FPUL,Rn */
static inline void STSFPUL(const UINT16 opcode)
{
	m_r[Rn] = m_fpul;
}

/*  STS     FPSCR,Rn */
static inline void STSFPSCR(const UINT16 opcode)
{
	m_r[Rn] = m_fpscr & 0x003FFFFF;
}

/*  STC     DBR,Rn */
static inline void STCDBR(const UINT16 opcode)
{
	m_r[Rn] = m_dbr;
}

/*  STCRBANK   Rm_BANK,Rn */
static inline void STCRBANK(const UINT16 opcode)
{
	UINT32 m = Rm;

	m_r[Rn] = m_rbnk[m_sr&sRB ? 0 : 1][m & 7];
}

/*  STCMRBANK   Rm_BANK,@-Rn */
static inline void STCMRBANK(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_rbnk[m_sr&sRB ? 0 : 1][m & 7]);
	m_sh4_icount--;
	sh3_total_cycles += 1;
}

/*  MOVCA.L     R0,@Rn */
static inline void MOVCAL(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	WL(m_ea, m_r[0] );
}

static inline void CLRS(const UINT16 opcode)
{
	m_sr &= ~S;
}

static inline void SETS(const UINT16 opcode)
{
	m_sr |= S;
}

/*  STS.L   SGR,@-Rn */
static inline void STCMSGR(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_sgr );
}

/*  STS.L   FPUL,@-Rn */
static inline void STSMFPUL(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_fpul );
}

/*  STS.L   FPSCR,@-Rn */
static inline void STSMFPSCR(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_fpscr & 0x003FFFFF);
}

/*  STC.L   DBR,@-Rn */
static inline void STCMDBR(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_dbr );
}

/*  STC.L   SSR,@-Rn */
static inline void STCMSSR(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_ssr );
}

/*  STC.L   SPC,@-Rn */
static inline void STCMSPC(const UINT16 opcode)
{
	UINT32 n = Rn;

	m_r[n] -= 4;
	m_ea = m_r[n];
	WL(m_ea, m_spc );
}

/*  LDS.L   @Rm+,FPUL */
static inline void LDSMFPUL(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	m_fpul = RL(m_ea );
	m_r[Rn] += 4;
}

/*  LDS.L   @Rm+,FPSCR */
static inline void LDSMFPSCR(const UINT16 opcode)
{
	UINT32 s;

	s = m_fpscr;
	m_ea = m_r[Rn];
	m_fpscr = RL(m_ea );
	m_fpscr &= 0x003FFFFF;
	m_r[Rn] += 4;
	if ((s & FR) != (m_fpscr & FR))
		sh4_swap_fp_registers();
#ifdef LSB_FIRST
	if ((s & PR) != (m_fpscr & PR))
		sh4_swap_fp_couples();
#endif
	m_fpu_sz = (m_fpscr & SZ) ? 1 : 0;
	m_fpu_pr = (m_fpscr & PR) ? 1 : 0;
}

/*  LDC.L   @Rm+,DBR */
static inline void LDCMDBR(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	m_dbr = RL(m_ea );
	m_r[Rn] += 4;
}

/*  LDC.L   @Rn+,Rm_BANK */
static inline void LDCMRBANK(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	m_ea = m_r[n];
	m_rbnk[m_sr&sRB ? 0 : 1][m & 7] = RL(m_ea );
	m_r[n] += 4;
}

/*  LDC.L   @Rm+,SSR */
static inline void LDCMSSR(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	m_ssr = RL(m_ea );
	m_r[Rn] += 4;
}

/*  LDC.L   @Rm+,SPC */
static inline void LDCMSPC(const UINT16 opcode)
{
	m_ea = m_r[Rn];
	m_spc = RL(m_ea );
	m_r[Rn] += 4;
}

/*  LDS     Rm,FPUL */
static inline void LDSFPUL(const UINT16 opcode)
{
	m_fpul = m_r[Rn];
}

/*  LDS     Rm,FPSCR */
static inline void LDSFPSCR(const UINT16 opcode)
{
	UINT32 s;

	s = m_fpscr;
	m_fpscr = m_r[Rn] & 0x003FFFFF;
	if ((s & FR) != (m_fpscr & FR))
		sh4_swap_fp_registers();
#ifdef LSB_FIRST
	if ((s & PR) != (m_fpscr & PR))
		sh4_swap_fp_couples();
#endif
	m_fpu_sz = (m_fpscr & SZ) ? 1 : 0;
	m_fpu_pr = (m_fpscr & PR) ? 1 : 0;
}

/*  LDC     Rm,DBR */
static inline void LDCDBR(const UINT16 opcode)
{
	m_dbr = m_r[Rn];
}

/*  SHAD    Rm,Rn */
static inline void SHAD(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if ((m_r[m] & 0x80000000) == 0)
		m_r[n] = m_r[n] << (m_r[m] & 0x1F);
	else if ((m_r[m] & 0x1F) == 0) {
		if ((m_r[n] & 0x80000000) == 0)
			m_r[n] = 0;
		else
			m_r[n] = 0xFFFFFFFF;
	} else
		m_r[n]=(INT32)m_r[n] >> ((~m_r[m] & 0x1F)+1);
}

/*  SHLD    Rm,Rn */
static inline void SHLD(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if ((m_r[m] & 0x80000000) == 0)
		m_r[n] = m_r[n] << (m_r[m] & 0x1F);
	else if ((m_r[m] & 0x1F) == 0)
		m_r[n] = 0;
	else
		m_r[n] = m_r[n] >> ((~m_r[m] & 0x1F)+1);
}

/*  LDCRBANK   Rn,Rm_BANK */
static inline void LDCRBANK(const UINT16 opcode)
{
	UINT32 m = Rm;

	m_rbnk[m_sr&sRB ? 0 : 1][m & 7] = m_r[Rn];
}

/*  LDC     Rm,SSR */
static inline void LDCSSR(const UINT16 opcode)
{
	m_ssr = m_r[Rn];
}

/*  LDC     Rm,SPC */
static inline void LDCSPC(const UINT16 opcode)
{
	m_spc = m_r[Rn];
}

/*  PREF     @Rn */
static inline void PREFM(const UINT16 opcode)
{
	int a;
	UINT32 addr,dest,sq;

	addr = m_r[Rn]; // address
	if ((addr >= 0xE0000000) && (addr <= 0xE3FFFFFF))
	{
		if (m_sh4_mmu_enabled)
		{
			addr = addr & 0xFFFFFFE0;
			dest = sh4_getsqremap(addr); // good enough for naomi-gd rom, probably not much else

		}
		else
		{
			sq = (addr & 0x20) >> 5;
			dest = addr & 0x03FFFFE0;
			if (sq == 0)
			{
				if (m_cpu_type == CPU_TYPE_SH4)
				{
					dest |= (m_m[QACR0] & 0x1C) << 24;
				}
				else
				{
					fatalerror("m_cpu_type != CPU_TYPE_SH4 but access internal regs\n", 0);
				}
			}
			else
			{
				if (m_cpu_type == CPU_TYPE_SH4)
				{
					dest |= (m_m[QACR1] & 0x1C) << 24;
				}
				else
				{
					fatalerror("m_cpu_type != CPU_TYPE_SH4 but access internal regs\n", 0);
				}

			}
			addr = addr & 0xFFFFFFE0;
		}

		for (a = 0;a < 4;a++)
		{
			// shouldn't be causing a memory read, should store sq writes in registers.
			//m_program->write_qword(dest, m_program->read_qword(addr));
			WL(dest, RL(addr));
			WL(dest+4, RL(addr+4));
			addr += 8;
			dest += 8;
		}
	}
}

/*****************************************************************************
 *  OPCODE DISPATCHERS
 *****************************************************************************/

















/*  FMOV.S  @Rm+,FRn PR=0 SZ=0 1111nnnnmmmm1001 */
/*  FMOV    @Rm+,DRn PR=0 SZ=1 1111nnn0mmmm1001 */
/*  FMOV    @Rm+,XDn PR=0 SZ=1 1111nnn1mmmm1001 */
/*  FMOV    @Rm+,XDn PR=1      1111nnn1mmmm1001 */
static inline void FMOVMRIFR(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		n = n & 14;
		m_ea = m_r[m];
		m_r[m] += 8;
		m_xf[n+NATIVE_ENDIAN_VALUE_LE_BE(1,0)] = RL(m_ea );
		m_xf[n+NATIVE_ENDIAN_VALUE_LE_BE(0,1)] = RL(m_ea+4 );
	} else {              /* PR = 0 */
		if (m_fpu_sz) { /* SZ = 1 */
			if (n & 1) {
				n = n & 14;
				m_ea = m_r[m];
				m_xf[n] = RL(m_ea );
				m_r[m] += 4;
				m_xf[n+1] = RL(m_ea+4 );
				m_r[m] += 4;
			} else {
				m_ea = m_r[m];
				m_fr[n] = RL(m_ea );
				m_r[m] += 4;
				m_fr[n+1] = RL(m_ea+4 );
				m_r[m] += 4;
			}
		} else {              /* SZ = 0 */
			m_ea = m_r[m];
			m_fr[n] = RL(m_ea );
			m_r[m] += 4;
		}
	}
}

/*  FMOV.S  FRm,@Rn PR=0 SZ=0 1111nnnnmmmm1010 */
/*  FMOV    DRm,@Rn PR=0 SZ=1 1111nnnnmmm01010 */
/*  FMOV    XDm,@Rn PR=0 SZ=1 1111nnnnmmm11010 */
/*  FMOV    XDm,@Rn PR=1      1111nnnnmmm11010 */
static inline void FMOVFRMR(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		m= m & 14;
		m_ea = m_r[n];
		WL(m_ea,m_xf[m+NATIVE_ENDIAN_VALUE_LE_BE(1,0)] );
		WL(m_ea+4,m_xf[m+NATIVE_ENDIAN_VALUE_LE_BE(0,1)] );
	} else {              /* PR = 0 */
		if (m_fpu_sz) { /* SZ = 1 */
			if (m & 1) {
				m= m & 14;
				m_ea = m_r[n];
				WL(m_ea,m_xf[m] );
				WL(m_ea+4,m_xf[m+1] );
			} else {
				m_ea = m_r[n];
				WL(m_ea,m_fr[m] );
				WL(m_ea+4,m_fr[m+1] );
			}
		} else {              /* SZ = 0 */
			m_ea = m_r[n];
			WL(m_ea,m_fr[m] );
		}
	}
}

/*  FMOV.S  FRm,@-Rn PR=0 SZ=0 1111nnnnmmmm1011 */
/*  FMOV    DRm,@-Rn PR=0 SZ=1 1111nnnnmmm01011 */
/*  FMOV    XDm,@-Rn PR=0 SZ=1 1111nnnnmmm11011 */
/*  FMOV    XDm,@-Rn PR=1      1111nnnnmmm11011 */
static inline void FMOVFRMDR(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		m= m & 14;
		m_r[n] -= 8;
		m_ea = m_r[n];
		WL(m_ea,m_xf[m+NATIVE_ENDIAN_VALUE_LE_BE(1,0)] );
		WL(m_ea+4,m_xf[m+NATIVE_ENDIAN_VALUE_LE_BE(0,1)] );
	} else {              /* PR = 0 */
		if (m_fpu_sz) { /* SZ = 1 */
			if (m & 1) {
				m= m & 14;
				m_r[n] -= 8;
				m_ea = m_r[n];
				WL(m_ea,m_xf[m] );
				WL(m_ea+4,m_xf[m+1] );
			} else {
				m_r[n] -= 8;
				m_ea = m_r[n];
				WL(m_ea,m_fr[m] );
				WL(m_ea+4,m_fr[m+1] );
			}
		} else {              /* SZ = 0 */
			m_r[n] -= 4;
			m_ea = m_r[n];
			WL(m_ea,m_fr[m] );
		}
	}
}

/*  FMOV.S  FRm,@(R0,Rn) PR=0 SZ=0 1111nnnnmmmm0111 */
/*  FMOV    DRm,@(R0,Rn) PR=0 SZ=1 1111nnnnmmm00111 */
/*  FMOV    XDm,@(R0,Rn) PR=0 SZ=1 1111nnnnmmm10111 */
/*  FMOV    XDm,@(R0,Rn) PR=1      1111nnnnmmm10111 */
static inline void FMOVFRS0(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		m= m & 14;
		m_ea = m_r[0] + m_r[n];
		WL(m_ea,m_xf[m+NATIVE_ENDIAN_VALUE_LE_BE(1,0)] );
		WL(m_ea+4,m_xf[m+NATIVE_ENDIAN_VALUE_LE_BE(0,1)] );
	} else {              /* PR = 0 */
		if (m_fpu_sz) { /* SZ = 1 */
			if (m & 1) {
				m= m & 14;
				m_ea = m_r[0] + m_r[n];
				WL(m_ea,m_xf[m] );
				WL(m_ea+4,m_xf[m+1] );
			} else {
				m_ea = m_r[0] + m_r[n];
				WL(m_ea,m_fr[m] );
				WL(m_ea+4,m_fr[m+1] );
			}
		} else {              /* SZ = 0 */
			m_ea = m_r[0] + m_r[n];
			WL(m_ea,m_fr[m] );
		}
	}
}

/*  FMOV.S  @(R0,Rm),FRn PR=0 SZ=0 1111nnnnmmmm0110 */
/*  FMOV    @(R0,Rm),DRn PR=0 SZ=1 1111nnn0mmmm0110 */
/*  FMOV    @(R0,Rm),XDn PR=0 SZ=1 1111nnn1mmmm0110 */
/*  FMOV    @(R0,Rm),XDn PR=1      1111nnn1mmmm0110 */
static inline void FMOVS0FR(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		n= n & 14;
		m_ea = m_r[0] + m_r[m];
		m_xf[n+NATIVE_ENDIAN_VALUE_LE_BE(1,0)] = RL(m_ea );
		m_xf[n+NATIVE_ENDIAN_VALUE_LE_BE(0,1)] = RL(m_ea+4 );
	} else {              /* PR = 0 */
		if (m_fpu_sz) { /* SZ = 1 */
			if (n & 1) {
				n= n & 14;
				m_ea = m_r[0] + m_r[m];
				m_xf[n] = RL(m_ea );
				m_xf[n+1] = RL(m_ea+4 );
			} else {
				m_ea = m_r[0] + m_r[m];
				m_fr[n] = RL(m_ea );
				m_fr[n+1] = RL(m_ea+4 );
			}
		} else {              /* SZ = 0 */
			m_ea = m_r[0] + m_r[m];
			m_fr[n] = RL(m_ea );
		}
	}
}

/*  FMOV.S  @Rm,FRn PR=0 SZ=0 1111nnnnmmmm1000 */
/*  FMOV    @Rm,DRn PR=0 SZ=1 1111nnn0mmmm1000 */
/*  FMOV    @Rm,XDn PR=0 SZ=1 1111nnn1mmmm1000 */
/*  FMOV    @Rm,XDn PR=1      1111nnn1mmmm1000 */
/*  FMOV    @Rm,DRn PR=1      1111nnn0mmmm1000 */
static inline void FMOVMRFR(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		if (n & 1) {
			n= n & 14;
			m_ea = m_r[m];
			m_xf[n+NATIVE_ENDIAN_VALUE_LE_BE(1,0)] = RL(m_ea );
			m_xf[n+NATIVE_ENDIAN_VALUE_LE_BE(0,1)] = RL(m_ea+4 );
		} else {
			n= n & 14;
			m_ea = m_r[m];
			m_fr[n+NATIVE_ENDIAN_VALUE_LE_BE(1,0)] = RL(m_ea );
			m_fr[n+NATIVE_ENDIAN_VALUE_LE_BE(0,1)] = RL(m_ea+4 );
		}
	} else {              /* PR = 0 */
		if (m_fpu_sz) { /* SZ = 1 */
			if (n & 1) {
				n= n & 14;
				m_ea = m_r[m];
				m_xf[n] = RL(m_ea );
				m_xf[n+1] = RL(m_ea+4 );
			} else {
				n= n & 14;
				m_ea = m_r[m];
				m_fr[n] = RL(m_ea );
				m_fr[n+1] = RL(m_ea+4 );
			}
		} else {              /* SZ = 0 */
			m_ea = m_r[m];
			m_fr[n] = RL(m_ea );
		}
	}
}

/*  FMOV    FRm,FRn PR=0 SZ=0 FRm -> FRn 1111nnnnmmmm1100 */
/*  FMOV    DRm,DRn PR=0 SZ=1 DRm -> DRn 1111nnn0mmm01100 */
/*  FMOV    XDm,DRn PR=1      XDm -> DRn 1111nnn0mmm11100 */
/*  FMOV    DRm,XDn PR=1      DRm -> XDn 1111nnn1mmm01100 */
/*  FMOV    XDm,XDn PR=1      XDm -> XDn 1111nnn1mmm11100 */
static inline void FMOVFR(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if ((m_fpu_sz == 0) && (m_fpu_pr == 0)) /* SZ = 0 */
		m_fr[n] = m_fr[m];
	else { /* SZ = 1 or PR = 1 */
		if (m & 1) {
			if (n & 1) {
				m_xf[n & 14] = m_xf[m & 14];
				m_xf[n | 1] = m_xf[m | 1];
			} else {
				m_fr[n] = m_xf[m & 14];
				m_fr[n | 1] = m_xf[m | 1];
			}
		} else {
			if (n & 1) {
				m_xf[n & 14] = m_fr[m];
				m_xf[n | 1] = m_fr[m | 1]; // (a&14)+1 -> a|1
			} else {
				m_fr[n] = m_fr[m];
				m_fr[n | 1] = m_fr[m | 1];
			}
		}
	}
}

/*  FLDI1  FRn 1111nnnn10011101 */
static inline void FLDI1(const UINT16 opcode)
{
	m_fr[Rn] = 0x3F800000;
}

/*  FLDI0  FRn 1111nnnn10001101 */
static inline void FLDI0(const UINT16 opcode)
{
	m_fr[Rn] = 0;
}

/*  FLDS FRm,FPUL 1111mmmm00011101 */
static inline void  FLDS(const UINT16 opcode)
{
	m_fpul = m_fr[Rn];
}

/*  FSTS FPUL,FRn 1111nnnn00001101 */
static inline void  FSTS(const UINT16 opcode)
{
	m_fr[Rn] = m_fpul;
}

/* FRCHG 1111101111111101 */
void FRCHG()
{
	m_fpscr ^= FR;
	sh4_swap_fp_registers();
}

/* FSCHG 1111001111111101 */
void FSCHG()
{
	m_fpscr ^= SZ;
	m_fpu_sz = (m_fpscr & SZ) ? 1 : 0;
}

/* FTRC FRm,FPUL PR=0 1111mmmm00111101 */
/* FTRC DRm,FPUL PR=1 1111mmm000111101 */
static inline void FTRC(const UINT16 opcode)
{
	UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		if(n & 1)
			fatalerror("SH-4: FTRC opcode used with n %d",n);

		n = n & 14;
		*((INT32 *)&m_fpul) = (INT32)FP_RFD(n);
	} else {              /* PR = 0 */
		/* read m_fr[n] as float -> truncate -> fpul(32) */
		*((INT32 *)&m_fpul) = (INT32)FP_RFS(n);
	}
}

/* FLOAT FPUL,FRn PR=0 1111nnnn00101101 */
/* FLOAT FPUL,DRn PR=1 1111nnn000101101 */
static inline void FLOAT(const UINT16 opcode)
{
	UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		if(n & 1)
			fatalerror("SH-4: FLOAT opcode used with n %d",n);

		n = n & 14;
		FP_RFD(n) = (double)*((INT32 *)&m_fpul);
	} else {              /* PR = 0 */
		FP_RFS(n) = (float)*((INT32 *)&m_fpul);
	}
}

/* FNEG FRn PR=0 1111nnnn01001101 */
/* FNEG DRn PR=1 1111nnn001001101 */
static inline void FNEG(const UINT16 opcode)
{
	UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		FP_RFD(n) = -FP_RFD(n);
	} else {              /* PR = 0 */
		FP_RFS(n) = -FP_RFS(n);
	}
}

/* FABS FRn PR=0 1111nnnn01011101 */
/* FABS DRn PR=1 1111nnn001011101 */
static inline void FABS(const UINT16 opcode)
{
	UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
#ifdef LSB_FIRST
		n = n | 1; // n & 14 + 1
		m_fr[n] = m_fr[n] & 0x7fffffff;
#else
		n = n & 14;
		m_fr[n] = m_fr[n] & 0x7fffffff;
#endif
	} else {              /* PR = 0 */
		m_fr[n] = m_fr[n] & 0x7fffffff;
	}
}

/* FCMP/EQ FRm,FRn PR=0 1111nnnnmmmm0100 */
/* FCMP/EQ DRm,DRn PR=1 1111nnn0mmm00100 */
static inline void FCMP_EQ(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		n = n & 14;
		m = m & 14;
		if (FP_RFD(n) == FP_RFD(m))
			m_sr |= T;
		else
			m_sr &= ~T;
	} else {              /* PR = 0 */
		if (FP_RFS(n) == FP_RFS(m))
			m_sr |= T;
		else
			m_sr &= ~T;
	}
}

/* FCMP/GT FRm,FRn PR=0 1111nnnnmmmm0101 */
/* FCMP/GT DRm,DRn PR=1 1111nnn0mmm00101 */
static inline void FCMP_GT(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		n = n & 14;
		m = m & 14;
		if (FP_RFD(n) > FP_RFD(m))
			m_sr |= T;
		else
			m_sr &= ~T;
	} else {              /* PR = 0 */
		if (FP_RFS(n) > FP_RFS(m))
			m_sr |= T;
		else
			m_sr &= ~T;
	}
}

/* FCNVDS DRm,FPUL PR=1 1111mmm010111101 */
static inline void FCNVDS(const UINT16 opcode)
{
	UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		n = n & 14;
		if (m_fpscr & RM)
			m_fr[n | NATIVE_ENDIAN_VALUE_LE_BE(0,1)] &= 0xe0000000; /* round toward zero*/
		*((float *)&m_fpul) = (float)FP_RFD(n);
	}
}

/* FCNVSD FPUL, DRn PR=1 1111nnn010101101 */
static inline void FCNVSD(const UINT16 opcode)
{
	UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		n = n & 14;
		FP_RFD(n) = (double)*((float *)&m_fpul);
	}
}

/* FADD FRm,FRn PR=0 1111nnnnmmmm0000 */
/* FADD DRm,DRn PR=1 1111nnn0mmm00000 */
static inline void FADD(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		n = n & 14;
		m = m & 14;
		FP_RFD(n) = FP_RFD(n) + FP_RFD(m);
	} else {              /* PR = 0 */
		FP_RFS(n) = FP_RFS(n) + FP_RFS(m);
	}
}

/* FSUB FRm,FRn PR=0 1111nnnnmmmm0001 */
/* FSUB DRm,DRn PR=1 1111nnn0mmm00001 */
static inline void FSUB(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		n = n & 14;
		m = m & 14;
		FP_RFD(n) = FP_RFD(n) - FP_RFD(m);
	} else {              /* PR = 0 */
		FP_RFS(n) = FP_RFS(n) - FP_RFS(m);
	}
}


/* FMUL FRm,FRn PR=0 1111nnnnmmmm0010 */
/* FMUL DRm,DRn PR=1 1111nnn0mmm00010 */
static inline void FMUL(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		n = n & 14;
		m = m & 14;
		FP_RFD(n) = FP_RFD(n) * FP_RFD(m);
	} else {              /* PR = 0 */
		FP_RFS(n) = FP_RFS(n) * FP_RFS(m);
	}
}

/* FDIV FRm,FRn PR=0 1111nnnnmmmm0011 */
/* FDIV DRm,DRn PR=1 1111nnn0mmm00011 */
static inline void FDIV(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		n = n & 14;
		m = m & 14;
		if (FP_RFD(m) == 0)
			return;
		FP_RFD(n) = FP_RFD(n) / FP_RFD(m);
	} else {              /* PR = 0 */
		if (FP_RFS(m) == 0)
			return;
		FP_RFS(n) = FP_RFS(n) / FP_RFS(m);
	}
}

/* FMAC FR0,FRm,FRn PR=0 1111nnnnmmmm1110 */
static inline void FMAC(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	if (m_fpu_pr == 0) { /* PR = 0 */
		FP_RFS(n) = (FP_RFS(0) * FP_RFS(m)) + FP_RFS(n);
	}
}

/* FSQRT FRn PR=0 1111nnnn01101101 */
/* FSQRT DRn PR=1 1111nnnn01101101 */
static inline void FSQRT(const UINT16 opcode)
{
	UINT32 n = Rn;

	if (m_fpu_pr) { /* PR = 1 */
		n = n & 14;
		if (FP_RFD(n) < 0)
			return;
		FP_RFD(n) = sqrtf(FP_RFD(n));
	} else {              /* PR = 0 */
		if (FP_RFS(n) < 0)
			return;
		FP_RFS(n) = sqrtf(FP_RFS(n));
	}
}

/* FSRRA FRn PR=0 1111nnnn01111101 */
static inline void FSRRA(const UINT16 opcode)
{
	UINT32 n = Rn;

	if (FP_RFS(n) < 0)
		return;
	FP_RFS(n) = 1.0 / sqrtf(FP_RFS(n));
}

/*  FSSCA FPUL,FRn PR=0 1111nnn011111101 */
void FSSCA(const UINT16 opcode)
{
	UINT32 n = Rn;

float angle;

	angle = (((float)(m_fpul & 0xFFFF)) / 65536.0) * 2.0 * M_PI;
	FP_RFS(n) = sinf(angle);
	FP_RFS(n+1) = cosf(angle);
}

/* FIPR FVm,FVn PR=0 1111nnmm11101101 */
static inline void FIPR(const UINT16 opcode)
{
	UINT32 n = Rn;

UINT32 m;
float ml[4];
int a;

	m = (n & 3) << 2;
	n = n & 12;
	for (a = 0;a < 4;a++)
		ml[a] = FP_RFS(n+a) * FP_RFS(m+a);
	FP_RFS(n+3) = ml[0] + ml[1] + ml[2] + ml[3];
}

/* FTRV XMTRX,FVn PR=0 1111nn0111111101 */
void FTRV(const UINT16 opcode)
{
	UINT32 n = Rn;

int i,j;
float sum[4];

	n = n & 12;
	for (i = 0;i < 4;i++) {
		sum[i] = 0;
		for (j=0;j < 4;j++)
			sum[i] += FP_XFS((j << 2) + i)*FP_RFS(n + j);
	}
	for (i = 0;i < 4;i++)
		FP_RFS(n + i) = sum[i];
}

static inline void op1111_0xf13(const UINT16 opcode)
{
	if (opcode & 0x100) {
			if (opcode & 0x200) {
				switch (opcode & 0xC00)
				{
					case 0x000:
						FSCHG();
						break;
					case 0x800:
						FRCHG();
						break;
					default:
						//debugger_break(machine());
						break;
				}
			} else {
				FTRV(opcode);
			}
		} else {
			FSSCA(opcode);
		}
}

static void dbreak(const UINT16 opcode)
{
	//debugger_break(machine());
}


static inline void op1111_0x13(UINT16 opcode)
{
	switch((opcode >> 4) & 0x0f)
	{
		case 0x00:  FSTS(opcode); break;
		case 0x01:  FLDS(opcode); break;
		case 0x02:  FLOAT(opcode); break;
		case 0x03:  FTRC(opcode); break;
		case 0x04:  FNEG(opcode); break;
		case 0x05:  FABS(opcode); break;
		case 0x06:  FSQRT(opcode); break;
		case 0x07:  FSRRA(opcode); break;
		case 0x08:  FLDI0(opcode); break;
		case 0x09:  FLDI1(opcode); break;
		case 0x0a:  FCNVSD(opcode); break;
		case 0x0b:  FCNVDS(opcode); break;
		case 0x0c:  dbreak(opcode); break;
		case 0x0d:  dbreak(opcode); break;
		case 0x0e:  FIPR(opcode); break;
		case 0x0f:  op1111_0xf13(opcode); break;
	}
}


/*****************************************************************************
 *  MAME CPU INTERFACE
 *****************************************************************************/

static void sh34_device_reset()
{
	m_ppc = 0;
	m_spc = 0;
	m_pr = 0;
	m_sr = 0;
	m_ssr = 0;
	m_gbr = 0;
	m_vbr = 0;
	m_mach = 0;
	m_macl = 0;
	memset(m_r, 0, sizeof(m_r));
	memset(m_rbnk, 0, sizeof(m_rbnk));
	m_sgr = 0;
	memset(m_fr, 0, sizeof(m_fr));
	memset(m_xf, 0, sizeof(m_xf));
	m_ea = 0;
	m_delay = 0;
	m_cpu_off = 0;
	m_pending_irq = 0;
	m_test_irq = 0;
	memset(m_exception_priority, 0, sizeof(m_exception_priority));
	memset(m_exception_requesting, 0, sizeof(m_exception_requesting));
	memset(m_m, 0, sizeof(m_m));
	memset(m_sh3internal_upper, 0, sizeof(m_sh3internal_upper));
	memset(m_sh3internal_lower, 0, sizeof(m_sh3internal_lower));
	memset(m_irq_line_state, 0, sizeof(m_irq_line_state));
	m_SH4_TSTR = 0;
	m_SH4_TCNT0 = 0;
	m_SH4_TCNT1 = 0;
	m_SH4_TCNT2 = 0;
	m_SH4_TCR0 = 0;
	m_SH4_TCR1 = 0;
	m_SH4_TCR2 = 0;
	m_SH4_TCOR0 = 0;
	m_SH4_TCOR1 = 0;
	m_SH4_TCOR2 = 0;
	m_SH4_TOCR = 0;
	m_SH4_TCPR2 = 0;
	m_SH4_IPRA = 0;
	m_SH4_IPRC = 0;
	m_SH4_SAR0 = 0;
	m_SH4_SAR1 = 0;
	m_SH4_SAR2 = 0;
	m_SH4_SAR3 = 0;
	m_SH4_DAR0 = 0;
	m_SH4_DAR1 = 0;
	m_SH4_DAR2 = 0;
	m_SH4_DAR3 = 0;
	m_SH4_CHCR0 = 0;
	m_SH4_CHCR1 = 0;
	m_SH4_CHCR2 = 0;
	m_SH4_CHCR3 = 0;
	m_SH4_DMATCR0 = 0;
	m_SH4_DMATCR1 = 0;
	m_SH4_DMATCR2 = 0;
	m_SH4_DMATCR3 = 0;
	m_SH4_DMAOR = 0;
	m_nmi_line_state = 0;
	m_frt_input = 0;
	m_internal_irq_vector = 0;
	m_refresh_timer_base = 0;
	memset(m_dma_timer_active, 0, sizeof(m_dma_timer_active));
	memset(m_dma_source, 0, sizeof(m_dma_source));
	memset(m_dma_destination, 0, sizeof(m_dma_destination));
	memset(m_dma_count, 0, sizeof(m_dma_count));
	memset(m_dma_wordsize, 0, sizeof(m_dma_wordsize));
	memset(m_dma_source_increment, 0, sizeof(m_dma_source_increment));
	memset(m_dma_destination_increment, 0, sizeof(m_dma_destination_increment));
	memset(m_dma_mode, 0, sizeof(m_dma_mode));
	m_ioport16_pullup = 0;
	m_ioport16_direction = 0;
	m_ioport4_pullup = 0;
	m_ioport4_direction = 0;

	sh4_default_exception_priorities();

	//m_rtc_timer->adjust(attotime::from_hz(128)); sh4 only.

	m_pc = 0xa0000000;
	m_r[15] = RL(4);
	m_sr = 0x700000f0;
	m_fpscr = 0x00040001;
	m_fpu_sz = (m_fpscr & SZ) ? 1 : 0;
	m_fpu_pr = (m_fpscr & PR) ? 1 : 0;
	m_fpul = 0;
	m_dbr = 0;

	m_internal_irq_level = -1;
	m_irln = 15;
	m_sleep_mode = 0;

	m_sh4_mmu_enabled = 0;

	m_sh4_icount = 0;
	sh3_total_cycles = 0;
	sh3_end_run = 0;

	memset(AreaWS1, 0, sizeof(AreaWS1));
	memset(AreaWS2, 0, sizeof(AreaWS2));
	area_cur = 0;
	area_last = 0;
	area_write = 0;
}

/*-------------------------------------------------
    sh3_reset - reset the processor
-------------------------------------------------*/

void Sh3Reset()
{
	Cv1kV2InvalidateAll();
	sh34_device_reset();

	m_SH4_TCOR0 = 0xffffffff;
	m_SH4_TCNT0 = 0xffffffff;
	m_SH4_TCOR1 = 0xffffffff;
	m_SH4_TCNT1 = 0xffffffff;
	m_SH4_TCOR2 = 0xffffffff;
	m_SH4_TCNT2 = 0xffffffff;

	m_timer[0].reset();
	m_timer[1].reset();
	m_timer[2].reset();

	m_dma_timer[0].reset();
	m_dma_timer[1].reset();
	m_dma_timer[2].reset();
	m_dma_timer[3].reset();

	cave_blitter_delay.reset();
}

#if 0
// no sh4 -dink
void sh4_base_device::device_reset()
{
	device_reset();

	m_m[RCR2] = 0x09;
	m_SH4_TCOR0 = 0xffffffff;
	m_SH4_TCNT0 = 0xffffffff;
	m_SH4_TCOR1 = 0xffffffff;
	m_SH4_TCNT1 = 0xffffffff;
	m_SH4_TCOR2 = 0xffffffff;
	m_SH4_TCNT2 = 0xffffffff;
}
#endif

#define SH3_DEC_RET void
#define SH3_DEC_NAME(name) name
#define SH3_DEC_SUB(name) name(opcode)
#define SH3_DEC_OP(name) name(opcode)
#define SH3_DEC_DEFAULT() return
#include "sh3_decode.inc"
#undef SH3_DEC_RET
#undef SH3_DEC_NAME
#undef SH3_DEC_SUB
#undef SH3_DEC_OP
#undef SH3_DEC_DEFAULT

#if defined(__GNUC__) && defined(__linux__) && (defined(__aarch64__) || defined(SH3_INTERPRETER_TEST))
#define SH3_FLAT_DISPATCH 1
enum Sh3OpId {
#define SH3_OPCODE(name) SH3_ID_##name,
#include "sh3_decode_ops.inc"
#undef SH3_OPCODE
	SH3_ID_COUNT
};
static_assert(SH3_ID_COUNT <= 256, "SH3 decode IDs exceed one byte");
#define SH3_DEC_RET Sh3OpId
#define SH3_DEC_NAME(name) id_##name
#define SH3_DEC_SUB(name) return id_##name(opcode)
#define SH3_DEC_OP(name) return SH3_ID_##name
#define SH3_DEC_DEFAULT() return SH3_ID_NOP
#include "sh3_decode.inc"
#undef SH3_DEC_RET
#undef SH3_DEC_NAME
#undef SH3_DEC_SUB
#undef SH3_DEC_OP
#undef SH3_DEC_DEFAULT
static UINT8 sh3OpIds[65536];
// Opcode identity (not operands): top nibble plus low byte, except
// groups 8/C whose handler depends on the second nibble.
static inline unsigned Sh3DispatchKey(UINT16 op)
{
	return ((op&0xf000u)>>4) |
		(((op&0xb000u)==0x8000u)?((op>>8)&15u):(op&255u));
}
static bool Sh3DispatchKeysValid()
{
	INT16 ids[4096];
	for (auto &id:ids) id=-1;
	for (unsigned op=0;op<65536;++op) {
		INT16 &id=ids[Sh3DispatchKey(UINT16(op))];
		if (id>=0 && id!=sh3OpIds[op]) return false;
		id=sh3OpIds[op];
	}
	return true;
}
static void InitFlatDecode()
{
	for (unsigned op=0; op<65536; op++) sh3OpIds[op] = id_execute_one(UINT16(op));
}
#else
#define SH3_FLAT_DISPATCH 0
#endif

	static inline void sh4_check_pending_irq(/*const char *message*/) // look for highest priority active exception and handle it
	{
		int a,irq,z;

		irq = 0;
		z = -1;
		for (a=0;a <= SH4_INTC_ROVI;a++)
		{
			if (m_exception_requesting[a])
			{
				if ((int)m_exception_priority[a] > z)
				{
					z = m_exception_priority[a];
					irq = a;
				}
			}
		}
		if (z >= 0)
		{
			sh4_exception(/*message, */irq);
		}
	}


static bool cv1kNativeEnabled = false;
static bool cv1kFlatEnabled = false;
static bool cv1kCompactEnabled = false;
static bool cv1kBatchEnabled = false;
static bool cv1kFetchPageEnabled = false;
static bool cv1kDispatch8Enabled = false;
static bool cv1kSlowFetchReuseEnabled = false;
static bool cv1kDirectMemEnabled = false;
static bool cv1kSlotDispatchEnabled = false;
static bool cv1kFlowEnabled = false;
// GCC AArch64 supports symbolic "S" operands for local static objects
// and GNU label addresses. Other toolchains retain portable C dispatch.
#if defined(__aarch64__) && !defined(__AARCH64EB__) && defined(__linux__) && !defined(__ANDROID__) && defined(__GNUC__) && !defined(__clang__) && SH3_FLAT_DISPATCH
#define SH3_CV1K_PCREL 1
#else
#define SH3_CV1K_PCREL 0
#endif
static bool cv1kPCRelEnabled = false;
static bool cv1kV2Enabled = false;
static bool cv1kPollEnabled=false;
static Sh3Cv1kPollStats cv1kPollStats;
struct Cv1kPollReject {UINT32 pc;unsigned cooldown;};
static Cv1kPollReject cv1kPollReject[1024];
static bool cv1kInterpLoopsEnabled = false;
static bool cv1kDataLoopsEnabled=false;
static Sh3Cv1kDataStats cv1kDataStats;
static bool cv1kDataFullEnabled=false;
static UINT32 cv1kDataHeadPC=~UINT32(0);
static Sh3Cv1kDataFullStats cv1kDataFullStats;

static bool cv1kHybridEnabled = false;
static bool cv1kLoopEnabled = false;
#if SH3_FLAT_DISPATCH
static bool cv1kCompactActive = false;
static bool cv1kBatchActive = false;
static bool cv1kFetchPageActive = false;
#if defined(SH3_INTERPRETER_TEST)
static bool cv1kDispatch8Active = false;
static UINT64 cv1kSlowFetchReuseHits;
static bool cv1kDirectMemActive;
static UINT64 cv1kDirectMoveHits,cv1kSlotDispatchHits;
static bool cv1kFlowActive,cv1kPCRelActive;
static UINT64 cv1kFlowBranchHits,cv1kFlowSlotHits,cv1kFlowStackHits,cv1kFlowDTHits,cv1kFlowSeqHits;
#endif
#endif
static UINT64 cv1kNativeOps, cv1kNativeSpans, cv1kNativeBuilt;
static UINT64 cv1kNativeMisses, cv1kNativeRejected, cv1kNativeFull;
static UINT64 cv1kLoopAttempts, cv1kLoopHits, cv1kLoopOps;
static UINT64 cv1kInterpLoopBlocks,cv1kInterpLoopLaps,cv1kInterpLoopOps,cv1kInterpLoopKinds[3];
#if SH3_FLAT_DISPATCH
#include "sh3_cv1k_opstat.inc"
#include "sh3_cv1k_direct.inc"
#include "sh3_cv1k_flow.inc"
#include "sh3_cv1k_interp_loops.inc"
#include "sh3_cv1k_data_loops.inc"
#include "sh3_cv1k_poll.inc"
#endif
#if SH3_NATIVE
static_assert(SH3_SHIFT==16 && SH3_MAXHANDLER==8 && AM==0x1fffffff && T==1,
	"native SH3 guards must match the interpreter");
struct Cv1kSpan {
	UINT32 pc, offset;
	UINT16 ops[Sh3Native::MaxOps];
	UINT8 count, checked, hot, shortExits;
	UINT8 body;
	bool valid, loop, delay;
};
static const unsigned Cv1kSets=8192, Cv1kWays=4, Cv1kHot=8, Cv1kMin=8;
struct Cv1kSet {
	Cv1kSpan ways[Cv1kWays];
	UINT8 next;
};
static Cv1kSet cv1kSets[Cv1kSets];
// A tagged front cache only skips the associative search, never validation.
static Cv1kSpan *cv1kFast[1024];
static Sh3Native::Cache cv1kCode;
static bool cv1kCodeFull;

static unsigned Cv1kHash(UINT32 pc)
{
	UINT32 h=pc>>1;
	h ^= h>>13; h *= 0x9e3779b1u; h ^= h>>16;
	return h & (Cv1kSets-1);
}

// Bounded, opt-in run-entry sampling. This deliberately has no per-instruction
// hook: PCs are biased toward timer-slice boundaries, not an instruction census.
struct Cv1kHotPC { UINT32 pc, hits; UINT16 opcode; };
static Cv1kHotPC cv1kHotPC[1024];
static bool cv1kSampleEnabled;
static unsigned cv1kSamples, cv1kSampleDropped;
static UINT32 cv1kSampleRng;
static void Cv1kSamplePC()
{
	if(m_cpu_off)return;
	// Decimate slice-entry sampling so the bounded capture covers
	// thousands of frames instead of only the initial loading screen.
	cv1kSampleRng^=cv1kSampleRng<<13;
	cv1kSampleRng^=cv1kSampleRng>>17;
	cv1kSampleRng^=cv1kSampleRng<<5;
	if(cv1kSampleRng&15)return;
	const UINT32 pc=m_delay?m_delay:m_pc, physical=pc&AM;
	const UINT8 *page=MemMapF[physical>>SH3_SHIFT];
	if((uintptr_t)page<SH3_MAXHANDLER || (pc&1))return;
	const UINT16 opcode=*(const UINT16 *)(page+(physical&SH3_PAGEM));
	const unsigned hash=((pc>>1)^(pc>>11))&1023;
	unsigned i=0;
	for(;i<4;i++) {
		Cv1kHotPC &entry=cv1kHotPC[(hash+i)&1023];
		if(!entry.hits || entry.pc==pc) {entry.pc=pc;entry.opcode=opcode;entry.hits++;break;}
	}
	if(i==4)cv1kSampleDropped++;
	if((++cv1kSamples&4095)==0) {
		bprintf(PRINT_UI,_T("CV1K HOT samples=%u dropped=%u native_ops=%llu spans=%llu built=%llu (slice-entry samples)\n"),
			cv1kSamples,cv1kSampleDropped,(unsigned long long)cv1kNativeOps,
			(unsigned long long)cv1kNativeSpans,(unsigned long long)cv1kNativeBuilt);
		unsigned top[8];
		for(unsigned rank=0;rank<8;rank++) {
			unsigned best=1024;
			for(unsigned j=0;j<1024;j++) {
				bool used=false;for(unsigned k=0;k<rank;k++)if(top[k]==j)used=true;
				if(!used && cv1kHotPC[j].hits && (best==1024 || cv1kHotPC[j].hits>cv1kHotPC[best].hits))best=j;
			}
			top[rank]=best;
			if(best!=1024)bprintf(PRINT_UI,_T("CV1K HOT pc=%08x opcode=%04x samples=%u\n"),
				cv1kHotPC[best].pc,unsigned(cv1kHotPC[best].opcode),cv1kHotPC[best].hits);
		}
	}
	if(cv1kSamples==32768)cv1kSampleEnabled=false;
}

static bool Cv1kTryNative(UINT16 opcode)
{
	if (!Sh3Native::Candidate(opcode) || cv1kCode.Denied() || m_test_irq ||
		m_delay || m_sh4_icount < INT32(Cv1kMin) || (m_pc & 1)) return false;
	const UINT32 pc=m_pc&AM, inpage=pc&SH3_PAGEM;
	const UINT8 *page=MemMapF[pc>>SH3_SHIFT];
	if ((uintptr_t)page<SH3_MAXHANDLER) return false;
	const UINT16 *code=(const UINT16 *)(page+inpage);
	unsigned available=(SH3_PAGE_SIZE-inpage)/2;
	if (available<(cv1kLoopEnabled?2:Cv1kMin)) return false;
	if (available>Sh3Native::MaxOps) available=Sh3Native::MaxOps;
	Cv1kSet &set=cv1kSets[Cv1kHash(pc)];
	const unsigned fast=(pc>>1)&1023;
	Cv1kSpan *slot=cv1kLoopEnabled?cv1kFast[fast]:NULL;
	if(slot && (!slot->checked || slot->pc!=pc))slot=NULL;
	for (unsigned i=0;!slot && i<Cv1kWays;i++) {
		Cv1kSpan &w=set.ways[i];
		if(w.checked && w.pc==pc) {slot=&w;break;}
	}
	if (!slot) {
		cv1kNativeMisses++;
		if(cv1kCodeFull) return false;
		// Keep compiled spans resident. Prefer empty or non-native slots.
		for(unsigned i=0;i<Cv1kWays;i++) if(!set.ways[i].checked){slot=&set.ways[i];break;}
		if(!slot) for(unsigned i=0;i<Cv1kWays;i++) {
			unsigned j=(set.next+i)&(Cv1kWays-1);
			if(!set.ways[j].valid){slot=&set.ways[j];set.next=(j+1)&(Cv1kWays-1);break;}
		}
		if(!slot) {cv1kNativeRejected++;return false;}
		*slot={};slot->pc=pc;slot->checked=1;slot->ops[0]=opcode;slot->hot=1;
		return false;
	}
	if(cv1kLoopEnabled)cv1kFast[fast]=slot;
	Cv1kSpan &b=*slot;
	if(b.checked>available || memcmp(b.ops,code,b.checked*sizeof(UINT16))) {
		b={};b.pc=pc;b.checked=1;b.ops[0]=opcode;b.hot=1;
		return false;
	}
	if(!b.valid) {
		if(cv1kCodeFull || b.hot>Cv1kHot) return false;
		if(++b.hot<Cv1kHot) return false;
		// Scan cheap opcode candidates before entering the emitter. Negative
		// entries retain the full rejected prefix including its terminator.
		unsigned n=0;
		while(n<available && Sh3Native::Candidate(code[n])) n++;
		b.checked=n<available?n+1:n;
		unsigned regs=0;
		bool delay=false;
		bool loop=cv1kLoopEnabled && n>0 && n<available &&
			Sh3Native::LoopBranch(code[n],n,delay);
		if(loop && delay) {
			loop=n+1<available && Sh3Native::Candidate(code[n+1]) && !Sh3Native::MemoryOp(code[n+1]);
			// Include a rejected slot in the negative-cache validation too.
			if(n+1<available)b.checked=n+2;
		}
		if(loop) {
			for(unsigned i=0;i<n;i++)regs|=Sh3Native::Registers(code[i]);
			if(delay)regs|=Sh3Native::Registers(code[n+1]);
			loop=__builtin_popcount(regs)<=7;
		}
		memcpy(b.ops,code,b.checked*sizeof(UINT16));
		b.hot=Cv1kHot+1;
		// Mode 3 spends call/validation overhead on closed loops or longer
		// spans; modes 1/2 retain their original A/B policy.
		const unsigned min=cv1kLoopEnabled?16:Cv1kMin;
		if(!loop && n<min){cv1kNativeRejected++;return false;}
		UINT32 words[Sh3Native::MaxWords];
		Sh3Native::Emitter emit(words);
		if(loop)emit.BeginLoop(regs);
		unsigned count=0;
		for(;count<n;count++) if(!emit.Emit(code[count],count))break;
		if(count!=n || (!loop && count<min)){cv1kNativeRejected++;return false;}
		if(loop)emit.EndLoop(code[n],delay?code[n+1]:0,n,delay);
		else emit.Finish(count);
		if(!cv1kCode.Publish(words,emit.End()-words,b.offset)){
			// No repeated global resets or mprotect storms under pressure:
			// retain published code, interpret every new site when full.
			cv1kCodeFull=true;cv1kNativeFull++;
			return false;
		}
		b.body=n;b.loop=loop;b.delay=delay;
		b.count=loop?n+1+unsigned(delay):count;b.valid=true;cv1kNativeBuilt++;
	}
	if(b.count>m_sh4_icount)return false;
	unsigned iterations=0;
	if(b.loop) {
		cv1kLoopAttempts++;
		iterations=unsigned(m_sh4_icount)/(b.body+3);
		// Amortize entry costs; bound the packed result independently of
		// caller budget. Short tails retain original branch/IRQ semantics.
		if(iterations<2)return false;
		if(iterations>65535)iterations=65535;
	}
	unsigned done=cv1kCode.Run(b.offset,m_r,&m_sr,&m_ea,MemMapR,MemMapRL,MemMapW,code,b.count*sizeof(UINT16),iterations,m_pc);
	cv1kNativeSpans++;
	unsigned spent=done;
	UINT32 next=m_pc+done*2,ppc=next;
	if(b.loop) {
		const unsigned laps=done>>8,exit=done&255;
		done=laps*b.count;spent=laps*(b.body+3);
		if(exit==65) {
			done+=b.count;spent+=b.count;
			next=m_pc+b.count*2;ppc=next;
		} else if(exit==64 || exit==0) {
			next=m_pc;
			ppc=laps?(b.delay?m_pc:m_pc+(b.body+1)*2):m_ppc;
		} else {
			done+=exit;spent+=exit;
			next=m_pc+exit*2;ppc=next;
		}
		if(done){cv1kLoopHits++;cv1kLoopOps+=done;}
	}
	// Do not call the same generated code indefinitely if a hot MMIO
	// or alignment guard repeatedly leaves less than a useful span.
	if (done < Cv1kMin) {
		if (++b.shortExits >= 8) {
			b.valid = false;
			b.hot = Cv1kHot + 1;
			cv1kNativeRejected++;
		}
	} else b.shortExits = 0;
	if(!done)return false;
	m_pc=next;m_ppc=ppc;EAT(spent);
	cv1kNativeOps+=done;
	return true;
}
#endif

#include "sh3_cv1k_v2.inc"

void Sh3Cv1kFrameFence(bool on){Cv1kC4FrameScope(on);}

void Sh3Cv1kV2Report()
{
#if SH3_NATIVE_V2
	if(!cv1kV2Enabled)return;
	const auto &s=cv1kV2Stats;
	bprintf(PRINT_UI,_T("CV1K V2 total: probes %llu calls %llu ops %llu built %llu reject %llu changed %llu guard %llu full %llu negative %llu code %u bytes\n"),
		(unsigned long long)s.probes,(unsigned long long)s.hits,(unsigned long long)s.ops,
		(unsigned long long)s.built,(unsigned long long)s.rejected,(unsigned long long)s.changed,
		(unsigned long long)s.guards,(unsigned long long)s.full,(unsigned long long)s.negative,
		(unsigned)cv1kV2Code.Used());
	bprintf(PRINT_UI,_T("CV1K V2 frontend: samples %llu ready %llu setfull %llu retired %llu disabled %llu skipped %llu\n"),
		(unsigned long long)s.samples,(unsigned long long)s.ready,(unsigned long long)s.setFull,
		(unsigned long long)s.retired,(unsigned long long)s.disabled,(unsigned long long)s.skipped);
	bprintf(PRINT_UI,_T("CV1K V2 emitter: compact built %llu calls %llu ops %llu bytes %llu legacy built %llu resets %llu\n"),
		(unsigned long long)s.compactBuilt,(unsigned long long)s.compactCalls,
		(unsigned long long)s.compactOps,(unsigned long long)s.compactBytes,
		(unsigned long long)s.legacyBuilt,(unsigned long long)s.resets);
	bprintf(PRINT_UI,_T("CV1K V2 blocks: extended %llu terminal %llu exits %llu taken %llu nottaken %llu cycles %llu\n"),
		(unsigned long long)s.extendedBuilt,(unsigned long long)s.terminalBuilt,
		(unsigned long long)s.terminalCalls,(unsigned long long)s.taken,
		(unsigned long long)s.notTaken,(unsigned long long)s.cycles);
	bprintf(PRINT_UI,_T("CV1K V2 audit: pages %u dirty %llu untracked %llu opaque %llu\n"),
		cv1kV2WatchCount,(unsigned long long)s.dirtyChecks,(unsigned long long)s.untracked,
		(unsigned long long)cv1kV2OpaqueEpoch);
	bprintf(PRINT_UI,_T("CV1K V2 chain: groups %llu joined %llu selflaps %llu selfbuilt %llu\n"),
		(unsigned long long)s.chainGroups,(unsigned long long)s.chainJoined,
		(unsigned long long)s.selfLaps,(unsigned long long)s.selfBuilt);
	bprintf(PRINT_UI,_T("CV1K V2 verify: checked %llu skipped %llu slices %llu\n"),
		(unsigned long long)s.verifyChecks,(unsigned long long)s.verifySkipped,(unsigned long long)cv1kV2SliceEpoch);
	bprintf(PRINT_UI,_T("CV1K V2 links: groups %llu bodies %llu edges %llu patches %llu built %llu stops %llu\n"),
		(unsigned long long)s.linkGroups,(unsigned long long)s.linkBodies,(unsigned long long)s.linkEdges,
		(unsigned long long)s.linkPatched,(unsigned long long)s.linkBuilt,(unsigned long long)s.linkStops);
	if(cv1kC3Enabled && !cv1kC4Enabled)bprintf(PRINT_UI,_T("CV1K V3 resident: groups %llu blocks %llu ops %llu built %llu reject %llu invalid %llu checks %llu\n"),
		(unsigned long long)s.residentGroups,(unsigned long long)s.residentBlocks,
		(unsigned long long)s.residentOps,(unsigned long long)s.residentBuilt,
		(unsigned long long)s.residentReject,(unsigned long long)s.residentInvalid,
		(unsigned long long)s.residentChecks);
	if(cv1kC4Enabled){
		bprintf(PRINT_UI,_T("CV1K V4 global: groups %llu blocks %llu ops %llu built %llu patches %llu checks %llu invalid %llu eager %llu full %llu fastDT %llu\n"),
			(unsigned long long)s.globalGroups,(unsigned long long)s.globalBlocks,
			(unsigned long long)s.globalOps,(unsigned long long)s.globalBuilt,
			(unsigned long long)s.globalPatched,(unsigned long long)s.globalChecks,
			(unsigned long long)s.globalInvalid,(unsigned long long)s.globalEager,
			(unsigned long long)s.globalFull,(unsigned long long)s.globalDT);
		bprintf(PRINT_UI,_T("CV1K V4 exits: fence %llu cold %llu budget %llu guard %llu skipped %llu cooldown %llu masked %llu reenabled %llu\n"),
			(unsigned long long)s.globalFence,(unsigned long long)s.globalCold,
			(unsigned long long)s.globalBudget,(unsigned long long)s.globalGuard,
			(unsigned long long)s.globalSkipped,(unsigned long long)s.globalCooldown,
			(unsigned long long)s.globalMasked,(unsigned long long)s.globalReenabled);
	}
#endif
}

void Sh3Cv1kOpStatsFrame()
{
#if SH3_FLAT_DISPATCH && defined(SH3_CV1K_OPSTAT) && SH3_CV1K_OPSTAT
	if(cv1kFetchPageEnabled)Cv1kOpStatReport();
#endif
}

void Sh3SetCv1kNative(INT32 enable)
{
#if SH3_FLAT_DISPATCH && defined(SH3_CV1K_OPSTAT) && SH3_CV1K_OPSTAT
	Cv1kOpStatReset();
	if(enable)bprintf(PRINT_UI,_T("CV1000 opcode/path counters: enabled (diagnostic build, timing overhead)\n"));
#endif
	cv1kNativeEnabled = cv1kFlatEnabled = cv1kCompactEnabled = cv1kBatchEnabled = cv1kHybridEnabled = cv1kLoopEnabled = false;
	cv1kFetchPageEnabled = cv1kInterpLoopsEnabled = false;
	cv1kDispatch8Enabled=cv1kSlowFetchReuseEnabled=false;
	cv1kDirectMemEnabled=cv1kSlotDispatchEnabled=cv1kFlowEnabled=cv1kPCRelEnabled=false;
	cv1kV2Enabled=false;
#if SH3_NATIVE_V2
	Cv1kV2Reset();
#endif
	cv1kPollEnabled=cv1kTimerFastEnabled=false;
	cv1kDataLoopsEnabled=false;cv1kDataStats={};
	cv1kDataFullEnabled=false;cv1kDataHeadPC=~UINT32(0);cv1kDataFullStats={};
#if SH3_FLAT_DISPATCH
	memset(cv1kDataPlans,0,sizeof(cv1kDataPlans));
#endif
	cv1kPollStats={};memset(cv1kPollReject,0,sizeof(cv1kPollReject));
	cv1kInterpLoopBlocks=cv1kInterpLoopLaps=cv1kInterpLoopOps=0;
	memset(cv1kInterpLoopKinds,0,sizeof(cv1kInterpLoopKinds));
#if SH3_FLAT_DISPATCH
	cv1kCompactActive=cv1kBatchActive=cv1kFetchPageActive=false;
#if defined(SH3_INTERPRETER_TEST)
	cv1kDispatch8Active=false;cv1kSlowFetchReuseHits=0;
	cv1kDirectMemActive=false;cv1kDirectMoveHits=cv1kSlotDispatchHits=0;
	cv1kFlowActive=cv1kPCRelActive=false;cv1kFlowBranchHits=cv1kFlowSlotHits=cv1kFlowStackHits=cv1kFlowDTHits=cv1kFlowSeqHits=0;
#endif
#endif
	cv1kNativeOps=cv1kNativeSpans=cv1kNativeBuilt=0;
	cv1kNativeMisses=cv1kNativeRejected=cv1kNativeFull=0;
	cv1kLoopAttempts=cv1kLoopHits=cv1kLoopOps=0;
#if SH3_NATIVE
	cv1kCode.Reset();cv1kCode.Enable();cv1kCodeFull=false;
	memset(cv1kSets,0,sizeof(cv1kSets));
	memset(cv1kFast,0,sizeof(cv1kFast));
	const char *sample=getenv("FBNEO_CV1K_HOTPCS");
	cv1kSampleEnabled=enable && sample && !strcmp(sample,"1");
	cv1kSamples=cv1kSampleDropped=0;
	cv1kSampleRng=0x9e3779b9u;
	if(cv1kSampleEnabled){
		memset(cv1kHotPC,0,sizeof(cv1kHotPC));
		bprintf(PRINT_UI,_T("CV1000 SH3 hot PC sampling: enabled (approx 1/16 slice entries, max 32768)\n"));
	}
	// Actual RK3566 trace showed significant short-span overhead. Do not
	// automatically enable experimental JIT on any CPU until measured.
	const char *option=getenv("FBNEO_CV1K_JIT");
	cv1kNativeEnabled=enable && option && (!strcmp(option,"1") || !strcmp(option,"2") || !strcmp(option,"3"));
	cv1kLoopEnabled=cv1kNativeEnabled && !strcmp(option,"3");
	cv1kHybridEnabled=cv1kNativeEnabled && (cv1kLoopEnabled || !strcmp(option,"2"));
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 native spans: %s (opt-in, hot >=8 ops)\n"),
		cv1kNativeEnabled?(cv1kLoopEnabled?_T("closed loops (JIT=3)"):cv1kHybridEnabled?_T("hybrid"):_T("enabled")):_T("disabled"));
#endif
#if SH3_FLAT_DISPATCH
	const char *flat=getenv("FBNEO_CV1K_DISPATCH");
	cv1kFlatEnabled=enable && (!cv1kNativeEnabled || cv1kHybridEnabled) && (!flat || strcmp(flat,"0")!=0);
	if(cv1kFlatEnabled)InitFlatDecode();
	const char *compact=getenv("FBNEO_CV1K_COMPACT");
	cv1kCompactEnabled=cv1kFlatEnabled && (!compact || strcmp(compact,"0")!=0);
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 compact dispatch: %s\n"),
		cv1kCompactEnabled?_T("enabled"):_T("disabled"));
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 flat dispatch: %s\n"),
		cv1kFlatEnabled?_T("enabled"):_T("disabled"));
	const char *batch=getenv("FBNEO_CV1K_BATCH");
	cv1kBatchEnabled=cv1kFlatEnabled && cv1kCompactEnabled && (!batch || strcmp(batch,"0")!=0);
	const char *fetchPage=getenv("FBNEO_CV1K_FETCH_PAGE");
	cv1kFetchPageEnabled=cv1kBatchEnabled && !cv1kNativeEnabled && (!fetchPage || strcmp(fetchPage,"0")!=0);
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 pure fetch-page reuse: %s\n"),
		cv1kFetchPageEnabled?_T("enabled"):_T("disabled"));
	const char *dispatch8=getenv("FBNEO_CV1K_DISPATCH8");
	cv1kDispatch8Enabled=cv1kFetchPageEnabled && dispatch8 && !strcmp(dispatch8,"1") &&
		Sh3DispatchKeysValid();
	const char *reuseSlow=getenv("FBNEO_CV1K_FETCH_REUSE");
	cv1kSlowFetchReuseEnabled=cv1kFetchPageEnabled &&
		Cv1kPerformanceOption(reuseSlow,cv1kPerformanceDefault || cv1kDispatch8Enabled);
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 8 KiB dispatch: %s, sequential slow fetch reuse: %s\n"),
		cv1kDispatch8Enabled?_T("enabled (opt-in)"):_T("disabled"),
		cv1kSlowFetchReuseEnabled?_T("enabled"):_T("disabled"));
	const char *directMem=getenv("FBNEO_CV1K_DIRECT_MEM");
	const char *slotDispatch=getenv("FBNEO_CV1K_SLOT_DISPATCH");
	cv1kDirectMemEnabled=cv1kFetchPageEnabled && !cv1kDispatch8Enabled &&
		!SH3_WAIT_STATE_ACTIVE && Cv1kPerformanceOption(directMem);
	cv1kSlotDispatchEnabled=cv1kFetchPageEnabled && Cv1kPerformanceOption(slotDispatch);
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 batched direct memory: %s, decoded delay slots: %s\n"),
		cv1kDirectMemEnabled?_T("enabled"):_T("disabled"),
		cv1kSlotDispatchEnabled?_T("enabled"):_T("disabled"));
	const char *flow=getenv("FBNEO_CV1K_FLOW");
	cv1kFlowEnabled=cv1kDirectMemEnabled && cv1kSlotDispatchEnabled &&
		Cv1kPerformanceOption(flow);
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 branch/stack batching: %s\n"),
		cv1kFlowEnabled?_T("enabled"):_T("disabled"));
	cv1kPCRelEnabled=SH3_CV1K_PCREL && cv1kFlowEnabled &&
		Cv1kPerformanceOption(getenv("FBNEO_CV1K_PCREL_DISPATCH"),false);
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 PC-relative dispatch: %s\n"),
		cv1kPCRelEnabled?_T("enabled (page-map flow)"):_T("disabled"));
#if SH3_NATIVE_V2
	// Unlike V1, V2 keeps the fast interpreter and its special kernels.
	// Only this dedicated experimental branch defaults to V2 on native
	// Linux ARM64 libretro. Explicit V1 and other experiments win.
	cv1kV2Enabled=enable && cv1kFlowEnabled && !cv1kNativeEnabled &&
		!cv1kPCRelEnabled && !Cv1kPerformanceOption(getenv("FBNEO_CV1K_FAST_RAM"),false) &&
		Cv1kPerformanceOption(getenv("FBNEO_CV1K_JIT_V2")) && Cv1kV2Enable();
#endif
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 Dynarec V2: %s\n"),
		cv1kV2Enabled?
#if SH3_NATIVE_V2
		(cv1kC4Enabled?_T("enabled (stage C-4, global resident blocks)"):(cv1kC3Enabled?_T("enabled (stage C-3, resident regions)"):_T("enabled (stage C-2, guarded static links)")))
#else
		_T("enabled")
#endif
		:_T("disabled"));
#if SH3_NATIVE_V2
	if(cv1kV2Enabled)bprintf(PRINT_UI,_T("CV1000 SH3 V2 lookup-only: %s; 64 KiB pinned directory\n"),
		cv1kV2LookupOnly?_T("enabled (learn/compile/validate, no native execution)"):_T("disabled"));
	if(cv1kV2Enabled)bprintf(PRINT_UI,_T("CV1000 SH3 V2 compact emitter: %s; exact entry bitmap, bounded slice samples\n"),
		cv1kV2Compact?_T("enabled"):_T("disabled (legacy emitter)"));
	if(cv1kV2Enabled)bprintf(PRINT_UI,_T("CV1000 SH3 V2 basic blocks: %s\n"),
		cv1kV2Blocks?_T("enabled (literal/DT/terminal)"):_T("disabled (B-1 candidate scope)"));
	if(cv1kV2Enabled)bprintf(PRINT_UI,_T("CV1000 SH3 V2 write audit: %s; verification: %s\n"),
		cv1kV2WriteWatch?_T("8 KiB map"):_T("disabled"),
		cv1kV2VerifyAlways?_T("always"):_T("registered pages, dirty/slice fence"));
	if(cv1kV2Enabled)bprintf(PRINT_UI,_T("CV1000 SH3 V2 chaining: %s, self loops: %s\n"),
		cv1kV2Chain?_T("enabled"):_T("disabled"),cv1kV2SelfLoop?_T("enabled"):_T("disabled"));
	if(cv1kV2Enabled)bprintf(PRINT_UI,_T("CV1000 SH3 V2 native static links: %s\n"),
		cv1kV2LinkEnabled?_T("enabled (B rel26)"):_T("disabled"));
	if(cv1kC3Enabled && !cv1kC4Enabled)bprintf(PRINT_UI,_T("CV1000 SH3 V3 resident regions: enabled (experimental, direct B, RAS)\n"));
	if(cv1kC4Enabled)bprintf(PRINT_UI,_T("CV1000 SH3 V4 global: direct B/BR, Tier2, RAS, eager; fence %s, min ops %u\n"),
		cv1kC4FrameOption?_T("scoped CV1000 frame"):_T("CPU slice"),cv1kC4MinOps);
	if(cv1kV2RepeatOnly)bprintf(PRINT_UI,_T("CV1000 SH3 V2 self-repeat only: enabled\n"));
#endif
	const char *interpLoops=getenv("FBNEO_CV1K_INTERP_LOOPS");
	// Real mmpork A/B: substantially slower startup with fusion on.
	// Retain the experiment but require explicit opt-in.
	cv1kInterpLoopsEnabled=cv1kFetchPageEnabled && interpLoops && !strcmp(interpLoops,"1");
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 counted interpreter loops: %s\n"),
		cv1kInterpLoopsEnabled?_T("enabled"):_T("disabled"));
	const char *dataLoops=getenv("FBNEO_CV1K_DATA_LOOPS");
	cv1kDataLoopsEnabled=cv1kFetchPageEnabled && !cv1kInterpLoopsEnabled &&
		Cv1kPerformanceOption(dataLoops);
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 specialized data-copy loops: %s\n"),
		cv1kDataLoopsEnabled?_T("enabled"):_T("disabled"));
	const char *dataFull=getenv("FBNEO_CV1K_DATA_FULL");
	cv1kDataFullEnabled=cv1kDataLoopsEnabled && Cv1kPerformanceOption(dataFull);
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 full literal kernel: %s\n"),
		cv1kDataFullEnabled?_T("enabled"):_T("disabled"));
	const char *poll=getenv("FBNEO_CV1K_POLL");
	const char *timerFast=getenv("FBNEO_CV1K_TIMER_FAST");
	cv1kPollEnabled=cv1kFetchPageEnabled && Cv1kPerformanceOption(poll);
	cv1kTimerFastEnabled=enable && Cv1kPerformanceOption(timerFast);
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 fixed-point polling: %s, no-event timer advance: %s\n"),
		cv1kPollEnabled?_T("enabled"):_T("disabled"),
		cv1kTimerFastEnabled?_T("enabled"):_T("disabled"));
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 host arithmetic: %s, shared long-read tail: %s\n"),
		SH3_FAST_ARITHMETIC?_T("enabled"):_T("disabled"),
		SH3_SMALL_HANDLERS?_T("enabled"):_T("disabled"));
	// Hybrid mode uses the batched interpreter between native blocks.
	if (cv1kHybridEnabled && !cv1kBatchEnabled)
		cv1kLoopEnabled=cv1kHybridEnabled=cv1kNativeEnabled=false;
	if(enable)bprintf(PRINT_UI,_T("CV1000 SH3 batched dispatch: %s\n"),
		cv1kBatchEnabled?_T("enabled"):_T("disabled"));
#else
	(void)enable;
#endif
}

void Sh3Cv1kGetPollStats(Sh3Cv1kPollStats *stats){*stats=cv1kPollStats;}
void Sh3Cv1kGetDataStats(Sh3Cv1kDataStats *stats){*stats=cv1kDataStats;}
void Sh3Cv1kGetDataFullStats(Sh3Cv1kDataFullStats *stats){*stats=cv1kDataFullStats;}

void Sh3Cv1kCacheStats(UINT64 *misses, UINT64 *rejected, UINT64 *full)
{
	*misses=cv1kNativeMisses;*rejected=cv1kNativeRejected;*full=cv1kNativeFull;
}

void Sh3Cv1kNativeStats(UINT64 *instructions, UINT64 *spans, UINT64 *built)
{
	*instructions = cv1kNativeOps; *spans = cv1kNativeSpans; *built = cv1kNativeBuilt;
}

void Sh3Cv1kLoopStats(UINT64 *attempts, UINT64 *hits, UINT64 *instructions)
{
	*attempts=cv1kLoopAttempts;*hits=cv1kLoopHits;*instructions=cv1kLoopOps;
}
void Sh3Cv1kInterpLoopStats(UINT64 *blocks,UINT64 *laps,UINT64 *ops,UINT64 *kinds)
{
	*blocks=cv1kInterpLoopBlocks;*laps=cv1kInterpLoopLaps;*ops=cv1kInterpLoopOps;
	memcpy(kinds,cv1kInterpLoopKinds,sizeof(cv1kInterpLoopKinds));
}


template<bool Native> static int Sh3Run_timerhack_switch(int cycles)
{
	m_sh4_icount = cycles;
	sh3_end_run = 0;

	if (m_cpu_off)
	{
		m_sh4_icount = 0;
		sh3_total_cycles += cycles;
		return cycles;
	}

	do
	{
		if (m_delay)
		{
			const UINT16 opcode = sh3_cpu_readop16((UINT32)(m_delay & AM));

			m_delay = 0;
			m_ppc = m_pc;

			execute_one(opcode);
		}
		else
		{
			const UINT16 opcode = sh3_cpu_readop16((UINT32)(m_pc & AM));
#if SH3_NATIVE
			if (Native && Cv1kTryNative(opcode)) continue;
#endif
			m_pc += 2;
			m_ppc = m_pc;

			execute_one(opcode);
		}
		if (m_test_irq && !m_delay)
		{
			sh4_check_pending_irq(/*"mame_sh4_execute"*/);
		}

		EAT(1);
	} while( m_sh4_icount > 0);// && !sh3_end_run );

	cycles = cycles - m_sh4_icount;

	sh4_run_timers(cycles);

	m_sh4_icount = 0;

	return cycles;
}

#if SH3_FLAT_DISPATCH
// Single flat dispatch, retaining the exact original instruction handlers,
// IRQ check, delay-slot PC, CPU-off behavior and timerhack accounting.
template<bool Compact> static int Sh3Run_timerhack_flat(int cycles)
{
	static const void *const handlers[]={
#define SH3_OPCODE(name) &&sh3_op_##name,
#include "sh3_decode_ops.inc"
#undef SH3_OPCODE
	};
	// Only the compact specialization uses this table. The function's
	// label addresses are local to it; verify the signed displacement
	// range rather than assuming a particular compiler layout.
	static INT16 offsets[Compact?65536:1];
	static bool initialized=false, fits=false;
	const intptr_t base=(intptr_t)&&dispatch_base;
	if (Compact && !initialized) {
		fits=true;
		for (unsigned i=0;i<SH3_ID_COUNT;i++) {
			const intptr_t delta=(intptr_t)handlers[i]-base;
			if (delta<-32768 || delta>32767) {fits=false;break;}
		}
		if (fits) for (unsigned i=0;i<65536;i++)
			offsets[i]=(INT16)((intptr_t)handlers[sh3OpIds[i]]-base);
		initialized=true;
		if (!fits) bprintf(PRINT_UI,_T("CV1000 SH3 compact dispatch: range fallback\n"));
	}
	if (Compact && !fits) return Sh3Run_timerhack_flat<false>(cycles);
	if (Compact) cv1kCompactActive=true;
	m_sh4_icount=cycles;sh3_end_run=0;
	if(m_cpu_off){m_sh4_icount=0;sh3_total_cycles+=cycles;return cycles;}
	UINT16 opcode;
dispatch_base:
next:
	if(m_delay){
		opcode=sh3_cpu_readop16(UINT32(m_delay&AM));
		m_delay=0;m_ppc=m_pc;
	}else{
		opcode=sh3_cpu_readop16(UINT32(m_pc&AM));
		m_pc+=2;m_ppc=m_pc;
	}
	if (Compact) goto *(void *)(base+offsets[opcode]);
	goto *handlers[sh3OpIds[opcode]];
#define SH3_OPCODE(name) sh3_op_##name: name(opcode); goto finished;
#include "sh3_decode_ops.inc"
#undef SH3_OPCODE
finished:
	if(m_test_irq && !m_delay)sh4_check_pending_irq();
	EAT(1);
	if(m_sh4_icount>0)goto next;
	cycles-=m_sh4_icount;
	sh4_run_timers(cycles);
	m_sh4_icount=0;
	return cycles;
}
// Keep PC and cycle budget in host registers across consecutive pure
// operations. Slow handlers see exactly the published state they saw in
// the original interpreter (including memory callbacks and IRQ tests).
// Delay slots and pending IRQs use the original single-step path.
template<bool Hybrid, bool FetchPage=false, bool Dense=false, bool ReuseCapable=Dense, bool DirectMem=false, bool Flow=false, bool Regions=false, bool PCRelative=false, bool NativeV2=false> static int Sh3Run_timerhack_batch(int cycles)
{
	static const void *const handlers[]={
#define SH3_OPCODE(name) &&batch_op_##name,
#include "sh3_decode_ops.inc"
#undef SH3_OPCODE
	};
	// The PC-relative variant uses ADRP alone for the table address.
	// Alignment is a property of the static object and its ELF section,
	// not the host OS page size. Keep the compact signed 16-bit entries.
	alignas(PCRelative?4096:alignof(INT16)) static INT16 offsets[Dense?4096:65536];
	static bool initialized=false,fits=false;
	const intptr_t base=(intptr_t)&&batch_base;
	if (!initialized) {
		fits=!PCRelative || (((uintptr_t)offsets&4095u)==0);
		for (unsigned i=0;i<SH3_ID_COUNT;i++) {
			const intptr_t d=(intptr_t)handlers[i]-base;
			if (d<-32768 || d>32767) {fits=false;break;}
		}
		if (fits) for (unsigned i=0;i<65536;i++)
			offsets[Dense?Sh3DispatchKey(UINT16(i)):i]=(INT16)((intptr_t)handlers[sh3OpIds[i]]-base);
		initialized=true;
		if (!fits) bprintf(PRINT_UI,_T("CV1000 SH3 batched dispatch: range fallback\n"));
	}
	if (!fits) return Sh3Run_timerhack_flat<true>(cycles);
	cv1kBatchActive=true;
	if(FetchPage)cv1kFetchPageActive=true;
#if defined(SH3_INTERPRETER_TEST)
	if(Dense)cv1kDispatch8Active=true;
	if(DirectMem)cv1kDirectMemActive=true;
	if(Flow)cv1kFlowActive=true;
	if(PCRelative)cv1kPCRelActive=true;
#endif
	m_sh4_icount=cycles;sh3_end_run=0;
	if (m_cpu_off) {m_sh4_icount=0;sh3_total_cycles+=cycles;return cycles;}
	if(FetchPage && cv1kPollEnabled)Cv1kTryPoll();
	if(FetchPage && cv1kDataLoopsEnabled && m_sh4_icount>0 &&
		(!cv1kDataFullEnabled || m_pc!=cv1kDataHeadPC))Cv1kTryDataLoop();
	if(FetchPage && m_sh4_icount==0 && cycles>0){
		sh4_run_timers(cycles);
		m_sh4_icount=0;
		return cycles;
	}
	UINT32 pc=m_pc;
	INT32 remaining=m_sh4_icount,pure=0;
	const UINT16 *fetch=NULL;
	INT32 window=0;
	UINT32 fetchEpoch=0;
	UINT32 opcode;
	Cv1kBranchKind branchKind=CV1K_BRANCH_BF;
	if(NativeV2)Cv1kV2SliceSample();
start:
	// Enter native spans only at a run/control-flow boundary, not on
	// every interpreted instruction. Guest code is still live-checked.
	if (m_delay || m_test_irq) {
		const UINT32 slotPC=FetchPage?m_delay:0;
		if (m_delay) {
			opcode=sh3_cpu_readop16(UINT32(m_delay&AM));
			m_delay=0;m_ppc=m_pc;
		} else {
			opcode=sh3_cpu_readop16(UINT32(m_pc&AM));
			m_pc+=2;m_ppc=m_pc;
		}
		CV1K_VISIT(opcode);
		if(FetchPage && cv1kSlotDispatchEnabled && slotPC){
#if defined(SH3_INTERPRETER_TEST)
			++cv1kSlotDispatchHits;
#endif
			Cv1kDispatchSlot(opcode);
		}else execute_one(opcode);
		const bool pending=m_test_irq!=0;
		if (m_test_irq && !m_delay) sh4_check_pending_irq();
		EAT(1);
		// Data kernels are tested only at the slice boundary or
		// after their narrowly matching backward branch/slot.
		if(FetchPage && cv1kDataLoopsEnabled && slotPC && !pending && !m_delay &&
			m_sh4_icount>=13){
			const UINT32 distance=slotPC-m_pc;
			if(distance==22 && (opcode&0xf00f)==0x600e &&
				(!cv1kDataFullEnabled || m_pc!=cv1kDataHeadPC))Cv1kTryDataLoop(1);
			else if(distance==8 && (opcode&0xf000)==0x7000 &&
				((opcode&255)==1 || (opcode&255)==2 || (opcode&255)==4))Cv1kTryDataLoop(3);
		}
		// After a pure delay slot, a short backward edge may enter
		// a counted loop. Read its *live* branch/body/slot afresh.
		if(FetchPage && cv1kInterpLoopsEnabled && slotPC && !pending && !m_delay &&
			m_sh4_icount>=10 && (opcode==0x0009 || (opcode&0xf000)==0x7000)){
			const UINT32 distance=slotPC-m_pc;
			if(distance==4 || distance==6 || distance==8)
				Cv1kTryInterpLoop(UINT16(0x8f00|UINT8(-INT32(distance/2+1))),true);
		}
		if (m_sh4_icount<=0) goto done;
		goto start;
	}
	pc=m_pc;remaining=m_sh4_icount;pure=0;
#if SH3_NATIVE
	if (Hybrid && Cv1kTryNative(sh3_cpu_readop16(pc&AM))) {
		if (m_sh4_icount<=0) goto done;
		// A full capped block may continue at the next block boundary.
		// Guard exits still interpret the failing instruction immediately.
		if (m_pc==pc+Sh3Native::MaxOps*2) goto start;
		pc=m_pc;remaining=m_sh4_icount;
		// Other exits interpret the next instruction without another lookup.
	}
#endif
reload_fetch:
	if(NativeV2 && Cv1kV2Try(pc,false,true)){
		if(m_sh4_icount<=0)goto done;
		pc=m_pc;remaining=m_sh4_icount;pure=0;
	}
reload_fetch_no_v2:
	if (FetchPage) {
		// Run entries, delay/IRQ boundaries and nonsequential exits
		// reload the live map. Dense mode may retain a sequential
		// pointer after a slow op if the map epoch is unchanged.
		// Do not cache opcodes: stores through aliases remain visible.
		const UINT32 address=pc&AM;
		fetch=(const UINT16 *)(MemMapF[address>>SH3_SHIFT]+(address&SH3_PAGEM));
		if(ReuseCapable)fetchEpoch=sh3FetchMapEpoch;
		window=(SH3_PAGE_SIZE-(address&SH3_PAGEM))/2;
		if (remaining<window) window=remaining;
	}
batch_base:
fast_next:
	opcode=FetchPage?*fetch++:sh3_cpu_readop16(pc&AM);
	if (Dense || ReuseCapable) {
		// Keep the fetched value in a register across dispatch. GCC
		// otherwise rematerializes the operand with a second ldrh.
		asm("" : "+r"(opcode));
	}
	CV1K_VISIT(opcode);
	if (!FetchPage) pc+=2;
#if SH3_CV1K_PCREL
	if(PCRelative){
		// Rematerialize two local addresses, rather than reloading
		// spilled copies on every guest opcode. No fixed host register
		// reservation or global memory clobber. The single output
		// limits register pressure; x16 is an explicitly clobbered
		// scratch. The whole-table memory input models the read
		// dependency on the initialized immutable offset array.
		intptr_t target;
		asm volatile("adrp %0, %2\n\tldrsh %0, [%0, %w1, uxtw #1]\n\tadr x16, %3\n\tadd %0, %0, x16"
			: "=&r"(target) : "r"(opcode), "S"(offsets), "S"(&&batch_base), "m"(offsets) : "x16");
		goto *(void *)target;
	}
#endif
	goto *(void *)(base+offsets[Dense?Sh3DispatchKey(opcode):opcode]);
#define SH3_SLOW_BODY(name) \
	if(Flow)goto shared_slow; \
	if (FetchPage) pc+=(pure+1)*2; \
	m_pc=m_ppc=pc; m_sh4_icount=remaining-(FetchPage?pure:0); sh3_total_cycles+=pure; \
	name(opcode); \
	if (FetchPage && (&name==&BF) && !(m_sr&T) && \
		(cv1kInterpLoopsEnabled || (opcode==0x8bfa && cv1kDataLoopsEnabled))) goto interp_bf; \
	goto slow_finished;
#define SH3_OPCODE(name) batch_op_##name: SH3_SLOW_BODY(name)
#define SH3_MEMORY_OPCODE(name,width,store,form) batch_op_##name: \
	if(DirectMem && Cv1kTryDirectMove<width,store,form,Regions>(opcode,pc+(pure+1)*2))goto direct_finished; \
	SH3_SLOW_BODY(name)
#define SH3_FLOW_OPCODE(name) batch_op_##name: \
	if(Flow){name(opcode);goto flow_seq;} \
	SH3_SLOW_BODY(name)
#if defined(SH3_INTERPRETER_TEST)
#define SH3_FLOW_TEST(counter) ++counter;
#else
#define SH3_FLOW_TEST(counter)
#endif
#define SH3_STACK_OPCODE(name,store,reg) batch_op_##name: \
	if(Flow && Cv1kTryStack<store,&Sh3HotState::reg,Regions>(opcode)){SH3_FLOW_TEST(cv1kFlowStackHits) goto flow_seq;} \
	SH3_SLOW_BODY(name)
#define SH3_DT_OPCODE(name) batch_op_##name: \
	if(Flow && Cv1kTryFlowDT<Regions>(opcode,pc+(pure+1)*2)){SH3_FLOW_TEST(cv1kFlowDTHits) goto flow_seq;} \
	SH3_SLOW_BODY(name)
#define SH3_BRANCH_OPCODE(name,kind) batch_op_##name: \
	if(Flow){ \
		if(((kind==CV1K_BRANCH_BF || kind==CV1K_BRANCH_BFS) && (m_sr&T)) || \
		   ((kind==CV1K_BRANCH_BT || kind==CV1K_BRANCH_BTS) && !(m_sr&T)))goto flow_seq; \
		branchKind=kind;goto flow_branch; \
	} \
	SH3_SLOW_BODY(name)
#define SH3_PURE_OPCODE(name) batch_op_##name: \
	if(FetchPage && (&name==&CMPPZ) && cv1kDataFullEnabled && \
		pc+pure*2==cv1kDataHeadPC && remaining-pure>=4)goto data_head; \
	name(opcode); if(Flow)goto batched_finished; \
	++pure; if (FetchPage?--window>0:--remaining>0) goto fast_next; \
	if (FetchPage) {remaining-=pure; pc+=pure*2;} \
	m_pc=m_ppc=pc; m_sh4_icount=remaining; sh3_total_cycles+=pure; \
	if (FetchPage && remaining>0) {pure=0; goto reload_fetch;} goto done;
#include "sh3_decode_ops.inc"
#undef SH3_PURE_OPCODE
#undef SH3_MEMORY_OPCODE
#undef SH3_BRANCH_OPCODE
#undef SH3_DT_OPCODE
#undef SH3_STACK_OPCODE
#undef SH3_FLOW_OPCODE
#undef SH3_OPCODE
#undef SH3_SLOW_BODY
#undef SH3_FLOW_TEST
flow_branch:
	{
		const UINT32 nextPC=pc+(pure+1)*2;
		Cv1kFlowResult r;
		if(Cv1kTryFlowBranch<Regions>(branchKind,opcode,nextPC,remaining-pure,
			fetchEpoch==sh3FetchMapEpoch?fetch:nullptr,r)){
#if defined(SH3_INTERPRETER_TEST)
			++cv1kFlowBranchHits;
			if(r.slot)++cv1kFlowSlotHits;
#endif
			const INT32 spent=pure+r.cost;
			remaining-=spent;m_sh4_icount=remaining;
			sh3_total_cycles+=spent;m_pc=r.target;m_ppc=r.ppc;
			pc=r.target;pure=0;
#if SH3_NATIVE_V2
			if(NativeV2 && cv1kC3Enabled && !cv1kC4Enabled &&
				(branchKind==CV1K_BRANCH_JSR || branchKind==CV1K_BRANCH_JMP))
				Cv1kC3RecordDynamic(nextPC-2,r.target);
#endif
			if(remaining<=0)goto done;
			// A committed control-flow boundary is a safe native
			// entry. A cache miss keeps the same-page fetch reuse.
			if(NativeV2 && Cv1kV2Try(pc,pc<nextPC,true)){
				if(m_sh4_icount<=0)goto done;
				pc=m_pc;remaining=m_sh4_icount;
				goto reload_fetch_no_v2;
			}
			// Reuse the source fetch page only for the same physical
			// page. A fused slot cannot call a mapping callback.
			if((((nextPC-2)^pc)&AM&~SH3_PAGEM)==0 && fetchEpoch==sh3FetchMapEpoch){
				fetch=fetch-(((nextPC-2)&SH3_PAGEM)/2+1)+(pc&SH3_PAGEM)/2;
				window=(SH3_PAGE_SIZE-(pc&SH3_PAGEM))/2;
				if(remaining<window)window=remaining;
				goto fast_next;
			}
			goto reload_fetch_no_v2;
		}
		// Unsupported slots, short slices and speedhack edges take
		// the exact original branch handler and post-branch probes.
		goto shared_slow;
	}
shared_slow:
	// The Flow dispatcher publishes state in one common cold tail.
	// The same decoder table as ordinary delay slots selects the
	// authoritative instruction handler, including every callback.
	if(FetchPage)pc+=(pure+1)*2;
	m_pc=m_ppc=pc;m_sh4_icount=remaining-(FetchPage?pure:0);sh3_total_cycles+=pure;
	Cv1kDispatchSlot(opcode);
	if(FetchPage && sh3OpIds[opcode]==SH3_ID_BF && !(m_sr&T) &&
		(cv1kInterpLoopsEnabled || (opcode==0x8bfa && cv1kDataLoopsEnabled)))goto interp_bf;
	goto slow_finished;
flow_seq:
#if defined(SH3_INTERPRETER_TEST)
	++cv1kFlowSeqHits;
#endif
	goto batched_finished;
direct_finished:
#if defined(SH3_INTERPRETER_TEST)
	++cv1kDirectMoveHits;
#endif
batched_finished:
	++pure;
	if(--window>0)goto fast_next;
	remaining-=pure;pc+=pure*2;
	m_pc=m_ppc=pc;m_sh4_icount=remaining;sh3_total_cycles+=pure;
	if(remaining>0){pure=0;goto reload_fetch;}
	goto done;
data_head:
	// Commit preceding pure operations, not CMP/PZ itself. The
	// full kernel either executes a complete verified path or
	// leaves the guest state alone. On rejection execute the
	// original CMP/PZ and continue via the standard dispatcher.
	m_pc=pc+pure*2;m_ppc=m_pc;
	m_sh4_icount=remaining-pure;sh3_total_cycles+=pure;
	++cv1kDataFullStats.headProbes;
	if(!Cv1kTryDataLoop(1)){
		m_pc+=2;m_ppc=m_pc;CMPPZ(opcode);EAT(1);
	}
	goto after_slow;
interp_bf:
	if (m_test_irq || m_delay) goto slow_finished;
	EAT(1);
	if(cv1kDataLoopsEnabled && opcode==0x8bfa)Cv1kTryDataLoop(2);
	else if(cv1kInterpLoopsEnabled && m_sh4_icount>=10 && opcode>=0x8bfa && opcode<=0x8bfc)
		Cv1kTryInterpLoop(opcode);
	goto after_slow;
slow_finished:
	if (m_test_irq && !m_delay) sh4_check_pending_irq();
	EAT(1);
	// The ordinary slow path advanced pc and fetch by the same
	// amount. Retain the live pointer only for a sequential exit
	// within its page. Special fused-loop exits below always reload.
	if (ReuseCapable && ((!Dense && !DirectMem) || cv1kSlowFetchReuseEnabled) && m_sh4_icount>0 &&
		!m_delay && !m_test_irq && m_pc==pc && (pc&SH3_PAGEM) &&
		fetchEpoch==sh3FetchMapEpoch) {
		remaining=m_sh4_icount;pure=0;
		window=(SH3_PAGE_SIZE-(pc&SH3_PAGEM))/2;
		if(remaining<window)window=remaining;
#if defined(SH3_INTERPRETER_TEST)
		++cv1kSlowFetchReuseHits;
#endif
		goto fast_next;
	}
after_slow:
	if (m_sh4_icount<=0) goto done;
	if (m_delay || m_test_irq || (Hybrid && m_pc!=pc)) goto start;
	pc=m_pc;remaining=m_sh4_icount;pure=0;
	goto reload_fetch;
done:
	cycles-=m_sh4_icount;
	sh4_run_timers(cycles);
	m_sh4_icount=0;
	return cycles;
}
#endif

static int Sh3Run_timerhack(int cycles)
{
#if SH3_NATIVE
	if(cv1kSampleEnabled)Cv1kSamplePC();
	if(cv1kNativeEnabled && !cv1kHybridEnabled)return Sh3Run_timerhack_switch<true>(cycles);
#endif
#if SH3_FLAT_DISPATCH
	if(cv1kFetchPageEnabled) {
		if(cv1kDispatch8Enabled)return Sh3Run_timerhack_batch<false,true,true>(cycles);
		if(cv1kFlowEnabled){
#if SH3_NATIVE_V2
			if(cv1kV2Enabled)
				return Sh3Run_timerhack_batch<false,true,false,true,true,true,false,false,true>(cycles);
#endif
			if(cv1kFastRamBase || cv1kFastRomBase)
				return Sh3Run_timerhack_batch<false,true,false,true,true,true,true>(cycles);
#if SH3_CV1K_PCREL
			if(cv1kPCRelEnabled)
				return Sh3Run_timerhack_batch<false,true,false,true,true,true,false,true>(cycles);
#endif
			return Sh3Run_timerhack_batch<false,true,false,true,true,true>(cycles);
		}
		if(cv1kDirectMemEnabled){
			if(cv1kFastRamBase || cv1kFastRomBase)
				return Sh3Run_timerhack_batch<false,true,false,true,true,false,true>(cycles);
			return Sh3Run_timerhack_batch<false,true,false,true,true>(cycles);
		}
		if(cv1kSlowFetchReuseEnabled)return Sh3Run_timerhack_batch<false,true,false,true>(cycles);
		return Sh3Run_timerhack_batch<false,true>(cycles);
	}
	if(cv1kBatchEnabled)return cv1kHybridEnabled?Sh3Run_timerhack_batch<true>(cycles):Sh3Run_timerhack_batch<false>(cycles);
	if(cv1kFlatEnabled)return cv1kCompactEnabled?Sh3Run_timerhack_flat<true>(cycles):Sh3Run_timerhack_flat<false>(cycles);
#endif
	return Sh3Run_timerhack_switch<false>(cycles);
}

static int Sh3Run_normal(int cycles)
{
	m_sh4_icount = cycles;
	sh3_end_run = 0;

	if (m_cpu_off)
	{
		m_sh4_icount = 0;
		sh3_total_cycles += cycles;
		return cycles;
	}

	do
	{
		INT32 tot_cyc_start = sh3_total_cycles;
		if (m_delay)
		{
			const UINT16 opcode = sh3_cpu_readop16((UINT32)(m_delay & AM));

			m_delay = 0;
			m_ppc = m_pc;

			execute_one(opcode);
		}
		else
		{
			const UINT16 opcode = sh3_cpu_readop16((UINT32)(m_pc & AM));

			m_pc += 2;
			m_ppc = m_pc;

			execute_one(opcode);
		}
		if (m_test_irq && !m_delay)
		{
			sh4_check_pending_irq(/*"mame_sh4_execute"*/);
		}

		EAT(1);
		sh4_run_timers((sh3_total_cycles - tot_cyc_start));
	} while( m_sh4_icount > 0);// && !sh3_end_run );

	cycles = cycles - m_sh4_icount;

	m_sh4_icount = 0;

	return cycles;
}

int Sh3Run(int cycles)
{
	return (timer_granularity == 0) ? Sh3Run_normal(cycles) : Sh3Run_timerhack(cycles);
}

#if 0
void device_start()
{
	for (int i=0; i<3; i++)
	{
		m_timer[i] = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(sh4_timer_callback), this));
		m_timer[i]->adjust(attotime::never, i);
	}

	for (int i=0; i<4; i++)
	{
		m_dma_timer[i] = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(sh4_dmac_callback), this));
		m_dma_timer[i]->adjust(attotime::never, i);
	}

	m_refresh_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(sh4_refresh_timer_callback), this));
	m_refresh_timer->adjust(attotime::never);
	m_refresh_timer_base = 0;

	m_rtc_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(sh4_rtc_timer_callback), this));
	m_rtc_timer->adjust(attotime::never);

	sh4_parse_configuration();

	m_internal = &space(AS_PROGRAM);
	m_program = &space(AS_PROGRAM);
	m_io = &space(AS_IO);
	m_direct = &m_program->direct();
	sh4_default_exception_priorities();
	m_irln = 15;
	m_test_irq = 0;

	save_item(NAME(m_pc));
	save_item(NAME(m_r[15]));
	save_item(NAME(m_sr));
	save_item(NAME(m_pr));
	save_item(NAME(m_gbr));
	save_item(NAME(m_vbr));
	save_item(NAME(m_mach));
	save_item(NAME(m_macl));
	save_item(NAME(m_spc));
	save_item(NAME(m_ssr));
	save_item(NAME(m_sgr));
	save_item(NAME(m_fpscr));
	save_item(NAME(m_r[ 0]));
	save_item(NAME(m_r[ 1]));
	save_item(NAME(m_r[ 2]));
	save_item(NAME(m_r[ 3]));
	save_item(NAME(m_r[ 4]));
	save_item(NAME(m_r[ 5]));
	save_item(NAME(m_r[ 6]));
	save_item(NAME(m_r[ 7]));
	save_item(NAME(m_r[ 8]));
	save_item(NAME(m_r[ 9]));
	save_item(NAME(m_r[10]));
	save_item(NAME(m_r[11]));
	save_item(NAME(m_r[12]));
	save_item(NAME(m_r[13]));
	save_item(NAME(m_r[14]));
	save_item(NAME(m_fr[ 0]));
	save_item(NAME(m_fr[ 1]));
	save_item(NAME(m_fr[ 2]));
	save_item(NAME(m_fr[ 3]));
	save_item(NAME(m_fr[ 4]));
	save_item(NAME(m_fr[ 5]));
	save_item(NAME(m_fr[ 6]));
	save_item(NAME(m_fr[ 7]));
	save_item(NAME(m_fr[ 8]));
	save_item(NAME(m_fr[ 9]));
	save_item(NAME(m_fr[10]));
	save_item(NAME(m_fr[11]));
	save_item(NAME(m_fr[12]));
	save_item(NAME(m_fr[13]));
	save_item(NAME(m_fr[14]));
	save_item(NAME(m_fr[15]));
	save_item(NAME(m_xf[ 0]));
	save_item(NAME(m_xf[ 1]));
	save_item(NAME(m_xf[ 2]));
	save_item(NAME(m_xf[ 3]));
	save_item(NAME(m_xf[ 4]));
	save_item(NAME(m_xf[ 5]));
	save_item(NAME(m_xf[ 6]));
	save_item(NAME(m_xf[ 7]));
	save_item(NAME(m_xf[ 8]));
	save_item(NAME(m_xf[ 9]));
	save_item(NAME(m_xf[10]));
	save_item(NAME(m_xf[11]));
	save_item(NAME(m_xf[12]));
	save_item(NAME(m_xf[13]));
	save_item(NAME(m_xf[14]));
	save_item(NAME(m_xf[15]));
	save_item(NAME(m_ea));
	save_item(NAME(m_fpul));
	save_item(NAME(m_dbr));
	save_item(NAME(m_exception_priority));
	save_item(NAME(m_exception_requesting));

	save_item(NAME(m_SH4_TSTR));
	save_item(NAME(m_SH4_TCNT0));
	save_item(NAME(m_SH4_TCNT1));
	save_item(NAME(m_SH4_TCNT2));
	save_item(NAME(m_SH4_TCR0));
	save_item(NAME(m_SH4_TCR1));
	save_item(NAME(m_SH4_TCR2));
	save_item(NAME(m_SH4_TCOR0));
	save_item(NAME(m_SH4_TCOR1));
	save_item(NAME(m_SH4_TCOR2));
	save_item(NAME(m_SH4_TOCR));
	save_item(NAME(m_SH4_TCPR2));

	save_item(NAME(m_SH4_IPRA));

	save_item(NAME(m_SH4_IPRC));

	// Debugger state

	state_add(SH4_PC,             "PC", m_pc).formatstr("%08X").callimport();
	state_add(SH4_SR,             "SR", m_sr).formatstr("%08X").callimport();
	state_add(SH4_PR,             "PR", m_pr).formatstr("%08X");
	state_add(SH4_GBR,            "GBR", m_gbr).formatstr("%08X");
	state_add(SH4_VBR,            "VBR", m_vbr).formatstr("%08X");
	state_add(SH4_DBR,            "DBR", m_dbr).formatstr("%08X");
	state_add(SH4_MACH,           "MACH", m_mach).formatstr("%08X");
	state_add(SH4_MACL,           "MACL", m_macl).formatstr("%08X");
	state_add(SH4_R0,             "R0", m_r[ 0]).formatstr("%08X");
	state_add(SH4_R1,             "R1", m_r[ 1]).formatstr("%08X");
	state_add(SH4_R2,             "R2", m_r[ 2]).formatstr("%08X");
	state_add(SH4_R3,             "R3", m_r[ 3]).formatstr("%08X");
	state_add(SH4_R4,             "R4", m_r[ 4]).formatstr("%08X");
	state_add(SH4_R5,             "R5", m_r[ 5]).formatstr("%08X");
	state_add(SH4_R6,             "R6", m_r[ 6]).formatstr("%08X");
	state_add(SH4_R7,             "R7", m_r[ 7]).formatstr("%08X");
	state_add(SH4_R8,             "R8", m_r[ 8]).formatstr("%08X");
	state_add(SH4_R9,             "R9", m_r[ 9]).formatstr("%08X");
	state_add(SH4_R10,            "R10", m_r[10]).formatstr("%08X");
	state_add(SH4_R11,            "R11", m_r[11]).formatstr("%08X");
	state_add(SH4_R12,            "R12", m_r[12]).formatstr("%08X");
	state_add(SH4_R13,            "R13", m_r[13]).formatstr("%08X");
	state_add(SH4_R14,            "R14", m_r[14]).formatstr("%08X");
	state_add(SH4_R15,            "R15", m_r[15]).formatstr("%08X");
	state_add(SH4_EA,             "EA", m_ea).formatstr("%08X");
	state_add(SH4_R0_BK0,         "R0 BK 0", m_rbnk[0][0]).formatstr("%08X");
	state_add(SH4_R1_BK0,         "R1 BK 0", m_rbnk[0][1]).formatstr("%08X");
	state_add(SH4_R2_BK0,         "R2 BK 0", m_rbnk[0][2]).formatstr("%08X");
	state_add(SH4_R3_BK0,         "R3 BK 0", m_rbnk[0][3]).formatstr("%08X");
	state_add(SH4_R4_BK0,         "R4 BK 0", m_rbnk[0][4]).formatstr("%08X");
	state_add(SH4_R5_BK0,         "R5 BK 0", m_rbnk[0][5]).formatstr("%08X");
	state_add(SH4_R6_BK0,         "R6 BK 0", m_rbnk[0][6]).formatstr("%08X");
	state_add(SH4_R7_BK0,         "R7 BK 0", m_rbnk[0][7]).formatstr("%08X");
	state_add(SH4_R0_BK1,         "R0 BK 1", m_rbnk[1][0]).formatstr("%08X");
	state_add(SH4_R1_BK1,         "R1 BK 1", m_rbnk[1][1]).formatstr("%08X");
	state_add(SH4_R2_BK1,         "R2 BK 1", m_rbnk[1][2]).formatstr("%08X");
	state_add(SH4_R3_BK1,         "R3 BK 1", m_rbnk[1][3]).formatstr("%08X");
	state_add(SH4_R4_BK1,         "R4 BK 1", m_rbnk[1][4]).formatstr("%08X");
	state_add(SH4_R5_BK1,         "R5 BK 1", m_rbnk[1][5]).formatstr("%08X");
	state_add(SH4_R6_BK1,         "R6 BK 1", m_rbnk[1][6]).formatstr("%08X");
	state_add(SH4_R7_BK1,         "R7 BK 1", m_rbnk[1][7]).formatstr("%08X");
	state_add(SH4_SPC,            "SPC", m_spc).formatstr("%08X");
	state_add(SH4_SSR,            "SSR", m_ssr).formatstr("%08X");
	state_add(SH4_SGR,            "SGR", m_sgr).formatstr("%08X");
	state_add(SH4_FPSCR,          "FPSCR", m_fpscr).formatstr("%08X");
	state_add(SH4_FPUL,           "FPUL", m_fpul).formatstr("%08X");

	state_add(SH4_FR0,            "FR0", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR1,            "FR1", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR2,            "FR2", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR3,            "FR3", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR4,            "FR4", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR5,            "FR5", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR6,            "FR6", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR7,            "FR7", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR8,            "FR8", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR9,            "FR9", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR10,           "FR10", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR11,           "FR11", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR12,           "FR12", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR13,           "FR13", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR14,           "FR14", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_FR15,           "FR15", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF0,            "XF0", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF1,            "XF1", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF2,            "XF2", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF3,            "XF3", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF4,            "XF4", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF5,            "XF5", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF6,            "XF6", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF7,            "XF7", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF8,            "XF8", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF9,            "XF9", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF10,           "XF10", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF11,           "XF11", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF12,           "XF12", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF13,           "XF13", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF14,           "XF14", m_debugger_temp).callimport().formatstr("%25s");
	state_add(SH4_XF15,           "XF15", m_debugger_temp).callimport().formatstr("%25s");

	state_add(STATE_GENPC, "GENPC", m_debugger_temp).callimport().callexport().noshow();
	state_add(STATE_GENSP, "GENSP", m_r[15]).noshow();
	state_add(STATE_GENPCBASE, "GENPCBASE", m_ppc).noshow();
	state_add(STATE_GENFLAGS, "GENFLAGS", m_sr).formatstr("%20s").noshow();

	m_icountptr = &m_sh4_icount;
}

void state_import(const device_state_entry &entry)
{
#ifdef LSB_FIRST
	UINT8 fpu_xor = m_fpu_pr;
#else
	UINT8 fpu_xor = 0;
#endif

	switch (entry.index())
	{
		case STATE_GENPC:
			m_pc = m_debugger_temp;
		case SH4_PC:
			m_delay = 0;
			break;

		case SH4_SR:
			sh4_exception_recompute();
			sh4_check_pending_irq("sh4_set_info");
			break;

		case SH4_FR0:
			m_fr[0 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR1:
			m_fr[1 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR2:
			m_fr[2 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR3:
			m_fr[3 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR4:
			m_fr[4 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR5:
			m_fr[5 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR6:
			m_fr[6 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR7:
			m_fr[7 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR8:
			m_fr[8 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR9:
			m_fr[9 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR10:
			m_fr[10 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR11:
			m_fr[11 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR12:
			m_fr[12 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR13:
			m_fr[13 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR14:
			m_fr[14 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_FR15:
			m_fr[15 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF0:
			m_xf[0 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF1:
			m_xf[1 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF2:
			m_xf[2 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF3:
			m_xf[3 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF4:
			m_xf[4 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF5:
			m_xf[5 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF6:
			m_xf[6 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF7:
			m_xf[7 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF8:
			m_xf[8 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF9:
			m_xf[9 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF10:
			m_xf[10 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF11:
			m_xf[11 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF12:
			m_xf[12 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF13:
			m_xf[13 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF14:
			m_xf[14 ^ fpu_xor] = m_debugger_temp;
			break;

		case SH4_XF15:
			m_xf[15 ^ fpu_xor] = m_debugger_temp;
			break;
	}
}

void state_export(const device_state_entry &entry)
{
	switch (entry.index())
	{
		case STATE_GENPC:
			m_debugger_temp = (m_delay) ? (m_delay & AM) : (m_pc & AM);
			break;
	}
}

void state_string_export(const device_state_entry &entry, astring &string)
{
#ifdef LSB_FIRST
	UINT8 fpu_xor = m_fpu_pr;
#else
	UINT8 fpu_xor = 0;
#endif

	switch (entry.index())
	{
		case STATE_GENFLAGS:
			string.printf("%s%s%s%s%c%c%d%c%c",
					m_sr & MD ? "MD ":"   ",
					m_sr & sRB ? "RB ":"   ",
					m_sr & BL ? "BL ":"   ",
					m_sr & FD ? "FD ":"   ",
					m_sr & M ? 'M':'.',
					m_sr & Q ? 'Q':'.',
					(m_sr & I) >> 4,
					m_sr & S ? 'S':'.',
					m_sr & T ? 'T':'.');
			break;

		case SH4_FR0:
			string.printf("%08X %f", m_fr[0 ^ fpu_xor], (double)FP_RFS(0 ^ fpu_xor));
			break;

		case SH4_FR1:
			string.printf("%08X %f", m_fr[1 ^ fpu_xor], (double)FP_RFS(1 ^ fpu_xor));
			break;

		case SH4_FR2:
			string.printf("%08X %f", m_fr[2 ^ fpu_xor], (double)FP_RFS(2 ^ fpu_xor));
			break;

		case SH4_FR3:
			string.printf("%08X %f", m_fr[3 ^ fpu_xor], (double)FP_RFS(3 ^ fpu_xor));
			break;

		case SH4_FR4:
			string.printf("%08X %f", m_fr[4 ^ fpu_xor], (double)FP_RFS(4 ^ fpu_xor));
			break;

		case SH4_FR5:
			string.printf("%08X %f", m_fr[5 ^ fpu_xor], (double)FP_RFS(5 ^ fpu_xor));
			break;

		case SH4_FR6:
			string.printf("%08X %f", m_fr[6 ^ fpu_xor], (double)FP_RFS(6 ^ fpu_xor));
			break;

		case SH4_FR7:
			string.printf("%08X %f", m_fr[7 ^ fpu_xor], (double)FP_RFS(7 ^ fpu_xor));
			break;

		case SH4_FR8:
			string.printf("%08X %f", m_fr[8 ^ fpu_xor], (double)FP_RFS(8 ^ fpu_xor));
			break;

		case SH4_FR9:
			string.printf("%08X %f", m_fr[9 ^ fpu_xor], (double)FP_RFS(9 ^ fpu_xor));
			break;

		case SH4_FR10:
			string.printf("%08X %f", m_fr[10 ^ fpu_xor], (double)FP_RFS(10 ^ fpu_xor));
			break;

		case SH4_FR11:
			string.printf("%08X %f", m_fr[11 ^ fpu_xor], (double)FP_RFS(11 ^ fpu_xor));
			break;

		case SH4_FR12:
			string.printf("%08X %f", m_fr[12 ^ fpu_xor], (double)FP_RFS(12 ^ fpu_xor));
			break;

		case SH4_FR13:
			string.printf("%08X %f", m_fr[13 ^ fpu_xor], (double)FP_RFS(13 ^ fpu_xor));
			break;

		case SH4_FR14:
			string.printf("%08X %f", m_fr[14 ^ fpu_xor], (double)FP_RFS(14 ^ fpu_xor));
			break;

		case SH4_FR15:
			string.printf("%08X %f", m_fr[15 ^ fpu_xor], (double)FP_RFS(15 ^ fpu_xor));
			break;

		case SH4_XF0:
			string.printf("%08X %f", m_xf[0 ^ fpu_xor], (double)FP_XFS(0 ^ fpu_xor));
			break;

		case SH4_XF1:
			string.printf("%08X %f", m_xf[1 ^ fpu_xor], (double)FP_XFS(1 ^ fpu_xor));
			break;

		case SH4_XF2:
			string.printf("%08X %f", m_xf[2 ^ fpu_xor], (double)FP_XFS(2 ^ fpu_xor));
			break;

		case SH4_XF3:
			string.printf("%08X %f", m_xf[3 ^ fpu_xor], (double)FP_XFS(3 ^ fpu_xor));
			break;

		case SH4_XF4:
			string.printf("%08X %f", m_xf[4 ^ fpu_xor], (double)FP_XFS(4 ^ fpu_xor));
			break;

		case SH4_XF5:
			string.printf("%08X %f", m_xf[5 ^ fpu_xor], (double)FP_XFS(5 ^ fpu_xor));
			break;

		case SH4_XF6:
			string.printf("%08X %f", m_xf[6 ^ fpu_xor], (double)FP_XFS(6 ^ fpu_xor));
			break;

		case SH4_XF7:
			string.printf("%08X %f", m_xf[7 ^ fpu_xor], (double)FP_XFS(7 ^ fpu_xor));
			break;

		case SH4_XF8:
			string.printf("%08X %f", m_xf[8 ^ fpu_xor], (double)FP_XFS(8 ^ fpu_xor));
			break;

		case SH4_XF9:
			string.printf("%08X %f", m_xf[9 ^ fpu_xor], (double)FP_XFS(9 ^ fpu_xor));
			break;

		case SH4_XF10:
			string.printf("%08X %f", m_xf[10 ^ fpu_xor], (double)FP_XFS(10 ^ fpu_xor));
			break;

		case SH4_XF11:
			string.printf("%08X %f", m_xf[11 ^ fpu_xor], (double)FP_XFS(11 ^ fpu_xor));
			break;

		case SH4_XF12:
			string.printf("%08X %f", m_xf[12 ^ fpu_xor], (double)FP_XFS(12 ^ fpu_xor));
			break;

		case SH4_XF13:
			string.printf("%08X %f", m_xf[13 ^ fpu_xor], (double)FP_XFS(13 ^ fpu_xor));
			break;

		case SH4_XF14:
			string.printf("%08X %f", m_xf[14 ^ fpu_xor], (double)FP_XFS(14 ^ fpu_xor));
			break;

		case SH4_XF15:
			string.printf("%08X %f", m_xf[15 ^ fpu_xor], (double)FP_XFS(15 ^ fpu_xor));
			break;

	}
}


void sh4_set_ftcsr_callback(sh4_ftcsr_callback callback)
{
	m_ftcsr_read_callback = callback;
}
// unnecessary -dink
#endif
