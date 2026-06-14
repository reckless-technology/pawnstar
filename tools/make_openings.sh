#!/usr/bin/env bash
# Build an EPD openings book for SPRT from self-play data: sample distinct early-game positions
# (fullmove <= 5) so matches start from varied, roughly balanced positions.
#
# Usage: make_openings.sh <data_dir> [out.epd] [count]
set -euo pipefail

DATA_DIR="${1:?data_dir}"
OUT="${2:-$DATA_DIR/openings.epd}"
N="${3:-1000}"

awk -F' \\| ' '{n=split($1,a," "); if (a[6]+0 <= 5) print $1}' \
    "$DATA_DIR"/data_*.txt "$DATA_DIR"/seed*.txt 2>/dev/null \
    | sort -u | shuf -n "$N" > "$OUT"

echo "wrote $(wc -l < "$OUT") openings -> $OUT"
