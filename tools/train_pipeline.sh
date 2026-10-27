#!/usr/bin/env bash
# End-to-end NNUE training pipeline. Trains a quantised net and exports it to a path the engine loads.
#
# Two input modes (auto-detected from the first argument):
#   * a DIRECTORY of text shards (data_*.txt / seed*.txt in bullet's "FEN | eval | wdl" text format)
#     -> combined, CHECKED FOR DUPLICATE POSITIONS (tools/check_duplicates.sh: a pool that is mostly copies
#     of the same games is refused here rather than trained on, see [2/4]),
#     converted to bulletformat, and shuffled before training; or
#   * an already-converted bulletformat .data FILE (e.g. a public dataset, already shuffled) ->
#     trained directly, skipping the combine/convert/shuffle steps.
#
# The shipped net (nnue/pawnstar-v12.bin) and its predecessors from v2 on were trained the second way,
# on the public PlentyChess bulletformat dataset — strong-engine label quality, not self-play volume, is what
# made NNUE beat the handcrafted eval. Self-play text was only the original bootstrap source.
#
# Usage: train_pipeline.sh <data_dir | shuffled.data> <out_net.bin> [end_superbatch]
#
# The net width is fixed by the trainer example (tools/bullet/pawnstar.rs, HIDDEN_SIZE) and MUST match
# the engine's nnue::kHiddenSize (1024 for v12). setup_bullet.sh installs/builds that example.
#
# Prerequisites: CUDA toolkit at $CUDA_PATH, bullet cloned at $BULLET (default ~/pawnstar_nnue/bullet; under
# %USERPROFILE% on Windows, as MSYS2 has a ~ of its own).
set -euo pipefail

DATA_IN="${1:?data_dir or shuffled.data}"
OUT_NET="${2:?out_net.bin}"
END_SB="${3:-40}"

BULLET="${BULLET:-${USERPROFILE:-$HOME}/pawnstar_nnue/bullet}"
REPO="$(cd "$(dirname "$0")" && pwd)/.."
# Backend: GPU by default; set BULLET_FEATURES="" to train on CPU (no CUDA needed). The `---features cuda`
# default form means "unset -> --features cuda, but empty-string -> empty" so BULLET_FEATURES="" disables it.
FEATURES="${BULLET_FEATURES---features cuda}"
export CUDA_PATH="${CUDA_PATH:-/usr/local/cuda}"
if command -v cygpath >/dev/null 2>&1; then
    # MSYS2 / Git Bash on Windows: the CUDA installer sets CUDA_PATH as a Windows path (C:\Program Files\NVIDIA GPU
    # Computing Toolkit\CUDA\v12.x), which cannot go on a POSIX PATH as it is. Put its bin on the PATH converted, so
    # nvcc is found whatever the Windows PATH the installer edited reaches this shell; CUDA_PATH itself stays the
    # Windows path, which the cuda crates' build scripts read. Windows finds the CUDA DLLs on the PATH, not
    # LD_LIBRARY_PATH.
    export PATH="$(cygpath -u "$CUDA_PATH")/bin:$PATH"
else
    export PATH="$CUDA_PATH/bin:$PATH"
    export LD_LIBRARY_PATH="$CUDA_PATH/lib64:${LD_LIBRARY_PATH:-}"
fi

# A file's size in bytes on any platform: GNU stat (Linux, MSYS2), BSD stat (macOS), else wc.
file_size() {
    stat -c%s "$1" 2>/dev/null || stat -f%z "$1" 2>/dev/null || wc -c <"$1"
}

# [1/4] Make sure bullet is checked out at the pinned commit and build the data utils + the trainer example.
echo "[1/4] ensuring bullet is set up, building utils + trainer ($FEATURES)"
bash "$REPO/tools/setup_bullet.sh"
( cd "$BULLET/crates/utils" && cargo build --release )
( cd "$BULLET/crates/bullet_lib" && cargo build --release $FEATURES --example pawnstar )
UTILS="$BULLET/target/release/bullet-utils"

# [2/4] Produce a single shuffled bulletformat file (SHUF_DATA) for the trainer, by whichever route fits the
# input. WORK is the scratch/output directory (checkpoints land under it).
if [ -f "$DATA_IN" ] && [[ "$DATA_IN" == *.data ]]; then
    # Already bulletformat (e.g. a public dataset, pre-shuffled). Train on it directly.
    echo "[2/4] using pre-converted bulletformat data: $DATA_IN"
    SHUF_DATA="$DATA_IN"
    WORK="$(dirname "$DATA_IN")"
