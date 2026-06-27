---
name: project_nnue_experiment
description: "NNUE lineage + Pawnstar's measured strength; HCE removed, NNUE-only (~2900 Elo); SHIPPED v11 (1024-wide + 4 file-pair king buckets, ~6.05B data) — beat v10 by +9.28 Elo @ 8+0.08 (LOS 99.7%). More data is the recurring lever, but the per-step gain is shrinking (+16 v9→v10 → +9 v10→v11)."
metadata: 
  node_type: memory
  type: project
  originSessionId: cdbb9b11-76b2-4149-8044-c595d1949392
---

NNUE experiment results (as of 2026-06-14):

- **Bootstrap run:** ~3.6M self-play positions (3.37M at depth 6 + 235K depth-8 seed),
  generated in ~6h on 12 cores, trained 40 superbatches via bullet GPU.
- **SPRT vs HCE (depth 8):** the 3.6M-position net loses to the handcrafted eval by
  **~-67 Elo** (40.4% over 402 games, cut off by a 90-min cap). The prior ~1M-position
  net (`net_solid`) lost by **-151 Elo** — so ~3.6x the data roughly halved the deficit.
  **Data volume is the dominant lever; NNUE has not yet overtaken HCE.**
- Depth-6 self-play generation ran ~11,300 pos/min (4.2x the depth-8 rate of ~2,670/min).
- Net file: `~/pawnstar_nnue/boot/net_boot.bin`; dataset `~/pawnstar_nnue/boot/data/`.

**Pawnstar-HCE absolute strength** (fastchess vs Arena CCRL engines in `~/.local/bin/Engines`,
fast TC 8+0.08, single-thread): scored ~23% vs K2 0.87 and Spike 1.2 Turin (~-205 Elo each),
~5% vs Toga II 4.0. Anchored estimate **~2350 CCRL-ballpark (range ~2300-2500)** — all
working anchors are stronger than Pawnstar, so the estimate extrapolates from lopsided scores
and a non-CCRL TC; treat as approximate. AICE 0.99.2 can't be driven by fastchess (errors out).

