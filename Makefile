PROGRAM             = pawnstar
CXX                 = g++
CXXFLAGS            = -I include -Wall -Wextra -std=c++20 -Wno-class-memaccess

GENERATED_DATA      = generated_data.cpp
GENERATED_DATA_SRC  = generate_constants/generate_constants.cpp
GENERATED_DATA_EXE  = generate_constants/gen_constants

HDRS                = $(notdir $(wildcard include/*.h))
SRCS                = $(notdir $(wildcard src/*.cpp)) $(GENERATED_DATA)

OBJS                = $(SRCS:.cpp=.o)

DBGDIR              = debug
DBGEXE              = $(DBGDIR)/$(PROGRAM)
DBGOBJS             = $(addprefix $(DBGDIR)/,$(OBJS))
DBGFLAGS            = -g -O0 -DDEBUG

RELDIR              = release
RELEXE              = $(RELDIR)/$(PROGRAM)
RELOBJS             = $(addprefix $(RELDIR)/, $(OBJS))
RELFLAGS            = -O3 -DNDEBUG

.PHONY: all clean debug prep release remake

all: debug release

debug: prep $(DBGEXE)

$(DBGEXE): $(DBGOBJS)
	$(CXX) $(CXXFLAGS) $(DBGFLAGS) -o $(DBGEXE) $^

$(DBGDIR)/%.o: src/%.cpp $(addprefix include/, $(HDRS))
	$(CXX) -c $(CXXFLAGS) $(DBGFLAGS) -o $@ $<

release: prep $(RELEXE)

$(RELEXE): $(RELOBJS)
	$(CXX) $(CXXFLAGS) $(RELFLAGS) -o $(RELEXE) $^

$(RELDIR)/%.o: src/%.cpp $(addprefix include/, $(HDRS))
	$(CXX) -c $(CXXFLAGS) $(RELFLAGS) -o $@ $<

src/$(GENERATED_DATA) : $(GENERATED_DATA_SRC)
	$(CXX) $< -o $(GENERATED_DATA_EXE) $(CXXFLAGS)
	$(GENERATED_DATA_EXE) > $@
	
prep:
	mkdir -p $(DBGDIR) $(RELDIR)

remake: clean all

clean:
	rm -f $(RELEXE) $(RELOBJS) $(DBGEXE) $(DBGOBJS) src/$(GENERATED_DATA) $(GENERATED_DATA_EXE)