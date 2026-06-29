---
name: project_tried_pext_direct_lookup
description: "Tried & REJECTED — direct-index pext sliding-attack lookup (drop indices_ indirection); faster in perft, Elo-neutral in play"
metadata: 
  node_type: memory
  type: project
  originSessionId: 4d178dc7-b2cc-4bef-90ca-ce2b53eb2817
---

**Superseded by [[project_magic_bitboards]] (2026-06-29): pext is gone — sliders now use magic bitboards.**
But the lesson below still governs: the same `indices_` 1-byte compression was kept for the magic tables
(the user asked for it), re-confirming that the direct one-load layout's perft edge isn't worth the cache
cost. The experiment below is the original pext-era data.

Experiment (2026-06-23, branch `pext-direct-lookup`, now deleted): removed the `indices_` indirection from
the pext sliding-attack lookup. Baseline does `attacks_[indices_[pext(occ,mask)]]` (two dependent loads;
de-dups rooks' 4096 occupancies to ~100 unique attack sets behind a 1-byte index table). Variant indexed
`attacks_[pext(occ,mask)]` directly — a dense 8-byte attack per occupancy (~2.25 MB sliding tables vs
~350 KB compressed), one fewer dependent load.

**Result: faster in perft, Elo-neutral in real play → NOT merged.**
- Perft (pure movegen): **+3.9% ± , +12.2 Mnps** over 20 interleaved test_perft A/B runs (313.3→325.6 Mnps,
  paired t=3.99). Correctness identical (perft 770/770 match).
- SPRT at 8+0.08, same v11 net both sides, binary-vs-binary (BIN_A=variant, BIN_B=baseline): **+0.6 ± 10.5
  Elo over 2376 games (50.08%)**, LLR −0.17 in [0,5] — stopped manually as clearly neutral (early +15 Elo
  was small-sample noise, regressed to ~0 by ~2000 games).

**Why:** move generation is only ~1–2% of a node's cost (NNUE eval dominates), so a 4% movegen speedup is
<0.1% of node throughput — far too small to register as Elo. The bigger tables also add cache pressure that
competes with the TT / NNUE accumulators / eval cache in real search (perft's tiny working set hides this).
The compressed `indices_` layout on main is the better tradeoff. **Don't repeat.**

How to run a binary-vs-binary *speed* SPRT (same net, two builds): `BASELINE_NET=<net> BIN_A=<cand build>
BIN_B=<base build> TC=8+0.08 tools/run_sprt.sh <net> ~/pawnstar_nnue/openings.epd <rounds>` — must be a
*time control* (fixed depth shows nothing; both reach identical nodes). Prereqs present on this box:
fastchess on PATH, openings at `~/pawnstar_nnue/openings.epd`. Related: [[project_tried_search_features]].
