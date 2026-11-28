# CV1000 Dynarec V2 B-3 shadow barrier + C-1 guarded chaining

Base: 0de09dda (B-2), experimental V2 branch only. Real RK3566
mmpork, retroarch30b2-2.log, first 7320 frames: CPU 15.450 ms vs
stage 2.7 10.452 ms; high-load 360 frames CPU 28.334 vs 18.967 ms,
core 28.772 vs 19.425 ms. Native calls 251382202, ops 1236203016,
cycles 1449256513, built 1655, code 579264 bytes. High-load native
calls 22489190, ops 112564692, cycles 128086035. B-2 coverage
increased but per-block invocation overhead remained. POLL/DATA/FULL
counters matched; this is not a proof of all guest state. Same-binary
hardware A/B and controlled clock/temperature are still missing.

## B-3 shadow barrier (validation retained)

Compiled spans register their 64 KiB fetch backing page by HOST
address. Every direct write-map alias receives a pointer to the same
64-bit generation; partially overlapping/ambiguous maps increment a
global opaque epoch. Map APIs rebuild the watch mappings. Cross-page
and opaque handler writes conservatively increment the global epoch.
On native entry compare generations and the live fetch pointer/bytes.
Changed blocks retire before executing; unchanged dirty blocks refresh
their stamps. No generated code is reclaimed during a call. Reset and
ACB_WRITE restore clear the directory, code cache and watch records.

Audited direct write paths:

- WB/WW/WL (including cheat_write and ordinary DMAC transfers).
- Cv1kTryDirectMove and Cv1kTryStack; delay-slot handlers use the same
  paths. Fast region shortcuts are exclusive with V2.
- Counted interpreter copy/fill, Cv1kTryDataLoop, full literal kernel:
  one guest-page notification after range/overlap guards, before copy.
- Serial-flash DMAC serflash_io_read_words and scalar direct byte
  stores in sh4dmac.inc. No assumption that DMA uses WB.
- Compact generated stores: use live write-page generation pointer,
  increment after the store. Legacy V1 fallback store spans mark the
  opaque epoch. Existing host-span self-modifying-code guard retained.
- d_cv1k AllRam reset and restore use existing Sh3Reset/Sh3Scan hooks.

This is deliberately a **shadow audit**, not permission to skip
memcmp on arbitrary RAM. Untracked direct host mutations still retire
the old block by live byte validation. An opaque epoch can mask the
source of a concurrent dirty event; untracked=0 does not prove all
external writers are hooked. The direct-link stage must handle all
incoming links, generation lifetimes and host aliases. DT data-map
lookahead remains live; no literal or DT folding on mutable RAM.

## C-1 execution

- After a normal native exit, run up to 64 successive cached blocks
  within the existing CPU/timer slice. Stop at misses, guard/short
  budget, pending IRQ/delay/endrun or CPU-off. Do not immediately retry
  a failed instruction. No new compiler call inside a chain.
- For static taken branches targeting their own exact virtual entry,
  retain the compact emitter's register bindings and SR in the native
  loop. A saved x29 counts the iteration cap; each call reserves the
  worst-case complete branch+slot cost, cap 4096 laps. Calls/indirect
  branches are not self-repeat candidates. Live direct reads/stores
  retain all previous guards; in particular every store must not
  overlap the executing host code span, including mirrors. No guest
  callbacks during a repeated lap. The last completed lap, non-taken
  exit and a partial guard after earlier completed laps have distinct
  exact instruction/cycle/PC/PPC accounting. Existing kernel-edge and
  DT/BF busy-loop exclusions remain.
- No B-rel26 patching, RAS, cross-distinct-block register residency,
  change of guest clock, timer scheduling or save format. Self loops
  compare their span once per native call, not every lap; distinct
  blocks still individually validate their live code.

## A/B switches (dedicated experimental branch)

Default: V2, compact, basic blocks, write audit, chain and self loops
enabled on supported native Linux ARM64 Libretro. Ordinary ES launch.
FBNEO_CV1K_JIT_V2=0: interpreter control.
FBNEO_CV1K_V2_CHAIN=0: unchained B-2 scope with shadow write audit.
FBNEO_CV1K_V2_SELF_LOOP=0: C++ chaining without native loop repetition.
FBNEO_CV1K_V2_WRITE_AUDIT=0: disable shadow tracking, retain memcmp.
FBNEO_CV1K_V2_LOOKUP_ONLY=1: compile/validate, no native execution.
No same-binary mode is claimed byte-identical to old code generation.

New cumulative stats:
CV1K V2 audit: pages, dirty (dirty entry validations), untracked
(changed blocks with no observed generation/opaque change), opaque.
CV1K V2 chain: groups, joined (additional C++-dispatched blocks),
selflaps (completed taken laps beyond the final returned block),
selfbuilt (self-loop compiled blocks).
The existing calls field counts actual machine-code invocations;
terminal exits/taken now include repeated guest exits, so can exceed
calls. Ops/cycles include all completed laps and partial exits.

## Test plan

test_sh3_v2_c1.cpp: exact interpreter/native snapshots across pure,
conditional/DT, delayed, MMIO after repeated laps, distinct chained
blocks, and self-modifying store through a mirror. Watch tests cover
byte/cheat/word/long/direct/stack/generated store and intentional
untracked mutation, plus restore. test_cv1k_flash.cpp executes actual
NAND bulk and scalar DMA into a mirrored compiled code page. Run the
existing B-2 literal/terminal/slot and integrated regression suite,
poll/timer/data loops, non-ARM64 interpreter and production ARM64 PIC
object/link. Synthetic benchmarks are not RK3566 FPS measurements.

## Results

Final ARM64 Cortex-A55 QEMU: 168 C-1 differential slices, including
native repeated stores followed by a guard on a mirrored self-code
write, and retirement on the subsequent entry. Native write-watch
fixtures (WB, cheat, WW, WL, direct, stack, compact, untracked host)
and restore passed. 16928 literal/MOVA/DT encoding cases, 796 terminal
cases, 64 basic-loop + 168 integrated slices passed. Final ARM64
production O3 SH3 object and standalone shared link passed.
Final polling 1440 slice comparisons, timer 24000 differential cases,
1008 data-loop and 2268 full literal comparisons passed. Earlier
revision (before enabling repeated native stores, same shadow barrier)
passed real mirrored NAND bulk/scalar invalidation, 20000 flash
byte/state and 240 DMA comparisons, and x86 interpreter 448 x 10.
Saved field names and order unchanged. No full Libretro link or
physical RK3566 timing/long-run test performed.

Synthetic 5M guest-cycle benchmarks, three rotated trials, QEMU
Cortex-A55; host load not controlled. Same-binary interpreter,
unchained B-2 scope with write audit, and C-1:
- Two-op body + BRA/slot: 450.428, 642.398, 3.616 ms.
- Literal/DT/branch/store loop: 498.018, 575.299, 97.235 ms.
Corresponding CPU/memory/guest-cycle snapshots matched. Timings are not hardware FPS predictions.
