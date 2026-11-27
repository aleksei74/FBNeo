# CV1000 SH3 direct memory and delay slot experiment (stage 2)

The batched fetch-page interpreter can now defer PC/PPC and cycle
publication across 32 ordinary one-cycle MOV handler forms, including
register, predecrement, postincrement, indexed, GBR, displacement and
PC-relative memory accesses. Reads use the live read map (the separate
long map for 32-bit accesses); writes use the live write map.
The aligned direct path preserves sign extension, word-swapped endian
layout, aliasing of source/destination registers and the original EA
behavior. No guest memory or decoded code bytes are cached.

Unaligned, P4/internal/cache, MMIO and long-only handler accesses always
fall back before changing guest state. Direct batching requires disabled
wait states. The existing idle-page burn and all callback publication
remain unchanged. Delayed branch slots may use the shared decoder's
handler-ID table instead of the nested switch. Slot execution still uses
the original handler, target PC, PPC, IRQ checks and cycle accounting.

## Options

Native Linux little-endian ARM64 libretro defaults:

- FBNEO_CV1K_DIRECT_MEM=1: direct mapped MOV batching, requires fetch-page
  interpreter and the original 128 KiB relative dispatch table.
- FBNEO_CV1K_SLOT_DISPATCH=1: decoded delay-slot dispatch, requires
  fetch-page interpreter.
- Explicit 0 disables either option; other platforms retain opt-in.
- Existing FETCH_REUSE, POLL, TIMER_FAST, DATA_LOOPS, DATA_FULL, VRAM16
  and BLIT_TILES defaults are unchanged. JIT, DISPATCH8 and INTERP_LOOPS
  remain opt-in. Speed Hacks, Thread Blitter, Thread Sync, guest clocks,
  timer scheduling and state serialization are unchanged.

## Verified

- x86 and AArch64 Cortex-A55 QEMU: 896 actual interpreter comparisons
  across all 32 MOV forms, aliases, MMIO, unaligned and P4 addresses,
  page boundaries, long-only handlers and delayed slots.
- x86 and AArch64: 23,136 pure opcode encodings in actual delayed slots.
  Reset the timer/bank state between the two test runs.
- x86: 448 scenarios x 9 interpreter modes, 23,136 pure encodings x 7
  modes, 65,536 decoder keys and host arithmetic differential tests.
- AArch64 mode 8 (both new options): 1,440 polling slice comparisons,
  24,000 timer comparisons, 1,008 data-loop comparisons, 2,268 full
  literal comparisons and live-code/cache rejection tests.
- Cortex-A55 production O3 sh4.cpp object compiled. The new relative
  dispatcher fits the existing signed 16-bit offset range.

Synthetic 15M-instruction/cycle workload tests (QEMU, test counters
present; not RK3566 speed measurements), baseline vs both new options:
memory 1749.525 -> 622.915 ms; delayed branch 1203.563 -> 928.981 ms.
The separate short/terminal literal benchmark is workload-dependent
and did not consistently improve. Host load was not controlled.
Real game FPS, full libretro link and hardware correctness still
require testing.
