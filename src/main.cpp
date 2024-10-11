#include "debug_hashtable.h"
#include "game.h"
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

void ProcessInput(Game &game, std::string_view line);

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
    InitializeTranspositionTable(HASHTABLE_MEGABYTES);
    if (!InitializeOpeningBookFromFile("doc/pawnstar.book"))
    {
        std::cout << "NOTE: unable to open book file\n";
    }
    Game game{};
    DebugXClear();
    for (std::string line; std::getline(std::cin, line);)
    {
        ProcessInput(game, line);
    }
}
