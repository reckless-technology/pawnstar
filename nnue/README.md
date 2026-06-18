# NNUE evaluation

Pawnstar evaluates **exclusively** with an **NNUE** (Efficiently Updatable Neural Network) — it is the
engine's only evaluator (the former hand-crafted evaluation, "HCE", was removed). At startup the engine
loads the shipped net `nnue/pawnstar-v8.bin` (relative to the working directory) and evaluates with it;
if that file can't be loaded the engine falls back to a copy of the net embedded in the binary (§3), and
errors out only if both fail. This document is the complete reference for how the net works, how to load a
net, and how to generate data, train, validate and strength-test a net.

The shipped net (`pawnstar-v8.bin`) is a **1024-wide, single-bank (no king buckets)** `ChessBuckets` net
(i.e. plain Chess768 at width 1024). §1, §2 and §6 describe that architecture; §7 has the lineage
(including the retired Chess768 256/512 nets and the retired 512-wide 4-king-bucket v6/v7).

In-engine code: [src/nnue.h](../src/nnue.h), [src/nnue.cpp](../src/nnue.cpp). The net is called from
[src/evaluation.cpp](../src/evaluation.cpp) (`EvaluatePosition`). Tooling: [tools/](../tools).

---

## 1. Architecture

A "perspective" network using bullet's `ChessBuckets` feature set (the shipped net uses a single bank —
no king buckets):

```
768 inputs per (perspective, king bucket)  ->  1024 feature transformer (weights shared across perspectives)
SCReLU,  concat[side-to-move | opponent] = 2048  ->  1 output  (centipawns, side-to-move relative)
```

Constants (must match the trainer): `kInputSize=768` (features per bucket), `kNumKingBuckets=1`,
`kFeatureRows=768*1=768`, `kHiddenSize=1024`, `kQA=255`, `kQB=64`, `kScale=400`.

**Features.** The 768 base inputs per perspective are `colour (2) × piece type (6) × square (64)`. The
shipped net has a single weight bank (`kKingBucketMap` all-zero → `wb = bb = 0`, no king buckets), so the
feature row is just the Chess768 index below. The bucket mechanism is retained and generalises: with N
banks each perspective selects one by *its own* king's square (in that perspective's orientation) through
the 64-entry `kKingBucketMap`. For a piece of `colour` (0=white, 1=black), type `pt` (pawn=1 … king=6) on
`square`, with `wb`/`bb` the white/black king buckets (both 0 for the shipped net):

```
white-perspective row = wb*768 + colour     * 384 + (pt-1)*64 + square
black-perspective row = bb*768 + (1-colour) * 384 + (pt-1)*64 + (square ^ 0x38)   # 0x38 flips the rank
```

Two accumulators are built (white-perspective and black-perspective), each seeded with `feature_bias`
and then summing the feature column of every piece in its king bucket's bank. At evaluation the
**side-to-move** accumulator is fed to the first 1024 output weights and the opponent's to the second
1024. This is mathematically identical to bullet's `ChessBuckets::map_features` with the same bucket map
(verified by the cross-check in §6). The bucket map MUST stay byte-identical across `src/nnue.cpp`,
`tools/bullet/pawnstar.rs` and `tools/bullet/pawnstar_eval.rs`.

**Forward pass** — the **reference** path (`Network::EvaluateExact`) mirrors bullet's `simple` example
exactly; `screlu(x) = clamp(x,0,QA)²`:

```
out  = Σ screlu(stm[i])·w_stm[i]  +  Σ screlu(ntm[i])·w_ntm[i]   # i64
out /= QA                 # SCReLU leaves QA*QA*QB units; reduce one QA
out += output_bias        # bias is in QA*QB units
out *= SCALE
out /= QA * QB            # -> centipawns, side-to-move relative
```

**Shipped output layer is int8** (`Network::Evaluate`, the search hot path): the activations are requantised
to uint8 (`screlu(x) >> 9` → `[0,127]`) and the output weights to int8, with the dot accumulated in int32 —
~1.86× faster than the int16 dot (a measured **+31.8 ± 10.3 Elo at 8+0.08**) at a bounded ~26 cp deviation
from `EvaluateExact`. It runs on the baseline AVX2 build via `pmaddubsw`; the `>>9` keeps every adjacent pair
inside int16, so the AVX2 and VNNI (`vpdpbusd`, `make VNNI=1`) kernels are saturation-free and bit-identical.
The int16 *accumulator* is unchanged (the int8 *feature*-weight experiment behind `INT8_FT=1` is not shipped;
see [int8_quant_study.md](int8_quant_study.md)).

