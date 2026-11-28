# CV1000 Dynarec V2 stage 3.0B-1

Base: bab9e26b3 (stage A-fix), dedicated
feature/cv1000-arm64-dynarec-v2 branch. The low-spec branch is not changed.
Actual RK3566 stage A-fix first 7320 frames: mean CPU 12.291 ms,
vs 2.7 10.452 ms and initial V2 15.382 ms. Worst 360-frame core
22.058 ms vs 2.7 19.425 ms. The 48 published blocks retained
124925100 guest ops; 75248 bytes of executable cache, no exhaustion,
guard failure or retirement. These observed numbers do not establish
the exact unique guest block population or isolated lookup latency.

## Emitter and ABI

sh3_arm64_v2_emit.h contains a separate block emitter with static
register bindings, not the V1 age-based slot allocator. Existing
audited opcode encodings/semantics are retained. x19 = register
array, x20 = SR address, x21 = EA address, x22 = Memory context.
w9-w15 and w23-w28 hold up to 13 guest integer registers. No x18,
no per-op register eviction, no generated C calls. Wide blocks
preload live maps into x0/x1/x2 and the protected code interval
into x3/x4. The shared assembly trampoline preserves AAPCS64
callee-saved registers and LR in a 96-byte aligned frame, with
BTI entry and CFI annotations. After the initial A/B showed call
overhead in the small trampoline, blocks using at most seven
guest bindings were changed to leaf functions using the ordinary
x0-x3 ABI directly, without saving callee registers. Unused w9-w15
bindings can hold map and code interval pointers as x-registers.
If not enough bindings remain, the corresponding map/interval
pointer is loaded from the context on demand. w5 is SR, loaded
only when needed. Leaf exit count is kept in w6 until register
writeback through x0 is complete.

All used guest registers are initialized at block entry, including
write-only operands, so the union of modified registers can be
committed in one epilogue even after an early guard. All failure
branches for one guest instruction share a two-instruction stub
(return completed-op count + branch to the block epilogue). The
original direct-map alignment, handler, endian and host-alias code
overlap guards are retained. P4 accesses deliberately fall back.
No MMIO in generated code. No block-to-block residency yet: each
invocation returns through the C++ frontend (wide blocks via the trampoline).
The original emitter remains the fallback when a span uses more
than 13 guest registers or cannot be compacted. Existing 12-64
straight-line candidate scope is unchanged; branch/PC-relative/DT
coverage is the next stage.

## Frontend

- 8192 physical page tags (16 bits each). A sticky tag bit derived
  from virtual PC is set only after successful publication, so pages
  without a possible compiled entry avoid the four-way directory.
  False positives are harmless; live validation remains authoritative.
  Retired entry bits remain until cache reset to avoid false negatives.
- Sample once at a pseudorandom guest-cycle threshold per CPU slice,
  on an eligible backward miss; also at every 16th slice entry.
  This replaces the repeated RMW of the global sample clock on
  every backward miss. The policy is deliberately bounded and can
  miss hot targets; measure actual compact coverage on hardware.
- Sh3Reset and Sh3Scan(ACB_WRITE) invalidate all native directory,
  page/learning tags and code cache (no guest scan-field change).
  Stats remain cumulative across these cache resets. The full
  live fetch pointer and memcmp guards remain. DMA/CPU/cheat write
  invalidation for removing memcmp is NOT implemented yet.
- 8 MiB RW/RX cache remains, no eviction on hash collisions.
- Diagnostic log CV1K V2 emitter reports compact built/calls/ops/bytes,
  legacy built and cache resets. compact bytes is cumulative raw
  generated payload, not necessarily current aligned cache occupancy.

## A/B

Dedicated V2 branch defaults to V2 and compact emitter on supported
Linux ARM64 libretro, normal ES launch. Options:
  FBNEO_CV1K_JIT_V2=0 : same-binary stage 2.7 control
  FBNEO_CV1K_JIT_V2=1 FBNEO_CV1K_V2_COMPACT=0 : new frontend + legacy emitter
  FBNEO_CV1K_JIT_V2=1 FBNEO_CV1K_V2_COMPACT=1 : new frontend + compact emitter
  FBNEO_CV1K_V2_LOOKUP_ONLY=1 : with V2 on, validate/promotion without native execution
