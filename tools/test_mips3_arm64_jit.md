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


## Packed native-code cache (MAME-inspired)

MAME's [drccache.cpp](https://github.com/mamedev/mame/blob/master/src/devices/cpu/drccache.cpp)
advances an aligned code pointer after emission instead of consuming an entire
page per block. FBNeo now packs publications at 16-byte boundaries, reserves
up to 128 MiB of virtual address space, and uses RW then RX permissions
(no RWX). Only the tail page can temporarily become writable; compilation
and generated-code execution are serialized on the CPU thread. If a
permission transition fails, the dispatcher refuses all native spans until
the next explicit enable/init. Decoded blocks and native code still share
their original invalidation/reset lifetime.

- Default: packed 128 MiB cache. Unused virtual address space is not a
  preallocated 128 MiB physical RAM buffer.
- FBNEO_MIPS3_JIT_CACHE_MB=32 or =64 selects a smaller limit.
- FBNEO_MIPS3_JIT_PACKED=0 selects the old page-aligned allocation.
  Combine with FBNEO_MIPS3_JIT_CACHE_MB=32 for the old cache policy.
- Y2 PERF CACHE shows emitted payload, page-rounded code extent, capacity,
  and rejected publications in the last interval. Mapped code extent is
  not an RSS measurement.
- TestPackedCache covers multiple executable spans sharing one page,
  a span crossing a page boundary, exact capacity, overflow rejection,
  preservation of old code, and reset/reuse.
- This change does not implement MAME's UML compiler or block linking.


## Integrated Y2 performance branch

Based on the tested Fast RAM / packed-cache commit 72618c026.
References: MAME's BSD-3-Clause mips3drc.cpp (sequence generation,
delay-slot/branch generation and cycle accounting), drcbearm64.cpp (host
register mapping) and drccache.cpp (packed executable code storage).
The implementation is independent, retaining FBNeo's CPU/device model.

- Six caller-saved ARM64 registers cache guest GPRs across ALU and guarded
  memory operations. Dirty registers are committed on eviction and all
  returns, including guard failure. Immediate arithmetic uses direct
  encodings where possible.
- Native normal and likely integer branches, link registers, ALU and
  supported RAM delay slots. Likely annul and delay exceptions keep the
  interpreter's architectural PC, EPC, BD and cycle behavior.
- Non-taken conditional branches continue inside an existing decoded
  superblock while its event budget permits. Taken branches, expired
  deadlines, exceptions and unsupported instructions exit to the original
  dispatcher. Cross-block machine-code linking, FPU/CP0 and unaligned
  merge operations are deliberately not implemented.
- FBNEO_MIPS3_JIT_INTEGRATED=0 selects the preceding ALU/Fast RAM emitter;
  =1 opts into the integrated emitter (default for VR5500). All previous
  Fast RAM, cache and profile controls remain available.
- Y2 batches protection-map changes and fences/copies geometry list
  input in page ranges rather than per word. FBNEO_Y2_BATCH_MAP=0
  restores individual map invalidations.
- Y2 range synchronization drains the whole queue on a pending conflict,
  or joins only the running batch when pending work does not conflict;
  unrelated queued jobs can then run asynchronously. Explicit full
  fences (state scan, reset, cache sync and list reuse) stay unchanged.
  FBNEO_Y2_SELECTIVE_SYNC=0 restores full draining on range conflicts.
- ARM64 NEON accelerates integer constant-colour modulation and alpha-factor
  blending without changing rounding or saturation. Other architectures
  use the original scalar arithmetic. FBNEO_Y2_NO_NEON is a compile-time
  fallback. No floating-point raster or texture interpolation precision changes.
- Y2 PERF FLOW includes completed native branches, sync calls, serial
  pending jobs, selective waits and map update/range counts.

Tests: the standalone MIPS3 test adds register-cache differential tests,
guard failures with dirty GPRs, integer branches/likely branches, native
RAM delay slots, fused superblock fallthrough and event exits, nested map
batches and delay exceptions. tools/test_y2_pixel_ops.cpp compares NEON
against scalar colour arithmetic; tools/test_y2_job_sync.cpp checks the
page conflict planner. Build these two small tests using the target C++
compiler with -O2. These are correctness tests, not actual game timing or
multi-threaded rendering/frame checksum tests. Real-device validation
is still required.


## Y2 media/IO follow-up (based on 1d4b359475)

The integrated branch's retroarch8.log reached 91.41% weighted native
coverage in the 55 matched high-load windows. Render synchronization
remained approximately 3.05 ms/frame in those windows, while a 189 ms
peak coincided with substantial NAND I/O. These nested timers are not
independent costs. This follow-up does not modify the CPU JIT or virtual
device timing.

* MVC (movie decoder) batches contiguous independent macroblocks.
  A group must have the same output address, width and height; each
  macroblock coordinate must be unique; every used reference frame
  must be entirely in RAM and not overlap the output. Repeated blocks,
  aliases, other device commands and incompatible layouts end a group.
  Groups of at least 16 blocks use the existing render workers and join
  before any dependent work. No extra persistent worker threads.
  FBNEO_Y2_MVC_BATCH=0 restores serial MVC execution.
* ARM64 NEON accelerates exact half-pel prediction, bidirectional
  averaging and saturation of residual plus prediction. IDCT and
  boundary-clamping behavior are unchanged. FBNEO_Y2_MEDIA_SIMD=0
  restores the scalar media arithmetic.
* Queue-generation-scoped footprint memoization skips repeated or
  contained marks that already have sufficient protection. Write
  tracking is still called before checking the memo; clearing or
  swapping the pending map invalidates the memo.
  FBNEO_Y2_FOOTPRINT_MEMO=0 disables this optimization.
* Decrypted NAND cache: default 32 MiB of page data, four-way LRU,
  with a last-page shortcut. FBNEO_Y2_NAND_CACHE_MB=2 restores the
  old 2 MiB direct-mapped layout; 8,16,32,64 select four-way capacities.
  Default memory increase is approximately 30 MiB plus metadata.
  Only demanded records are read; no speculative I/O or extra
  disk threads. Decryption and record addressing are unchanged.
  Cold CHD reads may still cause a long stall; a larger cache primarily
  helps recurring data and conflict misses.

Y2 PERF MEDIA reports completed MVC jobs, parallel blocks, batch
count and wall time, and render/YUV job counts and time. The background
totals are merged only after the existing worker join, so work can
cross interval boundaries. These timers overlap CPU/sync/background
time and are not to be added to frame time.
Y2 PERF IO reports NAND evictions and footprint call/memo-hit counts.
Existing profile fields remain intact for comparison.

tools/test_y2_media.cpp includes the actual driver for standalone
regression tests. Build with the normal d_y2.cpp include paths and
defines, -ffunction-sections -fdata-sections -Wl,--gc-sections
and -pthread. It compares the actual sequential and parallel MVC
outputs (including alias and duplicate barriers), exercises the cache
addressing/replacement, 20000 exact SIMD arithmetic cases, and 8000
real driver footprint calls across queue generations. ARM64 runs
under QEMU, x86-64 directly. This is a functional regression test,
not a game benchmark or complete frame hash comparison.
