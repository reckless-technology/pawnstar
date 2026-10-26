# NNUE evaluation

Pawnstar evaluates **exclusively** with an **NNUE** (Efficiently Updatable Neural Network) — it is the
engine's only evaluator (the former hand-crafted evaluation, "HCE", was removed). At startup the engine
loads the shipped net `nnue/pawnstar-v12.bin` (relative to the working directory) and evaluates with it;
if that file can't be loaded the engine falls back to a copy of the net embedded in the binary (§3), and
errors out only if both fail. This document is the complete reference for how the net works, how to load a
net, and how to generate data, train, validate and strength-test a net.

The shipped net (`pawnstar-v12.bin`) is a **1024-wide, 8-king-bucket (file-pair × board-half)** `ChessBuckets`
net. §1, §2 and §6 describe that architecture; §7 has the lineage (including the retired Chess768 256/512 nets,
the retired 512-wide 4-king-bucket v6/v7, and the retired 1024-wide 4-bucket v9/v10/v11).

In-engine code: [src/nnue.h](../src/nnue.h) (header-only). The net is called from `EvaluatePosition`
(declared in [src/evaluation.h](../src/evaluation.h), defined in the [src/search_state.h](../src/search_state.h)
hub). Tooling: [tools/](../tools).

---

## 1. Architecture

A "perspective" network using bullet's `ChessBuckets` feature set (the shipped net uses 8 king buckets,
selected by king file-pair × board-half):

```
768 inputs per (perspective, king bucket)  ->  1024 feature transformer (weights shared across perspectives)
SCReLU,  concat[side-to-move | opponent] = 2048  ->  1 output  (centipawns, side-to-move relative)
```

Constants (must match the trainer): `kInputSize=768` (features per bucket), `kNumKingBuckets=8`,
`kFeatureRows=768*8=6144`, `kHiddenSize=1024`, `kQA=255`, `kQB=64`, `kScale=400`.

**Features.** The 768 base inputs per perspective are `colour (2) × piece type (6) × square (64)`. The
shipped net has 8 weight banks selected by king file-pair × board-half (`kKingBucketMap` = `file/2`, plus
4 when the king is in its own advanced half, ranks 5–8 in that perspective's orientation: ranks 1–4 →
`0,0,1,1,2,2,3,3`, ranks 5–8 → `4,4,5,5,6,6,7,7`), so the feature row offsets into that bank. With N
banks each perspective selects one by *its own* king's square (in that perspective's orientation) through
the 64-entry `kKingBucketMap`. For a piece of `colour` (0=white, 1=black), type `pt` (pawn=1 … king=6) on
`square`, with `wb`/`bb` the white/black king buckets (each 0–7 for the shipped net):

```
white-perspective row = wb*768 + colour     * 384 + (pt-1)*64 + square
black-perspective row = bb*768 + (1-colour) * 384 + (pt-1)*64 + (square ^ 0x38)   # 0x38 flips the rank
```

Two accumulators are built (white-perspective and black-perspective), each seeded with `feature_bias`
and then summing the feature column of every piece in its king bucket's bank. At evaluation the
**side-to-move** accumulator is fed to the first 1024 output weights and the opponent's to the second
1024. This is mathematically identical to bullet's `ChessBuckets::map_features` with the same bucket map
(verified by the cross-check in §6). The bucket map MUST stay byte-identical across `src/nnue.h`,
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
from `EvaluateExact`. The `>>9` keeps every adjacent pair inside int16, so the AVX2 (`pmaddubsw`) and
AVX-VNNI (`vpdpbusd`) kernels are saturation-free and bit-identical. **Which one runs is decided at runtime
by cpuid** (`CPUID.7.1:EAX` bit 4; ~+1.3% nps on VNNI CPUs); the VNNI kernel sits behind a function-level
`target("avx2,avxvnni")` attribute, so the engine is a single baseline `-mavx2` binary that also runs on
AVX2-only CPUs (no `make VNNI=1` flag — the gate just never calls the VNNI kernel there).
The int16 *accumulator* and feature transformer are unchanged (the int8 *feature*-weight experiment was
measured and **removed** after a definitive −8 Elo SPRT; see [int8_quant_study.md](int8_quant_study.md)).

The net is an `nnue::Network` class (the quantised weights plus `Load`/`Refresh`/`Update`/`Evaluate`
methods), **owned by the `Game`** — there are no nnue globals. The `Game` is **heap-allocated**
(`std::make_unique<Game>()` in `main.cpp` and the tests): at 8 king buckets the inline ~12.6 MB
`feature_weights_` array would overflow the default 8 MB stack if the owning object were a local. The FT
weights stay an **inline member** rather than a `unique_ptr` to a separate array — a separate (page-aligned)
allocation makes the 2048-byte feature-column stride alias the same cache sets, costing ~10% nps (measured);
keeping the array inline in the heap object is speed-neutral. Its accumulator is **maintained
incrementally and lazily** on each thread's `SearchState` — `PlayMove` defers the update; the accumulator is
brought current only when an evaluation reads it (and only on an eval-cache miss), so nodes that cut off
before evaluating pay nothing (SPRT-measured +20.66 ± 9.04 Elo at 8+0.08, +13.6% nps). The *delta* in the
common case (no king-bucket
crossing — the common case) `Network::Update` derives the changed squares directly from
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
| `feature_weights` | `768*8 * 1024 = 6291456` (column-major: `row*1024 + i`, rows bucket-major) | QA |
| `feature_bias` | `1024` | QA |
| `output_weights` | `2 * 1024 = 2048` (first 1024 = stm, next 1024 = ntm) | QB |
| `output_bias` | `1` (scalar) | QA·QB |

Packed payload is `(6291456 + 1024 + 2048 + 1) * 2 = 12589058` bytes; bullet pads the trailing 1-element
`output_bias` tensor up to a 64-byte boundary, so its raw output is **12589120 bytes** (for the shipped
8-bucket v12; the retired 4-bucket nets were 6297664, and a single-bank net would be 1579072).

**Self-describing header.** A shipped net is prepended with a **32-byte `NetHeader`** (`src/nnue.h`):
a magic (`"PSN1"`), a format version, and the architecture parameters — `kInputSize`, `kNumKingBuckets`,
`kHiddenSize`, `kQA`, `kQB`, `kScale`. So `nnue/pawnstar-v12.bin` is **12589152 bytes** (32 + 12589120). The
loader (`Network::Load`) accepts **only a stamped net**:

- **Stamped** (magic present): every architecture field is checked against the engine's compile-time
  constants; any mismatch is rejected with a descriptive message (e.g. `file is …buckets4…hidden512…,
  engine expects …buckets8…hidden1024…`) — so an incompatible net can never be silently misread.
