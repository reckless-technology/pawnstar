---
name: project_tried_update_prefetch
description: NNUE eval profile (Update dominates) + TRIED & REJECTED Update column-prefetch (neutral, L3-resident)
metadata:
  type: project
---

**NNUE eval profile** (via `tools/nnue_quant_study` Phase 0, i9-13900HX, single thread):
- Per-call: `Update` (FT incremental delta) **56–62 ns**, `Evaluate` (int8 output dot) **124 ns**,
  `Refresh` (from-scratch rebuild) **~700 ns**.
- Call ratio `Update:Evaluate` = **3.07:1 middlegame, 4.71:1 endgame**.
- Time split → **`Update` is ~58–68% of NNUE compute** (the *dominant* cost), output dot ~32–42%,
  `Refresh` ~11% (rare — only ~2% of plies cross a king-bucket boundary). The eval cache also skips ~42%
  of forward passes (and their catch-up Updates) entirely.
- NB: the tool's own summary line "Evaluate ~50–60%" is **stale/inconsistent** — its per-call math gives
  32–42%. Trust the per-call × call-rate numbers.

**TRIED & REJECTED: prefetching the feature-weight columns in `Network::Update`.** Prepended a pass to the
common-case (no bucket-cross) loop that `_mm_prefetch(_MM_HINT_T0)`s each touched `feature_weights_` column
before the add/sub loop, to overlap the random-access first-line misses. Bit-identical (bench = 7689693 = N0;
prefetch is a no-op hint). **Neutral: +0.03% median nps, flat microbench, won 8/10 pairwise but inside the
laptop's ~5–10% noise.** Branch `nnue-update-prefetch` deleted (not committed).

**Why neutral (the durable lesson):** `feature_weights_` is **12.6 MB and fits in this CPU's 36 MB L3**, so
during a single-threaded search the columns are **L3-resident (~40 cyc), not DRAM misses (~200 cyc)** — there
is little latency for a prefetch to hide. `Update`'s cost is therefore **int16 add-throughput** over the
columns, NOT memory latency — which also explains why int8 *feature* weights (a bandwidth play) was rejected
−8 Elo (see [[project_nnue_experiment]]). Prefetch (latency) and int8-FT (bandwidth) both miss the actual
bottleneck.

**Open eval-speed levers (none clearly worth it):** finny tables / accumulator-refresh cache → only the ~11%
`Refresh` slice, so ~1–2% bounded; **AVX-512-dispatched kernels** → would halve the `Update` add-throughput
and the dot, but only help Zen4/server (this Raptor Lake has AVX-512 fused off; untestable here) — this is the
lever best-matched to the throughput-bound `Update`; threaded prefetch → the one case the rejected prototype
might pay (shared-L3 contention under Lazy SMP), needs a threaded SPRT. Eval speed is otherwise near
saturation; at equal nodes the Elo is now in eval *quality* (data/buckets). Related: [[project_tried_asm_kernels]]
(hand-asm kernels, also rejected −3%).
