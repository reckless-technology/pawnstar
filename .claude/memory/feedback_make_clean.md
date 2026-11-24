---
name: feedback-make-clean
description: "STALE since the CMake wrapper (2026-11-24 correction): the old premise (stale .d files in the hand-written Makefile) is gone; make clean is only needed when switching CXX"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 9060cfe0-a261-495f-b0ec-b6db79ed2919
---

**STALE (correction 2026-11-24): the premise is gone.** The Makefile is now a thin wrapper over CMake (build in `build/`, version header regenerated when `HEAD` moves) and has no `.d` dependency rules, so stale `.d` files cannot occur and `make clean && make` every time is unneeded. `make clean` still removes `build/`; the one case that needs it is switching `CXX` for an existing tree (CMake caches the compiler). Kept for the history; see CLAUDE.md, "Build".

Always run `make clean && make` rather than just `make` in this repo.

**Why:** Stale `.d` dependency files from previous builds can reference old file paths (e.g. `.c` instead of `.cpp` after a rename) and cause spurious "no rule to make target" failures that have nothing to do with the actual code change.

**How to apply:** Every time a build is triggered, prepend `make clean &&` to ensure a fresh build.
