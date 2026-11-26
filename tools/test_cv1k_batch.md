# RK3566 follow-up: batched SH3 dispatch and hybrid ARM64 spans

Base: 99cddb89b. The real akatana log still had 1931 frames above
16.67 ms out of 11760 profiled, 50.101 ms worst frame, and 13 periods
of ALSA silence. The two runs were not controlled gameplay benchmarks.

The default Linux AArch64 CV1000 timerhack path now keeps the PC and
remaining cycle budget in host registers while executing audited
single-cycle register/flag operations. Each opcode is fetched live;
before a memory/branch/device/other handler or slice exit, publish PC,
PPC, consumed cycles and remaining budget exactly as the existing core.
Delay slots and pending interrupts use the original single-step
handler path. No guest clock, timer granularity, DIP or save format
change. FBNEO_CV1K_BATCH=0 restores compact dispatch.

FBNEO_CV1K_JIT=2 is a new **opt-in** hybrid: use the batched interpreter
and attempt native spans at control-flow/run boundaries only, not on
every instruction. Keep the existing full opcode comparison (guest
RAM has aliases, direct NAND DMA and save/cheat writes; page-epoch
validation would need all these invalidation paths audited). Short
native spans, guard exits, and cache pressure retain existing behavior.
JIT=1 retains the older experimental per-instruction check. JIT=0 is
still the default. Add native MOVT, XTRCT, SWAP.B, CMP/PZ, CMP/PL and
single-bit shifts, using exact interpreter semantics and T bit.

Cortex-A55 QEMU regression with GCC -O3:
- 20435 native instruction encodings, 4096 mixed spans.
- 23136 pure dispatch encodings, four random register states each.
- Switch/flat/compact/batch/JIT/hybrid state/cycle comparison,
  including observed MMIO callback PC and cycle values.
- 64 mixed branch/store programs for each dispatcher, 64 pure
  programs crossing fetch pages with live opcode rewrite.
- Real IRQ request from a memory callback, including a delay slot.
- 100000 timer prescaler, 20000 direct idle-page, 20000 DT/BF cases,
  cache saturation, long-only and handler-replacement checks.
- Cortex-A55 and x86-64 sh4.cpp objects compiled with GCC -O3.

QEMU is functional validation, not a host performance benchmark.
Full core build, sustained real-device speed and audio continuity
remain unverified.
