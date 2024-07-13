PROGRAM             = pawnstar
CXX                 = clang++
CPPFLAGS            = -I include -D DEBUGX=1 -D DO_NULL_MOVE_PRUNING=1 -D DO_LATE_MOVE_REDUCTION=1
CXXFLAGS            = $(CPPFLAGS) -Wall -Wextra -std=c++20

DEBUG_DIR           = debug
RELEASE_DIR         = release

GENERATED_DATA_CPP  = generated_data.cpp
GENERATOR_SOURCE    = generate_constants/generate_constants.cpp
GENERATOR_EXE       = generate_constants/gen_constants

HEADERS             = $(notdir $(wildcard include/*.h))
SOURCES             = $(notdir $(wildcard src/*.cpp)) $(GENERATED_DATA_CPP)

OBJECTS             = $(SOURCES:.cpp=.o)

DEBUG_EXE           = $(DEBUG_DIR)/$(PROGRAM)
DEBUG_OBJECTS       = $(addprefix $(DEBUG_DIR)/,$(OBJECTS))
DEBUG_FLAGS         = -g -O0 -DDEBUG

RELEASE_EXE         = $(RELEASE_DIR)/$(PROGRAM)
RELEASE_OBJECTS     = $(addprefix $(RELEASE_DIR)/, $(OBJECTS))
RELEASE_FLAGS       = -O3 -DNDEBUG

.PHONY: all clean debug prep release remake gen

all: debug release

gen: $(GENERATOR_EXE)

debug: prep $(DEBUG_EXE)

$(DEBUG_EXE): $(DEBUG_OBJECTS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(DEBUG_EXE) $(DEBUG_OBJECTS)

# Compile a debug object from source
$(DEBUG_DIR)/%.o: src/%.cpp $(addprefix include/, $(HEADERS))
	$(CXX) -c $(CXXFLAGS) $(DEBUG_FLAGS) -o $@ $<

release: prep $(RELEASE_EXE)

$(RELEASE_EXE): $(RELEASE_OBJECTS)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -o $(RELEASE_EXE) $(RELEASE_OBJECTS)

# Compile a release object from source
$(RELEASE_DIR)/%.o: src/%.cpp $(addprefix include/, $(HEADERS))
	$(CXX) -c $(CXXFLAGS) $(RELEASE_FLAGS) -o $@ $<

src/$(GENERATED_DATA_CPP) : $(GENERATOR_EXE)
	$(GENERATOR_EXE) > src/$(GENERATED_DATA_CPP)

$(GENERATOR_EXE) : $(GENERATOR_SOURCE)
	$(CXX) -g -Og $(CXXFLAGS) -o $(GENERATOR_EXE) $(GENERATOR_SOURCE)
	
prep:
	mkdir -p $(DEBUG_DIR) $(RELEASE_DIR)

remake: clean all

clean:
	rm -f $(RELEASE_EXE) $(RELEASE_OBJECTS) $(DEBUG_EXE) $(DEBUG_OBJECTS) src/$(GENERATED_DATA_CPP) $(GENERATOR_EXE)