PROGRAM_BASE        = pawnstar
CXX                 = clang++
# Shell used to run the UCI integration test. It needs bash >= 4 (the script uses `coproc`); macOS still
# ships bash 3.2 as /bin/bash, so override on macOS, e.g. `make check BASH=$(brew --prefix)/bin/bash`.
BASH               ?= bash
CPPFLAGS            = -I src

# Per-architecture ISA flags. x86-64 needs -mavx2 (NNUE SIMD) and -mbmi2 (BMI1 tzcnt/blsr for
# std::countr_zero); those options are hard errors on ARM. On arm64 (Apple Silicon / aarch64) we instead
# target a baseline that includes the AdvSIMD dot-product extension (FEAT_DotProd) the NEON NNUE kernels
# will use, and the engine's SIMD falls back to its scalar path until the NEON kernels land. Detected from
# uname so a single Makefile serves x86-64 Linux/Windows and arm64 macOS/Linux.
UNAME_S             := $(shell uname -s 2>/dev/null)
UNAME_M             := $(shell uname -m 2>/dev/null)
ifneq (,$(filter arm64 aarch64,$(UNAME_M)))
  ifeq ($(UNAME_S),Darwin)
    ARCH_FLAGS      = -mcpu=apple-m1
  else
    ARCH_FLAGS      = -march=armv8.2-a+dotprod
  endif
else
  ARCH_FLAGS        = -mbmi2 -mavx2
endif

CXXFLAGS            = $(CPPFLAGS) -Wall -Wextra -Wpedantic -std=c++23 $(ARCH_FLAGS)
BUILD_DIR           = build
DOC_DIR             = doc/html

# Windows fallback (e.g. Git Bash + the VS-bundled clang). The Makefile is the primary Linux build; these two
# lines just let it serve as a clean Windows build too: emit .exe-suffixed binaries (so they run from cmd,
# Explorer and chess GUIs, not only from a Unix-style shell) and define _CRT_SECURE_NO_WARNINGS to silence
# MSVC's UCRT "insecure" deprecation warnings on getenv/setbuf (correct, portable C++) — mirroring what
# CMakeLists.txt does for WIN32. OS=Windows_NT is set by Windows and inherited by Git Bash/make. No effect on
# Linux (EXE_SUFFIX stays empty, the define is not added).
ifeq ($(OS),Windows_NT)
EXE_SUFFIX          = .exe
CPPFLAGS           += -D_CRT_SECURE_NO_WARNINGS
endif

# The target executable carries the engine version: pawnstar_<major>_<minor>_<build> (+ .exe on Windows).
# Major/minor are read from src/version.h (the single source of truth, same as the C++ build); the build
# number is the git commit count (BUILD_NUMBER, defined below). PROGRAM is recursively expanded, so it picks
# up VERSION_MAJOR/MINOR/BUILD_NUMBER defined further down.
VERSION_MAJOR      := $(shell sed -n 's/.*kVersionMajor = \([0-9][0-9]*\);.*/\1/p' src/version.h)
VERSION_MINOR      := $(shell sed -n 's/.*kVersionMinor = \([0-9][0-9]*\);.*/\1/p' src/version.h)
PROGRAM             = $(PROGRAM_BASE)_$(VERSION_MAJOR)_$(VERSION_MINOR)_$(BUILD_NUMBER)
PROGRAM_EXE         = $(BUILD_DIR)/$(PROGRAM)$(EXE_SUFFIX)
# Stable convenience name (build/pawnstar[.exe]) pointing at the version-named binary, so tooling, scripts
# and docs can use a fixed path. A symlink on Linux/macOS; a plain copy on Windows (symlinks need privilege
# there). The symlink target is the bare filename, so it resolves relative to build/.
PROGRAM_ALIAS       = $(BUILD_DIR)/$(PROGRAM_BASE)$(EXE_SUFFIX)
ifeq ($(OS),Windows_NT)
MAKE_ALIAS          = cp -f $(PROGRAM_EXE) $(PROGRAM_ALIAS)
else
MAKE_ALIAS          = ln -sf $(PROGRAM)$(EXE_SUFFIX) $(PROGRAM_ALIAS)
endif
TEST_DIR            = test
TOOLS_DIR           = tools
TOOL_STAMP_EXE      = $(BUILD_DIR)/stamp_net$(EXE_SUFFIX)
TOOL_FILTERBOOK_EXE = $(BUILD_DIR)/filter_book$(EXE_SUFFIX)
TOOL_QUANT_EXE      = $(BUILD_DIR)/nnue_quant_study$(EXE_SUFFIX)
TOOL_DUMPMAGICS_EXE = $(BUILD_DIR)/dump_magics$(EXE_SUFFIX)

