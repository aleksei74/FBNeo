# Stage 2.7B: PC-relative AArch64 dispatch

Stage 2.7 improved the RK3566 mmpork high-load demo core mean from
21.627 to 19.425 ms (360 frames), but the inspected Cortex-A55 O3
fast_next sequence still reloaded the table and label base from the
stack. Remove these loads without changing guest instruction semantics.

The signed 16-bit relative offset table retains all 65,536 opcode
entries. Its new specialization has 4096-byte alignment, allowing
ADRP alone to reconstruct the local table pointer. ADR reconstructs
the same batch_base label used to generate the offsets. The combined
assembly produces the target with one compiler-visible output and
an explicitly clobbered caller-saved x16 scratch. Its whole-table
memory input models the read from the initialized table, not guest
RAM. No global memory clobber or fixed register ABI is introduced.
The final indirect jump is still a GNU C++ computed goto to the
original labels. Check table alignment and signed offset range at
initialization, as with the existing dispatcher range fallback.

Multiple code-generation candidates were compared: table-only ADRP,
label-only ADR, dual-output ADRP/ADR, and combined single-output
lookup. The initial dual-output path eliminated the two address loads
but introduced an opcode store; the final combined path eliminated
that spill in the inspected dispatch sequences as well. This is not
a claim that every cold handler or the entire CPU function is spill-free.

The new default applies to the native little-endian Linux ARM64 GCC
libretro Flow page-map dispatcher, with the original C variant retained
for A/B. Clang and other targets use the C path. The optional full
RAM/ROM region variant also uses C dispatch. Override the default with
FBNEO_CV1K_PCREL_DISPATCH=0 (same build, other settings unchanged).
The active table is still 128 KiB, but retaining the C specialization
adds one additional 128 KiB table and another function to the binary.
No timer schedule, clock, savestate, DMA, mapping, or DIP changes.

## Test selection

On supported ARM64, interpreter harness mode 9 selects PC-relative
dispatch and mode 10 selects its stage 2.7 C control. The 448 scenario
suite checks actual entry into the new path and compares both modes
with the legacy switch. Pure encoding tests also exercise the new
path. The branch/stack, polling/timer, data-loop, and memory fixtures
using mode 9 cover the new dispatcher, except explicitly opted-in
full-region tests. `test_sh3_flow pcrel` benchmarks modes 10 and 9
with the same guest loops and alternating order (synthetic host/QEMU
timing, not hardware FPS). Verify the production -O3 -mcpu=cortex-a55
-fPIC object and shared link separately from the instrumented harness.

Use the ordinary non-OPSTAT build on the device. Compare the same ES
launch, input/demo, power and clock settings, especially the windows
ending at frames 6360, 6480 and 6600. The startup log identifies the
PC-relative option; full region shortcuts must remain disabled for it
to select the intended path. PMU and hardware FPS still require a real
device measurement.

## Final verification

- GCC 13.3.0 ARM64 Cortex-A55 production O3 PIC object and standalone
  shared link passed. The active function is 0x3100 (12,544 bytes).
  All four indirect dispatch sites contain ADRP, LDRSH, ADR, ADD, BR
  without stack accesses in the lookup sequence. Compared with the
  stage 2.7 object (0x33dc, 13,276 bytes), active code is 5.5% smaller.
- Final ARM64 QEMU: 448 scenarios x 11 modes; 23,136 pure encodings x 9
  modes; all 65,536 decoder keys; arithmetic oracles.
- Final ARM64 QEMU: 1,430 branch, 210 stack, 360 DT/BRA, 23,824 fused
  slot encodings; 260 memory scenarios x 2 paths, region/remap checks,
  diagnostic window reset; 1,440 poll, 24,000 timer, 1,008 data loop,
  2,268 full literal cases. All completed with zero exit status.
- x86 448 x 10-mode and 23,136 x 8-mode suites passed before the final
  ARM64-only assembly refinement. The final asm is excluded on x86.
- Full libretro link and RK3566 real-game performance remain unverified.

Final synthetic AArch64/QEMU 10M guest cycles, three alternating runs,
median milliseconds (test-only counters, host load not controlled):
- RAM/branch: 1081.474 -> 816.924 ms (24.5% lower).\n- stack/call/return: 1256.312 -> 988.524 ms (21.3% lower).\n- DT/conditional branch: 911.025 -> 665.396 ms (27.0% lower).\nAll corresponding checksums and path counts matched. These are not\nreal-device FPS predictions.\n