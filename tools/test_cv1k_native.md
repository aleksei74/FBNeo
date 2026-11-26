# CV1000 low-spec ARM64 performance experiment

Based on FBNeo master dd17fb0b2. This branch keeps existing game CPU clocks,
blitter timing, busy-loop hacks, timer granularity and default DIP options.
It is an independent implementation of guarded SH-3 integer spans, not a
port of MAME's UML DRC, Flycast's SH-4 backend or GPL-3.0 Spelunkomp code.

## Changes
- Linux little-endian AArch64: hot 4?24 instruction spans, seven host GPRs,
  integer arithmetic, comparisons/T flag and guarded byte/word/long loads.
  Each span checks its original guest opcodes against live mapped code;
  reads check the current mapping, endianness, and alignment. Guard
  failure commits completed instructions, then the original interpreter
  handles the remainder. Stores, branches, delay slots, devices, pending
  IRQs and the per-instruction timer mode remain interpreted.
- Packed 8 MiB maximum W^X code cache (virtual reservation, not 8 MiB
  committed upfront), 8192-entry metadata, cold code interpreted.
  Uses the packed allocator independently adapted from our Y2 experiment.
- Restore DSNo 7ca2efd92's separate long-read mapping. Existing speedhack
  remains on long reads, byte/word ordinary RAM access bypasses it.
- Collapse prescaler increments only if no timer callback can fire;
  keep the original callback loop whenever an event is possible.
- EPIC12 upload shadow-copy chunks split at RAM wrap, NEON exact RGB555
  plus transparency-bit expansion, non-overlapping simple sprite copy
  and transparent copy acceleration, overlap retains per-pixel ordering.
  32-bit screen copy uses wrap-aware row copies. No extra worker threads:
  DSNo's old helper pool served palette initialization and scanout only.
- No changes to YMZ770 decoding, native store/branch, FPU, full SH-3
  timing model or GPU renderer. Actual performance depends on workload.

## Runtime (initial experiment; superseded by low-spec follow-up below)
Default native spans enabled on Linux AArch64 for CV1000's existing
timerhack mode only (Speed Hacks DIP). Disable for comparison:
FBNEO_CV1K_JIT=0. Restart content after changing the environment.
FBNEO_CV1K_PROFILE=0 disables default ARM64 host-time profiling.
FBNEO_EPIC12_NO_NEON is a compile-time scalar fallback.
CV1K PERF reports 120-frame averages, peak/late, CPU, audio, explicit
blitter wait and scanout time. CPU timing includes any nested blitter
synchronization. CV1K JIT shows guest ops, calls and spans built.

## Verification
Build tools/test_cv1k_native.cpp for AArch64 using the normal sh4.cpp
include/defines from the libretro Makefile, and -ffunction-sections
-fdata-sections -Wl,--gc-sections. Run under QEMU or on target.
Build tools/test_epic12_fast.cpp with -O2 on ARM64 or x86-64.
These tests exercise the real SH-3 interpreter, including code rewrite,
timer budget, direct memory and handler fallbacks, and compare all
supported encodings, mixed native spans, prescalers, upload colours,
transparency and overlapping copies. Not a real game benchmark or
a full frame/audio comparison. Real H700/RK3566/RK3568 tests required.

## Design references
- MAME SH family DRC (BSD-3-Clause): https://github.com/mamedev/mame/tree/master/src/devices/cpu/sh
- Flycast SH-4 dynarec: https://github.com/flyinghead/flycast
- Spelunkomp hardware and timing documentation: https://github.com/tmcleroy/spelunkomp/blob/main/docs/SLOWDOWN.md
- DSNo previous source: https://github.com/aleksei74/FBNeo/commit/7ca2efd92376fc517e9c59dcb18401c4eb2923cb


## Low-spec follow-up (RK3566)

