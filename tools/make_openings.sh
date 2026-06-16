#!/usr/bin/env bash
# Build an EPD openings book for SPRT from self-play data: sample distinct early-game positions
# (fullmove <= 5) so matches start from varied, roughly balanced positions.
#
# Usage: make_openings.sh <data_dir> [out.epd] [count]
set -euo pipefail

DATA_DIR="${1:?data_dir}"           # directory of self-play text shards (data_*.txt / seed*.txt)
OUT="${2:-$DATA_DIR/openings.epd}"  # where to write the EPD opening book
N="${3:-1000}"                      # how many distinct positions to sample

# Self-play text rows are "FEN | eval | result", so -F' | ' makes $1 the FEN. Splitting the FEN on spaces,
# field 6 is the fullmove number; keep only early positions (move <= 5) so games start near the opening.
# sort -u drops duplicate positions; shuf -n takes a random N of what remains. The glob may not match both
# shard naming schemes, hence 2>/dev/null to swallow "no such file" noise.
awk -F' \\| ' '{n=split($1,a," "); if (a[6]+0 <= 5) print $1}' \
    "$DATA_DIR"/data_*.txt "$DATA_DIR"/seed*.txt 2>/dev/null \
    | sort -u | shuf -n "$N" > "$OUT"

echo "wrote $(wc -l < "$OUT") openings -> $OUT"
