# Stage 2.7: hot state, shared slow dispatch and idle-only memory

The stage 2.6 RK3566 mmpork run (7320 frames) regressed from 11.215 to
11.522 ms mean CPU; the worst 360-frame core mean rose 21.155 to
21.627 ms. Frequency/temperature were not controlled. The measured
regression motivates separating idle-page rescue from full-region
shortcuts, not changing clocks or speedhack accuracy.

- FBNEO_CV1K_FAST_RAM defaults to 0. Explicit 1 retains the stage 2.6
  validated RAM/ROM region path. The default Flow dispatcher is a
  compile-time specialization without per-access full-region guards.
- FBNEO_CV1K_IDLE_BATCH defaults to 1 on native little-endian ARM64
  Linux libretro (other platforms opt in). The cold long-handler
  exception checks the live map, registered RAM/handler identity,
  alignment and exact idle address. Other accesses retain the
  ordinary stage 2.5 map lookup; exact idle reads retain cycle burns.
- Sh3HotState groups hot integer state under one 64-byte-aligned base.
  References preserve original handler names and individual scan fields.
  Stack-transfer template parameters use pointers to state members.
  The 104 SCAN_VAR field names/order are identical to the prior commit;
  field types and sizes are unchanged. No raw struct serialization.
- Flow's slow handlers and branch fallback share a single published
  state/decoded-handler tail; its pure paths share one completion tail.
  Preserve BF probes, IRQ/delay checks, callback visibility and cycles.
- The batched opcode temporary is now UINT32, preserving the zero
  extended 16-bit fetch through the compiler barrier.

Cortex-A55 production O3 disassembly, same local compiler and flags:
active default Flow function 0x4cb4 (19,636 B) -> 0x33dc (13,276 B),
32.39% smaller. The former two and #0xffff instructions in fast_next
are absent. The two dispatch-base/table stack reloads still remain
(new [sp,#104] and [sp,#120]); no claim of eliminating all spills or
branch misses. Opt-in region variants add code/tables to the total
binary, although the active default function shrinks.

Verified:
- x86: 448 x 10-mode interpreter scenarios, 23136 x 8 pure encodings,
  65536 decoder keys, arithmetic independent oracles.
- ARM64 Cortex-A55/QEMU: 1430 branch, 210 stack, 360 DT/BRA and
  23824 fused-slot encodings.
- ARM64: 256 pages per RAM type and 260 slice scenarios comparing
  both full-region and idle-only paths, live handler replacement,
  exact idle burn, diagnostic counts/window reset.
- ARM64: 1440 poll slices, 24000 timer cases, 1008 data loop and
  2268 full-literal comparisons, all previous native/JIT hybrid
  differential tests (including 20000 idle-page and 20000 DT/BF).
- Production ARM64 SH3 objects compile (old and new); all tests above
  returned zero. Full libretro link, hardware FPS and savestate
  file round-trip on the actual console remain unverified.
No reliable new timing claim: the host was heavily delayed during
these builds. Compare normal builds in the same ES environment,
first 7320 frames and especially windows ending 6360/6480/6600.
Event scheduling, lazy total-cycle accounting, and remaining spills
are separate future work. No timer/IRQ ordering or guest clock change.
