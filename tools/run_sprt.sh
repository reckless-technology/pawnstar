#!/usr/bin/env bash
# Run an SPRT / fixed match of NNUE vs the handcrafted eval, using the same pawnstar binary with
# different UCI options. Requires fastchess on PATH and a built ./build/pawnstar.
#
# Usage: run_sprt.sh <net.bin> <openings.epd> [rounds] [depth]
#   rounds: number of opening rounds; total games = rounds * 2 (colours reversed). Default 500.
#   depth:  fixed search depth for both engines (isolates eval quality from speed). Default 8.
set -euo pipefail

NET="${1:?net.bin}"
OPENINGS="${2:?openings.epd}"
ROUNDS="${3:-500}"
DEPTH="${4:-8}"

REPO="$(cd "$(dirname "$0")/.." && pwd)"
PS="$REPO/build/pawnstar"

# Single-threaded engines: faster per node, avoids Lazy SMP non-determinism, frees cores for concurrency.
export PAWNSTAR_THREADS=1
# The NNUE/HCE distinction must come ONLY from the per-engine UCI options below; make sure neither side
# inherits a global NNUE setting from the environment.
unset PAWNSTAR_NNUE PAWNSTAR_EVALFILE

fastchess \
    -engine cmd="$PS" name=NNUE option.UseNNUE=true option.EvalFile="$NET" \
    -engine cmd="$PS" name=HCE  option.UseNNUE=false \
    -each proto=uci depth="$DEPTH" \
    -rounds "$ROUNDS" -games 2 -repeat \
    -openings file="$OPENINGS" format=epd order=random \
    -draw movenumber=40 movecount=8 score=20 -resign movecount=3 score=600 \
    -sprt elo0=0 elo1=10 alpha=0.05 beta=0.05 \
    -concurrency "$(nproc)"
