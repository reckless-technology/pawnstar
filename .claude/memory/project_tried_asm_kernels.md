---
name: project_tried_asm_kernels
description: Tried & REJECTED — hand-written inline asm for the NNUE forward kernels (−3% nps vs clang intrinsics)
metadata:
  type: project
---

Tried & REJECTED: converting the NNUE forward hot path — the int8 output dot (`OutputDotInt8Vnni`/`Avx2`
+ `ScReluU8`) and the FT `AddColumn`/`SubColumn` — from AVX2/AVX-VNNI intrinsics to hand-written inline
`asm volatile` in `src/nnue.h`, behind an `NNUE_ASM` toggle (default on, intrinsics in `#else`).

**Why:** test whether hand asm beats clang `-O3` on the already-vectorised kernels — specifically by using
**4 independent dot accumulators** to break the single-accumulator `vpdpbusd`/`madd` dependency chain the
intrinsics leave to the compiler.

**Result: −3.4% nps, REJECTED.** Bit-identical (bench node count = **7689693** = baseline; ctest 8/8 incl.
`test_nnue` + `test_nnue_incremental`). A/B on the i9-13900HX (Raptor Lake — has AVX-VNNI, so `bench` uses
the VNNI dot), 10 interleaved `bench` runs each: intrinsic **median 1768k nps** vs asm **1708k nps**; asm
lost pairwise in **9/10** rounds (incl. the warm high-turbo rounds).

**Why it lost:** the asm materialises all SCReLU activations to a stack buffer, then dots in a second pass —
the ~2 KB store/reload + the lost activation↔dot interleaving cost more than the multi-accumulator break
gained. clang already schedules the fused single-accumulator reduction well, so there was little latency to
reclaim. A *fused* asm (ScReluU8 in asm, no buffer) was considered but not attempted — ceiling ≈ match clang,
not beat it.

**Gotchas (each cost a cycle):**
- In a GCC/clang inline-asm template, `{...}` means a dialect-alternative block, so the AVX-VNNI VEX-forcing
  prefix must be escaped as `%{vex%}` (not `{vex}`) to emit a literal `{vex}` to the assembler.
- Without the VEX prefix, clang assembled `vpdpbusd %ymm` as **EVEX** (AVX-512VNNI, prefix byte `62`) →
  **SIGILL** on Raptor Lake (no AVX-512). The VEX form (prefix `c4`) is required; the function also still
  needs `[[gnu::target("avx2,avxvnni")]]` so the assembler accepts the mnemonic. Verify with `llvm-objdump`
  (want `c4 e2 ...`, not `62 ...`).
- `bench` node count is the bit-identical gate: a 7.7M-node search over 14 positions matching to the node
  proves eval equality far more strongly than `test_nnue`'s ±40 cp tolerance.

Branch `nnue-asm-kernels` was deleted (never committed/merged). Don't repeat without a fundamentally
different approach (e.g. pre-permuting the output weights to drop `PackU8`, or a measured fused no-buffer
kernel). Links: [[machine-windows-native-build]] (bench/build recipe); CLAUDE.md NNUE "Speed" levers.
