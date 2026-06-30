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
#   CAND_STARTUP_NET  for a cross-arch candidate (BIN_A) whose default/embedded net is incompatible: a net
#                  to give BIN_A at *startup* (via PAWNSTAR_EVALFILE) so it survives the UCI handshake.
#                  Without it, such a candidate fatal-exits before the per-engine EvalFile is applied.
#   ELO0, ELO1     SPRT hypothesis bounds (default 0 / 10).
set -euo pipefail

NET="${1:?net.bin}"
OPENINGS="${2:?openings.epd}"
ROUNDS="${3:-500}"
DEPTH="${4:-8}"
BASELINE_NET="${BASELINE_NET:?set BASELINE_NET=<baseline net.bin> (NNUE is the only evaluator; SPRT is net-vs-net)}"

REPO="$(cd "$(dirname "$0")/.." && pwd)"
# The engine binary is version-named (pawnstar_<major>_<minor>_<build>); default to the most recently built
# one. Override with BIN_A / BIN_B to A/B two specific versions.
PS="$(ls -t "$REPO"/build/pawnstar_* 2>/dev/null | head -n1)"
PS="${PS:-$REPO/build/pawnstar}"
BIN_A="${BIN_A:-$PS}"
BIN_B="${BIN_B:-$PS}"
ELO0="${ELO0:-0}"
ELO1="${ELO1:-10}"

# Single-threaded engines: faster per node, avoids Lazy SMP non-determinism, frees cores for concurrency.
export PAWNSTAR_THREADS=1
# The net distinction must come ONLY from the per-engine EvalFile options below; make sure neither side
# inherits a net path from the environment.
unset PAWNSTAR_NNUE PAWNSTAR_EVALFILE

# Cross-arch SPRT: when BIN_A is built for a *different* architecture than its default/embedded net, it
# rejects that net and fatal-exits during the UCI handshake — before fastchess can send setoption EvalFile.
# Set CAND_STARTUP_NET=<net for BIN_A> to wrap the candidate so it loads a compatible net at *startup* (via
# PAWNSTAR_EVALFILE); the per-engine EvalFile below still applies normally. The baseline (BIN_B) is assumed
# to start fine on its own embedded/default net. Unset => behaviour is unchanged.
CAND_CMD="$BIN_A"
if [ -n "${CAND_STARTUP_NET:-}" ]; then
    CAND_CMD="$(mktemp "${TMPDIR:-/tmp}/sprt_cand_wrapper.XXXXXX.sh")"
    printf '#!/bin/sh\nexec env PAWNSTAR_EVALFILE=%q %q "$@"\n' "$CAND_STARTUP_NET" "$BIN_A" > "$CAND_CMD"
    chmod +x "$CAND_CMD"
    trap 'rm -f "$CAND_CMD"' EXIT
fi

# Per-engine clock spec: a fixed search depth (isolates eval quality) unless TC is set (measures real
# strength including the speed cost of a heavier net). Built as an array so it expands to one -each token.
if [ -n "${TC:-}" ]; then
    tc_args=(tc="$TC")
else
    tc_args=(depth="$DEPTH")
fi

# The two engines differ ONLY by their EvalFile (the net). -games 2 -repeat plays each opening twice with
# colours reversed; the draw/resign adjudication ends decided games early to save time without biasing the
# result; -sprt stops as soon as elo0 vs elo1 is decided at the given alpha/beta error rates.
fastchess \
    -engine cmd="$CAND_CMD" name=cand option.EvalFile="$NET" \
    -engine cmd="$BIN_B" name=base option.EvalFile="$BASELINE_NET" \
    -each proto=uci "${tc_args[@]}" \
    -rounds "$ROUNDS" -games 2 -repeat \
    -openings file="$OPENINGS" format=epd order=random \
    -draw movenumber=40 movecount=8 score=20 -resign movecount=3 score=600 \
    -sprt elo0="$ELO0" elo1="$ELO1" alpha=0.05 beta=0.05 \
    -concurrency "$(nproc)"
