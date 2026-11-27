# CV1000 SH3 dispatch structure experiment (stage 1)

The active relative handler table can be reduced from 128 KiB to 8 KiB.
The original decoder checks all 65,536 encodings for handler identity
collisions before enabling the reduced table. Operand bits remain intact.
The legacy table and execution path remain available; this is not a
claim that total static memory is reduced by 120 KiB.

New paths keep the fetched opcode in a register across dispatch.
Sequential slow-handler exits may reuse the next live fetch pointer only
if the page boundary is not crossed, PC is unchanged, no delay/IRQ is
pending, and the fetch mapping epoch is unchanged. The three map APIs
increment that epoch on fetch-map changes. No decoded instruction or
guest code bytes are cached. Special fused loop exits and delay/IRQ
boundaries still reload the original live mapping.

Options (CV1000 batch fetch-page path, Speed Hacks On, JIT=0):

| DISPATCH8 | FETCH_REUSE | Active path |
|---|---|---|
| 0 | 0 | Original 128 KiB table |
| 0 | 1 | 128 KiB table, single opcode load, guarded sequential fetch reuse |
| 1 | 0 | 8 KiB table, single opcode load |
| 1 | 1 | 8 KiB table and guarded sequential fetch reuse |

Native Linux ARM64 libretro defaults to DISPATCH8=0 and FETCH_REUSE=1.
POLL, TIMER_FAST, DATA_LOOPS, DATA_FULL, VRAM16 and BLIT_TILES also
default to 1 on that platform, subject to their existing prerequisites.
JIT and INTERP_LOOPS remain opt-in. Other builds retain the previous
opt-in policy. DISPATCH8=1 also enables fetch reuse unless FETCH_REUSE=0
is specified. Environment variables have the FBNEO_CV1K_ prefix; explicit
0 or 1 overrides the preset. Keep other options and game settings identical
for A/B. The baseline fetch-page mode can be selected with FETCH_REUSE=0.
The Speed Hacks, Thread Blitter and Thread Sync DIP settings are unchanged.
Do not infer real RK3566 performance from QEMU or synthetic loops.

## Verification

tools/test_sh3_interpreter.cpp tests six modes (legacy switch, batch,
original fetch page, dense only, dense+reuse, full-table+reuse).
The native x86 and AArch64 Cortex-A55 QEMU tests pass 448 scenarios x 6,
23,136 pure opcode encodings x 6, 65,536 decoder keys, direct-store
self-modifying code, actual reuse hits, signed arithmetic oracle and
100,000 host arithmetic checks. The production AArch64 O3 object
compiles, and nm confirms an 8192-byte dense relative table. The
compiler can already eliminate duplicate opcode loads on some builds
of the original path; the barrier is not a guaranteed speedup.

The poll/data harnesses accept FBNEO_CV1K_TEST_FETCH_MODE=4 or 5,
which selects dense+reuse or full-table+reuse. This test-only option
does not change a runtime policy. Timing, DMA, IRQ, guest clocks,
savestate serialization, and the existing speed-hack DIP are unchanged.

Initial synthetic AArch64/QEMU mixed loop measurements (30M guest
instructions, test counters present, not hardware speed claims):
original 1904.610 ms, dense only 2262.703 ms, dense+reuse 2097.304 ms,
full-table+reuse 1861.593 ms. The pure loop is slower with the new
paths. The full-table reuse variant is the preferred first hardware
A/B candidate; dense indexing must earn its place in real gameplay.
