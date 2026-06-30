---
name: project_deterministic_move_sort
description: "SortMoves (move.h) replaced std::sort/std::stable_sort with a hand-written STABLE in-place insertion sort, so move ordering — and the bench node count — is identical across libstdc++/libc++/MSVC. New cross-platform bench signature 7451559 (was per-STL). Insertion sort (not merge): move lists are small/hard-bounded, so it's simplest AND fastest — ~975k nps, beating the std::sort it replaced."
metadata:
  node_type: memory
  type: project
---

**`SortMoves` in [move.h](src/move.h) is a hand-written stable in-place insertion sort** (2026-06-30, at
Jonny's request — he dislikes the cross-platform non-determinism of `std::sort`). It replaced both the
inner-node `std::ranges::sort` (unstable) and the root-only `std::ranges::stable_sort`. Now there is ONE
sort, always stable.

**Why deterministic:** `std::sort`/`std::ranges::sort` leave the order of equal-keyed elements unspecified,
and libstdc++, libc++ and MSVC's STL tie-break differently. Equal-ordering-score moves got searched in a
different order per platform, cascading (via alpha-beta) to a different still-legal best move and a
**different bench node count per STL** — see [[project_apple_silicon_port]] (macOS libc++ 7628404 vs Linux
6857880) and the MSVC-STL note in [[machine-windows-native-build]]. A stable sort keeps equal scores in
generation order, so the search tree is now **identical on every platform**.

**Why insertion sort (not merge/quick):** a move list is small and **hard-bounded** — at most
`kMaxMovesPerPosition` (256), legal max 218, in practice a few dozen. For that size the simple in-place
insertion sort (tiny constant factor, no scratch buffer, no passes) beats an O(n log n) sort, and its O(n²)
worst case is irrelevant — even a maximal ~218-move position sorts in microseconds, and such positions
essentially never occur. Stable: the inner loop shifts an element left only past **strictly smaller** scores,
so equal scores keep their order.

**The path to it (don't redo the sweep):** first shipped as a stable bottom-up **merge sort** with a
stack scratch buffer (commit 9bf111c). That was ~6% LOWER bench nps than std::sort (the merge's log2(n)
ping-pong passes cost more than introsort's insertion-sort base case on small lists). Added an insertion-sort
base case and swept its length `PAWNSTAR_SORT_BASE_RUN` — crucially **the sorted order (node count 7451559)
is identical for every value**, so it's a pure sort-cost measurement (same tree). bench nps: base run 1 (pure
merge) 906k → 16: 927k → 32: 958k → 64: 995k → 128: 1030k → 256: 1007k. Since any list ≤ the base run is
pure insertion sort with an early return (no merge), and **no real position exceeds the base run**, base
runs ≥128 all execute the *same* code for real lists — so the apparent 128-vs-256 difference is noise, and
the merge branch is dead weight. Jonny's call: **drop the merge entirely, keep only the insertion sort.**
Final: ~975k nps (3-run bench), beating the old std::sort (~955k) AND ~14% over the pure merge sort, with
~30 fewer lines and no scratch buffer.

**Net signatures / gates:**
- **Cross-platform bench = 7451559** (deterministic across runs; by construction — bit-identical eval +
  deterministic stable sort — identical on Linux/Windows too; verify there). **Supersedes the old per-STL
  bench values** (macOS 7628404, Linux 6857880) wherever they appear in memory.
- BK accepted moves: only pos11 shifted (`a1a3`/`h2h4`) — added to kCases; 24/24 at depths 8–11; `make check`
  green. The per-STL BK additions (pos03/06/07/09/19/23) are now harmless legacy.

**Determinism is no longer a speed cost — it's a net speedup.** Related: [[project_apple_silicon_port]],
[[project_tried_search_features]] (move-ordering changes only cut/grow node count, never blunder),
[[machine-windows-native-build]].
