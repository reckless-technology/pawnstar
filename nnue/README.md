# NNUE evaluation (experimental)

Pawnstar has an **NNUE** (Efficiently Updatable Neural Network) evaluation alongside the hand-crafted
evaluation (HCE). It is **on by default** — at startup the engine loads the shipped net
`nnue/pawnstar-v6.bin` (relative to the working directory) and evaluates with it, falling back to the
HCE if the net is absent or NNUE is disabled. This document is the complete reference for how the net
works, how to enable/disable it, and how to generate data, train, validate and strength-test a net.

The shipped net (`pawnstar-v6.bin`) is a 512-wide, **4-king-bucket** `ChessBuckets` net. §1, §2 and §6
describe that architecture; §7 has the lineage (including the retired Chess768 nets and the rejected
1024-wide experiment).

In-engine code: [src/nnue.h](../src/nnue.h), [src/nnue.cpp](../src/nnue.cpp). The branch into NNUE is in
[src/evaluation.cpp](../src/evaluation.cpp) (`EvaluatePosition`). Tooling: [tools/](../tools).

---

## 1. Architecture

A "perspective" network using bullet's king-bucketed `ChessBuckets` feature set:

```
768 inputs per (perspective, king bucket)  ->  512 feature transformer (weights shared across perspectives)
SCReLU,  concat[side-to-move | opponent] = 1024  ->  1 output  (centipawns, side-to-move relative)
```

Constants (must match the trainer): `kInputSize=768` (features per bucket), `kNumKingBuckets=4`,
`kFeatureRows=768*4=3072`, `kHiddenSize=512`, `kQA=255`, `kQB=64`, `kScale=400`.

**Features.** The 768 base inputs per perspective are `colour (2) × piece type (6) × square (64)`. Each
perspective then selects one of **4 weight banks** by *its own* king's square (in that perspective's
orientation) through the 64-entry `kKingBucketMap` (a file/rank quadrant: files a–d vs e–h, own half
ranks 1–4 vs far half 5–8). For a piece of `colour` (0=white, 1=black), type `pt` (pawn=1 … king=6) on
`square`, with `wb`/`bb` the white/black king buckets:

```
white-perspective row = wb*768 + colour     * 384 + (pt-1)*64 + square
black-perspective row = bb*768 + (1-colour) * 384 + (pt-1)*64 + (square ^ 0x38)   # 0x38 flips the rank
```

Two accumulators are built (white-perspective and black-perspective), each seeded with `feature_bias`
and then summing the feature column of every piece in its king bucket's bank. At evaluation the
**side-to-move** accumulator is fed to the first 512 output weights and the opponent's to the second
512. This is mathematically identical to bullet's `ChessBuckets::map_features` with the same bucket map
(verified by the cross-check in §6). The bucket map MUST stay byte-identical across `src/nnue.cpp`,
`tools/bullet/pawnstar.rs` and `tools/bullet/pawnstar_eval.rs`.

**Forward pass** (mirrors bullet's `simple` example exactly; `screlu(x) = clamp(x,0,QA)²`):

```
out  = Σ screlu(stm[i])·w_stm[i]  +  Σ screlu(ntm[i])·w_ntm[i]   # i64
out /= QA                 # SCReLU leaves QA*QA*QB units; reduce one QA
out += output_bias        # bias is in QA*QB units
out *= SCALE
out /= QA * QB            # -> centipawns, side-to-move relative
```

The net is an `nnue::Network` class (the quantised weights plus `Load`/`Refresh`/`Update`/`Evaluate`
methods), **owned by the `Game`** — there are no nnue globals. Its accumulator is **maintained
incrementally** across make/undo on each thread's `SearchState`: `Network::Update` applies only the
feature-column deltas for pieces whose placement changed within a king bucket (a parent/child bitboard
diff — reversible on undo, a no-op for null moves). When a king move **crosses a bucket boundary** that
whole perspective's bank changes, so `Update` rebuilds just that one perspective from scratch (at most
one king moves per ply, so at most one perspective refreshes per update); `Network::Refresh` rebuilds
both at the root.
`Network::Evaluate(position)` (a full refresh) is the reference the incremental path is checked against
(§6). Both kernels (accumulator update and forward pass) are **AVX2-vectorised** when built with
`-mavx2`, with scalar fallbacks behind `#if defined(__AVX2__)`; the vectorised path is bit-identical to
the scalar one (verified by `test_nnue`).

