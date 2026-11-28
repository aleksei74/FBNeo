# CV1000 SH3 3.0C-3 resident regions (experimental)
Base: f6eedc71d, C-2. Separate feature/cv1000-arm64-dynarec-v2-c3
branch. Existing V2 and low-spec branches are not changed.

## Runtime design
- Region compiler takes up to twelve already promoted compatible
  compact blocks, with local B-rel26 and ADR fixups; one publication
  in the same 8 MiB dual-mapped code cache. Regions are immutable and
  bounded to 512 descriptors. No C-2 link_step in a resident region.
- One trampoline per region invocation. R0-R4/R15, SR, PR, PPC, budget
  and counters stay in registers. Other used GPRs load/store per body.
  Each destination checks worst-case cycle cost before execution;
  each completed body charges actual cycles and guest operations.
- Eight-entry call-local RAS, matching actual guest PR before BR.
  BSR and observed JSR/JMP use direct body links if the verified target
  belongs to the region. Unseen/mismatched targets return to the original
  dispatcher. The predictor requires two identical observed targets;
  changes retrain it. No host RAS address escapes the invocation.
- Preserve C-1 native self-repeat; non-repeat blocks without a resident
  region use the interpreter instead of C-2's expensive short links.
- Region entry checks all member snapshots when necessary. Registered
  pages retain C-2's dirty/opaque/slice fences, unknown pages compare
  live opcodes on each invocation. Direct stores to watched/ambiguous
  pages exit BEFORE writing; interpreter performs the exact operation.
  No MMIO callbacks, remapping, or timer callbacks inside regions.
  Reset/restore destroys all region addresses. Code writes/remaps
  invalidate stale regions before further execution.
- P4 scratch optimization requires explicit 16 KiB backing registration
  and validated R/W/F mapping. Watched backing overlap and remaps revoke
  the exemption; out-of-range and unknown P4 writes remain opaque.
  The per-Sh3Run slice fence is deliberately retained: no proof that
  every frontend raw RAM writer is restricted to frame boundaries.
- No change to guest clock, IRQ/timer schedule, save format, or DIP.
  No new Tier 2 opcodes. Synthetic tests do not guarantee game FPS.

## Controls
On this experimental branch, resident regions are enabled by default
on supported Linux ARM64 Libretro with V2, chaining and write watch.
FBNEO_CV1K_V2_RESIDENT=0 selects the C-2-style engine;
FBNEO_CV1K_JIT_V2=0 selects the optimized interpreter.
The C-2-style control includes the new bounded P4 policy; it is not
byte-identical to the previous C-2 commit. Other branches unchanged.
Startup: "CV1000 SH3 V3 resident regions: enabled".
CV1K V3 resident reports groups, completed blocks, guest ops, built,
reject, invalid and member comparisons. The older V2 counters retain
their other paths' semantics and are not a per-body probe count for C3.

## Validation
QEMU Cortex-A55, GCC AArch64 -O2, same executable and three rotated
5M guest-cycle trials. Independent interpreter snapshots compare
registers, PC/PPC/PR/SR/EA, cycles, callbacks, timers, RAM and code.
- 224 differential slices: conditional/delayed, BSR/RTS, wide/leaf
  RAM, MMIO guard, cross-page, timer remap/IRQ.
- 96 additional differential slices: JMP/JSR predicted target changes.
- Mirrored watched store exits before modifying a linked target;
  region invalidation and restore passed.
- P4 extent, alias watch and remap revocation passed.

Final synthetic benchmark medians (ms, lower is better):
| Fixture | Interpreter | C-2-style | C-3 |
|---|---:|---:|---:|
| Conditional cycle | 223.624 | 345.699 | 33.497 |
| BSR/RTS | 338.267 | 366.134 | 15.954 |
| Wide/leaf RAM | 241.267 | 111.689 | 11.374 |

All snapshots matched. Both conditional and BSR/RTS pass the 2x
synthetic target, as does wide/leaf. Host load varied (ROCKNIX build);
these are QEMU timings, NOT physical Cortex-A55 timings.
No full core build or RK3566 hardware test yet.
Test identical 7320 frames; report 480f startup, 6241-6600f high-load,
all core-late and audio underrun counts, resident region counters,
POLL/DATA/TILE agreement, and actual speed on RK3566. In particular
watch for short regions, frequent watched-page guards and region
capacity. Do not merge into the stable low-spec branch until measured.

## Reproduce standalone tests
From repository root:
aarch64-linux-gnu-g++ -w -O2 -std=gnu++11 -D__LIBRETRO__ -DLSB_FIRST -DUSE_SPEEDHACKS -mcpu=cortex-a55 -ffunction-sections -fdata-sections -Wl,--gc-sections -pthread -Isrc/burn -Isrc/burner -Isrc/burner/libretro -Isrc/cpu -Isrc/burn/devices -Isrc/intf -Isrc/burner/libretro/libretro-common/include tools/test_sh3_v2_c3.cpp -o /tmp/cv1k-c3-test
qemu-aarch64 -cpu cortex-a55 -L /usr/aarch64-linux-gnu /tmp/cv1k-c3-test
qemu-aarch64 -cpu cortex-a55 -L /usr/aarch64-linux-gnu /tmp/cv1k-c3-test bench
