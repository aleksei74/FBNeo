/* emulation of Altera Cyclone EPIC12 FPGA programmed as a blitter */

void epic12_init(INT32 ram_size, UINT16 *ram, UINT8 *dippy);
void epic12_exit();
void epic12_reset();
void epic12_scan(INT32 nAction, INT32 *pnMin);
void epic12_set_blitterdelay_method(INT32 delay_method); // 0 = accurate, !0 = ancient
void epic12_set_blitterdelay(INT32 delay, INT32 burn_cycles);
void epic12_set_blitterthreading(INT32 value);

void epic12_set_blitter_clipping_margin(INT32 c_margin_on); // for testing
void epic12_set_blitter_sleep_on_busy(INT32 busysleep_on); // for testing

void epic12_wait_blitterthread();
void epic12_draw_screen(UINT8 &recalc_palette);
UINT32 epic12_blitter_read(UINT32 offset); // 0x18000000 - 0x18000057
void epic12_blitter_write(UINT32 offset, UINT32 data);


// Monotonic host-time counters, not emulated or serialized device state.
enum Epic12SplitReject { EPIC12_SPLIT_SMALL, EPIC12_SPLIT_DEPENDENCY, EPIC12_SPLIT_GEOMETRY, EPIC12_SPLIT_UPLOAD, EPIC12_SPLIT_BALANCE, EPIC12_SPLIT_OTHER, EPIC12_SPLIT_REASONS };
struct Epic12PerfStats {
	UINT64 submitWaitNs, shadowNs, syncExecNs, workerExecNs, jobs, asyncJobs, parallelJobs, parallelFallback;
	UINT64 splitReject[EPIC12_SPLIT_REASONS];
};
void epic12_set_perf_enabled(bool enabled);
void epic12_get_perf_stats(Epic12PerfStats *stats);
