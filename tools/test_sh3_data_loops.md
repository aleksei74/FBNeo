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
