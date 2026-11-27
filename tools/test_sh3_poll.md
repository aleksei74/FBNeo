# CV1000 fixed-point polling and no-event timer advance

Base: 8b5d4292f. Both new changes are opt-in. The previously slower
counted-loop experiment and JIT stay disabled by default.
No upstream merge, audio or raster algorithm changes.

## Runtime options

- FBNEO_CV1K_POLL=1: enable short read-only polling acceleration in
  the existing JIT=0 batched fetch-page timer-sliced interpreter.
- FBNEO_CV1K_TIMER_FAST=1: enable constant-time prescaled timer
  updates when no expiration callback can fire, in timer-sliced mode.
- FBNEO_CV1K_DIAG=1: optional, includes worst-slice post-execution
  code snapshots. Instrumentation overhead affects timings.

The polling algorithm is an independent implementation inspired by
event-boundary and fixed-point techniques discussed in Spelunkomp and
ReBOX. No GPL implementation imported. At most one probe per original
CPU slice; a short backward branch on the same live fetch page can
form a rotated loop. Simulate only audited register operations, stable
direct ROM/RAM loads and the existing idle/EPIC12 busy read equivalents.
Accept only a complete taken round whose registers and SR are unchanged.
Preserve exact existing instruction/extra read cycles, PC/PPC/EA and
T. Consume only complete rounds fitting the original slice and interpret
the tail. No early or late timer updates, no new speedhack burns, no
writes or arbitrary device callbacks. Negative structural-cache entries
only suppress future optimization, never guest execution; all accepted
rounds re-read live code and data. Delayed branches and rotated entry
states are supported. Existing timer slices are not made more accurate.
Timer fast advance uses the original prescale arithmetic and falls back
before mutating any state if an expiry can invoke a callback; callbacks
remain in existing order, including restart and prescaler changes.

CV1K POLL reports actual attempts, accepted slice probes, complete
rounds, equivalent guest cycles and instructions, RAM/idle/device hits,
and last accepted PC, per 120 frames. These are counts, not performance
estimates. CV1K DIAG CODE is a post-slice snapshot, not an instruction
trace. Real mmpork RAM loop coverage and FPS remain unverified.

## Verified
- GCC O3 ARM64 Cortex-A55 under QEMU and x86: 1440 actual interpreter
  slice comparisons including rotations, delay slots, live code
  rewrites and timer/DMA/blitter state changes; 1472 accepted probes
  and 245988 complete rounds (1225 RAM, 54 idle, 193 device probes).
- Eight side-effect-free rejection cases: writes, postincrement,
  alignment, unknown MMIO, pending IRQ, delay, exit and changing state.
- 24000 prescale comparisons, including 1608 callback expiry/restart
  cases, on both architectures.
- Existing ARM64 interpreter and native/loop regression suite passes
  (448 scenarios x 3, 23136 pure encodings, 26067 native encodings,
  timers, IRQ, SMC, alias, handler, 281 branches and 200 linked slices).
  No JIT feature changes.
- ARM64 and x86 sh4 and d_cv1k objects compiled.

Full libretro link and actual device testing remain pending.

Final EPIC12 check: ARM64 actual driver test binary compiled and x86
EPIC12 object compiled. Actual busy peek vs read/extra-cycle oracle:
12 comparisons pass. Packed and expanded blend (8192 each), 36 row,
41 segmented, and 48 precise lane tests pass. The remaining long
graphics regression and separate ARM64 EPIC12 object build were not
completed within the host command limit; no claim of a full core
link or full graphics regression. Raster algorithms are unchanged.
