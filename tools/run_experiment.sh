#!/usr/bin/env bash
# One-shot NNUE experiment: build openings, train the net, VERIFY it, then run an SPRT.
#
# The data source can be a directory of self-play text shards (generate with run_gendata.sh first) or an
# already-converted bulletformat .data file (e.g. public PlentyChess data, which is how the shipped nets
# were trained — train_pipeline.sh auto-detects which). NNUE is the only evaluator, so the SPRT is
# net-vs-net: set BASELINE_NET=<baseline net.bin> to choose the opponent (see run_sprt.sh for TC /
# cross-width options).
#
# Usage: run_experiment.sh <data_dir | shuffled.data> <net.bin> [superbatches] [sprt_rounds] [depth]
#
# Full from-scratch self-play flow:
#   tools/setup_bullet.sh                                   # once: clone+register bullet
#   make tools && make check                                # build gen_data and the test exes
#   tools/run_gendata.sh ~/pawnstar_nnue/data 12 100000 8   # generate (stop with: pkill -f gen_data)
#   tools/run_experiment.sh ~/pawnstar_nnue/data net.bin    # openings + train + verify + SPRT
set -euo pipefail

DATA_IN="${1:?data_dir or shuffled.data}"   # self-play shard dir OR a pre-converted .data file
NET="${2:?net.bin}"                          # output net path (also the SPRT candidate)
SBS="${3:-40}"                               # superbatches to train (passed to train_pipeline.sh)
ROUNDS="${4:-500}"                           # SPRT rounds
DEPTH="${5:-8}"                              # SPRT fixed search depth (ignored if run_sprt.sh sees TC)
REPO="$(cd "$(dirname "$0")/.." && pwd)"

# Openings can be derived from text shards; a pre-converted .data file carries no FENs we can sample, so the
# caller must supply their own OPENINGS=<openings.epd> in that case.
if [ -d "$DATA_IN" ]; then
    bash "$REPO/tools/make_openings.sh" "$DATA_IN"
    OPENINGS="$DATA_IN/openings.epd"
else
    OPENINGS="${OPENINGS:?with bulletformat data, set OPENINGS=<openings.epd>}"
fi

# The four pipeline stages, in order. Any failure aborts the run (set -e), so a bad net never reaches SPRT.
bash "$REPO/tools/train_pipeline.sh" "$DATA_IN" "$NET" "$SBS"   # train + quantise + export
bash "$REPO/tools/verify_net.sh" "$NET"                         # gate: engine eval == trainer eval
bash "$REPO/tools/run_sprt.sh" "$NET" "$OPENINGS" "$ROUNDS" "$DEPTH"  # net-vs-baseline SPRT
