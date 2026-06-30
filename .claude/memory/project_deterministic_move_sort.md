---
name: project_deterministic_move_sort
description: "SortMoves (move.h) replaced std::sort/std::stable_sort with a hand-written STABLE bottom-up merge sort (stack scratch buffer, no heap) so move ordering — and the bench node count — is identical across libstdc++/libc++/MSVC. New cross-platform bench signature 7451559 (was per-STL). BK24@d12 ~16% faster; bench per-node ~6% slower nps."
metadata:
  node_type: memory
  type: project
---

**`SortMoves` in [move.h](src/move.h) is now a hand-written stable bottom-up merge sort** (2026-06-30, at
Jonny's request — he dislikes the cross-platform non-determinism of `std::sort`). It replaced both the
inner-node `std::ranges::sort` (unstable) and the root-only `std::ranges::stable_sort`. Now there is ONE
sort, always stable.

**Why:** `std::sort`/`std::ranges::sort` leave the order of equal-keyed elements unspecified, and
libstdc++, libc++ and MSVC's STL tie-break differently. Equal-ordering-score moves therefore got searched in
a different order per platform, cascading (via alpha-beta) to a different still-legal best move and a
**different bench node count per STL** — see [[project_apple_silicon_port]] (macOS libc++ bench 7628404 vs
Linux 6857880) and the MSVC-STL note in [[machine-windows-native-build]]. A merge sort is naturally stable
(equal scores keep generation order), so the search tree is now **identical on every platform**.

**Implementation:** iterative bottom-up merge, descending by `score()`, ping-ponging between the `MoveList`
and a **stack-allocated `std::array<Move, kMaxMovesPerPosition>` (256) scratch buffer — no heap allocation**.
Stable: takes from the right run only when STRICTLY greater, so equal scores keep the left (earlier) move.
Copies back from scratch only if an odd number of passes left the result there. The old `SortMoves<bool>`
template + its 3 call sites (search_state.h ×2, game.h ×1) became a plain `inline void SortMoves(MoveList&)`.

**New cross-platform bench signature: 7451559** (deterministic across runs AND — by construction, bit-
identical eval + deterministic sort — should be identical on Linux/Windows too; verify on those). This
**supersedes the old per-STL bench values** (macOS 7628404, Linux 6857880, etc.) everywhere they appear in
memory. BK accepted moves: only pos11 shifted (now `a1a3`/`h2h4` at d8/d9/d10) — added to kCases; 24/24 at
depths 8–11, `make check` green. The per-STL BK additions (pos03/06/07/09/19/23) are now harmless legacy
(the chosen move is the same everywhere); the BK test header comment was updated to say so.

**Speed (measured on M1 Pro):**
- **BK24 @ depth 12: ~16% FASTER — min 6894 ms (merge) vs 8231 ms (std::sort)**, rigorous 5-run A/B, both
  deterministic/tight. The stable ordering yields a smaller search tree for these positions at this depth.
- **bench @ depth 13: ~6% LOWER nps** (~897k vs ~955k) and ~2% fewer nodes — i.e. the merge sort itself is
  marginally **slower per call** than introsort (which insertion-sorts small arrays in place; the bottom-up
  merge does log2(n) full ping-pong passes). So the speed effect is **workload/tree-shape dependent** — a win
  on BK24@d12, roughly flat-to-slightly-slower on bench. **The guaranteed, durable win is determinism, not
  speed.**
- **Open optimization (not done):** add an insertion-sort base case for small runs (e.g. ≤16–32) — standard
  optimized-merge-sort form, still stable+deterministic — to recover the per-call cost (most move lists are
  ~30–40 moves, where introsort uses insertion sort). Would likely neutralize the ~6% bench-nps cost while
  keeping the BK gain. Offered to Jonny; do it if per-node nps matters for game play (it's ~10–15 Elo at fast TC).

Not committed yet. Related: [[project_apple_silicon_port]], [[project_tried_search_features]] (move-ordering
changes only cut/grow node count, never blunder), [[machine-windows-native-build]].
