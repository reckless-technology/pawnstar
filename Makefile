PROGRAM             = pawnstar
CXX                 = clang++
CPPFLAGS            = -I src
CXXFLAGS            = $(CPPFLAGS) -Wall -Wextra -Wpedantic -std=c++20 -mbmi2 -mavx2
BUILD_DIR           = build
DOC_DIR             = doc/html
PROGRAM_EXE         = $(BUILD_DIR)/$(PROGRAM)
TEST_DIR            = test
TOOLS_DIR           = tools
TOOL_STAMP_EXE      = $(BUILD_DIR)/stamp_net
TOOL_FILTERBOOK_EXE = $(BUILD_DIR)/filter_book
TOOL_QUANT_EXE      = $(BUILD_DIR)/nnue_quant_study

ENGINE_SOURCES      = \
	debug_hashtable.cpp \
	evaluation.cpp \
	game.cpp \
	input_handling.cpp \
	nnue.cpp \
	opening_book.cpp \
	position.cpp \
	search_state.cpp \
	static_exchange_evaluation.cpp \
	transposition_table.cpp \
	generated_data.cpp

SOURCES             = $(ENGINE_SOURCES) main.cpp
ENGINE_OBJECTS      = $(addprefix $(BUILD_DIR)/, $(ENGINE_SOURCES:.cpp=.o))
OBJECTS             = $(ENGINE_OBJECTS) $(BUILD_DIR)/main.o
DEPS                = $(OBJECTS:.o=.d)
# The shipped NNUE net embedded into the engine binary (.incbin) as a runtime-load fallback. Linked into
# the engine executable only — the test binaries load nets explicitly and don't need it.
EMBED_NET           = nnue/pawnstar-v9.bin
EMBED_OBJECT        = $(BUILD_DIR)/embedded_net.o

TEST_PERFT_EXE      = $(BUILD_DIR)/test_perft
TEST_SEE_EXE        = $(BUILD_DIR)/test_see
TEST_BK_NNUE_EXE    = $(BUILD_DIR)/test_bratko_kopec_nnue
TEST_NNUE_EXE       = $(BUILD_DIR)/test_nnue
TEST_NNUE_INC_EXE   = $(BUILD_DIR)/test_nnue_incremental
TEST_BOOK_EXE       = $(BUILD_DIR)/test_opening_book
# Shipped net + checked-in trainer reference, used by `make check` to exercise NNUE inference and the
# incremental accumulator. Resolved with wildcard so the checks degrade to a no-op (still green) if absent.
NNUE_NET            = $(wildcard nnue/pawnstar-v9.bin)
NNUE_REF            = $(wildcard test/nnue_reference.txt)
TEST_OBJECTS        = $(BUILD_DIR)/perft_test.o \
                      $(BUILD_DIR)/see_test.o \
                      $(BUILD_DIR)/bratko_kopec_nnue_test.o \
                      $(BUILD_DIR)/nnue_test.o \
                      $(BUILD_DIR)/nnue_incremental_test.o \
                      $(BUILD_DIR)/opening_book_test.o
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

# Link-time optimization (ThinLTO), on by default for release builds. LTO lets the optimizer inline and
# specialize across translation units, which plain per-TU -O3 cannot do — it matters most for the small
# hot-path methods that callers reach across .cpp boundaries. CXXFLAGS feeds both the compile and link
# steps, so adding the flag here enables LTO end to end. Disable with `make LTO=0`; forced off for the
# sanitizer DEBUG build, where LTO interferes with instrumentation and slows the build for no benefit.
LTO ?= 1
ifeq ($(DEBUG), 1)
	LTO := 0
endif
ifeq ($(LTO), 1)
	CXXFLAGS += -flto=thin
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

tests: prep $(TEST_PERFT_EXE) $(TEST_SEE_EXE) $(TEST_BK_NNUE_EXE) $(TEST_NNUE_EXE) $(TEST_NNUE_INC_EXE) $(TEST_BOOK_EXE)

# Run every suite in one shell, chained with && so the first failure aborts (and the summary below is
# skipped). On full success report the wall-clock time spent running the tests (not the build). Depends on
# `tools` as well so a broken helper (e.g. filter_book) fails the check instead of rotting unnoticed.
check: tests tools
	@start=$$(date +%s%3N); \
	$(TEST_PERFT_EXE) && \
	$(TEST_BK_NNUE_EXE) $(NNUE_NET) && \
	$(TEST_NNUE_EXE) $(NNUE_NET) $(NNUE_REF) && \
	$(TEST_NNUE_INC_EXE) $(NNUE_NET) && \
	$(TEST_SEE_EXE) && \
	$(TEST_BOOK_EXE) && \
	echo "all tests passed in $$(( $$(date +%s%3N) - start )) ms"

prep:
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(PROGRAM_EXE) $(TEST_PERFT_EXE) $(TEST_SEE_EXE) $(TEST_BK_NNUE_EXE) $(TEST_NNUE_EXE) $(TEST_NNUE_INC_EXE) $(TEST_BOOK_EXE) \
	      $(TOOL_STAMP_EXE) $(BUILD_DIR)/stamp_net.o $(BUILD_DIR)/stamp_net.d \
	      $(TOOL_FILTERBOOK_EXE) $(BUILD_DIR)/filter_book.o $(BUILD_DIR)/filter_book.d \
	      $(TOOL_QUANT_EXE) $(BUILD_DIR)/nnue_quant_study.o $(BUILD_DIR)/nnue_quant_study.d \
	      $(OBJECTS) $(EMBED_OBJECT) $(TEST_OBJECTS) $(DEPS) $(TEST_DEPS)

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

# Link the main executable (with the embedded-net object).
$(PROGRAM_EXE): $(OBJECTS) $(EMBED_OBJECT)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(PROGRAM_EXE) $(OBJECTS) $(EMBED_OBJECT)

# Link test executables against all engine objects (no main.o).
$(TEST_PERFT_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/perft_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_SEE_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/see_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_BK_NNUE_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/bratko_kopec_nnue_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_NNUE_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/nnue_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_NNUE_INC_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/nnue_incremental_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_BOOK_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/opening_book_test.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Link the net-stamping tool (header-only use of nnue.h constants; no engine objects needed).
$(TOOL_STAMP_EXE): $(BUILD_DIR)/stamp_net.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Link the opening-book filter against all engine objects (it drives the NNUE search).
$(TOOL_FILTERBOOK_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/filter_book.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Link the int8-quantisation study tool (drives NNUE inference; needs the engine objects).
$(TOOL_QUANT_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/nnue_quant_study.o
	$(CXX) $(CXXFLAGS) -o $@ $^

-include $(DEPS)
-include $(TEST_DEPS)
