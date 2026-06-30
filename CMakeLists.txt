# Cross-platform build for Pawnstar (Linux + Windows, clang). The repo's GNU Makefile remains the primary
# Linux build; this CMake file mirrors it and adds Windows support. Pawnstar is a header-only engine — every
# class lives in src/*.h as inline definitions — so each executable is a single translation unit that pulls
# the engine in through its headers (main.cpp is the UCI entry point; tests and tools are standalone).
#
# Configure + build (any platform):
#     cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#     cmake --build build
#     ctest --test-dir build --output-on-failure
#
# Windows: use a clang toolchain (clang++ / clang-cl, or a MinGW cross toolchain file). MSVC is not
# supported (the engine uses clang/GCC builtins and a function-level target attribute for the VNNI kernel).

cmake_minimum_required(VERSION 3.20)
project(pawnstar LANGUAGES CXX ASM)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Engine ISA, per architecture. On x86-64: AVX2 (NNUE SIMD); -mbmi2 only lets the compiler emit BMI1
# tzcnt/blsr for std::countr_zero (sliding attacks use magic bitboards, not pext). The AVX-VNNI output-dot
# kernel is compiled behind a function-level target attribute and chosen at runtime by cpuid, so no global
# -mavxvnni is needed and the binary still runs on AVX2-only CPUs. On arm64 (Apple Silicon / aarch64) those
# x86 options are hard errors; target a baseline with the AdvSIMD dot-product extension (FEAT_DotProd) the
# NEON NNUE kernels use, with the SIMD scalar fallback in between.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
  if(APPLE)
    add_compile_options(-mcpu=apple-m1)
  else()
    add_compile_options(-march=armv8.2-a+dotprod)
  endif()
else()
  add_compile_options(-mbmi2 -mavx2)
endif()
add_compile_options(-Wall -Wextra -Wpedantic)
add_compile_options($<$<CONFIG:Release>:-O3>)
add_compile_options($<$<CONFIG:RelWithDebInfo>:-O3>)

# On Windows, clang targets the MSVC ABI and pulls in Microsoft's UCRT headers, which mark standard C
# functions (getenv, setbuf, ...) "deprecated/insecure" via _CRT_INSECURE_DEPRECATE. Those uses are correct,
# portable C++ here (getenv has no portable non-deprecated replacement), so silence the noise — before
# WERROR turns it into errors. No effect on the Linux/MinGW builds.
if(WIN32)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

option(WERROR "Treat warnings as errors (CI)" OFF)
if(WERROR)
  add_compile_options(-Werror)
endif()

# DEBUGX diagnostic counters (the `dbg` command) — on by default, like the Makefile; turn off for releases.
option(PAWNSTAR_DEBUGX "Compile in the DEBUGX diagnostic counters" ON)
if(PAWNSTAR_DEBUGX)
  add_compile_definitions(DEBUGX=1)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- Shipped resources embedded into the engine binary via .incbin (runtime-load fallback). Absolute paths
#     so .incbin resolves regardless of the (out-of-tree) build directory; the .S files rebuild if they change.
set(EMBED_NET  "${CMAKE_CURRENT_SOURCE_DIR}/nnue/pawnstar-v12.bin")
set(EMBED_BOOK "${CMAKE_CURRENT_SOURCE_DIR}/pawnstar.book")
set_source_files_properties(src/embedded_net.S PROPERTIES
  COMPILE_DEFINITIONS "EMBED_NET_PATH=\"${EMBED_NET}\"" OBJECT_DEPENDS "${EMBED_NET}")
set_source_files_properties(src/embedded_book.S PROPERTIES
  COMPILE_DEFINITIONS "EMBED_BOOK_PATH=\"${EMBED_BOOK}\"" OBJECT_DEPENDS "${EMBED_BOOK}")

