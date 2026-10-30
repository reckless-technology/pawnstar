# NNUE evaluation

Pawnstar evaluates **exclusively** with an **NNUE** (Efficiently Updatable Neural Network) — it is the
engine's only evaluator (the former hand-crafted evaluation, "HCE", was removed). At startup the engine
loads the shipped net `nnue/pawnstar-v20.bin` (relative to the working directory) and evaluates with it;
if there is no such file the engine falls back to a copy of the net embedded in the binary (§3) — but a
net that is read and *refused* is fatal, never a fallback (§3) — and
errors out only if both fail. This document is the complete reference for how the net works, how to load a
net, and how to generate data, train, validate and strength-test a net.

The shipped net (`pawnstar-v20.bin`, release 2.0.0) is a **1024-wide net with 8 horizontally mirrored king
buckets (file × board-half) and 8 material output heads** — a `ChessBucketsMirrored` net at the shared
Reckless NNUE shape — trained **entirely on Leela Chess Zero self-play data**. §1, §2 and §6 describe that
architecture; §7 has the lineage (including the retired Chess768 256/512 nets, the retired 512-wide
4-king-bucket v6/v7, the retired 1024-wide 4-bucket v9/v10/v11 and the retired 8-bucket unmirrored v12)
and, in *Provenance of the earlier networks*, where the retired nets' training data came from.

In-engine code: [src/nnue.h](../src/nnue.h) (header-only). The net is called from `EvaluatePosition`
(declared in [src/evaluation.h](../src/evaluation.h), defined in the [src/search_state.h](../src/search_state.h)
hub). Tooling: [tools/](../tools).

---

## 1. Architecture

A "perspective" network using bullet's `ChessBucketsMirrored` feature set (the shipped net uses 8 king
buckets, selected by king file × board-half after the mirror has folded files e-h onto d-a) with bullet's
`MaterialCount<8>` output heads:

```
768 inputs per (perspective, king bucket)  ->  1024 feature transformer (weights shared across perspectives)
SCReLU,  concat[side-to-move | opponent] = 2048  ->  8 output heads  ->  1  (centipawns, stm relative)
```

Constants (must match the trainer): `kInputSize=768` (features per bucket), `kNumKingBuckets=8`,
`kFeatureRows=768*8=6144`, `kHiddenSize=1024`, `kNumOutputBuckets=8`, `kQA=255`, `kQB=64`, `kScale=400`.

