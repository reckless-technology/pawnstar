# NNUE evaluation (experimental)

Pawnstar has an optional **NNUE** (Efficiently Updatable Neural Network) evaluation alongside the
hand-crafted evaluation (HCE). It is **off by default** — the HCE remains the engine's normal
evaluator. This document is the complete reference for how the net works, how to enable it, and how to
generate data, train, validate and strength-test a net.

In-engine code: [src/nnue.h](../src/nnue.h), [src/nnue.cpp](../src/nnue.cpp). The branch into NNUE is in
[src/evaluation.cpp](../src/evaluation.cpp) (`EvaluatePosition`). Tooling: [tools/](../tools).

---

## 1. Architecture

A simple "perspective" network using bullet's standard `Chess768` feature set:

```
768 inputs per perspective  ->  256 feature transformer (weights shared across both perspectives)
SCReLU,  concat[side-to-move | opponent] = 512  ->  1 output  (centipawns, side-to-move relative)
```

Constants (must match the trainer): `kInputSize=768`, `kHiddenSize=256`, `kQA=255`, `kQB=64`,
`kScale=400`.

**Features.** The 768 inputs per perspective are `colour (2) × piece type (6) × square (64)`. For a
piece of `colour` (0=white, 1=black), type `pt` (pawn=1 … king=6) on `square`:

```
white-perspective index = colour     * 384 + (pt-1)*64 + square
black-perspective index = (1-colour) * 384 + (pt-1)*64 + (square ^ 0x38)   # 0x38 flips the rank
```

Two accumulators are built (white-perspective and black-perspective), each seeded with `feature_bias`
and then summing the feature column of every piece. At evaluation the **side-to-move** accumulator is
fed to the first 256 output weights and the opponent's to the second 256. This is mathematically
identical to bullet's `Chess768::map_features` (verified by the cross-check in §6).

