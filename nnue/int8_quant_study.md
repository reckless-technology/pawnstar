# int16 → int8 NNUE quantisation — feasibility study & plan

Measured investigation of switching Pawnstar's NNUE quantisation from 16-bit to 8-bit: where the time
goes, what int8 can actually accelerate, and what it costs in eval accuracy / dynamic range. All numbers
are reproducible with the bundled tool:

```bash
make build/nnue_quant_study
build/nnue_quant_study nnue/pawnstar-v8.bin test/nnue_reference.txt
```

The tool ([tools/nnue_quant_study.cpp](../tools/nnue_quant_study.cpp)) loads the shipped net, drives the
250 checked-in reference FENs through the real `nnue::Network`, and reports dynamic-range histograms, an
int8 emulation error sweep, AVX2 saturation rates, and per-call kernel timings. The Update:Evaluate
*call ratio* was measured separately with temporary DEBUGX counters in `nnue::Network::Update` /
`Evaluate` over single-threaded `go depth 12` searches (counters reverted afterwards — not in the tree).

Environment: clang-18, this box is `znver`/Intel-class with **AVX2 + AVX-512 + AVX-VNNI/AVX-512-VNNI**
(`/proc/cpuinfo`), but the shipped binary targets only `-mavx2` (Makefile), so VNNI is currently unused.

---

## TL;DR

1. **The output layer (`Evaluate`) is ~50–60% of NNUE compute, not a few percent.** My initial assumption
   (FT accumulator dominates, output layer is negligible) was **wrong**. `Evaluate` costs 443 ns/call vs
   `Update`'s 93 ns/call — 4.7× more — and fires often enough (1 per ~3–5 Updates) that it is half the bill.
   **int8's reachable target is therefore large.** This is the finding that flips the recommendation.
2. **The FT accumulator must stay ≥ int16** (live values reach −4840; int8 would overflow instantly). int8
   does *not* speed up the dominant-by-count `Update` path. So int8 is an **output-layer** play, not an FT play.
3. **int8 output layer is clean to quantise.** Output weights already fit int8 (`max|w|=127`, scale `WS=1`),
   and **S=8** maps SCReLU's `[0, 65025]` onto `uint8 [0,254]` with no extra clamp — the natural operating
   point. Emulated eval error vs int16: **mean 3.1 cp, max 13 cp** over the 250 positions.
4. **VNNI is the right kernel** and is viable: the int64 dot never exceeds 86M, so an int32 `vpdpbusd`
   accumulator suffices. AVX2 `pmaddubsw` (no VNNI) saturates int16 on ~1-in-256000 pairs at S=8 — rare but
   nonzero, so an AVX2-only int8 kernel needs saturation-safe staging.
5. **Net expectation:** int8 on the output layer can plausibly cut `Evaluate` ~2–3× (VNNI), i.e. a **~20–35%
   reduction in NNUE time** → a real nps/clock gain, *if* the 3 cp eval error is strength-neutral under SPRT.
   This is worth prototyping. It is **not** the "marginal, skip it" conclusion the pre-measurement plan reached.

---

## Phase 1 — dynamic range & saturation (250 positions, both perspectives)

| Quantity | min | p50 | p95 | p99 | p99.9 | max | notes |
|---|---|---|---|---|---|---|---|
| Accumulator (int16) | −4840 | −92 | 130 | 254 | 445 | 752 | **needs int16**; int8 (±127) overflows |
| SCReLU act `[0,QA²]` | 0 | 0 | 16900 | 64516 | 65025 | 65025 | 74.4% are 0; only 0.98% exceed QA pre-clamp |
| `|output dot|` (i64) | 18105 | 7.67M | 43.8M | 72.6M | 85.7M | 85.7M | **max 86M ≪ 2.1B → fits int32** |

Static weights: `feature_weights max|w|=505` (QA=255 units), `output_weights max|w|=127` (QB=64 units →
**fits int8 directly, WS=1**), `output_bias=1841`.

