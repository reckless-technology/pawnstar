#!/usr/bin/env bash
# Build an EPD opening set from Pawnstar's own opening book, for SPRT matches (tools/run_sprt.sh OPENINGS=)
# and for self-play data generation (tools/gen_data.cpp --openings).
#
# The source is pawnstar.book, the public-domain TSCP book.txt in coordinate notation, so the result carries
# no licence restriction of its own and no third party's data. Every book line is replayed from the start
# position; each position it reaches inside the ply window is a candidate. The engine then evaluates each
# one and only roughly balanced positions are kept, so a match that starts there is decided by the engines
# rather than by the opening. Duplicates are dropped - the book's lines share prefixes heavily.
#
# "Roughly balanced" is measured against the engine's own evaluation of the START POSITION, not against
# zero: this net scores the initial position at about +158 cp, so a zero-centred filter would reject normal
# openings and keep only the ones it mildly dislikes. The baseline is read from the engine at run time, so
# this stays right when the net changes.
#
# Usage: make_openings.sh [out.epd] [min_ply] [max_ply] [margin_cp]
#   out.epd    where to write (default nnue/openings.epd, the committed set).
#   min_ply    first ply harvested from each line (default 4).
#   max_ply    last ply harvested from each line (default 24, i.e. the end of every line in this book).
#   margin_cp  keep a position only when its White-relative evaluation is within this many centipawns of
#              the start position's (default 125).
#
# SIZE: the TSCP book is 385 heavily-overlapping lines, which hold only ~420 distinct positions in this
# window however wide the margin; the default margin keeps ~306 of them. That is the ceiling of this source,
# not a choice - an SPRT with more rounds than openings will replay some of them (fastchess `order=random`),
# which is fine but worth knowing.
#
# Requires build/pawnstar (`make`, or `tools\build.ps1` on Windows). The engine is driven over one UCI
# session: per candidate it is sent `position startpos moves ...` then `getboard` (which prints the FEN) and
# `nnue` (which prints the raw network evaluation, side-to-move relative). Those two reply lines per
# candidate come back in order, so they pair up with no correlation needed.
#
# The static network evaluation is used rather than a search: it costs microseconds instead of milliseconds
# per candidate, and "is this opening roughly level" does not need search depth - a book line that is
# actually losing shows up as a large static score too.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/nnue/openings.epd}"
MIN_PLY="${2:-4}"
MAX_PLY="${3:-24}"
MARGIN="${4:-125}"

ENGINE="$ROOT/build/pawnstar"
[ -x "$ENGINE" ] || ENGINE="$ROOT/build/pawnstar.exe"
if [ ! -x "$ENGINE" ]; then
    echo "make_openings.sh: build the engine first (make, or tools\\build.ps1)" >&2
    exit 1
fi
BOOK="$ROOT/pawnstar.book"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# [1/4] The baseline: what this engine's net thinks of the start position, White to move. Every candidate is
# compared with this rather than with zero.
BASELINE="$( (cd "$ROOT" && printf 'position startpos\nnnue\nquit\n' | "$ENGINE") 2>/dev/null \
    | awk '/^nnue /{print $2; exit}')"
if [ -z "$BASELINE" ]; then
    echo "make_openings.sh: the engine did not answer 'nnue'; is a net loadable?" >&2
    exit 1
fi
echo "[1/4] start-position baseline ${BASELINE} cp; keeping positions within +/-${MARGIN} cp of it"

# [2/4] Turn each book line into the UCI commands for its candidate positions.
#
# A book line is whitespace-separated coordinate moves; filter_book may have suffixed a move with '?' to
# mark it questionable, which is not part of the move, and move-number tokens start with a digit. Strip
# both. Then, for every ply in [MIN_PLY, MAX_PLY] that the line reaches, emit the three commands that set
# that position up and report it.
awk -v min_ply="$MIN_PLY" -v max_ply="$MAX_PLY" '
    { sub(/#.*/, ""); n = 0; delete moves }
    {
        for (i = 1; i <= NF; i++) {
            token = $i
            if (token ~ /^[0-9]/) { continue }        # a move number, not a move
            sub(/\?$/, "", token)                     # a questionable-move mark, not part of the move
            if (token == "") { continue }
            moves[++n] = token
        }
        for (ply = min_ply; ply <= max_ply && ply <= n; ply++) {
            line = ""
            for (i = 1; i <= ply; i++) { line = line " " moves[i] }
            print "position startpos moves" line
            print "getboard"
            print "nnue"
        }
    }
' "$BOOK" > "$WORK/commands.txt"
printf 'quit\n' >> "$WORK/commands.txt"
echo "[2/4] $(( $(wc -l < "$WORK/commands.txt") / 3 )) candidate positions from $(wc -l < "$BOOK") book lines"

# [3/4] One engine session reports them all. Run from the repository root so the engine finds its net.
( cd "$ROOT" && "$ENGINE" < "$WORK/commands.txt" ) > "$WORK/replies.txt" 2>/dev/null

# [4/4] Pair each FEN with the `nnue <score>` line that follows it, turn that score into White's
# perspective (the engine reports it side-to-move relative, and half these positions have Black to move),
# keep the balanced ones, dedup, and write the EPD as the four position fields - consumers append their own
# clock, so a position never carries a stale fifty-move count into a game.
awk -v baseline="$BASELINE" -v margin="$MARGIN" '
    /^nnue / {
        if (fen != "") {
            split(fen, field, " ")
            white_score = (field[2] == "w") ? $2 : -$2
            deviation   = white_score - baseline
            if (deviation < 0) { deviation = -deviation }
            if (deviation <= margin) { print fen }
        }
        fen = ""
        next
    }
    /^[1-8rnbqkpRNBQKP\/]+ [wb] / { fen = $1 " " $2 " " $3 " " $4; next }
    { fen = "" }
' "$WORK/replies.txt" | sort -u > "$WORK/positions.epd"

{
    echo "# Pawnstar opening set, for SPRT matches and self-play data generation."
    echo "#"
    echo "# Generated by tools/make_openings.sh from pawnstar.book, which is the opening book of TSCP"
    echo "# (Tom Kerrigan's Simple Chess Program, https://github.com/terredeciels/TSCP) and is in the"
    echo "# PUBLIC DOMAIN - so this set carries no licence restriction of its own and no third party's"
    echo "# data. Every book line was replayed from the start position, and each position it reached"
    echo "# between ply $MIN_PLY and ply $MAX_PLY was kept when the engine's static NNUE evaluation of it was"
    echo "# within $MARGIN centipawns of its evaluation of the start position ($BASELINE cp, White's"
    echo "# perspective); then duplicates were removed. The book's 385 lines overlap heavily and hold only"
    echo "# about 420 distinct positions in this window, so this is near the ceiling of the source: an SPRT"
    echo "# with more rounds than there are openings here replays some of them."
    echo "#"
    echo "# One position per line, as the first four FEN fields. Lines beginning with '#' are comments:"
    echo "# tools/gen_data.cpp skips them, and tools/run_sprt.sh strips them before handing the file to"
    echo "# fastchess, whose EPD reader does not."
    cat "$WORK/positions.epd"
} > "$OUT"

echo "[4/4] wrote $(grep -cv '^#' "$OUT") openings -> $OUT"
