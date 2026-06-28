# Cross-compile Pawnstar for Windows (x86-64) with clang + the MinGW-w64 sysroot, producing a
# self-contained statically-linked pawnstar.exe (no MinGW runtime DLLs needed). Usage:
#   cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-clang-toolchain.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-win
#   ctest --test-dir build-win --output-on-failure     # runs the .exe under wine (emulator below)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_triple x86_64-w64-windows-gnu)
set(CMAKE_C_COMPILER   clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET   ${_triple})
set(CMAKE_CXX_COMPILER_TARGET ${_triple})
set(CMAKE_ASM_COMPILER_TARGET ${_triple})

# Self-contained binary: statically link the C++ / libgcc / winpthread runtimes.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")

# Run the resulting .exe through wine for ctest (host has no native Windows).
find_program(WINE_EXE wine)
if(WINE_EXE)
  set(CMAKE_CROSSCOMPILING_EMULATOR ${WINE_EXE})
endif()

# Only look for libraries/headers in the target sysroot, but allow host programs (clang, wine).
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
