# NNUE evaluation (experimental)

Pawnstar has an **NNUE** (Efficiently Updatable Neural Network) evaluation alongside the hand-crafted
evaluation (HCE). It is **on by default** — at startup the engine loads the shipped net
`nnue/pawnstar-v4.bin` (relative to the working directory) and evaluates with it, falling back to the
HCE if the net is absent or NNUE is disabled. This document is the complete reference for how the net
works, how to enable/disable it, and how to generate data, train, validate and strength-test a net.

In-engine code: [src/nnue.h](../src/nnue.h), [src/nnue.cpp](../src/nnue.cpp). The branch into NNUE is in
[src/evaluation.cpp](../src/evaluation.cpp) (`EvaluatePosition`). Tooling: [tools/](../tools).

---

## 1. Architecture

A simple "perspective" network using bullet's standard `Chess768` feature set:

```
768 inputs per perspective  ->  512 feature transformer (weights shared across both perspectives)
SCReLU,  concat[side-to-move | opponent] = 1024  ->  1 output  (centipawns, side-to-move relative)
```

Constants (must match the trainer): `kInputSize=768`, `kHiddenSize=512`, `kQA=255`, `kQB=64`,
`kScale=400`.

**Features.** The 768 inputs per perspective are `colour (2) × piece type (6) × square (64)`. For a
piece of `colour` (0=white, 1=black), type `pt` (pawn=1 … king=6) on `square`:

```
white-perspective index = colour     * 384 + (pt-1)*64 + square
black-perspective index = (1-colour) * 384 + (pt-1)*64 + (square ^ 0x38)   # 0x38 flips the rank
```

Two accumulators are built (white-perspective and black-perspective), each seeded with `feature_bias`
and then summing the feature column of every piece. At evaluation the **side-to-move** accumulator is
fed to the first 512 output weights and the opponent's to the second 512. This is mathematically
identical to bullet's `Chess768::map_features` (verified by the cross-check in §6).

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
feature-column deltas for pieces whose placement changed (a parent/child bitboard diff — reversible on
undo, a no-op for null moves), while `Network::Refresh` rebuilds it from scratch at the root.
`Network::Evaluate(position)` (a full refresh) is the reference the incremental path is checked against
(§6). Both kernels (accumulator update and forward pass) are **AVX2-vectorised** when built with
`-mavx2`, with scalar fallbacks behind `#if defined(__AVX2__)`; the vectorised path is bit-identical to
the scalar one (verified by `test_nnue`).

## 2. File format

The engine loads bullet's **native quantised `.bin`** directly — headerless, tightly packed,
little-endian `int16`, in this order:

| section | count (int16) | scale |
|---|---|---|
| `feature_weights` | `768 * 512 = 393216` (column-major: `feature*512 + i`) | QA |
| `feature_bias` | `512` | QA |
| `output_weights` | `2 * 512 = 1024` (first 512 = stm, next 512 = ntm) | QB |
| `output_bias` | `1` (scalar) | QA·QB |

Packed size is `(393216 + 512 + 1024 + 1) * 2 = 789506` bytes. bullet pads the trailing 1-element
`output_bias` tensor up to a 64-byte boundary, so its files are **789568 bytes**; the loader requires
`size >= 789506` and ignores any trailing padding. A size below that is rejected (wrong hidden size /
format). The forward-pass math and quantisation are kept in lock-step with the trainer
([tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs)); §6 verifies they agree to within rounding.

## 3. Using a net in the engine

```
setoption name EvalFile value /path/to/net.bin
setoption name UseNNUE  value true
```

or via environment variables at launch:

```bash
PAWNSTAR_EVALFILE=/path/to/net.bin PAWNSTAR_NNUE=1 ./build/pawnstar
```

