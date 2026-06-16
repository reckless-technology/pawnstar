#!/usr/bin/env bash
# Run an SPRT between two NNUE nets with fastchess. Requires fastchess on PATH.
#
# NNUE is Pawnstar's only evaluator (the handcrafted eval was removed), so an SPRT always compares two
# nets: the candidate ($NET) against a baseline ($BASELINE_NET). Net selection is purely via the EvalFile
# UCI option.
#
# Usage: run_sprt.sh <net.bin> <openings.epd> [rounds] [depth]
# Env:
#   BASELINE_NET   REQUIRED: the opponent net (NNUE with this net).
#   TC             time control (e.g. "8+0.08"); if set, used instead of fixed depth. Fixed depth
#                  isolates eval quality; time control measures real strength incl. the speed cost of a
#                  wider/heavier net.
#   BIN_A, BIN_B   engine binaries for the candidate / baseline (default ./build/pawnstar for both).
#                  Different net widths need different builds (kHiddenSize is compile-time), so a
#                  cross-width SPRT points BIN_A and BIN_B at the two separately-built binaries.
#   ELO0, ELO1     SPRT hypothesis bounds (default 0 / 10).
set -euo pipefail

NET="${1:?net.bin}"
OPENINGS="${2:?openings.epd}"
ROUNDS="${3:-500}"
DEPTH="${4:-8}"
BASELINE_NET="${BASELINE_NET:?set BASELINE_NET=<baseline net.bin> (NNUE is the only evaluator; SPRT is net-vs-net)}"

REPO="$(cd "$(dirname "$0")/.." && pwd)"
PS="$REPO/build/pawnstar"
BIN_A="${BIN_A:-$PS}"
BIN_B="${BIN_B:-$PS}"
ELO0="${ELO0:-0}"
ELO1="${ELO1:-10}"

# Single-threaded engines: faster per node, avoids Lazy SMP non-determinism, frees cores for concurrency.
export PAWNSTAR_THREADS=1
# The net distinction must come ONLY from the per-engine EvalFile options below; make sure neither side
# inherits a net path from the environment.
unset PAWNSTAR_NNUE PAWNSTAR_EVALFILE

# Fixed depth by default; time control if TC is set.
if [ -n "${TC:-}" ]; then
    tc_args=(tc="$TC")
else
    tc_args=(depth="$DEPTH")
fi

fastchess \
    -engine cmd="$BIN_A" name=cand option.EvalFile="$NET" \
    -engine cmd="$BIN_B" name=base option.EvalFile="$BASELINE_NET" \
    -each proto=uci "${tc_args[@]}" \
    -rounds "$ROUNDS" -games 2 -repeat \
    -openings file="$OPENINGS" format=epd order=random \
    -draw movenumber=40 movecount=8 score=20 -resign movecount=3 score=600 \
    -sprt elo0="$ELO0" elo1="$ELO1" alpha=0.05 beta=0.05 \
    -concurrency "$(nproc)"
