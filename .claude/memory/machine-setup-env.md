---
name: machine-setup-env
description: "Current dev machine (post-migration 2026-06-23): tool locations, GPU/CUDA, NNUE training stack — what's installed and where, after restoring from a fresh checkout."
metadata:
  node_type: memory
  type: project
  originSessionId: 9c8f1d8c-5768-4b87-83c0-07d07141f44a
---

The dev/training/testing stack was rebuilt from scratch on a NEW machine (2026-06-23, Linux Mint 22.3,
32 cores). None of it is version-controlled, so a future fresh checkout must redo this. As-restored state:

- **Memory symlink**: `~/.claude/projects/-home-jonny-work-pawnstar/memory` → `<repo>/.claude/memory`
  (the live dir was a stale empty real dir after migration; recreated per `.claude/README.md`).
- **fastchess** (the actual SPRT/rate dep — NOT cutechess; nothing in the repo uses cutechess): built from
  source (Disservin/fastchess, alpha 1.8.1), installed at `~/.local/bin/fastchess`. `~/.local/bin` was
  added to PATH via `~/.bashrc`.
- **Rust/cargo** 1.96.0 via rustup at `~/.cargo` (`. ~/.cargo/env`). Needed for the bullet trainer.
- **bullet trainer**: `tools/setup_bullet.sh` cloned it to `~/pawnstar_nnue/bullet` (pinned commit
  8893e489…), examples built with **CUDA** (`cargo build --release --features cuda --example pawnstar
  --example pawnstar_eval`). Binaries at `~/pawnstar_nnue/bullet/target/release/examples/`.
- **GPU/CUDA**: **RTX 4070 Laptop (8 GB, sm_89)**, driver 595.71.05, **CUDA 13.2** toolkit at
  `/usr/local/cuda` (set `CUDA_PATH=/usr/local/cuda`). This SUPERSEDES the stale `~/cuda-12.2` / GTX 1050 Ti
  notes in [[project_nnue_experiment]]. CUDA libs are on the loader path, so the v11 `LD_LIBRARY_PATH`
  gotcha no longer applies. The pinned (CUDA-12-era) bullet NVRTC kernels compile + run fine on CUDA 13.2 /
  sm_89. **Smoke-tested end-to-end**: trains at **~2.4M pos/sec (~7× the old 1050 Ti's ~345K)**, writes a
  `quantised.bin` of exactly 6,297,664 B = the correct v11 arch (1024 + 4 file-pair buckets).
- **clang-format**: pip pkg `clang-format==18.1.8` in venv `/home/jonny/work/.pawnstar`, symlinked to
  `~/.local/bin/clang-format`. MUST be 18.x — see [[feedback_clang_format]].
- **openings.epd**: `~/pawnstar_nnue/openings.epd` = UHO_Lichess_4852_v1 (2.63M lines, the engine-testing
  standard), downloaded fresh (the user's old curated openings file was not backed up).
- **One small data shard** for smoke tests: `~/pawnstar_nnue/smoke.data` (PlentyChess 12892.data, 61 MB,
  the smallest of the 168 shards). The full ~21 B / ~122 GB PlentyChess pool is NOT downloaded.

**Still MISSING / needs the user (no reliable auto-source):**
- **Anchor engines** for `tools/rate.sh` (the curated CCRL set in `~/.local/bin/Engines` — gaviota~2650,
  arminius~2750, toga~2950, senpai~3050, arasan~3150). Gone, no backup found; they're the user's local
  binaries passed as `name:rating:cmd`. rate.sh is only for absolute-Elo estimates — the core SPRT loop
  (net-vs-net via fastchess) does NOT need them.
- **Full PlentyChess training data** (only the 61 MB smoke shard is present).

**`tools/rate.sh` net default:** now `NET=nnue/pawnstar-v11.bin` (verified 2026-06-26 — the earlier v10
default was fixed, and no other repo script/Makefile references a retired net). See [[machine-setup-env]] siblings.