# Header-only engine: every engine class/function lives in src/*.h as `inline` definitions, so there are
# no per-class engine .cpp files or object files to link. The only compiled engine translation unit is
# main.cpp (the UCI entry point); each test and tool compiles as a single TU that pulls the engine in
# through its headers. Full cross-"file" inlining now happens within each TU at -O3, with no LTO needed.
OBJECTS             = $(BUILD_DIR)/main.o
DEPS                = $(OBJECTS:.o=.d)
# Globally-reproducible build number = the git commit count (`git rev-list --count HEAD`): identical for
# every clone at a given commit, monotonic along history, no state file. Falls back to 0 outside a git
# checkout (e.g. a source tarball). Stamped into the engine via a generated header (see VERSION_HEADER) so
# the UCI "id name" carries it. NOTE: a shallow clone (CI default) undercounts — fetch full history.
BUILD_NUMBER       := $(shell git rev-list --count HEAD 2>/dev/null || echo 0)
VERSION_HEADER      = $(BUILD_DIR)/version_build.h
# The shipped NNUE net embedded into the engine binary (.incbin) as a runtime-load fallback. Linked into
# the engine executable only — the test binaries load nets explicitly and don't need it.
EMBED_NET           = nnue/pawnstar-v12.bin
EMBED_OBJECT        = $(BUILD_DIR)/embedded_net.o
# The shipped opening book embedded into the engine binary (.incbin) as a runtime-load fallback, mirroring
# the embedded net. Linked into the engine executable only.
EMBED_BOOK          = pawnstar.book
EMBED_BOOK_OBJECT   = $(BUILD_DIR)/embedded_book.o

TEST_PERFT_EXE      = $(BUILD_DIR)/test_perft$(EXE_SUFFIX)
TEST_SEE_EXE        = $(BUILD_DIR)/test_see$(EXE_SUFFIX)
TEST_BK_NNUE_EXE    = $(BUILD_DIR)/test_bratko_kopec_nnue$(EXE_SUFFIX)
TEST_NNUE_EXE       = $(BUILD_DIR)/test_nnue$(EXE_SUFFIX)
TEST_NNUE_INC_EXE   = $(BUILD_DIR)/test_nnue_incremental$(EXE_SUFFIX)
TEST_BOOK_EXE       = $(BUILD_DIR)/test_opening_book$(EXE_SUFFIX)
TEST_CLOCK_EXE      = $(BUILD_DIR)/test_chess_clock$(EXE_SUFFIX)
# Shipped net + checked-in trainer reference, used by `make check` to exercise NNUE inference and the
# incremental accumulator. Resolved with wildcard so the checks degrade to a no-op (still green) if absent.
NNUE_NET            = $(wildcard nnue/pawnstar-v12.bin)
NNUE_REF            = $(wildcard test/nnue_reference.txt)
TEST_OBJECTS        = $(BUILD_DIR)/perft_test.o \
                      $(BUILD_DIR)/see_test.o \
                      $(BUILD_DIR)/bratko_kopec_nnue_test.o \
                      $(BUILD_DIR)/nnue_test.o \
                      $(BUILD_DIR)/nnue_incremental_test.o \
                      $(BUILD_DIR)/opening_book_test.o \
                      $(BUILD_DIR)/chess_clock_test.o
TEST_DEPS           = $(TEST_OBJECTS:.o=.d)

# Diagnostic counters (DEBUGX) are compiled in by default; build with RELEASE=1 to omit them.
ifneq ($(RELEASE), 1)
	CPPFLAGS += -D DEBUGX=1
endif

ifeq ($(DEBUG), 1)
	CXXFLAGS += -g -Og -D DEBUG -fsanitize=undefined -fsanitize=address
else
	CXXFLAGS += -g -O3 -D NDEBUG
endif

