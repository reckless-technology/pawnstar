#!/usr/bin/env bash
# Anchored Elo estimate for Pawnstar: play the engine against one or more rated reference engines at a
# fast time control and convert each result to an absolute estimate (anchor_rating + measured Elo diff).
# Requires fastchess on PATH. The reference engines are a local prerequisite (e.g. the CCRL engines that
# ship with Arena) — they are not in the repo, so you supply their paths and CCRL ratings as arguments.
#
# Usage: rate.sh <name:rating:cmd> [<name:rating:cmd> ...]
#   each anchor:  name      short label for the opponent
#                 rating    its (CCRL-ballpark) Elo
#                 cmd       path to the opponent UCI engine binary
#
# Env:
#   ENGINE       candidate engine binary           (default: <repo>/build/pawnstar)
#   NET          NNUE net for the candidate       (default: <repo>/nnue/pawnstar-v9.bin)
#   OPENINGS     EPD opening book                  (required)
#   TC           time control                      (default: 8+0.08)
#   GAMES        games per anchor                  (default: 100)
#   CONCURRENCY  parallel games                    (default: nproc)
#   OUT          output directory                  (default: ./rating_out)
#
# Example:
#   ENGINE=build/pawnstar NET=nnue/pawnstar-v9.bin OPENINGS=~/openings.epd \
#   tools/rate.sh toga:2950:/path/to/togaii senpai:3050:/path/to/senpai
set -uo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${ENGINE:-$REPO/build/pawnstar}"
NET="${NET:-$REPO/nnue/pawnstar-v9.bin}"
OPENINGS="${OPENINGS:?set OPENINGS=<openings.epd>}"
TC="${TC:-8+0.08}"
GAMES="${GAMES:-100}"
CONCURRENCY="${CONCURRENCY:-$(nproc)}"
OUT="${OUT:-./rating_out}"

[ "$#" -ge 1 ] || { echo "usage: $0 <name:rating:cmd> [...]   (see header for env)"; exit 2; }
mkdir -p "$OUT"

# Single-threaded candidate (deterministic-ish, frees cores); the EvalFile option picks the net.
export PAWNSTAR_THREADS=1
unset PAWNSTAR_NNUE PAWNSTAR_EVALFILE
cand_opts=("option.EvalFile=$NET")

: > "$OUT/summary.txt"   # truncate the summary; each anchor appends one line
estimates=()
for anchor in "$@"; do
    # Split the "name:rating:cmd" triple. cmd may itself contain ':' (rare), so take name and rating off the
    # front with %% / # expansions and leave the remainder as the command path.
    name="${anchor%%:*}"; rest="${anchor#*:}"; rating="${rest%%:*}"; cmd="${rest#*:}"
    if [ ! -x "$cmd" ]; then
        echo "$name: opponent binary not found/executable: $cmd" | tee -a "$OUT/summary.txt"
        continue
    fi
    log="$OUT/vs_$name.log"
    # Play GAMES games as paired rounds (-games 2 -repeat = each opening played once with each colour, so
    # GAMES/2 rounds). The opponent is pinned to 1 thread / 64 MB hash for a fair, reproducible-ish match.
    fastchess -engine cmd="$ENGINE" name=pawnstar "${cand_opts[@]}" \
              -engine cmd="$cmd" name="$name" option.Threads=1 option.Hash=64 \
              -each proto=uci tc="$TC" -rounds $(( GAMES / 2 )) -games 2 -repeat \
              -openings file="$OPENINGS" format=epd order=random -concurrency "$CONCURRENCY" > "$log" 2>&1 || true
    # Scrape fastchess's final report: the score line (for display) and the measured Elo difference.
    pts=$(grep -E '^Games:|Points:' "$log" | tail -1)
    diff=$(grep '^Elo:' "$log" | tail -1 | sed -E 's/^Elo: (-?[0-9.]+).*/\1/')
    if [ -n "${diff:-}" ]; then
        # Absolute estimate = the opponent's known rating + our measured difference against it.
        est=$(awk -v r="$rating" -v d="$diff" 'BEGIN{printf "%.0f", r + d}')
        estimates+=("$est")
        echo "$name (anchor ~$rating): diff=$diff  -> ~$est   [$pts]" | tee -a "$OUT/summary.txt"
    else
        echo "$name: no Elo parsed (match failed?) -- $(tail -1 "$log")" | tee -a "$OUT/summary.txt"
    fi
done

# Average the per-anchor estimates into one number (more anchors -> less dependence on any single one).
if [ "${#estimates[@]}" -gt 0 ]; then
    mean=$(printf '%s\n' "${estimates[@]}" | awk '{s+=$1; n++} END{printf "%.0f", s/n}')
    echo "mean anchored estimate over ${#estimates[@]} anchor(s): ~$mean" | tee -a "$OUT/summary.txt"
fi
echo DONE > "$OUT/STATUS"   # completion marker for any watching script
