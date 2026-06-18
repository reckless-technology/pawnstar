#!/usr/bin/env bash
# Runs ON the EC2 instance (uploaded by launch_training.sh). Sets up the toolchain, fetches PlentyChess
# data, and trains a net with the repo's existing pipeline. Staged so each step can be run/reviewed alone.
#
# Stages (env RUN=comma,list — default "setup"):
#   setup  verify GPU/CUDA, install Rust, build bullet (tools/setup_bullet.sh)
#   data   download + validate + concat + shuffle PlentyChess shards -> ~/pawnstar_nnue/shuffled.data
#   train  train_pipeline.sh on the data -> $OUT_NET, then verify_net.sh
#
# Env:
#   RUN        stages to run                          (default: setup)
#   SHARDS     space list of PlentyChess shard ids    (default: a small screen set; see README for the
#              full v8 pool). Each big shard is ~11-12 GB / ~370M positions.
#   SBS        superbatches to train                  (default: 40; v8 used ~120 on the full pool)
#   OUT_NET    output net path                        (default: ~/pawnstar_nnue/out.bin)
#   BULLET_FEATURES   set to "" to force CPU training (default: --features cuda)
set -euo pipefail

RUN="${RUN:-setup}"
REPO="$HOME/pawnstar"
WORK="$HOME/pawnstar_nnue"
DATA="$WORK/shuffled.data"
OUT_NET="${OUT_NET:-$WORK/out.bin}"
SBS="${SBS:-40}"
SHARDS="${SHARDS:-11128}"   # one shard ~370M positions: a quick screen. Add more for a real net.
BASE="https://huggingface.co/datasets/Yoshie2000/plentychess_data_bulletformat/resolve/main"
export BULLET="${BULLET:-$WORK/bullet}"
# The Deep Learning AMI ships CUDA at /usr/local/cuda; train_pipeline.sh otherwise defaults CUDA_PATH to
# ~/cuda-12.2 (wrong here). Point it at the real toolkit so bullet's --features cuda build/link works.
if [ -d /usr/local/cuda ]; then
    export CUDA_PATH="${CUDA_PATH:-/usr/local/cuda}"
    export PATH="$CUDA_PATH/bin:$PATH"
    export LD_LIBRARY_PATH="$CUDA_PATH/lib64:${LD_LIBRARY_PATH:-}"
fi
mkdir -p "$WORK"
has() { case ",$RUN," in *",$1,"*) return 0;; *) return 1;; esac; }

if has setup; then
    echo "=== [setup] GPU / CUDA ==="
    if command -v nvidia-smi >/dev/null; then nvidia-smi -L; nvidia-smi --query-gpu=driver_version --format=csv,noheader
    else echo "WARNING: no nvidia-smi (CPU-only box?) — set BULLET_FEATURES='' to train on CPU"; fi
    echo "=== [setup] Rust ==="
    command -v cargo >/dev/null || { curl -fsSL https://sh.rustup.rs | sh -s -- -y; }
    # shellcheck disable=SC1090
    [ -f "$HOME/.cargo/env" ] && . "$HOME/.cargo/env"
    # Resize the root FS in case cloud-init didn't grow it to the requested EBS size.
    sudo growpart /dev/nvme0n1 1 2>/dev/null || sudo growpart /dev/xvda 1 2>/dev/null || true
    sudo resize2fs "$(findmnt -no SOURCE /)" 2>/dev/null || true
    echo "=== [setup] bullet ==="
    bash "$REPO/tools/setup_bullet.sh"
    echo "[setup] done. df -h /:"; df -h / | tail -1
fi

if has data; then
    echo "=== [data] downloading shards: $SHARDS ==="
    . "$HOME/.cargo/env" 2>/dev/null || true
    ( cd "$BULLET/crates/utils" && cargo build --release )
    UTILS="$BULLET/target/release/bullet-utils"
    cd "$WORK"
    for sh in $SHARDS; do
        echo "  -> $sh.data"; curl -L --fail -C - -o "$sh.data" "$BASE/$sh.data"
        # Hard gate: a bulletformat record is 32 bytes, so a non-multiple size is unambiguously corrupt.
        sz=$(stat -c%s "$sh.data"); [ $((sz % 32)) -eq 0 ] || { echo "CORRUPT (size%32) $sh.data"; rm -f "$sh.data"; exit 1; }
        # Best-effort content check (warn only — don't delete on a CLI-syntax mismatch; training fails loud
        # on a truly bad record anyway). Some shards are known corrupt (e.g. 11496.data).
        "$UTILS" validate "$sh.data" 2>/dev/null || echo "  WARNING: bullet-utils could not validate $sh.data (check manually)"
    done
    echo "=== [data] concat + shuffle -> $DATA ==="
    cat $(for s in $SHARDS; do [ -f "$s.data" ] && echo "$s.data"; done) > all.data
    "$UTILS" shuffle --input all.data --output "$DATA" --mem-used-mb 4096
    rm -f all.data
    echo "[data] positions: $(( $(stat -c%s "$DATA") / 32 ))"
fi

if has train; then
    echo "=== [train] train_pipeline.sh ($SBS superbatches) ==="
    . "$HOME/.cargo/env" 2>/dev/null || true
    [ -f "$DATA" ] || { echo "error: $DATA missing — run RUN=data first"; exit 1; }
    bash "$REPO/tools/train_pipeline.sh" "$DATA" "$OUT_NET" "$SBS"
    echo "=== [train] verify_net.sh ==="
    bash "$REPO/tools/verify_net.sh" "$OUT_NET" || echo "WARNING: verify failed — check arch/quantisation match"
    echo "[train] net written: $OUT_NET"; ls -la "$OUT_NET"
    echo "[train] fetch it: scp from this host to your machine, then SPRT locally."
fi

echo "done (stages: $RUN)"
