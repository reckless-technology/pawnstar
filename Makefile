# Thin wrapper around the CMake build, which is Pawnstar's single build system of record (see CMakeLists.txt).
# It preserves the historical `make` UX — `make`, `make check`, `make tools`, `make tests`, `make doc`,
# `make clean`, and `make coverage` — by mapping to cmake + ctest, so muscle memory and existing tooling keep working. On Windows,
# tools/build.ps1 is the same wrapper for PowerShell, target for target and knob for knob.
#
# Requires cmake (>= 3.20) and a generator: Ninja is used if available, else Unix Makefiles.
#
# Knobs (mirror the former hand-written Makefile):
#   RELEASE=1   omit the DEBUGX diagnostic counters               -> -DPAWNSTAR_DEBUGX=OFF
#   DEBUG=1     Debug build with AddressSanitizer + UBSan         -> -DCMAKE_BUILD_TYPE=Debug -DPAWNSTAR_SANITIZE=ON
#   WERROR=1    warnings-as-errors (CI)                           -> -DWERROR=ON
#   COVERAGE=1  instrument for source-based coverage (clang)      -> -DPAWNSTAR_COVERAGE=ON (`make coverage` sets it)
#   COVERAGE_MIN_LINES=<n>  the coverage gate, src/'s minimum line % -> -DPAWNSTAR_COVERAGE_MIN_LINES=<n> (default 90,
#               set in CMakeLists.txt; 0 reports without gating)
#   CXX=<cc>    C++ compiler (default clang++, which the engine requires). Changing it for an existing build
#               tree needs `make clean` first, since CMake caches the compiler.
#   BASH=<sh>   bash used for the UCI ctest. macOS /bin/bash is 3.2 (no `coproc`); point at a >= 4 bash,
#               e.g. `make check BASH=$(brew --prefix)/bin/bash`.
#
# The version-named binary (build/pawnstar_<major>_<minor>_<build>) and the stable build/pawnstar alias are
# produced by CMake, which derives the version from the git tags (cmake/version.cmake) and configures again by itself
# when HEAD moves, so the filename tracks each commit.

BUILD_DIR ?= build
CMAKE     ?= cmake
CTEST     ?= ctest
# ctest verbosity for `make check`. The default -V streams each suite's own output (the [PASS]/[FAIL] lines,
# perft/SEE/bench numbers, ...) like the former direct-executable `make check`. Override for a terse summary,
# e.g. `make check CTEST_ARGS=--output-on-failure` (prints a suite's output only when it fails).
CTEST_ARGS ?= -V

# Prefer Ninja; fall back to Unix Makefiles if it is not installed.
ifeq ($(shell command -v ninja 2>/dev/null),)
  GENERATOR := Unix Makefiles
else
  GENERATOR := Ninja
endif

ifeq ($(DEBUG),1)
  CMAKE_ARGS := -DCMAKE_BUILD_TYPE=Debug -DPAWNSTAR_SANITIZE=ON
else
  CMAKE_ARGS := -DCMAKE_BUILD_TYPE=Release
endif

ifeq ($(RELEASE),1)
  CMAKE_ARGS += -DPAWNSTAR_DEBUGX=OFF
else
  CMAKE_ARGS += -DPAWNSTAR_DEBUGX=ON
endif

ifeq ($(WERROR),1)
  CMAKE_ARGS += -DWERROR=ON
else
  CMAKE_ARGS += -DWERROR=OFF
endif

ifeq ($(COVERAGE),1)
  CMAKE_ARGS += -DPAWNSTAR_COVERAGE=ON
else
  CMAKE_ARGS += -DPAWNSTAR_COVERAGE=OFF
endif

# The NNUE network shape. The defaults are the shared architecture, KING_MIRRORING=1 with
# OUTPUT_BUCKETS=8, which is the shape the shipped net was trained for; KING_MIRRORING=0 with
# OUTPUT_BUCKETS=1 is the shape the retired v12 net needed. A shape change is an evaluation change, so give
# it a build tree of its own and expect the shipped net to be refused there:
#   make check BUILD_DIR=build-v12shape KING_MIRRORING=0 OUTPUT_BUCKETS=1
ifeq ($(KING_MIRRORING),0)
  CMAKE_ARGS += -DPAWNSTAR_NNUE_KING_MIRRORING=OFF
else
  CMAKE_ARGS += -DPAWNSTAR_NNUE_KING_MIRRORING=ON
endif

ifdef OUTPUT_BUCKETS
  CMAKE_ARGS += -DPAWNSTAR_NNUE_OUTPUT_BUCKETS=$(OUTPUT_BUCKETS)
endif

# Default the compiler to clang++ (the engine requires clang) unless the user set CXX explicitly (command
# line / environment); `origin` is "default" only for make's built-in value.
ifeq ($(origin CXX),default)
  CMAKE_ARGS += -DCMAKE_CXX_COMPILER=clang++
else
  CMAKE_ARGS += -DCMAKE_CXX_COMPILER=$(CXX)
endif

ifdef BASH
  CMAKE_ARGS += -DBASH_EXE=$(BASH)
endif

ifdef COVERAGE_MIN_LINES
  CMAKE_ARGS += -DPAWNSTAR_COVERAGE_MIN_LINES=$(COVERAGE_MIN_LINES)
endif

TOOL_TARGETS := gen_data gen_openings lichess_extract lc0_extract make_book stamp_net print_net_header filter_book \
                nnue_quant_study dump_magics
TEST_TARGETS := test_perft test_see test_bratko_kopec_nnue test_nnue test_nnue_incremental \
                test_eval_symmetry test_opening_book test_chess_clock test_bench_golden \
                test_uci_commands test_primitives test_principal_variation test_lichess_extract \
                test_lc0_extract test_net_header

.PHONY: all tools tests check coverage doc clean configure

# Default: build just the engine (matches the former `make`).
all: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel --target pawnstar

tools: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel --target $(TOOL_TARGETS)

tests: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel --target $(TEST_TARGETS)

# Build everything (engine, tools, tests) and run the full ctest suite — the former `make check`.
check: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel
	$(CTEST) --test-dir $(BUILD_DIR) $(CTEST_ARGS)

# Build everything instrumented for coverage, run every ctest and report src/'s coverage (llvm-cov; the lcov export
# and the HTML report land in $(BUILD_DIR)/coverage), failing under the gate (COVERAGE_MIN_LINES, default 90 %).
# Configures with COVERAGE=1 whatever the command line says, so
# a later plain `make` configures the tree back without it.
coverage:
	@$(CMAKE) -S . -B $(BUILD_DIR) -G "$(GENERATOR)" $(filter-out -DPAWNSTAR_COVERAGE=%,$(CMAKE_ARGS)) \
		-DPAWNSTAR_COVERAGE=ON
	$(CMAKE) --build $(BUILD_DIR) --parallel
	$(CMAKE) --build $(BUILD_DIR) --target coverage

doc: configure
	$(CMAKE) --build $(BUILD_DIR) --target doc

clean:
	rm -rf $(BUILD_DIR)

# (Re)configure the CMake build tree. A fast no-op when nothing changed; reconfigures when a knob changes or HEAD
# moves (the version).
configure:
	@$(CMAKE) -S . -B $(BUILD_DIR) -G "$(GENERATOR)" $(CMAKE_ARGS)
