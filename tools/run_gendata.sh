#!/usr/bin/env bash
# Launch parallel self-play data-generation shards for NNUE training.
#
# Usage: run_gendata.sh <out_dir> <num_processes> <games_per_process> [gen_data options...]
#
# Each process writes its own shard (data_<i>.txt, plus the data_<i>.txt.meta provenance sidecar) with a
# distinct seed and runs a single-threaded search (gen_data forces PAWNSTAR_THREADS=1), so N processes use N
# cores. Everything after the third argument is passed through to gen_data unchanged, so the campaign's
# labelling settings live in one command line:
#
#   tools/run_gendata.sh ~/pawnstar_nnue/selfplay 30 400 \
#       --openings nnue/openings.epd --play-nodes 20000 --label-nodes 220000 --label-interval 4 --pv-plies 4
#
# RESUMABLE: a shard whose .meta sidecar exists is skipped, because gen_data writes the sidecar only after
# the shard is complete. So a run that was interrupted (or that is being extended with more workers) can be
# re-issued with the same arguments and it picks up only the shards that did not finish. A shard that was
# killed part-way leaves a .txt with no .meta: it is still valid training data, but it will be REGENERATED
# and overwritten by a re-run, so move it aside first if you want to keep it.
#
# Stop a run early with `pkill -f gen_data`; the finished shards are unaffected.
set -euo pipefail

OUT_DIR="${1:?out_dir}"         # where shards (data_<i>.txt), sidecars and logs are written
NPROC="${2:?num_processes}"     # number of parallel gen_data processes (~ cores to use)
GAMES="${3:?games_per_process}" # self-play games each process generates
shift 3                         # everything left is gen_data's own options

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GEN="$ROOT/build/gen_data"      # built by `make tools` (tools\build.ps1 tools on Windows)
[ -x "$GEN" ] || GEN="$ROOT/build/gen_data.exe"
if [ ! -x "$GEN" ]; then
    echo "run_gendata.sh: build the tools first (make tools, or tools\\build.ps1 tools)" >&2
    exit 1
fi
mkdir -p "$OUT_DIR"

echo "run_gendata.sh: $NPROC processes x $GAMES games -> $OUT_DIR"
if [ $# -gt 0 ]; then
    echo "run_gendata.sh: gen_data options: $*"
fi
pids=()
launched=0
skipped=0
for i in $(seq 1 "$NPROC"); do
    shard="$OUT_DIR/data_$i.txt"
    if [ -f "$shard.meta" ]; then
        skipped=$((skipped + 1))
        continue # finished on an earlier run (the sidecar is written last); leave it alone
    fi
    # Distinct per-process seed so the shards explore different games rather than duplicating each other.
    # The shard index fixes the seed, so a resumed worker replays the games it was always going to play.
    seed=$((1000 + i))
    # One process per shard, backgrounded; each is single-threaded (gen_data forces PAWNSTAR_THREADS=1).
    # Run from the repository root so the default net path and nnue/openings.epd resolve.
    ( cd "$ROOT" && "$GEN" "$shard" "$GAMES" "$seed" "$@" ) > "$OUT_DIR/log_$i.txt" 2>&1 &
    pids+=($!) # remember the PID so the run can be monitored or killed
    launched=$((launched + 1))
done
echo "run_gendata.sh: launched $launched, skipped $skipped already-finished shard(s)"
if [ "$launched" -eq 0 ]; then
    echo "run_gendata.sh: nothing to do"
    exit 0
fi
echo "run_gendata.sh: pids: ${pids[*]}"

# Progress while the shards run: each worker's own stderr progress lines go to its log, so one line here
# per interval summarising the whole run is enough to see it is alive and how fast it is going. The loop
# runs until every worker has gone, not just the first one to finish.
started="$(date +%s)"
report_progress() {
    local records elapsed
    records="$(cat "$OUT_DIR"/data_*.txt 2>/dev/null | wc -l || echo 0)"
    elapsed=$(( $(date +%s) - started ))
    [ "$elapsed" -le 0 ] && elapsed=1
    echo "$records $elapsed $launched" |
        awk '{printf "run_gendata.sh: %ds, %d records, %.1f records/s total, %.2f records/s/core\n",
                     $2, $1, $1/$2, $1/$2/$3}'
}
(
    while true; do
        sleep 60
        alive=0
        for pid in "${pids[@]}"; do
            if kill -0 "$pid" 2>/dev/null; then
                alive=1
                break
            fi
        done
        if [ "$alive" -eq 0 ]; then
            break
        fi
        report_progress
    done
) &
progress_pid=$!

# Block until every shard finishes (or the script is interrupted). A worker that failed is named rather
# than letting `set -e` end the script on the first non-zero wait with nothing said: the shard's log holds
# the reason, and the shards that did finish are still good data.
failed=0
for pid in "${pids[@]}"; do
    wait "$pid" || failed=$((failed + 1))
done
kill "$progress_pid" 2>/dev/null || true
if [ "$failed" -gt 0 ]; then
    echo "run_gendata.sh: $failed of $launched workers failed; see $OUT_DIR/log_*.txt" >&2
fi

echo "run_gendata.sh: all shards complete."
report_progress
echo "run_gendata.sh: train with  tools/train_pipeline.sh $OUT_DIR <out_net.bin>"
