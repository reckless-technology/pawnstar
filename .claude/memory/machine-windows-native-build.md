---
name: machine-windows-native-build
description: Building/testing pawnstar natively on the user's Windows 11 box (VS-bundled clang + cmake/ninja, MSVC-STL caveats)
metadata:
  type: project
---

On the user's **Windows 11** dev machine the engine builds with **Visual Studio 2026 (v18) Community's bundled clang**, NOT MinGW (that's the separate cross-compile-from-Linux path — see [[project_windows_port]]). clang/cmake/ninja are NOT on PATH; prepend them per session:

```
$vs = "C:\Program Files\Microsoft Visual Studio\18\Community"
$env:PATH = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$vs\VC\Tools\Llvm\x64\bin;$env:PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_ASM_COMPILER=clang
cmake --build build
ctest --test-dir build --output-on-failure        # 8/8 green
```

The Clang compiler is an optional VS component ("C++ Clang Compiler for Windows"), installed via the VS Installer GUI (the CLI `setup.exe modify` needs elevation AND the install path quoted, or it splits on the space in "Program Files"). Installed clang is **22.x targeting `x86_64-pc-windows-msvc`** → uses **Microsoft's STL**, not libstdc++/libc++. That STL differs from the MinGW path CLAUDE.md's "Windows with clang" docs assume, and it surfaced three issues (all fixed on 2026-06-29):

- `std::array::iterator` is a checked class, not a raw pointer → `&elem - container.begin()` won't compile. Use `&elem - &container[0]` (fixed at 3 sites in game.h / search_state.h).
- `std::sort` tie-breaks equal-ordering-score moves differently than libstdc++, cascading at fixed depth to a different (still-legal) best move. The Bratko-Kopec move-gate (`test/bratko_kopec_nnue_test.cpp`) needed this build's moves added: pos06 `c2c4`, pos23 `c8f5` (depth 8), pos03 `e7d8` (depth 11). NNUE eval itself is bit-correct here (`test_nnue`/`test_nnue_incremental` pass) — the divergence is pure sort order, not a miscompile.
- ctest's `find_program(bash)` grabbed WSL's `System32\bash.exe` (can't read `C:/...` paths) → uci suite failed. CMakeLists.txt now HINTS at Git Bash first; on a fresh cache, clear it with `cmake -U BASH_EXE` then reconfigure.

The stale `out/build/x64-Debug` is an MSVC (cl.exe) configure — unusable (MSVC can't build this engine); ignore/delete it and build into `build/`.
