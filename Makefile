PROGRAM     = pawnstar
CC          = gcc
CXX         = g++
COMMONFLAGS = -I include -Wall -Wextra
CFLAGS      = $(COMMONFLAGS) -std=c99
CXXFLAGS    = $(COMMONFLAGS) -std=c++11

HDRS = \
	bitboard_constants.h \
	function_prototypes.h \
	inline_functions.h \
	macros.h \
	move.h \
	move_generation_template.h \
	options.h \
	pawnstar.h \
	types.h
	
GENERATED_DATA      = generated_data.c
GENERATED_DATA_SRC  = generate_constants/generate_constants.c
GENERATED_DATA_EXE  = generate_constants/gen_constants

CSRCS = \
	attacks.c \
	debug_hashtable.c \
	evaluation.c \
	forsyth_edwards.c \
	game_over_detection.c \
	input_handling.c \
	main.c \
	make_moves.c \
	move_generation.c \
	move_to_string.c \
	opening_book.c \
	opening_book_moves.c \
	pins.c \
	random.c \
	search_alphabeta.c \
	search_quiescent.c \
	search_root_node.c \
	search_single_move.c \
	sort_moves.c \
	static_exchange_evaluation.c \
	tests_bratko_kopec.c \
	tests_merge_sort.c \
	tests_move_generation.c \
	tests_static_exchange.c \
	transposition_table.c \
	zobrist.c \
	$(GENERATED_DATA)

CPPSRCS = \
	search_start_stop.cpp \
	timer.cpp

OBJS = $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o)

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

$(DBGDIR)/%.o: src/%.c $(addprefix include/, $(HDRS))
	$(CC) -c $(CFLAGS) $(DBGFLAGS) -o $@ $<
	
$(DBGDIR)/%.o: src/%.cpp $(addprefix include/, $(HDRS))
	$(CXX) -c $(CXXFLAGS) $(DBGFLAGS) -o $@ $<

release: $(RELEXE)

$(RELEXE): $(RELOBJS)
	$(CXX) $(CXXFLAGS) $(RELFLAGS) -o $(RELEXE) $^

$(RELDIR)/%.o: src/%.c $(addprefix include/, $(HDRS))
	$(CC) -c $(CFLAGS) $(RELFLAGS) -o $@ $<
	
$(RELDIR)/%.o: src/%.cpp $(addprefix include/, $(HDRS))
	$(CXX) -c $(CXXFLAGS) $(RELFLAGS) -o $@ $<

src/$(GENERATED_DATA) : $(GENERATED_DATA_SRC)
	$(CC) $< -o $(GENERATED_DATA_EXE) $(CFLAGS)
	$(GENERATED_DATA_EXE) > $@
	
prep:
	mkdir -p $(DBGDIR) $(RELDIR)

remake: clean all

clean:
	rm -f $(RELEXE) $(RELOBJS) $(DBGEXE) $(DBGOBJS) src/$(GENERATED_DATA) $(GENERATED_DATA_EXE)
	rm -r $(DBGDIR) $(RELDIR)