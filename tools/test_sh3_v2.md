# CV1000 Dynarec V2 stage 3.0A

Base: merged DSNo master PR #185 (829442f19), followed by the
stage 2.7 default restoration on the original low-spec branch.
V2 is developed on feature/cv1000-arm64-dynarec-v2, separate from
feature/cv1000-arm64-low-spec. The legacy JIT modes are unchanged.

## Scope and activation

Initial native Linux little-endian ARM64 implementation, reusing the
previously audited straight-line integer/memory emitter. This is not
the final V2 IR, full SH3 basic-block translator or direct linker.
The stage 2.7 Flow interpreter remains available, including polling,
full-literal and other data-loop acceleration. Keep JIT=0, FAST_RAM=0,
PCREL_DISPATCH=0 and FLOW=1 when enabling FBNEO_CV1K_JIT_V2=1.
The dedicated V2 branch defaults to enabled for native Linux ARM64
libretro to support normal ES launch; the low-spec branch retains the
stage 2.7 default and does not contain V2. Conflicting modes or
unsupported architectures disable V2. Explicit
FBNEO_CV1K_JIT_V2=0 provides the control in the same build.
The timer schedule, clock, IRQ delivery, driver DIP, graphics and
serialized guest state are unchanged.

The new template specialization attempts native execution only at
published fetch/control-flow boundaries, not every opcode. An aligned,
same-page 12-64-op span must be visited four times before compilation.
All emitted instructions are from the existing one-cycle Candidate
set; the CMP/PZ full-literal hook is deliberately excluded. The new
16,384-entry direct-tag table uses virtual PC and live fetch host
pointer, plus a complete byte comparison before each execution.
Negative sites have bounded cooldown; repeatedly short guard exits
disable the entry temporarily. Existing branch/slot and exact-device
handlers run in the interpreter. A compiled span must fit the entire
remaining cycle budget; partial guards publish only executed ops.
The previous emitter checks live memory maps, alignment and store
overlap with the validated code interval (including host aliases).
Fetch is live on fallback. No stale-pointer dereference or persistent
native link. The driver logs cumulative V2 probe/call/op/build,
rejection, code-change, guard, full, cooldown and code-byte counts
alongside the existing 120-frame performance report.

## Code cache

The memfd-backed 8 MiB cache has separate shared RW and RX mappings
(no RWX mapping), 16-byte block alignment and immutable published
blocks. Publication cleans the writer alias and invalidates the
executable alias with cache maintenance/barriers before entry.
The file descriptor is closed after mapping. No mprotect toggles per
span. Mapping failure or exhaustion gracefully falls back; compiled
blocks remain usable after ordinary exhaustion. Cache/table reset
on CPU disable/exit; live code validation also covers remap, DMA,
cheat and restored guest bytes without relying on a write barrier.
No attempt to circumvent platform executable-memory policy.

## Tests

Build the standalone tests with the existing libretro include flags,
-O3 -std=gnu++11 -D__LIBRETRO__ -DLSB_FIRST -DUSE_SPEEDHACKS,
-mcpu=cortex-a55, -ffunction-sections -fdata-sections, -Wl,--gc-sections
and -pthread, then execute with qemu-aarch64 -cpu cortex-a55 and the
AArch64 sysroot. test_sh3_v2.cpp checks RW/RX publish/execute/full/reset,
enable/disable dependencies, actual native execution in seven
24-slice differential cases (pure, interior code replacement,
remap, MMIO burn, self-modifying store, long-map callback, timer/IRQ,
near-page-edge; some cases combine these), and budget/IRQ/delay/odd-PC
fallbacks. It compares registers, callback traces, timers, data and
code hash, not just a final checksum. test_sh3_v2 bench alternates
baseline/V2 for pure and mixed RAM loops. The existing interpreter
and poll/data tests remain relevant; test fetch mode 10 selects the
stage 2.7 C path and can exercise V2 with JIT_V2=1.

Future phases: new V2 emitter / cheaper direct memory guards, block
terminal branch and audited delay-slot translation, register-resident
state, complete physical-code write invalidation for CPU/DMA/cheat/
state restore and guarded direct links. Keep full live validation
until that invalidation is proven. Real RK3566 log coverage and FPS
are mandatory before judging whether to enable V2 generally.

## Results (2026-11-28)

Final ARM64 Cortex-A55 QEMU: cache lifecycle, default/override and
conflict policy, seven 24-slice differential scenarios with actual
native execution, budget/IRQ/delay/page/odd-PC guards, all passed.
Final SH3 production object and standalone PIC shared link passed.
The CV1000 driver object also compiled before the final default-only
change. Before that default change, ARM64 interpreter 448 x 11 and
23,136 x 9 cases, x86 448 x 10 and 23,136 x 8 cases, 65,536 decoder
keys and arithmetic checks passed. ARM64 polling (1,440 cases),
timer (24,000), data loops (1,008), full literal (2,268) passed with
V2 requested and test fetch mode 10; many short kernel cases do not
execute native code. The final default-policy and V2 execution suite
was then rerun successfully. No full libretro link or hardware test.

Synthetic 5M guest-cycle benchmark before the default-only change,
explicit on/off in both cases, Cortex-A55 QEMU, 3 alternating trials,
test-only counters and uncontrolled host load:
- Pure arithmetic: 409.381 -> 110.554 ms (73.0% lower).
- RAM/arithmetic: 526.744 -> 212.563 ms (59.6% lower).
Checksums and guest cycles matched across all trials. Native coverage was
about 93% / 91% in these intentionally long synthetic loops; actual
game coverage and host cost must be measured independently.