NNUE is **on by default** — `main.cpp` loads `nnue/pawnstar-v4.bin` (cwd-relative) and enables it at
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
| `run_gendata.sh`          | launch N parallel self-play shards |
| `setup_bullet.sh`         | clone bullet at the pinned commit + install/register our trainer examples |
| `bullet/pawnstar.rs`      | the trainer (arch `768→512×2→1`, SCReLU, QA/QB/SCALE) |
| `bullet/pawnstar_eval.rs` | reference evaluator for the `test_nnue` cross-check |
| `make_openings.sh`        | build an EPD openings book from self-play data |
| `train_pipeline.sh`       | combine → convert → shuffle → train → export the net |
| `run_sprt.sh`             | SPRT / match of NNUE vs the hand-crafted eval |
| `run_experiment.sh`       | one-shot: openings + train + SPRT from an existing dataset |

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
`run_experiment.sh <data_dir> <net.bin> [superbatches=40] [sprt_rounds=500] [depth=8]` simply chains
`make_openings.sh` → `train_pipeline.sh` → `run_sprt.sh`.

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

`train_pipeline.sh <data_dir> <out_net.bin> [end_superbatch=30]`:

1. combines `data_*.txt` + `seed*.txt` → `all.txt`;
2. ensures bullet is set up, builds `bullet-utils` and the trainer (`$BULLET_FEATURES`, default
   `--features cuda`);
3. `bullet-utils convert --from text` → `all.data` (bullet `ChessBoard`, 32 bytes/record);
4. `bullet-utils shuffle` → `shuffled.data` (self-play data is highly correlated — shuffling matters);
5. trains, computing `batches_per_superbatch = positions / 16384` so a superbatch ≈ one epoch;
6. copies `<data_dir>/checkpoints/pawnstar-<end_superbatch>/quantised.bin` → `out_net.bin`.

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

`pawnstar_eval` uses bullet's own `Chess768` feature extraction and `simple`-example inference, so a
match confirms our feature indexing, perspective/orientation, SCReLU and dequantisation are all correct.

The repo checks in `test/nnue_reference.txt` — 250 reference evals for the shipped `pawnstar-v4.bin` —
and `make check` runs `test_nnue nnue/pawnstar-v4.bin test/nnue_reference.txt` automatically (current
engine: max |diff| 0 cp). The reference's first field is the FEN, so regenerate it after shipping a *new*
net with: `cut -d'|' -f1 test/nnue_reference.txt > fens.txt && "$EVAL" nnue/pawnstar-v4.bin fens.txt >
test/nnue_reference.txt`. With no arguments `test_nnue` is a no-op, so `make check` stays green if the
net/reference are absent.

A second test, `test_nnue_incremental <net.bin>`, plays random move sequences through a `SearchState`
and asserts the incrementally-maintained accumulator evaluates **bit-identically** to a full refresh at
every node (and again after every undo, to catch reverse-update bugs). With no argument it is a no-op,
so `make check` stays green; run it with a net (e.g. `./build/test_nnue_incremental nnue/pawnstar-v4.bin`)
after any change to the accumulator, feature indexing, or make/undo.

### Strength testing (the verdict)

NNUE scores differ from the HCE reference scores, so `test_bratko_kopec` stays an **HCE-only** test
(run with `UseNNUE` off; do not retune its references for NNUE). Real strength is measured by game
play. `run_sprt.sh <net.bin> <openings.epd> [rounds=500] [depth=8]` runs:

- the **same** `build/pawnstar` binary as both engines, differing only by UCI options
  (`UseNNUE`/`EvalFile`), with `PAWNSTAR_THREADS=1`;
- **fixed depth** (default 8) — pawnstar has no `go nodes`, and fixed depth isolates eval *quality* from
  NNUE's per-node speed. To measure the equal-*time* effect (e.g. incremental vs full-refresh, or NNUE
  vs HCE on the clock) run a `tc=`-based match instead (`fastchess … -each tc=8+0.08`);
- **win/draw adjudication** (`-resign movecount=3 score=600`, `-draw movenumber=40 movecount=8
  score=20`) to keep games short and decisive (recommended, not required — the old fixed game-history
  cap that crashed long games is gone; the history is a dynamic `std::vector` now);
- `-sprt elo0=0 elo1=10 alpha=0.05 beta=0.05` and concurrency `nproc`.

fastchess prints "Illegal PV move" warnings occasionally — these are cosmetic (a junk move in the
printed PV tail; the actual best move played is legal).

## 7. Shipped nets