**Key structural facts:**
- The post-SCReLU activation is **very sparse** (74% exactly zero) and **wide** (`[0,65025]`, 17 bits). The
  17-bit range is why SCReLU fights int8 — but a uniform `>>8` lands it in uint8 cleanly because QA²≈2¹⁶.
- The accumulator's −4840 floor is the hard reason the **feature transformer can't go int8**; this matches
  every top engine keeping an int16 FT accumulator even for int8 nets.

### int8 output-layer emulation (Option 1: requantise SCReLU `>>S`, int8 weights), error vs int16 eval

| S | mean \|Δcp\| | max \|Δcp\| | AVX2 pmaddubsw int16 overflow |
|---|---|---|---|
| 7 | 334.236 | 2662 | 7/256000 (0.003%) — activations saturate uint8 |
| **8** | **3.068** | **13** | **1/256000 (0.000%)** ← natural fit |
| 9 | 6.240 | 26 | 0/256000 |
| 10 | 13.016 | 57 | 0 |
| 11 | 23.208 | 119 | 0 |
| 12 | 44.772 | 190 | 0 |
| 13 | 84.776 | 484 | 0 |

S=7 is too small (square `>>7` overflows uint8 → catastrophic clamp). S=8 is the sweet spot: full SCReLU
range fits uint8 with no extra clamp, error is 3 cp mean. (The current int16 cross-check tolerance is ±2 cp;
3 cp mean is just above it — strength impact must be settled by SPRT, not by the cross-check gate.)

---

## Phase 0 — per-call kernel cost & the cost split

Single-thread, this CPU, 4M-iteration micro-timing of the real `nnue::Network` kernels:

| Kernel | ns/call | when it fires |
|---|---|---|
| `Update` (FT incremental) | **93 ns** | ~2× per node (make + undo) |
| `Evaluate` (output dot) | **443 ns** | ~1× per leaf, on eval-cache miss (~58%) |
| `Refresh` (full rebuild) | ~1000 ns | root / king-bucket crossing only |

Measured call ratios (DEBUGX counters, `go depth 12`):

| Position type | Update calls | Evaluate calls | Update:Evaluate | **Evaluate share of NNUE time** |
|---|---|---|---|---|
| Middlegame (`5r1k/.../7K w`) | 3,427,415 | 1,115,776 | 3.07 : 1 | **≈ 61%** |
| Endgame (`8/2p5/.../8 w`) | 714,258 | 151,577 | 4.71 : 1 | **≈ 50%** |

`Evaluate` is expensive per call because the AVX2 SCReLU dot does int32 squares + int32 weight multiplies +
int64 accumulation across 2048 lanes (widening-heavy). That is exactly the work `vpdpbusd` collapses.

---

## What int8 can and cannot do here

| Target | int8 verdict |
|---|---|
| **FT weights / accumulator** | **No.** Accumulator needs int16 (range −4840); int8 columns would need widening before the add → *more* ALU ops, only a cache-footprint win (1.5 MB→768 KB). Not worth the accuracy risk. Keep int16. |
| **Output activations + weights** | **Yes — the real opportunity.** It's 50–60% of NNUE time, weights already fit int8, S=8 fits the activation, int32 (VNNI) accumulation suffices. |

The pre-measurement plan's "int8 is marginal on this arch" was based on assuming the output layer was small.
It isn't. The corrected conclusion: **int8 on the output layer is the highest-value NNUE speed lever
currently on the table**, ahead of a wider net.

---

## Revised plan

**Phase 2 — prototype an int8 (VNNI) output layer behind a compile flag.**
- Add `-mavx512vnni`/`-mavxvnni` build variant (this box supports it; keep the AVX2 path as the portable
  default). Implement `Evaluate` with `vpdpbusd`: compute SCReLU in int32, `>>8` + pack to `uint8`, dot with
  int8 output weights (WS=1) into an int32 accumulator, then the existing dequant chain (rescaled by `2^8`).
- Cross-check against `test_nnue` with a relaxed tolerance (~13 cp, per the table) and bench nps. Acceptance:
  a clear `Evaluate` speedup with eval error matching the S=8 emulation.

