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

## Runtime
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
