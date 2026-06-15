#!/usr/bin/env bash
# Verify a freshly trained net before trusting it in an SPRT: the engine's NNUE inference must match the
# bullet trainer's own evaluation of the same weights, and the incrementally-maintained accumulator must
# stay bit-identical to a full refresh. This is the gate run_experiment.sh applies between training and
# the SPRT — a net that fails it has a format/arch mismatch and any match result would be meaningless.
#
# Usage: verify_net.sh <net.bin>
# Env:   BULLET   bullet checkout (default ~/pawnstar_nnue/bullet)
#
# Requires: a built engine + tests (make check builds them) and the bullet pawnstar_eval example
# (setup_bullet.sh installs it; train_pipeline.sh builds it). The net's width must match the engine's
# nnue::kHiddenSize and the pawnstar_eval HIDDEN — otherwise the cross-check fails by design.
set -euo pipefail

NET="${1:?net.bin}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
BULLET="${BULLET:-$HOME/pawnstar_nnue/bullet}"
EVAL="$BULLET/target/release/examples/pawnstar_eval"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "[1/2] cross-check: engine NNUE eval vs bullet pawnstar_eval (within +/-2cp)"
# Reuse the checked-in reference FENs; regenerate the expected evals for *this* net with the trainer.
cut -d'|' -f1 "$REPO/test/nnue_reference.txt" > "$WORK/fens.txt"
"$EVAL" "$NET" "$WORK/fens.txt" > "$WORK/ref.txt"
"$REPO/build/test_nnue" "$NET" "$WORK/ref.txt"

echo "[2/2] incremental accumulator == full refresh (bit-identical)"
"$REPO/build/test_nnue_incremental" "$NET"

echo "verify OK: $NET"