## 2. File format

The **payload** is bullet's native quantised weights — tightly packed, little-endian `int16`, in this order:

| section | count (int16) | scale |
|---|---|---|
| `feature_weights` | `768*4 * 512 = 1572864` (column-major: `row*512 + i`, rows bucket-major) | QA |
| `feature_bias` | `512` | QA |
| `output_weights` | `2 * 512 = 1024` (first 512 = stm, next 512 = ntm) | QB |
| `output_bias` | `1` (scalar) | QA·QB |

Packed payload is `(1572864 + 512 + 1024 + 1) * 2 = 3148802` bytes; bullet pads the trailing 1-element
`output_bias` tensor up to a 64-byte boundary, so its raw output is **3148864 bytes**.

**Self-describing header.** A shipped net is prepended with a **32-byte `NetHeader`** (`src/nnue.h`):
a magic (`"PSN1"`), a format version, and the architecture parameters — `kInputSize`, `kNumKingBuckets`,
`kHiddenSize`, `kQA`, `kQB`, `kScale`. So `nnue/pawnstar-v6.bin` is **3148896 bytes** (32 + 3148864). The
loader (`Network::Load`) accepts two layouts:

- **Stamped** (magic present): every architecture field is checked against the engine's compile-time
  constants; any mismatch is rejected with a descriptive message (e.g. `file is …buckets1…, engine
  expects …buckets4…`) — so an incompatible net can never be silently misread.
- **Raw** (no magic, e.g. a freshly-exported bullet `quantised.bin`): accepted only when its size equals
  the payload exactly, within bullet's `<64`-byte padding (`kNetFileBytes ≤ size < kNetFileBytes+64`).
  This exact window — not a `>=` floor — is what rejects a *larger* (e.g. wider) raw net rather than
  misreading it.

Stamp a raw net with `tools/stamp_net` (`make tools`); the header values come from the building engine's
constants, so it can only stamp a net that matches this build. The forward-pass math and quantisation are
kept in lock-step with the trainer ([tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs)); §6 verifies
they agree to within rounding. (The retired Chess768/512 nets — v4 and earlier — had a 789506-byte payload
and were headerless; a king-bucket engine cannot load them.)

## 3. Using a net in the engine

```
setoption name EvalFile value /path/to/net.bin
setoption name UseNNUE  value true
```

or via environment variables at launch:

```bash
PAWNSTAR_EVALFILE=/path/to/net.bin PAWNSTAR_NNUE=1 ./build/pawnstar
```

NNUE is **on by default** — `main.cpp` loads `nnue/pawnstar-v6.bin` (cwd-relative) and enables it at
startup, so the commands above are only needed to use a *different* net or to disable NNUE
(`setoption name UseNNUE value false`, or `PAWNSTAR_NNUE=0`). `UseNNUE`/`EvalFile` are advertised in the
`uci` response (UseNNUE default true). The `eval` command reports which evaluator is active; `nnue`
prints the raw network eval of the current position. The net is a single
`nnue::Network` instance owned by the `Game` (UCI `setoption` routes to `game.NnueNetwork().Load(...)`
and `game.SetUseNnue(...)`), read-only after load and shared by all Lazy SMP threads through their
`Game&` — they call only its `const` methods.

## 4. Tooling