**Forward pass** (mirrors bullet's `simple` example exactly; `screlu(x) = clamp(x,0,QA)²`):

```
out  = Σ screlu(stm[i])·w_stm[i]  +  Σ screlu(ntm[i])·w_ntm[i]   # i64
out /= QA                 # SCReLU leaves QA*QA*QB units; reduce one QA
out += output_bias        # bias is in QA*QB units
out *= SCALE
out /= QA * QB            # -> centipawns, side-to-move relative
```

The accumulator is recomputed from scratch on every evaluation (a **full refresh**; no incremental
updates yet), so NNUE is slower per node than the HCE — a clean baseline to optimise later.

## 2. File format

The engine loads bullet's **native quantised `.bin`** directly — headerless, tightly packed,
little-endian `int16`, in this order:

| section | count (int16) | scale |
|---|---|---|
| `feature_weights` | `768 * 256 = 196608` (column-major: `feature*256 + i`) | QA |
| `feature_bias` | `256` | QA |
| `output_weights` | `2 * 256 = 512` (first 256 = stm, next 256 = ntm) | QB |
| `output_bias` | `1` (scalar) | QA·QB |

Packed size is `(196608 + 256 + 512 + 1) * 2 = 394754` bytes. bullet pads the trailing 1-element
`output_bias` tensor up to a 64-byte boundary, so its files are **394816 bytes**; the loader requires
`size >= 394754` and ignores any trailing padding. A size below that is rejected (wrong hidden size /
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

`UseNNUE`/`EvalFile` are advertised in the `uci` response. The `eval` command reports which evaluator
is active; `nnue` prints the raw network eval of the current position. The net is process-global and
read-only after load (shared by all Lazy SMP threads).

## 4. Tooling

| file | purpose |
|------|---------|
| `gen_data.cpp`            | self-play data generator (built by `make tools` → `build/gen_data`) |
| `run_gendata.sh`          | launch N parallel self-play shards |
| `setup_bullet.sh`         | clone bullet at the pinned commit + install/register our trainer examples |
| `bullet/pawnstar.rs`      | the trainer (arch `768→256×2→1`, SCReLU, QA/QB/SCALE) |
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
  (`|score| > 1500` for 4 consecutive plies), or at a 200-ply cap (kept below the engine's
  `kMaxGameLength` so the game history never overflows).

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

Trainer hyperparameters ([tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs)): `HIDDEN_SIZE=256`,
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
match confirms our feature indexing, perspective/orientation, SCReLU and dequantisation are all
correct. `test_nnue` with no arguments is a no-op, so `make check` stays green when no net is present.

### Strength testing (the verdict)

NNUE scores differ from the HCE reference scores, so `test_bratko_kopec` stays an **HCE-only** test
(run with `UseNNUE` off; do not retune its references for NNUE). Real strength is measured by game
play. `run_sprt.sh <net.bin> <openings.epd> [rounds=500] [depth=8]` runs:

- the **same** `build/pawnstar` binary as both engines, differing only by UCI options
  (`UseNNUE`/`EvalFile`), with `PAWNSTAR_THREADS=1`;
- **fixed depth** (default 8) — pawnstar has no `go nodes`, and fixed depth isolates eval quality from
  the unoptimised full-refresh NNUE speed;
- **win/draw adjudication** (`-resign movecount=3 score=600`, `-draw movenumber=40 movecount=8
  score=20`). This is essential: it bounds game length so games never exceed the engine's
  `kMaxGameLength` (512-ply) game-history stack, which would otherwise crash the engine in long
  endgames;
- `-sprt elo0=0 elo1=10 alpha=0.05 beta=0.05` and concurrency `nproc`.

fastchess prints "Illegal PV move" warnings occasionally — these are cosmetic (a junk move in the
printed PV tail; the actual best move played is legal).

## 7. Shipped nets

The repo ships two reference nets in this directory. Both are 256-hidden bullet nets (394816 bytes,
trained 40 superbatches on GPU) and load the same way — **off by default**, since the HCE remains the
engine's normal evaluator unless you switch:

```
setoption name EvalFile value nnue/pawnstar-v2.bin
setoption name UseNNUE  value true
```

| net | training data | vs HCE (fixed depth 8) |
|---|---|---|
| **[pawnstar-v2.bin](pawnstar-v2.bin)** *(recommended; md5 `ca2239fb…`)* | ~60M positions of **public PlentyChess** self-play (bulletformat, strong-engine labels) | **≈ +330 Elo** (≈87%) |
| [pawnstar-v1.bin](pawnstar-v1.bin) *(md5 `d967b96f…`)* | ~3.6M Pawnstar **HCE** self-play (3.37M depth-6 + 235k depth-8 seed) | **−67 Elo** (40%) |

**The decisive lesson is label quality, not quantity.** `pawnstar-v1`, trained only on Pawnstar's own
handcrafted-eval self-play, *loses* to the HCE — and a smaller ~1.0M-position predecessor lost by
−151 Elo, so more data only halved the gap, never closing it. `pawnstar-v2`, trained on a public
dataset of **strong-engine** (PlentyChess) self-play, *beats* the HCE by a wide margin — roughly a
400 Elo swing from the data source alone (SPRT at fixed depth 8, `run_sprt.sh`).

PlentyChess data: <https://huggingface.co/datasets/Yoshie2000/plentychess_data_bulletformat> (public,
already bulletformat — `bullet-utils validate` a shard, `cat` a few together, `shuffle`, then train per
§6, skipping the text-`convert` step).

Caveats: these SPRTs are at **fixed depth**, which neutralises NNUE's slower per-node cost; on equal
*time* the full-refresh net gives some of that margin back until the Phase-4 incremental-accumulator and
SIMD work lands. For absolute context the HCE measures ~2350 Elo (CCRL-ballpark), so `pawnstar-v2` is a
large step up. NNUE remains **off by default**; set `UseNNUE`/`EvalFile` (or the §3 env vars) to use it.

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
- **`NNUE: net '…' is N bytes, expected at least 394754`** — the file isn't a 256-hidden bullet net in
  the expected format (wrong `HIDDEN_SIZE`, wrong architecture, or truncated).
- **Engine "disconnects" in long games during a match** — the `kMaxGameLength` overflow described
  above; always run matches with adjudication (`run_sprt.sh` does).
- **bullet build links `-lcuda` against the system driver** (`/usr/lib/x86_64-linux-gnu/libcuda.so`),
  not the toolkit, so the *driver* version is what gates `cuFuncLoad`; a newer toolkit alone won't help.