The repo ships one reference net, **[pawnstar-v4.bin](pawnstar-v4.bin)** (512-hidden bullet net, 789568
bytes, `md5 a9c7308e…`) — loaded and used **by default** at startup (the engine resolves it relative to
the working directory). To use a different net, or to switch evaluators at runtime:

```
setoption name EvalFile value nnue/pawnstar-v4.bin
setoption name UseNNUE  value true
```

**`pawnstar-v4.bin`** is trained on ~60M positions of **public PlentyChess** self-play (bulletformat,
strong-engine labels) with a 512-wide transformer. It beats the handcrafted eval by a wide margin
(its 256-wide predecessor scored ~87% / ≈ +330 Elo vs HCE at fixed depth, and v4 adds another
**+55 ± 20 Elo** over that 256 net — see the lineage below).

**Lineage — what moved the needle (all SPRT-measured):**

| step | change | result |
|---|---|---|
| v1 (256) | Pawnstar's own **HCE self-play** (~3.6M) | **loses** to HCE (−67 Elo; a 1.0M predecessor was −151) |
| v2 (256) | **public PlentyChess** data (~60M) | **beats** HCE ≈ +330 Elo — *label quality*, not quantity, was the lever |
| v3 (256) | ~12× more PlentyChess data (~750M) | **no gain** over v2 (+9.5 ± 13.6, inconclusive) — 256 net is *capacity-limited* |
| **v4 (512)** | **double the width**, same 60M as v2 | **+55 ± 20 Elo fixed-depth, +71 ± 25 at time control** over v2 — once data saturates, *net size* is the lever |

PlentyChess data: <https://huggingface.co/datasets/Yoshie2000/plentychess_data_bulletformat> (public,
already bulletformat — `bullet-utils validate` a shard first (some are corrupt, e.g. `11496.data`),
`cat` a few clean ones together, `shuffle`, then train per §6, skipping the text-`convert` step).

The eval is also fast on the clock: the **incremental accumulator** (~+80 Elo equal-time vs full
refresh) and **AVX2 SIMD kernels** (~+180 Elo equal-time vs scalar) mean v4 wins on time as well as
depth despite the wider net. For absolute context the HCE measures ~2350 Elo (CCRL-ballpark), so
`pawnstar-v4` is a large step up — which is why it is now the engine's **default** evaluator (disable
with `UseNNUE false` / `PAWNSTAR_NNUE=0`). Next levers: more data for the *512* net (v4 used only 60M),
or a wider net still (1024) / king-buckets.

### Recreating `pawnstar-v4.bin` (step by step)

`pawnstar-v4.bin` is a **512-wide** net trained on ~60M positions of **public PlentyChess self-play**
(the *same* data as the 256-wide v2 — only the width differs). The data is already in bulletformat
(32-byte `bullet::ChessBoard` records), so the text→`convert` step is skipped. GPU training is
nondeterministic, so this reproduces a *functionally equivalent* net, not a byte-identical one.

The key requirement: the **width must be 512 everywhere** — `kHiddenSize=512` in [src/nnue.h](../src/nnue.h)
(this is the shipped engine) and `HIDDEN_SIZE`/`HIDDEN = 512` in [tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs)
and [tools/bullet/pawnstar_eval.rs](../tools/bullet/pawnstar_eval.rs). `setup_bullet.sh` copies those `.rs`
files into bullet, so as long as the repo is on the 512 arch the trainer produces a 512 net automatically.