| file | purpose |
|------|---------|
| `gen_data.cpp`            | self-play data generator (built by `make tools` → `build/gen_data`) |
| `stamp_net.cpp`           | prepend the self-describing `NetHeader` to a raw bullet net (`make tools` → `build/stamp_net`) |
| `run_gendata.sh`          | launch N parallel self-play shards |
| `setup_bullet.sh`         | clone bullet at the pinned commit + install/register our trainer examples |
| `bullet/pawnstar.rs`      | the trainer (arch `768×4 king-bucketed → 512×2 → 1`, SCReLU, QA/QB/SCALE) |
| `bullet/pawnstar_eval.rs` | reference evaluator for the `test_nnue` cross-check |
| `make_openings.sh`        | build an EPD openings book from self-play data |
| `train_pipeline.sh`       | train → export a net, from either self-play text shards or a bulletformat `.data` |
| `verify_net.sh`           | cross-check (engine == trainer) + incremental-accumulator gate for a trained net |
| `run_sprt.sh`             | SPRT / match: NNUE vs HCE (default) or net-vs-net, at fixed depth or time control |
| `rate.sh`                 | anchored absolute-Elo estimate vs rated reference engines (you supply the opponent binaries) |
| `run_experiment.sh`       | one-shot: openings + train + **verify** + SPRT from an existing dataset |

**Prerequisites:** `clang++`/`make` (engine), Rust/`cargo` (bullet), `fastchess` on `PATH` (SPRT). For
GPU training, a CUDA toolkit (`CUDA_PATH`, default `~/cuda-12.2`) **and** a driver new enough for the
toolkit: bullet's CUDA backend calls `cuFuncLoad`, which needs a driver supporting CUDA ≥ 12.3
(driver ≥ 545). Otherwise train on CPU with `BULLET_FEATURES=""` — the net is small enough that CPU
training is practical.

## 5. Quick start (minimal retrain)

```bash
tools/setup_bullet.sh                                   # once: clone + register bullet (pinned commit)
make tools                                              # build build/gen_data
tools/run_gendata.sh ~/pawnstar_nnue/data 12 100000 8   # 12 procs, depth-8 self-play (pkill -x gen_data to stop)
tools/run_experiment.sh ~/pawnstar_nnue/data net.bin    # openings + train + SPRT  ->  net.bin
```

Then load `net.bin` in the engine (§3). To train on CPU: `BULLET_FEATURES="" tools/run_experiment.sh …`.
`run_experiment.sh <data_dir | shuffled.data> <net.bin> [superbatches=40] [sprt_rounds=500] [depth=8]`
chains `make_openings.sh` → `train_pipeline.sh` → `verify_net.sh` → `run_sprt.sh`. The first argument
may also be an already-converted bulletformat `.data` file (how the shipped nets were trained — see §7);
in that case there are no text shards to sample openings from, so supply your own with `OPENINGS=…`.

## 6. Step-by-step

### Data generation

`build/gen_data <out_file> <num_games> <seed> [depth=8] [random_plies=8]` plays self-play games with the
**current** evaluator and writes one record per quiet position:

```
<fen> | <white_relative_eval_cp> | <wdl>        # wdl: 1.0 win / 0.5 draw / 0.0 loss, White's POV
```

This is exactly the text format bullet's `convert --from text` expects (white-relative score and
result). Details:

- The search runs **single-threaded** (the driver forces `PAWNSTAR_THREADS=1`), so each process pins
  one core and games are deterministic for a given seed — run many processes for throughput.
- `random_plies` random opening moves per game add diversity (not recorded).
- Only **quiet, not-in-check** positions with `|eval| < 3000` cp are recorded (clean targets).
- Games end on checkmate/stalemate/50-move/repetition/insufficient-material, on resign adjudication
  (`|score| > 1500` for 4 consecutive plies), or at a 200-ply cap (a data-quality bound — the engine's
  game history is a dynamic `std::vector` now, so game length is no longer a correctness limit).

