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


## Y2 CHD / YUV / raster follow-up (based on e3492aaae)

retroarch9.log vs retroarch8.log matched 103 windows: mean core
9.65 -> 9.29 ms, high-load sync 3.87 -> 1.93 ms, NAND misses
25482 -> 25455. The 32 MiB decrypted cache did not materially reduce
misses in KOF2002UM; the largest frame peak rose from 189 to 206 ms.
These are individual runs, not a controlled storage benchmark.

- Default decrypted NAND cache returns to 2 MiB; 8/16/32/64 MiB
  remain selectable using FBNEO_Y2_NAND_CACHE_MB.
- For Y2's immutable CHDs, BurnHDDReadBytes copies byte ranges directly
  from cached hunks when the diff is empty. Other drivers keep the
  original sector loop. On hunk error use the original error/sector path.
- Linux AArch64 Y2 defaults to bounded asynchronous CHD read-ahead.
  After three consecutive distinct increasing hunks, request the next
  hunk. At most one speculative hunk per disk, using a separately opened
  libchdr/file context. No simultaneous access to the demand decoder.
  Exact matching in-flight demand may join; unrelated demand never waits
  for that worker. Pending requests may be discarded, completed ones may
  be reused. Thread/handle failure falls back to demand. All worker
  resources are joined and freed before closing the image. If a diff
  exists at initialization, do not start read-ahead. Diff-sector reads
  retain precedence even if writes are added later.
  FBNEO_Y2_CHD_READAHEAD=0 disables speculation (direct byte-range
  copies and diagnostics still work). On other Linux architectures
  opt in with =1; non-Linux remains demand-only.
- Exact NEON YUV 4:2:0 conversion of eight pixels, all four output modes,
  with packed RGB555/RGB565/ARGB4444/ARGB8888 and Morton pairs. Bounds
  and nonaliasing input/output checks required; invalid/aliased layouts
  use the original scalar conversion. No rounding, sampling, virtual
  device time or output format changes. FBNEO_Y2_YUV_SIMD=0 restores
  the old YUV pixel loop; FBNEO_Y2_MEDIA_SIMD=0 also disables it.
- Extend existing NEON blend to all source/destination factor pairs
  for the generic rasterizer and the channel-dependent fast span.
  FBNEO_Y2_RASTER_SIMD=0 restores those scalar loops. The already
  optimized alpha-only fast span is unchanged.
- Y2 PERF CHD: demand decode count, speculative issued/hit/waited/
  discarded/failed counts, foreground hunk read time including matching
  waits and session maximum. These timings are nested in NAND/CPU
  counters, not additional frame time. An issued job may complete in
  a later profiling window.

Regression tests:
- tools/test_y2_media.cpp: prior MVC/cache/map tests plus 240000 YUV
  pixels, 3840000 blend factor cases and 64 real YUV layouts, including
  partial tiles, all modes, bounds/alias fallback. Run on ARM64 QEMU
  as documented above.
- tools/test_hdd_readahead.cpp: standalone Linux C++11 -O2 -pthread,
  bounded queue and matching/unrelated demand, failure and clean exit.
- tools/make_y2_chd_fixture.py / tools/test_y2_chd.cpp: synthetic
  compressed V3 CHD, real libchdr, native stdio adapters to libretro
  VFS ABI, 1194 range comparisons with the old sector path, overlay
  writes and shutdown. Link normal libchdr/codec objects from the
  target build and zlib/pthreads, with the usual burn_hdd.cpp include
  paths, -ffunction-sections -fdata-sections -Wl,--gc-sections.
  Fixture generated locally, contains no game data.
- ARM64 and x86-64 production module compilation succeeded.
  Functional checks do not prove timing improvement on the Odin 3.
  Actual V5 game CHDs, frontend VFS, full-core build and real game
  performance remain to be validated. Prefetch can compete for I/O
  or CPU on some systems; compare with =0 if necessary. No GPU
  renderer, block linking, or FPU JIT implemented in this follow-up.


## Y2 linked native blocks / guarded FPU / IDCT (based on 9b5c5c745)

retroarch10.log and retroarch11.log both used CHD read-ahead. The
shell-only environment override did not reach the frontend. Because
log10's read-ahead frequently waited for completion and its worst
frame peak increased, the default is now **off**. Explicit opt-in
FBNEO_Y2_CHD_READAHEAD=1 remains available. Direct CHD byte-range reads,
YUV SIMD, MVC batching, and other previously verified changes remain.

### Linked native-block execution

