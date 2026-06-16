#!/usr/bin/env bash
# Launch parallel self-play data-generation shards for NNUE training.
#
# Usage: run_gendata.sh <out_dir> <num_processes> <games_per_process> <depth> [random_plies]
#
# Each process writes its own shard (data_<i>.txt) with a distinct seed and runs a single-threaded
# search (gen_data forces PAWNSTAR_THREADS=1), so N processes use N cores. Stop early at any time with
# `pkill -f gen_data` — partial shards are still valid training data.
set -euo pipefail

OUT_DIR="${1:?out_dir}"             # where shards (data_<i>.txt) and logs are written
NPROC="${2:?num_processes}"         # number of parallel gen_data processes (≈ cores to use)
GAMES="${3:?games_per_process}"     # self-play games each process generates
DEPTH="${4:?depth}"                 # fixed search depth per move
RANDOM_PLIES="${5:-8}"              # random opening plies, for position diversity across games

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GEN="$ROOT/build/gen_data"          # built by `make tools`
mkdir -p "$OUT_DIR"

echo "launching $NPROC processes x $GAMES games at depth $DEPTH -> $OUT_DIR"
pids=()
for i in $(seq 1 "$NPROC"); do
    # Distinct per-process seed so the shards explore different games rather than duplicating each other.
    seed=$(( 1000 + i ))
    # One process per shard, backgrounded; each is single-threaded (gen_data forces PAWNSTAR_THREADS=1).
    "$GEN" "$OUT_DIR/data_$i.txt" "$GAMES" "$seed" "$DEPTH" "$RANDOM_PLIES" \
        > "$OUT_DIR/log_$i.txt" 2>&1 &
    pids+=($!)   # remember the PID so the user can pkill / monitor
done
echo "pids: ${pids[*]}"
echo "tail $OUT_DIR/log_*.txt to watch progress; combined line count: wc -l $OUT_DIR/data_*.txt"
wait   # block until every shard finishes (or the script is interrupted)
echo "all shards complete"