`run_gendata.sh <out_dir> <num_processes> <games_per_process> <depth> [random_plies]` launches the
shards (`data_<i>.txt` + `log_<i>.txt`). Throughput at depth 8 is ~8–9 positions/sec/core
(~150k/hour across 12 cores); lower the depth for more, noisier data. Stop with `pkill -x gen_data` —
partial shards are valid. `train_pipeline.sh` combines both freshly generated `data_*.txt` and any
preserved `seed*.txt` shards in the directory, so you can keep earlier data by renaming its shards to
`seed*.txt`.

### Training

`train_pipeline.sh <data_dir | shuffled.data> <out_net.bin> [end_superbatch=40]` auto-detects its input
from the first argument:

1. ensures bullet is set up, builds `bullet-utils` and the trainer (`$BULLET_FEATURES`, default
   `--features cuda`);
2. **if the argument is a `.data` file** (already bulletformat — e.g. public PlentyChess data, the way
   the shipped nets were trained), it is used directly and steps 3–4 are skipped; **if it is a
   directory**, the self-play shards `data_*.txt` + `seed*.txt` are combined → `all.txt`,
   `bullet-utils convert --from text` → `all.data` (bullet `ChessBoard`, 32 bytes/record), then
   `bullet-utils shuffle` → `shuffled.data` (self-play data is highly correlated — shuffling matters);
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

`make_openings.sh <data_dir> [out.epd] [count=1000]` samples distinct early-game positions
(`fullmove <= 5`) from the shards into an EPD book, so matches start from varied, roughly balanced
positions. Use at least `sprt_rounds` openings to avoid repeats.

### Validate inference (cross-check)

The engine's inference must reproduce the trainer's. Build the reference evaluator, sample some FENs
(include both side-to-move colours), and compare:

```bash
# build the bullet-side reference evaluator (CPU is fine; add --features cuda to reuse the GPU build)
(cd ~/pawnstar_nnue/bullet/crates/bullet_lib && cargo build --release --example pawnstar_eval)
EVAL=~/pawnstar_nnue/bullet/target/release/examples/pawnstar_eval

# sample FENs from the data, get reference evals, compare with the engine
awk -F' \\| ' 'NR%500==0 {print $1}' ~/pawnstar_nnue/data/data_*.txt | head -200 > fens.txt
"$EVAL" net.bin fens.txt > ref.txt
./build/test_nnue net.bin ref.txt        # expects: max |diff| <= 2 cp (quantisation rounding)
```

`pawnstar_eval` uses bullet's own feature extraction (`ChessBuckets` on this branch, `Chess768` on
`main`) and the same SCReLU inference, so a match confirms our feature indexing, king bucketing,
perspective/orientation, SCReLU and dequantisation are all correct. It must be built at the same width
and feature set as the engine, or the cross-check fails by design.

The repo checks in `test/nnue_reference.txt` — 250 reference evals for the shipped `pawnstar-v6.bin` —
and `make check` runs `test_nnue nnue/pawnstar-v6.bin test/nnue_reference.txt` automatically (current
engine: max |diff| 0 cp). The reference's first field is the FEN, so regenerate it after shipping a *new*
net with: `cut -d'|' -f1 test/nnue_reference.txt > fens.txt && "$EVAL" nnue/pawnstar-v6.bin fens.txt >
test/nnue_reference.txt`. With no arguments `test_nnue` is a no-op, so `make check` stays green if the
net/reference are absent.

A second test, `test_nnue_incremental <net.bin>`, plays random move sequences through a `SearchState`
and asserts the incrementally-maintained accumulator evaluates **bit-identically** to a full refresh at
every node (and again after every undo, to catch reverse-update bugs). With no argument it is a no-op,
so `make check` stays green; run it with a net (e.g. `./build/test_nnue_incremental nnue/pawnstar-v6.bin`)
after any change to the accumulator, feature indexing, or make/undo.

