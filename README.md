# Pawnstar

Pawnstar is a [UCI](https://en.wikipedia.org/wiki/Universal_Chess_Interface) chess engine written in
C++20. It uses a bitboard board representation, a parallel alpha-beta search (Lazy SMP),
a lockless transposition table, and a tapered hand-crafted evaluation.

Legal move generation runs at roughly 700 million moves per second on a modern laptop.

## Features

- Bitboard board representation with BMI2 `pext`-based sliding-piece attacks.
- Fully legal move generator (pins, checks, en passant edge cases, and castling handled directly).
- Parallel iterative-deepening alpha-beta search with principal variation search (PVS).
- Lazy SMP parallelism: independent per-thread searches sharing one lockless transposition table.
- Lockless 128-bit transposition table using Hyatt's XOR trick.
- Null-move pruning, late-move reductions, killer and per-thread history move ordering.
- Quiescence search over captures with its own transposition table.
- Tapered evaluation: material, piece-square tables, pawn structure, king safety and mobility.
- Optional **NNUE** evaluation (efficiently-updatable neural network), switchable at runtime.
- Optional opening book (`pawnstar.book`).

## Requirements

- `clang++` with C++20 support.
- A CPU with the BMI2 instruction set (Intel Haswell / AMD Zen 1 or later); the build passes `-mbmi2`.
- GNU `make`.
- `doxygen` (and optionally Graphviz `dot`) only if you want to build the API docs.

## Build

```bash
make           # build/pawnstar
make check     # build and run all test suites
make doc       # generate Doxygen HTML into doc/html
make clean     # remove build artifacts and generated docs
```

A pre-compilation step builds and runs `generate_constants/generate_constants.cpp`, which emits
`src/generated_data.cpp` (the precomputed bitboard, Zobrist and pawn-structure tables). This file is
gitignored and regenerated on every build.

Debug build with AddressSanitizer + UndefinedBehaviorSanitizer:

```bash
make DEBUG=1
```

Release build for benchmarking, which omits the `DEBUGX` diagnostic counters:

```bash
make RELEASE=1
```

The `DEBUGX` diagnostic counters (the `dbg` command's data) are compiled in by default and cost roughly
6% on the search hot path; `RELEASE=1` leaves them out. It composes with `DEBUG=1` independently.

After switching branches or changing generated files, run `make clean && make` — stale `.d`
dependency files can otherwise cause spurious build failures.

## Usage

Run as a UCI engine (connect via Arena, Cute Chess, or any UCI-compatible GUI):

```bash
./build/pawnstar
```

In addition to the standard UCI commands, a few engine-specific commands are available at the prompt:

```
eval        print the static evaluation of the current position (and which evaluator is active)
nnue        print the raw NNUE evaluation of the current position
getboard    print the FEN of the current position
bookmoves   list opening-book moves for the current position
dbg         dump internal diagnostic counters
help        list all commands
```

### Enabling NNUE evaluation

NNUE is **off by default** (the hand-crafted evaluation is the engine's normal evaluator). Turn it on
with the standard UCI `setoption` mechanism — point `EvalFile` at a trained net, then enable `UseNNUE`:

```
setoption name EvalFile value /path/to/net.bin
setoption name UseNNUE value true
```

Equivalently, via environment variables at launch:

```bash
PAWNSTAR_EVALFILE=/path/to/net.bin PAWNSTAR_NNUE=1 ./build/pawnstar
```

`eval` reports which evaluator is in use. See [NNUE evaluation](#nnue-evaluation-experimental) below for
how it works and how to train a net.

## Architecture

### Board representation

`Position` ([src/position.h](src/position.h)) is the central immutable state class. It stores
per-piece-type bitboards in `pieces_` and per-colour bitboards in `colors_`, a 64-entry square→piece
array for O(1) lookup, the incrementally-maintained Zobrist hash, castling rights, the en passant
square, the half-move clock, and a `checkers` bitboard. `MakeMove` and `MakeNullMove` return a brand
new `Position` by value (copy-make); there is no unmake.

`pieces_` is a `std::array<Bitboard, 7>` indexed by piece type, but **`pieces_[0]` (the `kNone` slot)
holds the occupied-squares set** — the union of all pieces of both colours — rather than a piece
bitboard; the six real piece types occupy indices 1–6. This lets occupancy queries and the `pext`-based
attack lookups read it directly without recomputing a union.

`Bitboard` ([src/bitboard.h](src/bitboard.h)) wraps a `uint64_t` with the usual set operations plus a
range-based iterator that yields the `Square` of each set bit, enabling `for (Square s : bb)`.

### Move encoding

`Move` ([src/move.h](src/move.h)) packs everything into a single 64-bit word:

| Bits | Field |
|---|---|
| 0–5 | to square |
| 6–11 | from square |
| 12–14 | moving piece |
| 15–17 | captured piece |
| 18–21 | move type (capture, promotion, castling, en passant, double push, …) |
| 22 | gives-check flag |
| 23–24 | transposition node type (CUT / ALL / PV) |
| 25–32 | transposition depth (int8) |
| 33–40 | transposition age / generation (uint8) |
| 41–63 | score (signed 23-bit) — move-ordering score, and the stored TT score |

The upper bits (23–63) are scratch ordering space during search and carry the transposition metadata
once a move is stored in the table. Because all of that lives in the move, a transposition entry is
just a `{hash, move}` pair. `MoveList` / `StackList` are fixed-capacity stack containers, so no heap
allocation happens during move generation or search.

### Attack generation

Sliding-piece attacks (bishop, rook, queen) use the BMI2 `_pext_u64` instruction to gather the
occupancy bits relevant to a square and index a precomputed per-square attack table. Knight, king and
pawn attacks are plain 64-entry lookup tables. All tables are generated at build time into
`src/generated_data.cpp`. `Pins` ([src/pins.h](src/pins.h)) computes absolute pins and the squares
each pinned piece may still move to, before move generation.

### Search

The search is a fail-soft negamax with alpha-beta pruning, driven by iterative deepening and
parallelised with Lazy SMP: several threads each run an independent whole-tree search, sharing a single
lockless transposition table rather than splitting the tree between them.

#### Iterative deepening and time management

`SearchRootNode` ([src/search_root_node.cpp](src/search_root_node.cpp)) runs on a dedicated worker
thread — started by `Game::StartThinking` — so the UCI `stop` command, which sets an atomic
cancellation flag polled throughout the tree, can interrupt it at any time. It searches the root move
list at increasing depths starting from `kStartDepth` (3) up to `kMaxPly` (64), emitting a
`info depth … score cp … pv …` line each time the best move improves. After every iteration the root
moves are re-sorted by their scores from the previous iteration, so the prior best move is tried first
next time. The root searches a full `[kAlpha, kBeta]` window (no aspiration windows); alpha is raised
in place as root moves beat it.

Under a standard (time-budgeted) clock the engine can stop *before* the allotted time is spent, using
the stability of the result across iterations:

- if the best move has been **consistent** across iterations **and** the score has stayed within
  `kScoreInstabilityThreshold` (50 cp), it stops once it has used a quarter of the time budget;
- if **either** the move or the score is stable, it stops at half the budget;
- otherwise it runs until the full budget is reached, or until a forced mate is found
  (`|score| > kWinThreshold`).

This avoids burning the whole clock on positions whose answer is already settled. Fixed-depth and
fixed-time clocks bypass the heuristic.

#### Recursive search

`Search` ([src/search_alphabeta.cpp](src/search_alphabeta.cpp)) applies, at each node:

- **Transposition table** probe/store with CUT / ALL / PV node types; a TT cutoff or TT move ordering
  hint is taken before any moves are generated.
- **Principal variation search (PVS)** — the first move is searched with the full window; later moves
  get a null-window (`[alpha, alpha+1]`) scout search and are only re-searched with the full window if
  the scout beats alpha.
- **Null-move pruning** — when not in check and the previous move was not itself a null move, a null
  move is searched at reduced depth (`depth − 4`, i.e. R = 3); a resulting score `≥ beta` prunes the
  node.
- **Late-move reductions (LMR)** — at null-window nodes, any late move (past the 4th) outside a check
  sequence at `depth > 2` is searched one ply shallower, with a second ply shaved off past the 7th
  move at `depth > 3`. A reduced search that beats alpha is re-searched at full depth.
- **Search extensions** for promotions and en passant captures.
- **Move ordering** — the TT move first, then winning/equal captures and promotions by static exchange
  evaluation, then the two killer moves for the ply, then quiet moves by history-heuristic count, and
  finally losing captures (negative SEE) below the quiet moves. A quiet move that causes a beta cutoff
  is recorded as a killer and rewarded in the per-thread history table.

**Quiescence search** ([src/search_quiescent.cpp](src/search_quiescent.cpp)) extends the leaves with
captures only, using a separate transposition table, so the static evaluation is never called on a
position with a hanging capture available. It applies **SEE pruning** — a capture that loses material
by static exchange evaluation and does not give check is skipped rather than searched.

#### Parallel search (Lazy SMP)

Parallelism is provided by **Lazy SMP**: `SearchRootNode` launches `hardware_concurrency()` threads
(capped at `kMaxSearchThreads`), each running its *own* complete iterative-deepening search of the root
position via `IterativeDeepen`. There is no tree splitting and no work queue — the threads cooperate
purely through the shared lockless transposition table, where one thread's deeper or earlier results
prefill the entries another thread will probe. Each thread owns its own `SearchState` (position stack,
hash history, killers, node count) **and its own history table**, so the only cross-thread sharing is
the transposition tables and the cancellation flag; there is no per-(ply, move) counter contention.

The thread count can be overridden for testing and benchmarking with the `PAWNSTAR_THREADS`
environment variable (e.g. `PAWNSTAR_THREADS=1` forces a deterministic single-threaded search).

To keep the helper threads from recomputing the same tree in lockstep, the main thread (thread 0)
searches every depth while each helper follows a Stockfish-style **iteration-skip schedule**
(`kSkipSize` / `kSkipPhase` tables in `search_root_node.cpp`) that makes it skip selected depths. The
threads therefore desynchronise and seed the shared table with a diverse mix of depths. Only the main
thread prints `info` lines, applies time management and produces the authoritative best move; helpers
run silently and stop as soon as the shared cancellation flag is set.

Because the helpers race ahead through the table, a search run to a nominal depth is *non-deterministic*
and the returned best move (and its exact score) can vary slightly between runs — a known and accepted
property of Lazy SMP. `PAWNSTAR_THREADS=1` recovers determinism when that matters.

#### Tuning constants

The knobs above live in [src/constants.h](src/constants.h):

| Constant | Value | Meaning |
|---|---|---|
| `kStartDepth` | 3 | First (full-width) iterative-deepening depth |
| `kMaxPly` | 64 | Hard ceiling on search depth / ply |
| `kHashtableMegabytes` | 64 | Main transposition table size |
| `kQHashtableMb` | 8 | Quiescence transposition table size |
| `kScoreInstabilityThreshold` | 50 | Centipawn window for the "score stable" time-management test |
| `kMaxSearchThreads` | 256 | Upper bound on Lazy SMP search threads |

### Transposition table

Two tables are used: 64 MB for the main search and 8 MB for quiescence. Each entry is exactly 128
bits — a 64-bit Zobrist hash and a 64-bit move whose spare bits carry the score, depth, node type and
age.

The table is **lockless** (Hyatt's XOR trick): each cell stores `key = hash ^ data` alongside `data`
(the packed move), as two `std::atomic<uint64_t>` accessed with relaxed ordering. A reader accepts a
cell only if `key ^ data` equals the probe hash, so a torn pair written by two concurrent stores is
detected and treated as a miss. Concurrent writers race benignly (last writer wins). Aging is O(1):
`Age()` bumps a generation counter and an entry is stale when its stamped age differs from it.

### Evaluation

`EvaluatePosition` ([src/evaluation.cpp](src/evaluation.cpp)) is tapered between opening/middlegame and
endgame based on remaining material, and sums:

- **Material** with a bishop-pair bonus.
- **Piece-square tables** per piece type, with separate midgame and endgame king tables.
- **Pawn structure** — passed, isolated, unsupported, doubled and defended pawns.
- **King safety** — pawn-shelter and friendly-piece-proximity terms, open-file penalties.
- **Mobility** — bishop and rook mobility, excluding pawn-defended targets.

Static exchange evaluation (`EvaluateStaticExchange`,
[src/static_exchange_evaluation.h](src/static_exchange_evaluation.h)) resolves a capture sequence on a
square; it is used to order captures and promotions during search (see above) and is covered by
`test_see`.

### NNUE evaluation (experimental)

Pawnstar can optionally replace the hand-crafted evaluation with an **NNUE** (Efficiently Updatable
Neural Network) — a small neural net trained to score positions. It is off by default and selected at
runtime (see [Enabling NNUE evaluation](#enabling-nnue-evaluation) above); the hand-crafted evaluation
remains the default.

**How it works.** The network ([src/nnue.cpp](src/nnue.cpp), [src/nnue.h](src/nnue.h)) is a simple
"perspective" net using the standard `Chess768` feature set:

```
768 inputs per perspective  ->  256 feature transformer (x2 perspectives)
SCReLU,  concat[side-to-move | opponent] = 512  ->  1 output (centipawns, side-to-move relative)
```

The 768 inputs are (piece colour × piece type × square). Two accumulators are built — one from the
side-to-move's perspective and one from the opponent's (with the board vertically mirrored) — passed
through a squared-clipped-ReLU activation and a single output layer that produces a centipawn score.
`EvaluatePosition` branches to the net when NNUE is enabled, bypassing the hand-crafted terms. The
network is an `nnue::Network` instance owned by the `Game` (no globals), read-only after load and shared
by all search threads. Its accumulator is **maintained incrementally** across make/undo on each thread's
`SearchState` — only the feature columns for the pieces that moved are updated, rather than rebuilding it
from scratch every node — which makes NNUE competitive on equal time, not just equal depth. Weights are
quantised `int16` (`QA=255`, `QB=64`, output scaled by `SCALE=400`) and loaded directly from the
[bullet](https://github.com/jw1912/bullet) trainer's native `.bin`.

**Training a net.** All tooling lives in [tools/](tools) and is documented in
[nnue/README.md](nnue/README.md). The network is trained on positions from Pawnstar's own self-play,
labelled with the search score and the game result, using the bullet trainer (GPU or CPU). The
minimal-setup flow:

```bash
tools/setup_bullet.sh                                   # clone + register the bullet trainer (pinned commit)
make tools                                              # build build/gen_data
tools/run_gendata.sh ~/pawnstar_nnue/data 12 100000 8   # generate self-play data (12 procs, depth 8)
tools/run_experiment.sh ~/pawnstar_nnue/data net.bin    # build openings, train, then SPRT vs the HCE
```

`run_experiment.sh` produces `net.bin`, which you can then load with `setoption name EvalFile`. The
in-engine inference is verified against the trainer by `test_nnue`, and the incremental accumulator
against a full refresh by `test_nnue_incremental` (see [Test suite](#test-suite)); GPU training needs a
CUDA toolkit and a driver supporting CUDA ≥ 12.3, otherwise set `BULLET_FEATURES=""` to train on CPU.
Because NNUE scores differ from the hand-crafted scores, NNUE strength is measured by game play (an SPRT
of NNUE vs the hand-crafted eval), not by `test_bratko_kopec`.

**Shipped nets.** Two trained nets live in [nnue/](nnue): `nnue/pawnstar-v1.bin` (Pawnstar self-play
labels) *loses* to the HCE by ~67 Elo, whereas `nnue/pawnstar-v2.bin` (trained on **public PlentyChess
self-play** — strong-engine labels) *beats* the HCE by a wide margin at fixed depth. The lesson was label
quality, not quantity. See [nnue/README.md](nnue/README.md) §7 for the numbers and exact step-by-step
recreation of `pawnstar-v2.bin`.

### Opening book

`OpeningBook` ([src/opening_book.h](src/opening_book.h)) lets the engine play known opening lines
instantly instead of searching. At startup the engine loads `pawnstar.book` from the working directory;
the shipped book ([pawnstar.book](pawnstar.book)) is derived from the public-domain
[TSCP](https://github.com/terredeciels/TSCP) `book.txt`.

The file is plain text, one opening line per row, with each move in **coordinate notation**
(from-square then to-square, e.g. `e2e4 e7e5 g1f3`). Tokens that begin with a digit are treated as
move numbers and ignored, and a `#` begins a comment to end of line, so PGN-style numbered lines are
also accepted. Each line is replayed from the standard start position and every move it visits is
recorded.

Internally the book is a `std::map` from position Zobrist hash to a vector of legal `Move`s, *with
repeats*: a move that appears in many lines is stored many times, so picking uniformly at random
(`GetMove`) naturally weights selection by how often the move occurs. `SearchRootNode` consults the
book before searching and, on a hit, returns the chosen move immediately. The `bookmoves` UCI command
prints the available book moves and their frequencies for the current position.

## Test suite

```bash
make check
```

builds and runs seven standalone test executables:

| Executable | Description |
|---|---|
| `build/test_see` | 24 static exchange evaluation cases |
| `build/test_pawn_structure` | 12 pawn structure tests, 72 individual checks |
| `build/test_perft` | Move-generation regression on the standard PERFT positions |
| `build/test_bratko_kopec` | 24 Bratko-Kopec search positions, handcrafted eval (default depth 8) |
| `build/test_bratko_kopec_nnue` | The same 24 positions searched with the NNUE net; reports moves solved |
| `build/test_nnue` | NNUE inference cross-check against the trainer's reference evals |
| `build/test_nnue_incremental` | Incremental accumulator vs full-refresh consistency check |

`make check` runs the NNUE tests against the shipped `nnue/pawnstar-v2.bin`: `test_nnue` against the
checked-in `test/nnue_reference.txt` (250 trainer evals; max |diff| 0 cp), `test_nnue_incremental` which
asserts the incremental accumulator matches a full refresh at every node, and `test_bratko_kopec_nnue`
which searches the 24 BK positions with the net (single-threaded, deterministic) and reports how many of
the documented best moves it finds (currently 18/24 at depth 8). The Makefile passes the net/reference
via `$(wildcard …)`, so these degrade to a green no-op only if those files are absent; regenerate
`test/nnue_reference.txt` with the bullet `pawnstar_eval` example if you ship a new net (see
[nnue/README.md](nnue/README.md)). `test_bratko_kopec_nnue` gates only on search correctness (a legal
move for every position), not the solve count, so a retrained net cannot break the build.

The handcrafted Bratko-Kopec test (`test_bratko_kopec`) checks the parallel *score* against baked
references; the NNUE variant checks the *move*, since NNUE scores are on a different scale. NNUE does not
change the handcrafted Bratko-Kopec scores, so leave `UseNNUE` off when regenerating those references.

The Bratko-Kopec test takes an optional depth argument in the range 8–12, e.g.
`./build/test_bratko_kopec 12`. Because the Lazy SMP search is non-deterministic, the test does not
check the move played — it checks the **score**. Each position stores baked single-threaded reference
scores for depths 8–12 (regenerate them with `PAWNSTAR_THREADS=1 ./build/test_bratko_kopec 12`), and a
position passes when its parallel score falls within the span of those references across all depths
8–12, widened by ±50 cp. (Lazy SMP desynchronizes effective depth in both directions, so a nominal
depth-D result can match any stored depth's score.) At the shallowest depth (8) the search is least
stable, so depth 8 additionally accepts any best move recorded in that position's depth-8 move set. This
tolerates the legitimate move/score variation of Lazy SMP while still catching real search or evaluation
regressions. The suite passes 24/24 at every depth from 8 through 12.

## Documentation

```bash
make doc      # writes HTML to doc/html (open doc/html/index.html)
```

The codebase is fully Doxygen-commented; `make doc` generates the browsable API reference.

## Repository layout

| Path | Contents |
|---|---|
| `src/` | Engine source and headers |
| `test/` | Standalone test programs |
| `tools/` | NNUE data generation, bullet trainer sources, and training/SPRT scripts |
| `nnue/` | NNUE training pipeline documentation |
| `generate_constants/` | Build-time generator for the precomputed lookup tables |
| `Doxyfile` | Doxygen configuration |
| `LICENSE` | GNU General Public License v3 |

## Design notes

A few deliberate choices shape the codebase:

- **Immutable, copy-make positions.** `Position` is never mutated in place; `MakeMove` returns a new
  one by value and there is no `UnmakeMove`. This trades a per-ply copy for far simpler code: no
  state to restore, no aliasing between plies, and positions that are trivially safe to hand to other
  threads. The class is kept small (160 bytes) so the copy stays cheap.
- **No heap allocation on the search hot path.** Move lists and the per-thread search position stack are
  fixed-capacity stack containers. The per-search hash history is a `std::vector` reserved once when the
  `SearchState` is constructed, and the game history is a `std::vector` that grows only as real moves are
  played — so the node-by-node search loop itself performs no dynamic allocation.
- **Everything precomputed that can be.** Attack tables, Zobrist keys and pawn-structure masks are
  emitted at build time by `generate_constants`, so the engine binary starts instantly and the hot
  paths are pure lookups.
- **Lockless sharing.** Under Lazy SMP the transposition tables and the cancellation flag are the only
  cross-thread state — the history table and killers are per-thread — and the shared state is accessed
  with atomics (the TT via Hyatt's XOR trick) rather than locks, so threads never block one another
  mid-search.

## Benchmarks

Pawnstar has no official CCRL rating. The figures currently tracked are:

- **Move generation** — roughly 700 million legal moves per second on a modern laptop core.
- **Bratko-Kopec** — 24/24 at every depth from 8 through 12 (the default depth 8 runs in `make check`).
- **Strength (rough).** The hand-crafted evaluation measures ~2350 Elo (CCRL-ballpark, anchored against
  reference engines at a fast time control). `nnue/pawnstar-v2.bin` beats the HCE by a large margin at
  fixed depth, and the incremental accumulator is worth a further ~+80 Elo over the full-refresh version
  at equal time (SPRT).

`make check` is the regression baseline; the perft suite verifies move-generation correctness against
the published node counts for the standard positions.

## Limitations and known issues

- **BMI2 required.** The sliding-piece attacks depend on the `_pext_u64` instruction, so the engine
  needs an Intel Haswell / AMD Zen 1 (or newer) CPU and will not run without `-mbmi2`. On early AMD
  Zen parts `pext` is microcoded and comparatively slow.
- **Thread count** defaults to `hardware_concurrency()` and is not exposed as a UCI option, though it
  can be overridden with the `PAWNSTAR_THREADS` environment variable.
- **No pondering** (thinking on the opponent's time) and **no syzygy/tablebase** support.
- **NNUE is experimental.** It is off by default; the hand-crafted evaluation is the normal evaluator.
  The accumulator is maintained incrementally across make/undo, but the forward pass is still scalar (no
  SIMD yet), so each node's NNUE eval remains somewhat heavier than the hand-crafted eval.

## Contributing

1. Build and run the full test suite before and after your change:
   ```bash
   make clean && make check
   ```
2. Format any touched sources with clang-format (Microsoft base style, 120-column limit; config in
   `.clang-format`):
   ```bash
   clang-format -i src/<file>            # or your editor's format-on-save
   ```
   The generated `src/generated_data.cpp` is exempt — it is machine-emitted and gitignored.
3. Only edit `generate_constants/generate_constants.cpp` when the precomputed-table logic actually
   changes; never edit `src/generated_data.cpp` by hand.
4. Keep new public declarations Doxygen-commented so `make doc` stays complete.

## License

Pawnstar is free software, licensed under the **GNU General Public License, version 3** (GPLv3). You may
redistribute and/or modify it under the terms of that license; it comes with **no warranty**. See the
[LICENSE](LICENSE) file for the full text, or <https://www.gnu.org/licenses/gpl-3.0.html>.

The bundled opening book ([pawnstar.book](pawnstar.book)) is derived from the public-domain
[TSCP](https://github.com/terredeciels/TSCP) `book.txt`. The NNUE trainer sources under
[tools/bullet/](tools/bullet) are written for the [bullet](https://github.com/jw1912/bullet) trainer
(MIT-licensed), which is fetched separately by `tools/setup_bullet.sh` and is not redistributed here.