```bash
cd /path/to/pawnstar                 # repo root (so ./build and nnue/ paths resolve)
make                                 # build the 512-wide engine (kHiddenSize=512)
make tests                           # build test_nnue / test_nnue_incremental

# 0. One-time: install the bullet trainer (pinned commit) + the 512 examples, build utils + trainer + eval.
tools/setup_bullet.sh
BULLET=~/pawnstar_nnue/bullet
export CUDA_PATH=~/cuda-12.2 PATH="$CUDA_PATH/bin:$PATH" LD_LIBRARY_PATH="$CUDA_PATH/lib64"
( cd "$BULLET/crates/utils"      && cargo build --release )
( cd "$BULLET/crates/bullet_lib" && cargo build --release --features cuda --example pawnstar --example pawnstar_eval )
UTILS="$BULLET/target/release/bullet-utils"
EVAL="$BULLET/target/release/examples/pawnstar_eval"
mkdir -p ~/pawnstar_nnue/v4 && cd ~/pawnstar_nnue/v4

# 1. Download the two PlentyChess shards v4 used (public, tokenless; ~1.9 GB + ~61 MB ≈ 60M positions).
BASE=https://huggingface.co/datasets/Yoshie2000/plentychess_data_bulletformat/resolve/main
curl -L -o 11848.data "$BASE/11848.data"     # ~58M positions
curl -L -o 12892.data "$BASE/12892.data"     # ~1.9M positions

# 2. Validate the format (size % 32 == 0; no invalid records). Some shards in the repo are corrupt
#    (e.g. 11496.data fails this), so always validate before training.
"$UTILS" validate --input 11848.data
"$UTILS" validate --input 12892.data

# 3. Concatenate (flat 32-byte records) and shuffle (self-play data is highly correlated).
cat 11848.data 12892.data > all.data
"$UTILS" shuffle --input all.data --output shuffled.data --mem-used-mb 4096

# 4. Train 40 superbatches (1 superbatch ≈ 1 epoch, so PAWNSTAR_BPS = positions / 16384 ⇒ ~40 epochs,
#    the same recipe as v2). The 512 net trains ~2.3x slower than 256 (~1h on a GTX 1050 Ti).
COUNT=$(( $(stat -c%s shuffled.data) / 32 ))
( cd "$BULLET/crates/bullet_lib" && \
  PAWNSTAR_DATA=~/pawnstar_nnue/v4/shuffled.data PAWNSTAR_BPS=$(( COUNT / 16384 )) \
  PAWNSTAR_SBS=40 PAWNSTAR_OUT=~/pawnstar_nnue/v4/checkpoints \
  cargo run --release --features cuda --example pawnstar )   # drop --features cuda to train on CPU

# 5. Export the quantised net into the repo (a 512 net is 789568 bytes).
cp ~/pawnstar_nnue/v4/checkpoints/pawnstar-40/quantised.bin /path/to/pawnstar/nnue/pawnstar-v4.bin

# 6. Regenerate the engine-vs-trainer cross-check reference for the new net (§6), then verify.
cd /path/to/pawnstar
cut -d'|' -f1 test/nnue_reference.txt > /tmp/fens.txt      # reuse the existing FENs (first field)
"$EVAL" nnue/pawnstar-v4.bin /tmp/fens.txt > test/nnue_reference.txt
./build/test_nnue nnue/pawnstar-v4.bin test/nnue_reference.txt   # expect max |diff| <= 2 cp (engine == trainer)
./build/test_nnue_incremental nnue/pawnstar-v4.bin              # incremental accumulator == full refresh
make check                                                      # all suites, including the NNUE ones

# 7. (optional) Strength-test vs the HCE at fixed depth; reuse any EPD opening book.
tools/run_sprt.sh nnue/pawnstar-v4.bin /path/to/openings.epd
```

To train a **stronger** net, add more clean PlentyChess shards at steps 1–3 (the repo has dozens of
~10–12 GB shards ≈ 370M positions each; `validate` each first). For a **wider** net, change the width to
the same value in all three files above (`nnue.h`, `pawnstar.rs`, `pawnstar_eval.rs`), rebuild the engine
and the bullet examples, and retrain — a different width yields a different file size, so an engine built
for one width cannot load a net of another (§2).

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
- **`NNUE: net '…' is N bytes, expected at least 789506`** — the file isn't a 512-hidden bullet net in
  the expected format (wrong `HIDDEN_SIZE`, wrong architecture, or truncated).
- **Engine "disconnects" mid-match** — the old long-game history overflow is fixed (the game history is
  a dynamic `std::vector` now), so this should be rare; if it recurs it is an actual crash worth a debug
  build. Adjudication (`run_sprt.sh`) is still recommended to keep matches short and decisive.
- **bullet build links `-lcuda` against the system driver** (`/usr/lib/x86_64-linux-gnu/libcuda.so`),
  not the toolkit, so the *driver* version is what gates `cuFuncLoad`; a newer toolkit alone won't help.