- **Raw** (no magic, e.g. a freshly-exported bullet `quantised.bin`): **rejected** (`not a stamped
  Pawnstar net (missing 'PSN1' header)`). The old headerless-raw acceptance path was removed; stamp the
  net before loading it.

Stamp a raw net with `tools/stamp_net` (`make tools`); the header values come from the building engine's
constants, so it can only stamp a net that matches this build (and it accepts only a *raw*, exact-payload
net as input — `kNetFileBytes ≤ size < kNetFileBytes+64` — so it can't re-stamp an already-stamped file). The forward-pass math and quantisation are
kept in lock-step with the trainer ([tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs)); §6 verifies
they agree to within rounding. (The retired nets had different sizes/headers — e.g. the Chess768 512-wide
v4 a 789506-byte payload, the 512×4 king-bucketed v6/v7 a 3148802-byte payload, the 1024-wide 4-bucket v9/v10/v11
a 6297664-byte payload — so this 1024-wide 8-bucket build size/header-gates them out.)

## 3. Using a net in the engine

A net is required (NNUE is the only evaluator). `main.cpp` loads `nnue/pawnstar-v12.bin` (cwd-relative) at
startup. **Embedded fallback:** a byte-identical copy of the shipped net is compiled into the engine
binary (`src/embedded_net.S` `.incbin`'s the net at the build's `EMBED_NET` path — the Makefile and the
CMake build both pass it via `-DEMBED_NET_PATH`, the single source of truth, so the embedded copy can't
drift from the shipped net; the `.S` embeds into ELF `.rodata` or COFF `.rdata`, so the engine binary is
self-contained on Linux and Windows alike; linked into the engine only, not the test binaries). If the on-disk file can't be loaded — wrong cwd, missing or renamed file, or a stamped net that
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
net is a single `nnue::Network` instance owned by the (heap-allocated) `Game` (UCI `setoption` routes to
`game.nnue_network_.Load(...)`), read-only after load and shared by all Lazy SMP threads through their
`Game&` — they call only its `const` methods.

## 4. Tooling

| file | purpose |
|------|---------|
| `gen_data.cpp`            | self-play training-data generator: the engine plays itself and writes `<fen> \| <eval_cp> \| <wdl>` text shards plus a `.meta` provenance sidecar (`make tools` → `build/gen_data`) — see §6 |
| `gen_openings.cpp`        | balanced opening generator from **our own play**: short randomised lines from the start position, filtered on the net's static evaluation and deduplicated on the Zobrist key, written as EPD with its provenance header (`make tools` → `build/gen_openings`) — the data campaign's opening source, see §6 |
| `run_gendata.sh`          | fan `gen_data` across cores, one shard per worker (each with a disjoint slice of the opening pool), resumable, reporting the pool's duplicate rate at the end |
| `check_duplicates.sh`     | measure what fraction of an assembled pool is distinct positions — the gate `train_pipeline.sh` runs before converting a directory of shards, and the cheap way to catch a degenerate campaign in minutes |
| `make_openings.sh`        | build `openings.epd`, the **match** set, from the public-domain TSCP opening book: replay each line, keep the roughly balanced positions, dedup (capped at ~420 positions by that source, which is why data generation uses `gen_openings` instead) |
| `stamp_net.cpp`           | prepend the self-describing `NetHeader` to a raw bullet net (`make tools` → `build/stamp_net`) |
| `setup_bullet.sh`         | clone bullet at the pinned commit + install/register our trainer examples |
| `bullet/pawnstar.rs`      | the trainer (arch `768×8 king-bucketed → 1024×2 → 1`, SCReLU, QA/QB/SCALE) |
| `bullet/pawnstar_eval.rs` | reference evaluator for the `test_nnue` cross-check |
| `train_pipeline.sh`       | train → export a net from a bulletformat `.data` file (or a directory of text shards) |
| `verify_net.sh`           | cross-check (engine == trainer) + incremental-accumulator gate for a trained net |
| `run_sprt.sh`             | SPRT / match: net-vs-net (set `BASELINE_NET`), at fixed depth or time control |
| `rate.sh`                 | anchored absolute-Elo estimate vs rated reference engines (you supply the opponent binaries) |
| `run_experiment.sh`       | one-shot: train + **verify** + SPRT from an existing `.data` dataset |
| `filter_book.cpp`         | offline opening-book filter: NNUE-search every book move and flag questionable ones (lose > margin vs best) with a trailing `?`, so the engine replays them in book but never plays them (`make tools` → `build/filter_book`) |
| `nnue_quant_study.cpp`    | offline int16→int8 quantisation feasibility study (range/saturation histograms + kernel cost split); a measurement tool, not part of the engine — see [int8_quant_study.md](int8_quant_study.md) (`make tools` → `build/nnue_quant_study`) |

**Prerequisites:** `clang++`/`make` (engine), Rust/`cargo` (bullet), `fastchess` on `PATH` (SPRT). For
GPU training, a CUDA toolkit (`CUDA_PATH`, default `/usr/local/cuda`) **and** a driver new enough for the
toolkit: bullet's CUDA backend calls `cuFuncLoad`, which needs a driver supporting CUDA ≥ 12.3
(driver ≥ 545). Otherwise train on CPU with `BULLET_FEATURES=""` — the net is small enough that CPU
training is practical. `tools/setup-dev.sh --training` (`tools\setup-dev.ps1 -Training` on Windows) installs Rust's
stable toolchain, checks for the GPU and the toolkit and runs `setup_bullet.sh`; `fastchess` is still yours to build.
On Windows `setup_bullet.sh` runs under Git's bash and `train_pipeline.sh` under MSYS2 (with
`MSYS2_PATH_TYPE=inherit`, so cargo and nvcc are on its `PATH`), and `BULLET` defaults to
`%USERPROFILE%\pawnstar_nnue\bullet`, since MSYS2's `~` is a home of its own.

## 5. Quick start (minimal retrain)

```bash
tools/setup_bullet.sh                                          # once: clone + register bullet (pinned commit)
OPENINGS=nnue/openings.epd \
  tools/run_experiment.sh ~/pawnstar_nnue/shuffled.data net.bin   # train + verify + SPRT  ->  net.bin
```

Then load `net.bin` in the engine (§3). To train on CPU: `BULLET_FEATURES="" … tools/run_experiment.sh …`.
`run_experiment.sh <shuffled.data> <net.bin> [superbatches=40] [sprt_rounds=500] [depth=8]` chains
`train_pipeline.sh` → `verify_net.sh` → `run_sprt.sh`. The data argument is an already-converted
bulletformat `.data` file (how the shipped nets were trained — see §7); it carries no FENs to sample
openings from, so the SPRT's opening book comes from `OPENINGS=…` — the committed
[openings.epd](openings.epd) (§6, Openings), or one of your own.

## 6. Step-by-step

### Data

The shipped nets are trained on **the public PlentyChess bulletformat dataset** — a pre-converted, shuffled
`.data` file (bullet `ChessBoard` records, 32 bytes each). That file is the input to training below.

