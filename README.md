Pawnstar is a UCI chess engine written in C++20. It uses bitboard board representation, parallel alpha-beta search (Young Brothers Wait), and a tapered evaluation function.

Move generation runs at approximately 700 million moves per second on a modern laptop.

## Requirements

- `clang++` (C++20, BMI2 support required — `-mbmi2`)
- GNU `make`
- A CPU with BMI2 instructions (Intel Haswell / AMD Zen 1 or later)

## Build

```bash
make           # build/pawnstar
make check     # build and run all test suites
make clean     # remove build artifacts
```

Debug build (ASan + UBSan):

```bash
make DEBUG=1
```

## Usage

Run as a UCI engine (connect via Arena, Cute Chess, or any UCI-compatible GUI):

```bash
./build/pawnstar
```

Engine-specific diagnostic commands (type at the UCI prompt):

```
eval        print evaluation of the current position
getboard    print the board
dbg         dump internal debug counters
```

## Architecture

### Board representation

`Position` is the central state class. It stores six per-piece bitboards, two per-color bitboards, a 64-entry square→piece array for O(1) lookup, Zobrist hash, castling rights, en passant square, and a checkers bitboard. `Position` is immutable — `MakeMove` returns a new `Position` by value.

`Bitboard` wraps a `uint64_t` with bitwise operations and a range iterator over set bits.

### Move encoding

`Move` packs from/to squares, moving piece, captured piece, move type flags (castling, en passant, promotion, double pawn push, check), and a sort score into 64 bits. No heap allocation occurs during search — `MoveList` and `StackList` are fixed-capacity stack vectors.

### Attack generation

Sliding-piece attacks (bishops, rooks, queens) use BMI2 `_pext_u64` to extract the relevant occupancy bits for a given square and look up precomputed attack sets. Knight, king, and pawn attack tables are plain 64-entry arrays. All lookup tables are generated at build time by `generate_constants/generate_constants.cpp` and written to `src/generated_data.cpp`.

### Search

Iterative deepening from depth 3, driven by `SearchRootNode`. The alpha-beta implementation uses:

- **Transposition table** — two tables (64 MB main, 8 MB quiescence) with per-entry spinlock protection and age-based replacement.
- **Null-move pruning** and **late-move reduction (LMR)** — non-captures and non-pawn moves are reduced after the 4th move at depth > 2.
- **Killer heuristic** — 2 killers per ply.
- **History heuristic** — per-(ply, from, to) move counts stored as `std::atomic<uint32_t>` for thread safety.

**Parallel search (Young Brothers Wait)**: after 2 sequential moves at a null-window node with idle threads available, the remaining moves in that batch are dispatched to a thread pool in parallel. Each worker gets its own `SearchState` (position stack, hash history, killers, node count) from a 64-slot slab allocator (`SearchStatePool`). A `std::latch` and a per-batch `std::atomic<bool>` cutoff flag synchronize the batch. Workers never sub-parallelize.

The search runs on a dedicated thread; `is_cancel_pending` (`std::atomic<bool>`) lets the UCI `stop` command interrupt it cleanly.

Quiescence search handles captures only; captures with negative SEE that don't give check are pruned.

### Evaluation

Tapered evaluation blending opening/endgame scores based on remaining material:

- Material values
- Piece-square tables (per piece type)
- Pawn structure: passed, isolated, unsupported, doubled, and defended pawn detection
- King safety (attacker proximity penalty)
- Mobility bonus

SEE (static exchange evaluation) is used for capture move ordering and quiescence pruning.

## Test suite

```bash
make check
```

Runs four standalone test executables:

| Executable | Description |
|---|---|
| `build/test_see` | 18 static exchange evaluation cases |
| `build/test_pawn_structure` | 8 pawn structure tests, 38 individual checks |
| `build/test_perft` | Move generation regression (standard PERFT positions) |
| `build/test_bratko_kopec` | 24 Bratko-Kopec search positions at depth 8 |

The BK test accepts an optional depth argument: `./build/test_bratko_kopec 9`.

## Documentation

Generate Doxygen HTML documentation:

```bash
doxygen .
```
