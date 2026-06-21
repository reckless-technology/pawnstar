# Pawnstar

Pawnstar is a [UCI](https://en.wikipedia.org/wiki/Universal_Chess_Interface) chess engine written in
C++20. It uses a bitboard board representation, a parallel alpha-beta search (Lazy SMP),
a lockless transposition table, and a perspective NNUE evaluation.

Legal move generation runs at roughly 600 million moves per second on a modern laptop.

## Features

- Bitboard board representation with BMI2 `pext`-based sliding-piece attacks.
- Fully legal move generator (pins, checks, en passant edge cases, and castling handled directly).
- Parallel iterative-deepening alpha-beta search with principal variation search (PVS).
- Lazy SMP parallelism: independent per-thread searches sharing one lockless transposition table.
- Lockless 128-bit transposition table using Hyatt's XOR trick.
- Internal iterative reduction, reverse futility pruning, null-move pruning, late-move pruning + reductions,
  and per-thread move ordering (killers, countermove, main + 1-ply continuation history).
- Quiescence search over captures and promotions (SEE-pruned).
- **NNUE** evaluation (efficiently-updatable neural network) — a perspective net (currently 1024-wide with 4 king buckets). A net is loaded at startup; load a different one at runtime with `setoption name EvalFile`.
- Configurable `Hash` and `Threads` UCI options, and **pondering** (think on the opponent's clock).
- Optional opening book (`pawnstar.book`).

## Requirements

- `clang++` with C++20 support.
- A CPU with the BMI2 and AVX2 instruction sets (Intel Haswell / AMD Zen 1 or later); the build passes `-mbmi2 -mavx2`. BMI2 is required for the `pext` instruction (sliding move generation) and AVX2 is required for the NNUE evaluation SIMD calculation.
- GNU `make`.
- `doxygen` (and optionally Graphviz `dot`) only if you want to build the API docs.

## Build

```bash
make           # build/pawnstar
make check     # build and run all test suites
make tests     # build the test executables without running them
make doc       # generate Doxygen HTML into doc/html
make clean     # remove build artifacts and generated docs
```

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
eval        print the full evaluation the search uses for the current position
nnue        print the raw NNUE network output for the current position
getboard    print the FEN of the current position
bookmoves   list opening-book moves for the current position
dbg         dump internal diagnostic counters
help        list all commands
```

`eval` and `nnue` are **not** duplicates. `eval` is the value the search actually uses: the draw
short-circuit (0 for a material/fifty-move/repetition draw), then the NNUE forward pass (memoised in the
eval cache), then fifty-move scaling (`ScaleByRule50`, which fades the score toward 0 as the fifty-move
clock climbs past 20 plies). `nnue` is the **bare** network output with none of that wrapping. They agree in a
plain position but diverge on a draw or once the fifty-move clock is high.

### UCI options

Pawnstar advertises these `setoption` options in its `uci` response:

- **`Hash`** (spin, default 64, 1–4096 MB) — transposition-table size; resized at runtime (`setoption name Hash value <MB>`).
- **`Clear Hash`** (button) — empties the transposition table without resizing it.
- **`Threads`** (spin, default = `PAWNSTAR_THREADS` or all cores, 1–256) — number of Lazy SMP search threads.
- **`Move Overhead`** (spin, default 30, 0–5000 ms) — time reserved from each move's deadline to cover GUI/network lag, so Pawnstar doesn't forfeit on time.
- **`EvalFile`** (string) — path to an NNUE net to load at runtime (see below).
- **`Ponder`** (check) — advertised so GUIs know Pawnstar can ponder. **Its value is declarative only** — the
  GUI's checkbox controls whether the GUI itself uses pondering; Pawnstar ignores the `setoption … Ponder`
  value (per the UCI spec it only signals that pondering is *allowed*, e.g. to adjust time management, which
  Pawnstar doesn't do). Pondering is driven entirely by the `go ponder` command: when the GUI sends it,
  Pawnstar thinks on the opponent's clock and reports `bestmove <move> ponder <reply>`; on a ponder *hit*
  (`ponderhit`) it keeps the work already done (warm TT, current depth) and converts to a normally-timed
  search; on a *miss* the GUI sends `stop` and Pawnstar returns its move, then searches the real position
  afresh.

### The NNUE net

NNUE is Pawnstar's **only** evaluator, so a net is required. At startup the engine loads the shipped net
`nnue/pawnstar-v10.bin` (resolved relative to the working directory, like `pawnstar.book`). If that file
can't be loaded (wrong cwd, missing/renamed file), the engine **falls back to a copy of the net embedded
in the binary** at build time, so it always has a working evaluator; it only errors out if both the file
and the embedded copy fail. To use a different net:

```
setoption name EvalFile value /path/to/net.bin      # load a different net at runtime
```

or via an environment variable at launch:

```bash
PAWNSTAR_EVALFILE=/path/to/net.bin ./build/pawnstar   # use a different net
```

See [NNUE evaluation](#nnue-evaluation) below for how it works and how to train a net.

## Architecture

### Board representation

`Position` ([src/position.h](src/position.h)) is the central immutable state class. It stores
per-piece-type bitboards in `pieces` and per-colour bitboards in `colors` (public members), a 64-entry square→piece
array for O(1) lookup, the incrementally-maintained Zobrist hash, castling rights, the en passant
square, the half-move clock, and a `checkers` bitboard. `MakeMove` and `MakeNullMove` return a brand
new `Position` by value (copy-make); there is no unmake.

`pieces` is a `std::array<Bitboard, 7>` indexed by piece type, but `pieces[0]` (the `kNone` slot)
holds the occupied-squares set — the union of all pieces of both colours — rather than a piece
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
just a `{hash, move}` pair. `MoveList` is a `StackList` — a fixed-capacity, stack-allocated container —
so move generation does no heap allocation. (The search's principal variation and per-thread position
stack are `std::vector`s, the position stack reserved once up front; see the design notes.)

### Attack generation

Sliding-piece attacks (bishop, rook, queen) use the BMI2 `_pext_u64` instruction to gather the
occupancy bits relevant to a square and index a precomputed per-square attack table. Knight, king and
pawn attacks are plain 64-entry lookup tables. All tables are computed once at program startup in
`src/generated_data.cpp`.

`Pins` ([src/pins.h](src/pins.h)) computes absolute pins and the squares
each pinned piece may still move to, before move generation.

### Search

The search is a fail-soft negamax with alpha-beta pruning, driven by iterative deepening and
parallelised with Lazy SMP: several threads each run an independent whole-tree search, sharing a single
lockless transposition table rather than splitting the tree between them.

#### Iterative deepening and time management

`Game::SearchRootNode` ([src/game.cpp](src/game.cpp)) runs on a dedicated worker
thread — started by `Game::StartThinking` — so the UCI `stop` command, which sets an atomic
cancellation flag polled throughout the tree, can interrupt it at any time. It searches the root move
list at increasing depths starting from `kStartDepth` (3) up to `kMaxPly` (64), emitting an
`info depth … score … time … nodes … nps … hashfull … pv …` line each time the best move improves
(`score mate <±moves>` once a forced mate is found, otherwise `score cp <centipawns>`). After every iteration the root
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

The per-move soft budget is `remaining_time / moves_to_go`, and the configured `Move Overhead` is
subtracted from the hard deadline to leave headroom for GUI/network lag. (Folding the per-move increment
into the soft budget was SPRT-tested at 8+0.08 and 10+0.1 and proved neutral-to-slightly-negative in
equal-clock self-play, so `winc`/`binc` are parsed but not currently used in budgeting.)

`ChessClock` ([src/chess_clock.h](src/chess_clock.h)) is implemented with `std::chrono` on the monotonic
`steady_clock`: the soft/hard budgets are `std::chrono::milliseconds` durations and the hard stop is an
absolute `time_point` deadline, polled (every ~2k nodes) via `HasReachedHardDeadline`. On `ponderhit` the
clock restarts the budget from that instant (the deadline and search-start instant are `std::atomic` so the
UCI thread can retarget the running search). It is covered by the `test_chess_clock` unit suite.

#### Recursive search

`SearchState::Search` ([src/search_state.cpp](src/search_state.cpp)) applies, at each node:

- **Transposition table** probe/store with CUT / ALL / PV node types; a TT cutoff or TT move ordering
  hint is taken before any moves are generated.
- **Principal variation search (PVS)** — the first move is searched with the full window; later moves
  get a null-window (`[alpha, alpha+1]`) scout search and are only re-searched with the full window if
  the scout beats alpha.
- **Internal iterative reduction (IIR)** — at a node with no usable transposition-table move and depth ≥ 4,
  the search depth is reduced by one ply: without a TT move the move ordering is only a guess, so spending the
  full depth is wasteful; the shallower search still fills the TT with a best move, recouped by better ordering
  when the node is revisited. SPRT-tested **+31.8 ± 16.2 Elo at 8+0.08** (898 games) — a large, clean win.
- **Reverse futility pruning (RFP)** — at a non-PV node not in check, at shallow depth (≤ 7) and outside
  the mate zone, if `static_eval − 80·depth ≥ beta` the node is cut immediately (returning `static_eval`)
  without searching any move — when we are that far above beta a quiet move will hold it. The static eval
  is computed once and shared with null-move pruning. SPRT-tested **+114.8 ± 22.5 Elo at 8+0.08** — the
  largest single search gain; pawnstar had previously lacked the static-eval pruning suite.
- **Null-move pruning** — when not in check and the previous move was not itself a null move, a null
  move is searched at reduced depth (`depth − 4`, i.e. R = 3); a resulting score `≥ beta` prunes the
  node. (Reducing less — `depth − 3` — was SPRT-tested at 8+0.08 and lost ≈16 Elo over 1000 games
  [−15.99 ± 16.16], so the `depth − 4` cut stands.)
- **Late-move reductions (LMR)** — at null-window nodes, any late move (past the 4th) outside a check
  sequence at `depth > 2` is searched one ply shallower, with a second ply shaved off past the 7th
  move at `depth > 3`, and a third ply if that very-late move also has **zero history count** (the
  least-promising quiets are pruned hardest). A reduced search that beats alpha is re-searched at full
  depth. The third-ply reduction was SPRT-tested: roughly neutral at fast 8+0.08, but **+11 Elo
  (±18, 578 games)** at 40 moves/minute — a time-control-dependent gain that deeper trees reward.
- **Late-move (move-count) pruning (LMP)** — at a non-PV node not in check at shallow depth (≤ 8), once
  past the `3 + depth²`-th move the remaining **quiet, non-checking** moves are skipped entirely (captures,
  promotions and checks are never pruned). Where LMR *reduces* late quiets, LMP *drops* them. SPRT-tested
  **+18.6 ± 9.0 Elo at 8+0.08** on top of RFP.
- **Search extensions** for checks, promotions and en passant captures.
- **Move ordering** — the TT move first, then winning/equal captures and promotions by static exchange
  evaluation, then the two killer moves for the ply, then the **countermove** (the best quiet refutation
  to the previous move), then quiet moves scored by the **main history** count plus a **1-ply
  continuation history** (how often the move was good as a follow-up to the previous move), and finally
  losing captures (negative SEE) below the quiet moves. A quiet move that causes a beta cutoff is
  recorded as a killer and as the countermove to the previous move; every good quiet move is rewarded in
  both the per-thread history and continuation-history tables. All ordering tables are per-thread — see
  [Move ordering and history heuristics](#move-ordering-and-history-heuristics) below for the full scheme.

**Quiescence search** (`SearchState::SearchQuiescent`, [src/search_state.cpp](src/search_state.cpp)) extends the leaves with
captures only, so the static evaluation is never called on a position with a hanging capture available.
It does not use a transposition table (no probe, no store). It applies **SEE pruning** — the captures are
SEE-sorted, so the loop reads each move's precomputed ordering score and **exits on the first losing
capture** (`move.score() < 0`); every remaining move loses material too, so none are searched (and no
SEE is recomputed in the loop). Dropping the quiescence transposition table and switching to this sorted
early-exit was SPRT-tested **+38.65 ± 13.78 Elo at 8+0.08** (1318 games, LLR-accepted against `[-5, 0]`).

#### Move ordering and history heuristics

Alpha-beta prunes far more when the best move is searched first, so `SearchState::ScoreAndSortMoves`
([src/search_state.cpp](src/search_state.cpp)) assigns every move a 23-bit sort score (stored in the
`Move` itself) in strict descending bands. The TT move is searched before the list is even generated;
the remaining moves sort as:

| Band | Score | Moves |
| --- | --- | --- |
| Winning captures | `kWinningCaptureBase` (1 << 20) + SEE | captures / promotions with SEE ≥ 0 |
| Killers | `kKillerBase` (1 << 19) +1 / +0 | the two killer moves for this ply |
| Countermove | `kKillerBase − 1` | the best quiet refutation to the previous move |
| Quiet moves | `min(main history + continuation history, kKillerBase − 2)` | everything else |
| Losing captures | negative SEE | captures with SEE < 0, below the quiet moves |

The **killer** and **main-history** tables answer *"which quiet moves are good at this ply / in
general?"*. The **countermove** and **continuation-history** tables refine that by conditioning on the
**previous move** — the one move the opponent played to reach this node. That move is threaded down the
tree as `Search`'s `prev_move` parameter (`Move::None()` at the root and after a null move).

Both previous-move tables share a key that abstracts a move down to **`(piece, to-square)`**:

```cpp
static constexpr int ContKey(Move m) { return m.piece() * 64 + m.to(); }   // 7 × 64 = 448 keys
```

This is the standard "a piece lands on a square" abstraction — it generalises across positions (a knight
reaching f5 is the *same* follow-up however it got there), and `Move::None` collapses to a harmless slot 0.

- **Countermove** (`countermoves_`, an `array<Move, 448>`) — a single best refutation per previous move.
  When a quiet move causes a beta cutoff it is recorded keyed by `ContKey(prev_move)`
  (`RecordCountermove`). In ordering it is a *boolean* signal — a move that equals the recorded
  countermove gets the one fixed band just below the killers (`kKillerBase − 1`).
- **Continuation history** (`continuation_history_`, a 448 × 448 `int16_t` table, ~0.4 MB, indexed
  `[ContKey(prev) * 448 + ContKey(move)]`) — a *graded* score for every `(prev → move)` pairing. It is a
  saturating counter (caps at 16384, like the main `HistoryTable`) bumped whenever a quiet move proves
  good as a follow-up to `prev`: on a beta cutoff, on an alpha-raise, and for the final PV best move
  (`RecordContinuationHistory`). In ordering, an ordinary quiet move's score is **`main_history(ply, move) +
  continuation_history(prev, move)`**, clamped to `kKillerBase − 2` so a hot quiet can never jump into the
  countermove or killer bands.

Captures are excluded from both tables (every record site guards on `Move::IsQuiet()`) because captures
are already ordered by SEE — history signals only matter for quiets, which have no static score to sort
on. Both tables are **per-thread** (owned by `SearchState`), so there is no cross-thread contention; the
`continuation_history_` buffer is heap-allocated once in the constructor, so the per-node hot path allocates nothing.

#### Parallel search (Lazy SMP)

Parallelism is provided by **Lazy SMP**: `Game::SearchRootNode` launches `Game::thread_count` threads
(clamped to `kMaxSearchThreads`), each running its *own* complete iterative-deepening search of the root
position via `SearchState::IterativeDeepen`. There is no tree splitting and no work queue — the threads cooperate
purely through the shared lockless transposition table, where one thread's deeper or earlier results
prefill the entries another thread will probe. Each thread owns its own `SearchState` (position stack,
hash history, killers, node count) **and its own ordering tables** (history, countermove and
continuation history), so the only cross-thread sharing is the transposition tables and the
cancellation flag; there is no per-(ply, move) counter contention.

The thread count is set by the UCI **`Threads`** option (stored in `Game::thread_count`); its default is
computed once at construction from the `PAWNSTAR_THREADS` environment variable if set (>0), else
`hardware_concurrency()`, clamped to `[1, kMaxSearchThreads]` (e.g. `PAWNSTAR_THREADS=1` forces a
deterministic single-threaded search, used to regenerate Bratko-Kopec reference data).

To keep the helper threads from recomputing the same tree in lockstep, the main thread (thread 0)
searches every depth while each helper follows a Stockfish-style **iteration-skip schedule**
(`kSkipSize` / `kSkipPhase` tables in `search_state.cpp`) that makes it skip selected depths. The
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
| `kScoreInstabilityThreshold` | 50 | Centipawn window for the "score stable" time-management test |
| `kMaxSearchThreads` | 256 | Upper bound on Lazy SMP search threads |

### Transposition table

A 64 MB TT is used for the main search. Each entry is exactly 128 bits — a 64-bit Zobrist hash and a 64-bit move whose spare bits carry the score, depth, node type and age.

The table is **lockless** (Hyatt's XOR trick): each cell stores `key = hash ^ data` alongside `data`
(the packed move), as two `std::atomic<uint64_t>` accessed with relaxed ordering.

A reader accepts a
cell only if `key ^ data` equals the probe hash, so a torn pair written by two concurrent stores is
detected and treated as a miss.

Concurrent writers race benignly (last writer wins). Aging is O(1):
`Age()` bumps a generation counter and an entry is stale when its stamped age differs from it.

The probe is a random memory access, so once a child move has been made (its Zobrist hash known) the
search `Prefetch`es the cell the recursive node will read, hiding the cache-miss latency behind the
intervening work. This is purely a latency optimization and does not change results.

### Evaluation

`EvaluatePosition` ([src/evaluation.cpp](src/evaluation.cpp)) is **NNUE-only**: after a draw
short-circuit it returns the network's score for the incrementally-maintained accumulator (memoised by
Zobrist hash in an eval cache). See [NNUE evaluation](#nnue-evaluation) below. Earlier versions also had a
tapered hand-crafted evaluation (material, piece-square tables, pawn structure, king safety, mobility);
it was removed once NNUE became the sole evaluator.

Static exchange evaluation (`EvaluateStaticExchange`,
[src/static_exchange_evaluation.h](src/static_exchange_evaluation.h)) is independent of the NNUE eval: it
resolves a capture sequence on a square, is used to order captures and promotions during search (see
above) and to prune losing captures in quiescence, and is covered by `test_see`.

### NNUE evaluation

Pawnstar evaluates exclusively with an **NNUE** (Efficiently Updatable Neural Network) — a neural net
trained to score positions. It is the engine's only evaluator: the shipped net is loaded at startup (with
a copy embedded in the binary as a fallback if the file can't be loaded), and a different net can be
loaded at runtime (see [The NNUE net](#the-nnue-net) above).

**How it works.** The network ([src/nnue.cpp](src/nnue.cpp), [src/nnue.h](src/nnue.h)) is a
"perspective" net using bullet's `ChessBuckets` feature set:

```
768 inputs per (perspective, king bucket)  ->  1024 feature transformer (x2 perspectives)
SCReLU,  concat[side-to-move | opponent] = 2048  ->  1 output (centipawns, side-to-move relative)
```

The 768 base inputs are (piece colour × piece type × square). The shipped net uses **4 king buckets**
selected by king file-pair, so the feature row is `bucket*768 + base` (each perspective picks its bank by
its own king's square). Two accumulators are built — one from the side-to-move's perspective and one from the
opponent's (board vertically mirrored) — passed through a squared-clipped-ReLU activation and a single
output layer that produces a centipawn score. `EvaluatePosition` evaluates with the net directly (after a
draw short-circuit) — NNUE is the only evaluator. The network is an `nnue::Network` instance owned by the
`Game` (no globals), read-only after load and shared by all search threads. Its accumulator is **maintained
incrementally** across make/undo on each thread's `SearchState` — only the feature columns for the pieces
that moved are updated (a king crossing a file-pair bucket boundary rebuilds that one perspective's
accumulator). This keeps NNUE competitive on equal time, not just equal depth. Weights are quantised
`int16` (`QA=255`, `QB=64`, output scaled by `SCALE=400`) from the
[bullet](https://github.com/jw1912/bullet) trainer, with a small self-describing header that the engine
**requires**: an unstamped (raw bullet) net or one whose header doesn't match this build's architecture
is rejected rather than misread. Stamp a freshly trained raw net with `tools/stamp_net` before loading
(see [nnue/README.md](nnue/README.md)).

**Training a net.** All tooling lives in [tools/](tools) and is documented in
[nnue/README.md](nnue/README.md). The shipped nets are trained on **public PlentyChess** bulletformat
data (strong-engine labels), using the bullet trainer (GPU or CPU). The minimal flow:

```bash
tools/setup_bullet.sh                                       # clone + register the bullet trainer (pinned commit)
OPENINGS=~/openings.epd \
  tools/run_experiment.sh ~/pawnstar_nnue/shuffled.data net.bin   # train, verify, then SPRT
```

`run_experiment.sh` takes an already-converted bulletformat `.data` file (plus your own `OPENINGS`
EPD book); it produces `net.bin`, **verifies** it (`tools/verify_net.sh`: engine inference vs the
trainer, plus the incremental accumulator vs a full refresh), then runs the SPRT. Load the net with
`setoption name EvalFile`. The same two checks are wired into `make check` via `test_nnue` and
`test_nnue_incremental` (see [Test suite](#test-suite)); GPU training needs a CUDA toolkit and a driver
supporting CUDA ≥ 12.3, otherwise set `BULLET_FEATURES=""` to train on CPU. NNUE strength is measured by
game play (SPRT — now always net-vs-net, since the hand-crafted baseline was removed), not by the
Bratko-Kopec suite.

**Shipped net.** [nnue/pawnstar-v10.bin](nnue/pawnstar-v10.bin) is a **1024-wide net with 4 king buckets
(file-pair)** trained on **~3.82B** positions of **public PlentyChess** data. Lineage, all SPRT-measured
(against the since-removed hand-crafted eval, "HCE", in the early steps): Pawnstar self-play *lost* to the
HCE (label quality, not quantity, was the lever); scaling that data ~12× gave nothing (capacity-limited);
**doubling the width 256→512 added +55 Elo (fixed depth) / +71 (time control)** — that is v4; doubling
again to **1024 on 60M gave ~0 at fixed depth and −74 on the clock** (rejected *then* — 60M saturates the
512 net). **King buckets were flat on 60M (data-starved) but on ~818M gave +47 fixed depth and +17 on the
clock** — the v6 net — once two speed fixes (an 8 MB lockless eval cache and a single-pass accumulator
update) clawed back the wider table's per-move cost. **Scaling that 512×4 arch to ~2.31B gave +29.96 fixed
depth and +20.73 on the clock — the v7 net**. Then, at that same 2.31B data, **doubling the width to 1024
*without* king buckets (a single bank) beat v7 by +31.9 ± 16.6 Elo at 40/20 — and at half the file size —
reversing the old 60M result now that the data no longer saturates the wider net: the v8 net**. Then,
holding the width at 1024 and the data at ~2.31B, **adding 4 king buckets (file-pair) beat v8 by +11.4 ±
6.75 Elo at 8+0.08 — the v9 net**; the bucket gain shrinks with data (+29 at 750M → +11 at
2.31B) but stays positive, and the earlier "width beats buckets" call was really width-vs-buckets
confounded (it compared 512+buckets v7 to 1024-no-buckets v8). Then, holding that v9 arch fixed,
**scaling the data ~2.31B→~3.82B (four more PlentyChess shards) beat v9 by +16.3 ± 10.8 Elo at 8+0.08 — the
v10 net (shipped)**. Each shipped net retires its predecessor
(v4 at v6, v6 at v7, v7 at v8, v8 at v9, v9 at v10). The recurring lesson: **more data is the lever** (v10's 3.82B is
still only ~18% of the ~21B available). See [nnue/README.md §7](nnue/README.md)
for the full lineage and exact recreation steps.

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
| `build/test_perft` | Move-generation regression on the standard PERFT positions |
| `build/test_bratko_kopec_nnue` | 24 Bratko-Kopec positions searched with the NNUE net; reports moves solved |
| `build/test_nnue` | NNUE inference cross-check against the trainer's reference evals |
| `build/test_nnue_incremental` | Incremental accumulator vs full-refresh consistency check |

`make check` runs the NNUE tests against the shipped `nnue/pawnstar-v10.bin`: `test_nnue` against the
checked-in `test/nnue_reference.txt` (250 trainer evals; max |diff| 0 cp), `test_nnue_incremental` which
asserts the incremental accumulator matches a full refresh at every node, and `test_bratko_kopec_nnue`
which searches the 24 BK positions with the net (single-threaded, deterministic) and **must solve all 24**
— each position's best move must be in its accepted-move set (the `kCases` array in `test/bratko_kopec_nnue_test.cpp`). The Makefile
passes the net/reference via `$(wildcard …)`, so these degrade to a green no-op only if those files are
absent. When you ship a new net, regenerate **both** `test/nnue_reference.txt` (bullet `pawnstar_eval`)
and the BK accepted moves (the net-specific union over depths 8–11) — see [nnue/README.md](nnue/README.md)
§7.

The Bratko-Kopec suite is the **move-based** `test_bratko_kopec_nnue` (the classic metric — did the engine
find a good move). It takes an optional depth argument, e.g.
`./build/test_bratko_kopec_nnue nnue/pawnstar-v10.bin 10`, and searches single-threaded for a reproducible
result; the accepted moves are recorded over depths 8–11, so it solves 24/24 at each of those depths.
There is no score-based Bratko-Kopec test: NNUE scores are non-deterministic under Lazy SMP
and on a different scale. The earlier score-based `test_bratko_kopec` and the `test_pawn_structure` suite
tested the removed hand-crafted evaluation and were deleted with it.

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
| `Doxyfile` | Doxygen configuration |
| `LICENSE` | GNU General Public License v3 |

## Design notes

A few deliberate choices shape the codebase:

- **Immutable, copy-make positions.** `Position` is never mutated in place; `MakeMove` returns a new
  one by value and there is no `UnmakeMove`. This trades a per-ply copy for far simpler code: no
  state to restore, no aliasing between plies, and positions that are trivially safe to hand to other
  threads. The class is kept small (160 bytes) so the copy stays cheap.
- **Minimal heap allocation on the search hot path.** Move lists are fixed-capacity stack containers
  (`StackList`), so move generation allocates nothing. The per-thread position stack and the per-search
  hash history are `std::vector`s reserved once when the `SearchState` is constructed (the position stack
  to `kMaxPly + 4`, so a `push_back` during search never reallocates and invalidates the live position
  reference); the principal-variation lists are `std::vector`s bounded by the search depth; and the game
  history is a `std::vector` that grows only as real moves are played — so the node-by-node search loop
  performs effectively no dynamic allocation.
- **Everything precomputed that can be.** Attack tables and Zobrist keys are built once at startup
  (dynamic initialisation of `const` globals in `generated_data.cpp`), so the hot paths are pure lookups.
- **Lockless sharing.** Under Lazy SMP the transposition tables and the cancellation flag are the only
  cross-thread state — the ordering tables (history, killers, countermove, continuation history) are
  per-thread — and the shared state is accessed
  with atomics (the TT via Hyatt's XOR trick) rather than locks, so threads never block one another
  mid-search.

## Benchmarks

Pawnstar has no official CCRL rating. The figures currently tracked are:

- **Move generation** — roughly 700 million legal moves per second on a modern laptop core.
- **Bratko-Kopec** — `test_bratko_kopec_nnue` solves 24/24 at each depth 8–11 with the shipped net (depth 8 runs in `make check`).
- **Strength (rough).** `nnue/pawnstar-v10.bin` measures roughly ~2900+ Elo (CCRL-ballpark, anchored
  against reference engines at a fast time control; v8 is +31.9 ± 16.6 over v7 at 40/20, and v7 measured
  ~2900) — far above the since-removed hand-crafted eval (~2350). At equal *time*, the incremental accumulator is
  worth ~+80 Elo over a full refresh and the AVX2 SIMD kernels a further ~+180 Elo over the scalar
  versions (both by SPRT).

`make check` is the regression baseline; the perft suite verifies move-generation correctness against
the published node counts for the standard positions.

## Limitations and known issues

- **BMI2 + AVX2 required.** The sliding-piece attacks use `_pext_u64` (BMI2) and the NNUE kernels use
  AVX2, so the engine needs an Intel Haswell / AMD Zen 1 (or newer) CPU and is built with `-mbmi2 -mavx2`.
  On early AMD Zen parts `pext` is microcoded and comparatively slow. (The NNUE kernels keep scalar
  fallbacks behind `#if defined(__AVX2__)`, but the default build enables AVX2.)
- **Thread count** defaults to `hardware_concurrency()` and is not exposed as a UCI option, though it
  can be overridden with the `PAWNSTAR_THREADS` environment variable.
- **No pondering** (thinking on the opponent's time) and **no syzygy/tablebase** support.
- **NNUE is the only evaluator.** The engine loads the shipped net at startup and **exits** if it cannot
  (there is no hand-crafted fallback). The accumulator is maintained incrementally across make/undo (with
  a refresh of one perspective when its king crosses a file-pair bucket boundary) and the kernels are
  AVX2-vectorised, so it is fast on the clock; the shipped net is a 1024-wide, 4-king-bucket (file-pair)
  `ChessBuckets` net (v9).

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
3. `src/generated_data.cpp` holds the precomputed-table *logic* (the compute functions and the `const`
   globals they fill at startup); edit it when that logic changes.
4. Keep new public declarations Doxygen-commented so `make doc` stays complete.

## License

Pawnstar is free software, licensed under the **GNU General Public License, version 3** (GPLv3). You may
redistribute and/or modify it under the terms of that license; it comes with **no warranty**. See the
[LICENSE](LICENSE) file for the full text, or <https://www.gnu.org/licenses/gpl-3.0.html>.

The bundled opening book ([pawnstar.book](pawnstar.book)) is derived from the public-domain
[TSCP](https://github.com/terredeciels/TSCP) `book.txt`. The NNUE trainer sources under
[tools/bullet/](tools/bullet) are written for the [bullet](https://github.com/jw1912/bullet) trainer
(MIT-licensed), which is fetched separately by `tools/setup_bullet.sh` and is not redistributed here.
