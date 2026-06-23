---
name: feedback-make-clean
description: Always run make clean before make in this repo to avoid stale .d dependency files causing spurious build failures
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 9060cfe0-a261-495f-b0ec-b6db79ed2919
---

Always run `make clean && make` rather than just `make` in this repo.

**Why:** Stale `.d` dependency files from previous builds can reference old file paths (e.g. `.c` instead of `.cpp` after a rename) and cause spurious "no rule to make target" failures that have nothing to do with the actual code change.

**How to apply:** Every time a build is triggered, prepend `make clean &&` to ensure a fresh build.
