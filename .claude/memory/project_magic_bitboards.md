---
name: project_magic_bitboards
description: "Sliding-piece attacks switched from BMI2 pext to magic bitboards (portability for AMD Zen 1/2 where pext is microcoded). Magics are precomputed constants baked into generated_data.h (regenerate with tools/dump_magics, fixed seed); kept the uint8_t index compression so tables are ~155 KB. bench unchanged (6857880), perft node counts identical."
metadata: 
  node_type: memory
  type: project
  originSessionId: bb1fa17a-29da-4e59-97eb-705cf2550c6a
---

**Sliders use magic bitboards, not pext (switched 2026-06-29 at the user's request, for portability).**
The user wanted off the BMI2 `pext` instruction because it is microcoded/slow on AMD Zen 1/2; magic
bitboards are a plain 64-bit multiply (`hash = (occupancy & mask) * magic >> shift`) that is fast on every
x86-64 CPU. The pext path (`PextBitboard`, `ComputePext`/`MakePexts`, the `static_assert(__BMI2__)`) was
**deleted** — magic is the only slider implementation now.

Key decisions:
- **Magics are precomputed constants**, not searched at startup. `kBishopMagicNumbers` / `kRookMagicNumbers`
  in [generated_data.h](src/generated_data.h) hold 64+64 baked-in multipliers. Only the attack/index tables
  are *filled* at startup (cheap). Regenerate the constants with **`tools/dump_magics`** (wired into the
  Makefile + CMake `tools` targets) — a fixed-seed (`0xD1CE5EED5EED5EED`) randomised search, so an unchanged
  engine reproduces them byte-identically (verified). Only needed if the occupancy-mask/attack generators
  change; the `assert` in `BuildMagicTable` fires on a stale magic.
- **Kept the uint8_t index compression** (the user explicitly asked to, matching the old pext layout):
  `MagicBitboard` has `indices_` (1 byte per hash slot) → `attacks_` (de-duplicated 8-byte sets). A rook
  square has 4096 occupancies but only ~100 distinct attack sets, so this shrinks the tables ~5×: **~155 KB**
  vs ~841 KB for a direct (one-load) layout. (<256 distinct sets/square, so uint8_t fits.) This re-adds the
  second dependent load, trading back the ~1.3% perft edge the direct layout had — accepted for the memory
  saving. See [[project_tried_pext_direct_lookup]].
- **Not an Elo change** — a portability change. perft on this Intel i9-13900HX: direct-magic was ~+1.3% over
  pext (one fewer load), compressed-magic ≈ pext baseline (same two-load pattern, multiply ≈ pext on Intel).
  Movegen is ~1-2% of node cost (NNUE-eval-bound), so perft deltas this size are ~0 Elo — consistent with
  the rejected pext-direct experiment. No SPRT run.
- **CPU floor unchanged:** the binary still needs AVX2 (Haswell/Zen1) for NNUE, so magics don't lower the
  minimum CPU — they only remove the slow-pext penalty on Zen 1/2. The build still passes `-mbmi2 -mavx2`
  (`-mbmi2` now only for the `tzcnt`/`blsr` bit-scan builtins).

Verified: `make check` all 8 suites pass, perft node counts identical (5,981,754,399), `bench` = 6857880
(unchanged determinism signature), magics regenerate byte-identically. Docs updated (CLAUDE.md attack-gen +
build sections, README features/requirements/attack-gen/limitations).
