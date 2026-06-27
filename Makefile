PROGRAM             = pawnstar
CXX                 = clang++
CPPFLAGS            = -I src
CXXFLAGS            = $(CPPFLAGS) -Wall -Wextra -Wpedantic -std=c++23 -mbmi2 -mavx2
BUILD_DIR           = build
DOC_DIR             = doc/html
PROGRAM_EXE         = $(BUILD_DIR)/$(PROGRAM)
TEST_DIR            = test
TOOLS_DIR           = tools
TOOL_STAMP_EXE      = $(BUILD_DIR)/stamp_net
TOOL_FILTERBOOK_EXE = $(BUILD_DIR)/filter_book
TOOL_QUANT_EXE      = $(BUILD_DIR)/nnue_quant_study

# Header-only engine: every engine class/function lives in src/*.h as `inline` definitions, so there are
# no per-class engine .cpp files or object files to link. The only compiled engine translation unit is
# main.cpp (the UCI entry point); each test and tool compiles as a single TU that pulls the engine in
# through its headers. Full cross-"file" inlining now happens within each TU at -O3, with no LTO needed.
OBJECTS             = $(BUILD_DIR)/main.o
DEPS                = $(OBJECTS:.o=.d)
# The shipped NNUE net embedded into the engine binary (.incbin) as a runtime-load fallback. Linked into
# the engine executable only — the test binaries load nets explicitly and don't need it.
EMBED_NET           = nnue/pawnstar-v12.bin
EMBED_OBJECT        = $(BUILD_DIR)/embedded_net.o
# The shipped opening book embedded into the engine binary (.incbin) as a runtime-load fallback, mirroring
# the embedded net. Linked into the engine executable only.
EMBED_BOOK          = pawnstar.book
EMBED_BOOK_OBJECT   = $(BUILD_DIR)/embedded_book.o

TEST_PERFT_EXE      = $(BUILD_DIR)/test_perft
TEST_SEE_EXE        = $(BUILD_DIR)/test_see
TEST_BK_NNUE_EXE    = $(BUILD_DIR)/test_bratko_kopec_nnue
TEST_NNUE_EXE       = $(BUILD_DIR)/test_nnue
TEST_NNUE_INC_EXE   = $(BUILD_DIR)/test_nnue_incremental
TEST_BOOK_EXE       = $(BUILD_DIR)/test_opening_book
TEST_CLOCK_EXE      = $(BUILD_DIR)/test_chess_clock
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
# than the exact int16 dot, +31.8 Elo at 8+0.08, ~26 cp deviation). It runs on the engine's baseline AVX2
# target via pmaddubsw; VNNI=1 enables AVX-VNNI (256-bit vpdpbusd) for an exact, faster dot on capable CPUs
# (bit-identical result). The exact int16 forward pass remains as EvaluateExact (the test_nnue reference).
ifeq ($(VNNI), 1)
	CXXFLAGS += -mavxvnni
endif

.PHONY: all tests check prep clean doc tools

all: prep $(PROGRAM_EXE)

tools: prep $(TOOL_STAMP_EXE) $(TOOL_FILTERBOOK_EXE) $(TOOL_QUANT_EXE)

tests: prep $(TEST_PERFT_EXE) $(TEST_SEE_EXE) $(TEST_BK_NNUE_EXE) $(TEST_NNUE_EXE) $(TEST_NNUE_INC_EXE) $(TEST_BOOK_EXE) $(TEST_CLOCK_EXE)

# Run every suite in one shell, chained with && so the first failure aborts (and the summary below is
# skipped). On full success report the wall-clock time spent running the tests (not the build). Depends on
# `tools` so a broken helper (e.g. filter_book) fails the check, and on `all` so the engine binary exists
# for the UCI protocol test (test/uci_test.sh, which drives build/pawnstar over stdin/stdout).
check: tests tools all
	@start=$$(date +%s%3N); \
	$(TEST_PERFT_EXE) && \
	$(TEST_BK_NNUE_EXE) $(NNUE_NET) && \
	$(TEST_NNUE_EXE) $(NNUE_NET) $(NNUE_REF) && \
	$(TEST_NNUE_INC_EXE) $(NNUE_NET) && \
	$(TEST_SEE_EXE) && \
	$(TEST_BOOK_EXE) && \
	$(TEST_CLOCK_EXE) && \
	bash $(TEST_DIR)/uci_test.sh $(PROGRAM_EXE) && \
	echo "make check: All tests passed in $$(( $$(date +%s%3N) - start )) ms"

prep:
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(PROGRAM_EXE) $(TEST_PERFT_EXE) $(TEST_SEE_EXE) $(TEST_BK_NNUE_EXE) $(TEST_NNUE_EXE) $(TEST_NNUE_INC_EXE) $(TEST_BOOK_EXE) $(TEST_CLOCK_EXE) \
	      $(TOOL_STAMP_EXE) $(BUILD_DIR)/stamp_net.o $(BUILD_DIR)/stamp_net.d \
	      $(TOOL_FILTERBOOK_EXE) $(BUILD_DIR)/filter_book.o $(BUILD_DIR)/filter_book.d \
	      $(TOOL_QUANT_EXE) $(BUILD_DIR)/nnue_quant_study.o $(BUILD_DIR)/nnue_quant_study.d \
	      $(OBJECTS) $(EMBED_OBJECT) $(EMBED_BOOK_OBJECT) $(TEST_OBJECTS) $(DEPS) $(TEST_DEPS)

# Generate API documentation with doxygen (output configured by the Doxyfile).
doc:
	doxygen Doxyfile

# Compile an engine source object.
$(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) -c $(CXXFLAGS) -MMD -MP -o $@ $<

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

# Link the main executable (with the embedded-net and embedded-book objects).
$(PROGRAM_EXE): $(OBJECTS) $(EMBED_OBJECT) $(EMBED_BOOK_OBJECT)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(PROGRAM_EXE) $(OBJECTS) $(EMBED_OBJECT) $(EMBED_BOOK_OBJECT)

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

-include $(DEPS)
-include $(TEST_DEPS)
