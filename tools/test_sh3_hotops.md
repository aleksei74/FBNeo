# SH3 small-handler experiment

Base: 81b3a6cea. Scope: interpreter instruction bodies only; no dispatch-loop,
DT/BF, opcode classification, JIT translation, graphics or driver edits.

Build with `-DSH3_SMALL_HANDLERS=1` to enable; omitted or `=0` retains the
original implementation. This is a compile-time A/B switch with no default
runtime branch or profiling overhead. The existing runtime DISPATCH, COMPACT,
BATCH and FETCH_PAGE switches remain available.

* Long loads keep mapped RAM access inline and move the existing idle-page /
  MMIO tail into one non-inlined function. Address masking and WaitState stay
  in RL before the map lookup. The helper receives the current map entry;
  it retains all idle-page identity checks, callback state and cycle accounting.
  It caches neither mappings nor opcodes. Stores retain their existing path.
* ADDV/SUBV use unsigned sign-bit overflow expressions, preserving other SR
  bits and source/destination aliasing. The new test uses an independent signed
  64-bit arithmetic oracle, including extrema, carry-in T and aliased operands.

The existing compact table and deferred pure-sequence state publication were
inspected and retained. There is no measured opcode distribution, so these
instructions are candidates, not claimed mmpork hotspots. No sampled profiler
was added within this bounded experiment.

## Actual validation

Built at `-O3 -std=gnu++11 -D__LIBRETRO__ -DLSB_FIRST -DUSE_SPEEDHACKS
-Wno-write-strings -ffunction-sections -fdata-sections`, linked with
`-Wl,--gc-sections`, using the includes in `/tmp/cv1k-incflags` with
`-I../../` replaced by `-Isrc/` from the repository root.

Both `SH3_SMALL_HANDLERS=0` and `=1`, using g++ on x86 and
`aarch64-linux-gnu-g++ -mcpu=cortex-a55` on ARM64, passed
`tools/test_sh3_interpreter.cpp`:

* 448 scenarios x 3 interpreter paths: remap, alias self modification, page
  crossing/wrap, MMIO callbacks, IRQ, delay slots and timers.
* 23,136 pure encodings x 3 paths, including callback state publication.
* 392 ADDV/SUBV arithmetic/SR oracle cases.

The enabled ARM64 build of `tools/test_cv1k_loops.cpp` passed its full existing
suite, including 26,067 instruction encodings, 4,096 mixed spans, 1,050 store
and memory guards, 100,000 timer comparisons, 20,000 idle-page value/cycle
comparisons, handler invalidation, 20,000 existing DT/BF tests, 281 translated
branch cases and 200 linked slice/timer cases. That suite requires Linux
AArch64; x86 validation used the interpreter suite. ARM64 execution command:

```
qemu-aarch64 -cpu cortex-a55 -L /usr/aarch64-linux-gnu TEST_BINARY
```

`aarch64-linux-gnu-size` on the interpreter test binaries reported text
184,308 -> 172,736 bytes. `nm -S -C` reported JIT=0 batch function sizes:

| Function specialization | Off | On |
| --- | ---: | ---: |
| batch<false,false> | 12,488 | 10,764 |
| batch<false,true> (fetch-page) | 16,604 | 17,256 |
| shared long-load slow helper | absent | 232 |

These are compiler/layout observations, not runtime speed measurements.
In particular the default fetch-page function grew, despite smaller total
text. Keep this opt-in until real RK3566 A/B traces establish a benefit.
The supplied latest mmpork baseline (JIT=0, VRAM16=1, BLIT_TILES=1) is CPU
average 12.583 ms, first four 30.391 ms, worst gameplay 22.745 ms; no new
hardware trace, full emulator build, save/load test or speed claim was made.

## Integrated follow-up (v9)

The Astra worker committed a75bb58af locally. Integration retained
SH3_SMALL_HANDLERS=0 (shared RL tail remains a build-time experiment),
but separated arithmetic into SH3_FAST_ARITHMETIC, enabled by default
on AArch64. ADDV/SUBV use the tested branchless overflow expressions.
ADDC/SUBC use unsigned widened carry/borrow, CMPSTR uses zero-byte
detection, DMULS/DMULU use host 64-bit multiplication. Architectural
registers, flags and guest cycles are unchanged. Compile with
-DSH3_FAST_ARITHMETIC=0 to retain the old arithmetic. AArch64 QEMU
and x86 tests cover 392 overflow and 100000 further arithmetic cases
each. The ARM64 legacy arithmetic/shared tail build also passes.
Earlier worker code-size observations above refer to its isolated
experiment, not the final combined dispatcher.

## Counted interpreter loops (JIT=0)

FBNEO_CV1K_INTERP_LOOPS=0 disables the new loop path at runtime.
Default enabled in the existing pure fetch-page interpreter, never
in the JIT paths. Recognizes live, small, counted loops with DT and
a backward BF or BF/S:
- ADD #imm or register ADD with an independent loop counter,
  including ADD in a supported pure delay-slot arrangement;
- MOV.B/W/L postincrement source + direct destination store +
  pointer increment;
- MOV.B/W/L direct store + pointer increment, or predecrement fill.

At a completed backwards branch/slot boundary, preflight only
whole taken laps that fit the timer slice and leave a nonzero
counter. No terminal iteration is fused, no emulated cycles are
removed, and no persistent opcode or mapping cache is introduced.
Cap batches at 4096 laps and at live direct mapped page bounds.
Validate the entire code range, the separate DT data-read map and
BF word. Reject MMIO, unsupported register aliases, unaligned
addresses, P4 addresses, pending IRQ/delay and writes overlapping
the live host code or the DT data-read word. Preserve byte-swapped
host layout, sign extension, forward overlapping copy semantics,
EA/PPC/PC, T, counter, pointer and value registers. A safe contiguous
copy uses memmove only where it reproduces the original sequential
order. Fill and independent arithmetic can eliminate per-op dispatch.
The existing DT/BF busy-loop handler remains unchanged.

CV1K INTERP LOOP reports fused blocks, laps, equivalent guest ops
and arithmetic/copy/fill block counts per 120 frames. These are
execution counts, not performance estimates. We do not yet have
a real mmpork opcode histogram. FBNEO_CV1K_HOTPCS=1 can provide
the existing bounded slice-entry PC/opcode sample for diagnosis;
do not use that profiled run as an unprofiled timing benchmark.

Final verification in /tmp/fbneo-cv1k-rk3566-v9:
- ARM64 Cortex-A55 QEMU and x86: 3840 actual timer-dispatch loop
  comparisons each, 105354 fused laps in 8645 blocks, including
  55507 laps in 4333 delay-slot blocks; 10 side-effect-free guards.
- Both: 448 interpreter scenarios x 3 paths, 23136 pure encodings
  x 3 paths, 392 overflow, 100000 other arithmetic oracle checks.
- ARM64: existing full native/linked-loop/timer/IRQ/SMC suite
  (compatibility only; no JIT development), and legacy arithmetic
  plus opt-in shared-tail interpreter regression.
- sh4.cpp and d_cv1k.cpp compile at -O3 for ARM64 Cortex-A55 and x86.
Full libretro link, actual RK3566 speed, audio and save/load are not
verified. No clock, speedhack, DMA or graphics algorithm changes.
