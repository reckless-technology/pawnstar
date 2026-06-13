#!/usr/bin/env bash
# One-shot NNUE experiment from an existing self-play dataset: build openings, train the net, and run
# the SPRT vs the handcrafted eval. (Generate data first with run_gendata.sh.)
#
# Usage: run_experiment.sh <data_dir> <net.bin> [superbatches] [sprt_rounds] [depth]
#
# Full from-scratch flow:
#   tools/setup_bullet.sh                                   # once: clone+register bullet
#   make tools                                              # build gen_data
#   tools/run_gendata.sh ~/pawnstar_nnue/data 12 100000 8   # generate (stop with: pkill -x gen_data)
#   tools/run_experiment.sh ~/pawnstar_nnue/data net.bin    # openings + train + SPRT
set -euo pipefail

DATA_DIR="${1:?data_dir}"
NET="${2:?net.bin}"
SBS="${3:-40}"
ROUNDS="${4:-500}"
DEPTH="${5:-8}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"

bash "$REPO/tools/make_openings.sh" "$DATA_DIR"
bash "$REPO/tools/train_pipeline.sh" "$DATA_DIR" "$NET" "$SBS"
bash "$REPO/tools/run_sprt.sh" "$NET" "$DATA_DIR/openings.epd" "$ROUNDS" "$DEPTH"