The legacy-emitter setting is not a byte-identical build of bab9e26b3.
Compare first 7320 frames, initial 480f, high-load windows
6360/6480/6600, same clock/temperature/power/input where possible.
No claim of 50% coverage or full-speed hardware performance.

## Verification

test_sh3_v2_emitter.cpp compares against the real SH3 handlers:
- 26067 single-op encodings, 38400 unmapped/P4/long-map guard checks.
- Final mixed 64-op blocks test both wide and leaf ABI and every
  possible guard exit position, including preserved x19/x28 checks.
- Store cases cover both ABIs and alias/overlap/alignment, with
  dirty prefix state preserved.
- Initial two-trampoline candidate: memory-heavy 32-op span
  3532 -> 2608 bytes (-26.2%); mixed payload -23.3%; single-op
  payload +6.9%. Its small-block invocation cost regressed in
  the synthetic A/B, motivating the final leaf ABI below.
- test_sh3_v2: real slice differential with compact and legacy
  selection, cache lifecycle, reset/restore, collision/pinning,
  page-filter, bounded samples, wide-footprint fallback.
- ARM64 O3 SH3 object, standalone shared link and CV1000 driver
  object compiled; x86 interpreter differential passed.
  Full Libretro link and physical RK3566 performance still required.

Future work: 3.0B-2 PC-relative MOV/MOVA, exact DT semantics and
terminal branch/slot lowering with budget and device guards; 3.0B-3
audit all writes/aliases and only then consider removing memcmp;
3.0C direct links and cross-block resident registers.

## Final leaf / wide ABI results

Final ARM64 QEMU, all test processes exit zero:
- 26067 encodings and 38400 guard checks.
- 4096 mixed blocks, exactly 2048 wide and 2048 leaf, including all
  guard positions and preserved-register checks.
- 2100 alias/overlap/alignment and prefix-state tests.
- Compact and legacy integrated suites: 168 slices each, live code
  edits, remap, MMIO, timer/IRQ, self-modification, cache reset on
  ACB_WRITE and CPU reset, page filter, bounded samples, fallback.
- Final production ARM64 PIC object and standalone shared link passed.
- Before the final leaf-only emitter refinement, the ARM64 interpreter
  448 x 11 modes, 23136 x 9 pure encodings, 1430 branches, 210 stack,
  360 DT/BRA, 23824 fused slots, 1440 polling, 24000 timers, 1008 data
  loops and 2268 full literal cases passed. The driver object also
  compiled then. Final leaf/wide native and integrated tests above
  were rerun after the refinement. The non-ARM64 production object
  and standalone shared link also passed. Serialized fields unchanged.

Final payload:
- Memory-heavy 32-op span: 3532 -> 2612 B (-26.0%).
- Mixed blocks (new 50:50 leaf/wide test distribution):
  2444612 -> 2024332 B (-17.2%).
- Individual single-op blocks: 1718084 -> 1941408 B (+13.0%).
  V2 continues to require >=12 guest ops; not intended for single
  instruction entry. Shared trampoline excluded from payload totals.

Same final executable, three rotating trials per mode, 5M timed
guest cycles, Cortex-A55 QEMU, median milliseconds:
- Pure arithmetic: control 252.922, legacy 66.995, compact 67.988 ms.\n- RAM/arithmetic: control 194.925, legacy 77.427, compact 79.072 ms.\n- Uncompiled short loop: control 218.019, legacy 224.432, compact 217.921 ms.\n
Guest snapshots, native op counts (legacy vs compact), and cycles
matched. Host load was not controlled; trial variation was substantial.
The final leaf emitter is close to legacy timing in these synthetic
loops, not demonstrably faster. The shorter uncompiled-loop median
shows much lower observed overhead than the previous A-fix experiment,
but those are different runs and not controlled hardware A/B.
The dedicated experiment defaults to compact for real coverage and
performance testing; COMPACT=0 retains the original emitter with
the new frontend. No promise of improved hardware FPS.