**Update (later 2026-06-14) — NNUE now beats HCE, all merged to main:**
- **v2** net trained on ~60M positions of public **PlentyChess** bulletformat data
  (<https://huggingface.co/datasets/Yoshie2000/plentychess_data_bulletformat>) **beats HCE by ~+345 Elo**
  at fixed depth 8. Lesson: **label quality** (strong-engine data) was the lever, not self-play volume.
  Shipped as `nnue/pawnstar-v2.bin`. (v1 = our own self-play, lost -67.)
- **Incremental accumulator** (maintained across make/undo): **+80 Elo** equal-time SPRT vs full-refresh.
- **AVX2 SIMD** kernels (bit-identical to scalar): **+180 Elo** equal-time SPRT; ~2.65x faster NNUE eval.
- **v3 (data scale-up, ~750M positions, ~12.5x v2): NO meaningful gain** — v3 vs v2 only +9.5 ± 13.6
  (1278 games, inconclusive); v3 still ~90% vs HCE. Conclusion: the **256-wide Chess768 net is
  capacity-limited** — more data is not the bottleneck. **Next lever is a bigger net (L1 512/1024) and/or
  king-buckets, not more data.** v3 not shipped (no improvement over v2); kept at `~/pawnstar_nnue/v3/`.
- **Bigger net (v4, 512-wide) is a decisive win** (branch `bignet-512`; engine `kHiddenSize` and trainer
  `HIDDEN`/`HIDDEN_SIZE` = 512). Trained on the *same 60M* as v2 (single variable = width). SPRT v4(512)
  vs v2(256): **+55 ± 20 Elo at fixed depth, +71 ± 25 at time control** (both H1) — the wider eval more
  than pays for its ~2.3x slower forward pass. So after data saturates, **net width is the lever.** Net is
  789568 bytes (512 arch). NOTE: a 512 engine cannot load a 256 net and vice-versa (size-gated), so
  shipping 512 means re-shipping the net + a 512 reference + Makefile NNUE_NET change, and the old 256
  nets become unloadable. v4 only used 60M; 512 on more data (and/or 1024-wide) are likely further gains.
- **v4 shipped + NNUE ON by default** (main.cpp loads `nnue/pawnstar-v4.bin` cwd-relative; PAWNSTAR_NNUE=0
  disables). 256 nets retired (a 512 engine can't load them). **Measured Elo of v4 NNUE engine: ~2900
  CCRL-ballpark** (vs Arena CCRL engines, 8+0.08; Toga 45.5%→~2919 best-bracketed, Spike/Senpai/Arasan
  cluster ~2870-2965; K2 anchor too low). ~+550 over the ~2350 HCE baseline.
- Architecture program in progress (verify each step: test_nnue 0cp + incremental + SPRT vs prior best):
  (1) wider transformer 512->1024, (2) king-buckets/HalfKA features, (3) retrain on more data.
- **v5 (1024-wide) REJECTED — Step 1 done, stay at 512.** Trained on the same 60M as v2/v4 (branch
  `bignet-1024`). Verified clean (test_nnue 0/250 0cp; incremental 0 mismatches). SPRT v5(1024) vs
  v4(512) 2026-06-15: **fixed depth 8 = +2.0 ± 15.6 Elo over 1046 games (dead even, LLR drifting to H0)**;
  **tc 8+0.08 = -74.5 ± 24.5 over 502 games, H0 accepted (LLR -2.96)** — 1024 *loses badly on the clock*.
  Doubling width 512->1024 on the same 60M gives ~0 fixed-depth gain (unlike 256->512's +55), so **512
  is capacity-sufficient for 60M data** — echoes the v3 finding that 60M is the binding constraint. To
  benefit from 1024 you'd need much more data. **Decision: keep 512; do king-buckets on the 512 arch.**
  1024 not shipped; bignet-1024 branch kept for record (do NOT merge). Net at ~/pawnstar_nnue/v5/.
- **King-buckets (4, 512-wide) — Steps 2+3, branch `kingbuckets-4`.** Engine + bullet trainer use
  bullet `ChessBuckets` (4 buckets, file/rank quadrant map; refresh-on-king-bucket-crossing). All verified
  clean (test_nnue 0/250 0cp; incremental 0 mismatches incl. crossings). SPRT vs v4(512):
  - on **60M** data: **flat** (~-8 ± 22 fixed depth) — data-starved (4x feature params).
  - on **~818M** pool (2 big PlentyChess shards 11008+13349 + the 60M, shuffled; same v4 compute = 40 SB
    x 3688 BPS = 2.4B samples seen, ~3 epochs): **fixed depth +47.3 ± 18.2 Elo, H1 accepted** (decisive
    eval-quality win!) BUT **tc 8+0.08 -74.1 ± 25.2, H0 accepted** (loses on the clock). Confirms the
    hypothesis: **more data WAS the lever** for the higher-capacity arch (flatness was starvation).
  - **Why it loses on the clock:** kb runs at **~55% of v4's nps** (~163k vs ~297k nps, fixed midgame).
    NOT from refreshes (only **1.9%** of updates in real search). Cause is ~2x cache pressure from the 4x
    larger feature-weight table (3.1MB vs 786KB; two hot king-bucket banks ~1.5MB working set) + minor
    two-pass DiffSide overhead. Same "wins eval, loses clock" wall as 1024, here via memory not arithmetic.
  - **SPEED FIXED -> king-buckets now WINS on the clock too (shippable).** Two engine optimisations
    (branch `kingbuckets-4`): (a) **lockless 8MB eval cache** memoising NNUE static eval by zobrist
    (48-bit key / 16-bit score; 42% hit rate, ~5%); (b) **single-pass `Update`** — the big one: the
    per-move accumulator cost is dominated by the 12-piece-type bitboard scan, and the original two-pass
    DiffSide (one per perspective) scanned twice; combining into a single pass for the common case (~98%,
    no king-bucket crossing) cut depth-15 time ~11.5s->7.5s. kb went from ~2.2x slower than v4 to ~1.18x.
    Re-SPRT optimized-kb vs v4 tc 8+0.08: **+17.2 ± 11.7 Elo, 2000 games, LLR 2.83/2.94** (hit round cap
    just shy of H1; CI ~[+5,+29] excludes 0). So king-buckets+818M+speed-fixes beats v4 on BOTH axes
    (+47 fixed depth, +17 clock) — the program's first clean arch win. **Lesson: profile the update path;
    a wider feature table's cost is mostly the redundant per-move scan, not the table cache.**
  - Net: `~/pawnstar_nnue/kbbig/pawnstar-kbbig.bin` (3148864 bytes, 768x4 buckets). Data pool:
    `~/pawnstar_nnue/kbbig/shuffled.data` (818M).
- **PROMOTED TO MAIN as `nnue/pawnstar-v6.bin` (2026-06-15).** Merged kingbuckets-4 -> main (ff): engine
  king-buckets + ChessBuckets trainer + eval cache + single-pass Update + v6 net (3148864 bytes); main.cpp
  & Makefile default to v6; test/nnue_reference.txt regenerated (test_nnue 0/250 0cp); **Chess768
  pawnstar-v4.bin RETIRED** (4-bucket engine can't load it); docs (CLAUDE/README/nnue) updated to v6.
  `make check` fully green (~78s). **v6 is the shipped default.** NOT yet pushed to origin (local main
  ahead). bignet-1024 (1024, rejected) kept for record. The eval cache + single-pass also help any arch.
- **GOTCHA (cost an experiment):** the NNUE loader's size gate only rejects nets that are *too small*
  (`size >= kNetFileBytes`); a **wider** net is larger, so a narrower-built engine *accepts* it and
  silently misreads garbage. `run_v5.sh`'s verify used the repo's stale 512-built `build/test_nnue`
  against the 1024 net -> 249/250 false FAILs. **When training a different width, `make clean && make`
  (and test bins) at that width BEFORE verifying.** run_v5.sh should rebuild engine+tests for the target
  width (it only rebuilds the bullet trainer/eval). Same trap would hide a real bug, so consider an
  exact-size loader check.
- NNUE is object-oriented now (`nnue::Network` owned by `Game`).
  Bad PlentyChess shard seen: `11496.data` fails `bullet-utils validate`; 11008/11128 are clean.

**Update (2026-06-15, later) — HCE REMOVED; NNUE is the only evaluator (all pushed to origin/main):**
- **Handcrafted eval deleted entirely** (material/PST/pawn-structure/king-safety/mobility/tapering, their
  tables, and the HCE-only generated bitboards kKingAttacks2/kKingPawnShelter*/pawn-structure masks).
  `EvaluatePosition` is NNUE-only (draw check -> net -> eval cache). A net is now **mandatory**: main.cpp
  (and gen_data) exit if no net loads. Removed the `UseNNUE` UCI option/flag; `NnueActive()` == "net
  loaded". `run_sprt.sh` is net-vs-net only (BASELINE_NET required). ~950 LOC net removed.
- **Deleted tests** `test_bratko_kopec` (score-based, HCE) and `test_pawn_structure`. `make check` runs 5
  suites now. **test_bratko_kopec_nnue simplified + made a hard 24/24 gate**: each case is now {fen,
  up-to-5 accepted moves} (flat, no per-depth scores). Accepted moves regenerated empirically (10 runs x
  depths 8-11; single-thread search is deterministic per depth; union <=3 moves/pos). Verified **24/24 at
  each depth 8,9,10,11**.
- **The size-gate gotcha (below) is FIXED structurally**: shipped nets carry a 32-byte `NetHeader` (magic
  PSN1 + arch params) validated field-by-field on load; raw nets must match the exact payload size window
  (rejects too-large/wider nets too). `tools/stamp_net` stamps a raw bullet net. So an incompatible net
  can no longer be silently misread.
- **v6 final measured strength: ~2900 CCRL-ballpark.** Clean 5-anchor run (fastchess vs Arena CCRL
  engines, 8+0.08, 1 thread, concurrency 6 to avoid stalls): spike->~2896, toga->~2869, senpai->~2907,
  arasan->~3054; k2->~2604 is the known under-rated anchor (exclude); raw 5-anchor mean ~2866. Weight the
  spike/toga/senpai/arasan cluster -> **~2900**. ~+550 over the removed ~2350 HCE.
- Portable `tools/rate.sh` does anchored-Elo (name:rating:cmd args). Local orchestrators in
  `~/pawnstar_nnue/` (rate_v6_clean.sh -> rating_v6c/). Also: doc/html purged from git history via
  git-filter-repo; gen_constants now builds into build/.

**Update (2026-06-16) — v7: MORE DATA wins again (~+30 depth / +21 clock over v6). NOT yet shipped.**
- Scaled the **same 512×4 arch** from v6's 818M to **2.31B positions** (v6's kbbig/shuffled.data + 4 new
  big PlentyChess shards 11128+12128+12932+13227; a clean SUPERSET), trained **SBS=120 / BPS=3688 (~7.25B
  samples, ~3 epochs)** vs v6's SBS=40. Orchestrator: `~/pawnstar_nnue/run_v7.sh`; net
  `~/pawnstar_nnue/v7/pawnstar-v7.bin` (stamped, 3148896 bytes) + raw `quantised-raw.bin`.
- **SPRT v7 vs v6 (same binary, EvalFile differs): fixed depth 8 = +29.96 ± 14.05 (1174 games, H1
  accepted); tc 8+0.08 = +20.73 ± 12.73 (1762 games, H1 accepted).** Decisive on BOTH axes — same
  arch/speed, so the eval gain carries to the clock (no speed cost to recover, unlike width/buckets).
  **Confirms v6 was data-limited; more data is still the lever** (v6 used only ~4% of the 21B available).
- **GOTCHA (cost a verify cycle, now fixed in run_v7.sh):** `pawnstar_eval` (bullet example) is NOT
  header-aware, so the cross-check must use the **RAW** (unstamped) net; I stamped before verifying and it
  misread the 32-byte header -> 248/250 false fails. Re-checked on the raw net: 0/250 0cp; incremental 0
  mismatches. Net is good. **Rule: pawnstar_eval gets the raw net; test_nnue/incremental accept either.**
- **SHIPPED to main 2026-06-16** (commit b2911a7): `nnue/pawnstar-v7.bin` is the default; v6 retired.
  main.cpp/Makefile/gen_data/rate.sh default to v7; test/nnue_reference.txt regenerated (0 cp); BK accepted
  moves regenerated for v7 (union over depths 8-11, verified 24/24 at each of 8,9,10,11); docs updated
  incl. full step-by-step v7 recreation in nnue/README §7. `make check` green. **BK test is now a HARD
  24/24 gate** (regenerate its accepted moves per shipped net — see nnue/README §7 step 7).
- Next lever: even more data (2.31B is only ~11% of 21B), then revisit 1024-wide or 8 king buckets now
  that more data clearly helps the higher-capacity arch.
- **v7 measured strength at 40/60 (2026-06-16): ~2900-2950 CCRL-ballpark.** 200-game gauntlet (fastchess,
  1 thread, 64MB hash, anchors in ~/.local/bin/Engines): gaviota2650→2711, arminius2750→2839,
  toga2950→**2950 (dead 50%, cleanest anchor)**, senpai3050→2892, arasan3150→3070; games-weighted mean
  **~2893**. Anchored estimate trends upward with opponent rating (old engines' CCRL anchors aren't
  perfectly mutually calibrated) → treat as ballpark. Confirms the ~2900 prior, a touch higher (v7 > v6).
  Note: this is AFTER the search-ordering merge (countermove + continuation history + TT prefetch, +9 Elo).

**Update (2026-06-17) — 1024-wide / NO king buckets on full 2.31B data BEATS v7 (+31.9 Elo @ 40/20). SHIPPED as v8 (commit fb961b9).**
- Branch `bignet-1024-fulldata`: `kHiddenSize=1024`, `kNumKingBuckets=1` (all-zero bucket map = plain Chess768,
  single bank); trainer `HIDDEN_SIZE=1024` + all-zero KING_BUCKETS. Trained on the **exact same 2.31B v7
  dataset** (`~/pawnstar_nnue/v7/shuffled.data`), same schedule (SBS=120/BPS=3688). Single variable vs v7 =
  arch (wider, no buckets). Net `~/pawnstar_nnue/bignet1024/pawnstar-1024.bin` (stamped, **1,579,104 bytes —
  HALF of v7's 3,148,896**). Orchestrator `~/pawnstar_nnue/bignet1024/run.sh`.
- Verify clean: cross-check **0/250 0cp** (on the RAW net), incremental **0 mismatches**.
- **SPRT 1024-no-KB(cand) vs v7 512×4(base) @ tc 40/20: +31.90 ± 16.59 Elo (nElo +42.3), 972 games
  +429=203−340 (54.6%), Ptnml [37,60,234,87,68], LLR 2.95, H1 accepted** (bounds [0,10]). ~51 min.
- **Finding: at 2.31B data, doubling width (512→1024) beats king-bucketing** — and the winner is HALF the
  size (768×1×1024 = 786K feature rows vs v7's 768×4×512 = 1.57M; wider hidden, no bucket banks): stronger
  AND lighter (less memory bandwidth helps the clock). **Reverses the v5 "1024-on-60M loses −74 on the clock"
  result** — 60M was the binding constraint; at full data the extra width finally pays. More data is the lever
  (again). Open: 1024 *with* king buckets on full data? 8 buckets? even more data (still ~11% of 21B)?
- **GOTCHA 1 (eval cross-check):** `pawnstar_eval.rs` still had `NUM_BUCKETS=4` (from v7) while KING_BUCKETS
  was all-zero → it expected a 4-bank net and `Net::load` would panic on the 1-bank net → verify fails (or
  passes vacuously on an empty ref). Fixed to `NUM_BUCKETS=1`; **the prebuilt `$EVAL` binary was also stale
  (built for an older arch) and run.sh never rebuilds it — must rebuild `cargo build --example pawnstar_eval`.**
- **GOTCHA 2 (cross-arch SPRT, reusable):** the 1024 cand engine **could not start** under run_sprt — its
  startup-default net (cwd `nnue/pawnstar-v7.bin`) AND its **embedded fallback** (`embedded_net.S` hardcodes
  `.incbin "nnue/pawnstar-v7.bin"`) are both v7, so it rejected both and fatal-exited BEFORE fastchess could
  send `setoption EvalFile`. run_sprt deliberately `unset`s PAWNSTAR_EVALFILE and relies on the post-handshake
  EvalFile option — too late. **Fix: a wrapper script as BIN_A that does `exec env PAWNSTAR_EVALFILE=<cand net>
  build/pawnstar "$@"`, so the cand has a compatible net at STARTUP** (base needs none — its embedded v7 is
  fine). `~/pawnstar_nnue/bignet1024/cand_wrapper.sh`. **Rule: any cross-arch SPRT needs the new-arch engine to
  get a matching net at startup, not just via EvalFile** (or build it with a matching embedded net).
- **SHIPPED as v8 (2026-06-17, commit fb961b9, pushed to origin/main).** Done off a fresh main-based
  worktree (preserving main's cleanups): `kHiddenSize=1024`/`kNumKingBuckets=1` + all-zero `kKingBucketMap`;
  `nnue/pawnstar-v8.bin` (1579104 B) added, v7 removed; `embedded_net.S`/Makefile/main.cpp/rate.sh → v8;
  trainer .rs → 1024/single-bank/NUM_BUCKETS=1; `test/nnue_reference.txt` re-baselined from the RAW net
  (test_nnue 0/250 0cp); BK accepted moves regenerated (24/24 at depths 8-11, 5 positions changed);
  embedded fallback verified to load v8; CLAUDE/README/nnue-README updated (arch, sizes, lineage, recipe).
  `make check` green. Net also at `~/pawnstar_nnue/bignet1024/`; the bignet-1024-fulldata branch is now
  redundant (its arch shipped to main). **GOTCHA confirmed: `make check` does NOT build `build/pawnstar`
  (only tests+tools); build the engine separately to test the embedded fallback.**
- Next lever to try: **1024 *with* king buckets** on the same 2.31B (combine the two winning levers), and/or
  yet more data (2.31B is ~11% of 21B).

**Update (2026-06-18) — 1536-wide REJECTED (loses −17 Elo @ 40/20 to v8). NOT shipped. Width sweet spot at 2.31B is 1024.**
- Branch `bignet-1536-fulldata` (commit 75e1fac): `kHiddenSize=1024→1536`, same single-bank/no-king-bucket
  arch as v8, same 2.31B pool + schedule (SBS=120/BPS=3688) — only the width differs from v8. Net
  `~/pawnstar_nnue/bignet1536/pawnstar-1536.bin` (stamped 2368608 B, 1.5× v8's 1579104). run.sh now bakes
  the cand-wrapper into the SPRT step. Verify clean: cross-check 0/250 0cp (raw), incremental 0 mismatches.
- **SPRT 1536 vs v8 @ tc 40/20: Elo −17.39 ± 14.89 (nElo −25.21), 1000 games, Ptnml [57,68,269,80,26],
  LLR −2.51 (toward H0).** CI excludes 0 → 1536 is clearly WORSE on the clock. **Decision: keep v8 (1024).**
- **Lesson: 1024 is the width sweet spot at 2.31B.** Going wider costs more per-move compute (2.37MB net,
  heavier forward pass) than the eval gain returns at 40/20 — same "wins eval?/loses clock" wall that capped
  v5 (1024 on 60M) and the king buckets before the speed fixes. To benefit from >1024 width you'd likely
  need BOTH more data AND speed work (à la v6's eval-cache + single-pass Update). Net kept at
  `~/pawnstar_nnue/bignet1536/`; branch redundant (don't ship).
- **GOTCHA (1536 run): training HUNG once at the end of superbatch 28** — GPU went idle (1%) while one CPU
  thread spun in userspace (R state, empty WCHAN); process alive but frozen ~51 min. Killed by PID and
  relaunched (bullet has no mid-run resume in our setup → restarts from SB1, ~1.7h lost); the restart sailed
  past SB28 and finished. Treat as a transient GPU/driver/loader fluke (the 1024 run never did this). Also:
  **`pkill -f "<path>/run.sh"` matches your OWN command line — use explicit PIDs to kill the trainer.**

**Update (2026-06-18/19) — v9: 1024 + 4 king buckets (file-pair) SHIPPED, beat v8 by +11.4 Elo.**
- Combined the two winning levers: kept v8's 1024 width, **added 4 king buckets selected by king FILE-PAIR**
  (`kKingBucketMap` = file/2: a/b→0, c/d→1, e/f→2, g/h→3), trained on the same ~2.31B pool. SPRT v9 vs v8
  **+11.4 ± 6.75 Elo @ 8+0.08** (LLR 2.96, H1; 5520 games). Holding width fixed, buckets win at scale (gain
  shrinks with data — +29 @ 750M → +11 @ 2.31B — but stays positive). The old "v8: width beats buckets" call
  was width-vs-buckets confounded (v7 was 512+buckets). **File-pair bucketing > quadrant/rank-based** (king
  safety is file-dependent; back-rank kings exercise all 4 file banks). v9 net 6,297,696 B (4× v8's FT rows).
  Shipped as `nnue/pawnstar-v9.bin`; v8 retired.

**Update (2026-06-20) — v10: MORE DATA again. Scaled v9 arch 2.31B→3.82B, beat v9 by +16.3 Elo. SHIPPED (commit 4431461, pushed to main).**
- **Single variable = data.** Kept the v9 arch EXACTLY (1024-wide, 4 file-pair king buckets), added **four
  more PlentyChess shards** (`13247 13399 13364 13381`, ~378M each, ~1.51B new) on top of v9's ~2.31B pool →
  **~3.82B** (3,824,674,998 positions). Trained SBS 120→190 (~3 epochs, ~11.5B samples), BPS=3688, on a
  **GTX 1050 Ti** (~9h19m, ~330K pos/s). Data/checkpoints in `~/pawnstar_nnue/v10/`.
- **SPRT v10 vs v9 (same binary, EvalFile differs, same arch so NO cand-wrapper needed) @ 8+0.08:
  +16.3 ± 10.8 Elo, LLR 2.95 H1 accepted, 1944 games (52.3%), 46 min.** Larger than the v8→v9 bucket gain.
  **More data is the recurring lever** — v10's 3.82B is still only ~18% of the ~21B PlentyChess pool.
- Verify clean (raw net: 0/250 0cp; incremental 0 mismatches); make check green; BK regenerated 24/24
  (9 positions changed). v9 retired from repo (preserved as `~/pawnstar_nnue/pawnstar-v9-baseline.bin` for
  future SPRT baselines — same arch as v10 so still cross-loadable). Docs (CLAUDE/README/nnue-README §7
  lineage+recipe) updated to v10.
- **Reusable workflow notes for the next data scale-up:** the bullet trainer copy in the checkout drifts —
  ALWAYS re-`cp tools/bullet/pawnstar*.rs $BULLET/examples/` + rebuild before training (the installed copy
  was stale at HIDDEN_SIZE=1536 from the rejected experiment). Shuffle of the 122GB pool needs
  `--mem-used-mb 8192` on this 16GB box (not 16384). SB1 4-bank spot-check = quantised.bin 6,297,664 B.
- Next lever: yet more data (still only ~18% of pool), or a finer/more king buckets (8, or file+rank).

**Update (2026-06-22) — v11: MORE DATA again. Scaled v10 arch 3.82B→6.05B, beat v10 by +9.28 Elo. SHIPPED (pushed to main).**
- **Single variable = data.** Kept the v10 arch EXACTLY (1024-wide, 4 file-pair king buckets), added **six
  more PlentyChess shards** (`11756 12450 12862 13128 13148 13419`, ~2.22B new) on top of v10's ~3.82B pool →
  **~6.05B** (6,048,642,450 positions). Built the pool by concatenating v10's `shuffled.data` + the 6 raw
  shards and re-shuffling (saved re-downloading v10's 10). Trained SBS 190→300 (~3 epochs, ~18.1B samples),
  BPS=3688, on the **GTX 1050 Ti** (14h31m, ~345K pos/s, final loss 0.0228). Data/checkpoints in `~/pawnstar_nnue/v11/`.
- **SPRT v11 vs v10 (same binary, EvalFile differs, same arch so NO cand-wrapper) @ 8+0.08: +9.28 ± 6.58 Elo,
  LOS 99.7%, LLR 2.48, 6102 games (51.3%), 0 forfeits** — stopped at clear-H1 (user call) rather than run to formal H1.
- **The gain is diminishing as the base grows**: v9→v10 was +16.3 for +1.5B; v10→v11 +9.3 for +2.2B. Still
  clearly positive and the cheapest lever (no code/arch change). v11's 6.05B is still only ~29% of the ~21B pool.
- Verify clean (raw net: 0/250 0cp; incremental 36807/0 mismatches); make check green; BK regenerated 24/24
  at depths 8-11. v10 retired from repo. Docs (CLAUDE/README/nnue-README §7 lineage+recipe) updated to v11.
- **CUDA env gotcha (this box):** no toolkit at `/usr/local/cuda`; it lives at `~/cuda-12.2`. The prebuilt
  bullet `pawnstar`/`pawnstar_eval` need `LD_LIBRARY_PATH=~/cuda-12.2/targets/x86_64-linux/lib` (else
  libnvrtc.so.12 / libcublas.so.12 'not found'). Same arch as v10, so NO trainer rebuild was needed — reused
  the prebuilt example binary. Shard sizes vary (~58M–379M); target the POSITION count, not a fixed shard
  count. See [[project_tried_search_features]] for the (separate) search-experiment log.

**Update (2026-06-26) — CONFIRMED at 6B: king buckets still worth ~12 Elo (the bucket gain has PLATEAUED, not kept shrinking). NOT a new net; v11 arch reaffirmed.**
- Single-variable A/B at v11's data scale: trained a **1024-wide, NO-king-bucket (single bank)** net on the
  **same ~6B pool re-downloaded + re-shuffled** (16 shards, 5,988,211,081 positions; same SBS=300/BPS=3688
  schedule as v11; final loss 0.02332 vs v11's 0.0228 — slightly worse, an early hint). This is the v8 arch
  at v11's data. SPRT'd vs shipped v11 (1024 + 4 file-pair buckets), candidate engine rebased onto current
  main so the ONLY diff is king buckets (cross-arch, binary-vs-binary, CAND_STARTUP_NET wrapper).
- **Result: buckets WIN at both TCs. 8+0.08 → no-bucket −12.36 ± 9.09 Elo (nElo −16.5, 3150 games, LLR −3.00,
  H0[-10,0]); 12+0.12 → −11.71 ± 8.65 (nElo −16.6, 3088 games, LLR −2.97, H0[-10,0]).** Both decisive; the
  no-bucket net even had a small speed edge (1.58MB FT vs 6.3MB → less cache pressure) and still lost.
- **Key refinement to the "bucket gain shrinks with data" story: it shrank then PLATEAUED.** +29 @ 750M →
  +11.4 @ 2.31B (v9) → **~+12 @ 6B** — i.e. essentially flat from 2.31B to 6B, not continuing toward zero.
  At equal width, buckets remain a firm ~11–12 Elo at scale. v11's 4-file-pair-bucket arch is the right call.
- Reproduce: all-zero the `KING_BUCKETS` map in a copy of the trainer (→ `get_num_buckets`=1) and set engine
  `kNumKingBuckets=1` + zeroed `kKingBucketMap`; trainer variants were `tools/bullet/pawnstar_nb.rs` +
  `pawnstar_eval_nb.rs` (no-bucket), candidate engine on a throwaway worktree. Net/data in
  `~/pawnstar_nnue/exp_nobucket_6b/`. Toolchain was re-bootstrapped from bare metal this session — see
  [[machine-cargo-version-gotcha]] and [[machine-setup-env]]. Open bucket lever now: MORE banks / a finer
  map (file+rank, or mirrored), since 4 buckets clearly still pay at 6B.

**Update (2026-06-27) — 8 KING BUCKETS (file-pair × board-half) BEAT v11 at both TCs. Shippable as v12.**
- Doubled buckets 4→8: kept v11's 1024 width + file-pair files but ALSO split by board half at the middle
  rank (own half ranks 1–4 → banks 0–3, advanced ranks 5–8 → banks 4–7; per-perspective oriented, so a
  king in *its own* half → low bank). `kKingBucketMap` rows 5–8 = `4,4,5,5,6,6,7,7`. Trained on the SAME
  ~5.99B pool (reused `exp_nobucket_6b/shuffled.data`, same SBS=300/BPS=3688), final loss **0.02260 < v11's
  0.0228**. Net `~/pawnstar_nnue/exp_8kb/cand-8kb.bin` (12,589,120 B payload — 2× v11). Single variable vs
  v11 = bucket count (8 vs 4).
- **SPRT 8-bucket vs v11: 8+0.08 → +17.29 ± 8.68 (H1, 3520 games, LLR 2.96); 12+0.12 → +10.97 ± 6.57 (H1,
  5610 games, LLR 2.95).** Both accepted. Smaller + slower-to-decide at 12+0.12 (the 2× net size costs more
  on the clock), but a clear win on both controls.
- **KEY: king buckets are NOT at diminishing returns.** 0→4 was +11.4 (v9); 4→8 is +17/+11 — *bigger*. The
  rank-split captures real king-safety signal (own-half vs advanced king). More/finer buckets is now a live
  lever, not a tapped-out one. Next: 8→more banks, or a finer map (file+rank quadrants), still on the ~6B pool.
- **Required engine change (shipped to main first): heap-allocate Game/Network — the inline ~12.6 MB
  feature_weights_ overflows the 8 MB stack at 8 buckets.** Done via `std::make_unique<Game>()` in main + the
  tests, keeping the FT weights an INLINE member (commit 279f087). NOT a unique_ptr to a separate weight
  array: that block is page-aligned and the 2048-byte column stride then collides in the same cache sets →
  **−10% nps (measured, unloaded; `restrict`/hoist didn't help; hugepages wouldn't either — it's cache-set
  aliasing, not TLB)**. Inline-in-heap-Game puts the base at a struct offset → columns scatter across sets →
  speed-neutral (bench nps parity, node count unchanged). **Lesson: a separate page-aligned weight allocation
  with a power-of-2 column stride is a cache-conflict trap; keep big weight tables inline in a heap object.**
- Layout verified before SPRT: engine EvaluateExact == bullet `pawnstar_eval_8kb` at **0 cp** over 250 FENs
  (incl. black-to-move + advanced-half kings — the cases the rank-invariant 4-bucket map never exercised),
  incremental bit-identical, symmetric material sanity. bullet `ChessBuckets` is stm-relative (FromStr does
  `piece^=8; square^=56`), matching the engine's `WhiteBucket`/`BlackBucket` for both side-to-move cases.
  Trainer is now the canonical `tools/bullet/pawnstar.rs` + `pawnstar_eval.rs` (updated to the 8-bucket map).
  **SHIPPED as v12 (2026-06-27, commit b1211b8, pushed):** `kNumKingBuckets=8`, `nnue/pawnstar-v12.bin`
  (12,589,152 B); v11 retired; `test/nnue_reference.txt` + Bratko-Kopec accepted moves regenerated (24/24 at
  depths 8-11); docs updated; `make check` green (bench signature 6857880). Enabled by the heap-allocate
  Game/Network commit 279f087 (the 12.6 MB inline FT array would overflow the 8 MB stack).

Known pre-existing bug surfaced during matches: frequent "Illegal PV move" warnings from
**both** evaluators — the reported PV *string* sometimes contains an illegal continuation
(TT-collision in PV reconstruction); the move actually played is always legal (0 forfeits).
See [[project_go_port]].