`train_pipeline.sh` also accepts a directory of text shards (`data_*.txt` / `seed*.txt`) in bullet's
`convert --from text` format — one record per line, `<fen> | <white_relative_eval_cp> | <wdl>` (wdl:
1.0 win / 0.5 draw / 0.0 loss, White's POV) — which it combines, converts, and shuffles. Bring such
shards from any source in this format, use a `.data` file directly, or generate them from self-play with
`gen_data` below.

**Both the score and the result are White-relative, not side-to-move relative.** bullet's text parser
mirrors the board for a black-to-move FEN and negates the score and the result itself (`ChessBoard::from_str`
in `bulletformat`), so writing side-to-move-relative values inverts the label of every black-to-move
position. Verified by converting a white-to-move and a black-to-move copy of the start position, both
written `| 123 | 1.0`: the two 32-byte records came out `score=123 result=2` and `score=-123 result=0`.

#### Self-play generation (`gen_data`)

`gen_data` has the engine play itself with the loaded net and writes one record per kept position:

```bash
make tools      # -> build/gen_data
tools/run_gendata.sh ~/pawnstar_nnue/selfplay 24 400 \
    --openings nnue/openings-selfplay.epd --play-nodes 12000 --label-nodes 220000 --label-interval 4 \
    --pv-plies 4
tools/train_pipeline.sh ~/pawnstar_nnue/selfplay net.bin        # the directory mode above
```

**The search is the teacher, not the net** — the net only has to play a coherent game, while the label is
what the search backs up. That is why the generator is two-stage: it plays on a cheap budget
(`--play-nodes` / `--play-depth`) and re-labels only every Nth kept quiet position (`--label-interval`)
with a much larger budget (`--label-nodes` / `--label-depth`), which decouples the cost of playing from
the cost of the label. `--pv-plies` then harvests the first few plies of each deep label's principal
variation as further records; they all carry the same White-relative root score, since a White-relative
score does not depend on whose turn it is. That harvest is what makes a deep label affordable — 3.82×
the records at the same cost, measured below.

Each feature is switchable on its own so it can be A/B tested: `--label-interval 0` is single-stage,
`--pv-plies 0` turns the harvest off, `--disagreement-copies 1` turns off the mild reweighting that writes
a record twice when the search and the static eval disagree by more than `--disagreement-cp` (no position
is ever *dropped* for agreeing — that would skew the distribution the eval is calibrated against), and
`--no-dedup` keeps positions repeated within a shard. Kept positions are quiet, not in check and under
3000 cp; games are adjudicated at 1500 cp held for four plies and capped at 200 plies. The search runs
single-threaded, so a shard is reproducible from its seed. Every shard gets a `<shard>.meta` sidecar (a
separate file, because `convert --from text` would try to parse a comment line as a position) recording
the engine version, the labelling net **and its SHA-256**, every budget, the seed, the PV cap, the
disagreement settings, the opening set with the worker's slice of it (`openings_count`,
`openings_in_slice`, `worker_index`, `worker_count`) and the record and game counts, the last of these
including `games_abandoned`, `games_decided_at_first_label` and `games_without_records` — which is what
makes a net's data lineage auditable and a shard's own health readable.

**Measured throughput**, one worker on one core of an i9-13900HX (~2.0M nps) with net v12 and
`nnue/openings.epd`, every other option at its default, each run about 4½ minutes:

| configuration | records/s/core | records/game | bytes/record |
|---|---|---|---|
| single-stage (`--label-interval 0`), `--play-depth 8`, `--pv-plies 0` | 41.6 | 116 | 66.5 |
| `--play-depth 8 --label-interval 4 --label-nodes 220000 --pv-plies 0` | 6.45 | 33 | 65.4 |
| the same with `--pv-plies 4` | 24.6 | 129 | 65.1 |
| `--play-nodes 12000 --label-interval 4 --label-nodes 220000 --pv-plies 4` | 30.7 | 75 | 70.4 |

The deep label costs what it looks like it costs: a 220k-node label on every fourth position drops the
rate 6.5×, from 41.6 records/s to 6.45. **The PV harvest pays that back almost four times over** — the
same run with `--pv-plies 4` yields 3.82× the records, because one deep search then labels up to five
positions instead of one. Playing the game cheaply (the last row) recovers the rest. At ~70 bytes a
record, a 500M-position pool is ~35 GB of text and ~16 GB once converted to bulletformat.

**Size a campaign from a loaded machine, not from one worker.** The last configuration run across 24
workers on the same machine produced 74,960 records in 260 s — **288 records/s in total, but only 12.0
records/s per core**, 2.55× less than the 30.7 one worker managed with the machine to itself. This CPU has
8 performance and 16 efficiency cores, so most of those workers are on the slow ones and the fast ones
lose their turbo headroom; a machine of uniform cores will land between the two figures. On that measured
loaded rate a **500M-position pool needs about 13 days on 38 cores** (5 days at the single-worker rate —
treat that as the optimistic bound, not the plan).

**Openings matter more than they look.** With no `--openings` the generator starts each game from the
standard position plus `--random-plies` (8) uniformly random plies, which often reaches a position one
side is already lost in; the resign adjudication then ends the game four plies later and the whole game
yields about one record. Supplying the opening set raises that by two orders of magnitude.

#### Data diversity (what a campaign is actually limited by)

**The engine is deterministic at a fixed budget**, single-threaded, with its book off, so across processes a
self-play game is decided by the position it starts from — which makes the *opening set*, not the game count,
the limit on how many distinct games a campaign can produce. Measured: two different seeds, one opening and
`--random-plies 0` write **byte-identical shards**, because with no random plies the seed never enters the
game. Within one process it is not quite that stark — the transposition table carries over between games, so
a worker starting twice from the same opening plays a near-copy rather than a copy (ten games from one
opening in one process still wrote 676 records with the dedup on, not fifty and then nothing) — but the lines
overlap heavily and it is not a mechanism to rely on. And since `gen_data` deduplicates positions **per
shard**, every shard looks healthy from the outside while the assembled pool is mostly copies: shard sizes
never show this. A 500M-position campaign on the 306-position book-derived set would therefore have been a
few hundred games' worth of genuinely distinct play, replicated thousands of times.

Three things compose, and all three keep a shard reproducible from its seed (no clock, no entropy):

