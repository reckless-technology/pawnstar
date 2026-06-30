---
name: project_apple_silicon_port
description: "Apple Silicon (arm64 macOS) port — Phase 1 DONE (engine builds, runs, make check green on M1 with the SCALAR NNUE fallback); Phase 2 (NEON-vectorize the NNUE kernels) PENDING. The dev machine is now an Apple M1 Pro, not the old Linux/CUDA box."
metadata:
  node_type: memory
  type: project
---

**The dev machine migrated to an Apple M1 Pro (arm64, macOS Darwin 25.x, Apple clang 21).** This supersedes
the Linux/RTX-4070/CUDA stack in [[machine-setup-env]] for *engine* work (no NNUE training stack exists here:
no CUDA/bullet/fastchess/anchor engines — those need the Linux box or a re-bootstrap). `-mavx2`/`-mbmi2` are
**hard errors** on this target, so the engine did not build at all until the port below.

**Phase 1 — make it build/run/test on Apple Silicon with the SCALAR NNUE: DONE (2026-06-30).** `make` and
`make check` are **green (exit 0)** on M1. Changes (all behind arch/OS guards, x86 paths byte-unchanged):
- **Build flags** (Makefile + CMakeLists.txt): detect arm64 via `uname -m`/`CMAKE_SYSTEM_PROCESSOR`; use
  `-mcpu=apple-m1` (Apple) / `-march=armv8.2-a+dotprod` (other aarch64) instead of `-mbmi2 -mavx2`.
- **platform.h**: `<cpuid.h>` is x86-only and won't compile on ARM — gate it behind a new `PAWNSTAR_X86`
  macro; `HasAvxVnni()` returns a constant false off x86 (it's only consulted inside `#if defined(__AVX2__)`).
- **transposition_table.h**: dropped the unconditional x86 `<immintrin.h>`; `_mm_prefetch(...,_MM_HINT_T2)`
  → portable `__builtin_prefetch(ptr, 0, 1)` (clang/gcc, both ISAs).
- **embedded_net.S / embedded_book.S**: added a Mach-O branch — `.section __TEXT,__const` and the
  **leading-underscore symbol** (`_pawnstar_embedded_net`) via an `EMBED_SYMBOL()` cpp macro (Mach-O mangles
  C symbols with `_`). `.incbin` itself works under clang's integrated assembler.
- **main.cpp**: `ExecutablePath()` gained an `__APPLE__` branch using `_NSGetExecutablePath`
  (`<mach-o/dyld.h>`) + `std::filesystem::canonical`.

**Correctness is PROVEN, not assumed.** The scalar NNUE path is bit-identical to the AVX2 path by
construction (exact integer SCReLU/dot), and the gates confirm it on ARM: `test_nnue` **max |ref16-bullet| =
0 cp** (EvaluateExact exactly reproduces the trainer), int8 within bound (28 cp vs x86's documented 26 —
immaterial, well under the 40 cp gate); `test_nnue_incremental` 36807 checks bit-identical; **perft 770/770 =
5,981,754,399 nodes** (the exact published count); SEE 24/24.

**Two macOS-environment gotchas that are NOT engine bugs (cost real time to diagnose):**
1. **bench node count is 7,628,404 on macOS, NOT the Linux 6,857,880** — and it IS deterministic across runs.
   (SUPERSEDED 2026-06-30: `std::sort` was replaced with a deterministic stable merge sort — see
   [[project_deterministic_move_sort]] — so the cross-platform bench is now a single value, **7451559**, and
   the per-STL BK divergence below is gone. The diagnosis still stands as the reason that change was made.)
   Cause: **libc++ `std::sort` tie-breaks equal-ordering-score moves differently than libstdc++**, changing
   the (still-legal, still-correct) search tree. Same class as the MSVC-STL divergence in
   [[machine-windows-native-build]]. So bench is a per-STL signature, not cross-platform. BK then solved only
   22/24 at depth 8 → fixed the documented way (union accepted moves over depths 8–11): added the libc++
   tie-break moves to `test/bratko_kopec_nnue_test.cpp` kCases — **d1b1 (pos07), f1b5/f4f5 (pos09), e8e7
   (pos19)**. Now 24/24 at depths 8–11.
2. **macOS stock `/bin/bash` is 3.2.57** (2007) and lacks `coproc` (bash-4+), which `test/uci_test.sh` uses,
   and **BSD `date` lacks GNU's `+%s%3N`** (broke the `make check` timing line with "value too great for
   base"). Fixes: user ran `brew install bash` → `/opt/homebrew/bin/bash` (5.3) is first on PATH, so the
   Makefile's `bash` picks it up (no script change); and the `check` recipe's timer now uses a portable
   `now_ms()` helper (GNU `%s%3N` when all-digits, else `date +%s`×1000 → coarse seconds-as-ms on BSD).

**clang-format is NOT installed here** (repo pins 18.1.8 — [[feedback_clang_format]]); run it on the edited
C++ files before committing (`pip install clang-format==18.1.8`). Nothing committed/pushed yet — user wanted
to review Phase 1 first.

**Phase 2 — hand-written NEON NNUE kernels: TRIED & REJECTED (2026-06-30), reverted.** Implemented full
`<arm_neon.h>` kernels in nnue.h behind `#elif defined(__ARM_NEON)`: AddColumn/SubColumn (`vaddq_s16`/
`vsubq_s16`), a `NeonScReluI8` (clamp→`vmull_s16` square→`+round`→`vshrq_n_s32`→narrow to int8), and a
`vdotq_s32` output dot (M1 has **FEAT_DotProd=1 but FEAT_I8MM=0**, so SDOT with activations reinterpreted as
int8 — lossless since `kInt8Shift=9` caps them at [0,127]). **Bit-identical** (bench stayed 7,628,404,
test_nnue 28cp, test_nnue_incremental green). **But measured ~4–5% SLOWER than the plain scalar `#else`
path: a clean A/B (force-scalar `-DPAWNSTAR_SCALAR_NNUE` build vs default NEON, 10 interleaved full-bench
runs, identical node counts) had the scalar build win 10/10, median neon/scalar = 0.955.** Why: **clang
`-O3 -mcpu=apple-m1` already auto-vectorizes the "scalar" loops to NEON** — `objdump` of the scalar
NNUE object shows 164 `add.8h`/`sub.8h`-class int16x8 ops + 89 `.4s` + 32 `.16b` (column updates and a
widened dot), and clang's scheduling/unrolling beats the hand intrinsics (esp. the dominant column-update
path; the NeonScReluI8 narrow chain also adds overhead). **Exactly the [[project_tried_asm_kernels]]
lesson on ARM: clang's codegen wins.** Reverted via `git checkout src/nnue.h` (Phase 1 never touched
nnue.h), so the shipped ARM build uses the scalar fallback **which clang auto-vectorizes to NEON** — i.e.
the engine IS vectorized, just by the compiler, not by hand. **Don't re-add hand NEON without a
fundamentally better kernel** (e.g. wider-unrolled columns matching clang, or a fused no-narrow dot) AND a
measured win. perft on M1 Pro: **~437 Mnps** single-thread (vs the README's ~600 on an i9-13900HX).
Related: [[project_nnue_experiment]], [[project_tried_asm_kernels]], [[project_tried_update_prefetch]].