**Features.** The 768 base inputs per perspective are `colour (2) × piece type (6) × square (64)`. The
shipped net has 8 weight banks selected by king file × board-half, *after* horizontal mirroring
(`kKingBucketMap` = `min(file, 7 - file)`, plus 4 when the king is in its own advanced half, ranks 5–8 in
that perspective's orientation: ranks 1–4 → `0,1,2,3,3,2,1,0`, ranks 5–8 → `4,5,6,7,7,6,5,4`), so the
feature row offsets into that bank. With N banks each perspective selects one by *its own* king's square
(in that perspective's orientation) through the 64-entry `kKingBucketMap`.

**Mirroring.** A perspective whose own king stands on files e–h is mirrored: every one of *that*
perspective's feature indices is `^ 7`, which flips the file. So each perspective carries three things
that must stay put for an incremental update to be valid — its bank, its square flip and its colour flip
— and `nnue::Perspective` holds them together and compares them whole (see *Why mirroring needs its own
refresh rule*, below).

For a piece of `colour` (0=white, 1=black), type `pt` (pawn=1 … king=6) on `square`, with `wb`/`bb` the
white/black king buckets (each 0–7 for the shipped net) and `wm`/`bm` each perspective's mirror flip (7
when that perspective's own king is on files e–h, else 0):

```
white-perspective row = wb*768 + ((colour     * 384 + (pt-1)*64 +  square         ) ^ wm)
black-perspective row = bb*768 + (((1-colour) * 384 + (pt-1)*64 + (square ^ 0x38)) ^ bm)   # 0x38 flips the rank
```

Two accumulators are built (white-perspective and black-perspective), each seeded with `feature_bias`
and then summing the feature column of every piece in its king bucket's bank. At evaluation the position
selects one of the 8 **output heads** by its piece count (`(piece_count - 2) / 4`, bullet's
`MaterialCount<8>`), and within that head the **side-to-move** accumulator is fed to the first 1024
output weights and the opponent's to the second 1024. This is mathematically identical to bullet's
`ChessBucketsMirrored::map_features` and `MaterialCount<8>` with the same bucket map (verified by the
cross-check in §6 — the reference evaluator reproduces all 250 checked-in evals to **0 cp**). The bucket
map MUST stay byte-identical across `src/nnue.h`, `tools/bullet/pawnstar.rs` and
`tools/bullet/pawnstar_eval.rs`.

**Forward pass** — the **reference** path (`Network::EvaluateExact`) mirrors bullet's `simple` example
exactly; `screlu(x) = clamp(x,0,QA)²`:

```
out  = Σ screlu(stm[i])·w_stm[i]  +  Σ screlu(ntm[i])·w_ntm[i]   # i64, the SELECTED head's weights
out /= QA                 # SCReLU leaves QA*QA*QB units; reduce one QA
out += output_bias[head]  # that head's bias, in QA*QB units
out *= SCALE
out /= QA * QB            # -> centipawns, side-to-move relative
```

**Shipped output layer is int8** (`Network::Evaluate`, the search hot path): the activations are requantised
to uint8 (`screlu(x) >> 9` → `[0,127]`) and the output weights to int8, with the dot accumulated in int32 —
~1.86× faster than the int16 dot (a measured **+31.8 ± 10.3 Elo at 8+0.08**) at a bounded deviation from
`EvaluateExact` — `test_nnue`'s bound is 40 cp, and the shipped net measures exactly 40 over the 250
reference positions, against 26 for the retired single-head v12. One int8 scale is shared across all
eight heads (the dequantisation tail stays head-agnostic), so the head with the largest weight sets the
scale for the rest, which is where the extra deviation comes from. It is integer arithmetic throughout
and so identical on every platform. The `>>9` keeps every adjacent pair inside int16, so the AVX2 (`pmaddubsw`) and
AVX-VNNI (`vpdpbusd`) kernels are saturation-free and bit-identical. **Which one runs is decided at runtime
by cpuid** (`CPUID.7.1:EAX` bit 4; ~+1.3% nps on VNNI CPUs); the VNNI kernel sits behind a function-level
`target("avx2,avxvnni")` attribute, so the engine is a single baseline `-mavx2` binary that also runs on
AVX2-only CPUs (no `make VNNI=1` flag — the gate just never calls the VNNI kernel there).
The int16 *accumulator* and feature transformer are unchanged (the int8 *feature*-weight experiment was
measured and **removed** after a definitive −8 Elo SPRT; see [int8_quant_study.md](int8_quant_study.md)).

The net is an `nnue::Network` class (the quantised weights plus `Load`/`Refresh`/`Update`/`Evaluate`
methods), **owned by the `Game`** — there are no nnue globals. The `Game` is **heap-allocated**
(`std::make_unique<Game>()` in `main.cpp` and the tests): at 8 king buckets the inline ~12.6 MB
`feature_weights_` array would overflow the default 8 MB stack if the owning object were a local. The FT
weights stay an **inline member** rather than a `unique_ptr` to a separate array — a separate (page-aligned)
allocation makes the 2048-byte feature-column stride alias the same cache sets, costing ~10% nps (measured);
keeping the array inline in the heap object is speed-neutral. Its accumulator is **maintained
incrementally and lazily** on each thread's `SearchState` — `PlayMove` defers the update; the accumulator is
brought current only when an evaluation reads it (and only on an eval-cache miss), so nodes that cut off
before evaluating pay nothing (SPRT-measured +20.66 ± 9.04 Elo at 8+0.08, +13.6% nps). The *delta* in the
common case (no king-bucket
crossing — the common case) `Network::Update` derives the changed squares directly from
the parent/child **colour bitboards** — a move touches only 2–4 squares, and captures, en passant,
castling and promotion all fall out uniformly — and applies just those feature-column deltas to both
perspectives (no per-piece-type scan; reversible on undo, a no-op for null moves). This is bit-identical to
a per-piece diff: the same columns are added/subtracted and int16 accumulator add/sub is order-independent.
When a king move **crosses a bucket boundary** that whole perspective's bank changes, so `Update` rebuilds
just that one perspective from scratch (at most one king moves per ply, so at most one perspective
refreshes per update); `Network::Refresh` rebuilds both at the root.
`Network::Evaluate(position)` (a full refresh) is the reference the incremental path is checked against
(§6). Both kernels (accumulator update and forward pass) are **AVX2-vectorised** when built with
`-mavx2`, with scalar fallbacks behind `#if defined(__AVX2__)`; the vectorised path is bit-identical to
the scalar one (verified by `test_nnue`).

### The two network shapes, and the build knob that picks one

Five engines are being unified onto one NNUE evaluator (skill `reckless-nnue`), and the shared
architecture - **the default here since release 2.0.0, and the shape the shipped net was trained for** -
adds two things to the shape the retired v12 net needed. Both were measured in zenith on this hardware,
SPRT-gated, and both only paid past ~1.4B training positions - at 190M they were neutral or negative,
which is why v12 had neither:

| | v12's shape | the shared shape (**the default**) | measured |
|---|---|---|---|
| horizontal king mirroring | off | **on** - a perspective whose own king is on files e-h is mirrored (`square ^ 7`) before indexing | **+14.3 Elo** fixed depth |
| material output heads | 1 | **8**, selected by `(piece_count - 2) / 4` | **+14.7 Elo** fixed depth |
| king bucket map | `half * 4 + file / 2` (file-*pairs*) | `half * 4 + min(file, 7 - file)` (per-*file*) | |
| `format_version` | 1 | **2** | |
| payload bytes | 12,589,060 | 12,617,760 | |

Mirroring is worth more than its Elo suggests: halving the king-placement space gives **twice the
effective data per bucket at the same file size**, so it is a data multiplier as much as an Elo add.

**Either shape builds from this one tree.** The two knobs default to the shared shape; the v12 shape is
what they now reach:

```bash
make check BUILD_DIR=build-v12shape KING_MIRRORING=0 OUTPUT_BUCKETS=1                  # Unix
tools\build.ps1 check -BuildDir build-v12shape -KingMirroring:$false -OutputBuckets 1  # Windows
```

Give the other shape a **build tree of its own** - a CMake cache variable already set in an existing tree
is not reverted by a change of default, so a stale `build/` can end up a mixture of the two - and expect
it to refuse `nnue/pawnstar-v20.bin`: `refusing net '...': output_buckets is 8 in the file, 1 in this
engine`. That is the container working, not a fault: no net of that shape is shipped any more.
`test_nnue_incremental` builds its own synthetic net at whatever shape the build was configured for,
which is what gives the non-default shape a net to test with at all.

**The whole suite runs at either shape**, with one caveat worth knowing before you run it: at the
non-default shape sixteen of the seventeen suites pass in about 20 seconds, and `principal_variation`
searches the **synthetic** net to depth 8, where random weights give the pruning heuristics no traction -
measured on this machine, it had not finished in five minutes at the v12 shape. That is a property of
random weights rather than of the shape (the same thing is recorded in `CMakeLists.txt` for a depth-12
search at the shared shape), and it is why CI's guard on the non-default shape runs the two suites that
actually read the shape - `net_header` and `nnue_incremental` - rather than a second full `make check`.

The suites whose expected values belong to the shipped net specifically - the trainer reference
evaluations (`nnue`), the Bratko-Kopec move set, and the 5,617,906 bench signature - skip on
`nnue::IsNetFileThisBuildsShape`,
extending their existing "skipped (no net supplied)" contract from *absent* to *absent or of another
shape*, which is what the Rust and Go ports already do. The rest are handed a net of the build's own
shape instead of being skipped: at a non-default shape CMake builds `<build dir>/synthetic-net.bin`
(the `synthetic_net` target) and gives it to `nnue_incremental`, `principal_variation`, `uci_commands`
and, through `PAWNSTAR_EVALFILE`, the `uci` protocol suite. What those check holds for any weights at
all, so either shape gets real coverage rather than a row of skips.

### The new shape's cross-port node-count signature

`bench` is the ports' primary signature and, at the shared shape, runs on the shipped net as usual
(5,617,906 nodes at depth 13). Beside it sits a **net-independent** parity signature for that shape,
which is what gated it before any net existed for it and what still gates it whatever net is shipped:
six fixed-depth searches over the synthetic net, whose bytes are identical on every
platform and in all three ports. [`tools/new-shape-signature.sh`](../tools/new-shape-signature.sh) is
that gate, and it is **committed byte-identical in `pawnstar`, `pawnstar-rs` and `pawnstar-go`**: the
six expected results live in it and nowhere else, so `diff` between the three repositories is the whole
audit and there is no second copy of the numbers to drift. CI runs it on every pull request (the
`new-shape` job).

```bash
make BUILD_DIR=build-new-shape KING_MIRRORING=1 OUTPUT_BUCKETS=8 check   # explicit, though now the default
build-new-shape/test_nnue_incremental --write-synthetic-net build-new-shape/synthetic-net.bin
tools/new-shape-signature.sh --hash 64 --net build-new-shape/synthetic-net.bin build-new-shape/pawnstar
```

(At the default shape CMake does not build `synthetic-net.bin` for the test suites - they are handed the
shipped net instead - so write it explicitly, as the second line does. The script and its six expected
results are unchanged, and must stay byte-identical across the three repositories.)

It asserts the net's size, SHA-256 (`4abfdb7b…871b`, 12,617,824 bytes) and RNNUE1 header - including
`format_version` 2, `payload_bytes` 12,617,760 and `king_bucket_map_crc` `0x0A20060D4E9586E0` - **before**
searching it, so a drifted generator fails as a drifted generator instead of reading as a search
regression. `--hash` is mandatory and deliberately not defaulted, and its value is printed beside every
node count: table size changes the collision pattern and so the node count at a fixed depth, which is
how `pawnstar-go`'s 512 MB default stayed invisible for five months against this port's 64 MB.

**A shape change is an evaluation change.** It moves the bench signature and breaks parity with
`pawnstar-rs` and `pawnstar-go` until all three are ported together, so the defaults are not to be
flipped for one engine alone. The mirrored map also changes what a bucket *means* rather than only how
many there are, which by the container's own rule makes it a format change - hence `format_version` 2.

**Why mirroring needs its own refresh rule, and why getting it wrong is silent.** A perspective can be
updated incrementally only while its weight bank, its square flip and its colour flip all stay put; if
any changes, every feature row it reads moves. Before mirroring there was one way for that to happen, a
king crossing a bucket boundary, and the code compared bucket indices. Mirroring adds a second, and the
two are not the same: on the mirrored map a king stepping **d1 -> e1 keeps bucket 3** (file 3 and file 4
both mirror to file 3) while its square flip gains the file flip. A comparison of buckets alone would
diff an accumulator whose whole basis had just changed - no crash, no assertion, just a wrong
evaluation. So the three live together in one `nnue::Perspective`, compared whole, and
`test_nnue_incremental` crosses each boundary on purpose and reports how many plies forced a refresh, so
a case that quietly stopped crossing anything cannot pass. Breaking the comparison back to the bucket
alone makes that suite fail on the first ply of the first line, which is how the test was checked.

**The bucket map is written down once, for both the engine and the trainer.**
`tools/bullet/pawnstar.rs`'s `KING_BUCKETS_HALF` and `src/nnue.h`'s `kKingBucketMapHalf` hold the same 32
numbers - half a board, indexed `rank * 4 + mirrored_file` - and the engine *generates* its 64-entry map
from them with bullet's own expansion arithmetic
(`expanded[sq] = half[(sq / 8) * 4 + [0,1,2,3,3,2,1,0][sq % 8]]`). `test_primitives` reads the array out
of the `.rs` file and compares it number for number with the engine's, because `king_bucket_map_crc`
binds the map to the engine build that stamped a net and can say nothing about the trainer - that was the
digest's honest limitation, and this closes it. A map that drifts trains one network and runs another,
and the digest reports it only after the training run has been spent.

## 2. File format — the shared `RNNUE1` container

Pawnstar's nets are in **`RNNUE1`**, the one container every reckless-technology chess engine reads —
`pawnstar`, `pawnstar-rs`, `pawnstar-go`, `zenith` and `stoopid-chess` (skill `reckless-nnue`). A net
trained once loads in all five, and a change to the container is a change owed to all five. It replaced
pawnstar's own 32-byte `PSN1` header on 2026-10-27; `src/nnue.h` is the authority for this build.

Little-endian throughout: a **64-byte header**, then the weight payload, and nothing after it.

### The header

| offset | size | field | this build |
|---|---|---|---|
| 0 | 8 | `magic` | `"RNNUE1\0\0"` |
| 8 | 2 | `format_version` | 1 at v12's shape, **2** with king mirroring on (section 1) |
| 10 | 2 | `header_bytes` | 64 — lets a later version extend the header without breaking this reader |
| 12 | 2 | `features_per_bucket` | 768 |
| 14 | 2 | `king_buckets` | 8 |
| 16 | 2 | `hidden_size` | 1024 |
| 18 | 2 | `output_buckets` | 1, or 8 at the shared shape |
| 20 | 1 | `flags` | bit 0 = horizontal king mirroring; 0 at v12's shape, 1 at the shared shape. All other bits zero, and a non-zero `reserved` is refused |
| 21 | 1 | `activation` | 0 = SCReLU |
| 22 | 2 | `qa` | 255 |
| 24 | 2 | `qb` | 64 |
| 26 | 2 | `eval_scale` | 400 |
| 28 | 4 | `reserved` | zero |
| 32 | 8 | `king_bucket_map_crc` | CRC-64 of the 64 map entries, one `uint8` each. `0xCC6DDB5DAF0AB18D` for v12's file-pair map, `0x0A20060D4E9586E0` for the mirrored one |
| 40 | 8 | `payload_bytes` | 12589060, or 12617760 at the shared shape |
| 48 | 8 | `payload_crc` | CRC-64 over the payload |
| 56 | 8 | `header_crc` | CRC-64 over bytes 0–55 |

### A real header, decoded

The shipped `pawnstar-v20.bin`, stamped on 2026-10-30 (whole file SHA-256
`c8a29b328849cd1ea60c65f5d3dea84f585f404037ec026c44a94e1174b1cc34`, 12,617,824 bytes). Worth keeping
because every field below was *verified* against the specification above rather than assumed, and because
`test_net_header` pins these same values against the real file:

```
52 4e 4e 55 45 31 00 00  02 00  40 00  00 03  08 00
00 04  08 00  01  00  ff 00  40 00  90 01  00 00 00 00
e0 86 95 4e 0d 06 20 0a  20 88 c0 00 00 00 00 00
f7 3e dc 1d 24 60 c4 62  2c 2b 7f a6 8f fc 2d 5a
```

| bytes | field | value |
|---|---|---|
| `52 4e 4e 55 45 31 00 00` | `magic` | `RNNUE1  ` |
| `02 00` | `format_version` | **2** — mirrored, with eight output heads |
| `40 00` | `header_bytes` | 64 |
| `00 03` / `08 00` | `features_per_bucket` / `king_buckets` | **768** / **8** |
| `00 04` / `08 00` | `hidden_size` / `output_buckets` | **1024** / **8** |
| `01` / `00` | `flags` / `activation` | king mirroring on / SCReLU |
| `ff 00` / `40 00` / `90 01` | `qa` / `qb` / `eval_scale` | **255** / **64** / **400** |
| `00 00 00 00` | `reserved` | zero |
| `e0 86 95 4e 0d 06 20 0a` | `king_bucket_map_crc` | `0x0A20060D4E9586E0` |
| `20 88 c0 00 00 00 00 00` | `payload_bytes` | **12,617,760** |
| `f7 3e dc 1d 24 60 c4 62` | `payload_crc` | `0x62C460241DDC3EF7` |
| `2c 2b 7f a6 8f fc 2d 5a` | `header_crc` | `0x5A2DFC8FA67F2B2C` |

A net at the retired v12 shape carries `format_version` **1**, `output_buckets` **1**, `flags` **00**,
`payload_bytes` **12589060** and `king_bucket_map_crc` **0xCC6DDB5DAF0AB18D**. The v12 net itself, as
re-stamped on 2026-10-27, carried `payload_crc` `0x8A91EC366E6B450B` and `header_crc`
`0x19EE0DC79A04735D` with whole-file SHA-256
`531dab3e8c920a9dc66fa0ac8d61df86e1df5ddad5ea1d7fbd6ed2522744b420` — which three independent
implementations, C++, Rust and Go, produced identically.

### Reading a header: `print_net_header`

`build/print_net_header <net.bin>` (`make tools`) is the one reviewed reader. **Use it instead of `od`**: an
`od` script that read its `uint16` fields from byte 4 when the magic is 8 bytes printed every label two fields
out for several multi-hour matches, because a decoder outside the tree has no test. This one decodes against the
engine's own `nnue::NetHeader` (nothing transcribed), prints every field, verifies `header_crc` and
`payload_crc`, checks the payload length and the header's own arithmetic, and says whether the file's shape is
this build's. It **never refuses a file for its shape**: it describes any `RNNUE1` net, including one this build
could not load, which is when a header is most worth reading.

Output is `key=value` lines in a fixed order on stdout, so a script needs no parser
(`print_net_header net.bin | sed -n 's/^king_buckets=//p'`); a human reading goes to stderr. Integers are
decimal, flags and CRCs `0x` upper-case hex, booleans `yes`/`no`, a value that cannot be known `unavailable`.
`integrity=ok` means the magic, both CRCs, the payload length and the payload arithmetic all hold, and the exit
status says it too (0 intact whatever the shape, 1 not intact, 2 unreadable). The keys are the header's own field
names, then `<check>_computed` / `<check>_ok` pairs, then `integrity`, then `shape_matches_build` and
`shape_mismatches` (a comma-separated list of the architecture fields that differ from this build's constants).

```
$ build/print_net_header nnue/pawnstar-v20.bin
file=nnue/pawnstar-v20.bin
file_bytes=12617824
header_complete=yes
magic=RNNUE1  
magic_ok=yes
format_version=2
header_bytes=64
features_per_bucket=768
king_buckets=8
hidden_size=1024
output_buckets=8
flags=0x01
king_mirroring=on
...
king_bucket_map_crc=0x0A20060D4E9586E0
payload_bytes=12617760
payload_crc=0x62C460241DDC3EF7
header_crc=0x5A2DFC8FA67F2B2C
header_crc_ok=yes
payload_size_ok=yes
payload_crc_ok=yes
integrity=ok
shape_matches_build=yes
shape_mismatches=
```

`test/net_header_test.cpp` is its test: hand-laid fixtures with literal expected values and CRCs from a separate
implementation, and the failing verdict asserted for each kind of damage.

### Who writes the header, and why it should be the trainer

**The trainer is the authority on what a net is; the engine is only an authority on what it can
load.** `tools/stamp_net.cpp` currently derives the header from *this engine's* compile-time
constants, which describes the reader's expectation rather than the artefact's truth — and that has a
consequence that is easy to miss:

`king_bucket_map_crc` exists to catch a **trainer/engine map drift**, the failure where the trainer
trains one network and the engine runs another. But a stamper compiled from the engine's own map
writes that engine's map digest **by construction**, and the loader then checks it against the same
map. It agrees tautologically, and the field can only ever fire when a net is cross-loaded into a
*differently built* engine. The drift it was added for is exactly the case it cannot see.

Moving the write to the trainer makes it a real cross-check: the trainer asserts *the map I trained
with*, the engine verifies *the map I index*, and a drift between them is refused at load. The
separation to aim for is **producer asserts, consumer verifies** — the trainer writes the whole
header, both CRCs and the bullet-save conversion (int16 output bias widened to int32, trailing
padding stripped); each engine validates every field against its own constants and refuses loudly,
and does nothing else. That also collapses one stamper per architecture into one stamper, and removes
the "stamper built at the wrong architecture" failure class entirely.

`verify_net.sh` stays engine-side, because "does this net load *and play* here" is the one question
only this engine can answer.

### The payload

Bucket-major, exactly as bullet's save order emits it:

| section | count | type | scale |
|---|---|---|---|
| `feature_weights` | `768*8 * 1024 = 6291456` (column-major `row*1024 + i`, rows bucket-major `bucket*768 + feature`) | int16 | QA |
| `feature_bias` | `1024` | int16 | QA |
| `output_weights` | `output_buckets * 2 * 1024 = 16384` (per bucket: own perspective's 1024 first, then the opponent's) | int16 | QB |
| `output_bias` | `output_buckets = 8` | **int32** | QA·QB |

So `12582912 + 2048 + 32768 + 32 = `**`12617760`** payload bytes, and `nnue/pawnstar-v20.bin` is
**12617824 bytes** (64 + 12617760). At the retired v12 shape — one output bucket and mirroring off — it
is `12582912 + 2048 + 4096 + 4 = 12589060`, i.e. 12589124 with the header.

Two details that catch a reader of the old format out. The output bias is **int32**, one per output
bucket, where PSN1 stored a single int16; and the payload length is **exact**, where PSN1 kept bullet's
tail padding (bullet pads its quantised output up to a 64-byte boundary with repeats of the word
"bullet"). `stamp_net` drops that padding, because an exact length is what lets a truncated or an
appended-to file be told apart from a sound one.

### CRC-64

**CRC-64/XZ**, also called CRC-64/GO-ECMA (`src/crc64.h`): polynomial `0x42F0E1EBA9EA3693`, reflected
(table constant `0xC96C5795D7870F42`), init `0xFFFFFFFFFFFFFFFF`, reflect in and out, final XOR
`0xFFFFFFFFFFFFFFFF`. A 64-bit CRC because the payload is ~12.6 MB; a CRC and not a cryptographic hash
because this guards against accidental corruption, not tampering. The 256-entry table is built by a
`consteval` function, so there is no dependency and nothing generated to keep in step.

**The check value every implementation must reproduce:** CRC-64/XZ of the nine ASCII bytes `123456789` is
`0x995DC9BBDF1939FA`. `src/crc64.h` asserts it at compile time and `test_primitives` asserts it at run
time, before anything else that suite does. (Go gets the table free from `hash/crc64` with `crc64.ECMA`,
but Go's `Checksum` starts at zero: CRC-64/XZ is `crc64.Update(^uint64(0), table, data) ^ ^uint64(0)`.)

### The two checks the loader makes, in this order

`Network::LoadFromMemory` (and so `Network::Load`):

1. **Header.** Read 64 bytes. Verify the magic, then `header_crc` over bytes 0–55, then every architecture
   field against this build's compile-time constants, then `king_bucket_map_crc` against the CRC of this
   build's own `kKingBucketMap`, then that every `reserved` byte is zero. `reserved` is refused unless it
   is zero on purpose: those four bytes are the only room a later version has to add a field without
   moving anything, and that only works if a reader of *this* version refuses a net that has already
   spent them. `qa`, `qb` and `eval_scale` are read **unsigned**.
2. **Payload.** Verify `payload_bytes` against the bytes actually present after the header, then
   `payload_crc` over them.

**A refusal is loud and does not fall back.** Each one names the field, the file's value and the engine's
(`refusing net '…': king_buckets is 0x9 in the file, 0x8 in this engine`), on **stdout as an `info
string` and on stderr as a plain line** — a GUI reads the first, a match harness keeps the second and
routinely drops info lines. The net is not loaded, and because every check happens before a weight is
written, a net that was already loaded is left untouched and still playing. A net that loads says so the same way
(`info string NNUE: loaded net '…'` on stdout, `NNUE: loaded net '…'` on stderr), so the line a GUI reads and the
line `tools/run_sprt.sh` reads name the same net. The failures that make this worth the bytes have all been measured on these machines:

- a stock engine handed a mismatched net through an env var **silently fell back to its embedded net** and
  played on, so a match reported a plausible Elo for a net that never played a move;
- a mismatched net set through `setoption name EvalFile` left the network unloaded, the engine still
  answered `readyok`, then crashed on the first `go`;
- `king_buckets` matching while the **map contents** differed loaded cleanly and played while reading the
  wrong weight bank. That is what `king_bucket_map_crc` closes.

`test_uci_commands` drives one net per fault through `setoption name EvalFile` — wrong magic, a PSN1 net, a
corrupt header, a corrupt payload, truncated, appended to, too short, and each architecture field and
digest changed in turn with `header_crc` recomputed so the field check is what fires — and asserts that
every one leaves no net loaded.

### Pinning the king-bucket map in the net header — **done** (2026-10-27)

This section used to propose a map digest and defer it to the next format change. **This was that change,
and the digest shipped with it**: `king_bucket_map_crc`, at offset 32, mandatory, checked beside the other
architecture fields.

The hole it closes: a header that records `kNumKingBuckets` — the bank *count* — says nothing about
`kKingBucketMap`'s *contents*. Two maps of the same cardinality produce a byte-identical file size and a
header that validates field for field, while selecting different banks for the same king square. A net
trained with one map and evaluated with another **loads, plays, and reads the wrong bank on every
evaluation**; `payload_bytes` cannot catch it either, since the payload size depends only on the row count
`768 * king_buckets`. The failure mode is a confident wrong number rather than a crash — the engine simply
plays worse, and the natural reading is that the architecture was a bad idea.

That was live, not hypothetical: a 4-bucket experimental net whose map is **file-pair only**
(`0,0,1,1,2,2,3,3` repeated down all eight ranks, no board-half split) and an `h512-b4` build directory on
one of the CI machines both stamped and loaded against a `kNumKingBuckets = 4` build. Those two maps are
both plausible things to have in a tree at once, and before the digest nothing reported the difference.

Three things changed from the proposal. It is a **CRC-64, not an FNV-1a**, and it has its own 8-byte field
rather than living in `reserved_` — the container carries CRC-64 anyway for the header and the payload, so
reusing it costs no new code. The format version did **not** need bumping to make the field mandatory,
because the magic changed: a PSN1 net cannot be mistaken for an RNNUE1 one, so there is no "stamped before
the field existed" case to tell apart from a map that really hashes to zero. And `stamp_net` **prints**
the digest (and every other stamped value) on every stamp, which was the cheap fallback — it composes, so
§7's net table can record a number per net and two experiment nets can be told apart by comparing numbers
rather than reading two arrays side by side.

The honest limitation stands: the digest binds only the engine side. It proves the net in front of this
build was stamped by a build with *this* map; it cannot prove the **trainer** used that map, because
`tools/bullet/pawnstar.rs` computes nothing from this header. That narrows the window to "stamped by the
wrong build", which is the likelier mistake anyway.

### Migrating a PSN1 net

`tools/convert_psn1_payload.py` reads a net in the retired PSN1 container and writes a *raw* RNNUE1
payload — sign-extending the single int16 output bias to int32 and dropping bullet's tail padding, which
are the only two differences. `stamp_net` then writes the header from its own compiled constants, so no
header is ever produced by hand:

```bash
python3 tools/convert_psn1_payload.py old-psn1-net.bin /tmp/raw.bin
./build/stamp_net /tmp/raw.bin nnue/converted.bin
```

That is exactly how the then-shipped `pawnstar-v12.bin` was moved across on 2026-10-27, and it was
**bit-for-bit the same trained weights**: widening an int16 to int32 is a sign extension. Which is why the
bench signature stayed at **6,601,454** across that format bump, and `test_nnue` still matched the
trainer's 250 reference evals. A moved bench would have meant the loader was wrong, and that gate cost no
training at all. (No PSN1 net is shipped now - v12 was the last - so this is a migration tool for a net
held outside the repository, and only a one-output-head net can be converted, which is every PSN1 net
there ever was.)

**Stamping.** Stamp a raw bullet net with `tools/stamp_net` (`make tools`); the header values come from the
*building engine's* constants, so it can only stamp a net that matches that build — a 512-wide net stamped
by a 1024-wide tool gets a 1024-wide header and is refused on first load, which is the point. It accepts
only a raw net of the exact payload size (plus at most 63 bytes of bullet padding), so it cannot re-stamp
an already-stamped file. The forward-pass math and quantisation are kept in lock-step with the trainer
([tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs)); §6 verifies they agree to within rounding.
**Never hand-edit a net file** — nets come from the trainer and are stamped by a `stamp_net` built at that
net's own architecture. A net file's name says nothing about its architecture; the header does.

(The retired nets had different sizes and the PSN1 header — the Chess768 512-wide v4 a 789506-byte
payload, the 512×4 king-bucketed v6/v7 a 3148802-byte payload, the 1024-wide 4-bucket v9/v10/v11 a
6297664-byte payload — so this build's size, header and map digest gate all of them out.)

## 3. Using a net in the engine

A net is required (NNUE is the only evaluator). `main.cpp` loads `nnue/pawnstar-v20.bin` (cwd-relative) at
startup. **Embedded fallback:** a byte-identical copy of the shipped net is compiled into the engine
binary (`src/embedded_net.S` `.incbin`'s the net at the build's `EMBED_NET` path — the Makefile and the
CMake build both pass it via `-DEMBED_NET_PATH`, the single source of truth, so the embedded copy can't
drift from the shipped net; the `.S` embeds into ELF `.rodata` or COFF `.rdata`, so the engine binary is
self-contained on Linux and Windows alike; linked into the engine only, not the test binaries).

**The embedded copy covers a net file that is not there, and nothing else.** A net that was read and then *refused* — wrong architecture, the wrong bucket map, a corrupt payload — is **fatal**: the engine prints the reason on stdout and on stderr and exits non-zero rather than quietly playing a different net. The old code fell back on *any* failure, which is the measured trap: pointing `PAWNSTAR_EVALFILE` at a mismatched net started an engine that played the embedded net instead and said so only in an `info string`, so a match reported a plausible Elo for a net that never played a move. An engine that cannot play the net it was given must not play at all.

So the embedded copy is loaded when there is no net file to read — a wrong working directory, or a
binary copied somewhere on its own — via `Network::LoadFromMemory`, which makes the same two checks as
`Network::Load`. The engine exits only if that copy is refused too, which would mean the build itself is
corrupt. `nnue::IsNetFileReadable` is what tells the two cases apart, and it is the only place in the
engine that needs to. To use a *different* net, point the engine at it — at launch:

```bash
PAWNSTAR_EVALFILE=/path/to/net.bin ./build/pawnstar
```

or at runtime:

```
setoption name EvalFile value /path/to/net.bin
```

`EvalFile` is advertised in the `uci` response; there is no `UseNNUE` option (nothing to toggle to). The
`eval` command prints the NNUE evaluation of the current position; `nnue` prints the raw network eval. The
net is a single `nnue::Network` instance owned by the (heap-allocated) `Game` (UCI `setoption` routes to
`game.nnue_network_.Load(...)`), read-only after load and shared by all Lazy SMP threads through their
`Game&` — they call only its `const` methods.

## 4. Tooling

| file | purpose |
|------|---------|
| `gen_data.cpp`            | self-play training-data generator: the engine plays itself and writes `<fen> \| <eval_cp> \| <wdl>` text shards plus a `.meta` provenance sidecar (`make tools` → `build/gen_data`) — see §6 |
| `gen_openings.cpp`        | balanced opening generator from **our own play**: short randomised lines from the start position, filtered on the net's static evaluation and deduplicated on the Zobrist key, written as EPD with its provenance header (`make tools` → `build/gen_openings`) — the data campaign's opening source, see §6 |
| `run_gendata.sh`          | fan `gen_data` across cores, one shard per worker (each with a disjoint slice of the opening pool), resumable, reporting the pool's duplicate rate at the end |
| `check_duplicates.sh`     | measure what fraction of an assembled pool is distinct positions — the gate `train_pipeline.sh` runs before converting a directory of shards, and the cheap way to catch a degenerate campaign in minutes |
| `make_openings.sh`        | build `openings.epd`, the **match** set, from the public-domain TSCP opening book: replay each line, keep the roughly balanced positions, dedup (capped at ~420 positions by that source, which is why data generation uses `gen_openings` instead) |
| `print_net_header.cpp`    | read any `RNNUE1` net and print every header field as `key=value`, with `header_crc` and `payload_crc` verified and the shape compared with this build's (`make tools` → `build/print_net_header <net.bin>`; the logic is `net_header_report.h`, tested by `test_net_header`) |
| `stamp_net.cpp`           | wrap a raw bullet net in the shared `RNNUE1` container: the 64-byte header, written from that build's own constants, with the map digest and both CRCs (`make tools` → `build/stamp_net`) |
| `convert_psn1_payload.py` | one-off: read a net in the retired `PSN1` container and write a raw `RNNUE1` payload for `stamp_net` (§2) |
| `setup_bullet.sh`         | clone bullet at the pinned commit + install/register our trainer examples |
| `bullet/pawnstar.rs`      | the trainer (arch `768×8 king-bucketed → 1024×2 → 1`, SCReLU, QA/QB/SCALE) |
| `bullet/pawnstar_eval.rs` | reference evaluator for the `test_nnue` cross-check |
| `train_pipeline.sh`       | train → export a net from a bulletformat `.data` file (or a directory of text shards) |
| `verify_net.sh`           | cross-check (engine == trainer) + incremental-accumulator gate for a trained net |
| `run_sprt.sh`             | SPRT / match: net-vs-net (set `BASELINE_NET`), at fixed depth or time control |
| `rate.sh`                 | anchored absolute-Elo estimate vs rated reference engines (you supply the opponent binaries) |
| `run_experiment.sh`       | one-shot: train + **verify** + SPRT from an existing `.data` dataset |
| `filter_book.cpp`         | offline opening-book filter: NNUE-search every book move and flag questionable ones (lose > margin vs best) with a trailing `?`, so the engine replays them in book but never plays them (`make tools` → `build/filter_book`) |
| `nnue_quant_study.cpp`    | offline int16→int8 quantisation feasibility study (range/saturation histograms + kernel cost split); a measurement tool, not part of the engine — see [int8_quant_study.md](int8_quant_study.md) (`make tools` → `build/nnue_quant_study`) |

**Prerequisites:** `clang++`/`make` (engine), Rust/`cargo` (bullet), `fastchess` on `PATH` (SPRT). For
GPU training, a CUDA toolkit (`CUDA_PATH`, default `/usr/local/cuda`) **and** a driver new enough for the
toolkit: bullet's CUDA backend calls `cuFuncLoad`, which needs a driver supporting CUDA ≥ 12.3
(driver ≥ 545). Otherwise train on CPU with `BULLET_FEATURES=""` — the net is small enough that CPU
training is practical. `tools/setup-dev.sh --training` (`tools\setup-dev.ps1 -Training` on Windows) installs Rust's
stable toolchain, checks for the GPU and the toolkit and runs `setup_bullet.sh`; `fastchess` is still yours to build.
On Windows `setup_bullet.sh` runs under Git's bash and `train_pipeline.sh` under MSYS2 (with
`MSYS2_PATH_TYPE=inherit`, so cargo and nvcc are on its `PATH`), and `BULLET` defaults to
`%USERPROFILE%\pawnstar_nnue\bullet`, since MSYS2's `~` is a home of its own.

## 5. Quick start (minimal retrain)

```bash
tools/setup_bullet.sh                                          # once: clone + register bullet (pinned commit)
OPENINGS=nnue/openings.epd \
  tools/run_experiment.sh ~/pawnstar_nnue/shuffled.data net.bin   # train + verify + SPRT  ->  net.bin
```

Then load `net.bin` in the engine (§3). To train on CPU: `BULLET_FEATURES="" … tools/run_experiment.sh …`.
`run_experiment.sh <shuffled.data> <net.bin> [superbatches=40] [sprt_rounds=500] [depth=8]` chains
`train_pipeline.sh` → `verify_net.sh` → `run_sprt.sh`. The data argument is an already-converted
bulletformat `.data` file (how the shipped nets were trained — see §7); it carries no FENs to sample
openings from, so the SPRT's opening book comes from `OPENINGS=…` — the committed
[openings.epd](openings.epd) (§6, Openings), or one of your own.

## 6. Step-by-step

### Data

The shipped net's pool was built **entirely from Leela Chess Zero self-play data** with
`tools/lc0_extract` (below) and assembled into one pre-converted, shuffled `.data` file (bullet
`ChessBoard` records, 32 bytes each). The nets up to and including v12 were trained the same way on **the
PlentyChess bulletformat dataset, by Yoshie2000**, which **is no longer used and is not to be
re-downloaded** - it has been withdrawn, so it is no longer public either. It stays named here, and
credited in the repository README and in §7 (*Provenance of the earlier networks*), because those nets
were trained on it. Training still takes a shuffled `.data` file as its input, and for a new net that
file is built from the sources below.

`train_pipeline.sh` also accepts a directory of text shards (`data_*.txt` / `seed*.txt`) in bullet's
`convert --from text` format — one record per line, `<fen> | <white_relative_eval_cp> | <wdl>` (wdl:
1.0 win / 0.5 draw / 0.0 loss, White's POV) — which it combines, converts, and shuffles. Bring such
shards from any source in this format, use a `.data` file directly, or write them with one of the three
sources in `tools/`: **`gen_data`** (this engine's own self-play, below), **`lichess_extract`** (the Lichess
open database, CC0 — human games, about a tenth of them carrying inline Stockfish `[%eval]` comments) or
**`lc0_extract`** (Leela's `test80` v6 archive, ODbL — engine self-play with engine results and an MCTS value
estimate from a far stronger net). The three are not on one numeric scale, so read `tools/lc0_extract.h`
before blending them.

**Both the score and the result are White-relative, not side-to-move relative.** bullet's text parser
mirrors the board for a black-to-move FEN and negates the score and the result itself (`ChessBoard::from_str`
in `bulletformat`), so writing side-to-move-relative values inverts the label of every black-to-move
position. Verified by converting a white-to-move and a black-to-move copy of the start position, both
written `| 123 | 1.0`: the two 32-byte records came out `score=123 result=2` and `score=-123 result=0`.

#### Self-play generation (`gen_data`)

`gen_data` has the engine play itself with the loaded net and writes one record per kept position:

```bash
make tools      # -> build/gen_data
tools/run_gendata.sh ~/pawnstar_nnue/selfplay 24 400 \
    --openings nnue/openings-selfplay.epd --play-nodes 12000 --label-nodes 220000 --label-interval 4 \
    --pv-plies 4
tools/train_pipeline.sh ~/pawnstar_nnue/selfplay net.bin        # the directory mode above
```

**The search is the teacher, not the net** — the net only has to play a coherent game, while the label is
what the search backs up. That is why the generator is two-stage: it plays on a cheap budget
(`--play-nodes` / `--play-depth`) and re-labels only every Nth kept quiet position (`--label-interval`)
with a much larger budget (`--label-nodes` / `--label-depth`), which decouples the cost of playing from
the cost of the label. `--pv-plies` then harvests the first few plies of each deep label's principal
variation as further records; they all carry the same White-relative root score, since a White-relative
score does not depend on whose turn it is. That harvest is what makes a deep label affordable — 3.82×
the records at the same cost, measured below.

Each feature is switchable on its own so it can be A/B tested: `--label-interval 0` is single-stage,
`--pv-plies 0` turns the harvest off, `--disagreement-copies 1` turns off the mild reweighting that writes
a record twice when the search and the static eval disagree by more than `--disagreement-cp` (no position
is ever *dropped* for agreeing — that would skew the distribution the eval is calibrated against), and
`--no-dedup` keeps positions repeated within a shard. Kept positions are quiet, not in check and under
3000 cp; games are adjudicated at 1500 cp held for four plies and capped at 200 plies. The search runs
single-threaded, so a shard is reproducible from its seed. Every shard gets a `<shard>.meta` sidecar (a
separate file, because `convert --from text` would try to parse a comment line as a position) recording
the engine version, the labelling net **and its SHA-256**, every budget, the seed, the PV cap, the
disagreement settings, the opening set with the worker's slice of it (`openings_count`,
`openings_in_slice`, `worker_index`, `worker_count`) **and the worker's campaign-wide identity**
(`global_worker_offset`, `global_worker_index`, `global_worker_count`), and the record and game counts, the
last of these including `games_abandoned`, `games_decided_at_first_label` and `games_without_records` —
which is what makes a net's data lineage auditable and a shard's own health readable.

**Measured throughput**, one worker on one core of an i9-13900HX (~2.0M nps) with net v12 and
`nnue/openings.epd`, every other option at its default, each run about 4½ minutes:

| configuration | records/s/core | records/game | bytes/record |
|---|---|---|---|
| single-stage (`--label-interval 0`), `--play-depth 8`, `--pv-plies 0` | 41.6 | 116 | 66.5 |
| `--play-depth 8 --label-interval 4 --label-nodes 220000 --pv-plies 0` | 6.45 | 33 | 65.4 |
| the same with `--pv-plies 4` | 24.6 | 129 | 65.1 |
| `--play-nodes 12000 --label-interval 4 --label-nodes 220000 --pv-plies 4` | 30.7 | 75 | 70.4 |

The deep label costs what it looks like it costs: a 220k-node label on every fourth position drops the
rate 6.5×, from 41.6 records/s to 6.45. **The PV harvest pays that back almost four times over** — the
same run with `--pv-plies 4` yields 3.82× the records, because one deep search then labels up to five
positions instead of one. Playing the game cheaply (the last row) recovers the rest. At ~70 bytes a
record, a 500M-position pool is ~35 GB of text and ~16 GB once converted to bulletformat.

**Size a campaign from a loaded machine, not from one worker.** The last configuration run across 24
workers on the same machine produced 74,960 records in 260 s — **288 records/s in total, but only 12.0
records/s per core**, 2.55× less than the 30.7 one worker managed with the machine to itself. This CPU has
8 performance and 16 efficiency cores, so most of those workers are on the slow ones and the fast ones
lose their turbo headroom; a machine of uniform cores will land between the two figures. On that measured
loaded rate a **500M-position pool needs about 13 days on 38 cores** (5 days at the single-worker rate —
treat that as the optimistic bound, not the plan).

**Openings matter more than they look.** With no `--openings` the generator starts each game from the
standard position plus `--random-plies` (8) uniformly random plies, which often reaches a position one
side is already lost in; the resign adjudication then ends the game four plies later and the whole game
yields about one record. Supplying the opening set raises that by two orders of magnitude.

#### Data diversity (what a campaign is actually limited by)

**The engine is deterministic at a fixed budget**, single-threaded, with its book off, so across processes a
self-play game is decided by the position it starts from — which makes the *opening set*, not the game count,
the limit on how many distinct games a campaign can produce. Measured: two different seeds, one opening and
`--random-plies 0` write **byte-identical shards**, because with no random plies the seed never enters the
game. Within one process it is not quite that stark — the transposition table carries over between games, so
a worker starting twice from the same opening plays a near-copy rather than a copy (ten games from one
opening in one process still wrote 676 records with the dedup on, not fifty and then nothing) — but the lines
overlap heavily and it is not a mechanism to rely on. And since `gen_data` deduplicates positions **per
shard**, every shard looks healthy from the outside while the assembled pool is mostly copies: shard sizes
never show this. A 500M-position campaign on the 306-position book-derived set would therefore have been a
few hundred games' worth of genuinely distinct play, replicated thousands of times.

Three things compose, and all three keep a shard reproducible from its seed (no clock, no entropy):

1. **A large opening pool of our own play** — `--openings nnue/openings-selfplay.epd` (30,000 positions, and
   `gen_openings` writes millions in seconds: *The data campaign's openings* below).
2. **A couple of random plies on top of each opening** — `--random-plies`, which defaults to **2** when
   `--openings` is given (it was 0, and is still 8 from the start position). Each opening becomes the ~900
   two-ply legal lines out of it, which is what makes the distinct-game count unbounded rather than merely
   large.
3. **A disjoint slice of the pool per worker, across the whole campaign** — `--worker-index` /
   `--worker-count` for this machine's fan and `--global-worker-offset` / `--global-worker-count` for the
   campaign, all four of which `run_gendata.sh` passes: the worker whose **global** identity is *G* takes
   the openings whose index is congruent to *G* modulo the **campaign's** worker count. **No two workers
   anywhere can start a game from the same position**, so a duplicate game is impossible by construction
   rather than merely unlikely. Within a worker the slice is handed out in a seeded shuffle and reshuffled
   each pass — without replacement, rather than the uniform draws with replacement that start repeating
   after about the square root of the pool (20 games into a 306-position set).

   **Modulo the machine's own worker count, this would not hold across machines**, and that was a real hole
   until 2026-10-26: a box running 24 workers and a box running 12 both have a worker whose slice contains
   index 0, so their slices overlap and only their differing seeds keep the two workers' games apart —
   "unlikely", not "impossible by construction", which is not a promise worth a week of rented CPU. The
   global identity closes it. `gen_data` refuses an offset that overlaps or overruns the campaign's fan
   before it plays a game, and `check_duplicates.sh` audits the identities in the assembled pool's `.meta`
   sidecars afterwards — the second check matters because the unique fraction does **not** catch this: a
   deliberately colliding six-shard pool (two machines both defaulting to offset 0) still measured
   **1.0000 unique**, while the identity audit named both colliding pairs at once.

**Measured, `--random-plies` on top of the committed pool.** 480 games per setting (8 workers × 60 games,
each worker on its own slice, the same openings and seeds for every setting), `--play-nodes 20000
--label-nodes 220000 --label-interval 4 --pv-plies 4`, all 32 processes resident at once so every setting saw
the same loaded machine (i9-13900HX):

| `--random-plies` | records/game | records/s/core | decided at the first label | games with no records |
|---|---|---|---|---|
| 0 | 83.8 | 11.70 | 31/480 (6.5 %) | 4/480 |
| **2 — the default** | **70.8** | **11.10** | **59/480 (12.3 %)** | 11/480 |
| 3 | 63.8 | 10.43 | 72/480 (15.0 %) | 19/480 |
| 4 | 61.6 | 10.21 | 66/480 (13.8 %) | 13/480 |

*Decided at the first label* means the game's first searched position was already past the 1500 cp resign
threshold: the random line walked into a position one side has lost, and the game ends a few plies later
having taught little. Each shard counts it in its own `.meta`, with `games_abandoned` and
`games_without_records` beside it, so a campaign reports its own health per shard.

**Why 2 is the default.** Zero is not an option: it caps the campaign at the pool's size, which is the defect
this design exists to remove. Two is the cheapest setting that is not zero — 15 % fewer records a game and
5 % less throughput than zero, and twice as many games decided before they start (12.3 % against 6.5 %), all
of which buys ~900× the distinct starts per opening. Three and four cost 24 % and 27 % of the records a game
and improve nothing measurably; an even count also keeps the random line symmetric, each side moving the same
number of times rather than one of them getting a free move.

**Size the pool, not the random plies.** At 70.8 records a game, a 500M-position pool is about 7.1M games.
Two random plies on the committed 30,000-position pool give ~27M distinct starts — enough that a
development-scale run never repeats one, but only ~4× the games a full campaign plays, so about 12 % of its
games would share a start with another. **Generate a bigger pool for a real campaign** (`build/gen_openings
~/pawnstar_nnue/openings-1m.epd 1000000 1` — 1M positions from 3.83M random lines, measured at 52 s with the
machine fully loaded by a data run, 62 MB, which is why it belongs on a path of its own, not in the repo):
1M openings × ~900 two-ply lines is ~900M distinct starts, under 1 % of 7.1M games sharing one. Raising `--random-plies` to 3 is the alternative if the pool
must stay at 30,000, at the cost in the table.

**Check the pool before training on it** — `tools/check_duplicates.sh <pool.txt | shard_dir>
[min_unique_fraction] [records_per_shard]`:

```
check_duplicates.sh: 16 shard(s), 14 MiB in /…/selfplay
check_duplicates.sh: reading every record (exact)
check_duplicates.sh: records                              225285
check_duplicates.sh: after collapsing weighting copies    170846
check_duplicates.sh: distinct positions                   170846
check_duplicates.sh: unique fraction                      1.0000 (0.00 % duplicate)
check_duplicates.sh: games played (from the .meta)        3039 -> about 3039 distinct
check_duplicates.sh: OK (gate 0.80)
```

It counts records, records after collapsing the adjacent copies the disagreement weighting writes on purpose
(`--disagreement-copies`), and distinct positions, and fails below the `min_unique_fraction` gate (0.80 by
default; `0` reports only). A pool larger than 2 GB is sampled at 200,000 records per shard rather than read
whole — a duplicate here is a repeated *game*, which is a contiguous run of records in a shard, so a
contiguous block from every shard keeps whole games intact and still sees every worker; that sampled fraction
is an upper bound on how unique the whole pool is, which is the safe direction for a gate.
`run_gendata.sh` reports it when a run finishes, and **`train_pipeline.sh` runs it as a gate** before
converting a shard directory (`ALLOW_DUPLICATE_POOL=1` to train anyway, `MIN_UNIQUE_FRACTION` to move the
gate).

For a shard directory it **also audits the campaign identities** in the `.meta` sidecars: no two shards may
claim the same `global_worker_index`, every shard must name the same `global_worker_count`, and every shard
must name the same opening pool and `openings_count`. That is the check that catches two machines given
overlapping offsets, and it fails the script on its own — see the caveat below for why the unique fraction
cannot be relied on to notice.

**What the 0.80 gate is and is not.** It catches gross replication — a campaign whose workers are replaying
each other's games, or one exhausting a pool far too small for it. It does **not** catch subtle degeneracy:
a pool of millions of near-identical games from near-identical openings would pass it comfortably. And
**the unique fraction will fall below 1.0 at scale for entirely legitimate reasons**: distinct games
transpose into one another, and the more games a pool holds the more often two of them pass through the same
position. So the number to watch is **its trend as the pool grows**, not a single reading — measure it at a
few points through the campaign and expect a slow, shallow decline from 1.0000. A step change, or a decline
that keeps steepening, is the signal; one reading of 0.97 is not.

**The before and after, measured at the same size.** Two 16-worker runs of 190 games each (3,040 games,
~30 minutes apiece), side by side on the same machine, same budgets, differing only in the diversity
settings:

| settings | records | records/game | after collapsing | distinct positions | unique | implied distinct games |
|---|---|---|---|---|---|---|
| the book-derived set, `--random-plies 0`, no partition | 320,833 | 105.5 | 259,117 | 243,930 | **0.9414** | ~2,861 / 3,040 |
| these defaults: the 30,000 self-play pool, `--random-plies 2`, partitioned | 225,285 | 74.1 | 170,846 | 170,846 | **1.0000** | 3,039 / 3,039 |

**Not one duplicate position in 170,846** at the new defaults: 16 workers, disjoint opening slices, every
game distinct. The first row lost 5.86 % of its records to duplication **after only 3,040 games** — a
ten-fold reuse of a 306-position set — and that figure was still climbing (4.08 % a third of the way into the
same run), because the degeneracy grows as the pool is exhausted. A 500M campaign is ~7.1M games, which is
23,000 plays of each of those 306 openings: 5.86 % is the start of that curve, not its plateau.

**What it costs: 30 % of the records a game yields** (105.5 → 74.1), and it is worth naming where that goes,
because only half of it is the random plies. The book's openings are sound — **none of the 3,040 games from
them was already decided at its first label** — while 6.5 % of games from the randomly generated pool are
(9.5 % in this run, with the two random plies on top), since the balance filter is a static evaluation and a
position can be level by material and lost to a tactic. Those games still produce records, just fewer of
them. Duplicate records are worth nothing to train on, so 74 distinct records a game beats 105 records a
game that are 6 % — and climbing — copies; the obvious next improvement is a cheap tactical filter in
`gen_openings` (a shallow search, or rejecting a position whose mover has a winning capture by static
exchange evaluation) rather than more random plies.

### Training

`train_pipeline.sh <shuffled.data | data_dir> <out_net.bin> [end_superbatch=40]` auto-detects its input
from the first argument:

1. ensures bullet is set up, builds `bullet-utils` and the trainer (`$BULLET_FEATURES`, default
   `--features cuda`);
2. **if the argument is a `.data` file** (already bulletformat — an assembled, shuffled pool, the way
   every shipped net has been trained), it is used directly and steps 3–4 are skipped; **if it is a
   directory**, the text shards `data_*.txt` + `seed*.txt` are combined → `all.txt`,
   `bullet-utils convert --from text` → `all.data` (bullet `ChessBoard`, 32 bytes/record), then
   `bullet-utils shuffle` → `shuffled.data` (correlated data needs shuffling);
3. trains, computing `batches_per_superbatch = positions / 16384` so a superbatch ≈ one epoch;
4. copies `<work_dir>/checkpoints/pawnstar-<end_superbatch>/quantised.bin` → `out_net.bin`, where
   `<work_dir>` is the data directory (text mode) or the `.data` file's directory (bulletformat mode).

Trainer hyperparameters ([tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs)): `HIDDEN_SIZE=1024`,
AdamW, `batch_size=16384`, loss `sigmoid(out)` squared-error against
`target = wdl·result + (1-wdl)·sigmoid(score/SCALE)` with `ConstantWDL=0.5`, `StepLR{start=0.001,
gamma=0.3, step=SBS/3}`, `eval_scale=400`. Overridable via env: `PAWNSTAR_DATA`, `PAWNSTAR_BPS`,
`PAWNSTAR_SBS`, `PAWNSTAR_OUT`, `PAWNSTAR_LR_STEP`, `PAWNSTAR_RESUME`, `PAWNSTAR_START_SB`.
**The schedule is checked before the run
starts.** `StepLR` applies drop N from superbatch `N·step + 1`, so `step = SBS/3` puts the third
drop at `SBS + 1` whenever SBS divides by 3, where it never fires — which is what happened to
every net trained before 2026-10-29 (0, 1 or 2 superbatches at the lowest rate; the 819-superbatch
run got none, and the missing stage measured 0.88% of held-out loss). The trainer prints the
schedule it will follow, refuses to start one whose last drop cannot apply, reports the tail
length, and prints `LR CHECK: superbatch <n> -> lr <rate>` every superbatch — the only record of
the rate actually applied, since bullet logs the scheduler's description instead. Pin
`PAWNSTAR_LR_STEP` to repair a truncated schedule or to hold the lowest rate across a resume.
**A run is resumed through this same trainer**, not a copy of it: `PAWNSTAR_RESUME` names a
checkpoint directory (`<out>/pawnstar-<n>`, which holds the AdamW optimiser state — not a
quantised `.bin`, which carries none) and `PAWNSTAR_START_SB` the **absolute** superbatch to
re-enter the schedule at; both the learning rate and the data position are indexed by that
number, so those two are the whole of what a resume needs. The resume controls are validated
before the device, the checkpoint or the data file is touched, and the schedule check above runs
for a resume too — re-entering mid-schedule is exactly when its shape is easiest to break, since
raising `PAWNSTAR_SBS` to continue a run moves the derived step's drops rather than extending
past them.
GPU training reports the device (e.g. `Training on NVIDIA GeForce RTX
4070`) and runs at ~2.4M positions/sec on that card for this 1024-wide net (~0.34M on an older GTX
1050 Ti; see §7).

### Openings

The SPRT needs an EPD opening book (`OPENINGS=…`) of varied, roughly balanced early-game positions so
matches don't all start from the same spot. **[openings.epd](openings.epd) is committed**: 306 positions
built by `tools/make_openings.sh` from `pawnstar.book`, which is the **public-domain** TSCP book, so the
set carries no licence restriction of its own and no third party's data. Each book line is replayed from
the start position and every position it reaches between ply 4 and ply 24 is kept when the engine's static
evaluation of it is within 125 cp of its evaluation of the start position (which this net puts at +158 cp,
so a zero-centred filter would keep only the openings it mildly dislikes); then duplicates are dropped.

Regenerate it with `tools/make_openings.sh [out.epd] [min_ply] [max_ply] [margin_cp]` — worth doing when
the net changes, since the balance filter is the net's own judgement. **Its size is capped by the source:**
the TSCP book's 385 lines overlap heavily and hold only about 420 distinct positions in that window, so an
SPRT with more rounds than openings replays some of them (`order=random`), which is fine but worth knowing.
Any standard EPD set works instead. The file carries a `#` provenance header; `run_sprt.sh` strips comment
lines into a scratch copy before handing the file to fastchess, whose EPD reader would try to parse one as
a position.

#### The data campaign's openings (`gen_openings`) — a pool, not a book

That 420-position ceiling is fine for a match and fatal for a data campaign (see *Data diversity* above), so
the campaign's openings come from **our own play** instead. **[openings-selfplay.epd](openings-selfplay.epd)
is committed**: 30,000 positions written by `build/gen_openings`, which plays short randomised lines from the
start position and keeps the position a line ends at when it is not in check and the net's static evaluation
of it is within 125 cp of its evaluation of the start position — the same balance filter as the book set, so
a game from one of these is decided by the engines and not by the opening. Duplicates are dropped on the
Zobrist key, and **one position is kept per line played**, so no two openings in the file share a line (the
book script harvests a whole window from each line, which emits prefixes of one another).

```bash
make tools                                                  # -> build/gen_openings
build/gen_openings nnue/openings-selfplay.epd 30000 1        # the committed set, 0.4 s, 1.9 MB
build/gen_openings ~/pawnstar_nnue/openings-1m.epd 1000000 1 # a campaign's own pool, under a minute, 62 MB
```

`gen_openings <out.epd> <num_positions> <seed> [--min-ply 8] [--max-ply 12] [--margin-cp 125] [--max-lines N]
[--net PATH] [--progress N]`. **The count is a parameter, not a property of the source** — the committed
30,000 (1.9 MB) is a convenience, and a campaign should generate a bigger pool to a path of its own rather
than commit one. The acceptance rate is about one line in four (114,419 lines for the committed 30,000), and
the generator is deterministic: the same seed, options and net reproduce the file byte for byte, which is the
audit trail for a set with no external source. The output carries the same kind of `#` provenance header as
the book set, stating how it was produced; the engine version is stamped into it.

The positions are not book lines and do not look like them — that is the point for training data, where a
wider distribution of labelled positions is worth more than opening theory, and the balance filter is what
keeps them playable. For an SPRT, keep using `openings.epd`.

### Validate inference (cross-check)

The engine's inference must reproduce the trainer's. Build the reference evaluator, sample some FENs
(include both side-to-move colours), and compare:

```bash
# build the bullet-side reference evaluator (CPU is fine; add --features cuda to reuse the GPU build)
(cd ~/pawnstar_nnue/bullet/crates/bullet_lib && cargo build --release --example pawnstar_eval)
EVAL=~/pawnstar_nnue/bullet/target/release/examples/pawnstar_eval

# get a FEN list (reuse the checked-in reference FENs; or supply your own, one FEN per line, both colours)
cut -d'|' -f1 test/nnue_reference.txt > fens.txt
"$EVAL" net.bin fens.txt > ref.txt
./build/test_nnue net.bin ref.txt        # EvaluateExact vs ref <=2 cp; int8 Evaluate vs EvaluateExact <=40 cp
```

`pawnstar_eval` uses bullet's own feature extraction (`ChessBucketsMirrored`), its own output-head
selection (`MaterialCount<8>`) and the same SCReLU inference, so a match confirms our feature indexing,
king bucketing, mirroring, perspective/orientation, head selection, SCReLU and dequantisation are all
correct. It must be built at the same width, feature set and head count as the engine, or the cross-check
fails by design.

The repo checks in `test/nnue_reference.txt` — 250 reference evals for the shipped `pawnstar-v20.bin` —
and `make check` runs `test_nnue nnue/pawnstar-v20.bin test/nnue_reference.txt` automatically (current
engine: max |diff| 0 cp). The reference's first field is the FEN, so regenerate it after shipping a *new*
net with: `cut -d'|' -f1 test/nnue_reference.txt > fens.txt && "$EVAL" <raw net> fens.txt >
test/nnue_reference.txt` - note the **raw** net, since `pawnstar_eval` is not header-aware. With no
arguments `test_nnue` is a no-op, so `make check` stays green if the net/reference are absent.

A second test, `test_nnue_incremental <net.bin>`, plays random move sequences through a `SearchState`
and asserts the incrementally-maintained accumulator evaluates **bit-identically** to a full refresh at
every node (and again after every undo, to catch reverse-update bugs). With no argument it is a no-op,
so `make check` stays green; run it with a net (e.g. `./build/test_nnue_incremental nnue/pawnstar-v20.bin`)
after any change to the accumulator, feature indexing, or make/undo.

`verify_net.sh <net.bin>` bundles both checks into one gate: it regenerates the reference evals for
*that* net (reusing the FENs from `test/nnue_reference.txt`) with `pawnstar_eval`, then runs `test_nnue`
and `test_nnue_incremental`. `run_experiment.sh` runs it between training and the SPRT — a net that
fails it has a width/format mismatch, so any match result would be meaningless. Run it standalone after
training a net outside the one-shot flow.

### Strength testing (the verdict)

NNUE strength is measured by game play. NNUE is the only evaluator, so an SPRT is always **net-vs-net**.
`run_sprt.sh <net.bin> <openings.epd> [rounds=500] [depth=8]` runs:

- the candidate as `<net.bin>` against a **required** `BASELINE_NET=<other.bin>` opponent — the
  architecture program's test (e.g. v8 vs v7). Net selection is purely via the `EvalFile` option;
- the **same** `build/pawnstar` binary for both engines, differing only by `EvalFile`, with
  `PAWNSTAR_THREADS=1`. Different net *widths* need different builds (`kHiddenSize` is compile-time), so a
  cross-width SPRT points `BIN_A`/`BIN_B` at two separately-built binaries;
- **fixed depth** (default 8) — pawnstar has no `go nodes`, and fixed depth isolates eval *quality* from
  per-node speed. To measure the equal-*time* effect (incremental vs full-refresh, or a wider net's speed
  cost) set `TC=8+0.08` for a time-control match instead;
- **win/draw adjudication** (`-resign movecount=3 score=600`, `-draw movenumber=40 movecount=8
  score=20`) to keep games short and decisive (recommended, not required — the old fixed game-history
  cap that crashed long games is gone; the history is a dynamic `std::vector` now);
- `-sprt elo0=$ELO0 elo1=$ELO1 alpha=0.05 beta=0.05` (default bounds 0 / 10) and concurrency `nproc`.

For an **absolute** rating (rather than a net-vs-net delta), `rate.sh <name:rating:cmd> ...` plays the
engine against one or more externally-rated reference engines (e.g. the CCRL engines bundled with Arena —
which you supply, as they are not in the repo) and reports `anchor_rating + measured_diff` per anchor plus
the mean. It anchors only as well as those opponents and the chosen TC, so treat the result as a ballpark.

## 7. Shipped nets

The repo ships one reference net, **[pawnstar-v20.bin](pawnstar-v20.bin)** (1024-wide, 8 horizontally
mirrored king buckets by file × board-half, 8 material output heads; stamped file **12,617,824 bytes
≈ 12.6 MB**, SHA-256 `c8a29b328849cd1ea60c65f5d3dea84f585f404037ec026c44a94e1174b1cc34`) — loaded at
startup as the engine's only evaluator (resolved relative to the working directory). To use a different
net at runtime:

```
setoption name EvalFile value /path/to/other-net.bin
```

**`pawnstar-v20.bin`** is trained **entirely on Leela Chess Zero self-play data** — a
6,143,698,748-position pool, 62.83% lc0 `test91` and 37.17% `test60`, extracted by `tools/lc0_extract`
(§6, Data). It is the shared Reckless NNUE shape: a 1024-wide transformer, **8 horizontally mirrored
king buckets selected by king file × board-half** (`kKingBucketMap`, §1) and **8 material output heads**
selected by `(piece_count - 2) / 4`. It beats the v12 net it retires by **+9.99 Elo at 8+0.08** (8,000
games, 95% +4.33..+15.66) and **+1.13 at 12+0.12** (12,000 games, 95% −3.17..+5.43, Birge-scaled
±6.03) — a gain at fast time controls and level at slower ones. Two variables moved together there, the
training data and the shape, so that pair of numbers is the net-to-net verdict and not a measurement of
either lever on its own; the shape's two additions were measured separately in zenith at fixed depth,
**+14.3 Elo** for mirroring and **+14.7** for the output heads.

Its held-out loss is **0.012182 ± 0.000003** on the frozen 100,141,613-position validation set at lambda
0.0, which is **0.879%** better than the same run 51 superbatches earlier — the largest recipe gain this
project has measured, and the one that came from letting the learning-rate schedule's third drop actually
apply (§6, Training). Held-out loss is a training diagnostic and not a strength gate, so the games above
are the verdict; the loss is here because it is what the run was stopped on.

**Lineage — what moved the needle (all SPRT-measured):**

| step | change | result |
|---|---|---|
| v1 (256) | Pawnstar's own **HCE self-play** (~3.6M) | **loses** to HCE (−67 Elo; a 1.0M predecessor was −151) |
| v2 (256) | **the PlentyChess bulletformat dataset, by Yoshie2000** (~60M) | **beats** HCE ≈ +330 Elo — *label quality*, not quantity, was the lever |
| v3 (256) | ~12× more of the same data (~750M) | **no gain** over v2 (+9.5 ± 13.6, inconclusive) — 256 net is *capacity-limited* |
| v4 (512) | **double the width**, same 60M as v2 | **+55 ± 20 Elo fixed-depth, +71 ± 25 at time control** over v2 — once data saturates, *net size* is the lever (shipped, now retired) |
| v5 (1024) | **double width again**, same 60M | **rejected**: +2 ± 16 fixed-depth (dead even), **−74 ± 25 on the clock** — 60M *saturates* the 512 net; width only pays off with more data. Branch `bignet-1024`, not shipped |
| kb (512 + 4 king buckets) | king buckets, **same 60M** | **flat** (~−8 fixed depth) — the higher-capacity feature set is *data-starved* on 60M |
| v6 (512 + 4 king buckets) | king buckets, **~818M** | **+47 ± 18 fixed-depth, +17 ± 12 on the clock** over v4 — *more data* unlocked the king buckets (shipped, later retired). Needed two engine speed fixes (the eval cache and a single-pass `Update`) to turn an initial −74 clock result positive |
| v7 (512 + 4 king buckets) | **~2.31B** (v6's 818M + 4 more big shards), same arch, SBS 40→120 (~3 epochs) | **+29.96 ± 14.05 fixed-depth, +20.73 ± 12.73 on the clock** over v6 — *still more data*; same arch/speed, so the eval-quality gain carries straight to the clock (later retired) |
| **v8 (1024, no king buckets)** | **double the width, drop king buckets**, same 2.31B as v7 | **+31.9 ± 16.6 Elo at 40/20** over v7 (LLR 2.95, H1; 972 games) — at the full data scale width beats buckets, **at half the file size** (fewer feature rows, wider hidden). Reverses v5's 60M result now that data no longer saturates 1024. (later retired) |
| v9 (1024 + 4 king buckets, file-pair) | add king buckets to the v8 arch, same ~2.31B scale/schedule | **+11.4 ± 6.75 Elo at 8+0.08** over v8 (LLR 2.96, H1; 5520 games). Holding width fixed, buckets win at scale — the gain shrinks with data (+29 at 750M → +11 at 2.31B) but stays positive; the v8 'width beats buckets' call was width-vs-buckets confounded. **File-pair bucketing beats quadrant/rank-based** (king safety is file-dependent; back-rank kings use all 4 file banks). 4× the FT rows (1.58→6.3 MB). (later retired) |
| v10 (1024 + 4 king buckets, file-pair) | **more data**: scale the pool 2.31B→3.82B (four more PlentyChess shards), same v9 arch, SBS 120→190 (~3 epochs) | **+16.3 ± 10.8 Elo at 8+0.08** over v9 (LLR 2.95, H1; 1944 games, 52.3%). More data again the single biggest lever. (later retired) |
| **v11 (1024 + 4 king buckets, file-pair)** | **more data**: scale the pool 3.82B→6.05B (six more PlentyChess shards), same v10 arch, SBS 190→300 (~3 epochs) | **+9.28 ± 6.58 Elo at 8+0.08** over v10 (LOS 99.7%, LLR 2.48; 6102 games, 51.3%). More data again the single biggest lever — the per-step gain is shrinking (+16 for +1.5B → +9 for +2.2B) but stays clearly positive; v11's 6.05B was still a fraction of the pool available then. (later retired) |
| **v12 (1024 + 8 king buckets, file-pair × board-half)** | **double the king buckets 4→8** at fixed data (same ~6B pool as v11): bank = `file/2`, plus 4 when the king is in its own advanced half (ranks 5–8, per-perspective oriented); SBS=300, final loss 0.0226 (v11 0.0228) | **+17.29 ± 8.68 Elo at 8+0.08** (3520 games, H1, LLR 2.96) and **+10.97 ± 6.57 at 12+0.12** (5610 games, H1, LLR 2.95) over v11 — single variable vs v11 = bucket count. **King buckets are NOT at diminishing returns**: the 4→8 step (+17/+11) is *bigger* than the 0→4 step (+11.4), the rank split capturing real own-half-vs-advanced king-safety signal. 8× the FT rows (1.58→12.6 MB, so the owning `Game` is heap-allocated). (later retired) |
| **v20 (1024 + 8 MIRRORED king buckets, file × board-half, + 8 material output heads)** | **new training data and the shared shape, together**: a 6,143,698,748-position pool built entirely from Leela Chess Zero self-play (62.83% `test91`, 37.17% `test60`) in place of the retired dataset; horizontal king mirroring on, the bucket map per-*file* rather than per file-*pair*, and 8 output heads; 8 epochs at `PAWNSTAR_BPS=3662`, `PAWNSTAR_SBS=819`, `PAWNSTAR_WDL=0.0`, then 51 more superbatches with `PAWNSTAR_LR_STEP=273` so the schedule's third drop applied (held-out loss 0.012182, 0.879% better than superbatch 819) | **+9.99 Elo at 8+0.08** (8,000 games, 95% +4.33..+15.66) and **+1.13 at 12+0.12** (12,000 games, 95% −3.17..+5.43, Birge-scaled ±6.03) over v12 — a gain at fast time controls, level at slower ones. **Two variables at once** (data and shape), so this measures the net, not either lever; the shape's parts were measured separately in zenith at +14.3 (mirroring) and +14.7 (output heads) at fixed depth. **shipped** |

The data behind every net from v2 to v12 in that table was the **PlentyChess bulletformat dataset**, and
the recipe was: `bullet-utils validate` each shard first (a corrupt shard trains silently wrong), `cat` a
few clean ones together, `shuffle`, then train per §6 — the text-`convert` step skipped, the shards being
bulletformat already. **That dataset is no longer used and is not to be re-downloaded**; see *Provenance
of the earlier networks*, below. A new net's pool comes from the three sources in `tools/` instead —
`gen_data`, `lichess_extract`, `lc0_extract` (§6, Data) — which write text shards, so
`train_pipeline.sh`'s directory mode concatenates, converts and shuffles them itself and there is no
`validate` step to run: that one is for shards that are bulletformat already. Shuffling before training
still matters just as much.

The eval is also fast on the clock: the **incremental accumulator** (~+80 Elo equal-time vs full
refresh), **lazy/deferred accumulator updates** (skip the update for nodes that cut off before evaluating —
**+20.66 ± 9.04 Elo at 8+0.08**, +13.6% nps), **AVX2 SIMD kernels** (~+180 Elo equal-time vs scalar), the
**int8 output layer** (+31.8 Elo), and the **eval cache** mean NNUE wins on time as well as depth. The
shipped net's 8-bucket table is 8× the feature rows of a single-bank net (12.6 MB vs 1.58 MB) and
slightly heavier per eval (and needs the owning `Game` heap-allocated, since the inline weight array
would overflow the default stack), but every shipping SPRT (v8→v9→v10→v11→v12→v20) was at a time
control, so that cost is already priced in. **The lessons: more data was the lever** through the earlier
steps, **king buckets are not saturated** (the 4→8 double won at fixed data), and — measured on the v20
pool — **the learning-rate schedule is worth more than any data change measured here**, the third drop
being worth about 8× the whole data campaign for about 22 GPU-minutes (§6, Training).
Next levers: more epochs over the data already in hand (worth ~15 Elo, measured); more lc0 data; a finer
bucket map; or a wider transformer still.

**Rejected architecture experiments (not in the shipped lineage):**
- **Head depth — a hidden layer in the head (`concat 2048 → 16 → 1`, SCReLU):** REJECTED, **−11.3 ± 12.9 Elo
  at fixed depth 8, H0 (1908 games)**. Single-variable A/B — a control (current `2048 → 1` head) and the
  candidate trained on the *same* shuffled 200M set + same schedule, then head-to-head SPRT. Fixed depth is
  pure eval quality, and it's already negative there, so no time-control follow-up. The FT stayed int16 +
  incremental and the new head ran in **float** (f32-saved head weights — negligible vs the FT at fixed
  depth, and it makes the engine match the trainer trivially). `NetHeader` gained a `hidden2_size_` field
  (format_version 2). Branch `nnue-hidden-layer` (pushed, not merged); trainer `tools/bullet/pawnstar_h.rs`,
  reference `tools/bullet/pawnstar_eval_h.rs`. **Caveat:** reused the 2-layer LR schedule/epochs — a deeper
  head may want tuned hyperparameters/more data; this rejects the same-recipe hidden layer, not all heads.
  **Gotcha:** bullet stores affine weights **input-major** (`w[input*out + output]`, like the feature
  transformer); the `O=1` output layer hides this, so reading the `2048→16` layer output-major yields a dead
  near-constant head — and a 0 cp `test_nnue` cross-check passes while broken (engine and reference misread
  identically), so **sanity-check that evals track material (up-a-queen → ~+1200) before trusting a new arch.**
- **Output buckets — 8 piece-count banks (`concat 2048 → 8`, `select` by bullet `MaterialCount<8>`,
  `bucket = (popcount(occ) − 2) / 4`):** REJECTED at scale. Single-variable A/B (control = v11 arch, candidate =
  v11 + buckets, same pool + recipe). Weak **+6.1 ± 9.5 fixed-depth at 500M (H0, inconclusive)**, but at **6B
  (v11's ~6.36B pool, SBS=300) it regresses at both time controls — −22.96 ± 15.71 @ 8+0.08 and −14.70 ± 13.27
  @ 12+0.12 (both H0)**. The slower TC is less negative (−23 → −15, TC-dependence: the larger output table's
  per-move speed cost weighs less) but never positive. Causes: *output*-bucket gains shrink with data (note this
  is the output bucket; the v11→v12 *king*-bucket double instead grew the gain at fixed data — §7 lineage),
  plus a small TC speed penalty (the int8 output table is 8× larger, 16 KB vs 2 KB; the
  500M test was fixed-depth so never paid it). FT unchanged; the output is saved **bucket-major** (bullet
  `.transpose()`), so each bank's 2048-weight row is contiguous and the int8/int16 dot kernels are reused on the
  selected bank — no float path. `NetHeader` gained an `output_buckets_` field (format_version 2). Branch
  `nnue-output-buckets` (not merged); trainer `tools/bullet/pawnstar_ob.rs`, reference `pawnstar_eval_ob.rs`.

### Provenance of the earlier networks

Pawnstar networks up to and including **v12** were trained on self-play data generated by
**PlentyChess**, by Yoshie2000 — approximately 6 billion positions. That data is no longer used:
networks from **v20 (release 2.0.0)** onward are trained entirely on Leela Chess Zero self-play data.

The attribution is kept here deliberately. Those networks were measurably stronger for having been
trained on that data, and the record should show where they came from.

### Recreating `pawnstar-v20.bin` (step by step)

`pawnstar-v20.bin` is a **1024-wide net with 8 horizontally mirrored king buckets (file × board-half) and
8 material output heads**, trained **entirely on Leela Chess Zero self-play data**: a
**6,143,698,748-position** pool (196,598,359,936 bytes of 32-byte `bullet::ChessBoard` records), 62.83%
lc0 `test91` and 37.17% `test60`, extracted with `tools/lc0_extract`. GPU training is nondeterministic, so
this reproduces a *functionally equivalent* net, not a byte-identical one.

It was trained in two passes, and the second one is the point: eight epochs to superbatch 819, then 51
more superbatches with `PAWNSTAR_LR_STEP` pinned so that the learning-rate schedule's **third** drop
actually applied. `StepLR` applies drop N from superbatch `N·step + 1`, and the default
`step = PAWNSTAR_SBS / 3` puts the third at `PAWNSTAR_SBS + 1` — one superbatch past the end — so no run
before 2026-10-29 ever reached it. That missing stage is worth **0.879% of held-out loss**, more than
every data change this project has measured (§6, Training). The trainer now refuses a schedule whose last
drop cannot apply, so this trap costs one failed startup rather than a run.

**The defining requirement: the architecture (especially the king-bucket map) must match everywhere.**
`kHiddenSize=1024`, `kNumKingBuckets=8`, `kNumOutputBuckets=8`, mirroring on, and the **32-entry
half-board `kKingBucketMapHalf`** in [src/nnue.h](../src/nnue.h) must be byte-identical to `HIDDEN_SIZE`,
`OUTPUT_BUCKETS` and `KING_BUCKETS_HALF` in [tools/bullet/pawnstar.rs](../tools/bullet/pawnstar.rs) and
[tools/bullet/pawnstar_eval.rs](../tools/bullet/pawnstar_eval.rs). `test_primitives` reads the trainer's
array out of the `.rs` file as text and compares all 32 entries with the engine's, so a drift fails a unit
test rather than a training run. The repo is already on this arch, so the checked-in files match; the map
is:

```
// KING_BUCKETS_HALF / kKingBucketMapHalf, indexed rank * 4 + min(file, 7 - file). Mirroring has already
// folded files e-h onto d-a, so only four distinct files are left to bucket: bank = mirrored file for the
// king's own half (ranks 1-4), +4 (banks 4-7) once the king is advanced into ranks 5-8. The engine
// expands these 32 to the 64 it indexes with bullet's own arithmetic,
// expanded[sq] = half[(sq / 8) * 4 + [0,1,2,3,3,2,1,0][sq % 8]].
0, 1, 2, 3,   // ranks 1-4 (king's own half)
0, 1, 2, 3,
0, 1, 2, 3,
0, 1, 2, 3,
4, 5, 6, 7,   // ranks 5-8 (king advanced)
4, 5, 6, 7,
4, 5, 6, 7,
4, 5, 6, 7,
```

```bash
cd /path/to/pawnstar                 # repo root (so ./build and nnue/ paths resolve)
make                                 # build the engine (the default shape is this net's)
make tools                           # build build/stamp_net
make tests                           # build test_nnue / test_nnue_incremental / test_bratko_kopec_nnue

# 0. One-time: install the bullet trainer (pinned commit) + examples, build utils + trainer + eval FRESH.
#    GOTCHA: setup_bullet.sh copies tools/bullet/*.rs into the bullet checkout; if you ever edit the repo
#    .rs you MUST re-copy + rebuild before training, or a stale binary trains the wrong arch (cost a run).
#    GOTCHA: omitting either `-p bullet_lib` or `--features cuda` silently yields a MockGPU binary that
#    prints the right architecture line and then panics at the first gradient calculation. Check the
#    device line ("Training on NVIDIA ...", not "Training on MockGPU"), not the byte size.
tools/setup_bullet.sh
BULLET=~/pawnstar_nnue/bullet
export CUDA_PATH=/usr/local/cuda-13.3 PATH="$CUDA_PATH/bin:$PATH" LD_LIBRARY_PATH="$CUDA_PATH/lib64"
( cd "$BULLET/crates/utils"      && cargo build --release --bin bullet-utils )
( cd "$BULLET/crates/bullet_lib" && cargo build --release -p bullet_lib --features cuda \
                                      --example pawnstar --example pawnstar_eval )
UTILS="$BULLET/target/release/bullet-utils"
EVAL="$BULLET/target/release/examples/pawnstar_eval"
mkdir -p ~/pawnstar_nnue/v20 && cd ~/pawnstar_nnue/v20

# 1. Assemble the pool from Leela Chess Zero self-play. lc0_extract reads the published run archives and
#    writes this repo's text shard format; reserve a holdout as WHOLE TARS first (§6, Training) and keep
#    those tars out of the index the pool build reads, or the holdout is silently inside the pool.
#    The v20 pool was 62.83% test91 and 37.17% test60, 6,143,698,748 positions in all.
#    (Text shards go through train_pipeline.sh's directory mode instead of steps 2-3; the flow below is
#    the already-bulletformat one, which is what the assembled pool is by this point.)

# 2. Validate each shard (size % 32 == 0, no invalid records). A corrupt shard trains silently wrong,
#    so ALWAYS validate before training; drop and replace any that fail.
SHARDS=( *.data )
for sh in "${SHARDS[@]}"; do "$UTILS" validate --input "$sh"; done

# 3. Concatenate the shards (flat 32-byte records) and re-shuffle (concatenated shards are correlated).
#    --mem-used-mb: size to free RAM. On a machine with no page file this is a hard allocation, not a
#    hint: 6144 worked on a 200 GB pool where 12288 failed with exit 127 (OOM, not "command not found").
cat "${SHARDS[@]}" > all.data
"$UTILS" shuffle --input all.data --output shuffled.data --mem-used-mb 6144
echo "pool: $(( $(stat -c%s shuffled.data) / 32 )) positions"   # 6143698748 for v20
rm -f all.data                                                   # reclaim disk

# 4. Train. BPS=3662 and SBS=819 => ~8 epochs of the 6.14B pool; WDL=0.0 makes every target the eval
#    sigmoid, which suits lc0's labels. PAWNSTAR_SAVE_RATE=10 checkpoints every ten superbatches, which
#    is what makes step 5 possible. The trainer prints the schedule it will follow, and the learning rate
#    it applies at every superbatch — READ THAT RATE: it is the only reading that proves which stage is
#    in force, and bullet's own preamble prints the scheduler's description instead.
( cd "$BULLET/crates/bullet_lib" && \
  PAWNSTAR_DATA=~/pawnstar_nnue/v20/shuffled.data PAWNSTAR_BPS=3662 PAWNSTAR_SBS=819 PAWNSTAR_WDL=0.0 \
  PAWNSTAR_SAVE_RATE=10 PAWNSTAR_OUT=~/pawnstar_nnue/v20/checkpoints \
  cargo run --release -p bullet_lib --features cuda --example pawnstar )

# 4b. SPOT-CHECK at the first checkpoint: confirm the net is genuinely 8-bank, 8-head before sinking the
#     full run. bullet's RAW quantised.bin at this shape is 12,617,792 bytes (6144 FT rows = 768*8, plus
#     8 heads of 2048 int16 weights and 8 int32 biases, padded to a 64-byte boundary). The RNNUE1 payload
#     a stamped net carries is 12,617,760 -- the same bytes without the padding; see section 2.
python3 -c "import os;s=os.path.getsize('checkpoints/pawnstar-10/quantised.bin');print(s, 'ok' if s==12617792 else 'WRONG SHAPE')"

# 5. Continue from superbatch 819 so the THIRD learning-rate drop applies. PAWNSTAR_LR_STEP pins `step`
#     independently of PAWNSTAR_SBS: raising SBS alone MOVES the schedule rather than extending it (at
#     SBS=900 the derived step is 300, which puts superbatch 820 back on the second drop's rate). With
#     step pinned at 273, every resumed superbatch runs at 0.001 * 0.3^3 = 0.000027 — confirmed from the
#     rate the run printed, not from the configuration. v20 is checkpoint 870: the held-out curve was
#     flat from 850 to 870, so the remaining 30 superbatches would have collected nothing.
( cd "$BULLET/crates/bullet_lib" && \
  PAWNSTAR_DATA=~/pawnstar_nnue/v20/shuffled.data PAWNSTAR_BPS=3662 PAWNSTAR_SBS=900 PAWNSTAR_WDL=0.0 \
  PAWNSTAR_LR_STEP=273 PAWNSTAR_RESUME=~/pawnstar_nnue/v20/checkpoints/pawnstar-819 \
  PAWNSTAR_START_SB=820 PAWNSTAR_SAVE_RATE=10 PAWNSTAR_OUT=~/pawnstar_nnue/v20/checkpoints-resumed \
  cargo run --release -p bullet_lib --features cuda --example pawnstar )

# 6. Stamp the final RAW net into the shipped path (stamp_net embeds this build's arch header, §2).
RAW=~/pawnstar_nnue/v20/checkpoints-resumed/pawnstar-870/quantised.bin
./build/stamp_net "$RAW" /path/to/pawnstar/nnue/pawnstar-v20.bin   # writes a 12,617,824-byte stamped file
                                                                   # (64-byte RNNUE1 header + payload)
#    Note the stamped values it prints -- record king_bucket_map_crc beside the net in the table above.

# 7. Verify. GOTCHA: pawnstar_eval is NOT header-aware -> feed it the RAW net; test_nnue /
#    test_nnue_incremental load through the engine, which only accepts a STAMPED net (the raw path was
#    removed), so pass them the stamped net. (tools/verify_net.sh automates this: it runs pawnstar_eval on
#    the raw net and stamps a scratch copy for the engine tests.)
cd /path/to/pawnstar
BULLET=~/pawnstar_nnue/bullet tools/verify_net.sh "$RAW"          # cross-check (<=2 cp) + incremental
cut -d'|' -f1 test/nnue_reference.txt > /tmp/fens.txt            # reuse the checked-in FENs (first field)
"$EVAL" "$RAW" /tmp/fens.txt > test/nnue_reference.txt           # re-baseline the checked-in reference (RAW net)

# 8. Regenerate the Bratko-Kopec accepted moves (the eval changed), then a full check. The search is
#    single-threaded/deterministic per depth; the accepted set is the union of the engine's best moves over
#    depths 8-11. Capture them straight from the harness's own output (`got=<move>` per position):
for d in 8 9 10 11; do PAWNSTAR_THREADS=1 ./build/test_bratko_kopec_nnue nnue/pawnstar-v20.bin $d 2>&1 \
    | grep -oE "pos[0-9]+ got=[a-h][0-9][a-h][0-9][nbrq]?"; done   # union per pos -> kCases in
                                                                   # test/bratko_kopec_nnue_test.cpp
make clean && make && make check    # rebuild the engine (re-embeds the new net via embedded_net.S .incbin),
                                    # then run all suites; BK must be 24/24 at depths 8-11. The bench
                                    # signature in test/bench_golden_test.cpp moves with the net, and so
                                    # do the shipped net's two CRCs in test/net_header_test.cpp.

# 9. Strength-test vs the previous net (v12). v20 (mirrored, 8 heads) and v12 (unmirrored, 1 head) are
#    DIFFERENT shapes with DIFFERENT file sizes, so this is a CROSS-SHAPE match: the v12 baseline needs its
#    own engine build (KING_MIRRORING=0 OUTPUT_BUCKETS=1, which can load v12), and the v20 candidate's
#    default/embedded net is the new shape — so neither binary can setoption-load the other's net. Point
#    BIN_A/BIN_B at the two separately-built engines and give the candidate its startup net via
#    CAND_STARTUP_NET (run_sprt.sh wraps it). v20 measured +9.99 Elo @ 8+0.08 (8,000 games, 95%
#    +4.33..+15.66) and +1.13 @ 12+0.12 (12,000 games, 95% -3.17..+5.43).
CAND_STARTUP_NET=nnue/pawnstar-v20.bin BIN_A=/path/to/pawnstar-v20-engine BIN_B=/path/to/pawnstar-v12-engine \
  BASELINE_NET=/path/to/pawnstar-v12.bin TC=8+0.08 \
  tools/run_sprt.sh nnue/pawnstar-v20.bin /path/to/openings.epd 20000
```

To train a **stronger** net, the measured levers in order are: make sure the schedule's last
learning-rate drop applies with ~30 superbatches after it (§6, Training — worth 0.879% of held-out loss);
more epochs over the data already in hand (~15 Elo); then more lc0 data. To change the **architecture** —
width (`kHiddenSize`), bucket count, the bucket map or the output-head count — change it identically in
`nnue.h`, `pawnstar.rs` and `pawnstar_eval.rs`, re-sync + rebuild the bullet examples, rebuild the engine,
and retrain. A different architecture is stamped into the header, so an engine built for one shape refuses
a net of another (§2).

## 8. Reproducibility

`setup_bullet.sh` pins bullet to commit `da426a49…` (bullet's API drifts; `tools/bullet/*.rs` target
that commit). It is idempotent: re-running clones if needed, checks out the pinned commit, reinstalls
the example sources, and registers them in `crates/bullet_lib/Cargo.toml` only if absent. The default
working area is `~/pawnstar_nnue/` (bullet checkout, datasets, checkpoints); override the bullet
location with `BULLET=…`.

## 9. Troubleshooting

- **`undefined symbol: cuFuncLoad` when building bullet with `--features cuda`** — the NVIDIA driver is
  too old (it lacks the CUDA ≥ 12.3 driver API). Upgrade the driver to ≥ 545, or train on CPU with
  `BULLET_FEATURES=""`.
- **`NNUE: refusing net '…': it carries no 'RNNUE1' magic …`** — you handed the engine a raw (unstamped)
  bullet net, or a net in the retired `PSN1` container. Stamp a raw net with `tools/stamp_net <raw>
  <stamped>`; migrate a PSN1 net with `tools/convert_psn1_payload.py` first (§2).
- **`NNUE: refusing net '…': <field> is 0x… in the file, 0x… in this engine`** — a stamped net whose
  header does not match this build. Every architecture field is checked against the compile-time
  constants, so an incompatible net is never silently misread — including a *larger* (e.g. wider) one.
  `king_bucket_map_crc` is the one to read twice: the shape matches and only the bucket *map* differs,
  which before the digest loaded cleanly and played while reading the wrong weight bank.
- **`NNUE: refusing net '…': its header is corrupt` / `its payload is corrupt`** — the file's own CRC-64
  does not match its bytes: a truncated or interrupted copy, or a hand-edited net. Re-fetch or re-stamp
  it; never patch a net file in place.
- **`NNUE: refusing net '…': it carries N payload bytes after the header, not the M its payload_bytes
  claims`** — a truncated file, or a raw bullet net that was concatenated rather than stamped. RNNUE1
  records an exact length and bullet's tail padding is dropped at stamping time.
- **A refused net changes nothing, and nothing falls back to another net.** Every check the loader makes
  happens before a weight is written, so a refused `setoption name EvalFile` leaves the net that was
  playing still loaded and still playing — the engine is never left in the undefined state of answering
  `readyok` with weight arrays `Load` never filled. At startup a refused net is **fatal** and the engine
  exits non-zero. If a match looks like it is playing a net you replaced, read stderr: every refusal
  goes to stderr as well as to `info string`, because a harness keeps stderr and drops info lines.
- **Engine "disconnects" mid-match** — the old long-game history overflow is fixed (the game history is
  a dynamic `std::vector` now), so this should be rare; if it recurs it is an actual crash worth a debug
  build. Adjudication (`run_sprt.sh`) is still recommended to keep matches short and decisive.
- **bullet build links `-lcuda` against the system driver** (`/usr/lib/x86_64-linux-gnu/libcuda.so`),
  not the toolkit, so the *driver* version is what gates `cuFuncLoad`; a newer toolkit alone won't help.
