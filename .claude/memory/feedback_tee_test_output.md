---
name: feedback-tee-test-output
description: Always tee test output to a temp file when running tests in background
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 70cfcbed-8e15-4043-b052-e9bdf5c95a59
---

Always pipe test output through `tee /tmp/some_file.txt` when running tests, so the full output (including individual FAIL lines) is available for later analysis. Always run `make clean` before `make` when the build needs to reflect a changed compile-time flag (e.g. `N_HELPERS`, `SERIAL`, `DEBUG`).

**Why:** Without `tee`, FAIL details are lost. Without `make clean`, object files from a previous build with different flags may not be rebuilt, silently producing the wrong binary.

**How to apply:** For any background test run: `./build-test/test_bratko_kopec | tee /tmp/bk_run_N.txt | tail -1`. When changing compile-time flags: `make clean && make N_HELPERS=0 && make test N_HELPERS=0`.
