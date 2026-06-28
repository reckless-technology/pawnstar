---
name: project_windows_port
description: "Pawnstar is now portable to Windows (clang only, no MSVC) via CMake; validated by cross-compiling from Linux (clang+MinGW-w64) and running under Wine — bench BYTE-IDENTICAL to Linux (6857880), ctest 7/7. The GNU Makefile remains the primary Linux build."
metadata:
  type: project
---

**Pawnstar was ported to Windows (2026-06-28, commits 8edab83→498fbb5 on main).** Clang only — **MSVC is
NOT supported** (the engine relies on clang/GCC builtins and the `[[gnu::target("avx2,avxvnni")]]` VNNI
kernel). The GNU **Makefile stays the primary Linux build**; a new **`CMakeLists.txt`** builds the same
engine/tools/tests on both OSes and registers `ctest` to mirror `make check`.

**What was platform-specific (all the engine needed):**
- `bitboard.h`: `__builtin_ctzll/clzll/popcountll` → C++20 `<bit>` (`std::countr_zero/countl_zero/popcount`) —
  zero `#ifdef`, the cleanest fix.
- `transposition_table.h`: `__builtin_prefetch(p,0,1)` → `_mm_prefetch(p, _MM_HINT_T2)`.
- new **`src/platform.h`**: `platform::HasAvxVnni()` (cpuid) wraps GCC/Clang `<cpuid.h>` `__get_cpuid_count`
  vs MSVC `<intrin.h>` `__cpuidex`; nnue.h's `kCpuHasAvxVnni` uses it. (Kept lightweight — no `<windows.h>` —
  so it's safe to include from the engine headers.)
- `main.cpp`: `ExecutablePath()` = `/proc/self/exe` (POSIX) vs `GetModuleFileNameW` (`<windows.h>`, contained
  to that one TU with `WIN32_LEAN_AND_MEAN`/`NOMINMAX`). Dropped dead `<execinfo.h>/<signal.h>/<unistd.h>`.
- `embedded_net.S`/`embedded_book.S`: guard the ELF-only directives (`.rodata`, `.note.GNU-stack`, `@progbits`)
  behind `#if defined(__ELF__)`; COFF uses `.section .rdata,"dr"`. `.incbin` works on both via clang's
  integrated assembler.
- `setenv` (POSIX) is in `test/bratko_kopec_nnue_test.cpp` + `tools/filter_book.cpp` (NOT the engine) →
  `_putenv_s` on `_WIN32`.
- attacks.h needed NOTHING — clang defines `__BMI2__`/`__GNUC__` with `-mbmi2`.

**Cross-compile + validation (done from THIS Linux box, no Windows machine):**
`cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-clang-toolchain.cmake` then
`cmake --build build-win` → `pawnstar.exe`. The toolchain file targets `x86_64-w64-windows-gnu`, **static-links**
(`-static`) so the .exe is self-contained (only KERNEL32/msvcrt deps — no MinGW runtime DLLs), and sets
`CMAKE_CROSSCOMPILING_EMULATOR=wine` so `ctest` runs the .exe suites under Wine. Result: **bench = 6857880
nodes, byte-identical to Linux** (so eval+search match exactly across platforms), eval matches, **ctest 7/7
green** (perft, see, opening_book, chess_clock, BK 24/24, nnue 0cp cross-check, nnue_incremental bit-identical;
the bash uci_test is auto-skipped when cross-compiling).

**Gotchas:**
- `__builtin_cpu_supports("avxvnni")` is NOT available in clang 18 → use raw cpuid (see [[project_tried_search_features]]
  VNNI note). platform.h does this.
- MinGW std::thread/std::filesystem need a thread-capable libstdc++. Dynamic linking → wine fails with exit 53
  (missing libstdc++/libgcc/winpthread DLLs); **static-link (`-static`)** fixes it and is what you want to ship
  anyway. Debian's win32-threads libstdc++ emits cosmetic "duplicate section ... different size" lld warnings
  on static link — non-fatal, the .exe runs correctly (GCC 13 win32 model supports std::thread).
- ctest's `CMAKE_CROSSCOMPILING_EMULATOR` only runs TARGET commands under wine; a bash-script test (uci_test.sh)
  would run the .exe natively → guarded with `NOT CMAKE_CROSSCOMPILING`.

**Toolchain now on this box (2026-06-28):** `mingw-w64` (posix+win32 g++ variants at /usr/x86_64-w64-mingw32,
gcc 13), `wine` 9.0. See [[machine-setup-env]]. Native-Windows validation (real GUI, native perf) is the one
remaining manual step that needs an actual Windows machine — everything else is proven via cross-compile+Wine.
