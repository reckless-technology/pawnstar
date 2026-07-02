#!/usr/bin/env bash
set -uo pipefail
cd ~/pawnstar_nnue/cand_engine
export PATH="$HOME/.local/bin:$PATH"
echo "[$(date +%T)] building candidate engine (1024 single-bank) + tests + tools"
make clean >/dev/null 2>&1
make -j"$(nproc)" 2>&1 | tail -3
make -j"$(nproc)" tests tools 2>&1 | tail -3
echo "[$(date +%T)] CAND BUILD DONE"
ls -la build/pawnstar build/test_nnue build/test_nnue_incremental build/stamp_net 2>&1
