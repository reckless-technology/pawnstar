#include "constants.h"
#include "debug_hashtable.h"
#include "game.h"
#include "nnue.h"
#include "opening_book.h"
#include "position.h"
#include "transposition_table.h"

#include <execinfo.h>
#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <unistd.h>

/// @brief Process a single line of UCI input (defined in input_handling.cpp).
void ProcessInput(Game &game, std::string_view line);

/// @brief Program entry point. Prints the banner, loads the book, then runs the UCI input loop.
/// @return Process exit code.
int main()
{
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    std::cout << std::unitbuf;
    std::cout << "   .::.    \n"
                 "   _::_    \n"
                 " _/____\\_ \n"
                 " \\      / \n"
                 "  \\____/  \n"
                 "  (____)   \n"
                 "   |  |    \n"
                 "   |__|    \n"
                 "  /    \\  \n"
                 " (______)  \n"
                 "(________) \n"
                 "/________\\\n"
                 "Pawnstar: a UCI compatible chess engine\n"
                 "(C) Jonny Reckless 2009 - 2024\n"
                 "Compiled: " __DATE__ " " __TIME__ "\n";

    Game game{};
    if (!game.book.Initialize("pawnstar.book"))
    {
        std::cout << "info string Unable to open book file.\n";
    }
    // NNUE is the only evaluator, so a net is required. Load the shipped net from the working directory
    // (like the opening book); PAWNSTAR_EVALFILE=<path> overrides the path, and UCI `setoption name
    // EvalFile value <path>` loads a different net at runtime. If the net cannot be loaded there is no
    // evaluation to fall back to, so the engine reports the error and exits.
    const char       *eval_file = std::getenv("PAWNSTAR_EVALFILE");
    const std::string net_path  = eval_file ? eval_file : "nnue/pawnstar-v7.bin";
    if (!game.NnueNetwork().Load(net_path))
    {
        std::cout << "info string FATAL: could not load NNUE net '" << net_path
                  << "'. NNUE is the only evaluator; set PAWNSTAR_EVALFILE to a valid net.\n";
        return 1;
    }
    DebugXClear();
    std::cout << "ready\n";
    for (std::string line; std::getline(std::cin, line);)
    {
        ProcessInput(game, line);
    }
}