`verify_net.sh <net.bin>` bundles both checks into one gate: it regenerates the reference evals for
*that* net (reusing the FENs from `test/nnue_reference.txt`) with `pawnstar_eval`, then runs `test_nnue`
and `test_nnue_incremental`. `run_experiment.sh` runs it between training and the SPRT — a net that
fails it has a width/format mismatch, so any match result would be meaningless. Run it standalone after
training a net outside the one-shot flow.

### Strength testing (the verdict)

NNUE scores differ from the HCE reference scores, so `test_bratko_kopec` stays an **HCE-only** test
(run with `UseNNUE` off; do not retune its references for NNUE). Real strength is measured by game
play. `run_sprt.sh <net.bin> <openings.epd> [rounds=500] [depth=8]` runs:

- the candidate as NNUE with `<net.bin>`; the **opponent is HCE** by default (`UseNNUE=false`). Set
  `BASELINE_NET=<other.bin>` to instead compare **two nets** — the architecture program's actual test
  (e.g. v5(1024) vs v4(512)), since NNUE now beats HCE and ships on by default;
- the **same** `build/pawnstar` binary for both engines, differing only by UCI options
  (`UseNNUE`/`EvalFile`), with `PAWNSTAR_THREADS=1`. Different net *widths* need different builds
  (`kHiddenSize` is compile-time), so a cross-width SPRT points `BIN_A`/`BIN_B` at two separately-built
  binaries;
- **fixed depth** (default 8) — pawnstar has no `go nodes`, and fixed depth isolates eval *quality* from
  NNUE's per-node speed. To measure the equal-*time* effect (incremental vs full-refresh, a wider net's
  speed cost, or NNUE vs HCE on the clock) set `TC=8+0.08` for a time-control match instead;
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

The repo ships one reference net, **[pawnstar-v6.bin](pawnstar-v6.bin)** (512-wide, 4-king-bucket bullet
net, 3148864 bytes) — loaded and used **by default** at startup (the engine resolves it relative to the
working directory). To use a different net, or to switch evaluators at runtime:

```
setoption name EvalFile value nnue/pawnstar-v6.bin
setoption name UseNNUE  value true
```

**`pawnstar-v6.bin`** is trained on ~818M positions of **public PlentyChess** data (bulletformat,
strong-engine labels) with a 512-wide transformer and 4 king buckets. It beats the handcrafted eval by a
wide margin and beats the previous shipped net (v4) by **+47 ± 18 Elo at fixed depth and +17 ± 12 on the
clock** — see the lineage below.

**Lineage — what moved the needle (all SPRT-measured):**

| step | change | result |
|---|---|---|
| v1 (256) | Pawnstar's own **HCE self-play** (~3.6M) | **loses** to HCE (−67 Elo; a 1.0M predecessor was −151) |
| v2 (256) | **public PlentyChess** data (~60M) | **beats** HCE ≈ +330 Elo — *label quality*, not quantity, was the lever |
| v3 (256) | ~12× more PlentyChess data (~750M) | **no gain** over v2 (+9.5 ± 13.6, inconclusive) — 256 net is *capacity-limited* |
| v4 (512) | **double the width**, same 60M as v2 | **+55 ± 20 Elo fixed-depth, +71 ± 25 at time control** over v2 — once data saturates, *net size* is the lever (shipped, now retired) |
| v5 (1024) | **double width again**, same 60M | **rejected**: +2 ± 16 fixed-depth (dead even), **−74 ± 25 on the clock** — 60M *saturates* the 512 net; width only pays off with more data. Branch `bignet-1024`, not shipped |
| kb (512 + 4 king buckets) | king buckets, **same 60M** | **flat** (~−8 fixed depth) — the higher-capacity feature set is *data-starved* on 60M |
| **v6 (512 + 4 king buckets)** | king buckets, **~818M** | **+47 ± 18 fixed-depth, +17 ± 12 on the clock** over v4 — *more data* unlocked the king buckets; **shipped**. (Needed two engine speed fixes — the eval cache and a single-pass `Update` — to turn an initial −74 clock result positive.) |

