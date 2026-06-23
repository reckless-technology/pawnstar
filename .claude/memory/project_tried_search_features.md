---
name: project_tried_search_features
description: "Search-feature experiments already run on Pawnstar (don't re-recommend without checking)"
metadata: 
  node_type: memory
  type: project
  originSessionId: cdbb9b11-76b2-4149-8044-c595d1949392
---

Search features already experimented on in Pawnstar (SPRT-tested) — do not re-recommend as novel:

- **Aspiration windows: tested once, no gain.** PVS already null-windows root moves 2..N, and with Lazy
  SMP the helper threads desync the effective window, washing out the main thread's window discipline,
  so narrowing the root window bought nothing measurable. NOTE 2026-06-14: Jonny asked to re-implement
  and re-test it anyway as part of the Tier-1 search work — so it is being revisited, not dropped.

- **Tier-1 search stack (all 5 together): regressed -174 Elo** (26.8%, 440 games, H0 accepted), fixed
  depth 8, branch `search-tier1`. The block was: reverse futility pruning, late-move pruning, an LMR
  log(d)*log(m) formula, SEE pruning of bad captures + history-aware LMR, and root aspiration windows.
  Clean games (no crashes) — a genuine over-pruning regression with untuned conventional margins. Lesson:
  these features need per-change SPRT gating and margins tuned for Pawnstar, not a Stockfish-default block.
  Re-tuned attempt (softer LMR 0.5+ln*ln/3.0 with the zero-history +1 removed, LMP base 4, RFP margin 100,
  aspiration +/-50): still -110 Elo (34.6%, 596 games, H0). Softening recovered ~64 Elo but the block is
  still net-negative — testing the whole stack at once gives no attribution. Next: bisect each change
  individually vs baseline to find which (if any) actually gain for Pawnstar.

- **Null-move reduction depth−3 instead of depth−4 (R=2 vs R=3): regressed ≈16 Elo** (2026-06-16, branch
  `experiment-nullmove-r3`, binary-vs-binary SPRT, same v7 net, TC 8+0.08, full 1000 games). Final
  −15.99 ± 16.16 Elo (47.70%, W391/L437/D172), LLR −2.17 heading to the −2.94 reject line. Reducing the
  null-move verification search *less* makes each attempt deeper/slower, so at fixed time the engine
  searches less overall and plays weaker. The shipped `depth − 4` cut (in `AttemptNullMove`, now in
  search_state.cpp after the search refactor) is better — do not re-propose depth−3. Noted in README too.

- **make/unmake vs copy-make: copy-make is faster for Pawnstar** (Jonny tested ~a year before 2026-06,
  reported 2026-06-16). `Position` is immutable; `MakeMove`/`MakeNullMove` return a new 160-byte Position
  by value. A make/unmake (mutate-in-place + undo) rewrite was tried and proved *slower* — do not propose
  switching to make/unmake to "avoid the copy." Also: search is NNUE-eval-bound (~0.27 Mnps single-thread
  vs ~20-250 Mnps for pure perft movegen), so move-generation throughput is ~1-2% of a search node's cost
  — staged/lazy movegen and other movegen-speed ideas can't move the needle; spend effort on node-count
  reduction (move ordering) and per-node memory latency (TT/eval-cache prefetch) instead.
  **Staged move generation specifically SHELVED (Jonny, 2026-06-20): don't propose it.** Two reasons: (1)
  the ~1-2% movegen cost above means even eliminating it is negligible; (2) Pawnstar uses a *fast fully-legal*
  generator (`GenerateLegalMoves` computes checkers + pin masks and filters to the legal set up front), and
  staging only pays off with pseudo-legal generation + legality-on-make — retrofitting stages onto the legal
  generator means either re-running the pin/legality machinery per stage (paying it repeatedly) or generating
  everything anyway (saving nothing). Not worth the rewrite.

