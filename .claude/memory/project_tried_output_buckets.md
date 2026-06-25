---
name: project_tried_output_buckets
description: "Tried & REJECTED — adding 8 piece-count OUTPUT BUCKETS to the v11 arch. Weak +6.1 ± 9.5 fixed-depth at 500M but REGRESSES at 6B: -22.96 ± 15.7 @ 8+0.08 and -14.70 ± 13.3 @ 12+0.12 (both H0). Bucket gains shrink with data + a small TC speed cost. Don't repeat at this scale."
metadata:
  node_type: memory
  type: project
  originSessionId: 9c8f1d8c-5768-4b87-83c0-07d07141f44a
---

**Experiment (2026-06-24, branch `nnue-output-buckets` / worktree `worktree-nnue-output-buckets`, NOT merged):
augment the v11 NNUE with piece-count OUTPUT BUCKETS.** v11's head is `concat[stm|ntm]=2048 -> 1`. The
candidate made it `2048 -> 8` and **selects one bank by piece count** (bullet `MaterialCount<8>`:
`bucket = (popcount(occ) - 2) / 4`, divisor `ceil(32/8)`). FT unchanged (int16, incremental). Output is
bucket-major `[8][2048]` (bullet `.transpose()` save), so the int8/int16 dot kernels are reused as-is on the
selected bank's contiguous slice — no float path (lower-risk than the [[project_tried_hidden_layer]] head).

**Result: REJECTED — output buckets HURT v11 at scale, at both time controls.** Single-variable A/B (control
= v11 arch, candidate = v11 + buckets, trained on the SAME pool, same recipe):
- **500M, fixed depth 8: +6.1 ± 9.5 (H0, inconclusive, 3000 games)** — weakly positive but underpowered.
- **6B (v11's ~6.36B pool, SBS=300), time control:**
  - **8+0.08 → -22.96 ± 15.71, H0 accepted (970 games, 46.7%)**
  - **12+0.12 → -14.70 ± 13.27, H0 accepted (1466 games, 47.9%)**

The slower TC is **less negative** (-23 → -15) — classic TC-dependence (the 8× output table's per-move speed
cost weighs less at 12+0.12) — but it never turns positive. The weak +6 fixed-depth signal at 500M did NOT
survive at 6B. **Two compounding causes:** (1) **bucket gains shrink with data** — exactly the king-bucket
precedent in [[project_nnue_experiment]] (+29 @ 750M → +11 @ 2.31B); here output buckets went from +6 @ 500M
to negative @ 6B; (2) a **small TC speed penalty** — the int8 output table is 8× larger (16 KB vs 2 KB),
adding cache pressure per eval (the 500M number was fixed-depth, so it never paid this; the 6B TC does).
**Caveat:** a 6B *fixed-depth* A/B wasn't run, so the split between eval-quality regression vs pure speed
cost isn't isolated — but TC is the ship-relevant measure and it's a clear regression. If ever revisited:
fewer buckets (4) and/or a fixed-depth 6B check first; but don't expect a win at this data scale.

**Implementation notes (reusable):** bullet output buckets = `.output_buckets(MaterialCount::<N>)` +
`l1 = new_affine("l1", 2*HIDDEN, N)` + `l1.forward(concat).select(output_buckets)`; save `l1w` with
`.transpose()` (⇒ bucket-major) and `l1b` length N. Engine ([nnue.h] on the branch): `kNumOutputBuckets=8`,
`OutputBucket(pos)` = `(OccupiedSquares().PopCount()-2)/kOutputBucketDivisor`, threaded into
`Evaluate(acc, stm, bucket)`; NetHeader format_version 2 + `output_buckets_` field. Trainer
`tools/bullet/pawnstar_ob.rs`, reference `tools/bullet/pawnstar_eval_ob.rs`. **The trial-gate first
(3-superbatch net → cross-check + material/PHASE sanity) caught nothing this time (layout was right up
front), but is the right habit** — see the [[project_tried_hidden_layer]] transpose gotcha. Net/data scratch
in `~/pawnstar_nnue/exp_ob/`. See [[machine-setup-env]] for the toolchain.

**Process note (cost ~2h once):** a background watcher whose loop condition was `! pgrep -f get500m.sh`
matched its OWN command line → infinite loop, training never launched. Same self-match trap as the
`pkill -f` note in [[project_nnue_experiment]]. **For completion watchers use file-existence
(`until [ -f net.bin ]`) or `pgrep -x <exact-procname>` (e.g. `fastchess`), never `pgrep -f <script-name>`
that appears in the watcher's own argv.**