# Treat warnings as errors with `make WERROR=1` (e.g. in CI), so a new warning fails the build. Opt-in,
# not default, since -Werror can break otherwise-fine builds on a newer/different compiler version.
ifeq ($(WERROR), 1)
	CXXFLAGS += -Werror
endif

# The NNUE output layer is int8 by default (uint8 SCReLU activations x int8 output weights; ~1.86x faster
# than the exact int16 dot, +31.8 Elo at 8+0.08, ~26 cp deviation). On AVX-VNNI CPUs the output dot uses the
# 256-bit vpdpbusd instruction (bit-identical, ~+1.3% nps), chosen at RUNTIME via cpuid with the vpdpbusd
# kernel compiled behind a function-level target attribute — so the engine stays a single baseline -mavx2
# binary that also runs on AVX2-only CPUs (no -mavxvnni build flag needed; see Network::Evaluate /
# OutputDotInt8Vnni in src/nnue.h). The exact int16 forward pass remains as EvaluateExact (test_nnue ref).

.PHONY: all tests check prep clean doc tools FORCE

all: prep $(PROGRAM_EXE)

# Always-out-of-date target: a prerequisite of $(VERSION_HEADER) so the commit count is re-checked each build.
FORCE:

tools: prep $(TOOL_STAMP_EXE) $(TOOL_FILTERBOOK_EXE) $(TOOL_QUANT_EXE) $(TOOL_DUMPMAGICS_EXE)

tests: prep $(TEST_PERFT_EXE) $(TEST_SEE_EXE) $(TEST_BK_NNUE_EXE) $(TEST_NNUE_EXE) $(TEST_NNUE_INC_EXE) $(TEST_BOOK_EXE) $(TEST_CLOCK_EXE)

# Run every suite in one shell, chained with && so the first failure aborts (and the summary below is
# skipped). On full success report the wall-clock time spent running the tests (not the build). Depends on
# `tools` so a broken helper (e.g. filter_book) fails the check, and on `all` so the engine binary exists
# for the UCI protocol test (test/uci_test.sh, which drives build/pawnstar over stdin/stdout).
check: tests tools all
	@now_ms() { t=$$(date +%s%3N); case "$$t" in *[!0-9]*) echo $$(( $$(date +%s) * 1000 ));; *) echo "$$t";; esac; }; \
	start=$$(now_ms); \
	$(TEST_PERFT_EXE) && \
	$(TEST_BK_NNUE_EXE) $(NNUE_NET) && \
	$(TEST_NNUE_EXE) $(NNUE_NET) $(NNUE_REF) && \
	$(TEST_NNUE_INC_EXE) $(NNUE_NET) && \
	$(TEST_SEE_EXE) && \
	$(TEST_BOOK_EXE) && \
	$(TEST_CLOCK_EXE) && \
	$(BASH) $(TEST_DIR)/uci_test.sh $(PROGRAM_EXE) && \
	echo "make check: All tests passed in $$(( $$(now_ms) - start )) ms"

prep:
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(BUILD_DIR)/$(PROGRAM_BASE)* $(TEST_PERFT_EXE) $(TEST_SEE_EXE) $(TEST_BK_NNUE_EXE) $(TEST_NNUE_EXE) $(TEST_NNUE_INC_EXE) $(TEST_BOOK_EXE) $(TEST_CLOCK_EXE) \
	      $(TOOL_STAMP_EXE) $(BUILD_DIR)/stamp_net.o $(BUILD_DIR)/stamp_net.d \
	      $(TOOL_FILTERBOOK_EXE) $(BUILD_DIR)/filter_book.o $(BUILD_DIR)/filter_book.d \
	      $(TOOL_QUANT_EXE) $(BUILD_DIR)/nnue_quant_study.o $(BUILD_DIR)/nnue_quant_study.d \
	      $(TOOL_DUMPMAGICS_EXE) $(BUILD_DIR)/dump_magics.o $(BUILD_DIR)/dump_magics.d \
	      $(OBJECTS) $(EMBED_OBJECT) $(EMBED_BOOK_OBJECT) $(TEST_OBJECTS) $(DEPS) $(TEST_DEPS)

