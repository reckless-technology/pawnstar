---
name: Go port status
description: Current state of the Go port of the Pawnstar chess engine
type: project
originSessionId: d14a0775-8e27-42ef-86f1-5ac3fd3ebbdf
---
The Go port lives in its own repo at `/home/jonny/work/pawnstar-go`. All phases complete as of 2026-05-11.

**Files written:**
- `go/engine/constants.go` — Alpha, Beta, MaxPly, score constants
- `go/engine/see.go` — Static exchange evaluation
- `go/engine/evaluate.go` — Material, PST, pawn structure, mobility, king safety evaluation
- `go/engine/transposition.go` — TranspositionTable (fixed-size, hash-indexed)
- `go/engine/history.go` — HistoryTable indexed by ply × 4096 + from_to
- `go/engine/search.go` — Alpha-beta + PVS + null-move + LMR + quiescence + iterative deepening
- `go/engine/game.go` — Game struct with position stack, time control, search goroutine
- `go/engine/book.go` — Stub (no opening book in Go port)
- `go/cmd/pawnstar/main.go` — Full UCI protocol handler

**Binary:** build from repo root:
`go build -o pawnstar-go ./cmd/pawnstar/`

**Performance (point-in-time, 2026):** ~35 Mnps (Go ray-table) vs ~192 Mnps (C++). Gap is from the
sliding-piece attack method.

**Why:** at the time of this benchmark the C++ version used BMI2 `_pext_u64` (hardware PEXT) while Go used a
software ray-table XOR trick. NOTE: the C++ engine has since switched from pext to **magic bitboards** (a
portable multiply, see [[project_magic_bitboards]]) — so it no longer uses pext, though the slider-method
gap vs Go's ray-table remains the point. How to apply: if optimising Go perft speed, implementing magic
bitboards would be the main lever.
