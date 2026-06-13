#!/usr/bin/env bash
# Set up the bullet NNUE trainer for Pawnstar with minimal fuss: clone bullet at the pinned commit,
# install our trainer + reference-evaluator examples, and register them in bullet's Cargo.toml.
# Idempotent: safe to re-run.
#
# Usage: setup_bullet.sh
# Env:   BULLET   bullet checkout location (default ~/pawnstar_nnue/bullet)
#
# Prerequisites: git, Rust/cargo. For GPU training also a CUDA toolkit (CUDA_PATH) and a driver new
# enough for the toolkit (bullet's CUDA backend uses cuFuncLoad -> needs driver for CUDA >= 12.3).
set -euo pipefail

# Pinned bullet commit that our examples/pawnstar*.rs target (bullet's API changes over time).
BULLET_COMMIT=8893e4891781f4da2161dfbab0a776c156e22ead
BULLET="${BULLET:-$HOME/pawnstar_nnue/bullet}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"

if [ ! -d "$BULLET/.git" ]; then
    echo "[setup] cloning bullet -> $BULLET"
    mkdir -p "$(dirname "$BULLET")"
    git clone https://github.com/jw1912/bullet "$BULLET"
fi
echo "[setup] checking out pinned commit $BULLET_COMMIT"
git -C "$BULLET" fetch --quiet origin "$BULLET_COMMIT" 2>/dev/null || true
git -C "$BULLET" checkout --quiet "$BULLET_COMMIT"

echo "[setup] installing example sources"
cp "$REPO/tools/bullet/pawnstar.rs" "$BULLET/examples/pawnstar.rs"
cp "$REPO/tools/bullet/pawnstar_eval.rs" "$BULLET/examples/pawnstar_eval.rs"

CARGO="$BULLET/crates/bullet_lib/Cargo.toml"
if ! grep -q 'name = "pawnstar"' "$CARGO"; then
    echo "[setup] registering examples in $CARGO"
    cat >> "$CARGO" <<'EOF'

[[example]]
name = "pawnstar"
path = "../../examples/pawnstar.rs"

[[example]]
name = "pawnstar_eval"
path = "../../examples/pawnstar_eval.rs"
EOF
fi

echo "[setup] bullet ready at $BULLET (commit $BULLET_COMMIT)"
echo "[setup] build GPU trainer:  (cd $BULLET/crates/bullet_lib && CUDA_PATH=\$CUDA_PATH cargo build --release --features cuda --example pawnstar --example pawnstar_eval)"
echo "[setup] build CPU trainer:  (cd $BULLET/crates/bullet_lib && cargo build --release --example pawnstar --example pawnstar_eval)"