The RK3566 dsmbl trace of ddf44d019 showed 41.4 million native calls
for 223.7 million guest instructions (~5.4 instructions/call), plus
51,559 code publications. It also showed CPU/device time dominating
overall frame time and substantial blitter wait during later scenes.
There is no matched old-core baseline, so this does not establish a
measured speedup or regression percentage.

Default Linux AArch64 CPU path is now the shared-decoder flat
interpreter. It resolves the 16-bit opcode into one handler ID via a
64 KiB byte table, then performs one computed-goto dispatch. Original
instruction handlers, interrupt checks, delayed branch behavior,
per-instruction cycle accounting and timerhack boundaries are kept.
The non-timerhack path remains unchanged. Native spans are opt-in:
- FBNEO_CV1K_JIT=1: experimental native spans, no flat dispatch.
- FBNEO_CV1K_JIT=0 (default): no native checks inside the CPU loop.
- FBNEO_CV1K_DISPATCH=0: restore original switch interpreter.
- FBNEO_CV1K_NEON_BLEND=0: restore scalar tint/blend loops.
- FBNEO_CV1K_PROFILE=0: disable host profiling.

The optional JIT uses 8192 sets of four entries, improved hashing,
eight hits before emission, minimum eight guest ops, cached rejection
of short spans, and retires spans after eight consecutive short guard
exits. A saturated cache keeps existing native code; no repeated
global invalidation/compilation cycle. Live opcode comparison and
memory-handler fallbacks are retained. Branch/store native emission
is deliberately not expanded: the real trace indicated short-span
overhead, not missing instruction coverage alone.

The timer prescaler divides only when at least four ticks accrued.
The existing DT/BF busy-loop acceleration is O(1) with identical
cycles and flags; speedhack long reads query PC only at the idle
address. CPU clock, DIPs, timing and the original emulated blitter
delay are unchanged.

EPIC12 adds exact four-pixel NEON tint and blend arithmetic with
the original RGB555 truncation/clamping, transparency, flipped
source traversal, and the legacy red-base behavior for destination
square mode. Overlapping rows and scalar tails use original code.
No additional renderer threads or speculative reordering of blits.
Test tools/test_epic12_blend.cpp includes the real device and compares
all 512 specialized blend variants with the original scalar loops.

CV1K CACHE reports lookup misses, rejections/retirements and full
cache events (zero when JIT disabled). CV1K BLIT reports submit wait,
shadow-copy time, synchronous execution, worker execution and submitted
job counts. Submit wait, shadow and sync exec are already inside CPU
time; worker wall time can overlap CPU and draw, and is reported at
job completion. These are diagnostics, not additive frame costs.

New regression tests compare the switch/flat/JIT execution loops,
mixed branches and stores, code edits, memory handlers, busy-loop
cycle equivalence, saturated-cache preservation and all EPIC12 blend
variants. Actual RK3566 gameplay and measured performance remain
unverified; QEMU is used only for functional validation.

## RK3566 follow-up: EPIC12 sparse and safe dual-worker rendering

The c863eadd9 akatana RK3566 trace had 49 120-frame intervals,
16.796 ms mean core time and 42.321 ms peak. The last 24 intervals
averaged 16.818 ms of worker execution with up to 26.065 ms in one
interval. It used flat SH3 dispatch, not native spans. The earlier
ddf44d019 log was a different game (dsmbl), not a matched baseline.

- Exact NEON blending now handles four-pixel rows and skips all-transparent
  groups before reading or writing the destination; all-opaque groups
  need no transparency selection. Plain flipped rows use ordered NEON
  copies when nonoverlapping; overlapping rows retain scalar order.
- On little-endian AArch64 pthread builds, at least four online CPUs,
  existing asynchronous blitter + before-draw synchronization and accurate
  timing, a persistent second worker can render an independent Y band.
  The command list is preflighted with conservative 32x32 VRAM tile
  read/write footprints. Reject uploads, source/destination dependencies,
  wrapping or invalid geometry, unknown commands and small or unbalanced
  lists. Draws in each band retain original command order, clipping,
  flips and blend modes. Per-band pixel-delay totals are combined only
  after joining the helper. No emulated clock or slowdown changes.
