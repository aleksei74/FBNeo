# CV1000 Dynarec V2 C-2a/b: compact write watch and static native links

Base: d6f9af869 (C-1). Experimental V2 branch only. The actual
RK3566 mmpork 7320-frame log showed startup 480-frame CPU time
10.906 ms (stage 2.7 21.395 ms), but high-load 360-frame CPU
37.096 ms (B-2 28.334 ms; 2.7 18.967 ms). C-1 made 38.71M
native body calls in those 360 frames, with 27.36M additional
C++ chained calls. The shadow barrier saw nine code backing pages,
8.26M dirty entry checks, 10.58M opaque updates, no changed code.
These counts establish overhead candidates, not independent timings.

## C-2a: write watch and validation

- 8192 physical-page byte IDs replace the 65536-entry pointer table.
  254 unique host backing pages; generation array of 256 uint64_t.
  Zero = not watched, 255 = opaque/overlapping map. Host aliases
  receive the same ID. Overflowing, cross-page and P4 writes remain
  conservative; an exhausted watch directory falls back to exact
  validation. Store generation increments move to a native cold stub.
- The CV1000 driver explicitly registers its actual board MMIO and
  checked SH3 internal handlers (DMA writes are already instrumented).
  Unknown handlers still increment the opaque epoch; replacing
  handlers or write-port callbacks revokes their registration/trust.
- Only explicitly registered, live-validated CV1000 ROM/RAM backing
  pages qualify for conditional validation. RAM registration checks
  all 16 MiB of guest mappings, including B/C mirrors or Type D.
  Unknown mappings, allocation failure and VERIFY_ALWAYS keep exact
  byte comparison. All mapping changes revoke trust.
- Libretro can expose All RAM to the frontend. Do not assume the
  CPU/write hooks see raw external mutations: increment a separate
  epoch at each CPU slice. The first block entry in a new slice
  compares live bytes, even if the write generation has not changed.
  Within that slice, unchanged registered pages can skip comparison;
  writes to watched aliases or the opaque epoch trigger revalidation.
  External writes must be synchronized with the owner CPU thread,
  as in the original emulator. No promise of concurrent raw host
  writes or of universally complete write-hook coverage.
- Actual opcode changes retire the entry. Reset/ACB_WRITE clear the
  cache and watches. Original overlap guards and live DATA reads
  (including DT lookahead and idle-page guards) remain.

## C-2b: native static direct links

- Each eligible non-self-repeat compact block publishes a 64-byte
  entry/exit stub with a pinned descriptor, a common ARM64 native
  step helper and two aligned B-rel26 patch slots. Descriptors have
  their own immutable opcode snapshots; moving directory entries
  cannot change a published descriptor. Bodies retain the existing
  compact leaf/wide ABI. Cache capacity stays 8 MiB.
- Publication patches known static successors and incoming static
  edges (including taken and not-taken conditional exits). Only the
  owner CPU thread patches the writable alias outside native
  execution, then synchronizes both aliases. The range and existing
  B instruction are checked. No writable-executable mapping.
- One outer ABI frame per native group. The shared native helper
  validates the live block, reserves its worst-case cycle cost,
  invokes the original compact body, commits PC/PPC/PR/ops/cycles
  and chooses an actual patched B edge. No C++ calls between linked
  blocks. Up to 128 bodies per group and no timer-budget overrun.
  Dynamic targets still exit to C++; static calls update guest PR.
- A retired/disabled descriptor is marked inactive before incoming
  edges are unlinked. Even an obsolete branch cannot execute its
  body. Version checks reject old map pointers before any load;
  descriptors and executable bodies are not reused until reset.
  Mismatch, MMIO/SMC guard, short budget, missing edge and dynamic
  exit fall back to the original interpreter at the exact state.
- C-1 self-repeat blocks remain on their existing fast path.
  When no direct edge exists, short non-repeat blocks (<12 ops)
  remain compiled but interpret until they can be linked. The
  previous C++ chain remains available as a control.
- This is static direct linking through a shared native helper,
  not yet fixed cross-block guest GPR/SR residency or RAS. Each
  compact body still loads and flushes its own used registers.
  The helper still has native call/return and per-block accounting
  costs. Real game chain lengths and performance must be measured.

## A/B switches

- Default: V2, self-repeat, compact write audit and native static
  links enabled on supported native Linux ARM64 Libretro.