Two warm successor slots per decoded block store full virtual PCs,
decoded-arena offsets, and a shared 64-bit invalidation generation.
Only blocks consisting of a single complete native span, without idle
skipping or split delay slots, enter the linked runner. A link is learned
from a current-generation FastCache hit; no block compilation or cache
allocation occurs inside the linked loop. Every boundary retains the
original event deadline, cycle accounting, and delayed-slot checks.
Remap, code-write, restore and decoded-arena invalidation make stale
links unreachable. Guard failures complete using the original handlers.
This is data-only linking to native entries from a C++ runner; each
span still makes a host function call. It is NOT machine-code tail
jump patching or a cross-block register allocation scheme. Executable
pages remain W^X and the native emitter cache is unchanged.

FBNEO_MIPS3_JIT_LINK=0 disables linked execution. Default on for
ARM64 VR5500, off for other CPU types; explicit =1 opts in. Default
VR5500 ARM64 decoded arena 16 MiB (old 8 MiB), because log10 showed
a decoded/native cache flush late in the session. Set
FBNEO_MIPS3_DECODE_MB=8 or 32 to compare. Other hosts/CPU types retain
the original capacity. Link metadata adds a small amount of decoded
block storage. Y2 PERF LINK reports linked-runner blocks (including
guard exits), successor hits, newly learned links, and decoded arena
usage/capacity. These are coverage counters, not measured speedup.

### Guarded FPU spans

MFC1/DMFC1/MTC1/DMTC1; ADD/SUB/MUL/DIV/SQRT.S and .D; ABS/MOV/NEG.S
and .D now emit native AArch64 code in integrated spans. FR=0 register
pairs and FR=1 registers are separately specialized, with a live CU1
and FR guard. Single register writes preserve the other half in FR=0,
and clear the upper word in FR=1. Arithmetic requires normal finite
inputs and result; zero, subnormal, infinity, NaN and invalid result
fall back to the original handler before architectural writeback.
Bitwise moves and sign operations preserve all raw bit patterns.
Conversions, comparisons, FP memory operations, unsupported
encodings and FPU delay slots remain interpreted. No guest FPU
rounding/exception policy is changed. FBNEO_MIPS3_JIT_FPU=0 disables
emission; default on only with the integrated VR5500 backend.
Y2 PERF FPU reports attempted spans containing FPU and COP1 guard
exits. It is not an FPU instruction count.

### MVC inverse DCT

Double-precision NEON computes two output samples in parallel with the
same ordered nonzero terms and a transposed basis. DC-only shortcut
and scalar residual transform remain unchanged. A 1e-6 distance
guard around half-integer rounding ties falls back to the original
scalar block, ensuring no precision loss from fused versus unfused
operations for bounded int16 coefficients. FBNEO_Y2_IDCT_SIMD=0
disables the transform; FBNEO_Y2_MEDIA_SIMD=0 also disables it.

### Regression tests

- ARM64 QEMU real MIPS3 core: 240 linked/unlinked snapshots with
  branches, protected memory, code rewrite, remap, timer deadline and
  restore, plus 611 warmed successor hits in the synthetic program.
- 24,000 generated FPU operations compared with real interpreter
  handlers, both FR layouts, normal and arbitrary bit patterns,
  CU1 disabled, mismatched layout and guarded exceptional values.
  Mixed FPU/ALU/branch program with a runtime FR layout switch.
- Existing R4600 and VR5500 native ALU, RAM, branch/delay, event,
  cache and CPU snapshot differential tests.
- 30,000 actual sparse/dense IDCT blocks; 29,948 NEON and 52 scalar
  fallback in the tested seed, all 64 residuals identical in each.
  Existing real MVC, YUV and raster differential tests.
- ARM64 and x86-64 production compilation of mips3.cpp,
  mips3_intf.cpp and d_y2.cpp. Full core build and real device
  performance still unverified. Native machine-code tail linking
  and a Vulkan hardware renderer are not part of this change.


## Y2 resident NAND and native tail runner (based on 96936085a)

retroarch12.log matched the first 128 windows of log11. Mean core
8.796 -> 8.803 ms, NAND I/O 0.198 -> 0.149 ms, peak 284.76 ->
188.75 ms, decoded/native cache flush 1 -> 0. The longer 179-window
run nevertheless had a new 400.15 ms peak in window 162 alongside
380 CHD hunk decodes. These are observed associations, not isolated
storage or CPU benchmarks.

### Complete decoded CHD residency

