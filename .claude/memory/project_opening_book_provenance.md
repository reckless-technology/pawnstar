# The opening book's provenance, and why the old one had to go (2026-10-30)

`pawnstar.book` is now built by `tools/make_book.cpp` from the **Lichess open database**, which is released
under the **Creative Commons CC0 1.0** public-domain dedication. Before 2026-10-30 it was a 385-line book
derived from **TSCP's `book.txt`** and asserted to be "public domain" in about twenty places across this
repository - including strings the released engine printed, the READMEs, `nnue/openings.epd`'s own header,
and a `test_uci_commands` check that *enforced* the claim.

## The claim was false, and rewording it could not have fixed it

- TSCP is **Copyright 1997 Tom Kerrigan**.
- The only permission anyone has ever located is an informal 2002 email allowing use "as long as you're not
  making any money off of it". That is a **non-commercial** permission, which GPLv3 cannot take, and it is
  not a dedication of anything to the public domain.
- The `terredeciels/TSCP` repository the documents linked to carries **no LICENSE file at all**.

So the material was removed rather than re-described. **A provenance claim is either checkable or it is a
guess, and a guess written confidently in twenty places is worse than no claim.** CC0 was chosen for the
replacement precisely because it is a dedication the rights holder actually made.

**CC0 obliges no attribution.** `THIRD-PARTY-NOTICES.md`, the book's own `#` header and the `license` UCI
notice all credit Lichess *and say plainly that nothing requires them to*. Overstating an obligation is the
mirror image of the defect being fixed, and there are tests pinning both halves: that the new source and its
dedication are named, that "TSCP" and "Kerrigan" appear nowhere in what the engine prints or the book says,
and that the courtesy is still described as a courtesy.

## The width was a measurement problem in its own right

The 385-line book yielded only ~420 distinct positions in any sensible ply window, of which ~306 were
balanced - and `nnue/openings.epd` was those 306. A long match therefore replayed each opening about 13
times over 4,000 pairs, verbatim game repeats ran at 5.4% of one 8,000-game run, and the intraclass
correlation of pair scores grouped by opening measured **0.07-0.13**, which `tools/opening_icc.py` exists to
widen a bar for. Every ICC figure in that tool and in `nnue/README.md` was measured on the 306-position set
and is kept as a record of what was measured, not updated to the new size.

The new book is **3,244 lines** as `make_book` wrote them - **3,328** since the screening added 84 - and
`nnue/openings.epd` is **6,433 positions**, regenerated from the screened book on 2026-10-31. That
regeneration added 56 positions and **removed none**, which follows from what screening does: it marks a
move or adds a line, never deletes one, and `make_openings.sh` strips the `?` marks before harvesting. So
the 6,377-position set is a strict subset of this one, and a figure measured on it was not invalidated. The
replay factor falls by about 21x against the old 306 and the correction with it. More distinct openings is a direct, free win on
every confidence interval the matches produce.

## How the book is built (the decisions worth not re-deriving)

- **One PGN reader.** `tools/make_book.cpp` uses `tools/lichess_extract.h` - its SAN resolver, line reader,
  game splitter, Elo tag reader and byte-exact digest, all pinned by `test_lichess_extract`. Do not write a
  second PGN parser in this repository.
- **The source:** `lichess_db_standard_rated_2017-01.pgn.zst` from <https://database.lichess.org/>, 1,902,198,021 bytes, SHA-256 `d1236dcd954089aee162c7b0d82f51162f7c912882343d47b77ba5c0e05512f6`. Filters: both players' Elo >= 2200, `TimeControl` base >= 180 s (which is
  what excludes bullet - at bullet speeds the opening move says more about the clock than the position),
  standard variant, finished game, at least 16 plies long. 73,128 of 10,680,708 games survived.
- **The quality filter is frequency, nothing else.** Each accepted game's first 16 plies go into a
  prefix tree; a move is kept only where at least **8 games, and 1% of the games reaching the position** games played it from that position. A
  move a dozen-plus strong players chose does not need a second opinion, so `tools/filter_book` was not run
  when the book was first built. **It was run on 2026-10-31**, at a 100 cp margin against the engine's own
  judgement with `pawnstar-v20.bin` at depth 12, so the shipped book now carries 1,172 `?` marks over 120
  distinct moves, and 84 lines were added - the engine's own choice, unmarked - for the positions whose
  every move was marked and which would otherwise have had nothing to play. Every database line is still
  present in full; the book's own `# ---- filter_book ----` header records all of it.
- **Each pruned leaf is one line**, so no line is a prefix of another and each appears once. **That makes a
  move's weight in `OpeningBook::GetMove` the number of lines passing through it, not the number of games
  that played it** - the two correlate but are not the same number. Writing one line per game would
  reproduce game counts and make the file tens of megabytes, which `src/embedded_book.S` embeds in the
  engine binary; not worth it for a weighting that only breaks ties between sound opening moves.
- **The file carries its own provenance**, as `#` comments, which `OpeningBook::ParseLineOfPlay` already
  treats as comment to end of line - so no parser change was needed. It records the monthly export, every
  filter, the counts and the **SHA-256 of exactly the bytes read** (`PgnReader::Digest`, which hashes bytes
  as they are handed out, not as they are read). The input is a pipe over a multi-gigabyte file that can
  never be re-read, so that digest is the only evidence of what was consumed.

## This file is owed to the two sibling ports

The same 385-line book was byte-identical in `pawnstar-rs` and in `pawnstar-go` (as `engine/book.txt`,
`go:embed`ed), and the `license` notice is 30-of-32 lines identical across the three by design. **The new
book is the single canonical artefact: copy the bytes, do not regenerate.** Regenerating would produce a
different file (the header carries a timestamp, and a different month would differ entirely), and the three
ports are meant to be bit-exact.
