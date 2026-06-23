---
name: project_search_uninit_ub
description: RESOLVED — the search's uninitialized-stack-read (bench node counts differed across compiler -O levels) was killers_/countermoves_ Move arrays not being zeroed; fixed in the SearchState ctor.
metadata: 
  node_type: memory
  type: project
  originSessionId: 4d178dc7-b2cc-4bef-90ca-ce2b53eb2817
---

**STATUS: FOUND AND FIXED (2026-06-22).** The long-standing nondeterminism — `bench` node counts differing across compiler optimization levels (e.g. O1/O2=7,027,693 vs O3=6,932,566), collapsing to one value under `-ftrivial-auto-var-init=zero` — was an **uninitialized read of `SearchState::killers_` / `countermoves_`** (arrays of `Move`).

**Root cause.** `Move`'s default constructor `Move() {}` deliberately leaves `val_` uninitialized (so per-node `MoveList` = `StackList<Move,256>` need not zero 256 moves). `killers_{}` and `countermoves_{}` aggregate-init each `Move` *through that constructor*, so the `{}` does **not** zero `val_` — the never-recorded slots hold garbage. `ScoreAndSortMoves` reads them in its killer (`move == killer0/1`) and countermove (`IsCountermove` → `countermoves_[ContKey(prev)] == move`) `==` comparisons, so a garbage slot is a coin-flip that perturbs move ordering. Benign (legal moves + evals unaffected — only ordering, hence the node-count wobble), which is why it survived years of SPRTs. `-ftrivial-auto-var-init=zero` masked it by pre-zeroing the whole `SearchState` stack object.

**The fix** (search_state.h, SearchState ctor): `for (auto &k : killers_) k.fill(Move::None()); countermoves_.fill(Move::None());` — `Move::None()` is `Move{0}` (a value no legal move equals). **Zero perf cost** (once per search, not per node). Verified: O1/O2/O3 `bench` now ALL = **6,534,680** (== the old `auto-var-init=zero` value); valgrind reports **0 errors** (was 7 contexts); `make check` passes.

**How it was finally caught (the tooling trick that worked):** valgrind memcheck, but three things were needed that prior attempts lacked:
1. **Scalar build** (`-mbmi2` but NOT `-mavx2`) → the NNUE SIMD kernels fall back to scalar (`#if defined(__AVX2__)`), eliminating the AVX2 false positives that previously "drowned" valgrind.
2. **`--max-stackframe=16000000`** → deep qsearch recursion makes a ~6 MB single-step SP change that trips valgrind's default 2 MB heuristic ("client switching stacks?"), after which it floods bogus "Invalid read/write of size 8". Raising the limit makes those vanish, leaving the 7 real "Conditional jump depends on uninitialised value" reports, all at `ScoreAndSortMoves` (search_state.h, the `IsCountermove`/killer comparisons).
3. **Small main-thread scope** → a tiny harness searching ONE complex position (kiwipete) to depth 6 with `thread_count_=1` (so `SearchRootNode` runs on the *main* thread, no worker-thread stack switch) at `-O1` (fast under valgrind, value-tracking intact). Full `bench`/`go` was too slow or used the worker thread.

MSan was never needed. The earlier "localized to game.cpp's TU" finding was right in spirit (the search lives there) but never pinpointed the variable; the header-only refactor later moved it all into main.cpp's TU. See [[project_tried_search_features]].

**Implication now true:** `bench` IS a stable cross-build determinism signature (a neutral change that alters node count is a real bug). The MSan rig (~/llvm-project, ~/llvm-msan-build) is no longer needed and can be deleted to reclaim ~2.1 GB.
