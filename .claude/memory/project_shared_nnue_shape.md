# The shared NNUE shape: mirroring, output heads, and the one silent trap

2026-10-27. Five engines (`pawnstar`, `pawnstar-rs`, `pawnstar-go`, `zenith`, `stoopid-chess`) moved onto
one NNUE evaluator so a new evaluation function is developed once rather than five times. The
specification is the `reckless-nnue` skill; this note is only what is specific to this codebase.

## What landed, in order

1. **`RNNUE1`, the container, at v12's shape** (PR #36). 64-byte header, CRC-64/XZ over the header, the
   payload and the king-bucket map's contents. Zero evaluation change by construction, so the gate was the
   bench signature: **6,601,454, unchanged**. The re-stamped net is byte-identical to the one the Rust and
   Go ports produced independently — SHA-256 `531dab3e8c920a9dc66fa0ac8d61df86e1df5ddad5ea1d7fbd6ed2522744b420`.
2. **A refused net made fatal** (PR #37). See below.
3. **The new shape, behind a build knob** — mirroring and 8 output heads. Built, tested, measured; *not*
   merged, because it breaks bit-exactness with the Rust and Go ports until all three land together.

## The trap that is silent, and the shape of the fix

A perspective can be updated incrementally only while its weight bank, its square flip and its colour
flip all stay put. Before mirroring there was exactly one way for that to fail — a king crossing a bucket
boundary — and the code compared bucket indices, which was correct.

Mirroring adds a second way, and **the two are not the same**. On the mirrored map (`half * 4 + min(file,
7 - file)`) a king stepping **d1 → e1 keeps bucket 3**, because file 3 and file 4 both mirror to file 3 —
while the perspective's square flip gains the file flip and so *every feature row it reads moves*. A
comparison of buckets alone diffs an accumulator whose whole basis just changed. No crash, no assertion,
no out-of-range index: just a wrong evaluation, on about one king move in eight.

The fix is structural rather than a second condition: the three values live together in
`nnue::Perspective` and are compared **whole** (`operator==` defaulted). Both failures then go through one
comparison and neither can be forgotten when the next shape arrives.

**It was checked by breaking it.** Narrowing the comparison back to `bank_ == other.bank_` makes
`test_nnue_incremental` fail on the first ply of its first line, naming `white(bank=2304 flip=7)` — same
bank, different flip. The half-boundary lines still pass under the broken rule, which is the right
discrimination: they change the bank, which the broken comparison can still see. A test that passes under
a deliberately broken implementation is not evidence, and this one does not.

## Testing a shape before a net exists for it

Nothing has been trained at the new shape, and the shipped v12 net is rightly refused by a new-shape build
(`format_version is 0x1 in the file, 0x2 in this engine`). So `test_nnue_incremental` **builds its own net
in memory** at whatever shape the build was configured for: a deterministic xorshift payload, wrapped by
the engine's own `ExpectedNetHeader` + `FinaliseNetHeader`. Weight quality is irrelevant — the property
under test is that the incremental accumulator equals a from-scratch rebuild, which holds for any weights
at all. Two things fell out of it for free: the payload is written by an independent writer, which is a
second opinion on the layout `nnue.h` reads; and the suite stopped being a no-op in a checkout with no
`.bin`.

The hand-written crossing lines needed three attempts to be *legal* — kings adjacent, a rook covering the
square the king had to cross back over, a pawn capture onto an empty square. Generating the capture line
instead (prefer a capture, fixed seed) was both shorter and better: it walks the piece count down on its
own and reaches all eight output heads, and the test reports which heads it visited rather than assuming.
Each crossing line also reports **how many plies forced a refresh**, so a case that quietly stopped
crossing anything cannot pass while testing nothing.

## Two maps, not one map with a switch

The mirrored map is per-*file*; v12's is per file-*pair* (`half * 4 + file / 2`). Mirroring folds e–h onto
d–a, so only four distinct files are left to bucket. That means turning mirroring on changes **what a
bucket means**, which by the container's own rule is a format change — `format_version` 2. v12's map is
not mirror-symmetric, so it has no 32-entry form at all; the two shapes genuinely need different maps and
`kKingBucketMap` selects between them at compile time.

The 32 half-board numbers are written down **once for both the engine and the trainer**, and
`test_primitives` reads `KING_BUCKETS_HALF` out of `tools/bullet/pawnstar.rs` as text and compares it with
`kKingBucketMapHalf`. Crude on purpose: no Rust toolchain, no build step, no generated file to keep in
step. This is the half of the drift risk `king_bucket_map_crc` **cannot** see — the digest binds a net to
the engine build that stamped it and says nothing about the trainer, so a drifted map is only reported
after the training run has been spent. Checked by breaking it too: drifting two entries in the `.rs` file
fails the suite naming exactly those two.

## The fallback that made a match meaningless

`main.cpp` fell back to the embedded net on **any** load failure, printing "falling back to the embedded
net". So pointing `PAWNSTAR_EVALFILE` at a mismatched net started an engine playing a net nobody asked
for, and a match run that way reported a plausible Elo for a net that never played a move.

The distinction that fixes it is not env-var-versus-default but **unreadable versus refused**: a net file
that is not there is a deployment situation the embedded copy legitimately covers; a net that was read and
then refused is a mistake nothing should paper over. `nnue::IsNetFileReadable` tells them apart and is the
only place in the engine that needs to.

Its sibling: `LoadFromMemory` used to clear `loaded_` on entry, so a refused `setoption name EvalFile`
left the engine answering `readyok` with weight arrays `Load` had never filled. Every check already ran
before a weight was written, so simply not clearing it makes a refused net **change nothing** — the net
that was playing is still playing. `test_uci_commands` had asserted the old behaviour, i.e. it was
codifying the bug; the two no-net diagnostics it reached *by breaking* the loaded net are now reached on a
fresh `Game` that never loaded one.

## Values worth keeping

| | v12's shape | the shared shape |
|---|---|---|
| `format_version` | 1 | 2 |
| `flags` | 0 | 1 (bit 0, mirroring) |
| `output_buckets` | 1 | 8 |
| `king_bucket_map_crc` | `0xCC6DDB5DAF0AB18D` | `0x0A20060D4E9586E0` |
| `payload_bytes` | 12,589,060 | 12,617,760 |
| file size | 12,589,124 | 12,617,824 |

CRC-64/XZ check value, which every implementation must reproduce before anything else:
`123456789` → `0x995DC9BBDF1939FA`. It is what caught a real bug in the Go port on its first run.
