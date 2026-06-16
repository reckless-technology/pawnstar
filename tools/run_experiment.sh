#!/usr/bin/env bash
# One-shot NNUE experiment: train the net, VERIFY it, then run an SPRT.
#
# The data source is an already-converted bulletformat .data file (e.g. public PlentyChess data, which is
# how the shipped nets were trained). NNUE is the only evaluator, so the SPRT is net-vs-net: set
# BASELINE_NET=<baseline net.bin> to choose the opponent (see run_sprt.sh for TC / cross-width options).
#
# Usage: OPENINGS=<openings.epd> run_experiment.sh <shuffled.data> <net.bin> [superbatches] [sprt_rounds] [depth]
#
# Typical flow:
#   tools/setup_bullet.sh                                                  # once: clone+register bullet
#   OPENINGS=~/openings.epd \
#     tools/run_experiment.sh ~/pawnstar_nnue/shuffled.data net.bin        # train + verify + SPRT
set -euo pipefail

DATA_IN="${1:?shuffled.data}"   # pre-converted bulletformat .data file
NET="${2:?net.bin}"             # output net path (also the SPRT candidate)
SBS="${3:-40}"                  # superbatches to train (passed to train_pipeline.sh)
ROUNDS="${4:-500}"              # SPRT rounds
DEPTH="${5:-8}"                 # SPRT fixed search depth (ignored if run_sprt.sh sees TC)
REPO="$(cd "$(dirname "$0")/.." && pwd)"

# A .data file carries no FENs we can sample, so the caller supplies their own SPRT opening book.
OPENINGS="${OPENINGS:?set OPENINGS=<openings.epd>}"

# The three pipeline stages, in order. Any failure aborts the run (set -e), so a bad net never reaches SPRT.
bash "$REPO/tools/train_pipeline.sh" "$DATA_IN" "$NET" "$SBS"   # train + quantise + export
bash "$REPO/tools/verify_net.sh" "$NET"                         # gate: engine eval == trainer eval
bash "$REPO/tools/run_sprt.sh" "$NET" "$OPENINGS" "$ROUNDS" "$DEPTH"  # net-vs-baseline SPRT