The net is an `nnue::Network` class (the quantised weights plus `Load`/`Refresh`/`Update`/`Evaluate`
methods), **owned by the `Game`** — there are no nnue globals. Its accumulator is **maintained
incrementally** across make/undo on each thread's `SearchState`: in the common case (no king-bucket
crossing — always, for the single-bank net) `Network::Update` derives the changed squares directly from
the parent/child **colour bitboards** — a move touches only 2–4 squares, and captures, en passant,
castling and promotion all fall out uniformly — and applies just those feature-column deltas to both
perspectives (no per-piece-type scan; reversible on undo, a no-op for null moves). This is bit-identical to
a per-piece diff: the same columns are added/subtracted and int16 accumulator add/sub is order-independent.
When a king move **crosses a bucket boundary** that whole perspective's bank changes, so `Update` rebuilds
just that one perspective from scratch (at most one king moves per ply, so at most one perspective
refreshes per update); `Network::Refresh` rebuilds both at the root.
`Network::Evaluate(position)` (a full refresh) is the reference the incremental path is checked against
(§6). Both kernels (accumulator update and forward pass) are **AVX2-vectorised** when built with
`-mavx2`, with scalar fallbacks behind `#if defined(__AVX2__)`; the vectorised path is bit-identical to
the scalar one (verified by `test_nnue`).

## 2. File format

The **payload** is bullet's native quantised weights — tightly packed, little-endian `int16`, in this order:

| section | count (int16) | scale |
|---|---|---|
| `feature_weights` | `768*1 * 1024 = 786432` (column-major: `row*1024 + i`, rows bucket-major) | QA |
| `feature_bias` | `1024` | QA |
| `output_weights` | `2 * 1024 = 2048` (first 1024 = stm, next 1024 = ntm) | QB |
| `output_bias` | `1` (scalar) | QA·QB |

Packed payload is `(786432 + 1024 + 2048 + 1) * 2 = 1579010` bytes; bullet pads the trailing 1-element
`output_bias` tensor up to a 64-byte boundary, so its raw output is **1579072 bytes**.

**Self-describing header.** A shipped net is prepended with a **32-byte `NetHeader`** (`src/nnue.h`):
a magic (`"PSN1"`), a format version, and the architecture parameters — `kInputSize`, `kNumKingBuckets`,
`kHiddenSize`, `kQA`, `kQB`, `kScale`. So `nnue/pawnstar-v8.bin` is **1579104 bytes** (32 + 1579072). The
loader (`Network::Load`) accepts two layouts:

- **Stamped** (magic present): every architecture field is checked against the engine's compile-time
  constants; any mismatch is rejected with a descriptive message (e.g. `file is …buckets4…hidden512…,
  engine expects …buckets1…hidden1024…`) — so an incompatible net can never be silently misread.
- **Raw** (no magic, e.g. a freshly-exported bullet `quantised.bin`): accepted only when its size equals
  the payload exactly, within bullet's `<64`-byte padding (`kNetFileBytes ≤ size < kNetFileBytes+64`).
  This exact window — not a `>=` floor — is what rejects a *larger* (e.g. wider) raw net rather than
  misreading it.

Stamp a raw net with `tools/stamp_net` (`make tools`); the header values come from the building engine's
constants, so it can only stamp a net that matches this build. The forward-pass math and quantisation are
kept in lock-step with the trainer ([tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs)); §6 verifies
they agree to within rounding. (The retired nets had different sizes/headers — e.g. the Chess768 512-wide
v4 a 789506-byte payload, the 512×4 king-bucketed v6/v7 a 3148802-byte payload — so this 1024-wide build
size/header-gates them out.)

## 3. Using a net in the engine

