PROGRAM             = pawnstar
CXX                 = clang++
CPPFLAGS            = -I include -D DEBUGX=1
CXXFLAGS            = $(CPPFLAGS) -Wall -Wextra -Wpedantic -std=c++20 -mbmi2 
BUILD_DIR           = build
PROGRAM_EXE         = $(BUILD_DIR)/$(PROGRAM)
GENERATED_DATA      = src/generated_data.cpp
GENERATOR_SOURCE    = generate_constants/generate_constants.cpp
GENERATOR_EXE       = generate_constants/gen_constants
HEADERS             = $(wildcard include/*.h)
OBJECTS             = $(addprefix $(BUILD_DIR)/, $(SOURCES:.cpp=.o))
SOURCES             = \
	debug_hashtable.cpp \
	evaluation.cpp game.cpp \
	input_handling.cpp \
	main.cpp \
	opening_book.cpp \
	perft_results.cpp \
	position.cpp \
	search_alphabeta.cpp \
	search_quiescent.cpp \
	search_root_node.cpp \
	static_exchange_evaluation.cpp \
	tests_bratko_kopec.cpp \
	tests_move_generation.cpp \
	tests_static_exchange.cpp \
	timer.cpp \
	transposition_table.cpp \
	$(notdir $(GENERATED_DATA))

ifeq ($(DEBUG), 1)
	CXXFLAGS += -g -O0 -D DEBUG -fsanitize=undefined -fsanitize=address
else
	CXXFLAGS += -g -O3 -D NDEBUG
endif

.PHONY: all prep clean gen

all: prep $(PROGRAM_EXE)

prep:
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(PROGRAM_EXE) $(OBJECTS) $(GENERATED_DATA) $(GENERATOR_EXE)

gen: $(GENERATOR_EXE)

# Compile an object from source.
$(BUILD_DIR)/%.o: src/%.cpp $(HEADERS)
	$(CXX) -c $(CXXFLAGS)  -o $@ $<

# Link the executable.
$(PROGRAM_EXE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(PROGRAM_EXE) $(OBJECTS)

# Compile the generator executable.
$(GENERATOR_EXE) : $(GENERATOR_SOURCE)
	$(CXX) $(CXXFLAGS) -o $(GENERATOR_EXE) $(GENERATOR_SOURCE)

# Invoke the generator executable to create the generated data source file.
$(GENERATED_DATA) : $(GENERATOR_EXE)
	$(GENERATOR_EXE) > $(GENERATED_DATA)

