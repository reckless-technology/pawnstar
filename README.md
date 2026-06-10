# Pawnstar

Pawnstar is a [UCI](https://en.wikipedia.org/wiki/Universal_Chess_Interface) chess engine written in
C++20. It uses a bitboard board representation, a parallel alpha-beta search (Young Brothers Wait),
a lockless transposition table, and a tapered hand-crafted evaluation.

Legal move generation runs at roughly 700 million moves per second on a modern laptop.

## Features

- Bitboard board representation with BMI2 `pext`-based sliding-piece attacks.
- Fully legal move generator (pins, checks, en passant edge cases, and castling handled directly).
- Parallel iterative-deepening alpha-beta search with principal variation search (PVS).
- Young Brothers Wait (YBW) work splitting over a fixed thread pool.
- Lockless 128-bit transposition table using Hyatt's XOR trick.
- Null-move pruning, late-move reductions, killer and history move ordering.
- Quiescence search over captures with its own transposition table.
- Tapered evaluation: material, piece-square tables, pawn structure, king safety and mobility.
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

After switching branches or changing generated files, run `make clean && make` — stale `.d`
dependency files can otherwise cause spurious build failures.

## Usage

Run as a UCI engine (connect via Arena, Cute Chess, or any UCI-compatible GUI):

```bash
./build/pawnstar
```

In addition to the standard UCI commands, a few engine-specific commands are available at the prompt:

```
eval        print the static evaluation of the current position
getboard    print the FEN of the current position
bookmoves   list opening-book moves for the current position
dbg         dump internal diagnostic counters
help        list all commands
```

## Architecture

### Board representation

`Position` ([src/position.h](src/position.h)) is the central immutable state class. It stores six
per-piece bitboards and two per-colour bitboards, a 64-entry square→piece array for O(1) lookup, the
incrementally-maintained Zobrist hash, castling rights, the en passant square, the half-move clock,
and a `checkers` bitboard. `MakeMove` and `MakeNullMove` return a brand new `Position` by value
(copy-make); there is no unmake.

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

Iterative deepening starts at depth 3 (`SearchRootNode`, [src/search_root_node.cpp](src/search_root_node.cpp))
and runs on a dedicated worker thread so the UCI `stop` command (which sets an atomic cancellation
flag) can interrupt it. The recursive search (`Search`, [src/search_alphabeta.cpp](src/search_alphabeta.cpp))
is a fail-soft negamax with:

- **Transposition table** probe/store with CUT / ALL / PV node types.
- **Principal variation search** — null-window re-search of non-PV moves.
- **Null-move pruning**.
- **Late-move reductions** — quiet, non-check moves are reduced after the 4th move at depth > 2, with
  an extra reduction after the 7th.
- **Search extensions** for promotions and en passant captures.
- **Move ordering** — the TT move first, then captures by MVV/LVA in a high score band, then quiet
  moves by history-heuristic count. Killers (two per ply) are also maintained.

**Quiescence search** ([src/search_quiescent.cpp](src/search_quiescent.cpp)) extends the leaves with
captures only, using a separate transposition table. (An SEE-based pruning path exists but is
currently compiled out.)

**Parallel search (Young Brothers Wait):** after two moves have been searched sequentially at a
null-window node and idle threads are available, the remaining moves are dispatched to a thread pool.
Each worker borrows its own `SearchState` (position stack, hash history, killers, node count) from a
fixed slab allocator (`SearchStatePool`), searches one move, and writes its result back into a
stack-allocated job. A `std::latch` plus a per-batch `std::atomic<bool>` cutoff flag coordinate the
batch; the thread pool dispatches plain function-pointer tasks (no `std::function`, no per-task heap
allocation). Workers never sub-parallelize — only the main thread splits work.

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
square and is covered by `test_see`, though it is not currently wired into the live search.

## Test suite

```bash
make check
```

builds and runs four standalone test executables:

| Executable | Description |
|---|---|
| `build/test_see` | 18 static exchange evaluation cases |
| `build/test_pawn_structure` | 8 pawn structure tests, 38 individual checks |
| `build/test_perft` | Move-generation regression on the standard PERFT positions |
| `build/test_bratko_kopec` | 24 Bratko-Kopec search positions (default depth 8) |

The Bratko-Kopec test takes an optional depth argument, e.g. `./build/test_bratko_kopec 9`. Because
the YBW search is non-deterministic, each position lists every best move observed across many runs.

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
| `generate_constants/` | Build-time generator for the precomputed lookup tables |
| `Doxyfile` | Doxygen configuration |