- FBNEO_CV1K_BLIT_PARALLEL=0 disables the helper for an A/B comparison.
  CV1K PARALLEL logs split/fallback counts per profiling interval.
  A fallback is intentional, not a render failure.
- Differential tests: all 65536 uploads, 20000 forward/reverse overlap
  cases each (scalar and ARM64), 4096 real blend cases (all 512 variants,
  including fully transparent and opaque inputs), and 24 actual
  dual-worker command lists versus serial execution, identical VRAM and
  pixel delays, plus rejection tests for dependency, wrapping and uploads.
  ARM64 tests run under QEMU; Cortex-A55 and x86-64 device/driver objects
  compile. Target game frame rates, split coverage and thermal behavior
  must be measured on real RK3566 hardware.

## RK3566 follow-up: frame latency, compact dispatch and exact blend LUTs

The 1260f50 akatana RK3566 log contains 137 profiling intervals.
Mean core time was 12.726 ms, CPU 11.826 ms, explicit blitter wait
0.491 ms, worker 7.765 ms, with 2121 late frames of 16440 profiled.
11040 of 15714 asynchronous lists were split. Playback still exhibited
audible interruptions, with the frontend reporting 49.59 ms maximum
audio wait and a minimum queue of 5 frames. Different gameplay and
duration preclude a controlled A/B performance claim.

- Compact SH3 dispatch uses a 128 KiB opcode-to-signed-label-offset
  table to remove one dependent table lookup. Label displacement range
  is verified, with the existing flat dispatcher as fallback. Disable
  just the compact path with FBNEO_CV1K_COMPACT=0. Existing switch
  fallback and opt-in native spans are unchanged.
- The explicitly registered CV1000 long-only idle RAM page is now read
  directly, with the original aligned value, word swap, PC/delay-slot
  checks and cycle consumption. Other handlers, remapped pages and
  handler replacements still use the original generic access. Disable
  with FBNEO_CV1K_IDLE_DIRECT=0.
- For exact NEON RGB555 blending, a 32-entry TBL lookup replaces
  multiplication for constant-alpha and square/inverse-square terms.
  Variable factors and tint retain the original exact arithmetic.
- Parallel planning uses the actually sampled clipped source rectangle
  with original flip origins; still rejects all source/write tile
  dependencies, uploads and wrapped/invalid geometry. An 8-row,
  cost-weighted partition chooses the closest balanced split. All
  timing and serial fallback semantics are retained.
- CV1K SPLIT identifies why lists fall back: small, dependency,
  geometry, upload, balance or other. CV1K LATENCY reports CPU/wait
  peaks, frame counts over 25/33.33 ms and Linux scheduled CPU time
  (thread CPU clock), distinguishing host execution from time off CPU
  or blocked. These diagnostics are not emulated time and do not prove
  which host scheduler or memory subsystem caused a stall.

Regression: ARM64 Cortex-A55-targeted QEMU test passes 19811 native
instruction encodings, 4096 mixed spans, 100000 prescaler cases,
switch/flat/compact/JIT loop and code-rewrite/handler comparisons,
64 branch/store programs per flat variant, 20000 direct idle-page
value/cycle comparisons, 20000 DT/BF cycle cases and cache-pressure
tests. The compact-path test confirms it executes rather than falling
back. EPIC12 passes all 1024 alpha/channel pairs and both square LUTs,
65536 uploads, 20000 forward/reverse copy cases, 4096 real blend
variants, and 36 dual-worker lists including clipped/flipped source
footprints and serial fallback checks. Modified driver/device/CPU
objects compile with GCC -O3 on Cortex-A55 and x86-64. Full core
link and real device FPS, audio continuity and thermal behavior
remain unverified.