A net is required (NNUE is the only evaluator). `main.cpp` loads `nnue/pawnstar-v8.bin` (cwd-relative) at
startup. **Embedded fallback:** a byte-identical copy of the shipped net is compiled into the engine
binary (`src/embedded_net.S` `.incbin`'s the net at the Makefile's `EMBED_NET` path — passed in via
`-DEMBED_NET_PATH`, the single source of truth, so the embedded copy can't drift from the shipped net;
linked into the engine only, not the test binaries). If the on-disk file can't be loaded — wrong cwd, missing or renamed file, or a stamped net that
fails architecture validation — the engine loads the embedded copy via `Network::LoadFromMemory` (same
header detection/validation as `Network::Load`), so it always has a working evaluator; it exits only if
both the file and the embedded copy fail. To use a *different* net, point the engine at it — at launch:

```bash
PAWNSTAR_EVALFILE=/path/to/net.bin ./build/pawnstar
```

or at runtime:

```
setoption name EvalFile value /path/to/net.bin
```

`EvalFile` is advertised in the `uci` response; there is no `UseNNUE` option (nothing to toggle to). The
`eval` command prints the NNUE evaluation of the current position; `nnue` prints the raw network eval. The
net is a single `nnue::Network` instance owned by the `Game` (UCI `setoption` routes to
`game.NnueNetwork().Load(...)`), read-only after load and shared by all Lazy SMP threads through their
`Game&` — they call only its `const` methods.

## 4. Tooling

| file | purpose |
|------|---------|
| `stamp_net.cpp`           | prepend the self-describing `NetHeader` to a raw bullet net (`make tools` → `build/stamp_net`) |
| `setup_bullet.sh`         | clone bullet at the pinned commit + install/register our trainer examples |
| `bullet/pawnstar.rs`      | the trainer (arch `768 single-bank → 1024×2 → 1`, SCReLU, QA/QB/SCALE) |
| `bullet/pawnstar_eval.rs` | reference evaluator for the `test_nnue` cross-check |
| `train_pipeline.sh`       | train → export a net from a bulletformat `.data` file (or a directory of text shards) |
| `verify_net.sh`           | cross-check (engine == trainer) + incremental-accumulator gate for a trained net |
| `run_sprt.sh`             | SPRT / match: net-vs-net (set `BASELINE_NET`), at fixed depth or time control |
| `rate.sh`                 | anchored absolute-Elo estimate vs rated reference engines (you supply the opponent binaries) |
| `run_experiment.sh`       | one-shot: train + **verify** + SPRT from an existing `.data` dataset |

**Prerequisites:** `clang++`/`make` (engine), Rust/`cargo` (bullet), `fastchess` on `PATH` (SPRT). For
GPU training, a CUDA toolkit (`CUDA_PATH`, default `~/cuda-12.2`) **and** a driver new enough for the
toolkit: bullet's CUDA backend calls `cuFuncLoad`, which needs a driver supporting CUDA ≥ 12.3
(driver ≥ 545). Otherwise train on CPU with `BULLET_FEATURES=""` — the net is small enough that CPU
training is practical.

## 5. Quick start (minimal retrain)

```bash
tools/setup_bullet.sh                                          # once: clone + register bullet (pinned commit)
OPENINGS=~/openings.epd \
  tools/run_experiment.sh ~/pawnstar_nnue/shuffled.data net.bin   # train + verify + SPRT  ->  net.bin
```

Then load `net.bin` in the engine (§3). To train on CPU: `BULLET_FEATURES="" … tools/run_experiment.sh …`.
`run_experiment.sh <shuffled.data> <net.bin> [superbatches=40] [sprt_rounds=500] [depth=8]` chains
`train_pipeline.sh` → `verify_net.sh` → `run_sprt.sh`. The data argument is an already-converted
bulletformat `.data` file (how the shipped nets were trained — see §7); it carries no FENs to sample
openings from, so supply your own SPRT opening book with `OPENINGS=…`.

## 6. Step-by-step

### Data

The shipped nets are trained on **public PlentyChess** bulletformat data — a pre-converted, shuffled
`.data` file (bullet `ChessBoard` records, 32 bytes each). That file is the input to training below.

`train_pipeline.sh` also accepts a directory of text shards (`data_*.txt` / `seed*.txt`) in bullet's
`convert --from text` format — one record per line, `<fen> | <white_relative_eval_cp> | <wdl>` (wdl:
1.0 win / 0.5 draw / 0.0 loss, White's POV) — which it combines, converts, and shuffles. (Pawnstar's
own self-play generator, which produced such shards, was the original bootstrap source and has been
removed; bring text shards from any source in this format, or use a `.data` file directly.)

