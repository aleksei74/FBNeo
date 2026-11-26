# CV1000 interpreter follow-up (RK3566, JIT=0)
Base 28cb916c0. Real mmpork JIT=0, VRAM16=1, BLIT_TILES=1:
mean CPU 12.613 ms, core 14.120 ms, 10734/10784 async jobs
parallel; 10141 tile jobs. The JIT=3 run was slower and had more
long frames, so further JIT development is deferred.

## SH3 interpreter (Astra-assisted)
Reuse live fetch-page pointer and a bounded pure-op window. Publish
PC/PPC and consumed cycles before every slow handler; reload mappings
after slow handlers, page crossings, IRQ/delay and run boundaries.
No opcode cache; no changes to guest clocks or timing. Enabled by
default on the existing Linux ARM64 batched interpreter with JIT=0.
FBNEO_CV1K_FETCH_PAGE=0 restores its previous fetch implementation.
Direct mapped memory access functions use compiler always-inline
on ARM64; SH3_INLINE_MEMORY=0 is a compile-time fallback.
The previous JIT modes are retained without new JIT translation work.

Astra worked in a detached worktree. Its task did not finish a commit;
the integration agent stopped the session, reviewed and applied the
diff, corrected test fixture initialization (unmapped callback and
timer reset), verified the new dispatch was actually executed, and
reran tests.

## EPIC12
When packed VRAM is selected, specialize plain/tinted and common
alpha blend modes (0/0, 0/3, 3/0, 3/3). Generic mode remains available
with FBNEO_CV1K_PACKED_FAST=0.
If tile scheduling is enabled and succeeded on the previous job,
preflight the next job with tile scheduling first. Never reuse the
old plan or skip current safety checks. Fallback to row/segment
planning when unsuitable. FBNEO_CV1K_TILE_FIRST=0 restores old order.
CV1K PLAN reports per-frame mean total planning time, tile planning
time (subset), and successful predicted tile-first jobs per 120 frames.
Planning time is part of worker time, not an additional frame cost.

## Verification
Cortex-A55 QEMU and x86 interpreter differential:
448 scenarios x 3 paths and 23136 pure opcode encodings x 3 paths.
Actual memory mapping/SMC/IRQ/timer/delay/callback state comparisons.
The fetch-page path has an explicit active flag checked by the test.
Cortex-A55 QEMU: existing native/loop regressions (26067 encodings,
4096 mixed spans, 1050 memory guards, 281 loop branches, 200 slice
comparisons), 8192 cases each packed generic/specialized scalar/NEON
and original expanded comparison, original row/segmented/precise
tests, 32 expanded and 8 packed tile lists, 8 tile-first comparisons,
RGB555 expanded-state byte-stream comparisons.
SH3, EPIC12 and CV1000 driver objects compile ARM64 Cortex-A55 and x86.
Full libretro linking, actual RK3566 speed/stutter and save/load remain
unverified. Packed VRAM and tile mode stay opt-in; no DIP, speedhack,
clock or audio setting changes.
