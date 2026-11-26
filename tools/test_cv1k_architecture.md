# CV1000 architecture follow-up (RK3566)

Base 2dc241c76. Real mmpork JIT=2 log: 166747881 guest native ops
over 14048240 native invocations (11.87 ops/call); no cache exhaustion.
Initial CPU windows 42.944/54.716/56.212/58.450 ms vs JIT=0
29.569/30.904/32.393/32.267 ms. Full sessions have different gameplay
and duration. Precise cross-lane ranges were zero in both sessions.

## Experimental closed-loop JIT (Astra-assisted)
FBNEO_CV1K_JIT=3 enables the batched interpreter plus direct native
closed-loop translation. Supports BT/BF/BT.S/BF.S/BRA back edges to the
same validated entry with at most seven audited guest registers and
a pure supported delay slot where applicable. Registers and SR stay
resident across the native back edge. Taken/fallthrough cycle costs,
PC/PPC/EA, guard exits and remaining slice budget retain interpreter
semantics. Guarded RAM reads/stores preserve the existing live mapping
and host code interval checks. Unsupported control flow, larger register
footprints, cross-page regions, pending IRQ/delay and short budgets fall
back. Non-loop spans need at least 16 instructions (rather than 8).
Tagged front cache skips associative lookup only, never live-code
validation; no page-epoch invalidation shortcut. Not a general
cross-block linker or arbitrary SH3 control-flow recompiler.
JIT=0 remains default; JIT=1/2 remain available for A/B.
CV1K LOOP logs attempts/hits/guest native ops executed in closed loops.

gpt-6-astra developed CPU code in a detached worktree and ran tests;
session expired before its commit. Integration agent reviewed and
merged the patch and re-ran tests in the combined source.

## Experimental RGB555 packed VRAM
FBNEO_CV1K_VRAM16=1 on little-endian AArch64 libretro uses 64 MiB
rather than 128 MiB of VRAM. RGB555 + opacity flag stored losslessly;
uploads directly copy words, simple draws and blend/tint use packed
scalar or 8-lane NEON, output expands only visible pixels. Preserves
legacy quirky red base in destination-square modes, transparency,
overlap order, clipping and delay. Legacy expanded path remains default.
Libretro scan callbacks use the original expanded byte stream in 8192
pixel chunks, with no full-sized scratch buffer. Byte-stream format
tested in both directions; actual frontend save/load and compatibility
with old states need real-device verification. Standalone keeps expanded
VRAM. The normal 16bpp display palette LUT is reduced from 2^24 to 2^15
entries (only RGB555 colors are valid).

## Experimental command-level tile scheduling
FBNEO_CV1K_BLIT_TILES=1 enables alternative scheduling of complete draws
on two workers. Broad-phase 32x32 tile maps plus exact rectangles detect
cross-worker RAW/WAR/WAW hazards; each lane keeps original command/pixel
order, including self-overlap. A phase barrier is inserted when neither
lane can accept the next draw, or on an upload/unsafe geometry. The
entire list is preflighted, with 8192-command/64-phase/2048-rect-set and
comparison bounds. Unbalanced or small phases run serial. Existing
row-band and segmented paths retained; tile scheduling may recover
lists that row slicing cannot handle. Not a general GPU tiled rasterizer
or a guarantee of speedup. CV1K TILE reports successful jobs, phases and
draws in tile phases; counted once in CV1K PARALLEL split.

## Verification
Cortex-A55 QEMU, GCC -O3:
- Existing SH3 26067 encodings, 4096 mixed spans, 1050 store guards,
  timer prescaler/IRQ/SMC/handler/cache regression suite.
- 281 translated closed-loop branch comparisons and 200 slice/timer/
  live-code rewrite comparisons; RAM/MMIO/alignment/self-modification
  guards and pending IRQ/delay tested.
- 8192 legacy scalar vs NEON cases (512 blend variants), and each of
  packed scalar and packed NEON vs legacy scalar, including overlap.
- 36 original row lists, 41 segmented lists (including 4 packed mixed
  upload/parallel cases), 48 precise-lane lists, 32 expanded and 8 packed
  tile-scheduled lists, barrier and malformed/unprofitable fallback.
- All 65536 RGB555/flag values round-trip; byte-identical expanded
  libretro VRAM stream, chunked restore including short last chunk.
- SH3, EPIC12 and CV1000 driver ARM64 Cortex-A55 and x86-64 object
  compilation.
Native-port architecture references: Spelunkomp and ReBOX. Independent
implementation using existing FBNeo; no incompatible GPL code imported.
Full libretro link, actual RK3566 FPS, frontend save/load and audio
continuity are unverified. No guest clocks, delays, DIP or sound
settings changed. Experimental modes require on-device A/B.
