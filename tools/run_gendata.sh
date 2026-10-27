#!/usr/bin/env bash
# Launch parallel self-play data-generation shards for NNUE training.
#
# Usage: run_gendata.sh <out_dir> <num_processes> <games_per_process> [gen_data options...]
#
# Each process writes its own shard (data_<g>.txt, plus the data_<g>.txt.meta provenance sidecar) with a
# distinct seed and runs a single-threaded search (gen_data forces PAWNSTAR_THREADS=1), so N processes use N
# cores. Everything after the third argument is passed through to gen_data unchanged, so the campaign's
# labelling settings live in one command line:
#
#   tools/run_gendata.sh ~/pawnstar_nnue/selfplay 30 400 \
#       --openings nnue/openings-selfplay.epd --play-nodes 20000 --label-nodes 220000 --label-interval 4 \
#       --pv-plies 4
#
# NO TWO WORKERS IN THE WHOLE CAMPAIGN CAN GENERATE THE SAME GAME. Each is given --worker-index and
# --worker-count for its place in this machine's fan, and --global-worker-offset and --global-worker-count
# for its place in the campaign, which together hand it a disjoint slice of the opening pool (gen_data's
# OpeningSchedule). The engine is deterministic at a fixed budget, so a game is decided by where it starts:
# disjoint openings make a duplicate game impossible by construction rather than merely unlikely, which is
# the promise worth having over a 1-2 week campaign. The slices are the openings' indices modulo the
# CAMPAIGN'S worker count, so the pool must hold at least as many openings as the campaign has workers -
# generate one with tools/gen_openings.cpp, which has no ceiling.
#
# ACROSS MACHINES, SET THESE TWO (see nnue/README.md §6, "Running the campaign across machines"):
#
#   GLOBAL_WORKER_COUNT   how many workers the whole campaign has, summed over every machine. Default: this
#                         machine's <num_processes>, which is right only for a single-machine run.
#   GLOBAL_WORKER_OFFSET  the campaign index of this machine's first worker. Default 0. Machine 0 takes 0,
#                         and each later machine takes the previous machine's offset plus its worker count.
#
#   # a 24-core box, a 12-core box and two 48-core rented boxes: 132 workers in all
#   GLOBAL_WORKER_COUNT=132 GLOBAL_WORKER_OFFSET=0   tools/run_gendata.sh ~/selfplay 24 400 ...
#   GLOBAL_WORKER_COUNT=132 GLOBAL_WORKER_OFFSET=24  tools/run_gendata.sh ~/selfplay 12 400 ...
#   GLOBAL_WORKER_COUNT=132 GLOBAL_WORKER_OFFSET=36  tools/run_gendata.sh ~/selfplay 48 400 ...
#   GLOBAL_WORKER_COUNT=132 GLOBAL_WORKER_OFFSET=84  tools/run_gendata.sh ~/selfplay 48 400 ...
#
# EVERY MACHINE MUST USE THE IDENTICAL POOL FILE, or the partition partitions nothing: the slices are index
# ranges into one file, and two different files of the same size have different positions at the same
# indices. Copy the pool out, do not regenerate it per machine. An offset that overlaps another machine's
# range, or that runs past GLOBAL_WORKER_COUNT, is refused here and again in gen_data rather than silently
# generating colliding data - that is the one failure mode that wastes a week of rented CPU. The shards and
# logs are named by the worker's GLOBAL index, so every machine's output can be dropped into one directory
# without colliding, which is what tools/check_duplicates.sh and tools/train_pipeline.sh then read.
#
# When the run ends it reports the pool's duplicate rate (tools/check_duplicates.sh). That number is the one
# that says whether the campaign is worth training on; read it before spending a GPU on the result.
#
# RESUMABLE: a shard whose .meta sidecar exists is skipped, because gen_data writes the sidecar only after
# the shard is complete. So a run that was interrupted (or that is being extended with more workers) can be
# re-issued with the same arguments and it picks up only the shards that did not finish. A shard that was
# killed part-way leaves a .txt with no .meta: it is still valid training data, but it will be REGENERATED
# and overwritten by a re-run, so move it aside first if you want to keep it. Resume with the SAME
# <num_processes>, GLOBAL_WORKER_COUNT and GLOBAL_WORKER_OFFSET it was started with - all three fix the
# slices and the seeds. A finished shard records every one of them in its own .meta.
#
# Stop a run early with `pkill -f gen_data`; the finished shards are unaffected.
set -euo pipefail

OUT_DIR="${1:?out_dir}"         # where shards (data_<g>.txt), sidecars and logs are written
NPROC="${2:?num_processes}"     # number of parallel gen_data processes (~ cores to use)
GAMES="${3:?games_per_process}" # self-play games each process generates
shift 3                         # everything left is gen_data's own options

# The first worker's seed; the campaign worker whose global index is g takes SEED_BASE + g + 1. A SECOND
# CAMPAIGN WRITTEN TO A NEW DIRECTORY MUST MOVE THIS ON (e.g. SEED_BASE=2000), or it replays the first
# campaign's games: the seed and the worker's slice of the opening pool together decide what a worker plays,
# which is exactly what makes a shard reproducible. Within one campaign, leave it alone - on every machine,
# since the global index already separates them - so that a resumed worker replays its own games.
SEED_BASE="${SEED_BASE:-1000}"

