PROGRAM             = pawnstar
CXX                 = clang++
CPPFLAGS            = -I src
CXXFLAGS            = $(CPPFLAGS) -Wall -Wextra -Wpedantic -std=c++20 -mbmi2 -mavx2
BUILD_DIR           = build
DOC_DIR             = doc/html
PROGRAM_EXE         = $(BUILD_DIR)/$(PROGRAM)
GENERATED_DATA      = src/generated_data.cpp
GENERATOR_SOURCE    = src/generate_constants.cpp
GENERATOR_EXE       = $(BUILD_DIR)/gen_constants
TEST_DIR            = test
TOOLS_DIR           = tools
TOOL_GENDATA_EXE    = $(BUILD_DIR)/gen_data
TOOL_STAMP_EXE      = $(BUILD_DIR)/stamp_net
TOOL_FILTERBOOK_EXE = $(BUILD_DIR)/filter_book

ENGINE_SOURCES      = \
	debug_hashtable.cpp \
	evaluation.cpp \
	game.cpp \
	input_handling.cpp \
	nnue.cpp \
	opening_book.cpp \
	position.cpp \
	search_alphabeta.cpp \
	search_quiescent.cpp \
	search_root_node.cpp \
	search_state.cpp \
	static_exchange_evaluation.cpp \
	transposition_table.cpp \
	$(notdir $(GENERATED_DATA))

SOURCES             = $(ENGINE_SOURCES) main.cpp
ENGINE_OBJECTS      = $(addprefix $(BUILD_DIR)/, $(ENGINE_SOURCES:.cpp=.o))
OBJECTS             = $(ENGINE_OBJECTS) $(BUILD_DIR)/main.o
DEPS                = $(OBJECTS:.o=.d)
# The shipped NNUE net embedded into the engine binary (.incbin) as a runtime-load fallback. Linked into
# the engine executable only — the test binaries load nets explicitly and don't need it.
EMBED_NET           = nnue/pawnstar-v7.bin
EMBED_OBJECT        = $(BUILD_DIR)/embedded_net.o

TEST_PERFT_EXE      = $(BUILD_DIR)/test_perft
TEST_SEE_EXE        = $(BUILD_DIR)/test_see
TEST_BK_NNUE_EXE    = $(BUILD_DIR)/test_bratko_kopec_nnue
TEST_NNUE_EXE       = $(BUILD_DIR)/test_nnue
TEST_NNUE_INC_EXE   = $(BUILD_DIR)/test_nnue_incremental
TEST_BOOK_EXE       = $(BUILD_DIR)/test_opening_book
# Shipped net + checked-in trainer reference, used by `make check` to exercise NNUE inference and the
# incremental accumulator. Resolved with wildcard so the checks degrade to a no-op (still green) if absent.
NNUE_NET            = $(wildcard nnue/pawnstar-v7.bin)
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

.PHONY: all tests check prep clean gen doc tools

all: prep $(PROGRAM_EXE)

tools: prep $(TOOL_GENDATA_EXE) $(TOOL_STAMP_EXE) $(TOOL_FILTERBOOK_EXE)

tests: prep $(TEST_PERFT_EXE) $(TEST_SEE_EXE) $(TEST_BK_NNUE_EXE) $(TEST_NNUE_EXE) $(TEST_NNUE_INC_EXE) $(TEST_BOOK_EXE)

# Run every suite in one shell, chained with && so the first failure aborts (and the summary below is
# skipped). On full success report the wall-clock time spent running the tests (not the build).
check: tests
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
	      $(TOOL_GENDATA_EXE) $(BUILD_DIR)/gen_data.o $(BUILD_DIR)/gen_data.d \
	      $(TOOL_STAMP_EXE) $(BUILD_DIR)/stamp_net.o $(BUILD_DIR)/stamp_net.d \
	      $(TOOL_FILTERBOOK_EXE) $(BUILD_DIR)/filter_book.o $(BUILD_DIR)/filter_book.d \
	      $(OBJECTS) $(EMBED_OBJECT) $(TEST_OBJECTS) $(DEPS) $(TEST_DEPS) \
	      $(GENERATED_DATA) $(GENERATOR_EXE)

gen: prep $(GENERATOR_EXE)

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
# compile — the C++ flags don't apply to the .S stub. The .incbin path is relative to the make cwd (root).
$(EMBED_OBJECT): src/embedded_net.S $(EMBED_NET) | prep
	$(CXX) -c -o $@ src/embedded_net.S

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

# Link the self-play data generator against all engine objects.
$(TOOL_GENDATA_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/gen_data.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Link the net-stamping tool (header-only use of nnue.h constants; no engine objects needed).
$(TOOL_STAMP_EXE): $(BUILD_DIR)/stamp_net.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Link the opening-book filter against all engine objects (it drives the NNUE search).
$(TOOL_FILTERBOOK_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/filter_book.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile the generator executable.
$(GENERATOR_EXE): $(GENERATOR_SOURCE) | prep
	$(CXX) $(CXXFLAGS) -o $(GENERATOR_EXE) $(GENERATOR_SOURCE)

# Invoke the generator executable to create the generated data source file.
$(GENERATED_DATA): $(GENERATOR_EXE)
	$(GENERATOR_EXE) > $(GENERATED_DATA)

-include $(DEPS)
-include $(TEST_DEPS)
