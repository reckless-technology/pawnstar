#!/usr/bin/env bash
# Launch parallel self-play data-generation shards for NNUE training.
#
# Usage: run_gendata.sh <out_dir> <num_processes> <games_per_process> <depth> [random_plies]
#
# Each process writes its own shard (data_<i>.txt) with a distinct seed and runs a single-threaded
# search (gen_data forces PAWNSTAR_THREADS=1), so N processes use N cores. Stop early at any time with
# `pkill -f gen_data` — partial shards are still valid training data.
set -euo pipefail

OUT_DIR="${1:?out_dir}"
NPROC="${2:?num_processes}"
GAMES="${3:?games_per_process}"
DEPTH="${4:?depth}"
RANDOM_PLIES="${5:-8}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GEN="$ROOT/build/gen_data"
mkdir -p "$OUT_DIR"

echo "launching $NPROC processes x $GAMES games at depth $DEPTH -> $OUT_DIR"
pids=()
for i in $(seq 1 "$NPROC"); do
    seed=$(( 1000 + i ))
    "$GEN" "$OUT_DIR/data_$i.txt" "$GAMES" "$seed" "$DEPTH" "$RANDOM_PLIES" \
        > "$OUT_DIR/log_$i.txt" 2>&1 &
    pids+=($!)
done
echo "pids: ${pids[*]}"
echo "tail $OUT_DIR/log_*.txt to watch progress; combined line count: wc -l $OUT_DIR/data_*.txt"
wait
echo "all shards complete"
