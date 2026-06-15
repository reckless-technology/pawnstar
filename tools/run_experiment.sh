#!/usr/bin/env bash
# One-shot NNUE experiment: build openings, train the net, VERIFY it, then run an SPRT.
#
# The data source can be a directory of self-play text shards (generate with run_gendata.sh first) or an
# already-converted bulletformat .data file (e.g. public PlentyChess data, which is how the shipped nets
# were trained — train_pipeline.sh auto-detects which). The SPRT defaults to candidate-vs-HCE; set
# BASELINE_NET to compare two nets instead (see run_sprt.sh for TC / cross-width options).
#
# Usage: run_experiment.sh <data_dir | shuffled.data> <net.bin> [superbatches] [sprt_rounds] [depth]
#
# Full from-scratch self-play flow:
#   tools/setup_bullet.sh                                   # once: clone+register bullet
#   make tools && make check                                # build gen_data and the test exes
#   tools/run_gendata.sh ~/pawnstar_nnue/data 12 100000 8   # generate (stop with: pkill -f gen_data)
#   tools/run_experiment.sh ~/pawnstar_nnue/data net.bin    # openings + train + verify + SPRT
set -euo pipefail

DATA_IN="${1:?data_dir or shuffled.data}"
NET="${2:?net.bin}"
SBS="${3:-40}"
ROUNDS="${4:-500}"
DEPTH="${5:-8}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"

# Openings need text shards. With a pre-converted .data file, supply your own openings.epd instead.
if [ -d "$DATA_IN" ]; then
    bash "$REPO/tools/make_openings.sh" "$DATA_IN"
    OPENINGS="$DATA_IN/openings.epd"
else
    OPENINGS="${OPENINGS:?with bulletformat data, set OPENINGS=<openings.epd>}"
fi

bash "$REPO/tools/train_pipeline.sh" "$DATA_IN" "$NET" "$SBS"
bash "$REPO/tools/verify_net.sh" "$NET"
bash "$REPO/tools/run_sprt.sh" "$NET" "$OPENINGS" "$ROUNDS" "$DEPTH"
