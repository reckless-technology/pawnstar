#!/usr/bin/env bash
# Cross-compile the Windows pawnstar.exe from Linux (clang + MinGW-w64), via CMake.
#
# Produces a self-contained, statically-linked .exe (the NNUE net + opening book are embedded, so the
# only DLL deps are KERNEL32/msvcrt — nothing to ship alongside it). Requires a clang that can target
# x86_64-w64-windows-gnu, the MinGW-w64 sysroot, CMake >= 3.20 and Ninja. If `wine` is on PATH the
# script also runs `bench` on the result as a smoke test (expected node signature: 6857880).
#
# Usage: tools/build_windows.sh [release|debug] [build_dir]
#   release  (default) -DPAWNSTAR_DEBUGX=OFF — diagnostic counters compiled out; the build to distribute.
#   debug              -DPAWNSTAR_DEBUGX=ON  — keeps the `dbg` UCI counters (slightly slower).
#   build_dir          output directory (default build-win). The .exe lands at <build_dir>/pawnstar.exe.
set -euo pipefail

cd "$(dirname "$0")/.."   # repo root

variant="${1:-release}"
build_dir="${2:-build-win}"
toolchain="cmake/mingw-clang-toolchain.cmake"

case "$variant" in
    release) debugx=OFF ;;
    debug)   debugx=ON  ;;
    *) echo "usage: $0 [release|debug] [build_dir]" >&2; exit 2 ;;
esac

[ -f "$toolchain" ] || { echo "error: $toolchain not found (run from a pawnstar checkout)" >&2; exit 1; }

echo ">> configuring ($variant, DEBUGX=$debugx) -> $build_dir"
cmake -S . -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPAWNSTAR_DEBUGX="$debugx"

echo ">> building"
cmake --build "$build_dir" --target pawnstar

exe="$build_dir/pawnstar.exe"
echo ">> built $exe"
ls -la "$exe" | awk '{printf "   size: %.1f MB\n", $5/1048576}'

if command -v wine >/dev/null 2>&1; then
    echo ">> smoke test (wine: bench)"
    WINEDEBUG=-all printf 'bench\nquit\n' | wine "$exe" 2>/dev/null \
        | grep -oE '[0-9]+ nodes [0-9]+ nps' | tail -1 | sed 's/^/   /'
    echo "   (expected node signature: 6857880)"
else
    echo ">> wine not found; skipping smoke test"
fi
