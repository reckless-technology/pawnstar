PROGRAM  = pawnstar
CC       = gcc
CXX      = g++
CFLAGS   = -I. -Wall -Wextra -O3 -Wno-unused-parameter -Wno-unknown-pragmas -flto -std=gnu99
CXXFLAGS = -I. -Wall -Wextra -O3 -Wno-unused-parameter -Wno-unknown-pragmas -flto -std=c++11
LDFLAGS  = -lpthread
HDRS     = *.h
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

clean:
	rm -f $(OBJ) $(PROGRAM)