# --- Globally-reproducible build number (the git commit count) stamped into the engine version. Mirrors the
#     Makefile's VERSION_HEADER: a per-build script writes a header defining PAWNSTAR_BUILD_NUMBER (only when
#     the number changes), which is force-included into main.cpp (so version.h picks it up and the UCI "id
#     name" carries the build number). The custom target has no OUTPUT, so it runs on every build.
set(PAWNSTAR_BUILD_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/pawnstar_build_number.h")
add_custom_target(pawnstar_build_number
  BYPRODUCTS "${PAWNSTAR_BUILD_HEADER}"
  COMMAND ${CMAKE_COMMAND}
    -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
    -DHEADER_FILE=${PAWNSTAR_BUILD_HEADER}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/write_build_number.cmake
  COMMENT "Writing Pawnstar build number"
  VERBATIM)

# --- Engine (entry point + the two embedded-resource objects, like the Makefile's engine link). ---
add_executable(pawnstar src/main.cpp src/embedded_net.S src/embedded_book.S)
add_dependencies(pawnstar pawnstar_build_number)
set_source_files_properties(src/main.cpp PROPERTIES
  COMPILE_OPTIONS "-include;${PAWNSTAR_BUILD_HEADER}")

# Name the output executable pawnstar_<major>_<minor>_<build> (+ .exe on Windows), matching the Makefile.
# Major/minor are read from src/version.h (single source of truth); the build number is the git commit count.
# These are resolved at configure time, so reconfigure after a commit to refresh the filename (the embedded
# version always tracks via the per-build header above).
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h" _version_h)
string(REGEX MATCH "kVersionMajor = ([0-9]+)" _ "${_version_h}")
set(_vmajor "${CMAKE_MATCH_1}")
string(REGEX MATCH "kVersionMinor = ([0-9]+)" _ "${_version_h}")
set(_vminor "${CMAKE_MATCH_1}")
execute_process(
  COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}" rev-list --count HEAD
  OUTPUT_VARIABLE _vbuild OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE _vrc ERROR_QUIET)
if(NOT _vrc EQUAL 0 OR NOT _vbuild MATCHES "^[0-9]+$")
  set(_vbuild 0)
endif()
set_target_properties(pawnstar PROPERTIES OUTPUT_NAME "pawnstar_${_vmajor}_${_vminor}_${_vbuild}")

# Stable convenience name (build/pawnstar[.exe]) pointing at the version-named binary, so tooling, scripts
# and docs can use a fixed path. A symlink on Linux/macOS; a copy on Windows (symlinks need privilege there).
# Mirrors the Makefile's PROGRAM_ALIAS.
if(WIN32)
  add_custom_command(TARGET pawnstar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:pawnstar>" "$<TARGET_FILE_DIR:pawnstar>/pawnstar${CMAKE_EXECUTABLE_SUFFIX}"
    VERBATIM)
else()
  add_custom_command(TARGET pawnstar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      "$<TARGET_FILE_NAME:pawnstar>" "$<TARGET_FILE_DIR:pawnstar>/pawnstar${CMAKE_EXECUTABLE_SUFFIX}"
    VERBATIM)
endif()

# --- Helper tools. ---
add_executable(stamp_net        tools/stamp_net.cpp)
add_executable(filter_book      tools/filter_book.cpp)
add_executable(nnue_quant_study tools/nnue_quant_study.cpp)
add_executable(dump_magics      tools/dump_magics.cpp)

# --- Tests (each a standalone TU with its own main()). ---
enable_testing()
set(PAWNSTAR_TESTS perft see bratko_kopec_nnue nnue nnue_incremental opening_book chess_clock)
foreach(t IN LISTS PAWNSTAR_TESTS)
  add_executable(test_${t} test/${t}_test.cpp)
endforeach()

# ctest registration mirrors `make check`. Resource-free suites first; run from the source tree so any
# cwd-relative resource (e.g. the opening book) resolves.
set(NNUE_REF "${CMAKE_CURRENT_SOURCE_DIR}/test/nnue_reference.txt")
foreach(t perft see opening_book chess_clock)
  add_test(NAME ${t} COMMAND test_${t} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
# NNUE suites take the net (and reference) explicitly; only register them if the shipped net is present
# (so a checkout without the .bin still configures, matching the Makefile's $(wildcard) degrade-to-no-op).
if(EXISTS "${EMBED_NET}")
  add_test(NAME bratko_kopec_nnue COMMAND test_bratko_kopec_nnue "${EMBED_NET}"
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME nnue COMMAND test_nnue "${EMBED_NET}" "${NNUE_REF}"
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME nnue_incremental COMMAND test_nnue_incremental "${EMBED_NET}"
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(bratko_kopec_nnue PROPERTIES ENVIRONMENT "PAWNSTAR_THREADS=1")
endif()
# The UCI integration test is a bash script — register it only where a shell is available (skipped on a
# bash-less Windows host). HINTS point at Git for Windows' bash and are searched before the system PATH, so
# on Windows we prefer Git Bash (which understands C:/... paths) over the WSL `bash.exe` stub in System32
# (which cannot read a Windows-style script path); on Linux these hints don't exist and it falls through to
# the system /bin/bash.
find_program(BASH_EXE NAMES bash
  HINTS
    "$ENV{ProgramFiles}/Git/bin"
    "$ENV{ProgramFiles}/Git/usr/bin"
    "$ENV{ProgramW6432}/Git/bin"
    "$ENV{ProgramW6432}/Git/usr/bin"
    "$ENV{ProgramFiles\(x86\)}/Git/bin")
if(BASH_EXE AND NOT CMAKE_CROSSCOMPILING)
  add_test(NAME uci COMMAND ${BASH_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/test/uci_test.sh $<TARGET_FILE:pawnstar>
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
