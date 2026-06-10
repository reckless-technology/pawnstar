# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires `clang++` and GNU `make`. The build has a pre-compilation step that generates `src/generated_data.cpp` (precomputed bitboard lookup tables).

```bash
make          # full build → build/pawnstar
make clean    # remove build artifacts
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
./build/test_see              # 18 static exchange evaluation cases
./build/test_pawn_structure   # 8 pawn structure tests, 38 individual checks
./build/test_perft            # move generation regression (standard PERFT positions)
./build/test_bratko_kopec     # 24 Bratko-Kopec search positions (default depth 8)
./build/test_bratko_kopec 9   # same positions at depth 9
```

The BK test (`test/bratko_kopec.cpp`) accepts an optional depth argument. At depth 9 the YBW parallel search is non-deterministic; accepted move sets for each position include all moves observed across multiple runs.

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

The score field (bits 41–63) is shared: during move ordering it holds the sort score, and in a stored TT entry it holds the search score. The two never coexist — a move being ordered in a move list is never simultaneously a TT entry. The TT metadata bits (23–56) let a transposition entry be just `{hash, move}` (128 bits); see the transposition table section.

Move types: `kNonCapture`, `kCapture`, `kPawnDoublePush`, `kEpCapture`, `kCastling`, and promotion variants `kPromotionKnight/Bishop/Rook/Queen`.

`MoveList` / `StackList` are fixed-capacity stack vectors (no heap allocation during search).

### Attack generation

`Attacks` ([attacks.h](src/attacks.h)) uses BMI2 `_pext_u64` for sliding-piece attacks: for each square the occupancy bits relevant to that slider are extracted with `pext`, used to index a per-square attack table. Tables live in `src/generated_data.cpp` (generated at build time by `generate_constants/generate_constants.cpp`; gitignored and regenerated on every `make`).

Knight, king, and pawn attack tables are plain 64-entry arrays in `generated_data.cpp`.

`Pins` ([pins.h](src/pins.h)) computes absolute pins and allowed move masks before move generation by scanning enemy sliding pieces that see the king through exactly one friendly piece.

### Search

**Iterative deepening** — `SearchRootNode` ([search_root_node.cpp](src/search_root_node.cpp)) drives iterative deepening from `kStartDepth` (3) upwards, maintaining a principal variation. It runs on a dedicated worker thread started by `Game::StartThinking`; the UCI `stop` command sets `game.is_cancel_pending`.

**Alpha-beta** — `SearchAlphaBeta` ([search_alphabeta.cpp](src/search_alphabeta.cpp)) is a fail-soft negamax with:
- Transposition table probe/store (exact PV, lower-bound CUT, upper-bound ALL).
- Null-move pruning.
- Late-move reduction (LMR) via `ComputeLmrDepth()` helper: reduces non-captures, non-pawn, non-check moves after the 4th move at depth > 2, with an extra reduction after the 7th.
- Killer move heuristic (2 killers per ply).
- History heuristic (`HistoryTable`, thread-safe atomics) for move ordering.

**Quiescence search** — `SearchQuiescent` ([search_quiescent.cpp](src/search_quiescent.cpp)) searches captures only, with SEE pruning: captures with negative SEE that don't give check are skipped.

**Parallel search (YBW)** — After 2 sequential moves at a null-window node, remaining moves are dispatched to the thread pool in parallel batches (Young Brothers Wait). Each parallel worker gets its own `SearchState` from the `SearchStatePool` slab (64 slots). A per-batch `std::atomic<bool>` cutoff flag and a `std::latch` synchronize workers within a batch. Workers never sub-parallelize (only the main thread triggers YBW).

### Per-thread state

`SearchState` ([search_state.h](src/search_state.h)) owns:
- A reference to the shared `Game` (TT, history, clock, cancellation).
- `killers[kMaxPly][2]` — killer moves.
- `node_count` — nodes searched by this thread.
- `batch_cutoff*` — pointer to the per-batch abort flag (`nullptr` on the main thread).
- `positions_` (`StackList<Position>`) — position stack for the search tree.
- `hash_stack_` (`vector<HashEntry>`) — Zobrist hash history for 50-move/repetition detection.

`SearchStatePool` ([thread_pool.h](src/thread_pool.h)) is a 64-slot slab allocator with a mutex+condition variable for blocking `acquire()` calls. `ThreadPool` is a fixed-size pool of `hardware_concurrency() - 1` persistent worker threads.

### Transposition table

Two tables: 64 MB main (`kHashtableMegabytes`) and 8 MB quiescence (`kQHashtableMb`). Each entry is exactly 128 bits: a 64-bit Zobrist `hash` and a 64-bit `move` whose spare bits encode the score, depth, node type (CUT/ALL/PV) and age. `Transposition` exposes `score()`/`depth()`/`node_type()`/`age()` accessors that read those packed bits. Access is protected by a parallel array of `std::atomic_flag` spinlocks (one per entry) via the `TTLock` RAII guard in [transposition_table.cpp](src/transposition_table.cpp).

Aging is O(1): the table holds a `generation_` counter that `Age()` increments before each search; an entry is stale when its `age != generation_`, and `RecordTransposition` stamps the current generation when it writes. Replacement policy: prefer replacing stale entries, lower-depth entries, or any entry for PV nodes.

### Evaluation

`EvaluatePosition` ([evaluation.cpp](src/evaluation.cpp)) scores:
- **Material**: piece values from `SearchState::kPieceValues`.
- **Piece-square tables**: per-piece tables in [evaluation.h](src/evaluation.h) (`kPawnSquare`, `kKnightSquare`, `kBishopSquare`, `kRookSquare`, `kQueenSquare`, `kKingSquare`).
- **Pawn structure** via `DeterminePawnStructure<Color>()`: passed, isolated, unsupported, doubled, and defended pawn bitboards. Penalties for doubled/isolated/unsupported; bonuses for passed and defended.
- **King safety**: penalties based on attacker proximity.
- **Mobility**: bonus for piece mobility (number of legal moves available).

Scores are tapered between opening/middlegame and endgame phases based on remaining material.

SEE ([static_exchange_evaluation.h](src/static_exchange_evaluation.h)) is used for capture move ordering and quiescence pruning. `EvaluateStaticExchange` returns `{score, is_checking}`.

### UCI protocol

[input_handling.cpp](src/input_handling.cpp) parses all UCI commands plus engine-specific diagnostics: `eval` (print position evaluation), `getboard` (print board), `dbg` (dump debug counters).

### Generated data

[generate_constants/](generate_constants/) is a standalone program that outputs [src/generated_data.cpp](src/generated_data.cpp). Only modify `generate_constants.cpp` when attack mask logic changes. The output is gitignored and regenerated on every `make`.