1. **A large opening pool of our own play** — `--openings nnue/openings-selfplay.epd` (30,000 positions, and
   `gen_openings` writes millions in seconds: *The data campaign's openings* below).
2. **A couple of random plies on top of each opening** — `--random-plies`, which defaults to **2** when
   `--openings` is given (it was 0, and is still 8 from the start position). Each opening becomes the ~900
   two-ply legal lines out of it, which is what makes the distinct-game count unbounded rather than merely
   large.
3. **A disjoint slice of the pool per worker** — `--worker-index` / `--worker-count`, which
   `run_gendata.sh` passes automatically: worker *I* takes the openings whose index is congruent to *I*
   modulo the worker count. **Two workers cannot start a game from the same position**, so a cross-worker
   duplicate game is impossible by construction rather than merely unlikely. Within a worker the slice is
   handed out in a seeded shuffle and reshuffled each pass — without replacement, rather than the uniform
   draws with replacement that start repeating after about the square root of the pool (20 games into a
   306-position set).

**Measured, `--random-plies` on top of the committed pool.** 480 games per setting (8 workers × 60 games,
each worker on its own slice, the same openings and seeds for every setting), `--play-nodes 20000
--label-nodes 220000 --label-interval 4 --pv-plies 4`, all 32 processes resident at once so every setting saw
the same loaded machine (i9-13900HX):

| `--random-plies` | records/game | records/s/core | decided at the first label | games with no records |
|---|---|---|---|---|
| 0 | 83.8 | 11.70 | 31/480 (6.5 %) | 4/480 |
| **2 — the default** | **70.8** | **11.10** | **59/480 (12.3 %)** | 11/480 |
| 3 | 63.8 | 10.43 | 72/480 (15.0 %) | 19/480 |
| 4 | 61.6 | 10.21 | 66/480 (13.8 %) | 13/480 |

*Decided at the first label* means the game's first searched position was already past the 1500 cp resign
threshold: the random line walked into a position one side has lost, and the game ends a few plies later
having taught little. Each shard counts it in its own `.meta`, with `games_abandoned` and
`games_without_records` beside it, so a campaign reports its own health per shard.

**Why 2 is the default.** Zero is not an option: it caps the campaign at the pool's size, which is the defect
this design exists to remove. Two is the cheapest setting that is not zero — 15 % fewer records a game and
5 % less throughput than zero, and twice as many games decided before they start (12.3 % against 6.5 %), all
of which buys ~900× the distinct starts per opening. Three and four cost 24 % and 27 % of the records a game
and improve nothing measurably; an even count also keeps the random line symmetric, each side moving the same
number of times rather than one of them getting a free move.

**Size the pool, not the random plies.** At 70.8 records a game, a 500M-position pool is about 7.1M games.
Two random plies on the committed 30,000-position pool give ~27M distinct starts — enough that a
development-scale run never repeats one, but only ~4× the games a full campaign plays, so about 12 % of its
games would share a start with another. **Generate a bigger pool for a real campaign** (`build/gen_openings
~/pawnstar_nnue/openings-1m.epd 1000000 1` — 1M positions from 3.83M random lines, measured at 52 s with the
machine fully loaded by a data run, 62 MB, which is why it belongs on a path of its own, not in the repo):
1M openings × ~900 two-ply lines is ~900M distinct starts, under 1 % of 7.1M games sharing one. Raising `--random-plies` to 3 is the alternative if the pool
must stay at 30,000, at the cost in the table.

**Check the pool before training on it** — `tools/check_duplicates.sh <pool.txt | shard_dir>
[min_unique_fraction] [records_per_shard]`:

```
check_duplicates.sh: 16 shard(s), 14 MiB in /…/selfplay
check_duplicates.sh: reading every record (exact)
check_duplicates.sh: records                              225285
check_duplicates.sh: after collapsing weighting copies    170846
check_duplicates.sh: distinct positions                   170846
check_duplicates.sh: unique fraction                      1.0000 (0.00 % duplicate)
check_duplicates.sh: games played (from the .meta)        3039 -> about 3039 distinct
check_duplicates.sh: OK (gate 0.80)
```

It counts records, records after collapsing the adjacent copies the disagreement weighting writes on purpose
(`--disagreement-copies`), and distinct positions, and fails below the `min_unique_fraction` gate (0.80 by
default; `0` reports only). A pool larger than 2 GB is sampled at 200,000 records per shard rather than read
whole — a duplicate here is a repeated *game*, which is a contiguous run of records in a shard, so a
contiguous block from every shard keeps whole games intact and still sees every worker; that sampled fraction
is an upper bound on how unique the whole pool is, which is the safe direction for a gate.
`run_gendata.sh` reports it when a run finishes, and **`train_pipeline.sh` runs it as a gate** before
converting a shard directory (`ALLOW_DUPLICATE_POOL=1` to train anyway, `MIN_UNIQUE_FRACTION` to move the
gate).

**The before and after, measured at the same size.** Two 16-worker runs of 190 games each (3,040 games,
~30 minutes apiece), side by side on the same machine, same budgets, differing only in the diversity
settings:

| settings | records | records/game | after collapsing | distinct positions | unique | implied distinct games |
|---|---|---|---|---|---|---|
| the book-derived set, `--random-plies 0`, no partition | 320,833 | 105.5 | 259,117 | 243,930 | **0.9414** | ~2,861 / 3,040 |
| these defaults: the 30,000 self-play pool, `--random-plies 2`, partitioned | 225,285 | 74.1 | 170,846 | 170,846 | **1.0000** | 3,039 / 3,039 |

**Not one duplicate position in 170,846** at the new defaults: 16 workers, disjoint opening slices, every
game distinct. The first row lost 5.86 % of its records to duplication **after only 3,040 games** — a
ten-fold reuse of a 306-position set — and that figure was still climbing (4.08 % a third of the way into the
same run), because the degeneracy grows as the pool is exhausted. A 500M campaign is ~7.1M games, which is
23,000 plays of each of those 306 openings: 5.86 % is the start of that curve, not its plateau.

**What it costs: 30 % of the records a game yields** (105.5 → 74.1), and it is worth naming where that goes,
because only half of it is the random plies. The book's openings are sound — **none of the 3,040 games from
them was already decided at its first label** — while 6.5 % of games from the randomly generated pool are
(9.5 % in this run, with the two random plies on top), since the balance filter is a static evaluation and a
position can be level by material and lost to a tactic. Those games still produce records, just fewer of
them. Duplicate records are worth nothing to train on, so 74 distinct records a game beats 105 records a
game that are 6 % — and climbing — copies; the obvious next improvement is a cheap tactical filter in
`gen_openings` (a shallow search, or rejecting a position whose mover has a winning capture by static
exchange evaluation) rather than more random plies.

### Training

`train_pipeline.sh <shuffled.data | data_dir> <out_net.bin> [end_superbatch=40]` auto-detects its input
from the first argument:

1. ensures bullet is set up, builds `bullet-utils` and the trainer (`$BULLET_FEATURES`, default
   `--features cuda`);
2. **if the argument is a `.data` file** (already bulletformat — e.g. a public dataset, the way
   the shipped nets were trained), it is used directly and steps 3–4 are skipped; **if it is a
   directory**, the text shards `data_*.txt` + `seed*.txt` are combined → `all.txt`,
   `bullet-utils convert --from text` → `all.data` (bullet `ChessBoard`, 32 bytes/record), then
   `bullet-utils shuffle` → `shuffled.data` (correlated data needs shuffling);
3. trains, computing `batches_per_superbatch = positions / 16384` so a superbatch ≈ one epoch;
4. copies `<work_dir>/checkpoints/pawnstar-<end_superbatch>/quantised.bin` → `out_net.bin`, where
   `<work_dir>` is the data directory (text mode) or the `.data` file's directory (bulletformat mode).

Trainer hyperparameters ([tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs)): `HIDDEN_SIZE=1024`,
AdamW, `batch_size=16384`, loss `sigmoid(out)` squared-error against
`target = wdl·result + (1-wdl)·sigmoid(score/SCALE)` with `ConstantWDL=0.5`, `StepLR{start=0.001,
gamma=0.3, step=SBS/3}`, `eval_scale=400`. Overridable via env: `PAWNSTAR_DATA`, `PAWNSTAR_BPS`,
`PAWNSTAR_SBS`, `PAWNSTAR_OUT`. GPU training reports the device (e.g. `Training on NVIDIA GeForce RTX
4070`) and runs at ~2.4M positions/sec on that card for this 1024-wide net (~0.34M on an older GTX
1050 Ti; see §7).

### Openings

The SPRT needs an EPD opening book (`OPENINGS=…`) of varied, roughly balanced early-game positions so
matches don't all start from the same spot. **[openings.epd](openings.epd) is committed**: 306 positions
built by `tools/make_openings.sh` from `pawnstar.book`, which is the **public-domain** TSCP book, so the
set carries no licence restriction of its own and no third party's data. Each book line is replayed from
the start position and every position it reaches between ply 4 and ply 24 is kept when the engine's static
evaluation of it is within 125 cp of its evaluation of the start position (which this net puts at +158 cp,
so a zero-centred filter would keep only the openings it mildly dislikes); then duplicates are dropped.

Regenerate it with `tools/make_openings.sh [out.epd] [min_ply] [max_ply] [margin_cp]` — worth doing when
the net changes, since the balance filter is the net's own judgement. **Its size is capped by the source:**
the TSCP book's 385 lines overlap heavily and hold only about 420 distinct positions in that window, so an
SPRT with more rounds than openings replays some of them (`order=random`), which is fine but worth knowing.
Any standard EPD set works instead. The file carries a `#` provenance header; `run_sprt.sh` strips comment
lines into a scratch copy before handing the file to fastchess, whose EPD reader would try to parse one as
a position.

#### The data campaign's openings (`gen_openings`) — a pool, not a book

That 420-position ceiling is fine for a match and fatal for a data campaign (see *Data diversity* above), so
the campaign's openings come from **our own play** instead. **[openings-selfplay.epd](openings-selfplay.epd)
is committed**: 30,000 positions written by `build/gen_openings`, which plays short randomised lines from the
start position and keeps the position a line ends at when it is not in check and the net's static evaluation
of it is within 125 cp of its evaluation of the start position — the same balance filter as the book set, so
a game from one of these is decided by the engines and not by the opening. Duplicates are dropped on the
Zobrist key, and **one position is kept per line played**, so no two openings in the file share a line (the
book script harvests a whole window from each line, which emits prefixes of one another).

```bash
make tools                                                  # -> build/gen_openings
build/gen_openings nnue/openings-selfplay.epd 30000 1        # the committed set, 0.4 s, 1.9 MB
build/gen_openings ~/pawnstar_nnue/openings-1m.epd 1000000 1 # a campaign's own pool, under a minute, 62 MB
```

`gen_openings <out.epd> <num_positions> <seed> [--min-ply 8] [--max-ply 12] [--margin-cp 125] [--max-lines N]
[--net PATH] [--progress N]`. **The count is a parameter, not a property of the source** — the committed
30,000 (1.9 MB) is a convenience, and a campaign should generate a bigger pool to a path of its own rather
than commit one. The acceptance rate is about one line in four (114,419 lines for the committed 30,000), and
the generator is deterministic: the same seed, options and net reproduce the file byte for byte, which is the
audit trail for a set with no external source. The output carries the same kind of `#` provenance header as
the book set, stating how it was produced; the engine version is stamped into it.

The positions are not book lines and do not look like them — that is the point for training data, where a
wider distribution of labelled positions is worth more than opening theory, and the balance filter is what
keeps them playable. For an SPRT, keep using `openings.epd`.

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

The repo checks in `test/nnue_reference.txt` — 250 reference evals for the shipped `pawnstar-v12.bin` —
and `make check` runs `test_nnue nnue/pawnstar-v12.bin test/nnue_reference.txt` automatically (current
engine: max |diff| 0 cp). The reference's first field is the FEN, so regenerate it after shipping a *new*
net with: `cut -d'|' -f1 test/nnue_reference.txt > fens.txt && "$EVAL" nnue/pawnstar-v12.bin fens.txt >
test/nnue_reference.txt`. With no arguments `test_nnue` is a no-op, so `make check` stays green if the
net/reference are absent.

A second test, `test_nnue_incremental <net.bin>`, plays random move sequences through a `SearchState`
and asserts the incrementally-maintained accumulator evaluates **bit-identically** to a full refresh at
every node (and again after every undo, to catch reverse-update bugs). With no argument it is a no-op,
so `make check` stays green; run it with a net (e.g. `./build/test_nnue_incremental nnue/pawnstar-v12.bin`)
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

For an **absolute** rating (rather than a net-vs-net delta), `rate.sh <name:rating:cmd> ...` plays the
engine against one or more externally-rated reference engines (e.g. the CCRL engines bundled with Arena —
which you supply, as they are not in the repo) and reports `anchor_rating + measured_diff` per anchor plus
the mean. It anchors only as well as those opponents and the chosen TC, so treat the result as a ballpark.

## 7. Shipped nets

The repo ships one reference net, **[pawnstar-v12.bin](pawnstar-v12.bin)** (1024-wide, 8 king buckets,
file-pair × board-half; stamped file **12,589,152 bytes ≈ 12.6 MB**) — loaded at startup as the engine's only evaluator
(resolved relative to the working directory). To use a different net at runtime:

```
setoption name EvalFile value /path/to/other-net.bin
```

**`pawnstar-v12.bin`** is trained on the **same ~6B** positions of **the public PlentyChess bulletformat dataset**
(strong-engine labels) **as v11**, with a 1024-wide transformer and **8 king buckets selected
by king file-pair × board-half** (`kKingBucketMap`, §1). The **only** change from the previous shipped net
**v11** is the **bucket count**, doubled 4→8 (the rank split puts a king in its own advanced half, ranks
5–8, into banks 4–7). It beats v11 by **+17.29 ± 8.68 Elo at 8+0.08** (3520 games, H1, LLR 2.96) and
**+10.97 ± 6.57 at 12+0.12** (5610 games, H1, LLR 2.95); final training loss 0.0226 (v11 was 0.0228). The
key finding: **king buckets are NOT at diminishing returns** — the 0→4 step was +11.4 (v9), the 4→8 step
is *bigger* (+17/+11), because the rank split captures real own-half-vs-advanced king-safety signal. This
is the opposite of the old "bucket advantage shrinks with data" expectation: the *data-scaling* steps
(v9→v10→v11) still diminished, but adding *more buckets at fixed data* did not. See the lineage below.

**Lineage — what moved the needle (all SPRT-measured):**

| step | change | result |
|---|---|---|
| v1 (256) | Pawnstar's own **HCE self-play** (~3.6M) | **loses** to HCE (−67 Elo; a 1.0M predecessor was −151) |
| v2 (256) | **the public PlentyChess bulletformat dataset** (~60M) | **beats** HCE ≈ +330 Elo — *label quality*, not quantity, was the lever |
| v3 (256) | ~12× more of the same data (~750M) | **no gain** over v2 (+9.5 ± 13.6, inconclusive) — 256 net is *capacity-limited* |
| v4 (512) | **double the width**, same 60M as v2 | **+55 ± 20 Elo fixed-depth, +71 ± 25 at time control** over v2 — once data saturates, *net size* is the lever (shipped, now retired) |
| v5 (1024) | **double width again**, same 60M | **rejected**: +2 ± 16 fixed-depth (dead even), **−74 ± 25 on the clock** — 60M *saturates* the 512 net; width only pays off with more data. Branch `bignet-1024`, not shipped |
| kb (512 + 4 king buckets) | king buckets, **same 60M** | **flat** (~−8 fixed depth) — the higher-capacity feature set is *data-starved* on 60M |
| v6 (512 + 4 king buckets) | king buckets, **~818M** | **+47 ± 18 fixed-depth, +17 ± 12 on the clock** over v4 — *more data* unlocked the king buckets (shipped, later retired). Needed two engine speed fixes (the eval cache and a single-pass `Update`) to turn an initial −74 clock result positive |
| v7 (512 + 4 king buckets) | **~2.31B** (v6's 818M + 4 more big shards), same arch, SBS 40→120 (~3 epochs) | **+29.96 ± 14.05 fixed-depth, +20.73 ± 12.73 on the clock** over v6 — *still more data*; same arch/speed, so the eval-quality gain carries straight to the clock (later retired) |
| **v8 (1024, no king buckets)** | **double the width, drop king buckets**, same 2.31B as v7 | **+31.9 ± 16.6 Elo at 40/20** over v7 (LLR 2.95, H1; 972 games) — at the full data scale width beats buckets, **at half the file size** (fewer feature rows, wider hidden). Reverses v5's 60M result now that data no longer saturates 1024. (later retired) |
| v9 (1024 + 4 king buckets, file-pair) | add king buckets to the v8 arch, same ~2.31B scale/schedule | **+11.4 ± 6.75 Elo at 8+0.08** over v8 (LLR 2.96, H1; 5520 games). Holding width fixed, buckets win at scale — the gain shrinks with data (+29 at 750M → +11 at 2.31B) but stays positive; the v8 'width beats buckets' call was width-vs-buckets confounded. **File-pair bucketing beats quadrant/rank-based** (king safety is file-dependent; back-rank kings use all 4 file banks). 4× the FT rows (1.58→6.3 MB). (later retired) |
| v10 (1024 + 4 king buckets, file-pair) | **more data**: scale the pool 2.31B→3.82B (four more PlentyChess shards), same v9 arch, SBS 120→190 (~3 epochs) | **+16.3 ± 10.8 Elo at 8+0.08** over v9 (LLR 2.95, H1; 1944 games, 52.3%). More data again the single biggest lever. (later retired) |
| **v11 (1024 + 4 king buckets, file-pair)** | **more data**: scale the pool 3.82B→6.05B (six more PlentyChess shards), same v10 arch, SBS 190→300 (~3 epochs) | **+9.28 ± 6.58 Elo at 8+0.08** over v10 (LOS 99.7%, LLR 2.48; 6102 games, 51.3%). More data again the single biggest lever — the per-step gain is shrinking (+16 for +1.5B → +9 for +2.2B) but stays clearly positive; v11's 6.05B was still a fraction of the pool available then. (later retired) |
| **v12 (1024 + 8 king buckets, file-pair × board-half)** | **double the king buckets 4→8** at fixed data (same ~6B pool as v11): bank = `file/2`, plus 4 when the king is in its own advanced half (ranks 5–8, per-perspective oriented); SBS=300, final loss 0.0226 (v11 0.0228) | **+17.29 ± 8.68 Elo at 8+0.08** (3520 games, H1, LLR 2.96) and **+10.97 ± 6.57 at 12+0.12** (5610 games, H1, LLR 2.95) over v11 — single variable vs v11 = bucket count. **King buckets are NOT at diminishing returns**: the 4→8 step (+17/+11) is *bigger* than the 0→4 step (+11.4), the rank split capturing real own-half-vs-advanced king-safety signal. 8× the FT rows (1.58→12.6 MB, so the owning `Game` is heap-allocated). **shipped** |

The data is the public PlentyChess bulletformat dataset: `bullet-utils validate` each shard first (a corrupt shard
trains silently wrong), `cat` a few clean ones together, `shuffle`, then train per §6 — the
text-`convert` step is skipped, the shards being bulletformat already.

The eval is also fast on the clock: the **incremental accumulator** (~+80 Elo equal-time vs full
refresh), **lazy/deferred accumulator updates** (skip the update for nodes that cut off before evaluating —
**+20.66 ± 9.04 Elo at 8+0.08**, +13.6% nps), **AVX2 SIMD kernels** (~+180 Elo equal-time vs scalar), the
**int8 output layer** (+31.8 Elo), and the **eval cache** mean NNUE wins on time as well as depth. v12's 8-bucket table is 8× the feature rows of a single-bank net (12.6 MB vs 1.58 MB)
and slightly heavier per eval (and needs the owning `Game` heap-allocated, since the inline weight array would
overflow the default stack), but every shipping SPRT (v8→v9→v10→v11→v12) was at a time control, so that cost is
already priced in. **The lessons: more data is the lever** (v12's ~6B pool was never scaled to its limit),
**and king buckets are not yet saturated** (the 4→8 double won at fixed data).
Next levers: yet more data; a finer or mirrored bucket map (file+rank, or mirrored to halve the input
space); or a wider transformer still.

**Rejected architecture experiments (not in the shipped lineage):**
- **Head depth — a hidden layer in the head (`concat 2048 → 16 → 1`, SCReLU):** REJECTED, **−11.3 ± 12.9 Elo
  at fixed depth 8, H0 (1908 games)**. Single-variable A/B — a control (current `2048 → 1` head) and the
  candidate trained on the *same* shuffled 200M set + same schedule, then head-to-head SPRT. Fixed depth is
  pure eval quality, and it's already negative there, so no time-control follow-up. The FT stayed int16 +
  incremental and the new head ran in **float** (f32-saved head weights — negligible vs the FT at fixed
  depth, and it makes the engine match the trainer trivially). `NetHeader` gained a `hidden2_size_` field
  (format_version 2). Branch `nnue-hidden-layer` (pushed, not merged); trainer `tools/bullet/pawnstar_h.rs`,
  reference `tools/bullet/pawnstar_eval_h.rs`. **Caveat:** reused the 2-layer LR schedule/epochs — a deeper
  head may want tuned hyperparameters/more data; this rejects the same-recipe hidden layer, not all heads.
  **Gotcha:** bullet stores affine weights **input-major** (`w[input*out + output]`, like the feature
  transformer); the `O=1` output layer hides this, so reading the `2048→16` layer output-major yields a dead
  near-constant head — and a 0 cp `test_nnue` cross-check passes while broken (engine and reference misread
  identically), so **sanity-check that evals track material (up-a-queen → ~+1200) before trusting a new arch.**
- **Output buckets — 8 piece-count banks (`concat 2048 → 8`, `select` by bullet `MaterialCount<8>`,
  `bucket = (popcount(occ) − 2) / 4`):** REJECTED at scale. Single-variable A/B (control = v11 arch, candidate =
  v11 + buckets, same pool + recipe). Weak **+6.1 ± 9.5 fixed-depth at 500M (H0, inconclusive)**, but at **6B
  (v11's ~6.36B pool, SBS=300) it regresses at both time controls — −22.96 ± 15.71 @ 8+0.08 and −14.70 ± 13.27
  @ 12+0.12 (both H0)**. The slower TC is less negative (−23 → −15, TC-dependence: the larger output table's
  per-move speed cost weighs less) but never positive. Causes: *output*-bucket gains shrink with data (note this
  is the output bucket; the v11→v12 *king*-bucket double instead grew the gain at fixed data — §7 lineage),
  plus a small TC speed penalty (the int8 output table is 8× larger, 16 KB vs 2 KB; the
  500M test was fixed-depth so never paid it). FT unchanged; the output is saved **bucket-major** (bullet
  `.transpose()`), so each bank's 2048-weight row is contiguous and the int8/int16 dot kernels are reused on the
  selected bank — no float path. `NetHeader` gained an `output_buckets_` field (format_version 2). Branch
  `nnue-output-buckets` (not merged); trainer `tools/bullet/pawnstar_ob.rs`, reference `pawnstar_eval_ob.rs`.

### Recreating `pawnstar-v12.bin` (step by step)

`pawnstar-v12.bin` is a **1024-wide net with 8 king buckets (file-pair × board-half)** trained on the
**same ~6 billion** positions as v11 — sixteen big shards: v10's ten plus the six added for v11 (~370–379M
positions each, 32-byte `bullet::ChessBoard` records, already bulletformat so the text→`convert` step is
skipped). The **only** change from v11 is the architecture — bucket count 4→8 — so the *data* steps (1–3)
are identical to v11; only the arch and the spot-check size differ. GPU training is nondeterministic, so
this reproduces a *functionally equivalent* net, not a byte-identical one.

**The defining requirement: the architecture (especially the king-bucket map) must match everywhere.**
`kHiddenSize=1024`, `kNumKingBuckets=8`, and the **file-pair × board-half `kKingBucketMap`** in
[src/nnue.h](../src/nnue.h) must be byte-identical to `HIDDEN_SIZE` and the
`KING_BUCKETS` array in [tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs) and
[tools/bullet/pawnstar_eval.rs](../tools/bullet/pawnstar_eval.rs) (and `NUM_BUCKETS=8` in `pawnstar_eval.rs`).
The repo is already on this arch, so the checked-in files match; the map is:

```
// kKingBucketMap / KING_BUCKETS: bucket = king_file / 2 (a/b->0, c/d->1, e/f->2, g/h->3) for the king's
// own half (ranks 1-4), and +4 (banks 4-7) once the king is advanced into ranks 5-8. Indexed by each
// perspective's own king square (white directly; black square ^ rankflip).
0, 0, 1, 1, 2, 2, 3, 3,   // ranks 1-4 (king's own half)
0, 0, 1, 1, 2, 2, 3, 3,
0, 0, 1, 1, 2, 2, 3, 3,
0, 0, 1, 1, 2, 2, 3, 3,
4, 4, 5, 5, 6, 6, 7, 7,   // ranks 5-8 (king advanced)
4, 4, 5, 5, 6, 6, 7, 7,
4, 4, 5, 5, 6, 6, 7, 7,
4, 4, 5, 5, 6, 6, 7, 7,
```

```bash
cd /path/to/pawnstar                 # repo root (so ./build and nnue/ paths resolve)
make                                 # build the engine (1024-wide, 8-king-bucket arch)
make tools                           # build build/stamp_net
make tests                           # build test_nnue / test_nnue_incremental / test_bratko_kopec_nnue

# 0. One-time: install the bullet trainer (pinned commit) + examples, build utils + trainer + eval FRESH.
#    GOTCHA: setup_bullet.sh copies tools/bullet/*.rs into the bullet checkout; if you ever edit the repo
#    .rs you MUST re-copy + rebuild before training, or a stale binary trains the wrong arch (cost a run).
tools/setup_bullet.sh
BULLET=~/pawnstar_nnue/bullet
export CUDA_PATH=/usr/local/cuda-13.3 PATH="$CUDA_PATH/bin:$PATH" LD_LIBRARY_PATH="$CUDA_PATH/lib64"
( cd "$BULLET/crates/utils"      && cargo build --release --bin bullet-utils )
( cd "$BULLET/crates/bullet_lib" && cargo build --release --features cuda --example pawnstar --example pawnstar_eval )
UTILS="$BULLET/target/release/bullet-utils"
EVAL="$BULLET/target/release/examples/pawnstar_eval"
mkdir -p ~/pawnstar_nnue/v12 && cd ~/pawnstar_nnue/v12

# 1. Assemble the pool: put sixteen big bulletformat shards (~11–12 GB each, ~190 GB total, ~370–379M
#    positions apiece) in this directory as <name>.data files, from whichever public bulletformat
#    dataset you train on.
SHARDS=( *.data )

# 2. Validate each (size % 32 == 0, no invalid records). A corrupt shard trains silently wrong,
#    so ALWAYS validate before training; drop and replace any that fail.
for sh in "${SHARDS[@]}"; do "$UTILS" validate --input "$sh"; done

# 3. Concatenate the sixteen shards (flat 32-byte records) and re-shuffle (concatenated shards are correlated).
#    --mem-used-mb: size to free RAM (16384 if you have it; 8192 on a 16 GB box).
cat "${SHARDS[@]}" > all.data
"$UTILS" shuffle --input all.data --output shuffled.data --mem-used-mb 16384
echo "pool: $(( $(stat -c%s shuffled.data) / 32 )) positions"   # ~6.05e9
rm -f all.data                                                   # reclaim disk

# 4. Train the 1024-wide, 8-king-bucket arch. BPS=3688 and SBS=300 => ~18.1B samples seen (~3 epochs of
#    the 6.05B pool). LR schedule auto-scales (StepLR step = SBS/3). ~50 min on an RTX 4070 (~2.4M pos/s);
#    ~14.7 h on a GTX 1050 Ti (~0.34M pos/s). PAWNSTAR_SAVE_RATE=1 checkpoints every superbatch so you can
#    spot-check the FIRST one (below).
( cd "$BULLET/crates/bullet_lib" && \
  PAWNSTAR_DATA=~/pawnstar_nnue/v12/shuffled.data PAWNSTAR_BPS=3688 PAWNSTAR_SBS=300 \
  PAWNSTAR_SAVE_RATE=1 PAWNSTAR_OUT=~/pawnstar_nnue/v12/checkpoints \
  cargo run --release --features cuda --example pawnstar )   # drop --features cuda to train on CPU

# 4b. SPOT-CHECK at SB1 (~30 s in): confirm the net is genuinely 8-bank before sinking the full run.
#     An 8-bucket quantised.bin is 12,589,120 bytes (6144 FT rows = 768*8); a 4-bucket one is 6,297,664.
python3 -c "import os;s=os.path.getsize('checkpoints/pawnstar-1/quantised.bin');print(s, '8-bank' if s>10_000_000 else 'WRONG (not 8-bank)')"

# 5. Stamp the final RAW net into the shipped path (stamp_net embeds this build's arch header, §2).
RAW=~/pawnstar_nnue/v12/checkpoints/pawnstar-300/quantised.bin
./build/stamp_net "$RAW" /path/to/pawnstar/nnue/pawnstar-v12.bin   # writes a 12,589,152-byte stamped file

# 6. Verify. GOTCHA: pawnstar_eval is NOT header-aware -> feed it the RAW net; test_nnue /
#    test_nnue_incremental load through the engine, which only accepts a STAMPED net (the raw path was
#    removed), so pass them the stamped net. (tools/verify_net.sh automates this: it runs pawnstar_eval on
#    the raw net and stamps a scratch copy for the engine tests.)
cd /path/to/pawnstar
BULLET=~/pawnstar_nnue/bullet tools/verify_net.sh "$RAW"          # cross-check (<=2 cp) + incremental
cut -d'|' -f1 test/nnue_reference.txt > /tmp/fens.txt            # reuse the checked-in FENs (first field)
"$EVAL" "$RAW" /tmp/fens.txt > test/nnue_reference.txt           # re-baseline the checked-in reference (RAW net)

# 7. Regenerate the Bratko-Kopec accepted moves (the eval changed), then a full check. The search is
#    single-threaded/deterministic per depth; the accepted set is the union of the engine's best moves over
#    depths 8-11. Capture them straight from the harness's own output (`got=<move>` per position):
for d in 8 9 10 11; do ./build/test_bratko_kopec_nnue nnue/pawnstar-v12.bin $d 2>&1 \
    | grep -oE "pos[0-9]+ got=[a-h][0-9][a-h][0-9][nbrq]?"; done   # union per pos -> kCases in
                                                                   # test/bratko_kopec_nnue_test.cpp
make clean && make && make check    # rebuild the engine (re-embeds the new net via embedded_net.S .incbin),
                                    # then run all suites; BK must be 24/24 at depths 8-11

# 8. Strength-test vs the previous net (v11). v12 (8-bucket) and v11 (4-bucket) are DIFFERENT archs with
#    DIFFERENT file sizes, so this is a CROSS-ARCH SPRT: the v11 baseline needs its own engine build (a
#    4-bucket binary that can load v11), and the v12 candidate binary's default/embedded net is the new
#    8-bucket arch — so the v11 binary cannot setoption-load v12 and vice-versa. Point BIN_A/BIN_B at the two
#    separately-built engines and give the candidate its startup net via CAND_STARTUP_NET (run_sprt.sh wraps
#    it). v12 won +17.29 ± 8.68 Elo @ 8+0.08 and +10.97 ± 6.57 @ 12+0.12 (both H1).
CAND_STARTUP_NET=nnue/pawnstar-v12.bin BIN_A=/path/to/pawnstar-v12-engine BIN_B=/path/to/pawnstar-v11-engine \
  BASELINE_NET=/path/to/pawnstar-v11.bin TC=8+0.08 \
  tools/run_sprt.sh nnue/pawnstar-v12.bin /path/to/openings.epd 20000
```

To train a **stronger** net, add yet more clean shards at steps 1–3 (`validate` each first, and use a
data source you hold the rights to) and scale `SBS` to keep ~3 epochs. To change the
**architecture** — width (`kHiddenSize`), bucket count (`kNumKingBuckets`) or the `kKingBucketMap` —
change it identically in `nnue.h`, `pawnstar.rs` and `pawnstar_eval.rs` (and `NUM_BUCKETS` in
`pawnstar_eval.rs`), re-sync + rebuild the bullet examples, rebuild the engine, and retrain. A different
architecture yields a different file size, so an engine built for one cannot load a net of another (§2).

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
- **`NNUE: net '…' is not a stamped Pawnstar net (missing 'PSN1' header)`** — you handed the engine a raw
  (unstamped) bullet net. The engine only loads **stamped** nets now (the headerless-raw path was removed);
  stamp it first with `tools/stamp_net <raw> <stamped>`.
- **`NNUE: net '…' is incompatible — file is …, engine expects …`** — a stamped net whose header arch
  fields don't match this build (wrong `HIDDEN_SIZE`, bucket count, quantisation). The stamped header is
  validated field-by-field against the engine's compile-time constants, so an incompatible net is never
  silently misread — including a *larger* (e.g. wider) one. (`stamp_net` itself only accepts a *raw* net of
  the exact payload size, so it can't even produce a wrong-arch stamped net for this build.)
- **Engine "disconnects" mid-match** — the old long-game history overflow is fixed (the game history is
  a dynamic `std::vector` now), so this should be rare; if it recurs it is an actual crash worth a debug
  build. Adjudication (`run_sprt.sh`) is still recommended to keep matches short and decisive.
- **bullet build links `-lcuda` against the system driver** (`/usr/lib/x86_64-linux-gnu/libcuda.so`),
  not the toolkit, so the *driver* version is what gates `cuFuncLoad`; a newer toolkit alone won't help.