PlentyChess data: <https://huggingface.co/datasets/Yoshie2000/plentychess_data_bulletformat> (public,
already bulletformat — `bullet-utils validate` a shard first (some are corrupt, e.g. `11496.data`),
`cat` a few clean ones together, `shuffle`, then train per §6, skipping the text-`convert` step).

The eval is also fast on the clock: the **incremental accumulator** (~+80 Elo equal-time vs full
refresh), **AVX2 SIMD kernels** (~+180 Elo equal-time vs scalar), and the **eval cache** + **single-pass
`Update`** (which closed the king-bucket speed gap to v4 from ~2.2× to ~1.18×) mean v6 wins on time as
well as depth despite the 4× larger feature table. For absolute context the HCE measures ~2350 Elo
(CCRL-ballpark) and v4 ~2900, so the NNUE is a large step up — which is why it is the engine's **default**
evaluator (disable with `UseNNUE false` / `PAWNSTAR_NNUE=0`). Next lever: more data still (v6 used ~818M;
the dataset has ~21B positions available), and/or more/finer king buckets now that buckets have paid off.

### Recreating `pawnstar-v6.bin` (step by step)

`pawnstar-v6.bin` is a **512-wide, 4-king-bucket** net trained on ~818M positions of **public
PlentyChess** data (two big shards + the original 60M). The data is already in bulletformat (32-byte
`bullet::ChessBoard` records), so the text→`convert` step is skipped. GPU training is nondeterministic,
so this reproduces a *functionally equivalent* net, not a byte-identical one.

The key requirement: the **architecture must match everywhere** — `kHiddenSize=512`, `kNumKingBuckets=4`
and the `kKingBucketMap` in [src/nnue.h](../src/nnue.h)/[src/nnue.cpp](../src/nnue.cpp) must equal
`HIDDEN_SIZE` and the `KING_BUCKETS` array in [tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs) and
[tools/bullet/pawnstar_eval.rs](../tools/bullet/pawnstar_eval.rs). `setup_bullet.sh` copies those `.rs`
files into bullet, so as long as the repo is on the king-bucket arch the trainer matches the engine.

