# CV1000 host stall diagnostics (v10)

Base 9e1ff5215. Real RK3566 mmpork A/B using identical core:
counted loops enabled vs disabled:
first four 120-frame CPU means 51.840 vs 31.973 ms;
after first 1200 frames core means 13.880 vs 13.156 ms.
Sessions and subsequent scenes differ; these are not controlled FPS
benchmarks. Startup regression is sufficient to keep the counted-loop
experiment opt-in (FBNEO_CV1K_INTERP_LOOPS=1); JIT remains disabled
by default. No new arithmetic, renderer, timing or audio optimizations.

## Collect the actual cause of a slow frame

Set FBNEO_CV1K_DIAG=1 in addition to usual JIT=0, VRAM16=1,
BLIT_TILES=1. The option enables existing frame profiling too.
Play until the reproducible stall and at least 120 more frames, then
exit normally. Collect RetroArch verbose log. Do not treat a diagnostic
run as an uninstrumented performance benchmark: 240 slice measurements
per frame and device-level clocks have overhead.

- DIAG YMZ: mean inclusive register-write and stream-update wall
  times within CPU phase; mean AMM decode time inside CPU and after CPU;
  counts of writes, decodes, CPU decodes, empty/failed blocks, key-ons,
  source sample frames and bits consumed searching for sync, including
  accepted header prefix. max_scan is the largest search distance.
- DIAG DECODE_PEAK: individual slowest decode, frame number,
  YMZ phrase and channel, sync bits consumed, same frame CPU/core times.
  A phrase ID does not identify speech/music without ROM/game mapping.
  Decoder's sync search can scan a long region bit by bit before a
  valid header or end of ROM. That is a hypothesis, not a measured cause
  of real-game stalls.
- DIAG DMA: automatic synchronous SH3 DMA transfer wall time,
  calls and requested bytes, NAND subset and existing synchronous
  blit, submit-wait and shadow costs within CPU phase.
  DIAG DMA_PEAK identifies the longest such transfer's frame,
  width, source, destination and bytes. Does not cover all DMA APIs
  or standalone flash register accesses.
- DIAG SLOW core/cpu: worst frame by core time and (if different)
  CPU time per 120-frame interval, including same-frame device times
  and slowest of 240 CPU_RUN slice wall times and entry/exit guest PCs.
  A slow slice PC is a lead, not exact attribution to one opcode.
  Decoder time is nested in stream time and register-write time;
  DMA can invoke device handlers. Inclusive times must not be added
  to CPU time or subtracted as mutually exclusive. Existing async
  blitter worker time overlaps CPU.

Counters are host-only, not part of emulated or saved state. Disabled
diagnostics do not perform extra clock reads. No audio skipped or
guest cycles changed.

## Validation

GCC -O3 ARM64 Cortex-A55 and x86 objects compiled for d_cv1k, sh4,
ymz770, mpeg_audio. ARM64 execution via qemu-aarch64 -cpu cortex-a55
-L /usr/aarch64-linux-gnu.
- Synthetic AMM (no copyrighted ROM) valid mono packet after 16
  padding bytes, missing sync across 65536 bits, short input.
  Actual buffered YMZ register writes and end-frame render with
  diagnostics off/on: equal PCM/channel state; CPU vs end decode
  counts 2/3; key-ons and empty decode counted. Both architectures.
- Existing 20000 flash block comparisons and 240 actual SH3 DMA
  cases alternating diagnostics on/off, byte/call counts verified.
  Both architectures.
- ARM64 interpreter 448 scenarios x 3 paths, 23136 pure encodings
  x 3 paths, 392 overflow and 100000 arithmetic checks; explicit
  opt-in counted loops 3840 comparisons and 10 guards still pass.

No full core link or real RK3566 diagnostic log yet. No confirmed
audio bottleneck, no claimed speedup from this instrumentation.
