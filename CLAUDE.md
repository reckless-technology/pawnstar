# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires `clang++` and GNU `make`. The build has a pre-compilation step that generates `src/generated_data.cpp` (precomputed bitboard lookup tables).

```bash
make          # full build → build/pawnstar
make clean    # remove build artifacts
```

Compiler flags include `-mbmi2` (BMI2 intrinsics required), `-std=c++20`, and `-I include`. Debug builds use `-fsanitize=undefined,address`.

## Running Tests

All tests run inside the engine via UCI-style commands. Build first, then:

```bash
./build/pawnstar
perft          # move generation regression suite (standard PERFT positions)
seetests       # static exchange evaluation tests
postests 9     # Bratko-Kopec search positions at depth 9
```

There is no separate test binary or unit test framework — tests are embedded as engine commands.

## Code Formatting

clang-format with Microsoft base style, 120-column limit. Config is in `.clang-format`.

## Architecture

Pawnstar is a UCI chess engine using bitboard board representation and alpha-beta search.

**Board representation** — `Position` ([position.h](include/position.h)) is the central state class. It holds per-piece and per-color bitboards, a square→piece array for O(1) lookup, Zobrist hash, castling rights, and en passant square. `Bitboard` ([bitboard.h](include/bitboard.h)) provides the 64-bit set operations.

**Move encoding** — `Move` ([move.h](include/move.h)) packs from/to squares, moving piece, captured piece, move type flags (castling, en passant, promotion, double push, check), and a sort score into 64 bits. `MoveList` / `StackList` are fixed-capacity stack vectors with no heap allocation during search.

**Attack generation** — `Attacks` ([attacks.h](include/attacks.h)) uses BMI2 `_pext_u64` for sliding-piece attacks via precomputed occupancy masks. The lookup tables live in `src/generated_data.cpp` (generated at build time). Knight, king, and pawn attack tables are also precomputed. `Pins` ([pins.h](include/pins.h)) computes pin rays and absolute pins before move generation.

**Search** — `Game` ([game.h](include/game.h), [game.cpp](src/game.cpp)) owns the transposition tables, history table, opening book, chess clock, and position history stack. It drives iterative deepening via `SearchRootNode` ([search_root_node.cpp](src/search_root_node.cpp)). The alpha-beta implementation is in [search_alphabeta.cpp](src/search_alphabeta.cpp) with a separate quiescence search in [search_quiescent.cpp](src/search_quiescent.cpp). Search runs on a worker thread so UCI `stop` can interrupt it.

**Transposition table** — Two tables: 64 MB for the main search, 8 MB for quiescence. Entries store hash, best move, score, depth, node type (CUT/ALL/PV), and an age flag for replacement policy.

**Evaluation** — `Evaluation` ([evaluation.h](include/evaluation.h), [evaluation.cpp](src/evaluation.cpp)) scores material, piece-square tables, pawn structure (doubled/isolated/passed), and king safety. Scores are tapered between opening/middlegame and endgame phases. SEE ([static_exchange_evaluation.h](include/static_exchange_evaluation.h)) is used for move ordering of captures.

**UCI protocol** — [input_handling.cpp](src/input_handling.cpp) parses all UCI commands plus engine-specific diagnostics (`eval`, `getboard`, `dbg`) and the test commands above.

**Generated data** — [generate_constants/](generate_constants/) is a standalone program that outputs [src/generated_data.cpp](src/generated_data.cpp) at build time. The output is gitignored and regenerated on every `make`. Only modify `generate_constants.cpp` when attack mask logic changes.