- FBNEO_CV1K_JIT_V2=0: interpreter control.
- FBNEO_CV1K_V2_LINK=0: C++ chaining with the new write watch.
- FBNEO_CV1K_V2_CHAIN=0: unchained control (no native links).
- FBNEO_CV1K_V2_VERIFY_ALWAYS=1: full live comparison at all entries.
- FBNEO_CV1K_V2_WRITE_AUDIT=0: no watch; full validation.
- FBNEO_CV1K_V2_SELF_LOOP=0: disable native self-repeat.
- FBNEO_CV1K_V2_LOOKUP_ONLY=1: learn/compile/validate, no execution.

New log fields:
CV1K V2 verify: checked, skipped, slices.
CV1K V2 links: groups, bodies, edges, patches, built, stops.
The existing calls/ops/cycles still count actual generated body calls
and guest work, not C++ entries. Approximate total outer invocations
= calls - linkBodies + linkGroups; groups include validation/budget
stops. Edge attempts can include a rejected target. The total code
bytes include generated 64-byte link stubs. Report identical 7320
frames, startup 480f, 6360/6480/6600f and the new counters.

## Verification

Initial ARM64 QEMU C2 harness: 192 differential slices covering
both conditional exits, delayed branch, BSR/RTS, wide and leaf,
MMIO guard and page boundary; mirrored native SMC, retire/unlink,
recompile/relink, stale map, restore, full/conditional verification
and external slice fence passed. Earlier C1 suite 168 slices passed
with compact write watch and native links enabled.
An initial standalone harness link was missing the real serial flash
implementation because tracked-I/O registration retains internal
DMAC handlers. Included the real serflash.cpp; link and tests passed.

Initial O2 Cortex-A55 QEMU 5M-cycle synthetic medians, 3 rotated
trials (unregistered fixture pages, so full native validation):
- Conditional three-block cycle: interpreter 189.549 ms,
  C++ chain 432.162 ms, native links 160.395 ms.
  Native bodies 1298435, outer groups 10240.
- Wide/leaf RAM cycle: interpreter 156.399 ms,
  C++ chain 193.105 ms, native links 73.527 ms.
  Native bodies 523841, outer groups 4216.
CPU and memory snapshots matched. Same binary with the new watch
map, not a timing comparison to old C-1 binaries. Host load is not
controlled. Do not extrapolate these timings to physical RK3566 FPS.

Final regression results (Cortex-A55 QEMU, O2 unless noted):
- 224 C-2 differential slices, including timer-triggered remap,
  native and interpreter mirrored SMC within a trusted slice,
  external slice fences and VERIFY_ALWAYS, passed.
- 168 C-1 repeat/chain differential slices passed.
- Flash: 20000 byte/state comparisons, 240 DMA diagnostic transfers,
  240 actual SH3 DMA comparisons and real mirrored-code NAND
  invalidation (bulk/scalar), passed.
- Poll: 1440 real slice comparisons and 24000 timer cases plus
  1608 expiry/restart cases, passed.
- Data: 1008 actual dispatcher comparisons, 21 unsafe-pattern checks,
  positive cache/live invalidation and 2268 full-literal exact
  slice/exit/overlap comparisons, passed.
- x86: 448 x 10 interpreter state cases, 65536 dense opcode keys,
  23136 x 8 pure-encoding cases, 392 independent arithmetic oracle
  cases and 100000 host arithmetic cases, passed.
- ARM64 O3 production SH3 object and standalone shared link, and
  CV1000 driver object compiled successfully. This is not a complete
  Libretro core or an RK3566 hardware test.

The initial B-2 regression run stopped at an assertion requiring a
4-op idle block to execute natively. C-2 deliberately interprets
short unlinked blocks. The B-2 standalone harness now explicitly
selects LINK=0, preserving those native execution assertions;
the C-2 harness separately checks the same six idle cases through
the new default interpreter fallback against the reference state.
Both final runs passed: six C-2 short-unlinked idle cases plus 224
C-2 linked slices, and the B-2 suite (16928 literal/MOVA/DT, 796
terminal/slot, DT/data guards, six idle cases and 232 integrated
slices with LINK=0).

Final same-binary O2 QEMU medians (three rotated 5M-cycle trials):

| Fixture | Interpreter (ms) | C++ chain (ms) | C-2 links (ms) | Linked bodies / groups |
| --- | ---: | ---: | ---: | ---: |
| Conditional cycle | 173.764 | 432.640 | 165.794 | 1298435 / 10240 |
| BSR/RTS | 183.545 | 413.409 | 267.657 | 835999 / 418032 |
| Wide/leaf RAM cycle | 163.222 | 197.385 | 77.754 | 523841 / 4216 |

All compared snapshots matched. Static cycles benefit substantially;
dynamic return-heavy code still loses to the interpreter. These
fixtures retain full opcode validation on unregistered pages. No
real RK3566 frame-time or full-core build claim is made here.