# This machine's place in the campaign. The defaults describe a campaign of one machine.
GLOBAL_WORKER_OFFSET="${GLOBAL_WORKER_OFFSET:-0}"
GLOBAL_WORKER_COUNT="${GLOBAL_WORKER_COUNT:-$NPROC}"

# Guard the arguments before anything is launched. Every one of these, left to pass, costs the campaign's
# whole CPU budget: colliding slices generate duplicate games, and nothing says so until the pool is
# assembled days later.
for name in NPROC GAMES SEED_BASE GLOBAL_WORKER_OFFSET GLOBAL_WORKER_COUNT; do
    value="${!name}"
    case "$value" in
        '' | *[!0-9]*)
            echo "run_gendata.sh: $name must be a non-negative integer, got '$value'" >&2
            exit 1
            ;;
    esac
done
if [ "$NPROC" -lt 1 ]; then
    echo "run_gendata.sh: <num_processes> must be at least 1" >&2
    exit 1
fi
if [ "$GLOBAL_WORKER_COUNT" -lt 1 ]; then
    echo "run_gendata.sh: GLOBAL_WORKER_COUNT must be at least 1" >&2
    exit 1
fi
if [ $((GLOBAL_WORKER_OFFSET + NPROC)) -gt "$GLOBAL_WORKER_COUNT" ]; then
    echo "run_gendata.sh: GLOBAL_WORKER_OFFSET $GLOBAL_WORKER_OFFSET plus $NPROC workers runs past" >&2
    echo "run_gendata.sh: GLOBAL_WORKER_COUNT $GLOBAL_WORKER_COUNT, so this machine would wrap onto another" >&2
    echo "run_gendata.sh: machine's slices of the opening pool and play the same games. Set" >&2
    echo "run_gendata.sh: GLOBAL_WORKER_COUNT to the campaign's total worker count and give every machine a" >&2
    echo "run_gendata.sh: distinct offset (the first machine 0, each next one the previous machine's offset" >&2
    echo "run_gendata.sh: plus its worker count)." >&2
    exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GEN="$ROOT/build/gen_data"      # built by `make tools` (tools\build.ps1 tools on Windows)
[ -x "$GEN" ] || GEN="$ROOT/build/gen_data.exe"
if [ ! -x "$GEN" ]; then
    echo "run_gendata.sh: build the tools first (make tools, or tools\\build.ps1 tools)" >&2
    exit 1
fi
mkdir -p "$OUT_DIR"

echo "run_gendata.sh: $NPROC processes x $GAMES games -> $OUT_DIR"
echo "run_gendata.sh: campaign workers $GLOBAL_WORKER_OFFSET..$((GLOBAL_WORKER_OFFSET + NPROC - 1)) of $GLOBAL_WORKER_COUNT"
if [ $# -gt 0 ]; then
    echo "run_gendata.sh: gen_data options: $*"
fi
pids=()
launched=0
skipped=0
for i in $(seq 1 "$NPROC"); do
    # The worker's campaign-wide identity. The shard, the log and the seed are all named from it, so two
    # machines' output can share one directory and no two workers of the campaign share a seed.
    global=$((GLOBAL_WORKER_OFFSET + i - 1))
    shard="$OUT_DIR/data_$((global + 1)).txt"
    if [ -f "$shard.meta" ]; then
        skipped=$((skipped + 1))
        continue # finished on an earlier run (the sidecar is written last); leave it alone
    fi
    # Distinct per-worker seed so the shards explore different games rather than duplicating each other.
    # The global index fixes the seed, so a resumed worker replays the games it was always going to play.
    seed=$((SEED_BASE + global + 1))
    # One process per shard, backgrounded; each is single-threaded (gen_data forces PAWNSTAR_THREADS=1).
    # Run from the repository root so the default net path and the committed opening sets resolve.
    # The worker arguments come BEFORE "$@" so that a caller who passes their own still wins (gen_data takes
    # the last value of a repeated option), but a caller who does not gets the partition regardless.
    ( cd "$ROOT" && "$GEN" "$shard" "$GAMES" "$seed" \
        --worker-index "$((i - 1))" --worker-count "$NPROC" \
        --global-worker-offset "$GLOBAL_WORKER_OFFSET" --global-worker-count "$GLOBAL_WORKER_COUNT" \
        "$@" ) > "$OUT_DIR/log_$((global + 1)).txt" 2>&1 &
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

# The duplicate rate of what was just generated, and an audit of the campaign identities in the sidecars. A
# degenerate campaign is invisible in the shard sizes - the per-shard dedup makes every shard look healthy -
# so this is the only cheap place it shows. A low number is reported loudly and does not fail the script:
# the shards are written and are still data, and it is tools/train_pipeline.sh that refuses to train on a
# pool that measures badly.
if ! bash "$ROOT/tools/check_duplicates.sh" "$OUT_DIR"; then
    echo "run_gendata.sh: WARNING: the pool above is largely duplicate, or its workers' campaign identities" >&2
    echo "run_gendata.sh: collide. Do not train on it until the settings are fixed;" >&2
    echo "run_gendata.sh: tools/check_duplicates.sh's output says what to look at." >&2
fi

echo "run_gendata.sh: train with  tools/train_pipeline.sh $OUT_DIR <out_net.bin>"
