#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"
#include "game.h"
#include "opening_book.h"

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
        "Compiled: " __DATE__ " " __TIME__                     "\n"
        );
    InitializeTranspositionTable(HASHTABLE_MEGABYTES);
    InitializeGoodMoveCounts();
    if (!InitializeOpeningBookFromFile("pawnstar.book"))
    {
        printf("NOTE: using built in opening book\n");
        if (!InitializeDefaultOpeningBook())
        {
            printf("ERROR: unable to create default opening book\n");
        }
    }
    Game game;
    DEBUG_STATEMENT(DebugXClear());
    for ( ; ; )
    {
        char line_buffer[256];
        if (fgets(line_buffer, sizeof(line_buffer), stdin))
        {
            ProcessInput(game, line_buffer);
        }
    }
}
