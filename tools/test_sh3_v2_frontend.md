# CV1000 Dynarec V2 stage 3.0A-fix

Base: 52ab9f29e (initial V2). Real RK3566 mmpork, first 7320 frames:
2.7 CPU 10.452 ms -> V2 15.382 ms; startup 480f 21.395 -> 38.939 ms;
high-load 360f 18.967 -> 27.154 ms. The old V2 performed 406,204,589
candidate probes, 8,738,202 native calls, 5,988,644 rejections and
373,316,385 cooldown visits. Full 8 MiB cache at frame 12,720 window;
another 294,478,186 probes after that. Poll/data/full counters matched
the baseline. Host clocks/temperature and same-binary on/off were not
controlled. The observed loss/probe is not an isolated probe latency.

## Frontend redesign

- 2048 sets x 4 ways x 8 bytes = 64 KiB compact directory containing
  only compiled or pinned/disabled blocks. The large code snapshots
  are separate, touched only on a compiled hit or publication. Hash
  collisions never evict compiled entries. No per-miss fetch-map walk,
  opcode decode, metadata zeroing or call into the compiler.
- Promotion samples: once per 16 slice entries and every 64 eligible
  backward misses (shared sampling counter, approximate rate). Ordinary
  forward and reload misses are read-only directory lookups. Three
  sampled visits are required for compilation; the 1024-entry learning
  table is separate. Rejected candidates have no countdown write and
  are not periodically rescanned. A learning-table collision, fetch
  epoch change or changed first opcode can permit relearning. Without
  a complete write barrier, interior changes to a rejected site can
  postpone promotion, but cannot execute stale machine code.
- Cv1kV2Observe and Cv1kV2Compile are noinline/cold. The emission
  scratch (5152 words) belongs to the compiler, not the hot lookup.
- Full set rejects promotion without eviction. Cache full/denied stops
  all new promotions/sampling, but preserves existing usable blocks.
  Actual code changes retire an entry; old emitted storage remains
  immutable until CPU disable/exit. Repeated short guard exits pin a
  disabled entry to avoid recompilation. Cache capacity remains 8 MiB.
- Live fetch pointer and full code-byte validation remain mandatory.
  No direct links, page write-barrier, new emitter, changed clock,
  timer/IRQ ordering, guest memory semantics or savestate fields.

## A/B and diagnostics

The dedicated V2 branch still defaults to on on native Linux ARM64
libretro. The separate low-spec branch is unchanged. Set
FBNEO_CV1K_JIT_V2=0 for same-binary stage 2.7 control. Set
FBNEO_CV1K_V2_LOOKUP_ONLY=1 with V2 enabled to learn/compile/validate
but suppress actual native execution; this includes compilation cost
and is not an exact measurement of lookup alone. The lookup-only
trajectory can differ because all guest ops take the interpreter path.
Use ordinary ES launch and the same game, clocks, display and power
conditions. Report the first 7320 frames and 6360/6480/6600 windows.

The existing V2 total log's probes now means sampled observations plus
eligible compiled-hit checks, NOT all inline lookups. Do not compare
its hit ratio directly with the old probe denominator. New frontend
fields: samples, ready (compiled-hit checks before live validation),
setfull, retired (actual code/remap invalidation), disabled (repeated
guard exits), skipped (lookup-only validated blocks). The ordinary
inline miss path deliberately has no statistics write. Native calls
and ops retain their original meaning. Negative counts now represent
sampled rejected-site visits, not per-boundary cooldown decrements.

## Regression fixtures

test_sh3_v2 covers actual native execution before code/remap/MMIO/SMC
and timer/IRQ mutations, budget/page/delay boundaries, cache lifecycle,
four colliding compiled blocks plus rejected fifth, full-cache miss
with unchanged counters, live invalidation after full, sampled negative
site with no repeated scan and unchanged learning record, remap
relearning and lookup-only state preservation. The benchmark also
includes a two-op short loop with no compilable span, to expose
frontend overhead which long-block synthetic benchmarks miss.

## Results

ARM64 O3 Cortex-A55 production SH3 object and standalone PIC shared
link passed. Objdump: old Cv1kV2Try frame 20,896 bytes (96 + 20,800);
new compiled-hit Cv1kV2Run frame 160 bytes. The ~20 KiB emission
frame is now isolated to Cv1kV2Compile. Ordinary inline misses do not
call either function. The saved field names/order are unchanged.
Final QEMU V2 168 differential slices and cache/lookup/collision/
negative/full tests passed. Initial timer fixture fired before sparse
promotion (not a state mismatch); timer was moved to cycle 11000 and
the warmup/IRQ path was revalidated. x86 interpreter 448 x 10 modes;
ARM64 interpreter 448 x 11 modes, pure encodings, 1440 polling, 24000
timer, 1008 data loops and 2268 full-literal comparisons passed.
Full Libretro core link and real RK3566 performance remain unverified.

Synthetic 5M-cycle benchmark, Cortex-A55 QEMU, medians of 3
alternating control/Fix runs. Host load not controlled:
- Pure arithmetic: 612.123 -> 169.111 ms.
- RAM/arithmetic: 817.907 -> 318.932 ms.
- Short uncompiled loop: 872.308 -> 1105.447 ms.
These are synthetic timings, not device FPS predictions.
