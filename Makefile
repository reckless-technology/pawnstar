PROGRAM     = pawnstar
CXX         = g++
CXXFLAGS    = -I include -Wall -Wextra -std=c++17

HDRS = \
	bitboard.h \
	function_prototypes.h \
	move.h \
	move_generation_template.h \
	options.h \
	pawnstar.h \
	types.h
	
GENERATED_DATA      = generated_data.cpp
GENERATED_DATA_SRC  = generate_constants/generate_constants.cpp
GENERATED_DATA_EXE  = generate_constants/gen_constants

CPPSRCS = \
	attacks.cpp \
	debug_hashtable.cpp \
	evaluation.cpp \
	forsyth_edwards.cpp \
	game_over_detection.cpp \
	input_handling.cpp \
	main.cpp \
	make_moves.cpp \
	move_generation.cpp \
	move_to_string.cpp \
	opening_book.cpp \
	opening_book_moves.cpp \
	pins.cpp \
	random.cpp \
	search_alphabeta.cpp \
	search_quiescent.cpp \
	search_root_node.cpp \
	search_single_move.cpp \
	search_start_stop.cpp \
	sort_moves.cpp \
	static_exchange_evaluation.cpp \
	tests_bratko_kopec.cpp \
	tests_merge_sort.cpp \
	tests_move_generation.cpp \
	tests_static_exchange.cpp \
	timer.cpp \
	transposition_table.cpp \
	zobrist.cpp \
	$(GENERATED_DATA)


OBJS = $(CPPSRCS:.cpp=.o)

DBGDIR    = debug
DBGEXE    = $(DBGDIR)/$(PROGRAM)
DBGOBJS   = $(addprefix $(DBGDIR)/,$(OBJS))
DBGFLAGS  = -g -O0 -DDEBUG

RELDIR    = release
RELEXE    = $(RELDIR)/$(PROGRAM)
RELOBJS   = $(addprefix $(RELDIR)/, $(OBJS))
RELFLAGS  = -O3 -DNDEBUG

.PHONY: all clean debug prep release remake

all: prep debug release

debug: $(DBGEXE)

$(DBGEXE): $(DBGOBJS)
	$(CXX) $(CXXFLAGS) $(DBGFLAGS) -o $(DBGEXE) $^

$(DBGDIR)/%.o: src/%.cpp $(addprefix include/, $(HDRS))
	$(CXX) -c $(CXXFLAGS) $(DBGFLAGS) -o $@ $<

release: $(RELEXE)

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