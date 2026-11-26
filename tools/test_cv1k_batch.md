# RK3566 follow-up: batched SH3 dispatch and hybrid ARM64 spans

Base: 99cddb89b. The real akatana log still had 1931 frames above
16.67 ms out of 11760 profiled, 50.101 ms worst frame, and 13 periods
of ALSA silence. The two runs were not controlled gameplay benchmarks.

The default Linux AArch64 CV1000 timerhack path now keeps the PC and
remaining cycle budget in host registers while executing audited
single-cycle register/flag operations. Each opcode is fetched live;
before a memory/branch/device/other handler or slice exit, publish PC,
PPC, consumed cycles and remaining budget exactly as the existing core.
Delay slots and pending interrupts use the original single-step
handler path. No guest clock, timer granularity, DIP or save format
change. FBNEO_CV1K_BATCH=0 restores compact dispatch.

FBNEO_CV1K_JIT=2 is a new **opt-in** hybrid: use the batched interpreter
and attempt native spans at control-flow/run boundaries only, not on
every instruction. Keep the existing full opcode comparison (guest
RAM has aliases, direct NAND DMA and save/cheat writes; page-epoch
validation would need all these invalidation paths audited). Short
native spans, guard exits, and cache pressure retain existing behavior.
JIT=1 retains the older experimental per-instruction check. JIT=0 is
still the default. Add native MOVT, XTRCT, SWAP.B, CMP/PZ, CMP/PL and
single-bit shifts, using exact interpreter semantics and T bit.

Cortex-A55 QEMU regression with GCC -O3:
- 20435 native instruction encodings, 4096 mixed spans.
- 23136 pure dispatch encodings, four random register states each.
- Switch/flat/compact/batch/JIT/hybrid state/cycle comparison,
  including observed MMIO callback PC and cycle values.
- 64 mixed branch/store programs for each dispatcher, 64 pure
  programs crossing fetch pages with live opcode rewrite.
- Real IRQ request from a memory callback, including a delay slot.
- 100000 timer prescaler, 20000 direct idle-page, 20000 DT/BF cases,
  cache saturation, long-only and handler-replacement checks.
- Cortex-A55 and x86-64 sh4.cpp objects compiled with GCC -O3.

QEMU is functional validation, not a host performance benchmark.
Full core build, sustained real-device speed and audio continuity
remain unverified.

## Astra-assisted EPIC12 ordered segments

The 99cddb89b akatana trace had 4119 rejected split jobs, including
3091 with upload commands and 237 with read/write dependencies.
The whole-list fast path is retained. For lists rejected for upload,
dependency or geometry, preflight a bounded sequence of disjoint
draw ranges, split at upload, RAW/WAR hazards, self-overlap or
unsafe geometry. Adjacent serial ranges coalesce. Each parallel
range joins both lanes before any dependent draw or upload.
The original upload handler executes exactly once and guest pixel
delay totals are preserved. Preflight completes before changing
VRAM, with bounded command count, RAM traversal and range count.
FBNEO_CV1K_BLIT_SEGMENTS=0 restores whole-list-only parallelization.

CV1K PARALLEL split counts a list once if whole-list or segmented
parallel rendering succeeded. CV1K SEGMENT reports the subset of
split jobs recovered by segmented execution and number of actual
parallel ranges. CV1K SPLIT reasons describe final fallback jobs,
not all lists which contain uploads. Segments still require the
existing safe asynchronous and accurate-timing conditions.

Codex gpt-6-astra developed the initial segmentation patch in an
isolated worktree; the integration agent reviewed, corrected the
segment-cap fixture after serial-range coalescing, added consecutive
upload and actual callback/counter A/B tests, and ran final checks.
Independent implementation using existing FBNeo code, not imported
GPL-3.0-only native-port source.

GCC -O3 Cortex-A55 QEMU regression: 4096 blend cases, 36 original
dual-worker lists, 41 segmented differential lists including wrap,
uploads, clips, flips, RAW/WAR hazards, self-overlap and consecutive
uploads; 8 bounded side-effect-free rejection cases and actual job
callback/counter A/B test. Modified EPIC12, SH3 and CV1000 driver
objects compile for Cortex-A55 and x86-64. Real performance, full
libretro link and target audio continuity remain unverified.
