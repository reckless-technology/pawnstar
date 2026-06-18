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

## Reproducing

```bash
make build/nnue_quant_study
build/nnue_quant_study nnue/pawnstar-v8.bin test/nnue_reference.txt
```

Call-ratio counters are not committed (they were temporary DEBUGX increments in `nnue::Network::Update` and
`Evaluate`); to re-measure, add `INCREMENT("nnue Update calls")` / `INCREMENT("nnue Evaluate(acc) calls")`
to those two methods, `make`, then `printf 'position fen <FEN>\ngo depth 12\n' | PAWNSTAR_THREADS=1
build/pawnstar` and send `dbg` after `bestmove` (the search is async — wait for `bestmove` first, and use a
non-book position or the book short-circuits the search).