### Training

`train_pipeline.sh <shuffled.data | data_dir> <out_net.bin> [end_superbatch=40]` auto-detects its input
from the first argument:

1. ensures bullet is set up, builds `bullet-utils` and the trainer (`$BULLET_FEATURES`, default
   `--features cuda`);
2. **if the argument is a `.data` file** (already bulletformat — e.g. public PlentyChess data, the way
   the shipped nets were trained), it is used directly and steps 3–4 are skipped; **if it is a
   directory**, the text shards `data_*.txt` + `seed*.txt` are combined → `all.txt`,
   `bullet-utils convert --from text` → `all.data` (bullet `ChessBoard`, 32 bytes/record), then
   `bullet-utils shuffle` → `shuffled.data` (correlated data needs shuffling);
3. trains, computing `batches_per_superbatch = positions / 16384` so a superbatch ≈ one epoch;
4. copies `<work_dir>/checkpoints/pawnstar-<end_superbatch>/quantised.bin` → `out_net.bin`, where
   `<work_dir>` is the data directory (text mode) or the `.data` file's directory (bulletformat mode).

Trainer hyperparameters ([tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs)): `HIDDEN_SIZE=512`,
AdamW, `batch_size=16384`, loss `sigmoid(out)` squared-error against
`target = wdl·result + (1-wdl)·sigmoid(score/SCALE)` with `ConstantWDL=0.5`, `StepLR{start=0.001,
gamma=0.3, step=SBS/3}`, `eval_scale=400`. Overridable via env: `PAWNSTAR_DATA`, `PAWNSTAR_BPS`,
`PAWNSTAR_SBS`, `PAWNSTAR_OUT`. GPU training reports the device (e.g. `Training on NVIDIA GeForce GTX
1050 Ti (sm_61)`) and runs at ~1.7M positions/sec for this net.

### Openings

The SPRT needs an EPD opening book (`OPENINGS=…`) of varied, roughly balanced early-game positions so
matches don't all start from the same spot. Supply your own — any standard EPD opening set works. Use at
least `sprt_rounds` positions to avoid repeats.

### Validate inference (cross-check)

The engine's inference must reproduce the trainer's. Build the reference evaluator, sample some FENs
(include both side-to-move colours), and compare:

```bash
# build the bullet-side reference evaluator (CPU is fine; add --features cuda to reuse the GPU build)
(cd ~/pawnstar_nnue/bullet/crates/bullet_lib && cargo build --release --example pawnstar_eval)
EVAL=~/pawnstar_nnue/bullet/target/release/examples/pawnstar_eval

# get a FEN list (reuse the checked-in reference FENs; or supply your own, one FEN per line, both colours)
cut -d'|' -f1 test/nnue_reference.txt > fens.txt
"$EVAL" net.bin fens.txt > ref.txt
./build/test_nnue net.bin ref.txt        # EvaluateExact vs ref <=2 cp; int8 Evaluate vs EvaluateExact <=40 cp
```

`pawnstar_eval` uses bullet's own feature extraction (`ChessBuckets`) and the same SCReLU inference, so a
match confirms our feature indexing, king bucketing, perspective/orientation, SCReLU and dequantisation
are all correct. It must be built at the same width and feature set as the engine, or the cross-check
fails by design.

The repo checks in `test/nnue_reference.txt` — 250 reference evals for the shipped `pawnstar-v8.bin` —
and `make check` runs `test_nnue nnue/pawnstar-v8.bin test/nnue_reference.txt` automatically (current
engine: max |diff| 0 cp). The reference's first field is the FEN, so regenerate it after shipping a *new*
net with: `cut -d'|' -f1 test/nnue_reference.txt > fens.txt && "$EVAL" nnue/pawnstar-v8.bin fens.txt >
test/nnue_reference.txt`. With no arguments `test_nnue` is a no-op, so `make check` stays green if the
net/reference are absent.

