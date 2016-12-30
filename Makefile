PROGRAM  = pawnstar
CC       = clang
CXX      = clang++
CFLAGS   = -I. -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas -std=gnu99 -O3
CXXFLAGS = -I. -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas -std=c++11 -O3
LDFLAGS  = -lpthread
HDRS     = $(wildcard *.h)
CSRCS    = $(wildcard *.c)
CPPSRCS  = $(wildcard *.cpp)
OBJ      = $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o)

%.o: %.c $(HDRS)
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.cpp $(HDRS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

all: $(PROGRAM)

$(PROGRAM): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJ) $(PROGRAM)