- **Countermove + 1-ply continuation history + TT prefetch: SHIPPED, ~+9 Elo** (2026-06-16, branch
  `search-ordering`, merged to main f75efc1). Binary-vs-binary SPRT, same v7 net, single-threaded, TC
  **40/20** (40 moves / 20 s): +9.38 ± 14.96 Elo over the full 1000 games (51.35%, W394/L367/D239), LLR
  +0.70 — positive but capped before significance (CI grazes 0). Shipped anyway: move-ordering changes
  can only cut node count (never blunder), so the risk floor is "neutral". Implementation: per-thread
  `countermoves_` + `cont_hist_` on SearchState, keyed by the previous move's (piece, to); `prev_move`
  threaded through `Search`/`ScoreAndSortMoves`; bands are captures > killers > countermove > quiets
  (main history + continuation history) > losing captures. **Contrast with the Tier-1 stack: ordering
  heuristics are low-risk wins; PRUNING heuristics (RFP/LMP/log-LMR/SEE-pruning) regressed badly.** The
  ~1-2% TT prefetch (after PlayMove in SearchSingleMove) rode along, result-neutral.

- **Eval cache 8MB -> 32MB: rejected, ~-7 Elo** (2026-06-16, branch `eval-cache-32`, SPRT 40/20, same v7
  net, 1000 games). -6.60 ± 15.67 Elo (49.05%, W375/L394/D231), LLR -1.16; negative the whole run
  (-14.8 at 706 games). Pure cache-size change (`kEvalCacheMb` in constants.h; results bit-identical).
  Counterintuitive but consistent: the eval cache is probed every node, and a 32MB table thrashes the
  CPU L2/L3 worse than 8MB, and the engine is memory-latency-bound, so the bigger table is a net
  slowdown despite a marginally higher logical hit rate. **8MB is near the locality sweet spot — do not
  enlarge the eval cache.** (Same lesson likely applies to oversizing the TT.)

- **Internal iterative reduction (IIR): SHIPPED, +31.8 Elo at 8+0.08** (2026-06-20, commit 068f994). At a
  node with no usable TT move and `depth >= kIirMinDepth` (4, constants.h), reduce `depth` by 1 (block in
  `Search`, search_state.cpp, after the TT cutoff and before the qsearch drop). Binary-vs-binary SPRT, net
  held fixed at v10, TC 8+0.08: **+31.8 ± 16.2 Elo, LLR 2.95 H1 accepted, 898 games (54.6%)** — a big,
  clean win (node-count reduction directly buys depth in this NNUE-eval-bound engine). First run was
  contaminated when the laptop slept (clocks desync → 4 s time-loss overruns, throughput collapsed);
  re-ran clean. BK accepted moves regenerated (7 positions changed), make check green 24/24. **Contrast
  with the rejected PRUNING heuristics — IIR is a reduction/ordering-quality win, the low-risk category.**
  Remaining queued search-speed levers (untried): correction history, singular extensions, ProbCut, UCI
  Hash/Threads options. Staged movegen is SHELVED (see make/unmake bullet above).

- **Correction history (pawn-keyed): REJECTED, neutral/-3 Elo** (2026-06-20, not shipped, reverted). A
  per-colour 16384-entry table keyed by a hash of the pawn bitboards, holding a depth-weighted EMA of
  (search score − raw static eval); the raw NNUE static eval was corrected by it before RFP/null-move used
  it, updated at the three non-PV resolution points (TT-move cutoff, loop cutoff, all-node) gated on quiet
  moves + one-sided bound direction. Constants: grain 256, clamp ±48 cp, weight min(depth+1,16)/256.
  Binary-vs-binary SPRT vs the IIR baseline (same v10 net, TC 8+0.08): **H0 accepted, −3.26 ± 7.80 Elo over
  3726 games (49.5%)**. Clean implementation (correctness suites green); it just doesn't help *this* engine —
  Pawnstar's static-eval pruning (RFP+LMP) is already SPRT-tuned, so a single untuned pawn-correction table
  layered on top is redundant/slightly negative. Could *maybe* be rescued with tuned constants or additional
  correction tables (material / non-pawn), but Jonny chose to abandon it and move on. **Don't re-propose as a
  novel idea; if revisited, it needs tuning + multiple tables, not the basic single-table version.**