```bash
cd /path/to/pawnstar                 # repo root (so ./build and nnue/ paths resolve)
make                                 # build the engine (king-bucket arch)
make tests                           # build test_nnue / test_nnue_incremental

# 0. One-time: install the bullet trainer (pinned commit) + the examples, build utils + trainer + eval.
tools/setup_bullet.sh
BULLET=~/pawnstar_nnue/bullet
export CUDA_PATH=~/cuda-12.2 PATH="$CUDA_PATH/bin:$PATH" LD_LIBRARY_PATH="$CUDA_PATH/lib64"
( cd "$BULLET/crates/utils"      && cargo build --release )
( cd "$BULLET/crates/bullet_lib" && cargo build --release --features cuda --example pawnstar --example pawnstar_eval )
UTILS="$BULLET/target/release/bullet-utils"
EVAL="$BULLET/target/release/examples/pawnstar_eval"
mkdir -p ~/pawnstar_nnue/v6 && cd ~/pawnstar_nnue/v6

# 1. Download two big PlentyChess shards (~12 GB / ~378M positions each) + reuse the original 60M.
BASE=https://huggingface.co/datasets/Yoshie2000/plentychess_data_bulletformat/resolve/main
curl -L -o 11008.data "$BASE/11008.data"     # ~379M positions
curl -L -o 13349.data "$BASE/13349.data"     # ~378M positions
#    (the 60M set was 11848.data + 12892.data; together ≈ 818M positions)

# 2. Validate the format (size % 32 == 0; no invalid records). Some shards are corrupt
#    (e.g. 11496.data fails this), so always validate before training.
"$UTILS" validate --input 11008.data
"$UTILS" validate --input 13349.data

# 3. Concatenate (flat 32-byte records) and shuffle.
cat 11008.data 13349.data /path/to/60M/all.data > all.data
"$UTILS" shuffle --input all.data --output shuffled.data --mem-used-mb 8192

# 4. Train at CONSTANT compute: 40 superbatches of BPS=3688 (= 60M/16384) = 2.4B samples seen, drawn from
#    the bigger pool (~3 epochs over 818M, not 40 over 60M). ~45 min on a GTX 1050 Ti. Keeping BPS at the
#    60M value (NOT positions/16384) is what keeps training time ~constant as the pool grows.
( cd "$BULLET/crates/bullet_lib" && \
  PAWNSTAR_DATA=~/pawnstar_nnue/v6/shuffled.data PAWNSTAR_BPS=3688 \
  PAWNSTAR_SBS=40 PAWNSTAR_OUT=~/pawnstar_nnue/v6/checkpoints \
  cargo run --release --features cuda --example pawnstar )   # drop --features cuda to train on CPU

# 5. Stamp the raw net with the self-describing header and ship it (stamped file is 3148896 bytes).
#    stamp_net embeds this build's architecture, so it can only stamp a net matching the engine (§2).
make tools                           # builds build/stamp_net
./build/stamp_net ~/pawnstar_nnue/v6/checkpoints/pawnstar-40/quantised.bin /path/to/pawnstar/nnue/pawnstar-v6.bin

# 6. Regenerate the engine-vs-trainer cross-check reference for the new net (§6), then verify.
cd /path/to/pawnstar
cut -d'|' -f1 test/nnue_reference.txt > /tmp/fens.txt      # reuse the existing FENs (first field)
"$EVAL" nnue/pawnstar-v6.bin /tmp/fens.txt > test/nnue_reference.txt   # re-baseline the checked-in reference
./build/test_nnue nnue/pawnstar-v6.bin test/nnue_reference.txt   # expect max |diff| <= 2 cp (engine == trainer)
./build/test_nnue_incremental nnue/pawnstar-v6.bin              # incremental accumulator == full refresh
make check                                                      # all suites, including the NNUE ones
# (tools/verify_net.sh nnue/pawnstar-v6.bin runs the two test_nnue checks in one step, against a
#  throwaway reference — but shipping a new net still needs the re-baseline line above to update
#  the checked-in test/nnue_reference.txt.)

# 7. (optional) Strength-test vs the previous net (set BASELINE_NET) at fixed depth and time control.
BASELINE_NET=/path/to/old_net.bin tools/run_sprt.sh nnue/pawnstar-v6.bin /path/to/openings.epd
```

To train a **stronger** net, add more clean PlentyChess shards at steps 1–3 (the dataset has ~168 shards
≈ 21B positions; `validate` each first). To change the **architecture** — width (`kHiddenSize`), bucket
count (`kNumKingBuckets`) or the `kKingBucketMap` — change it identically in `nnue.h`/`nnue.cpp`,
`pawnstar.rs` and `pawnstar_eval.rs`, rebuild the engine and the bullet examples, and retrain. A
different architecture yields a different file size, so an engine built for one cannot load a net of
another (§2).

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
- **`NNUE: net '…' is N bytes, expected at least 3148802`** — the file isn't a 512-wide / 4-king-bucket
  bullet net in the expected format (wrong `HIDDEN_SIZE`, wrong bucket count, wrong architecture, or
  truncated). Note the size-gate only catches nets that are *too small*; a *larger* (e.g. wider) net is
  silently misread, so always build the engine/tests at the net's architecture before verifying.
- **Engine "disconnects" mid-match** — the old long-game history overflow is fixed (the game history is
  a dynamic `std::vector` now), so this should be rare; if it recurs it is an actual crash worth a debug
  build. Adjudication (`run_sprt.sh`) is still recommended to keep matches short and decisive.
- **bullet build links `-lcuda` against the system driver** (`/usr/lib/x86_64-linux-gnu/libcuda.so`),
  not the toolkit, so the *driver* version is what gates `cuFuncLoad`; a newer toolkit alone won't help.
