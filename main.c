#include "pawnstar.h"

Context globals[1];

void InitializeGame(Game* game)
{
    game->position = game->stack;
    PositionFromString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", game->position);
}

static void InitializeGlobals(void)
{
    globals->time_control.clock_type                        = CLOCK_STANDARD;
    globals->time_control.standard.milliseconds_per_period  = 5 * 60 * 1000; // 5 minutes
    globals->time_control.standard.moves_per_period         = 40;            // 40 moves in 5 mins
    globals->time_control.standard.milliseconds_remaining   = 5 * 60 * 1000;
    globals->node_count                                     = 0;
    globals->engine_color                                   = NEITHER_COLOR;
    globals->do_show_thinking                               = true;
}

int main()
{    
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);
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
    InitializeGlobals();
    InitializeEval();
    InitializeTranspositionTable(HASHTABLE_MEGABYTES);
    InitializeGoodMoveCounts();
    if (!InitializeOpeningBookFromFile("pawnstar.book"))
    {
        printf("NOTE: using built in opening book\n");
        InitializeOpeningBookFromString(OPENING_BOOK_MOVES);
    }
    InitializeGame(globals->game);
    DEBUG_STATEMENT(DebugXClear());
    for ( ; ; )
    {
        char line_buffer[STRING_BUF_LEN];
        if (fgets(line_buffer, sizeof(line_buffer), stdin) == NULL)
        {
            continue;
        }
        ProcessInput(line_buffer);
    }
}
