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

# Engine ISA: BMI2 (pext sliding attacks) + AVX2 (NNUE SIMD). The AVX-VNNI output-dot kernel is compiled
# behind a function-level target attribute and chosen at runtime by cpuid, so no global -mavxvnni is needed
# and the binary still runs on AVX2-only CPUs.
add_compile_options(-mbmi2 -mavx2 -Wall -Wextra -Wpedantic)
add_compile_options($<$<CONFIG:Release>:-O3>)
add_compile_options($<$<CONFIG:RelWithDebInfo>:-O3>)

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

# --- Engine (entry point + the two embedded-resource objects, like the Makefile's engine link). ---
add_executable(pawnstar src/main.cpp src/embedded_net.S src/embedded_book.S)

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
# bash-less Windows host).
find_program(BASH_EXE bash)
if(BASH_EXE AND NOT CMAKE_CROSSCOMPILING)
  add_test(NAME uci COMMAND ${BASH_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/test/uci_test.sh $<TARGET_FILE:pawnstar>
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
