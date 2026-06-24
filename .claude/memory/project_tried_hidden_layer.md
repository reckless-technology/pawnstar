---
name: project_tried_hidden_layer
description: "Tried & REJECTED — adding a hidden layer to the NNUE head (2048->16->1). Neutral-to-negative: -11.3 ± 12.9 Elo at fixed depth 8 vs the plain 2048->1 head, H0 accepted (1908 games), same 200M data + recipe. Don't repeat without tuned training/more data."
metadata:
  node_type: memory
  type: project
  originSessionId: 9c8f1d8c-5768-4b87-83c0-07d07141f44a
---

**Experiment (2026-06-24, branch `nnue-hidden-layer`, NOT merged): add a hidden layer to the NNUE head.**
Current arch is FT(768x4 -> 1024) per perspective -> SCReLU -> concat(2048) -> **1**. The candidate inserted
a head: concat(2048) -> **16** -> 1, SCReLU on the 16. Single-variable A/B: trained a control (current arch)
AND the candidate on the **same shuffled 200M** PlentyChess set, same schedule (30 superbatches, BPS 12207,
StepLR), then SPRT'd head-to-head.

**Result: REJECTED. Fixed-depth-8 SPRT, candidate vs control: Elo -11.29 ± 12.85, H0 accepted (LLR -2.95),
1908 games (48.4%).** Fixed depth = pure eval-quality (speed-independent), and it's already negative there,
so the heavier head can only lose more on the clock — **stage-2 (int8 head + TC) was not worth running.**
Conservative: control used its int8 head (~24cp deviation at fixed depth) vs the candidate's exact float
head, so control was if anything slightly handicapped and still won by 11. **Adding a hidden layer does not
make this engine stronger at this data scale + recipe.**

**Caveats (why this isn't the absolute last word):** the candidate reused the 2-layer net's LR
schedule/epochs/init — a deeper head may want tuned hyperparameters (LR, more superbatches, different init),
and 200M is modest (though the head is only ~33k params, so data-starvation is a weaker excuse than for
width/buckets — see [[project_nnue_experiment]]). If revisited: tune the candidate's training and/or scale
data; don't just re-run the same recipe.

**Implementation notes (reusable for any future multi-layer NNUE work):**
- **Kept the FT int16 + incremental (unchanged)**; ran the new head in **float** (f32-saved head weights,
  unquantised) — negligible vs the FT at fixed depth, and it makes the engine forward pass match the trainer
  trivially (no risky multi-layer integer requant). `test_nnue_incremental` passes untouched (FT unchanged).
- **GOTCHA that cost a debug cycle — bullet affine weight layout is INPUT-MAJOR.** bullet stores an affine
  weight `w[input*output_dim + output]` (its `transpose_impl` confirms; the FT already reads
  `feature_weights[feature*HIDDEN + i]` = input-major). The 2048->1 OUTPUT layer hides this (O=1, both
  layouts coincide), so I first read the 2048->16 `l1` output-major and the net evaluated a near-CONSTANT
  ~-135 regardless of material (dead head: garbage pre-activations -> SCReLU zeros -> output = l2_bias).
  The 0cp test_nnue cross-check PASSED while broken because the engine and `pawnstar_eval_h` read it the
  *same* wrong way — **so a cross-check alone is insufficient; always sanity-check evals track material
  (up-a-queen -> ~+1200, sign follows side-to-move) before trusting a new arch / running an SPRT.** Fix:
  read `l1_weights_[k*kHidden2Size + j]` (input-major); no retrain needed (same bytes, correct indexing).
- Arch is header-gated: `NetHeader` format_version bumped to 2 + a `hidden2_size_` field; the candidate
  engine rejects v1 nets (and vice versa), so the SPRT was binary-vs-binary with `CAND_STARTUP_NET` giving
  the candidate engine a compatible net at startup (the [[project_nnue_experiment]] cross-arch SPRT pattern).
- Files: trainer `tools/bullet/pawnstar_h.rs` (f32 head save), reference `tools/bullet/pawnstar_eval_h.rs`,
  engine on branch `nnue-hidden-layer`. Net/data scratch in `~/pawnstar_nnue/exp_hidden/`. See
  [[machine-setup-env]] for the toolchain and [[project_tried_search_features]] for the search-side log.
