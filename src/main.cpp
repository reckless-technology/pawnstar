#include "pawnstar.h"

Game the_game;

void InitializeGame(Game& game)
{
    game.time_control.hard_stop_search_ms              = 0;
    game.time_control.clock_type                       = CLOCK_STANDARD;
    game.time_control.standard.milliseconds_per_period = 5 * 60 * 1000; // 5 minutes
    game.time_control.standard.moves_per_period        = 40;            // 40 moves in 5 mins
    game.time_control.standard.milliseconds_remaining  = 5 * 60 * 1000;
    game.node_count                                    = 0;
    game.engine_color                                  = NEITHER_COLOR;
    game.do_show_thinking                              = true;
    game.position                                      = &game.stack[0];
    *game.position                                     = Position {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
}

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
    InitializeGame(the_game);
    DEBUG_STATEMENT(DebugXClear());
    for ( ; ; )
    {
        char line_buffer[256];
        gets_s(line_buffer, sizeof(line_buffer));
        ProcessInput(line_buffer);
    }
}