# Generate API documentation with doxygen (output configured by the Doxyfile). PROJECT_NUMBER is injected
# here from the current version (appended to the piped config — doxygen takes the last assignment) so the
# docs show major.minor.build without a stale hardcode in the Doxyfile.
doc:
	( cat Doxyfile; printf 'PROJECT_NUMBER = %s.%s.%s\n' '$(VERSION_MAJOR)' '$(VERSION_MINOR)' '$(BUILD_NUMBER)' ) | doxygen -

# Compile an engine source object.
$(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) -c $(CXXFLAGS) -MMD -MP -o $@ $<

# Generate the build-number header (#define PAWNSTAR_BUILD_NUMBER <commit count>). The FORCE prerequisite
# re-runs this every build so the committed count is always current, but the recipe rewrites the file only
# when the number actually changes (cmp), so main.o — which force-includes it — recompiles only on a real
# change, not on every `make`.
$(VERSION_HEADER): FORCE | prep
	@printf '#pragma once\n#define PAWNSTAR_BUILD_NUMBER %s\n' '$(BUILD_NUMBER)' > $@.tmp; \
	if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

# Compile the engine entry point, force-including the generated build-number header (off every other TU, so
# they don't all rebuild). This explicit rule takes precedence over the pattern rule above for main.o.
$(BUILD_DIR)/main.o: src/main.cpp $(VERSION_HEADER) | prep
	$(CXX) -c $(CXXFLAGS) -include $(VERSION_HEADER) -MMD -MP -o $@ $<

# Compile a test source object.
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) -MMD -MP -o $@ $<

# Compile a tools source object.
$(BUILD_DIR)/%.o: $(TOOLS_DIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) -MMD -MP -o $@ $<

# Assemble the embedded net object (.incbin of the shipped net). Rebuilds if the net changes. Plain
# compile — the C++ flags don't apply to the .S stub. The net path is passed from EMBED_NET (single source
# of truth) into the capital-.S via the preprocessor, so the embedded copy can't drift from the Makefile
# net. The .incbin path is relative to the make cwd (root).
$(EMBED_OBJECT): src/embedded_net.S $(EMBED_NET) | prep
	$(CXX) -c -DEMBED_NET_PATH='"$(EMBED_NET)"' -o $@ src/embedded_net.S

# Assemble the embedded opening-book object (.incbin of the shipped book). Rebuilds if the book changes.
# The book path is passed from EMBED_BOOK (single source of truth) into the capital-.S via the preprocessor,
# so the embedded copy can't drift from the Makefile book. The .incbin path is relative to the make cwd (root).
$(EMBED_BOOK_OBJECT): src/embedded_book.S $(EMBED_BOOK) | prep
	$(CXX) -c -DEMBED_BOOK_PATH='"$(EMBED_BOOK)"' -o $@ src/embedded_book.S

# Link the main executable (with the embedded-net and embedded-book objects), then refresh the stable
# build/pawnstar alias pointing at it.
$(PROGRAM_EXE): $(OBJECTS) $(EMBED_OBJECT) $(EMBED_BOOK_OBJECT)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(PROGRAM_EXE) $(OBJECTS) $(EMBED_OBJECT) $(EMBED_BOOK_OBJECT)
	$(MAKE_ALIAS)

# Header-only engine: each test executable is a single TU (its *_test.o) that pulls the engine in through
# headers, so it links on its own — there are no engine objects to link against any more (and no main.o).
$(TEST_PERFT_EXE): $(BUILD_DIR)/perft_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_SEE_EXE): $(BUILD_DIR)/see_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_BK_NNUE_EXE): $(BUILD_DIR)/bratko_kopec_nnue_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_NNUE_EXE): $(BUILD_DIR)/nnue_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_NNUE_INC_EXE): $(BUILD_DIR)/nnue_incremental_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_BOOK_EXE): $(BUILD_DIR)/opening_book_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_CLOCK_EXE): $(BUILD_DIR)/chess_clock_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Each tool is likewise a single TU pulling the engine in through headers; it links on its own.
$(TOOL_STAMP_EXE): $(BUILD_DIR)/stamp_net.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TOOL_FILTERBOOK_EXE): $(BUILD_DIR)/filter_book.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TOOL_QUANT_EXE): $(BUILD_DIR)/nnue_quant_study.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TOOL_DUMPMAGICS_EXE): $(BUILD_DIR)/dump_magics.o
	$(CXX) $(CXXFLAGS) -o $@ $^

-include $(DEPS)
-include $(TEST_DEPS)
