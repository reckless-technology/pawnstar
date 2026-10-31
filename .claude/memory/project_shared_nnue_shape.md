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

## Landed as the default (2026-10-30, release 2.0.0)

Step 3 of the plan above is done here: `PAWNSTAR_NNUE_KING_MIRRORING` and `PAWNSTAR_NNUE_OUTPUT_BUCKETS`
now default to **1 and 8**, and the shipped net is **`nnue/pawnstar-v20.bin`** at the shared shape -
12,617,824 bytes, SHA-256 `c8a29b328849cd1ea60c65f5d3dea84f585f404037ec026c44a94e1174b1cc34`,
`payload_crc` `0x62C460241DDC3EF7`, `header_crc` `0x5A2DFC8FA67F2B2C`. `pawnstar-v12.bin` is deleted from
the tree. The right-hand column of the values table above is therefore the *shipped* shape now, and the
left-hand one is what the knobs reach.

**The bench signature moved, which is the whole point of it**: 6,601,454 -> **5,617,906** at depth 13,
single-threaded. `pawnstar-rs` and `pawnstar-go` must produce the identical number.

**What the flip itself broke, and it was silent until the container caught it.** `CMakeLists.txt` added
`PAWNSTAR_NNUE_KING_MIRRORING=1` only in the `ON` branch and defined nothing in the `OFF` one, which was
correct while `src/nnue.h` defaulted the macro to 0. With the header's default at 1, `OFF` left the
header's value in force, so `KING_MIRRORING=0 OUTPUT_BUCKETS=1` built a **mirrored engine with one output
head** - a shape that exists nowhere. It stamped `format_version` 2 over a 12,589,060-byte payload, and
`test_net_header` is what reported it, by the mirrored map's CRC appearing on a one-head net. CMake now
defines the macro in both branches. **A compile-time default and a build-system default are two defaults,
and moving one moves the other only in the direction that happens to be spelled out.**

Its sibling trap, which cost a whole build and test cycle: `PAWNSTAR_NNUE_OUTPUT_BUCKETS` is a CMake
**cache** variable, so an existing build tree keeps the value it was configured with. A `build/` dir from
before the flip configured as mirroring-ON (passed explicitly by `tools/build.ps1`) with one output head
(from the stale cache) - the same mixed shape, from the other direction. Delete the build tree rather
than reconfiguring it.

**And the non-default shape's own suite is now only *mostly* runnable, which is worth knowing before
promising a green `make check` there.** At `KING_MIRRORING=0 OUTPUT_BUCKETS=1`, sixteen of the seventeen
suites then registered pass in **19.46 s** (Legion, clang 22.1.8 MSVC ABI, 2026-10-30); re-measured once
`opening_icc` and `pgn_nps` had landed it is **eighteen of nineteen in 22.0 s** (same machine,
2026-10-31). The one that does not,
`principal_variation`, searches the **synthetic** net to depth 8, and random weights give RFP, LMP and
LMR no traction at all, so the tree is near full width: it **had not finished in five minutes**. The
payload is the same fixed-seed stream at either shape, so this is not the weights changing - it is how
they are *read*: the same bytes under the mirrored map happened to prune well (that run measured
331.75 s) and under the unmirrored map do not. So the figure is luck of the weights, and a future
synthetic net could land either way at either shape. `CMakeLists.txt` already recorded the same effect
from the other side, a depth-12 search not finishing in two minutes. The consequence: CI's guard on the
non-default shape runs `net_header` and `nnue_incremental` - the two suites that actually read the shape,
and the two that caught the macro bug - rather than a second full `make check`, which would be an
open-ended job.
