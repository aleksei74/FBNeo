# CV1000 SH3 3.0C-4 global resident blocks (experimental)
Base e4d0ac227 (C3). Branch feature/cv1000-arm64-dynarec-v2-c4.
Stable low-spec/2.7 and C3 branches unchanged.

## Scope
- One pinned resident body per promoted virtual entry, up to 8192
  descriptors in the existing 8 MiB RW/RX executable cache. No 512
  region copies or 12-member boundary. Directory/set/byte limits remain.
  Static B slots initially branch to a local cold exit and are patched
  only outside native execution. Dynamic JMP/JSR/RTS fallback uses an
  exact-tag bounded indirect map; collisions miss safely. RAS uses local
  guarded continuation stubs. No host address is serialized.
- Preserve C3 register residency and C1 native self-repeat where its
  original instruction subset applies. C4 global candidates include
  PR stack and register transfers, displacement and R0 indexed MOV.B/W/L
  forms, GBR byte/word/long moves, SHAD/SHLD, MUL.L/MULS.W/MULU.W,
  MACL transfers. MUL.L extra cycle included in prefix and whole-block
  budget, even on partial exits.
- Callback-free direct memory slots are preflighted before branch effects;
  PR-access delay slots and complex/special kernel edges still interpret.
  Watch-address/misalignment/MMIO/P4 guards preserve original handlers.
  DT lookahead removed only when next opcode is part of the exact guarded
  snapshot, is not 8bfd, and DATA/FETCH backing matches at publication.
- Bounded static frontier + actual cold successor eager promotion, up to
  four attempts per CPU slice. No unbounded recursive compile.
- Per-body token + live page generation gate; registered RAM/ROM and
  unknown backing have distinct tokens. Unknown backing compares live
  bytes on every outer native invocation, regardless of the frame scope.
  Map changes retire old bodies even if bytes match. Watched or ambiguous
  native stores exit BEFORE write; original mapped/handler/DMA write
  barriers remain. Retired descriptors are pinned and invalidated before
  incoming B slots are unpatched. All RAS/dynamic entries hit the same gate.
- Explicit synchronous CV1000 CPU-loop frame scope (240 slices). Only
  registered backing may retain a verified token across those slices;
  tracked generation and opaque/map checks still apply. Outside this
  scope the slice fence remains. Assumes frontend raw memory changes
  occur outside the synchronous driver CPU loop (concurrent raw writes
  inside the loop are not supported). Scope can be disabled for strict A/B.
- RAS slots not zeroed on entry; depth starts at zero. Low-profit roots
  sample 32 native groups; mean <8 guest ops masks the entry for 480
  slices, while inbound native links stay valid. Cooldown queue restores
  exact bitmap bits; aliases keep their other live entries. Threshold
  20 was too aggressive in short-chain trials, so 8 is a cautious
  experimental default, not a measured universal optimum.
- No guest clock, DIP, IRQ/timer cadence, graphics or save-state format
  changes. Do not infer game FPS from the synthetic benchmark.

## Controls (normal launch uses C4 on supported Linux ARM64)
- FBNEO_CV1K_JIT_V2=0: optimized interpreter (2.7-style).
- FBNEO_CV1K_V2_REPEAT_ONLY=1: proven compact self-repeat only.
- FBNEO_CV1K_V2_GLOBAL=0: C3-style resident-region comparison.
- FBNEO_CV1K_V2_RESIDENT=0: C2-style control.
- FBNEO_CV1K_V2_FRAME_FENCE=0: conservative CPU-slice validation.
- FBNEO_CV1K_V2_GLOBAL_MIN_OPS=0: disable adaptive root cooldown.
- FBNEO_CV1K_V2_GLOBAL_MIN_OPS=8: default, valid range 0-128.
C3-style controls use the updated shared trampoline and are not
byte-identical to the original C3 commit. Use that commit for exact A/B.