For Linux ARM64 Y2, FBNEO_Y2_NAND_RESIDENT defaults to auto. Both
immutable base CHDs are fully decompressed at initialization into
host RAM, only when their combined padded image size plus 2 GiB
fits the current MemAvailable (and finite root cgroup-v2 headroom,
where exposed). Y2's two 0x42000000-byte images require 2112 MiB;
auto thus requires at least 4160 MiB available after the initial
driver allocation. FBNEO_Y2_NAND_RESIDENT=0 disables this mode;
=1 explicitly opts in with a smaller but still mandatory 1 GiB
reserve (also available on other Linux architectures). Non-Linux,
raw/missing CHDs, unknown memory budget, allocation or decode
failure fall back to demand reads. The second allocation rechecks
available memory. If either image fails, release both. This is
a conservative admission check, not a guarantee against external
memory pressure or every possible nested cgroup configuration.

All hunks must decode successfully before a resident pointer is
published. Original CHD byte ranges then use direct memcpy from
the immutable RAM image. The existing diff overlay takes precedence
for written sectors. No disk modifications, persistent raw files,
or speculative threads are needed. Both complete RAM images are
released on close. Existing NAND decryption and virtual timing
remain unchanged. The 2 MiB decrypted-page cache remains in place.
CHD read-ahead remains off by default; a resident image never
starts the read-ahead worker even when explicitly requested.
The tradeoff is a longer game startup and about 2.06 GiB extra
resident data; a real Odin 3 memory budget and game startup time
have not yet been measured. Full residence removes repeated CHD
I/O/decompression during gameplay, not every possible frame stall.
Y2 NAND resident startup messages report enabled/disabled/fallback,
image size, memory and elapsed initialization time.
Y2 PERF RESIDENT reports RAM reads and bytes (hunk bytes for sector
fallback) and resident status of each disk. Y2 PERF CHD demand
counts should be zero after successful complete residency.

### Native machine-code chain runner

ARM64 VR5500 uses a shared AArch64 assembly continuation for
current-generation warm successor links. Eligible blocks contain
one complete integrated terminal-flow span, no split/idle skipping
or FPU span. The entry uses the existing W^X code image. Emitted
blocks preserve x19..x28 and return to the common continuation;
it accounts for completed block cycles, RAM hits and branches,
checks the event deadline and full-PC/generation link, and branches
straight into the next eligible native entry without calling
C++ between warmed blocks. Taken superblock exits and annulled
delay slots use their exact returned entry/pseudo counts. A
guard failure, event deadline or missing/ineligible link exits
the runner, returning the final block result to the original
C++ completion path exactly once. No JIT executable pages are
patched; no registers remain cached across generated blocks.
Code writes, remapping, restore and cache flush retain the existing
generation invalidation. The native spans themselves cannot call
device callbacks: those go through the existing guarded side exit.
The assembly path is excluded from per-block debug profiling.
This implements native-to-native dispatch through a common
machine-code continuation, not a patched direct jump at each
generated block's own epilogue. FBNEO_MIPS3_JIT_TAIL=0 restores
the prior C++ linked path, and FBNEO_MIPS3_JIT_LINK=0 disables
both. Y2 PERF TAIL reports runner entries and successful inter-block
jumps. Some entries may have no jump, so coverage alone does not
prove speedup; test on real hardware.

### Validation

- ARM64 QEMU: real MIPS3 linked/unlinked/tail 240 snapshots,
  remapping, code rewrite, restored arena, deadlines and RAM guards:
  611 assembly successor jumps; state and memory identical.
- 16-block ring, 256 varying deadlines and a mid-run code rewrite:
  41,528 native jumps across 255 runner entries, architectural state,
  cycle debt, native instruction/span/branch and guest counts identical.
- Existing R4600/VR5500 ALU, guarded RAM, flow/delay, FPU regression
  tests all pass (including 24,000 FPU operations).
- Actual libchdr synthetic compressed V3 CHD test: 1,194 previous
  range comparisons plus 1,002 resident comparisons; zero demand
  decodes during resident reads; budget, diff precedence, freeing and
  demand fallback validated. Real V5 NAND data was not used.
- Existing MVC, IDCT, YUV, blend, footprint, NAND and read-ahead
  regression tests pass; memory-policy budget boundaries checked.
- ARM64 and x86-64 production compile of burn_hdd.cpp, d_y2.cpp,
  mips3.cpp and mips3_intf.cpp succeeded. Full linked libretro build,
  startup on the physical device, memory pressure and performance
  remain unverified. No Vulkan renderer or texture-cache redesign
  in this follow-up.
