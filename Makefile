PROGRAM	 = pawnstar
CC		 = gcc
CXX		 = g++
CFLAGS	 = -I. -Iinclude -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-unknown-pragmas -Wno-missing-field-initializers -std=gnu99
CXXFLAGS = -I. -Iinclude -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-unknown-pragmas -Wno-missing-field-initializers -std=c++11
LDFLAGS	 = -lpthread

HDRS = \
	bitboard_constants.h \
	function_prototypes.h \
	inline_functions.h \
	macros.h \
	options.h \
	pawnstar.h \
	types.h
	
GEN_DATA = generated_data.c
GEN_SRC	 = generate_constants/generate_constants.c
GEN_EXE	 = generate_constants/gen_constants

CSRCS = \
	attacks.c \
	debug_hashtable.c \
	evaluation.c \
	evaluation_full.c \
	forsyth_edwards.c \
	game_over_detection.c \
	input_handling.c \
	main.c \
	make_moves.c \
	move_generation.c \
	move_to_string.c \
	opening_book.c \
	opening_book_moves.c \
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
	$(GEN_DATA)
	

CPPSRCS	 = \
	search_start_stop.cpp \
	timer.cpp
	
OBJS = $(notdir $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o) )

DBGDIR = debug
DBGEXE = $(DBGDIR)/$(PROGRAM)
DBGOBJS = $(addprefix $(DBGDIR)/,$(OBJS))
DBGCFLAGS = -g -O0 -DDEBUG

RELDIR = release
RELEXE = $(RELDIR)/$(PROGRAM)
RELOBJS = $(addprefix $(RELDIR)/, $(OBJS))
RELCFLAGS = -g -O3 -DNDEBUG

.PHONY: all clean debug prep release remake

all: prep debug release

debug: $(DBGEXE)

$(DBGEXE): $(DBGOBJS) 
	$(CXX) $(CFLAGS) $(DBGCFLAGS) $(LDFLAGS) -o $(DBGEXE) $^

$(DBGDIR)/%.o: src/%.c $(addprefix include/, $(HDRS))
	$(CC) -c $(CFLAGS) $(DBGCFLAGS) -o $@ $<
	
$(DBGDIR)/%.o: src/%.cpp $(addprefix include/, $(HDRS))
	$(CXX) -c $(CXXFLAGS) $(DBGCFLAGS) -o $@ $<

release: $(RELEXE)

$(RELEXE): $(RELOBJS)
	$(CXX) $(CFLAGS) $(RELCFLAGS) $(LDFLAGS) -o $(RELEXE) $^

$(RELDIR)/%.o: src/%.c $(addprefix include/, $(HDRS))
	$(CC) -c $(CFLAGS) $(RELCFLAGS) -o $@ $<
	
$(RELDIR)/%.o: src/%.cpp $(addprefix include/, $(HDRS))
	$(CXX) -c $(CXXFLAGS) $(RELCFLAGS) -o $@ $<

src/$(GEN_DATA) : $(GEN_SRC)
	$(CC) $< -o $(GEN_EXE) $(CFLAGS)
	$(GEN_EXE) > $@
	
prep:
	mkdir -p $(DBGDIR) $(RELDIR)

remake: clean all

clean:
	rm -f $(RELEXE) $(RELOBJS) $(DBGEXE) $(DBGOBJS) src/$(GEN_DATA) $(GEN_EXE)
	rm -r $(DBGDIR) $(RELDIR)