## Diagnostics
CV1K V4 global: native groups, completed blocks, actual guest ops,
published bodies, patched static B slots, comparisons, invalid bodies,
eager promotions, full limits, compile-time DT lookahead elisions.
CV1K V4 exits: validation-fence, missing-link/indirect cold, budget,
memory/other guard, skipped (alias-visible cooldown), cooldown events,
bitmap mask and reenable events. Exit counts are per native invocation.
They are not necessarily distinct guest instructions. Specific RAS
hit/miss counters are not included in this revision.

## Reproduction
aarch64-linux-gnu-g++ -w -O2 -std=gnu++11 -D__LIBRETRO__ -DLSB_FIRST -DUSE_SPEEDHACKS -mcpu=cortex-a55 -ffunction-sections -fdata-sections -Wl,--gc-sections -pthread -Isrc/burn -Isrc/burner -Isrc/burner/libretro -Isrc/cpu -Isrc/burn/devices -Isrc/intf -Isrc/burner/libretro/libretro-common/include tools/test_sh3_v2_c4.cpp -o /tmp/cv1k-c4-test
qemu-aarch64 -cpu cortex-a55 -L /usr/aarch64-linux-gnu /tmp/cv1k-c4-test
qemu-aarch64 -cpu cortex-a55 -L /usr/aarch64-linux-gnu /tmp/cv1k-c4-test bench
qemu-aarch64 -cpu cortex-a55 -L /usr/aarch64-linux-gnu /tmp/cv1k-c4-test short

## Real hardware gate
Build only this experiment branch for RK3566. Compare the same mmpork
7320 frames, first 480f and high-load 6241-6600f, matching POLL/DATA/TILE.
Log V4 groups/blocks/ops, cold/guard/fence, masked roots, cache/full,
audio silence and late frames. Repeat-only and strict slice modes are
available if hardware coverage or timing regresses. Physical RK3566
speed and full Libretro core integration remain unverified.

## Final verification (2026-11-28)
- C4 Tier2: 3122 encodings x 3 differential states (9366 cases).
- 352 generated execution slice comparisons, including nested BSR/JSR,
  stack PR, memory delay slot, dynamic destination change, page boundary,
  MMIO, timer/remap/IRQ, 600 distinct global bodies.
- 192 trusted short-chain slice comparisons across raw C4, forced
  cooldown20, repeat-only, and default cooldown8. All states match.
- Mirrored watched store exits before write; invalidation/unlink/restore.
  Scoped frame raw-write boundary and tracked write inside scope pass.
- C3 regression suite on the updated shared emitter: 320 slices and
  scratch/SMC/restore tests pass.
- ARM64 production -O3 -mcpu=cortex-a55 -fPIC SH3 and d_cv1k objects
  compiled; standalone SH3 shared link and x86 syntax pass.
  Full Libretro link and C4 hardware/DMA stress remain unverified.

Final QEMU Cortex-A55 5M guest-cycle medians, rotated 3 trials (ms).
Host load varied significantly; compare direction, not predicted hardware FPS.

| Fixture (unregistered test code) | Interpreter | C3-style | Raw C4 |
|---|---:|---:|---:|
| Conditional | 340.152 | 20.889 | 21.116 |
| Leaf BSR/RTS | 522.537 | 39.275 | 70.645 |
| Wide/leaf RAM | 214.740 | 30.283 | 26.774 |
| Nested BSR + PR stack | 460.364 | 577.623 | 58.694 |
| Nested JSR + PR stack | 536.949 | 618.147 | 136.065 |
| Short 3-body exits | 473.712 | 433.438 | 731.431 |

Registered ROM, scoped frame (600 x 8333 cycles), short 3-body exit:
Interpreter 325.989, C3-style 179.153, raw C4 250.710, forced
cooldown20 314.474, default cooldown8 158.396 ms. Raw C4 checks 15,
default8 checks 14, 263240 native invocations/789072 bodies.
All benchmark end states matched.
Short unregistered path still regresses; default8 is not a universal
performance guarantee. Leaf BSR/RTS has a cost from global entry gates.
Frame-scope and broader real-code chaining must be measured on device.