- **Singular extensions: REJECTED, non-positive at BOTH time controls** (2026-06-20, not shipped, reverted).
  Standard impl: an `excluded_move` param on `Search`; before searching the TT move, a verification search of
  all OTHER moves at reduced depth `(depth-1)/2` against a window `singular_beta = tt_score - 2*depth` just
  below the TT score; if they all fail low the TT move is singular and gets +1 ply. Gated to depth>=8, a
  cut/pv (lower-bound) TT entry with `tt_depth >= depth-3`, outside the mate zone; during the verification the
  node disabled TT cutoffs, static-eval pruning, nested SE and ALL TT stores. Confirmed firing (243-747
  extensions/position at d12). Binary-vs-binary vs the IIR baseline, same v10 net: **8+0.08 → H0, −16.96 ±
  13.60 over 1230 games (47.6%)**; retested at **40/20 → hovered ~−3 to −5 (final −3.05 ± 14.56 at 910 games,
  49.6%), drifting toward H0** (abandoned before formal decision). The sign improved with slower TC (−17 → ~−4)
  — classic SE time-control dependence — but it never became a *win*. Likely over-extending (margin too small
  for Pawnstar) and/or the verification overhead doesn't pay at these TCs. **Don't re-propose as a sure win;
  if revisited it needs real margin/depth tuning (raise kSeMinDepth to ~10-12, widen the margin) and probably
  longer TC than 40/20.** Implementation pattern (excluded-move recursion) is sound and reusable.
- **Internal iterative reduction (IIR): SHIPPED this same session, +31.8 Elo** — see the dedicated bullet
  above. IIR is the one search-speed win of the 2026-06-20 batch; corrhist and SE both rejected.

- **Increment-aware time budget (`+ increment / 2` in soft budget): REJECTED, neutral/negative** (2026-06-21,
  reverted). Folded the per-move UCI increment (winc/binc) into the soft budget so the engine spends it
  instead of leaving it on the clock. First SPRT vs the pre-feature baseline lost **−29.6 Elo at 8+0.08** but
  was a **time-forfeit bug, not the idea**: the candidate (now actually spending time) hit the hard deadline,
  which was polled only every 64k nodes, and overran by 6-10 ms → **139 forfeits** vs 2. Fixed by polling the
  hard deadline every **2k nodes** (`& 0x7FF`) + raising default **Move Overhead 10→30 ms** (both SHIPPED —
  real robustness wins, kept). Re-ran as a **clean isolation** (same binary, sole diff the `+ increment/2`
  term, both sides carrying the forfeit fix): **8+0.08 → −14.5 ± 21 (480g); 10+0.1 → −2.5 ± 7.9 (3562g)** —
  both neutral-to-slightly-negative, zero forfeits. In equal-clock self-play at fast TCs spending the increment
  is ~symmetric so it doesn't convert to Elo. Reverted the budget term; `winc`/`binc` are still parsed into
  `ChessClock::increment` but unused. **May pay off at much slower increment TCs (e.g. 60+0.6) — untested; the
  infra is left in place.** Also shipped alongside (play-neutral): `info score mate`/`nps`/`hashfull`, `Clear
  Hash` and `Move Overhead` UCI options. **Lesson: when testing a "spend more time" change, watch forfeits and
  poll the deadline finely; and isolate the variable — the first −29.6 was confounded by the forfeit fix's own
  time-trimming.**

**How to apply:** before recommending a search/eval change, assume Jonny may have already SPRT-tested it
and ask. The SPRT harness (`tools/run_sprt.sh` + fastchess) is the arbiter — he tests changes rather
than taking them on theory. See [[project_nnue_experiment]].
