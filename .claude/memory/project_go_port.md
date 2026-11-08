---
name: Go port status
description: Where the Go port of Pawnstar lives and what it is today - pointer only; the Go repo's own CLAUDE.md and README are the authority
type: project
---
The Go port is the public repo `reckless-technology/pawnstar-go` (clone: `~/Documents/git/pawnstar-go`
on Jonny's PC). Read its `CLAUDE.md` and `README.md` before changing anything there; this note is only a
pointer (checked against them 2026-11-07).

- **Bit-exact with this C++ engine** at fixed depth, single-threaded, on the **shared NNUE** (v20, the
  same `pawnstar-v20.bin` blob, embedded with `go:embed`). Bench signature **5,617,906** nodes at depth
  13. A search change here is owed to `pawnstar-rs` and `pawnstar-go` too.
- **Layout:** `main.go` at the repo root; everything else in package `engine/` (`search*.go`,
  `movegen.go`, `evaluate.go`, `nnue_bridge.go`, `uci.go`, `book.go`, ...) with the evaluator in
  `engine/nnue/`. There is no `go/` directory and no `cmd/`. The hand-crafted evaluation was removed;
  NNUE is the only evaluator, as here.
- **Opening book:** present, `engine/book.txt` (3,328 lines, built from the CC0 Lichess open database),
  `go:embed`ed and switched by the `OwnBook` option; see [[project_opening_book_provenance]].
- **Speed:** 1,028,355 nps for pawnstar-go against 1,310,183 for this engine, at 7-way match occupancy
  on lin-0, measured 2026-11-07 in the Trebuchet match analysis (about 0.78x). The old "35 vs 192 Mnps"
  figure was a pre-NNUE, pre-magic-bitboard number and is withdrawn. Go's README gives perft separately
  (about 200 Mnps single-threaded against the C++ original's ~540 on one amd64 dev machine); quote
  it with its machine and date, not from here.
