Pawnstar is a chess engine that supports the UCI (Arena) protocol. It is written in C++20 and uses bitboards for representation of the board state. 
It has a fast legal move generator (approx 700 million moves per second on my laptop).

You need `clang++` and GNU `make` for compilation. 

To build the executable run `make` in this directory.
To execute pawnstar run `build/pawnstar` from this directory.
To generate doxygen documentation run `doxygen .` from this directory.
