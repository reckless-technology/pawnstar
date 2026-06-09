PROGRAM             = pawnstar
CXX                 = clang++
CPPFLAGS            = -I src -D DEBUGX=1
CXXFLAGS            = $(CPPFLAGS) -Wall -Wextra -Wpedantic -std=c++20 -mbmi2
BUILD_DIR           = build
PROGRAM_EXE         = $(BUILD_DIR)/$(PROGRAM)
GENERATED_DATA      = src/generated_data.cpp
GENERATOR_SOURCE    = generate_constants/generate_constants.cpp
GENERATOR_EXE       = generate_constants/gen_constants
TEST_DIR            = test

ENGINE_SOURCES      = \
	debug_hashtable.cpp \
	evaluation.cpp \
	game.cpp \
	input_handling.cpp \
	opening_book.cpp \
	position.cpp \
	search_alphabeta.cpp \
	search_quiescent.cpp \
	search_root_node.cpp \
	search_state.cpp \
	static_exchange_evaluation.cpp \
	thread_pool.cpp \
	transposition_table.cpp \
	$(notdir $(GENERATED_DATA))

SOURCES             = $(ENGINE_SOURCES) main.cpp
ENGINE_OBJECTS      = $(addprefix $(BUILD_DIR)/, $(ENGINE_SOURCES:.cpp=.o))
OBJECTS             = $(ENGINE_OBJECTS) $(BUILD_DIR)/main.o
DEPS                = $(OBJECTS:.o=.d)

TEST_PERFT_EXE      = $(BUILD_DIR)/test_perft
TEST_SEE_EXE        = $(BUILD_DIR)/test_see
TEST_BK_EXE         = $(BUILD_DIR)/test_bratko_kopec
TEST_PS_EXE         = $(BUILD_DIR)/test_pawn_structure
TEST_OBJECTS        = $(BUILD_DIR)/perft.o \
                      $(BUILD_DIR)/see.o \
                      $(BUILD_DIR)/bratko_kopec.o \
                      $(BUILD_DIR)/pawn_structure.o
TEST_DEPS           = $(TEST_OBJECTS:.o=.d)

ifeq ($(DEBUG), 1)
	CXXFLAGS += -g -Og -D DEBUG -fsanitize=undefined -fsanitize=address
else
	CXXFLAGS += -g -O3 -D NDEBUG
endif

.PHONY: all tests check prep clean gen

all: prep $(PROGRAM_EXE)

tests: prep $(TEST_PERFT_EXE) $(TEST_SEE_EXE) $(TEST_BK_EXE) $(TEST_PS_EXE)

check: tests
	$(TEST_SEE_EXE)
	$(TEST_PS_EXE)
	$(TEST_PERFT_EXE)
	$(TEST_BK_EXE)

prep:
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(PROGRAM_EXE) $(TEST_PERFT_EXE) $(TEST_SEE_EXE) $(TEST_BK_EXE) $(TEST_PS_EXE) \
	      $(OBJECTS) $(TEST_OBJECTS) $(DEPS) $(TEST_DEPS) \
	      $(GENERATED_DATA) $(GENERATOR_EXE)

gen: $(GENERATOR_EXE)

# Compile an engine source object.
$(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) -c $(CXXFLAGS) -MMD -MP -o $@ $<

# Compile a test source object.
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) -MMD -MP -o $@ $<

# Link the main executable.
$(PROGRAM_EXE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(PROGRAM_EXE) $(OBJECTS)

# Link test executables against all engine objects (no main.o).
$(TEST_PERFT_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/perft.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_SEE_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/see.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_BK_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/bratko_kopec.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_PS_EXE): $(ENGINE_OBJECTS) $(BUILD_DIR)/pawn_structure.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile the generator executable.
$(GENERATOR_EXE): $(GENERATOR_SOURCE)
	$(CXX) $(CXXFLAGS) -o $(GENERATOR_EXE) $(GENERATOR_SOURCE)

# Invoke the generator executable to create the generated data source file.
$(GENERATED_DATA): $(GENERATOR_EXE)
	$(GENERATOR_EXE) > $(GENERATED_DATA)

-include $(DEPS)
-include $(TEST_DEPS)