A second test, `test_nnue_incremental <net.bin>`, plays random move sequences through a `SearchState`
and asserts the incrementally-maintained accumulator evaluates **bit-identically** to a full refresh at
every node (and again after every undo, to catch reverse-update bugs). With no argument it is a no-op,
so `make check` stays green; run it with a net (e.g. `./build/test_nnue_incremental nnue/pawnstar-v8.bin`)
after any change to the accumulator, feature indexing, or make/undo.

`verify_net.sh <net.bin>` bundles both checks into one gate: it regenerates the reference evals for
*that* net (reusing the FENs from `test/nnue_reference.txt`) with `pawnstar_eval`, then runs `test_nnue`
and `test_nnue_incremental`. `run_experiment.sh` runs it between training and the SPRT — a net that
fails it has a width/format mismatch, so any match result would be meaningless. Run it standalone after
training a net outside the one-shot flow.

### Strength testing (the verdict)

NNUE strength is measured by game play. NNUE is the only evaluator, so an SPRT is always **net-vs-net**.
`run_sprt.sh <net.bin> <openings.epd> [rounds=500] [depth=8]` runs:

- the candidate as `<net.bin>` against a **required** `BASELINE_NET=<other.bin>` opponent — the
  architecture program's test (e.g. v8 vs v7). Net selection is purely via the `EvalFile` option;
- the **same** `build/pawnstar` binary for both engines, differing only by `EvalFile`, with
  `PAWNSTAR_THREADS=1`. Different net *widths* need different builds (`kHiddenSize` is compile-time), so a
  cross-width SPRT points `BIN_A`/`BIN_B` at two separately-built binaries;
- **fixed depth** (default 8) — pawnstar has no `go nodes`, and fixed depth isolates eval *quality* from
  per-node speed. To measure the equal-*time* effect (incremental vs full-refresh, or a wider net's speed
  cost) set `TC=8+0.08` for a time-control match instead;
- **win/draw adjudication** (`-resign movecount=3 score=600`, `-draw movenumber=40 movecount=8
  score=20`) to keep games short and decisive (recommended, not required — the old fixed game-history
  cap that crashed long games is gone; the history is a dynamic `std::vector` now);
- `-sprt elo0=$ELO0 elo1=$ELO1 alpha=0.05 beta=0.05` (default bounds 0 / 10) and concurrency `nproc`.

fastchess prints "Illegal PV move" warnings occasionally — these are cosmetic (a junk move in the
printed PV tail; the actual best move played is legal).

For an **absolute** rating (rather than a net-vs-net delta), `rate.sh <name:rating:cmd> ...` plays the
engine against one or more externally-rated reference engines (e.g. the CCRL engines bundled with Arena —
which you supply, as they are not in the repo) and reports `anchor_rating + measured_diff` per anchor plus
the mean. It anchors only as well as those opponents and the chosen TC, so treat the result as a ballpark.

## 7. Shipped nets

The repo ships one reference net, **[pawnstar-v8.bin](pawnstar-v8.bin)** (1024-wide, single-bank /
no-king-buckets bullet net; stamped file 1579104 bytes) — loaded at startup as the engine's only evaluator
(resolved relative to the working directory). To use a different net at runtime:

```
setoption name EvalFile value /path/to/other-net.bin
```

