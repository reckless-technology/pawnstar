# Memory Index

- [project_go_port.md](project_go_port.md) — Go port status: all phases complete, UCI + search fully working
- [project_nnue_experiment.md](project_nnue_experiment.md) — NNUE lineage; HCE REMOVED (NNUE-only, net mandatory); SHIPPED v11 (1024 + 4 file-pair king buckets, ~6.05B data, +9.28 over v10); ≈2900 CCRL-ballpark; more data is the recurring lever (gain shrinking)
- [project_tried_search_features.md](project_tried_search_features.md) — Search features already SPRT-tested (e.g. aspiration windows: no gain) — ask before re-recommending
- [project_tried_pext_direct_lookup.md](project_tried_pext_direct_lookup.md) — Tried & REJECTED: direct-index pext lookup (drop indices_ indirection) — +3.9% perft but Elo-neutral in play (movegen ~1-2% of node cost); don't repeat
- [project_per_thread_history.md](project_per_thread_history.md) — Why history/countermove/continuation tables are per-SearchState (Lazy SMP: no contention, search diversity), only TT shared
- [project_search_uninit_ub.md](project_search_uninit_ub.md) — RESOLVED: the search uninit-read (bench node counts differed across -O levels) was killers_/countermoves_ Move arrays not zeroed (Move() leaves val_ uninit); fixed by filling them with Move::None() in the SearchState ctor; bench is now a stable cross-build signature
- [feedback_tee_test_output.md](feedback_tee_test_output.md) — Always tee test output to a temp file so full FAIL details are available for analysis
- [feedback_make_clean.md](feedback_make_clean.md) — Always run `make clean && make`; stale .d files cause spurious build failures
- [feedback_clang_format.md](feedback_clang_format.md) — Always run clang-format on edited C/C++ files before committing
- [feedback_push_to_main.md](feedback_push_to_main.md) — OK to commit/push directly to main in pawnstar (no feature branch needed)
- [feedback_descriptive_names.md](feedback_descriptive_names.md) — Naming: long descriptive names, no abbreviations; bool names start with is/has/can/may/do/does
