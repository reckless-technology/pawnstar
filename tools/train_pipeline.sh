#!/usr/bin/env bash
# End-to-end NNUE training pipeline: combine self-play shards, convert to bullet format, shuffle, train,
# and export the quantised net to a path the engine can load.
#
# Usage: train_pipeline.sh <data_dir> <out_net.bin> [end_superbatch]
#
# Prerequisites: CUDA toolkit at $CUDA_PATH, bullet cloned at $BULLET (default ~/pawnstar_nnue/bullet),
# and self-play shards data_*.txt in <data_dir> (produced by run_gendata.sh).
set -euo pipefail

DATA_DIR="${1:?data_dir}"
OUT_NET="${2:?out_net.bin}"
END_SB="${3:-30}"

BULLET="${BULLET:-$HOME/pawnstar_nnue/bullet}"
REPO="$(cd "$(dirname "$0")" && pwd)/.."
# Backend: GPU by default; set BULLET_FEATURES="" to train on CPU (no CUDA needed).
FEATURES="${BULLET_FEATURES---features cuda}"
export CUDA_PATH="${CUDA_PATH:-$HOME/cuda-12.2}"
export PATH="$CUDA_PATH/bin:$PATH"
export LD_LIBRARY_PATH="$CUDA_PATH/lib64:${LD_LIBRARY_PATH:-}"

WORK="$DATA_DIR"
ALL_TXT="$WORK/all.txt"
ALL_DATA="$WORK/all.data"
SHUF_DATA="$WORK/shuffled.data"

echo "[1/5] combining shards -> $ALL_TXT"
# Include both freshly generated shards (data_*) and any preserved earlier shards (seed*).
cat "$DATA_DIR"/data_*.txt "$DATA_DIR"/seed*.txt 2>/dev/null > "$ALL_TXT"
wc -l "$ALL_TXT"

echo "[2/5] ensuring bullet is set up, building utils + trainer ($FEATURES)"
bash "$REPO/tools/setup_bullet.sh"
( cd "$BULLET/crates/utils" && cargo build --release )
( cd "$BULLET/crates/bullet_lib" && cargo build --release $FEATURES --example pawnstar )
UTILS="$BULLET/target/release/bullet-utils"

echo "[3/5] convert text -> bulletformat"
"$UTILS" convert --from text --input "$ALL_TXT" --output "$ALL_DATA" --threads 8

echo "[3b/5] shuffle (self-play data is highly correlated)"
( cd "$WORK" && "$UTILS" shuffle --input "$ALL_DATA" --output "$SHUF_DATA" --mem-used-mb 4096 )

# ChessBoard records are 32 bytes; one superbatch ~= one pass over the data.
COUNT=$(( $(stat -c%s "$SHUF_DATA") / 32 ))
BPS=$(( COUNT / 16384 ))
[ "$BPS" -lt 1 ] && BPS=1
echo "[4/5] training: $COUNT positions, batches_per_superbatch=$BPS, end_superbatch=$END_SB"

cd "$BULLET/crates/bullet_lib"
PAWNSTAR_DATA="$SHUF_DATA" PAWNSTAR_BPS="$BPS" PAWNSTAR_SBS="$END_SB" PAWNSTAR_OUT="$WORK/checkpoints" \
    cargo run --release $FEATURES --example pawnstar

echo "[5/5] exporting net -> $OUT_NET"
cp "$WORK/checkpoints/pawnstar-$END_SB/quantised.bin" "$OUT_NET"
ls -la "$OUT_NET"
echo "done. Load with: setoption name EvalFile value $OUT_NET ; setoption name UseNNUE value true"
