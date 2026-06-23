---
name: project_per_thread_history
description: "Why HistoryTable (and countermoves/continuation history) are per-SearchState, not global to Game"
metadata: 
  node_type: memory
  type: project
  originSessionId: 4d178dc7-b2cc-4bef-90ca-ce2b53eb2817
---

The `HistoryTable` is a **per-thread** member of `SearchState`, not a `Game`-global, by deliberate Lazy SMP design. Same for `countermoves_` and `continuation_history_`. The **only** intentionally shared mutable state across search threads is the transposition table(s) and `is_cancel_pending`.

**Why:**
1. **No cross-thread contention.** Under Lazy SMP every thread runs a complete independent search of the root tree. A global history table would have all threads writing the same counters on every beta cutoff — cache-line bouncing / false sharing on a hot path, requiring atomics or locks. Per-thread counters are plain ints updated with zero synchronization.
2. **Search diversity is the point of Lazy SMP.** Helpers help precisely by exploring differently (iteration-skip schedule). Independent history tables reinforce that; a shared one would pull threads toward lockstep. Cooperation happens through the shared lockless TT, where one thread's entries prefill another's probes.

**How to apply:** Don't propose making history/countermove/continuation tables global or shared — it was a conscious choice, not an oversight. New per-search heuristic state should default to per-thread `SearchState` ownership too. See [[project_tried_search_features.md]].