else
    # A directory of self-play text shards: concatenate -> convert to bulletformat -> shuffle. Shuffling
    # matters because consecutive self-play positions are from the same game and highly correlated.
    WORK="$DATA_IN"
    ALL_TXT="$WORK/all.txt"
    ALL_DATA="$WORK/all.data"
    SHUF_DATA="$WORK/shuffled.data"
    echo "[2/4] combining self-play shards -> $ALL_TXT"
    # Gather whichever shard names are actually there. The two globs are both optional - tools/run_gendata.sh
    # writes data_<i>.txt and the original bootstrap wrote seed*.txt - and `cat` exits 1 on an operand that
    # does not exist, so passing an unmatched glob straight to it aborted the whole script under `set -e`,
    # after all.txt had been written and with the message swallowed by 2>/dev/null: the run just stopped.
    # Collect the files first and say so plainly when there are none.
    SHARDS=()
    for shard in "$WORK"/data_*.txt "$WORK"/seed*.txt; do
        [ -f "$shard" ] && SHARDS+=("$shard")
    done
    if [ "${#SHARDS[@]}" -eq 0 ]; then
        echo "[2/4] no data_*.txt or seed*.txt shards in $WORK" >&2
        exit 1
    fi
    echo "[2/4] ${#SHARDS[@]} shard(s)"
    cat "${SHARDS[@]}" > "$ALL_TXT"
    wc -l "$ALL_TXT"
    # The duplicate gate. gen_data deduplicates positions PER SHARD, so a campaign that generated the same
    # games in several workers produces shards that each look healthy and a pool that is mostly copies -
    # which trains to nothing and is invisible in the shard sizes. Measure it here, where the pool first
    # exists, rather than after a week of CPU and a GPU run. ALLOW_DUPLICATE_POOL=1 trains anyway (for a
    # deliberately narrow experiment); MIN_UNIQUE_FRACTION moves the gate.
    # Measured over the shard DIRECTORY rather than over all.txt: on a pool too large to read exactly the
    # check samples per shard, which keeps every worker in scope (one concatenated file would sample only
    # whichever shards happen to be at the front of it), and the shards' .meta sidecars give it the game count.
    if ! bash "$REPO/tools/check_duplicates.sh" "$WORK" "${MIN_UNIQUE_FRACTION:-0.80}"; then
        if [ "${ALLOW_DUPLICATE_POOL:-0}" != "1" ]; then
            echo "[2/4] refusing to train on this pool; set ALLOW_DUPLICATE_POOL=1 to override" >&2
            exit 1
        fi
        echo "[2/4] ALLOW_DUPLICATE_POOL=1: training on it anyway" >&2
    fi
    echo "[2b/4] convert text -> bulletformat, then shuffle (self-play data is highly correlated)"
    "$UTILS" convert --from text --input "$ALL_TXT" --output "$ALL_DATA" --threads 8
    # bullet-utils shuffle now spills to "<output>.tmp" (it used to be ./tmp under the working directory)
    # and insists on creating that directory itself, so a killed shuffle leaves a directory behind that
    # makes every later run fail before it reads a byte. Clear it first - it holds nothing we want.
    rm -rf "${SHUF_DATA:?}.tmp"
    ( cd "$WORK" && "$UTILS" shuffle --input "$ALL_DATA" --output "$SHUF_DATA" --mem-used-mb 4096 )
fi

# Each bulletformat ChessBoard record is 32 bytes, so file_size/32 is the position count. bullet measures
# training in superbatches; ~16384 batches per superbatch makes one superbatch roughly one pass over the
# data. Clamp to >=1 for tiny datasets.
COUNT=$(( $(file_size "$SHUF_DATA") / 32 ))
BPS=$(( COUNT / 16384 ))
[ "$BPS" -lt 1 ] && BPS=1
CKPT="$WORK/checkpoints"
echo "[3/4] training: $COUNT positions, batches_per_superbatch=$BPS, end_superbatch=$END_SB"

# The pawnstar example reads its config from PAWNSTAR_* env vars (data path, batches/superbatch, end
# superbatch, checkpoint output dir) — there are no CLI flags for these.
cd "$BULLET/crates/bullet_lib"
PAWNSTAR_DATA="$SHUF_DATA" PAWNSTAR_BPS="$BPS" PAWNSTAR_SBS="$END_SB" PAWNSTAR_OUT="$CKPT" \
    cargo run --release $FEATURES --example pawnstar

# [4/4] bullet writes a checkpoint dir per superbatch; the final one holds the quantised net we ship.
echo "[4/4] exporting net -> $OUT_NET"
cp "$CKPT/pawnstar-$END_SB/quantised.bin" "$OUT_NET"
ls -la "$OUT_NET"
echo "done. Verify with: tools/verify_net.sh $OUT_NET"
echo "Then load with: setoption name EvalFile value $OUT_NET"
