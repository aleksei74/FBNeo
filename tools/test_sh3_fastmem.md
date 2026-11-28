# Stage 2.6 A/B: validated direct memory and opcode diagnostics

The CV1000 driver registers its actual Main-RAM and 4 MiB ROM mappings
after installation of the idle handler. Every page must agree with
the live read/write/long-read maps. Type B/C mirrors 8 MiB through the
16 MiB physical window; Type D has 16 MiB. The registered region is
revoked on any mapping API, long-handler replacement, idle registration,
map reset or CPU exit. Other addresses continue to use the live maps.

The first 64 KiB long-only idle handler is accepted only when the
explicit idle registration, RAM read page and handler identity agree.
Aligned accesses to all other bytes can stay batched. The exact idle
long address retains the original slow path and PC-dependent cycle burn.
No change to the read/write maps themselves. Preserve unaligned and P4
fallback, endian layout, aliases, live code modifications and device
callbacks. DT lookahead continues using the *data* read mapping, not
the instruction fetch mapping. No clock, timer schedule, DIP, state
serialization, data-loop default or display changes.

Stage 2.7 changes the default following the RK3566 stage 2.6 regression.
FBNEO_CV1K_FAST_RAM=0 is now the default; full region shortcuts remain
opt-in with 1. FBNEO_CV1K_IDLE_BATCH=1 is the independent native Linux
ARM64 libretro default, rescuing only aligned non-idle long reads in
the registered idle page after live mapping/handler checks. Explicit
IDLE_BATCH=0 disables that rescue. The normal map fallback remains.
The startup log prints both policies. See test_sh3_hotstate.md.

## Diagnostic build (2.6B)

Build libretro with make CV1K_OPSTAT=1 (clean rebuild/separate build
directory; default 0). No opcode/path counters are compiled in the
normal performance build. The diagnostic build prints a report every
120 frames, including direct RAM, ROM, ordinary mapped access,
idle-page rescue, idle rejection, handler, alignment and P4 rejection,
DT busy/handler cases, branch fallback reasons and per-handler dispatch
visits/fused slots/indirect fused slots.

Visits count the batched interpreter's dispatched opcode, including
separately executed delay slots; fused slots are a separate column.
They are not a complete guest instruction census: existing polling,
data-loop, counted-loop and native acceleration can elide dispatches;
a CMP/PZ visit may be consumed by the full literal kernel.
Access counts include attempted direct slot accesses and DT lookahead;
branch fallback counts reflect guard decisions, not hardware misses.
Use the ordinary CV1K POLL/DATA statistics with these counts. Do not
compare diagnostic build timing against normal build timing.

For game A/B, use the same normal build/launcher/display/clock settings:
FBNEO_CV1K_FAST_RAM=1 vs 0 (hold IDLE_BATCH fixed); independently compare
FBNEO_CV1K_DATA_LOOPS=1 vs 0 (DATA_FULL automatically follows its
prerequisite). No data-loop policy change until real-device evidence.
Recommended log window: mmpork frames 6000-6840, plus startup.

## Verification

Final x86 and ARM64 Cortex-A55 QEMU:
- All 256 RAM pages per memory type, edge alignment, byte/word/long
  mapping, mirrors, ROM, idle neighbors, exact idle and live remaps.
- 260 differential actual slices per architecture, MOV/stack/fused
  slots including exact idle burn, mapped callbacks, and unaligned reads.
- ARM64 diagnostic counters, window reset, driver object compile.
- ARM64 448 x 10-mode interpreter scenarios; 23136 pure encodings x 8;
  65536 decoder keys; 392 signed oracle and 100000 arithmetic cases.
- 1430 branch, 210 stack, 360 DT/BRA, 23824 fused-slot encodings.
- 1440 polling slice, 24000 timer, 1008 data-loop and 2268 full literal
  comparisons. These existing harnesses use generic fallback maps.
- Production ARM64 O3 SH3/driver objects compile; active dispatcher
  0x4cb4 bytes, within the signed relative offset range.
- Initial fixture incorrectly retained a write mapping for ROM;
  clear the write map before installing ROM to mirror the driver.
  Final tests passed. Full core link and real hardware not tested.

Synthetic 10M guest-cycle runs, median of 3 alternating tests, QEMU
Cortex-A55, test-only counters active, uncontrolled host load:
first idle page (non-idle aligned loads) 861.869 -> 474.632 ms;
ordinary RAM 634.694 -> 671.758 ms;
ROM reads 601.225 -> 490.874 ms.
x86 corresponding medians 127.521 -> 98.539, 125.760 -> 108.983,
108.085 -> 112.663 ms. Results vary by workload and host load.
Not actual device FPS; normal RAM path regression is possible.
