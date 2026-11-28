# CV1000 SH3 branch/stack batching (stage 2.5)

Keep stage 2's live fetch-page and direct mapped MOV paths. Batch audited
one-cycle special-register transfers, untaken conditional branches, six
PR/MACH/MACL stack transfer forms, and ordinary DT instructions.
DT must read the data map for its original lookahead; busy-loop matches,
unaligned accesses and handlers retain the original path.

Taken branches may commit the preceding sequential instructions and their
branch cycles together. A delayed branch may execute a live, audited
one-cycle slot if the original slice has enough cycles. Common ADDI,
NOP, MOVI, MOV and EXTS.B slots use the short inline path. Other pure
or guarded direct-memory slots use the existing decoder identities.
Keep branch-specific EA/PR/PPC effects, target-relative PC operands,
guest endian order, IRQ checks, self-modification, delay sentinel and
budget overshoot. Preserve original slot and BF data/count-loop probes,
BRA-self/DT busy-loop burns, and MMIO callbacks by rejecting unsupported
or potentially observable cases before modifying guest state.
Same-page target fetch reuse requires an unchanged fetch mapping epoch.

Native little-endian Linux ARM64 libretro defaults to
FBNEO_CV1K_FLOW=1 when DIRECT_MEM and SLOT_DISPATCH are enabled.
Explicit FLOW=0 selects the stage 2 dispatcher. Existing experimental
8 KiB dispatch and JIT remain off. Other platforms retain opt-in.
No clock, DIP, GPU, display, timer scheduling, or savestate format changes.

## Verification

Final x86 and AArch64 Cortex-A55/QEMU builds:
- 1430 branch/slice comparisons, including all eleven branch forms,
  short budgets, MMIO callbacks, cross-page slots and live code changes.
- 210 stack comparisons and 360 DT/BRA data-map/busy-loop comparisons.
- 23824 audited one-cycle slot encodings, including special registers.
- AArch64: all 896 MOV scenarios compare both stage 2 and stage 2.5
  against switch interpretation; 23136 original pure slot encodings.
- Production AArch64 O3 sh4.cpp compiles; new dispatcher fits the
  existing relative label-offset range.
Before the final common-slot inline refinement, mode 9 also passed
the complete x86/AArch64 448 x 10-mode interpreter differential suite,
23136 pure encodings x 8 modes, arithmetic checks, and AArch64
1440 polling, 24000 timer, 1008 data-loop and 2268 full-literal
comparisons. Final targeted tests exercise the changed branch/slot path.

## Synthetic comparison

Final AArch64 QEMU 10M guest-cycle runs, median of three alternating
baseline/new tests (test-only counters enabled; host load uncontrolled):
RAM/branch loop 590.812 -> 425.531 ms;
stack/call/return loop 650.653 -> 483.545 ms;
DT/conditional-branch loop 412.119 -> 314.979 ms.
The first generic implementation regressed the RAM/branch workload;
the final common-slot inline path removed that regression in this run.
These are not hardware FPS predictions. Full core link, exact opcode
frequency on the RK3566, and real-game correctness/performance remain
to be verified.
