#include "debug_hashtable.h"
#include "function_prototypes.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"
#include "transposition_table.h"

#include <iostream>
#include <string>

int main()
{
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    printf(
#if 1
        "                       .::.                            \n"
        "                       _::_                            \n"
        "                     _/____\\_                         \n"
        "                     \\      /                         \n"
        "                      \\____/                          \n"
        "                      (____)                           \n"
        "                       |  |                            \n"
        "                       |__|                            \n"
        "                      /    \\                          \n"
        "                     (______)                          \n"
        "                    (________)                         \n"
        "                    /________\\                      \n\n"
#endif
        "Pawnstar: A Winboard and Xboard compatible chess engine\n"
        "(C) Jonny Reckless 2009 - 2023                         \n"
        "Compiled: " __DATE__ " " __TIME__ "\n");
    InitializeTranspositionTable(HASHTABLE_MEGABYTES);
    if (!InitializeOpeningBookFromFile("pawnstar.book"))
    {
        printf("NOTE: unable to open book file\n");
    }
    Game game{};
    DEBUG_STATEMENT(DebugXClear());
    for (;;)
    {
        std::string line;
        std::getline(std::cin, line);
        ProcessInput(game, line);
    }
}