**Phase 3 — SPRT the honest comparison.** int8-output engine vs int16 baseline at 40/20
(`tools/run_sprt.sh`). The question is whether 3 cp mean eval error is strength-neutral while the clock gain
is real. Ship only on a non-regression (Elo) with a measured nps win.

**Phase 4 — (conditional) requantise-aware retrain.** If the static `>>8` error costs Elo, retrain in bullet
with the int8 output quantisation in the loss (quantisation-aware), or trim QA so activations need less shift.
Follow the full ship checklist in [nnue/README.md](README.md) §7.

**Portability note.** The shipped binary targets AVX2. An AVX2-only int8 path must avoid `pmaddubsw`
saturation (the 1-in-256000 S=8 overflow): either stage partial sums via `pmaddwd` against a ones-vector, or
shave the weight scale. The cleaner answer is to ship a VNNI build variant and keep AVX2 on the int16 path —
i.e. int8 is gated on a VNNI target, which the dev/cloud box now provides.

---

## Phase 2 — RESULTS (int8 output layer, implemented behind `INT8=1`)

Implemented the int8 output forward pass in `nnue::Network::Evaluate` (`src/nnue.cpp`), gated by the
`INT8=1` build flag; the int16 path remains the default/shipped evaluator. Output weights are requantised to
int8 at load (`output_weights_i8_`); SCReLU activations are requantised to uint8 via `>> kInt8Shift`. The dot
uses `vpdpbusd` where VNNI is present and AVX2 `pmaddubsw`+`madd` otherwise.

**Shift choice: kInt8Shift = 9 (not 8).** S=8 is more accurate (3 cp mean) but lets a rare `pmaddubsw`
adjacent-pair saturate int16 on AVX2 — and an adversarial-data probe showed S=8 maddubs diverges from the
exact dot on essentially every eval (worst case large), while **S=9 keeps `u8 ≤ 127`, so each pair
(`2·127·127 < 2¹⁵`) fits int16 — 0/20000 adversarial trials diverge.** At S=9 the scalar, AVX2-maddubs and
VNNI-vpdpbusd kernels are therefore **bit-identical**, with no data-dependent saturation. Cost: eval error
6.2 cp mean / **26 cp max** vs int16 (the inherent SCReLU-square-into-8-bits loss).

**Correctness:** the engine int8 eval reproduces the Phase-1 S=9 emulation exactly — `test_nnue` reports
`max |diff| = 26 cp` for both the AVX2 and VNNI builds (identical, as designed), and the 189/250 "failures"
are just the ±2 cp int16 gate flagging the deliberate requant error. `test_nnue_incremental` still **PASS**es
(the FT/accumulator path is untouched).

**Speed (per-call `Evaluate`, same input, this CPU):**

| Build | `Evaluate` ns/call | vs int16 |
|---|---|---|
| int16 (default/shipped) | 416 | 1.0× |
| **int8 AVX2 / `pmaddubsw`** (laptop target: AVX2+BMI2, no VNNI) | **224** | **1.86×** |
| int8 AVX-VNNI / `vpdpbusd` (this cloud box) | 204 | 2.04× |

**Key result for the AVX2 ship target:** VNNI buys almost nothing over AVX2 `pmaddubsw` (204 vs 224 ns),
because the kernel cost is dominated by the *shared* SCReLU square + uint8 pack, not the dot instruction. So
an **AVX2-only laptop gets ~the full speedup (1.86×) with no VNNI needed.**

