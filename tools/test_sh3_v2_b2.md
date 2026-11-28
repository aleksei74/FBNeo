# CV1000 Dynarec V2 stage 3.0B-2

Base 547585a5 (B-1). Dedicated experimental branch
feature/cv1000-arm64-dynarec-v2; low-spec stable branch unchanged.
RK3566 mmpork B-1: first 7320f mean CPU 12.121 ms vs B-1 predecessor
A-fix 12.291 ms and stage 2.7 10.452 ms; worst 360f 21.937 ms CPU,
22.378 ms core (44.7 equivalent FPS). 53 blocks, 58704 aligned cache
bytes, 125956800 native ops, no changed/guard/full; 50 compact builds.
No controlled same-binary hardware A/B or measured probe latency.

## Scope

- Keep the B-1 compact leaf/wide register bindings. Add live
  PC-relative MOV.W, MOV.L, MOVA and DT to the candidate set.
  Addresses use the virtual instruction PC (+4, aligned for long/MOVA);
  values are read at runtime from the DATA map. No literal folding.
  DT checks the next DATA word before decrementing, falling back for
  handlers or 8bfd (original busy-loop burn).
- Terminal BF/BT/BF.S/BT.S/BRA/BSR/JMP/JSR/RTS, including short
  two-op branch+slot blocks. Other nonterminal extended spans need
  four ops. BRAF/BSRF, unsupported ops, memory/PC-relative/DT delay
  slots and CMP/PZ literal hook remain interpreted. BRA self-burn,
  known data/counted-loop probe edges, missing slots and zero delay
  sentinel are excluded. Odd indirect targets guard before branch
  effects. No branch is entered from an already pending delay/IRQ.
- Terminal branch condition/target and supported register-only slot
  are generated in the block. Capture target before slot mutation.
  Distinct return flags identify taken vs not taken; C++ commits
  exact PC/PPC/PR and extra cycles. A not-taken delayed conditional
  returns before its sequential slot. Reserve the maximum branch/slot
  budget; original handlers execute tight tails and pending slots.
  Native-to-native direct links and cross-block register residency
  are not implemented.
- Long-load map handler rescue is out of line after RET, leaving the
  ordinary mapped read on the fall-through path. V2 Memory adds
  idlePage, idleAddress, PR at offsets 48/56/60; original V1 ABI
  prefix unchanged. Before each eligible compact call validate the
  live registered idle read map/handler and option. Only aligned
  non-exact first-page words may bypass; the exact idle address,
  replacement handler, P4 and other devices retain original handling.
- Replace the sticky 16-bit Bloom mask with a dynamically allocated
  4096-byte exact halfword-entry bitmap per active physical page.
  Pointer array is 64 KiB. Missing page/bit avoids directory lookup;
  retirement keeps bits sticky to avoid alias false negatives.
  Pack set slots after a live-code retirement, moving the metadata
  and maintaining slot-index IDs. First empty way ends lookup.
  Full cache/sets still cannot evict published blocks. New B-2
  time-stratified learning can sample forward as well as backward
  committed edges, still at most one threshold sample per slice
  plus low-rate slice-entry sampling.
- Normal exits fall through to the common epilogue; not-taken and
  guard stubs follow RET. Existing code fetch-pointer plus whole-span
  memcmp is mandatory, with host-alias store overlap guard and
  reset/ACB_WRITE invalidation retained. No page-write barrier,
  timer/IRQ schedule, DIP, clock, or savestate field changes.
  8 MiB RW/RX immutable cache retained.

## Runtime options and diagnostics

Dedicated V2 branch defaults to B-2 on supported native Linux ARM64
libretro, for normal ES launch. Same-binary comparison:
  FBNEO_CV1K_JIT_V2=0 : stage 2.7 interpreter
  FBNEO_CV1K_JIT_V2=1 FBNEO_CV1K_V2_BLOCKS=0 : B-1 candidate scope
  FBNEO_CV1K_JIT_V2=1 FBNEO_CV1K_V2_BLOCKS=1 : B-2 basic blocks
  FBNEO_CV1K_V2_LOOKUP_ONLY=1 : learn/compile/validate without native execution
  FBNEO_CV1K_V2_COMPACT=0 : legacy emitter (also disables B-2)
B-1-scope uses the new directory and epilogue, not byte-identical B-1.
CV1K V2 blocks reports cumulative extended builds, terminal builds,
completed terminal exits, taken/not-taken exits, and actual native
guest cycles including branch extras. Ops remain guest instructions.
Compare 7320f, initial 480f, high-load 6360/6480/6600f, same ES/power/
temperature/clock and matching poll/data/full counters. Native guest
cycle share does not equal host wall-clock share. No claim of 50%
coverage or speedup in real mmpork yet.

## Verification

Final Cortex-A55 QEMU test_sh3_v2_b2:
- 16928 live literal/MOVA/DT cases, separate DATA/FETCH maps and
  mutable literal values, address alignments and aliases.
- 796 terminal/slot differential cases against the existing CPU,
  leaf/wide bindings, taken/not taken, target register overwritten
  by slot, odd-target and max-budget guards, plus zero-prefix guard.
- DT DATA busy/MMIO checks, preserved special edges/slots.
- Idle neighbor/exact/mirror/PC-relative literal/replaced handler.
- 64 basic-loop slices (kernels on/off) and 168 existing scenarios
  with actual B2 execution, live code changes/remap/SMC/MMIO/IRQ/timer.
  Initial direct terminal fixture omitted slice-end timer processing;
  fixture corrected and rerun, no production state change required.
- Final ARM64 production -O3 -mcpu=cortex-a55 -fPIC SH3 object and
  standalone shared link passed.
Before the final out-of-line idle rescue refinement, existing
B-1/legacy integrated suites passed (168 slices each), collision
compaction and cache lifecycle; 26067 encodings, 38400 guards,
4096 mixed spans, 2100 store overlap tests. ARM64 and x86 interpreter
448 x 11/10 modes, 23136 x 9/8 pure encodings, 65536 decoder keys,
392 signed oracle, 100000 arithmetic cases; 1430 flow branches,
210 stack, 360 DT/BRA, 23824 fused slots; 1440 poll, 24000 timers,
1008 data loops, 2268 full literal passed. CV1000 driver object passed.
The final B-2 execution suite reran after idle-stub relocation.
Full Libretro core link and physical RK3566 performance remain untested.

Synthetic 5M timed guest cycles after warmup, same final executable,
Cortex-A55 QEMU, medians of three rotating trials:
- Literal/DT/branch loop: control 179.534, B-1 scope 198.128,
  B-2 176.612 ms. B-2 executes 1867758 native ops / 2490134 cycles
  including warmup; baseline and B-1 scope compile zero blocks.
- Two-op body plus BRA/slot loop: control 175.886, B-1 scope 174.262,
  B-2 194.590 ms. B-2 executes 2007652 native ops / 2509565 cycles
  including warmup. Very short blocks still pay C++ invocation cost.
All corresponding full snapshots and guest cycles matched.
These are synthetic timings with uncontrolled host load, not
hardware FPS predictions. Next: complete write-path audit before
removing memcmp; then guarded direct links and cross-block residency.
