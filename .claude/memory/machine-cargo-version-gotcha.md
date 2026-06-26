---
name: machine-cargo-version-gotcha
description: "Bullet trainer needs cargo >= ~1.85 (edition2024 manifest); the apt-installed /bin/cargo 1.75 CANNOT build it. Use the rustup cargo (absolute path) and beware a same-line `export CUDA_PATH=.. PATH=..` that silently prepends /bin."
metadata:
  node_type: memory
  type: project
---

**Two compounding traps that cost a debugging cycle (2026-06-26) when bootstrapping NNUE training on a
fresh machine.** Both surface as bullet failing with:
`error: failed to parse manifest ... feature 'edition2024' is required ... not stabilized in this version of Cargo (1.75.0)`.

**1. The apt `cargo`/`rustup` packages install Ubuntu's system cargo 1.75 at `/usr/bin/cargo` (and
`/bin/cargo`), which is too old for bullet.** Bullet's pinned commit (8893e489…) uses an `edition2024`
`Cargo.toml`, which needs a recent cargo (rustup **stable**, e.g. 1.96). `rustc`/`cargo` from
`apt install rustup cargo` is 1.75 and **cannot parse the manifest** — `cargo build` dies before compiling.
Fix: install via the official rustup script (`curl https://sh.rustup.rs | sh`), and **invoke cargo by its
absolute rustup path `$HOME/.cargo/bin/cargo`** (or `rustup run stable cargo`) in every training script, so
the system 1.75 can never shadow it. `. "$HOME/.cargo/env"; cargo --version` does normally pick the 1.96
shim, but don't rely on PATH order — see trap 2.

**2. A *combined-single-line* `export` silently prepends `/bin` to PATH and lets `/bin/cargo` (1.75) win.**
This line is BROKEN:
`export CUDA_PATH=/usr/local/cuda PATH="$CUDA_PATH/bin:$PATH" LD_LIBRARY_PATH="$CUDA_PATH/lib64:$LD_LIBRARY_PATH"`
In one simple command, all RHS are expanded against the *pre-command* environment, so `$CUDA_PATH` is still
**empty** when `$PATH` is built → `PATH="/bin:$PATH"`. `/bin/cargo` (apt 1.75) then shadows
`~/.cargo/bin/cargo` (1.96) → the edition2024 error, even though sourcing `~/.cargo/env` alone resolves to
1.96. **Use SEPARATE export statements** (set `CUDA_PATH` first, then `PATH`), which is what
`tools/setup_bullet.sh` / `nnue/README` already do:
```
export CUDA_PATH=/usr/local/cuda
export PATH="$CUDA_PATH/bin:$PATH"
export LD_LIBRARY_PATH="$CUDA_PATH/lib64:$LD_LIBRARY_PATH"
```
The diagnostic tell: `which cargo` prints `/bin/cargo` instead of `~/.cargo/bin/cargo`.

**Confirmed-good state after the fix (2026-06-26):** rustup stable `cargo 1.96.0`; CUDA 13.2 at
`/usr/local/cuda`; bullet builds + trains on the **RTX 4070 Laptop (sm_89)** at **~2.48M pos/sec** (≈7× the
old GTX 1050 Ti). See [[machine-setup-env]] (note: that memory's stack was wiped by yet another migration —
this machine needed a full re-bootstrap: rustup, bullet, fastchess, clang-format, data, openings all
re-created from scratch). Related: [[project_nnue_experiment]].
