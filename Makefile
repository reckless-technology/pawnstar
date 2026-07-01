# Thin wrapper around the CMake build, which is Pawnstar's single build system of record (see CMakeLists.txt).
# It preserves the historical `make` UX — `make`, `make check`, `make tools`, `make tests`, `make doc`,
# `make clean` — by mapping to cmake + ctest, so muscle memory and existing tooling keep working. On Windows,
# invoke CMake directly (this wrapper is for Unix-y shells).
#
# Requires cmake (>= 3.20) and a generator: Ninja is used if available, else Unix Makefiles.
#
# Knobs (mirror the former hand-written Makefile):
#   RELEASE=1   omit the DEBUGX diagnostic counters               -> -DPAWNSTAR_DEBUGX=OFF
#   DEBUG=1     Debug build with AddressSanitizer + UBSan         -> -DCMAKE_BUILD_TYPE=Debug -DPAWNSTAR_SANITIZE=ON
#   WERROR=1    warnings-as-errors (CI)                           -> -DWERROR=ON
#   CXX=<cc>    C++ compiler (default clang++, which the engine requires). Changing it for an existing build
#               tree needs `make clean` first, since CMake caches the compiler.
#   BASH=<sh>   bash used for the UCI ctest. macOS /bin/bash is 3.2 (no `coproc`); point at a >= 4 bash,
#               e.g. `make check BASH=$(brew --prefix)/bin/bash`.
#
# The version-named binary (build/pawnstar_<major>_<minor>_<build>) and the stable build/pawnstar alias are
# produced by CMake; the build number (git commit count) is passed through so the filename tracks each commit.

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

# Globally-reproducible build number = git commit count; passed to CMake so the versioned filename tracks
# each commit without a manual reconfigure (a changed cache value reconfigures automatically).
BUILD_NUMBER := $(shell git rev-list --count HEAD 2>/dev/null || echo 0)

CMAKE_ARGS := -DPAWNSTAR_VERSION_BUILD=$(BUILD_NUMBER)

ifeq ($(DEBUG),1)
  CMAKE_ARGS += -DCMAKE_BUILD_TYPE=Debug -DPAWNSTAR_SANITIZE=ON
else
  CMAKE_ARGS += -DCMAKE_BUILD_TYPE=Release
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

TOOL_TARGETS := stamp_net filter_book nnue_quant_study dump_magics
TEST_TARGETS := test_perft test_see test_bratko_kopec_nnue test_nnue test_nnue_incremental \
                test_opening_book test_chess_clock

.PHONY: all tools tests check doc clean configure

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

doc: configure
	$(CMAKE) --build $(BUILD_DIR) --target doc

clean:
	rm -rf $(BUILD_DIR)

# (Re)configure the CMake build tree. A fast no-op when nothing changed; reconfigures when a knob or the
# build number changes.
configure:
	@$(CMAKE) -S . -B $(BUILD_DIR) -G "$(GENERATOR)" $(CMAKE_ARGS)
