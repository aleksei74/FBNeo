# RK3566 Muchi Muchi Pork follow-up

Base 653084b7b; actual mmpork device log: 53 intervals of 120 guest
frames, 18.278 ms mean frame time, 53.514 ms peak, 2387/6360 late.
First ten intervals average CPU 30.020 ms with very little explicit
blitter wait; later 43 average CPU 11.566 ms, worker 13.693 ms,
wait 3.444 ms. 875 of 981 parallel fallback jobs cite dependencies.
These are workload-dependent results, not a controlled benchmark.

## Default EPIC12 changes
Retain coarse 32x32 map as the fast path. For candidate draws with
coarse overlap, split each clipped source and destination rectangle
according to its exact destination row ownership (including flip Y).
Only cross-lane read/write intersections prohibit execution: same-lane
dependencies and horizontal self-overlap are processed in original
order by that lane. Compare exact rectangles on coarse tile hits,
with at most 2048 rectangles per set and 131072 pair comparisons per
candidate. Try up to three balanced row boundaries, all bounded.
If the original segmented plan has no useful parallel ranges, retry
upload-separated runs with the precise proof. Wrapped geometry and
uploads remain serial barriers; no VRAM writes occur during planning.
FBNEO_CV1K_BLIT_PRECISE=0 restores the previous conservative policy.
CV1K LANES reports precise-proof ranges actually executed (a subset
of parallel whole lists and segmented ranges, not an additive job
count).

NEON blend/copy paths now process safe forward overlapping groups:
destination <= source, or destination >= source + four pixels.
The latter preserves successive group store/load ordering (not
memmove). Reversed overlap and 1..3-pixel forward dependencies
remain scalar. Nontransparent forward copies with destination below
source can use memmove. Preserve all existing 32-bit VRAM/state
formats and emulated timing.

## Default serial-flash DMA
For incrementing direct-mapped byte DMA, transfer complete guest
words in the current 64 KiB page via one serial flash call; retain
word-swapped host layout, exact last-byte saturation, address/count
updates and device state. Odd edges, fixed/decrementing modes and
handlers remain scalar. No changes to timer scheduling or DMA
completion cycles. This targets loading traffic; the log does not
establish that NAND DMA is the main startup CPU cost.

## Astra-assisted experimental ARM64 JIT
Independent design based on existing FBNeo emitter; native-port
architecture references include Spelunkomp (static SH3 translation,
ordered render runs) and ReBOX (low-spec DRC). No GPL code imported.
Max candidate length 24 -> 64; add guarded byte/word/long stores,
predecrement and displacement stores. Direct maps are checked live;
misalignment or MMIO exits before modifying guest registers/memory.
A host-address interval guard prevents overwriting live translated
opcodes, including aliases; every entry still validates live guest
code. No incomplete page-epoch invalidation scheme. Hybrid dispatch
continues at a full capped span boundary. Existing register allocation,
timers, IRQ and delay slots retained. 26067 tested candidate encodings
(up from 20435). Default interpreter unchanged; FBNEO_CV1K_JIT=2
enables experimental hybrid, FBNEO_CV1K_JIT=0 disables it.
FBNEO_CV1K_HOTPCS=1 enables bounded approximate 1/16 timer-slice-entry
PC/opcode samples (32768 accepted samples max, reports every 4096).
Not an instruction census. Do not use timing results with profiling
enabled as a controlled unprofiled benchmark.

Astra gpt-6-astra developed the CPU patch in a detached worktree;
its session expired before committing. Integration reviewed the
patch, fixed IRQ fixture span length to fit its cycle budget, extended
the hot-PC sampler window, and validated the final combined source.

## Regression results
GCC -O3 Cortex-A55 QEMU:
- 26067 native encodings, 4096 mixed spans, 1050 store guard cases,
  7 long store programs (RAM, SMC, external rewrite, cheat, remap,
  MMIO, timer), switch/flat/compact/batch/JIT/hybrid, IRQ including
  native prefix and delay-slot, existing prescaler and cache tests,
  bounded hot PC sampler.
- 8192 scalar/NEON real blend cases (512 variants with explicit
  overlap offsets -9,-4,-1,0,1,3,4,7), all 65536 uploads, 20000
  each forward/reversed copy cases, 36 whole-list and 41 segmented
  tests, 8 bounded fallback cases, 48 precise cross-lane lists,
  8 actual cross-lane hazards and scratch cap test.
- 20000 serial flash state/byte comparisons and 240 actual SH3 DMA
  comparisons (cross-page, odd start/count, handler pages, all
  increment/decrement/fixed address modes), also passed on x86.
- Modified sh4, epic12, serflash and d_cv1k objects compile on ARM64
  Cortex-A55 and x86-64. Full core link and real RK3566 FPS are not
  established by these functional tests. 16-bit VRAM and Vulkan
  blitter remain separate future design investigations.