Projected NNUE-time reduction (Evaluate is 50–60% of NNUE time, now ×0.54): **≈ 23–28% less NNUE compute.**
End-to-end nps is *not* a clean way to confirm this — the requant changes the eval, hence the search tree
(int8 reached depth 14 in 8.2M nodes vs int16's 16.2M on the same positions), so summed nps across divergent
trees is confounded. The real end-to-end payoff is measured by **Phase 3 SPRT at a time control**, which
counts games-per-time and so prices in the speedup directly against the 6 cp eval error.

## Can the *feature* (input) weights also go int8? (measured)

Yes for speed, but with a sharp accuracy catch the output layer didn't have.

- **Speed is real.** A column-add microbench over a random-access weight table (mimicking the FT `Update`
  hot path) measured **int8 feature weights 1.50× faster** (31 vs 46 ns/column): the FT is partly
  memory-bound, and halving the 1.5 MB weight table's bytes outweighs the int8→int16 widening the add needs.
  (This **corrects** the Phase-1 "FT int8 = only a cache win, not worth it" note — the cache/memory win is
  the point, and it's substantial. Caveat: the bench uses uniform-random rows = cache-pessimal; if a search's
  hot columns are L1-resident the real gain is smaller, so this is an upper-ish bound.)
- **The accumulator must still be int16.** Live accumulator values reach −4840; only the *weights* go int8,
  and each int8 column is widened to int16 before being added to the accumulator.
- **The catch — it's lossy, unlike the output layer.** `feature_weights max|w| = 505`, which does **not** fit
  int8 (±127). Storing them int8 needs a scale `FS = ⌈505/127⌉ = 4`, i.e. dividing every feature weight by 4
  and **losing ~2 bits of precision on the most numerous, most strength-critical parameters**. The output
  layer was free (`max|w|=127`, `WS=1`, lossless); the FT is not. Doing it well needs a **quantisation-aware
  retrain** in bullet (train feature weights clipped into int8 range, then export `i8`), not a post-hoc ÷4.

**Verdict (initial):** int8 feature weights looked like a real *second* speed lever (~1.5× on the
~40–50%-of-NNUE `Update` path), but **retrain-gated and higher-Elo-risk** than the output layer.

### Prototype measurement (engine, behind `INT8_FT=1`) — the optimism didn't survive contact

Implemented int8 FT storage in the engine (`PAWNSTAR_INT8_FT`): the weight table is int8, `Load`
requantises the int16 net on the fly (scale = `⌈max|w|/127⌉` = **4** for v8), and the column-add
reconstructs ~original scale via `*feature_w_scale_`, leaving the accumulator/SCReLU/output untouched.
Two results, both discouraging for the *post-hoc* path:

| metric | int16 | int8-FT (÷4) | note |
|---|---|---|---|
| eval error vs bullet ref (max) | 0 cp | **182 cp** | ÷4 is far too lossy — FT weights are the most strength-critical |
| `Update` ns/call (cache-resident microbench) | 107.6 | **133.6 (slower)** | reconstruct widen+`mullo` overhead, *not* paid back when columns are hot |

So the two `Update` benchmarks **bracket reality**: the standalone uniform-random bench (whole 1.5 MB table,
cache-pessimal → memory-bound) showed int8 **1.5× faster**; the engine microbench (one move repeated →
columns stay L1-resident → ALU-bound) shows it **1.24× slower**. Real search is in between, and the
study-tool microbenches (single position repeated) systematically *under*-show the memory benefit, so they
can't settle it. `test_nnue_incremental` still passes (the reconstruct is deterministic).

**Revised verdict:** int8 feature weights are **not** a clear win like the output layer.
- The **÷4 post-hoc prototype is dead** (182 cp ≫ the output layer's 26; it would shed Elo badly).
- A **quantisation-aware retrain** (train feature weights clipped to ±127 → scale 1) is *required* — it
  makes the FT lossless **and** drops the reconstruct `mullo` (just widen+add), the best case for speed.
- Even then the speedup is **regime-dependent**: it only materialises if real search is memory-bound on the
  FT column loads. That needs a real-search measurement (confounded by eval), not a per-call microbench.

So before spending GPU time on the retrain, the open question is **"is the FT `Update` memory-bound in real
search?"** If not, even a lossless int8-FT is neutral-to-slower and isn't worth it. The output-layer int8
(Phase 2/3, +31.8 Elo) remains the clear, shippable win; int8-FT is speculative. The engine support is
committed (off by default, `INT8_FT=1`) so a future quantisation-aware net can be measured directly.

## Phase 3 — RESULTS (SPRT: int8 vs int16 at a time control)

The end-to-end question: does the 1.86× faster `Evaluate` (deeper search) outweigh the 26 cp requant error
in real games? Played the **int8 engine (`INT8=1`, AVX2/`pmaddubsw`)** against the **int16 engine**, both on
the *same* `pawnstar-v8.bin` net (differing only by the output kernel), at **TC 8+0.08** so the speedup can
convert into depth (a fixed-depth match would only expose the error, never the speed gain).

| games | W–L–D (int8) | score | **Elo** | LOS |
|---|---|---|---|---|
| 75 | 29–24–22 | 53.3% | +23.2 ± 33.8 | 75% |
| 402 | 164–118–120 | 55.7% | +39.9 ± 14.6 | 99.7% |
| 811 | 324–250–237 | 54.6% | **+31.8 ± 10.3** | **99.9%** |

**int8 is +31.8 ± 10.3 Elo at 8+0.08 — a decisive gain** (SPRT elo0=0/elo1=10 passes H1). The faster eval
more than pays for the requant error at this time control; int8 is not just a free simplification, it is
*stronger* here. ~29% draws (UHO book) gave the SPRT good power.

**Caveat — time-control dependence.** This is fast TC. As TC lengthens, both engines reach high depth and the
26 cp eval error matters *more* while the extra-nodes advantage matters *less*, so +31.8 will shrink at
40/20. It is very unlikely to invert to a regression given this margin, but the repo's ship standard is a
40/20 confirmation before flipping the default — that run is long and was not completed in this ephemeral
container. **Recommendation: int8 output layer is a strong ship-candidate; confirm at 40/20, then make
`INT8=1` (with the AVX2 `pmaddubsw` kernel) the default build.**

Environment note (nothing here is version-controlled): the SPRT needed tooling built/fetched in-container —
`fastchess` compiled from source, separate int16/int8 binaries (`build/pawnstar_int16`, `build/pawnstar_int8`),
and the UHO_4060_v3 opening book downloaded. The engine emits occasional illegal-PV `info` lines (a
pre-existing PV-reconstruction quirk) that fastchess warns on; it occurs *equally* on both sides and never
affects a played move, so it does not bias the result.

## Reproducing

```bash
# Phase 0/1 study (int16):
make build/nnue_quant_study
build/nnue_quant_study nnue/pawnstar-v8.bin test/nnue_reference.txt

# Phase 2 int8 output layer (AVX2 = laptop target; add VNNI=1 for vpdpbusd on capable CPUs):
make clean && make INT8=1 build/nnue_quant_study build/test_nnue
build/nnue_quant_study nnue/pawnstar-v8.bin test/nnue_reference.txt | grep Evaluate   # ~224 ns vs 416
build/test_nnue nnue/pawnstar-v8.bin test/nnue_reference.txt                          # max|diff|=26 cp (by design)
make clean && make   # restore the default int16 engine

# Phase 3 SPRT (int8 vs int16, same net, time control) — needs fastchess + an openings book:
make clean && make            && cp build/pawnstar build/pawnstar_int16
make clean && make INT8=1     && cp build/pawnstar build/pawnstar_int8
make clean && make            # restore default
NET="$(pwd)/nnue/pawnstar-v8.bin"
BASELINE_NET="$NET" TC=8+0.08 BIN_A=build/pawnstar_int8 BIN_B=build/pawnstar_int16 \
  tools/run_sprt.sh "$NET" <openings.epd> 1200   # fastchess on PATH; both sides load the same net
```

Call-ratio counters are not committed (they were temporary DEBUGX increments in `nnue::Network::Update` and
`Evaluate`); to re-measure, add `INCREMENT("nnue Update calls")` / `INCREMENT("nnue Evaluate(acc) calls")`
to those two methods, `make`, then `printf 'position fen <FEN>\ngo depth 12\n' | PAWNSTAR_THREADS=1
build/pawnstar` and send `dbg` after `bestmove` (the search is async — wait for `bestmove` first, and use a
non-book position or the book short-circuits the search).