**`pawnstar-v8.bin`** is trained on ~2.31B positions of **public PlentyChess** data (bulletformat,
strong-engine labels) with a 1024-wide transformer and **no king buckets** (a single weight bank). It beats
the previous shipped net (v7, 512×4) by **+31.9 ± 16.6 Elo at 40/20** (LLR 2.95, H1 accepted; 972 games,
54.6%) — and at **half the file size** (1.58 MB vs v7's 3.15 MB). At the 2.31B data scale, doubling the
width to 1024 beats king-bucketing: fewer feature rows (768×1 vs 768×4) but a wider hidden layer, which is
both stronger and lighter on the clock. See the lineage below.

**Lineage — what moved the needle (all SPRT-measured):**

| step | change | result |
|---|---|---|
| v1 (256) | Pawnstar's own **HCE self-play** (~3.6M) | **loses** to HCE (−67 Elo; a 1.0M predecessor was −151) |
| v2 (256) | **public PlentyChess** data (~60M) | **beats** HCE ≈ +330 Elo — *label quality*, not quantity, was the lever |
| v3 (256) | ~12× more PlentyChess data (~750M) | **no gain** over v2 (+9.5 ± 13.6, inconclusive) — 256 net is *capacity-limited* |
| v4 (512) | **double the width**, same 60M as v2 | **+55 ± 20 Elo fixed-depth, +71 ± 25 at time control** over v2 — once data saturates, *net size* is the lever (shipped, now retired) |
| v5 (1024) | **double width again**, same 60M | **rejected**: +2 ± 16 fixed-depth (dead even), **−74 ± 25 on the clock** — 60M *saturates* the 512 net; width only pays off with more data. Branch `bignet-1024`, not shipped |
| kb (512 + 4 king buckets) | king buckets, **same 60M** | **flat** (~−8 fixed depth) — the higher-capacity feature set is *data-starved* on 60M |
| v6 (512 + 4 king buckets) | king buckets, **~818M** | **+47 ± 18 fixed-depth, +17 ± 12 on the clock** over v4 — *more data* unlocked the king buckets (shipped, later retired). Needed two engine speed fixes (the eval cache and a single-pass `Update`) to turn an initial −74 clock result positive |
| v7 (512 + 4 king buckets) | **~2.31B** (v6's 818M + 4 more big shards), same arch, SBS 40→120 (~3 epochs) | **+29.96 ± 14.05 fixed-depth, +20.73 ± 12.73 on the clock** over v6 — *still more data*; same arch/speed, so the eval-quality gain carries straight to the clock (later retired) |
| **v8 (1024, no king buckets)** | **double the width, drop king buckets**, same 2.31B as v7 | **+31.9 ± 16.6 Elo at 40/20** over v7 (LLR 2.95, H1; 972 games) — at the full data scale width beats buckets, **at half the file size** (fewer feature rows, wider hidden). Reverses v5's 60M result now that data no longer saturates 1024. **shipped** |

PlentyChess data: <https://huggingface.co/datasets/Yoshie2000/plentychess_data_bulletformat> (public,
already bulletformat — `bullet-utils validate` a shard first (some are corrupt, e.g. `11496.data`),
`cat` a few clean ones together, `shuffle`, then train per §6, skipping the text-`convert` step).

The eval is also fast on the clock: the **incremental accumulator** (~+80 Elo equal-time vs full
refresh), **AVX2 SIMD kernels** (~+180 Elo equal-time vs scalar), and the **eval cache** mean NNUE wins on
time as well as depth — and v8's single-bank table is actually *smaller* than v7's (fewer feature rows), so
it is lighter on memory too. For absolute context the since-removed HCE measured ~2350 Elo (CCRL-ballpark)
and v6/v7 ~2900, so v8 sits a little above that. **The repeated lesson: more data is the lever** — and at
the 2.31B scale the extra width finally pays (v8's 2.31B is still only ~11% of the ~21B available). Next
levers: yet more data, then revisit 1024 *with* king buckets (the two winning levers combined) or a wider
transformer still.

### Recreating `pawnstar-v8.bin` (step by step)

`pawnstar-v8.bin` is a **1024-wide, single-bank (no king buckets)** net trained on the **same ~2.31 billion**
positions as v7 — v6's 818M pool (two big shards `11008`+`13349` + the original 60M) **plus four more big
shards** (`11128`, `12128`, `12932`, `13227`). Only the architecture differs from v7 (wider, no buckets).
The data is already in bulletformat (32-byte `bullet::ChessBoard` records), so the text→`convert` step is
skipped. GPU training is nondeterministic, so this reproduces a *functionally equivalent* net, not a
byte-identical one. The whole pipeline is also captured in the orchestrator
`~/pawnstar_nnue/bignet1024/run.sh` (train → verify → SPRT vs v7).

The key requirement: the **architecture must match everywhere** — `kHiddenSize=1024`, `kNumKingBuckets=1`
and the all-zero `kKingBucketMap` in [src/nnue.h](../src/nnue.h)/[src/nnue.cpp](../src/nnue.cpp) must equal
`HIDDEN_SIZE` and the `KING_BUCKETS` array in [tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs) and
[tools/bullet/pawnstar_eval.rs](../tools/bullet/pawnstar_eval.rs). `setup_bullet.sh` copies those `.rs`
files into bullet, so as long as the repo is on this arch the trainer matches the engine.

```bash
cd /path/to/pawnstar                 # repo root (so ./build and nnue/ paths resolve)
# NOTE: v8 reuses v7's exact 2.31B pool; only the arch changed (1024-wide, single bank / no king buckets).
# If you still have ~/pawnstar_nnue/v7/shuffled.data, skip steps 1-3 and go straight to step 4.
make                                 # build the engine (1024 single-bank arch)
make tests                           # build test_nnue / test_nnue_incremental / test_bratko_kopec_nnue
make tools                           # build build/stamp_net

# 0. One-time: install the bullet trainer (pinned commit) + the examples, build utils + trainer + eval.
tools/setup_bullet.sh
BULLET=~/pawnstar_nnue/bullet
export CUDA_PATH=~/cuda-12.2 PATH="$CUDA_PATH/bin:$PATH" LD_LIBRARY_PATH="$CUDA_PATH/lib64"
( cd "$BULLET/crates/utils"      && cargo build --release )
( cd "$BULLET/crates/bullet_lib" && cargo build --release --features cuda --example pawnstar --example pawnstar_eval )
UTILS="$BULLET/target/release/bullet-utils"
EVAL="$BULLET/target/release/examples/pawnstar_eval"
mkdir -p ~/pawnstar_nnue/v7 && cd ~/pawnstar_nnue/v7

# 1. Download four big PlentyChess shards (~11–12 GB / ~370M positions each), on top of v6's 818M pool.
#    Browse sizes with the HF tree API; the big shards (>=300M positions) are 6974,7074,11008,11128,...,13419.
BASE=https://huggingface.co/datasets/Yoshie2000/plentychess_data_bulletformat/resolve/main
for sh in 11128 12128 12932 13227; do curl -L --fail -C - -o "$sh.data" "$BASE/$sh.data"; done

# 2. Validate each (size % 32 == 0, no invalid records). Some shards are corrupt (e.g. 11496.data),
#    so ALWAYS validate before training; drop and replace any that fail.
for sh in 11128 12128 12932 13227; do "$UTILS" validate --input "$sh.data"; done

# 3. Concatenate v6's shuffled pool + the new shards (flat 32-byte records) and re-shuffle. (v6's
#    shuffled.data already contains 11008+13349+60M, so the result is a clean SUPERSET of v6's data.)
cat ~/pawnstar_nnue/kbbig/shuffled.data 11128.data 12128.data 12932.data 13227.data > all.data
"$UTILS" shuffle --input all.data --output shuffled.data --mem-used-mb 8192
echo "pool: $(( $(stat -c%s shuffled.data) / 32 )) positions"   # ~2.31e9
rm -f all.data 1[12]*.data 132*.data                            # reclaim disk

# 4. Train the 1024-wide single-bank (no king buckets) arch on the same pool/schedule as v7 — only the
#    arch differs. BPS=3688 (= 60M/16384) and SBS=120 => ~7.25B samples seen (~3 epochs of the 2.31B
#    pool). LR schedule auto-scales (StepLR step = SBS/3). ~2-3 h on a GTX 1050 Ti.
( cd "$BULLET/crates/bullet_lib" && \
  PAWNSTAR_DATA=~/pawnstar_nnue/v7/shuffled.data PAWNSTAR_BPS=3688 \
  PAWNSTAR_SBS=120 PAWNSTAR_OUT=~/pawnstar_nnue/v7/checkpoints \
  cargo run --release --features cuda --example pawnstar )   # drop --features cuda to train on CPU

# 5. Keep the RAW quantised net AND a stamped copy. The raw net is what bullet writes; the stamped net
#    (32-byte NetHeader prepended, 1579104 bytes) is what ships. stamp_net embeds this build's arch (§2).
RAW=~/pawnstar_nnue/v7/checkpoints/pawnstar-120/quantised.bin
./build/stamp_net "$RAW" /path/to/pawnstar/nnue/pawnstar-v8.bin

# 6. Verify (§6). GOTCHA: pawnstar_eval is NOT header-aware, so generate the reference from the *RAW*
#    net; test_nnue / test_nnue_incremental are header-aware and accept either. (Stamping before the
#    cross-check makes pawnstar_eval misread the 32-byte header -> spurious failures.)
cd /path/to/pawnstar
cut -d'|' -f1 test/nnue_reference.txt > /tmp/fens.txt          # reuse the existing FENs (first field)
"$EVAL" "$RAW" /tmp/fens.txt > test/nnue_reference.txt          # re-baseline the checked-in reference (RAW net)
./build/test_nnue nnue/pawnstar-v8.bin test/nnue_reference.txt  # EvaluateExact==trainer <=2 cp; int8 <=40 cp
./build/test_nnue_incremental nnue/pawnstar-v8.bin             # incremental accumulator == full refresh

# 7. Regenerate the Bratko-Kopec accepted moves for the new net (the eval changed), then full check.
#    The search is single-threaded/deterministic per depth; the accepted set is the union over depths 8-11.
#    (Transcribe each position's got= move into the kCases array in test/bratko_kopec_nnue_test.cpp, then
#    verify 24/24 at each depth.)
for d in 8 9 10 11; do ./build/test_bratko_kopec_nnue nnue/pawnstar-v8.bin $d | grep -E 'pos|solved'; done
make clean && make && make check  # `make` rebuilds the engine, re-embedding the new net (embedded_net.S
                                  # .incbin's the EMBED_NET path); `make check` then runs all suites green
                                  # (check builds tests+tools, not the engine, so build the engine first)

# 8. Strength-test vs the previous net (BASELINE_NET) at fixed depth and time control.
BASELINE_NET=/path/to/previous-net.bin tools/run_sprt.sh nnue/pawnstar-v8.bin /path/to/openings.epd 1000 8
BASELINE_NET=/path/to/previous-net.bin TC=8+0.08 tools/run_sprt.sh nnue/pawnstar-v8.bin /path/to/openings.epd 1000
```

To train a **stronger** net, add yet more clean PlentyChess shards at steps 1–3 (the dataset has ~168
shards ≈ 21B positions; `validate` each first) and scale `SBS` to keep ~3 epochs. To change the
**architecture** — width (`kHiddenSize`), bucket count (`kNumKingBuckets`) or the `kKingBucketMap` —
change it identically in `nnue.h`/`nnue.cpp`, `pawnstar.rs` and `pawnstar_eval.rs`, rebuild the engine and
the bullet examples, and retrain. A different architecture yields a different file size, so an engine
built for one cannot load a net of another (§2).

## 8. Reproducibility

`setup_bullet.sh` pins bullet to commit `8893e489…` (bullet's API drifts; `tools/bullet/*.rs` target
that commit). It is idempotent: re-running clones if needed, checks out the pinned commit, reinstalls
the example sources, and registers them in `crates/bullet_lib/Cargo.toml` only if absent. The default
working area is `~/pawnstar_nnue/` (bullet checkout, datasets, checkpoints); override the bullet
location with `BULLET=…`.

## 9. Troubleshooting

- **`undefined symbol: cuFuncLoad` when building bullet with `--features cuda`** — the NVIDIA driver is
  too old (it lacks the CUDA ≥ 12.3 driver API). Upgrade the driver to ≥ 545, or train on CPU with
  `BULLET_FEATURES=""`.
- **`NNUE: file is …, engine expects …`** (stamped net) or **`NNUE: net '…' is N bytes, expected 1579010
  (raw, unstamped) — wrong architecture or format`** (raw net) — the file doesn't match this build's
  architecture (wrong `HIDDEN_SIZE`, bucket count, quantisation, or a truncated/corrupt file). Both are
  hard rejections: a *stamped* net is validated field-by-field against the engine's compile-time constants,
  and a *raw* net must match the payload size **exactly** (the window `kNetFileBytes ≤ size < kNetFileBytes
  +64`, §2). So an incompatible net is never silently misread — including a *larger* (e.g. wider) one, the
  case the old `>=` floor missed. Stamp nets with `tools/stamp_net` so the rejection message is precise.
- **Engine "disconnects" mid-match** — the old long-game history overflow is fixed (the game history is
  a dynamic `std::vector` now), so this should be rare; if it recurs it is an actual crash worth a debug
  build. Adjudication (`run_sprt.sh`) is still recommended to keep matches short and decisive.
- **bullet build links `-lcuda` against the system driver** (`/usr/lib/x86_64-linux-gnu/libcuda.so`),
  not the toolkit, so the *driver* version is what gates `cuFuncLoad`; a newer toolkit alone won't help.
