# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires `clang++` and GNU `make`. The build has a pre-compilation step that generates `src/generated_data.cpp` (precomputed bitboard lookup tables).

```bash
make          # full build → build/pawnstar
make check    # build and run all test suites
make doc      # generate Doxygen HTML into doc/html
make clean    # remove build artifacts and generated docs
```

Compiler flags include `-mbmi2` (BMI2 intrinsics required), `-std=c++20`, and `-I src`. Debug builds (`DEBUG=1`) add `-fsanitize=undefined,address`.

Always run `make clean && make` when switching branches or after changes to generated files — stale `.d` dependency files can cause spurious build failures.

## Running Tests

Tests are standalone executables in `test/`, linked against the engine objects but with their own `main()`. Build and run all tests with:

```bash
make check    # builds tests then runs all four suites; returns non-zero on any failure
```

Individual test executables:

```bash
./build/test_see              # 24 static exchange evaluation cases
./build/test_pawn_structure   # 12 pawn structure tests, 72 individual checks
./build/test_perft            # move generation regression (standard PERFT positions)
./build/test_bratko_kopec     # 24 Bratko-Kopec search positions (default depth 8)
./build/test_bratko_kopec 12  # same positions at depth 12
```

The BK test (`test/bratko_kopec.cpp`) accepts an optional depth argument in the range 8–12 (default 8). Because the Lazy SMP search is non-deterministic, the test checks the **score**, not the move played: each position stores baked single-threaded reference scores for depths 8–12, and a position passes when its parallel score lies within the span of those references across *all* depths 8–12, widened by ±50 cp (`kScoreTolerance`). (Lazy SMP desynchronizes effective depth in both directions — helpers race ahead, and aggressive pruning can make the main thread report a shallower valuation than a same-depth single-threaded search — so a nominal depth-D result can match any stored depth's score.) Mate scores match any mate of the same sign. At the shallowest depth (8) the parallel search is least stable, so depth 8 *additionally* accepts any best move recorded in that position's depth-8 move set (the full set of best moves observed across repeated parallel runs). It passes 24/24 at every depth 8–12. Regenerate the reference data after evaluation/search changes with a single-threaded run — `PAWNSTAR_THREADS=1 ./build/test_bratko_kopec 12` — and transcribe the per-depth `info depth D score cp S ... pv M` lines (last line per depth wins) into the `bk_cases` array; repopulate the depth-8 move sets from ~10 parallel `./build/test_bratko_kopec 8` runs, collecting each position's `got=` move.

## Code Formatting

clang-format with Microsoft base style, 120-column limit. Config is in `.clang-format`.

## Architecture

Pawnstar is a UCI chess engine using bitboard board representation and parallel alpha-beta search.

### Board representation

`Position` ([position.h](src/position.h)) is the central state class. It stores:
- Six per-piece bitboards (`pawns_`, `knights_`, `bishops_`, `rooks_`, `queens_`, `kings_`) plus two per-color bitboards (`white_pieces_`, `black_pieces_`).
- A `squares_[64]` array for O(1) square→piece lookup.
- Zobrist hash, castling rights (`CastlingRights`), en passant square, half-move clock, and a checkers bitboard (squares giving check to the side to move).
- State flags packed into `state_flags_` (color to move, null-move marker).

`Position` is immutable after construction. `MakeMove` and `MakeNullMove` return a new `Position` by value. `GenerateLegalMoves` / `GenerateLegalCaptures` dispatch to the templated `GenMoves<Color, bool>()`, which uses `Pins` to filter pseudo-legal moves.

`Bitboard` ([bitboard.h](src/bitboard.h)) wraps a `uint64_t` with bitwise operations and a range-based iterator over set bits (`Square` values).

### Move encoding

`Move` ([move.h](src/move.h)) packs all move information into 64 bits:
- Bits 0–5: to square; 6–11: from square; 12–14: moving piece; 15–17: captured piece; 18–21: move type; 22: is-checking flag.
- Bits 23–24: TT node type; 25–32: TT depth (int8_t); 33–40: TT age (uint8_t); 41–63: score (signed 23-bit).

The score field (bits 41–63) is shared: during move ordering it holds the sort score, and in a stored TT entry it holds the search score. The two never coexist — a move being ordered in a move list is never simultaneously a TT entry. The TT metadata bits (23–63) let a transposition entry be just `{hash, move}` (128 bits); see the transposition table section. `Move::WithTTData`/`SetTTAge` pack the metadata and `Bits`/`FromBits` round-trip the raw 64-bit value for the lockless TT.

Move types: `kNonCapture`, `kCapture`, `kPawnDoublePush`, `kEpCapture`, `kCastling`, and promotion variants `kPromotionKnight/Bishop/Rook/Queen`.

`MoveList` / `StackList` are fixed-capacity stack vectors (no heap allocation during search).

### Attack generation

`Attacks` ([attacks.h](src/attacks.h)) uses BMI2 `_pext_u64` for sliding-piece attacks: for each square the occupancy bits relevant to that slider are extracted with `pext`, used to index a per-square attack table. Tables live in `src/generated_data.cpp` (generated at build time by `generate_constants/generate_constants.cpp`; gitignored and regenerated on every `make`).

Knight, king, and pawn attack tables are plain 64-entry arrays in `generated_data.cpp`.

`Pins` ([pins.h](src/pins.h)) computes absolute pins and allowed move masks before move generation by scanning enemy sliding pieces that see the king through exactly one friendly piece.

### Search

**Iterative deepening** — `SearchRootNode` ([search_root_node.cpp](src/search_root_node.cpp)) drives iterative deepening from `kStartDepth` (3) upwards, maintaining a principal variation. The per-iteration loop lives in the static `IterativeDeepen` helper, which every Lazy SMP thread runs. It runs on a dedicated worker thread started by `Game::StartThinking`; the UCI `stop` command sets `game.is_cancel_pending`.

**Alpha-beta** — `SearchAlphaBeta` ([search_alphabeta.cpp](src/search_alphabeta.cpp)) is a fail-soft negamax with:
- Transposition table probe/store (exact PV, lower-bound CUT, upper-bound ALL).
- Principal variation search (PVS): full window on the first move, null-window scout on the rest, re-search on a fail-high.
- Null-move pruning.
- Late-move reduction (LMR): reduces any late move (after the 4th) outside a check sequence at depth > 2, with an extra reduction after the 7th. (Each thread searches its tree sequentially — there is no in-tree split.)
- Killer move heuristic (2 killers per ply): a quiet move that causes a beta cutoff is stored via `SearchState::RecordKiller`.
- History heuristic (`HistoryTable`) for move ordering — **per-thread** (owned by `SearchState`), so there is no cross-thread contention on its counters.

**Move ordering** — `SearchState::ScoreAndSortMoves` assigns each move a 23-bit ordering score stored in the `Move` itself, in descending bands: winning/equal captures and promotions (`kWinningCaptureBase` + SEE score), the two killer moves for the ply (just below winning captures), quiet moves (history count, clamped below the killers), and finally losing captures (their negative SEE, sorting below quiet moves). The TT move is searched first, before the list is generated and sorted.

**Quiescence search** — `SearchQuiescent` ([search_quiescent.cpp](src/search_quiescent.cpp)) searches captures only, using its own transposition table. It applies SEE pruning: a capture with negative SEE that does not give check is skipped rather than searched.

**Parallel search (Lazy SMP)** — `SearchRootNode` spawns `hardware_concurrency()` threads (capped at `kMaxSearchThreads`, 256), each running a complete independent `IterativeDeepen` search of the root. There is no tree splitting and no work queue — threads cooperate only through the shared lockless transposition table, where one thread's entries prefill another's probes. Each thread has its own `SearchState` (including its own history table), so the only shared mutable state is the transposition tables and `is_cancel_pending`. The thread count can be overridden with the `PAWNSTAR_THREADS` environment variable (e.g. `PAWNSTAR_THREADS=1` for a deterministic single-threaded search — used to regenerate BK reference data). To stop helpers recomputing the same tree in lockstep, the main thread (thread 0) searches every depth while each helper follows a Stockfish-style iteration-skip schedule (`kSkipSize`/`kSkipPhase` tables in `search_root_node.cpp`) that skips selected depths. Only the main thread prints `info`, applies time management and returns the authoritative move; because helpers race ahead through the table, the result (move and exact score) is non-deterministic between runs.

### Per-thread state

`SearchState` ([search_state.h](src/search_state.h)) owns:
- A reference to the shared `Game` (TT, clock, cancellation).
- `history` — the **per-thread** `HistoryTable` (no cross-thread sharing).
- `killers[kMaxPly][2]` — killer moves.
- `node_count` — nodes searched by this thread.
- `positions_` (`StackList<Position>`) — position stack for the search tree.
- `hash_stack_` (`StackList<HashEntry, kMaxGameLength + kMaxPly + 4>`) — Zobrist hash history for 50-move/repetition detection; fixed-capacity (no heap allocation per search).

Lazy SMP retired the old YBW machinery: `SearchStatePool` and `ThreadPool` (and their `kSearchStatePoolCapacity`/`kThreadPoolQueueCapacity` constants) no longer exist. Threads are plain `std::thread`s spawned per search in `SearchRootNode`, each constructing its own `SearchState` on its stack.

### Transposition table

Two tables: 64 MB main (`kHashtableMegabytes`) and 8 MB quiescence (`kQHashtableMb`). Each entry is exactly 128 bits: a 64-bit Zobrist `hash` and a 64-bit `move` whose spare bits encode the score, depth, node type (CUT/ALL/PV) and age. `Transposition` exposes `score()`/`depth()`/`node_type()`/`age()` accessors that read those packed bits.

The table is **lockless** (Hyatt's XOR trick). Each cell (`AtomicEntry`) stores two `std::atomic<uint64_t>`: `key = hash ^ data` and `data` (the packed move). A reader accepts a cell only when `key ^ data == probe hash`, so a torn pair written by concurrent stores is detected and read as a miss. Words are accessed with `relaxed` ordering; the XOR check supplies the cross-word consistency that relaxed ordering does not. Writers race benignly (last-writer-wins) — no spinlocks.

Aging is O(1): the table holds a `generation_` counter that `Age()` increments before each search; an entry is stale when its `age != generation_`, and `RecordTransposition` stamps the current generation when it writes. Replacement policy: prefer replacing stale entries, lower-depth entries, or any entry for PV nodes.

### Evaluation

`EvaluatePosition` ([evaluation.cpp](src/evaluation.cpp)) scores:
- **Material**: per-piece centipawn values (with a bishop-pair bonus) in `EvaluateMaterial`.
- **Piece-square tables**: per-piece tables in [evaluation.h](src/evaluation.h) (`kPawnSquare`, `kKnightSquare`, `kBishopSquare`, `kRookSquare`, `kQueenSquare`, and separate `kKingSquareMidgame`/`kKingSquareEndgame`).
- **Pawn structure** via `DeterminePawnStructure<Color>()`: passed, isolated, unsupported, doubled, and defended pawn bitboards. Penalties for doubled/isolated/unsupported; bonuses for passed and defended.
- **King safety**: penalties based on attacker proximity.
- **Mobility**: bonus for piece mobility (number of legal moves available).

Scores are tapered between opening/middlegame and endgame phases based on remaining material.

SEE (static exchange evaluation, [static_exchange_evaluation.h](src/static_exchange_evaluation.h)) resolves a capture sequence on a square; `EvaluateStaticExchange` returns `{score, is_checking}`. It is used to order captures and promotions in `ScoreAndSortMoves` (see Move ordering above), to prune losing captures in quiescence search, and is exercised by `test_see`.

### Opening book

`OpeningBook` ([opening_book.h](src/opening_book.h)) is loaded from `pawnstar.book` in the working directory at startup (`main.cpp`); the shipped book ([doc/pawnstar.book](doc/pawnstar.book)) is the public-domain [TSCP](https://github.com/terredeciels/TSCP) `book.txt`. The format is plain text, one opening line per row, each move in **coordinate notation** (e.g. `e2e4 e7e5 g1f3`); `ParseLineOfPlay` replays each line from the start position, ignoring tokens that begin with a digit (move numbers) and treating `#` as a to-end-of-line comment. The book is a `std::map<zobrist_t, std::vector<Move>>` storing moves *with repeats*, so `GetMove`'s uniform-random pick is frequency-weighted. `SearchRootNode` returns a book move immediately on a hit, before searching; the `bookmoves` UCI command lists moves and frequencies for the current position. There is no SAN/PGN parser — coordinate notation only.

### UCI protocol

[input_handling.cpp](src/input_handling.cpp) parses all UCI commands plus engine-specific diagnostics: `eval` (print position evaluation), `getboard` (print board), `bookmoves` (list opening-book moves for the current position), `dbg` (dump debug counters).

### Generated data

[generate_constants/](generate_constants/) is a standalone program that outputs [src/generated_data.cpp](src/generated_data.cpp). Only modify `generate_constants.cpp` when attack mask logic changes. The output is gitignored and regenerated on every `make`.
