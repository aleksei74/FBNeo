# CV1000 specialized SH3 data-copy loops (v12)

Base c2647989b. The mmpork two-player attract-mode diagnostic log
showed repeated high-CPU slices around 0x0c06af90. The live opcode
snapshot contains a signed-byte flag test, literal load/store, pointer
increment, two counters and a delayed backward branch. This is evidence
of a data-processing loop, not proof of its exact game function or
percentage of total CPU time.

The new interpreter path independently recognizes this 12-opcode
shape, and common 5-opcode postincrement B/W/L copy loops with BF or
BF/S. It validates register dependencies and branch targets without
hardcoding a game, PC or register allocation. Fuses only whole taken
rounds within the existing cycle budget and mapped-page boundaries.
Literal rounds stop before a flag or counter exit; the interpreter
executes the terminal iteration normally. No skipped guest cycles,
timer schedule changes, new burn hacks, JIT, audio or graphics changes.

The shape cache has 1024 tagged entries. Positive entries always
compare the live code words; negative entries suppress only future
optimization checks for up to 63 probes. Validate live fetch/read/write
maps, both DT data-map branch words, independent registers, alignment,
direct RAM/ROM ranges, and host-address overlap with live instructions
and DT words. Forward copies with source/destination overlap preserve
the original sequential semantics, including physical aliases and
guest byte order. No saved-state data is added. Only the specific
slice-entry opcode or exact backward branch/slot shape can probe the
cache. No checks on every guest instruction.

## Runtime
- FBNEO_CV1K_DATA_LOOPS=1: opt-in specialized data loops in JIT=0
  fetch-page interpreter; incompatible with old INTERP_LOOPS=1.
- Keep FBNEO_CV1K_POLL=1, FBNEO_CV1K_TIMER_FAST=1, JIT=0,
  VRAM16=1 and BLIT_TILES=1 for the current device baseline.
- CV1K DATA logs candidate probes, live positive cache matches,
  successful batches, literal rounds, counted copy rounds, bytes,
  equivalent guest ops and last PC per 120 frames. These count work,
  not measured speedup.
- Disable DATA_LOOPS (or set 0) for controlled comparison. Keep the
  other settings, game and attract-mode duration identical. Diagnostics
  add overhead; first measure with DIAG and HOTPCS off.

## Validation
GCC -O3 ARM64 Cortex-A55 QEMU and x86:
- 1008 actual interpreter slice comparisons with varying register
  allocation, budgets, flags, counters, overlap/alias, odd bytes,
  page crossings, alternate DT read map and timer/IRQ callbacks.
  Each: 126 literal rounds, 11495 copy rounds, 1034 accepted batches,
  2217 positive-cache matches.
- 12 rejection cases and live opcode mutation invalidates cached
  positive pattern; register/memory/cycle state unchanged on rejection.
- Synthetic 100000-slice benchmark (not actual game or hardware):
  ARM64 QEMU: literal off 1329.02 ms / on 147.86 ms;
  counted copy off 9720.46 ms / on 143.39 ms.
  x86 host: literal 112.47 / 16.21 ms; copy 937.96 / 16.25 ms.
  This intentionally repeats eligible full kernels and does not measure
  general dispatch overhead, actual usage, or game FPS.
- sh4.cpp and d_cv1k.cpp O3 ARM64 and x86 object compilation pass.

No whole-core link or real RK3566 benchmark performed.
- Existing ARM64 interpreter regression passes: 448 remap/SMC/page/
  wrap/MMIO/IRQ/delay/timer scenarios x 3 modes, 23136 pure encodings
  x 3 modes, 392 signed overflow and 100000 arithmetic comparisons.

## v13: full literal kernel (opt-in)

With FBNEO_CV1K_DATA_LOOPS=1 and FBNEO_CV1K_DATA_FULL=1 the validated
literal kernel also handles its original flag, output-count and
bit-count exits. The existing taken-lap mode remains available with
DATA_FULL=0. Count and bit-counter wraparound, noncanonical positive
flags, arbitrary handlers, code modification, or insufficient budget
retain normal interpretation. A 9-cycle output-count exit includes
the original MOV delay slot; the 12-cycle bit-count exit preserves the
ordinary fallthrough EXTS.B. A negative flag takes the 4-cycle BF
path without data accesses. Complete taken rounds still cost 13
guest cycles; an eligible subsequent negative exit can be fused as
well. PC/PPC/EA, T, count, bits and scratch registers are committed
to the exact branch result, with original cycle-budget boundaries.

A single learned hot-head PC allows entry before the first CMP/PZ
rather than after the first completed lap. This is a runtime hint,
never a hardcoded game address. Every entry validates live instruction
words and applicable data maps; stale hints only affect performance.
The delayed-branch and slice probes no longer duplicate checks for
that learned literal head. Other matching sites can still be learned
from the ordinary probes. Cache comparisons have fixed lengths.
Short (1..8 byte) copies use a small direct sequential path instead of
general memmove, preserving forward overlap and guest byte layout.
For a shared fetch/data page, the full live-code guard also covers the
DT reads. Separate data mappings are checked independently.

CV1K DATA FULL reports learned-head probes and the three types of
exit. In full mode CV1K DATA hits includes flag-only successful
batches (zero bytes), while literal/copy/bytes/ops retain their
individual meanings. Old and new hit counts are therefore not
directly comparable. No timing or raster/audio changes.

Run the same two-player attract sequence with DATA_FULL=1 and 0;
keep DATA_LOOPS, POLL, TIMER_FAST, VRAM16 and BLIT_TILES enabled,
JIT=0, and DIAG/HOTPCS off for the A/B performance measurement.

Validation: the 1008 data-loop fixtures compare all three modes
(original interpreter, taken-lap kernel, full kernel). Nine unsafe
full-mode patterns and live cache mutation are checked (three old
rejections become intentional terminal/flag paths). Another 2268
exact dispatcher comparisons cover flag/count/bit exits, overlapping
copies, short budgets and learned-head entry after pure operations.
Both architectures pass, with 1049 learned-head probes and 746
flag, 420 output-count, 1371 bit-count exits across the exact-path
fixtures. The production SH3 and CV1000 driver objects compile for
ARM64 Cortex-A55 and x86 at -O3.

Mixed short/terminal synthetic benchmark, 100000 slices (only a
microbenchmark; no RK3566 device timing):
- ARM64 under QEMU: original 987.30 ms, old kernel 961.80 ms,
  full kernel 818.26 ms.
- x86: original 93.91 ms, old kernel 92.52 ms, full kernel 66.04 ms.
- Existing ARM64 interpreter regression passes: 448 remap/SMC/page/
  wrap/MMIO/IRQ/delay/timer scenarios x 3 modes, 23136 pure encodings
  x 3 modes, 392 signed overflow and 100000 arithmetic comparisons.
- No full core link or hardware FPS measurement in this iteration.
