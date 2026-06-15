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
    // NNUE evaluation is ON by default, loading the shipped net from the working directory (like the
    // opening book). Env vars override: PAWNSTAR_EVALFILE=<path> picks a different net, PAWNSTAR_NNUE=0
    // disables NNUE. UCI setoption (UseNNUE / EvalFile) also works. If the net can't be loaded, the
    // engine falls back to the handcrafted evaluation.
    const char       *eval_file = std::getenv("PAWNSTAR_EVALFILE");
    const std::string net_path  = eval_file ? eval_file : "nnue/pawnstar-v4.bin";
    const bool        net_loaded = game.NnueNetwork().Load(net_path);
    const char       *use_env   = std::getenv("PAWNSTAR_NNUE");
    const bool        use_nnue =
        use_env ? (std::string_view(use_env) == "1" || std::string_view(use_env) == "true") : true;
    game.SetUseNnue(use_nnue);
    if (use_nnue && !net_loaded)
    {
        std::cout << "info string NNUE enabled but net '" << net_path
                  << "' not found; using the handcrafted evaluation (set EvalFile to a valid net)\n";
    }
    DebugXClear();
    std::cout << "ready\n";
    for (std::string line; std::getline(std::cin, line);)
    {
        ProcessInput(game, line);
    }
}
