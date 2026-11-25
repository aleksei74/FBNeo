# Experimental MIPS3 ARM64 native ALU and guarded RAM backend

This is an initial hybrid recompiler, not a full MIPS III/IV dynamic translator.
It shares the existing decoded-block cache and executes runs of at least three
non-trapping GPR ALU or guarded direct-memory instructions as AArch64 code.
Memory guard misses, exceptions, branches, delay slots, coprocessors, timing
and device callbacks still use the original interpreter. The existing idle-loop
optimizations remain intact.

## Device testing

On this experimental branch, Linux AArch64 builds enable the backend by default
for the VR5500 (System Board Y2). R4600 (Killer Instinct) defaults to interpreter.
Set `FBNEO_MIPS3_ARM64_JIT=0` in the RetroArch process environment to compare
against the interpreter; set it to `1` to explicitly enable either CPU type.
Other architectures continue using the interpreter. Define `MIPS3_ARM64_JIT=0`
at compile time to exclude the experimental backend entirely.

Example (adapt the config/ROM paths to the target):

```sh
FBNEO_MIPS3_ARM64_JIT=1 retroarch -v -L /tmp/cores/fbneo_libretro.so /storage/roms/fbneo/kof2002um.zip
FBNEO_MIPS3_ARM64_JIT=0 retroarch -v -L /tmp/cores/fbneo_libretro.so /storage/roms/fbneo/kof2002um.zip
```

Compare the same scene with the same governor, shader, frameskip and runahead
settings. Disable runahead and shaders for initial isolation. At core exit the
JIT reports compiled/executed spans and native guest instruction counts.
Those counters demonstrate coverage, not measured speedup. A Linux executable
mapping failure or a full code cache falls back to the interpreter.

### Y2 bottleneck profiling

On Linux ARM64 the experimental branch logs `Y2 PERF` every 120 emulated
frames by default at INFO level in libretro release builds. `FBNEO_Y2_PROFILE=0` disables the profiling timers and
guest-instruction counters. Set `FBNEO_Y2_PROFILE=1` to enable driver timings
on other hosts (JIT instruction counters are ARM64-only).

The core frame times exclude frontend shaders/presentation, frame pacing and
most frontend overhead. CPU+devices includes device handlers; wait, sync,
map and NAND times are nested and **must not be added together**. Background
render time is batch wall time collected at join, not extra serial frame cost.
The native ratio compares executed/attempted interpreter instructions with
native instructions; idle-loop skipped cycles are not counted as work.
Save/runahead/other driver callbacks outside DrvFrame can also contribute to
the nested timers, so disable runahead and rewind for controlled comparisons.

Test the same fight with JIT on/off using `FBNEO_MIPS3_ARM64_JIT=1/0`.
The experimental Linux ARM64 default now caps renderer helpers at four.
Compare `FBNEO_Y2_RENDER_WORKERS=2`, `=4`, and `=6` on an eight-core host
(maximum is the original automatic count). An explicit value overrides the cap.
The JIT also supports MOVZ/MOVN and removes redundant adjacent GPR reloads. Use `FBNEO_MIPS3_JIT_MIN_SPAN=2` or `=5`
to compare shorter/longer native spans against the unchanged default of 3.
This limits only the renderer helper threads, not the main CPU thread or the
asynchronous renderer coordinator. Unset the variable to restore the experimental four-helper default on Linux ARM64. Compare core average, peak and late frames, not just FPS.

## Differential test

`tools/test_mips3_arm64_jit.cpp` is a standalone AArch64 test (not linked into
the emulator). It includes the real CPU implementation so it can compare all
GPRs against its actual decoded instruction handlers, then compare CPU state,
memory, PC, cycle debt, and cache invalidation/rebuild across mixed blocks with
the backend disabled/enabled. Both R4600 and VR5500 are tested.

Use the compiler and flags from the libretro build:

```sh
make -Bn -C src/burner/libretro platform=unix ../../cpu/mips3/mips3.o ../../cpu/mips3_intf.o
```

From `src/burner/libretro`, substitute the Linux AArch64 cross compiler for g++,
remove `-DNDEBUG`, compile `../../../tools/test_mips3_arm64_jit.cpp` in place
of `../../cpu/mips3/mips3.cpp`, and compile `../../cpu/mips3_intf.cpp` separately.
Link the two test objects into a standalone executable with the same compiler.
Run on Linux AArch64, or under `qemu-aarch64 -L <target-runtime-root>` on x86-64.
The runtime root needs the AArch64 dynamic loader and target C/C++ libraries
if linking dynamically. No ROMs or copyrighted game data are needed.

Initial test coverage: 2,048 randomized sequences of 66 instructions per CPU
type; mixed ALU/load/store/branch blocks; annulled branch-likely delay slot;
guest code invalidation; save-state cache rebuild. Real game compatibility and
performance on handheld hardware still require testing.

## Limitations

- No native memory, branch, floating-point, HI/LO or trapping instructions.
- The native code cache is bounded and uses page-aligned RW-to-RX publication.
  Native code is discarded when the decoded-block arena is flushed; it is not
  serialized. Invalidated individual blocks cannot be looked up again.
- The prototype has no guest register allocator or block linking. Some small
  spans may be slower than the interpreter; actual device profiling is needed
  before expanding the backend or enabling it in master.

## Guarded Fast RAM experiment (MAME-inspired)

Reference: MAME's BSD-3-Clause MIPS III DRC memory accessors
([mips3drc.cpp](https://github.com/mamedev/mame/blob/master/src/devices/cpu/mips/mips3drc.cpp),
[ARM64 backend](https://github.com/mamedev/mame/blob/master/src/devices/cpu/drcbearm64.cpp)).
This is an independent adaptation of the fast-memory/fallback design, **not**
a port of MAME's UML backend. FBNeo already has read/write host-page caches
whose tags are invalidated when translation, mappings, or write tracking change.

- Linux AArch64 VR5500 defaults to guarded Fast RAM; R4600 remains opt-in.
- FBNEO_MIPS3_JIT_FASTRAM=0 restores ALU-only generation; =1 enables it.
- Supported: LB/LBU/LH/LHU/LW/LWU/LD and SB/SH/SW/SD, little-endian.
- The tag comparison includes the alignment bits. A failed guard exits
  before that instruction; its original leaf handler performs any TLB,
  MMIO, protected-page, first-write, or exception work. No replay of
  completed instructions. Remaining instructions execute normally.
- No native branch/delay-slot, unaligned merge, LL/SC, FPU or device access.
- Y2 PERF RAM reports successful native memory ops, guard exits, and
  uncached steps. Compare with the same game sequence using FBNEO_MIPS3_JIT_FASTRAM=0 and
  FBNEO_MIPS3_JIT_FASTRAM=1; a high guard exit rate or larger code cache can offset gains.
- Regression tests compare all 11 memory instructions, sign extension,
  address offsets, tag collisions, alignment, protected-page remapping,
  write tracking, code invalidation, restore, CPU state and cycle debt.
- QEMU correctness tests are not an Odin 3 throughput benchmark.
