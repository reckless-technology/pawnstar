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

**Performance:** ~35 Mnps (Go ray-table) vs ~192 Mnps (C++ PEXT). Gap is from PEXT vs ray-table sliding piece attacks.

**Why:** The C++ version uses BMI2 _pext_u64 via hardware PEXT instruction; Go uses software ray-table XOR trick.
How to apply: if optimising Go perft speed, implementing magic bitboards would be the main lever.
