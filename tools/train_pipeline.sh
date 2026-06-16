#!/usr/bin/env bash
# End-to-end NNUE training pipeline. Trains a quantised net and exports it to a path the engine loads.
#
# Two input modes (auto-detected from the first argument):
#   * a DIRECTORY of self-play text shards (data_*.txt / seed*.txt from run_gendata.sh) -> combined,
#     converted to bulletformat, and shuffled before training; or
#   * an already-converted bulletformat .data FILE (e.g. public PlentyChess data, already shuffled) ->
#     trained directly, skipping the combine/convert/shuffle steps.
#
# The shipped nets (nnue/pawnstar-v4.bin and its predecessors from v2 on) were trained the second way,
# on public PlentyChess bulletformat data — strong-engine label quality, not self-play volume, is what
# made NNUE beat the handcrafted eval. Self-play text was only the original bootstrap source.
#
# Usage: train_pipeline.sh <data_dir | shuffled.data> <out_net.bin> [end_superbatch]
#
# The net width is fixed by the trainer example (tools/bullet/pawnstar.rs, HIDDEN_SIZE) and MUST match
# the engine's nnue::kHiddenSize (512 for v4). setup_bullet.sh installs/builds that example.
#
# Prerequisites: CUDA toolkit at $CUDA_PATH, bullet cloned at $BULLET (default ~/pawnstar_nnue/bullet).
set -euo pipefail

DATA_IN="${1:?data_dir or shuffled.data}"
OUT_NET="${2:?out_net.bin}"
END_SB="${3:-40}"

BULLET="${BULLET:-$HOME/pawnstar_nnue/bullet}"
REPO="$(cd "$(dirname "$0")" && pwd)/.."
# Backend: GPU by default; set BULLET_FEATURES="" to train on CPU (no CUDA needed).
FEATURES="${BULLET_FEATURES---features cuda}"
export CUDA_PATH="${CUDA_PATH:-$HOME/cuda-12.2}"
export PATH="$CUDA_PATH/bin:$PATH"
export LD_LIBRARY_PATH="$CUDA_PATH/lib64:${LD_LIBRARY_PATH:-}"

echo "[1/4] ensuring bullet is set up, building utils + trainer ($FEATURES)"
bash "$REPO/tools/setup_bullet.sh"
( cd "$BULLET/crates/utils" && cargo build --release )
( cd "$BULLET/crates/bullet_lib" && cargo build --release $FEATURES --example pawnstar )
UTILS="$BULLET/target/release/bullet-utils"

if [ -f "$DATA_IN" ] && [[ "$DATA_IN" == *.data ]]; then
    # Already bulletformat (e.g. public PlentyChess data). Train on it directly.
    echo "[2/4] using pre-converted bulletformat data: $DATA_IN"
    SHUF_DATA="$DATA_IN"
    WORK="$(dirname "$DATA_IN")"
else
    # A directory of self-play text shards: combine, convert, shuffle.
    WORK="$DATA_IN"
    ALL_TXT="$WORK/all.txt"
    ALL_DATA="$WORK/all.data"
    SHUF_DATA="$WORK/shuffled.data"
    echo "[2/4] combining self-play shards -> $ALL_TXT"
    cat "$WORK"/data_*.txt "$WORK"/seed*.txt 2>/dev/null > "$ALL_TXT"
    wc -l "$ALL_TXT"
    echo "[2b/4] convert text -> bulletformat, then shuffle (self-play data is highly correlated)"
    "$UTILS" convert --from text --input "$ALL_TXT" --output "$ALL_DATA" --threads 8
    ( cd "$WORK" && "$UTILS" shuffle --input "$ALL_DATA" --output "$SHUF_DATA" --mem-used-mb 4096 )
fi

# ChessBoard records are 32 bytes; one superbatch ~= one pass over the data.
COUNT=$(( $(stat -c%s "$SHUF_DATA") / 32 ))
BPS=$(( COUNT / 16384 ))
[ "$BPS" -lt 1 ] && BPS=1
CKPT="$WORK/checkpoints"
echo "[3/4] training: $COUNT positions, batches_per_superbatch=$BPS, end_superbatch=$END_SB"

cd "$BULLET/crates/bullet_lib"
PAWNSTAR_DATA="$SHUF_DATA" PAWNSTAR_BPS="$BPS" PAWNSTAR_SBS="$END_SB" PAWNSTAR_OUT="$CKPT" \
    cargo run --release $FEATURES --example pawnstar

echo "[4/4] exporting net -> $OUT_NET"
cp "$CKPT/pawnstar-$END_SB/quantised.bin" "$OUT_NET"
ls -la "$OUT_NET"
echo "done. Verify with: tools/verify_net.sh $OUT_NET"
echo "Then load with: setoption name EvalFile value $OUT_NET"
