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

# build/compile_commands.json, the code model VS Code's C/C++ extension reads on every platform (.vscode).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
# Optimisation/debug flavours matching the Makefile: Release is -O3 with debug info kept (the Makefile's
# default `-g -O3 -DNDEBUG`; CMake defines NDEBUG for Release), Debug is `-Og -g` with asserts on (-DDEBUG).
add_compile_options($<$<CONFIG:Release>:-O3>)
add_compile_options($<$<CONFIG:Release>:-g>)
add_compile_options($<$<CONFIG:RelWithDebInfo>:-O3>)
add_compile_options($<$<CONFIG:Debug>:-Og>)
add_compile_options($<$<CONFIG:Debug>:-g>)
add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)
# clang targeting the MSVC ABI writes a .pdb only when -g reaches the link too, so a debugger has symbols there.
# The C and C++ runtimes are linked statically (/MT, the debug one in a Debug build), so the engine imports system DLLs
# alone - KERNEL32 - and runs on a Windows with no Visual C++ redistributable: no VCRUNTIME140.dll, MSVCP140.dll or
# UCRT api-ms-win-crt-*.dll (tools/windows-dependents.ps1 holds the shipped binary to that). A sanitizer build moves
# to the DLL runtime below, the only one AddressSanitizer supports there.
if(WIN32)
  add_link_options($<$<CONFIG:Release,RelWithDebInfo,Debug>:-g>)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Optional AddressSanitizer + UndefinedBehaviorSanitizer (the Makefile's DEBUG=1), applied to compile and
# link. The Makefile pairs this with a Debug build; the Makefile wrapper sets Debug + PAWNSTAR_SANITIZE together.
option(PAWNSTAR_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
if(PAWNSTAR_SANITIZE)
  # -fno-sanitize-recover=undefined: UBSan reports undefined behaviour and then aborts, so a finding fails the test;
  # by default it prints and carries on, and the process still exits 0.
  add_compile_options(-fsanitize=undefined -fsanitize=address -fno-sanitize-recover=undefined)
  add_link_options(-fsanitize=undefined -fsanitize=address)
  # On Windows the sanitizer runtime supports only the release C runtime: under the debug one (ucrtbased) a Debug
  # build reports its own allocations as a "bad-free" on the first realloc.
  if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  endif()
endif()

# Optional source-based code coverage (clang's -fprofile-instr-generate -fcoverage-mapping), applied to compile and
# link. With it on, the `coverage` target (below) runs every ctest with LLVM_PROFILE_FILE pointing into
# <build>/prof, merges the raw profiles with llvm-profdata and reports src/'s line coverage with llvm-cov. The
# instrumented binaries are slower and their counters are not the release build's: never ship or bench one.
option(PAWNSTAR_COVERAGE "Instrument for source-based code coverage (clang)" OFF)
# The coverage gate: the `coverage` target fails when src/'s line coverage is under this percentage. One number, read
# by CI's coverage job and by a developer's `make coverage` / `tools\build.ps1 coverage` alike (override it with
# -DPAWNSTAR_COVERAGE_MIN_LINES=..., `make coverage COVERAGE_MIN_LINES=...` or -CoverageMinLines; 0 reports only).
set(PAWNSTAR_COVERAGE_MIN_LINES 90 CACHE STRING "Minimum src/ line coverage, in percent, for the coverage target")
if(PAWNSTAR_COVERAGE)
  add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
  add_link_options(-fprofile-instr-generate)
endif()

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

# --- The NNUE network shape. Two knobs, so either shape can be built from one tree while the three
#     bit-exact ports (C++, Rust, Go) are brought across one at a time. The defaults are the v12 shape the
#     shipped net was trained for; the shared architecture is mirroring ON with 8 output heads:
#
#       cmake -S . -B build-new-shape -G Ninja -DCMAKE_CXX_COMPILER=clang++ \
#             -DPAWNSTAR_NNUE_KING_MIRRORING=ON -DPAWNSTAR_NNUE_OUTPUT_BUCKETS=8
#
#     Both are stamped into every net's header (`flags` bit 0, `output_buckets`, and the bucket map's
#     CRC-64), and the loader checks all three, so a net can only ever load into a build of its own shape —
#     a new-shape build refuses pawnstar-v12.bin, which is correct, not a fault. A shape change is also an
#     evaluation change, so it moves the bench signature and breaks parity with the Rust and Go ports until
#     all three are ported together: do not flip these defaults for one engine alone.
option(PAWNSTAR_NNUE_KING_MIRRORING
       "NNUE: mirror a perspective whose own king is on files e-h (the shared architecture; +14.3 Elo)" OFF)
set(PAWNSTAR_NNUE_OUTPUT_BUCKETS "1" CACHE STRING
    "NNUE: material output heads, selected by (piece_count - 2) / ceil(32 / N). 8 in the shared architecture (+14.7 Elo)")
if(PAWNSTAR_NNUE_KING_MIRRORING)
  add_compile_definitions(PAWNSTAR_NNUE_KING_MIRRORING=1)
endif()
add_compile_definitions(PAWNSTAR_NNUE_OUTPUT_BUCKETS=${PAWNSTAR_NNUE_OUTPUT_BUCKETS})
message(STATUS "Pawnstar NNUE shape: king mirroring ${PAWNSTAR_NNUE_KING_MIRRORING}, "
               "${PAWNSTAR_NNUE_OUTPUT_BUCKETS} output head(s)")

# Is the shipped net's shape this build's? v12 was trained with mirroring off and one output head, so
# anything else means the engine will REFUSE it — correctly, since nothing has been trained at the other
# shape yet. The test registration below needs to know, because a suite handed a net it cannot load fails
# rather than skipping, and `make check KING_MIRRORING=1 OUTPUT_BUCKETS=8` — the command this file's own
# comment above recommends — was red for seven of fourteen suites on that account.
if(PAWNSTAR_NNUE_KING_MIRRORING OR NOT PAWNSTAR_NNUE_OUTPUT_BUCKETS STREQUAL "1")
  set(PAWNSTAR_SHIPPED_NET_IS_THIS_SHAPE OFF)
else()
  set(PAWNSTAR_SHIPPED_NET_IS_THIS_SHAPE ON)
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

# --- The version, MAJOR.MINOR.BUILD, from the git tags (cmake/version.cmake): PAWNSTAR_VERSION, and a generated header
#     force-included into main.cpp, so version.h picks it up and the UCI "id name" carries it.
include(cmake/version.cmake)

# --- Engine (entry point + the two embedded-resource objects, like the Makefile's engine link). ---
add_executable(pawnstar src/main.cpp src/embedded_net.S src/embedded_book.S)
set_source_files_properties(src/main.cpp PROPERTIES
  COMPILE_OPTIONS "-include;${PAWNSTAR_VERSION_HEADER}" OBJECT_DEPENDS "${PAWNSTAR_VERSION_HEADER}")

# Name the output executable pawnstar_<major>_<minor>_<build> (+ .exe on Windows), from the version above.
string(REPLACE "." "_" _pawnstar_version_underscored "${PAWNSTAR_VERSION}")
set_target_properties(pawnstar PROPERTIES OUTPUT_NAME "pawnstar_${_pawnstar_version_underscored}")

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
add_executable(gen_data         tools/gen_data.cpp)
add_executable(gen_openings     tools/gen_openings.cpp)
add_executable(lichess_extract  tools/lichess_extract.cpp)
add_executable(lc0_extract      tools/lc0_extract.cpp)
add_executable(stamp_net        tools/stamp_net.cpp)
add_executable(filter_book      tools/filter_book.cpp)
add_executable(nnue_quant_study tools/nnue_quant_study.cpp)
add_executable(dump_magics      tools/dump_magics.cpp)

# gen_data stamps the engine version into each shard's .meta provenance sidecar, so it needs the generated
# version header force-included the way the engine's own translation unit does (without it VersionString()
# falls back to version.h's 0.0.0 placeholder, which would make the provenance useless).
set_source_files_properties(tools/gen_data.cpp PROPERTIES
  COMPILE_OPTIONS "-include;${PAWNSTAR_VERSION_HEADER}" OBJECT_DEPENDS "${PAWNSTAR_VERSION_HEADER}")

# gen_openings stamps the engine version into the provenance header of the opening set it writes, so it needs
# the generated version header force-included the same way.
set_source_files_properties(tools/gen_openings.cpp PROPERTIES
  COMPILE_OPTIONS "-include;${PAWNSTAR_VERSION_HEADER}" OBJECT_DEPENDS "${PAWNSTAR_VERSION_HEADER}")

# lichess_extract stamps the engine version into each shard's .meta provenance sidecar, the same reason
# gen_data does, so it needs the generated version header force-included too.
set_source_files_properties(tools/lichess_extract.cpp PROPERTIES
  COMPILE_OPTIONS "-include;${PAWNSTAR_VERSION_HEADER}" OBJECT_DEPENDS "${PAWNSTAR_VERSION_HEADER}")

# lc0_extract stamps the engine version into each shard's .meta provenance sidecar, the same reason gen_data
# does, so it needs the generated version header force-included too.
set_source_files_properties(tools/lc0_extract.cpp PROPERTIES
  COMPILE_OPTIONS "-include;${PAWNSTAR_VERSION_HEADER}" OBJECT_DEPENDS "${PAWNSTAR_VERSION_HEADER}")

# --- Tests (each a standalone TU with its own main()). ---
enable_testing()
set(PAWNSTAR_TESTS perft see bratko_kopec_nnue nnue nnue_incremental opening_book chess_clock bench_golden
                   uci_commands primitives principal_variation lichess_extract lc0_extract)
foreach(t IN LISTS PAWNSTAR_TESTS)
  add_executable(test_${t} test/${t}_test.cpp)
endforeach()

# The extraction rules live in headers under tools/ (the only tool headers a test drives directly), so that
# directory goes on those two tests' include paths. Not added globally: nothing else should reach into tools/.
target_include_directories(test_lichess_extract PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools)
target_include_directories(test_lc0_extract     PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools)

# ctest registration mirrors `make check`. Resource-free suites first; run from the source tree so any
# cwd-relative resource (e.g. the opening book) resolves.
set(NNUE_REF "${CMAKE_CURRENT_SOURCE_DIR}/test/nnue_reference.txt")

# The net the shape-independent suites are given. At the shipped net's own shape that is the shipped net; at
# any other it is the SYNTHETIC net, written by test_nnue_incremental from a fixed seed through fixed
# integer arithmetic, so its bytes are identical on every platform and in all three bit-exact ports. Those
# suites check properties that hold for any weights at all — an incrementally-updated accumulator equals a
# from-scratch rebuild, a published principal variation replays, every UCI command does what it says — so
# giving them a net of the build's own shape makes them RUN at a new shape instead of being skipped there,
# which is the half of the coverage a shape change most needs. The three suites whose expected values belong
# to v12 specifically (the trainer reference evaluations, the Bratko-Kopec move set, the 6,601,454 bench
# signature) keep the shipped net and skip themselves on nnue::IsNetFileThisBuildsShape.
set(PAWNSTAR_SHAPE_NET "${EMBED_NET}")
if(NOT PAWNSTAR_SHIPPED_NET_IS_THIS_SHAPE)
  set(PAWNSTAR_SHAPE_NET "${CMAKE_BINARY_DIR}/synthetic-net.bin")
  # Built with ALL so a plain `cmake --build` produces it, and through OUTPUT rather than POST_BUILD so it
  # is written again if it is deleted while the test binary is still up to date.
  add_custom_command(OUTPUT "${PAWNSTAR_SHAPE_NET}"
                     COMMAND test_nnue_incremental --write-synthetic-net "${PAWNSTAR_SHAPE_NET}"
                     DEPENDS test_nnue_incremental
                     COMMENT "Writing the synthetic net at this build's NNUE shape")
  add_custom_target(synthetic_net ALL DEPENDS "${PAWNSTAR_SHAPE_NET}")
  message(STATUS "Pawnstar NNUE shape: the shipped net is of another shape, so the shape-independent "
                 "suites run against ${PAWNSTAR_SHAPE_NET}")
endif()
foreach(t perft see opening_book chess_clock primitives lichess_extract lc0_extract)
  add_test(NAME ${t} COMMAND test_${t} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
# NNUE suites take the net (and reference) explicitly; only register them if the shipped net is present
# (so a checkout without the .bin still configures, matching the Makefile's $(wildcard) degrade-to-no-op).
if(EXISTS "${EMBED_NET}")
  add_test(NAME bratko_kopec_nnue COMMAND test_bratko_kopec_nnue "${EMBED_NET}"
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME nnue COMMAND test_nnue "${EMBED_NET}" "${NNUE_REF}"
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME nnue_incremental COMMAND test_nnue_incremental "${PAWNSTAR_SHAPE_NET}"
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  # The principal variation Game publishes, which tools/gen_data.cpp harvests: every ply legal, replayable
  # without touching the game history, and evaluating from its FEN exactly as it does from the position.
  add_test(NAME principal_variation COMMAND test_principal_variation "${PAWNSTAR_SHAPE_NET}"
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  # The bench signature - every position's best move and node count and the 6,601,454 total, which the Go and Rust
  # ports pin too (test/bench_golden_test.cpp). RunBench forces one thread itself; the environment says so as well.
  add_test(NAME bench_golden COMMAND test_bench_golden "${EMBED_NET}"
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(bratko_kopec_nnue bench_golden principal_variation PROPERTIES ENVIRONMENT
                       "PAWNSTAR_THREADS=1")
  # The UCI command table and Game driven in process (test/uci_commands_test.cpp): every setoption, position form
  # and go parameter, the diagnostics and the root search's short cuts. Run from the repository root for the book.
  add_test(NAME uci_commands COMMAND test_uci_commands "${PAWNSTAR_SHAPE_NET}"
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
# The UCI integration test is a bash script — register it only where a shell is available (skipped on a
# bash-less Windows host). HINTS point at Git for Windows' bash and are searched before the system PATH, so
# on Windows we prefer Git Bash (which understands C:/... paths) over the WSL `bash.exe` stub in System32
# (which cannot read a Windows-style script path); on Linux these hints don't exist and it falls through to
# the system /bin/bash.
# HINTS are searched before the system PATH. On Windows they point at Git Bash (which understands C:/…
# paths) over the WSL bash.exe stub. On macOS they point at Homebrew's bash — the system /bin/bash is 3.2
# and lacks `coproc` (used by uci_test.sh), so a `brew install bash` is preferred over it. Override with
# -DBASH_EXE=… (the Makefile wrapper maps `make check BASH=…` to it). On Linux none of these exist and it
# falls through to /bin/bash.
find_program(BASH_EXE NAMES bash
  HINTS
    "$ENV{ProgramFiles}/Git/bin"
    "$ENV{ProgramFiles}/Git/usr/bin"
    "$ENV{ProgramW6432}/Git/bin"
    "$ENV{ProgramW6432}/Git/usr/bin"
    "$ENV{ProgramFiles\(x86\)}/Git/bin"
    /opt/homebrew/bin
    /usr/local/bin)
if(BASH_EXE AND NOT CMAKE_CROSSCOMPILING)
  if(PAWNSTAR_SHIPPED_NET_IS_THIS_SHAPE)
    add_test(NAME uci COMMAND ${BASH_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/test/uci_test.sh $<TARGET_FILE:pawnstar>
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  else()
    # This suite is the one that cannot be carried across to another shape, and it is worth saying why
    # rather than leaving it unregistered, which would be an invisible skip. Its first half is pure
    # protocol and does run on the synthetic net — that was tried and passed — but its second half is
    # about the SHIPPED net specifically: a `bench` run twice for determinism, which at this shape has no
    # trained net to bench and over random weights does not finish a depth-13 search in any useful time
    # (measured: a depth-12 search from the start position had not finished in two minutes); and the
    # startup cases, which assert where the engine finds its net and book and that it falls back to the
    # copies embedded in the binary — all of which are statements about the shipped v12 net. Pointing
    # PAWNSTAR_EVALFILE at a synthetic net would make those halves assert the opposite of what they mean.
    add_test(NAME uci COMMAND ${CMAKE_COMMAND} -E echo
             "[PASS] uci: skipped - the shipped net is not this build's NNUE shape, and this suite's second half is about that net (bench determinism, where the net is found, the embedded fallback). The shape's own gates are nnue_incremental and tools/new-shape-signature.sh.")
  endif()
endif()

set(PAWNSTAR_TEST_TARGETS ${PAWNSTAR_TESTS})
list(TRANSFORM PAWNSTAR_TEST_TARGETS PREPEND test_)
set(PAWNSTAR_TOOLS gen_data gen_openings lichess_extract lc0_extract stamp_net filter_book nnue_quant_study
    dump_magics)
set(PAWNSTAR_EXECUTABLES pawnstar ${PAWNSTAR_TOOLS} ${PAWNSTAR_TEST_TARGETS})

# --- The sanitizer runtime beside the executables. Clang targeting the MSVC ABI links AddressSanitizer dynamically:
#     every sanitized executable loads clang_rt.asan_dynamic-x86_64.dll at start and dies without it. Windows looks
#     for a DLL first in the executable's own directory, so one copy there lets ctest, a debugger and a hand-run
#     binary start with no PATH change. One rule makes it (so parallel links never race on the file) and every
#     executable depends on it.
if(PAWNSTAR_SANITIZE AND WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
                  OUTPUT_VARIABLE _clang_resource_dir OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(_asan_dll_name clang_rt.asan_dynamic-x86_64.dll)
  set(_asan_dll "${_clang_resource_dir}/lib/windows/${_asan_dll_name}")
  if(NOT EXISTS "${_asan_dll}")
    message(FATAL_ERROR "PAWNSTAR_SANITIZE: ${_asan_dll_name} is not in ${_clang_resource_dir}/lib/windows")
  endif()
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${_asan_dll_name}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_asan_dll}" "${CMAKE_CURRENT_BINARY_DIR}/${_asan_dll_name}"
    DEPENDS "${_asan_dll}"
    COMMENT "Copying ${_asan_dll_name} beside the executables"
    VERBATIM)
  add_custom_target(asan_runtime DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${_asan_dll_name}")
  foreach(_exe IN LISTS PAWNSTAR_EXECUTABLES)
    add_dependencies(${_exe} asan_runtime)
  endforeach()
endif()

# --- Code coverage. `cmake --build build --target coverage` (`make coverage`, `tools\build.ps1 coverage`) runs every
#     ctest on the instrumented build (PAWNSTAR_COVERAGE), each process writing <build>/prof/%p-%m.profraw, merges
#     them with llvm-profdata and has llvm-cov report the engine's (src/) line, function and region coverage over
#     the engine and every test binary, with an lcov export and an HTML report under <build>/coverage, and fails when
#     src/'s line coverage is under PAWNSTAR_COVERAGE_MIN_LINES (above). src/ is the engine and its UCI front end; the
#     developer tools under tools/ (no release ships them, no ctest runs them) and the tests are not measured. The
#     LLVM tools are looked for beside the compiler first (Visual Studio's clang, /usr/lib/llvm-18/bin), so they
#     match it.
if(PAWNSTAR_COVERAGE)
  get_filename_component(_cxx_real "${CMAKE_CXX_COMPILER}" REALPATH)
  get_filename_component(_cxx_dir "${_cxx_real}" DIRECTORY)
  string(REGEX MATCH "^[0-9]+" _cxx_major "${CMAKE_CXX_COMPILER_VERSION}")
  find_program(LLVM_PROFDATA_EXE NAMES llvm-profdata llvm-profdata-${_cxx_major} HINTS "${_cxx_dir}" NO_CACHE)
  find_program(LLVM_COV_EXE NAMES llvm-cov llvm-cov-${_cxx_major} HINTS "${_cxx_dir}" NO_CACHE)
  if(NOT LLVM_PROFDATA_EXE OR NOT LLVM_COV_EXE)
    message(FATAL_ERROR "PAWNSTAR_COVERAGE needs llvm-profdata and llvm-cov (looked beside ${_cxx_real} first)")
  endif()
  set(_coverage_objects "")
  foreach(_exe IN ITEMS pawnstar ${PAWNSTAR_TEST_TARGETS}) # the tools run under no ctest
    list(APPEND _coverage_objects "$<TARGET_FILE:${_exe}>")
  endforeach()
  list(JOIN _coverage_objects "|" _coverage_objects)
  add_custom_target(coverage
    COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DCTEST=${CMAKE_CTEST_COMMAND}
      -DLLVM_PROFDATA=${LLVM_PROFDATA_EXE}
      -DLLVM_COV=${LLVM_COV_EXE}
      "-DOBJECTS=${_coverage_objects}"
      -DMIN_LINE_PERCENT=${PAWNSTAR_COVERAGE_MIN_LINES}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_coverage.cmake
    DEPENDS ${PAWNSTAR_EXECUTABLES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running the suites under coverage instrumentation"
    VERBATIM)
endif()

# --- API documentation. `cmake --build build --target doc` (or `make doc` via the wrapper) generates the
#     Doxygen HTML into doc/html with PROJECT_NUMBER stamped from the current version (mirrors the Makefile's
#     piped PROJECT_NUMBER, so the docs carry major.minor.build without hardcoding it in the Doxyfile). Not
#     built by ALL.
add_custom_target(doc
  COMMAND ${CMAKE_COMMAND}
    -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
    -DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -DVERSION=${PAWNSTAR_VERSION}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_doxygen.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating Doxygen HTML (Pawnstar ${PAWNSTAR_VERSION})"
  VERBATIM)
