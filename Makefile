PROGRAM             = pawnstar
CXX                 = clang++
CPPFLAGS            = -I include -D DEBUGX=1
CXXFLAGS            = $(CPPFLAGS) -Wall -Wextra -Wpedantic -std=c++20

DEBUG_DIR           = debug
RELEASE_DIR         = release

GENERATED_DATA      = include/generated_data.inc
GENERATOR_SOURCE    = generate_constants/generate_constants.cpp
GENERATOR_EXE       = generate_constants/gen_constants

HEADERS             = $(wildcard include/*.h)

SOURCES             = \
	debug_hashtable.cpp \
	evaluation.cpp game.cpp \
	input_handling.cpp \
	main.cpp \
	opening_book.cpp \
	perft_results.cpp \
	position.cpp \
	random.cpp \
	search_alphabeta.cpp \
	search_quiescent.cpp \
	search_root_node.cpp \
	sort_moves.cpp \
	static_exchange_evaluation.cpp \
	tests_bratko_kopec.cpp \
	tests_move_generation.cpp \
	tests_static_exchange.cpp \
	timer.cpp \
	transposition_table.cpp

OBJECTS             = $(SOURCES:.cpp=.o)

DEBUG_EXE           = $(DEBUG_DIR)/$(PROGRAM)
DEBUG_OBJECTS       = $(addprefix $(DEBUG_DIR)/,$(OBJECTS))
DEBUG_FLAGS         = -g -O0 -D DEBUG -fsanitize=undefined -fsanitize=address

RELEASE_EXE         = $(RELEASE_DIR)/$(PROGRAM)
RELEASE_OBJECTS     = $(addprefix $(RELEASE_DIR)/, $(OBJECTS))
RELEASE_FLAGS       = -g -O3 -D NDEBUG

.PHONY: all clean debug prep release remake gen

all: release

gen: $(GENERATOR_EXE)

debug: prep $(DEBUG_EXE)

$(DEBUG_EXE): $(DEBUG_OBJECTS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(DEBUG_EXE) $(DEBUG_OBJECTS)

# Compile a debug object from source
$(DEBUG_DIR)/%.o: src/%.cpp $(HEADERS) $(GENERATED_DATA)
	$(CXX) -c $(CXXFLAGS) $(DEBUG_FLAGS) -o $@ $<

release: prep $(RELEASE_EXE)

$(RELEASE_EXE): $(RELEASE_OBJECTS)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -o $(RELEASE_EXE) $(RELEASE_OBJECTS)

# Compile a release object from source
$(RELEASE_DIR)/%.o: src/%.cpp $(HEADERS) $(GENERATED_DATA)
	$(CXX) -c $(CXXFLAGS) $(RELEASE_FLAGS) -o $@ $<

$(GENERATED_DATA) : $(GENERATOR_EXE)
	$(GENERATOR_EXE) > $(GENERATED_DATA)

$(GENERATOR_EXE) : $(GENERATOR_SOURCE)
	$(CXX) -g -Og $(CXXFLAGS) -o $(GENERATOR_EXE) $(GENERATOR_SOURCE)
	
prep:
	mkdir -p $(DEBUG_DIR) $(RELEASE_DIR)

remake: clean all

clean:
	rm -f $(RELEASE_EXE) $(RELEASE_OBJECTS) $(DEBUG_EXE) $(DEBUG_OBJECTS) $(GENERATED_DATA) $(GENERATOR_EXE)