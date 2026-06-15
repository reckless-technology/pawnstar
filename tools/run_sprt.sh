#!/usr/bin/env bash
# Run an SPRT between two engine configurations with fastchess. Requires fastchess on PATH.
#
# Default comparison is the candidate net vs the handcrafted eval (HCE). Set BASELINE_NET to instead
# compare two nets (the architecture program's real test, e.g. v5(1024) vs v4(512)) — NNUE now beats
# HCE and ships on by default, so net-vs-net is the usual question.
#
# Usage: run_sprt.sh <net.bin> <openings.epd> [rounds] [depth]
# Env:
#   BASELINE_NET   if set, opponent is NNUE with this net; otherwise opponent is HCE (UseNNUE=false).
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

REPO="$(cd "$(dirname "$0")/.." && pwd)"
PS="$REPO/build/pawnstar"
BIN_A="${BIN_A:-$PS}"
BIN_B="${BIN_B:-$PS}"
ELO0="${ELO0:-0}"
ELO1="${ELO1:-10}"

# Single-threaded engines: faster per node, avoids Lazy SMP non-determinism, frees cores for concurrency.
export PAWNSTAR_THREADS=1
# The eval distinction must come ONLY from the per-engine UCI options below; make sure neither side
# inherits a global NNUE setting from the environment.
unset PAWNSTAR_NNUE PAWNSTAR_EVALFILE

# Candidate is always NNUE with $NET. Baseline is a second net if BASELINE_NET is set, else HCE.
if [ -n "${BASELINE_NET:-}" ]; then
    baseline=(name=base option.UseNNUE=true "option.EvalFile=$BASELINE_NET")
else
    baseline=(name=HCE option.UseNNUE=false)
fi

# Fixed depth by default; time control if TC is set.
if [ -n "${TC:-}" ]; then
    tc_args=(tc="$TC")
else
    tc_args=(depth="$DEPTH")
fi

fastchess \
    -engine cmd="$BIN_A" name=cand option.UseNNUE=true option.EvalFile="$NET" \
    -engine cmd="$BIN_B" "${baseline[@]}" \
    -each proto=uci "${tc_args[@]}" \
    -rounds "$ROUNDS" -games 2 -repeat \
    -openings file="$OPENINGS" format=epd order=random \
    -draw movenumber=40 movecount=8 score=20 -resign movecount=3 score=600 \
    -sprt elo0="$ELO0" elo1="$ELO1" alpha=0.05 beta=0.05 \
    -concurrency "$(nproc)"
