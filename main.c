#include "pawnstar.h"

Context globals[1];

static void InitializeGlobals(void)
{
    globals->time_control.clock_type                = CLOCK_STANDARD;
    globals->time_control.base_milliseconds         = 300000;
    globals->time_control.fixed_depth               = 7;
    globals->time_control.fixed_milliseconds        = 5000;
    globals->time_control.increment_milliseconds    = 5000;
    globals->time_control.milliseconds_per_period   = 300000;
    globals->time_control.milliseconds_remaining    = 300000;
    globals->time_control.moves_per_period          = 40;
    globals->node_count                             = 0;
    globals->engine_color                           = NEITHER_COLOR;
    globals->do_show_thinking                       = true;
}

extern const CommandHandler handlers[];

int main()
{       
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
        "(C) Jonny Reckless 2009 - 2016                         \n"
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
        char nullchar[1];
        char line_buffer[STRING_BUF_LEN];
        char* command;
        char* argument;
        char* nl;
        const CommandHandler* command_handler;
        if (fgets(line_buffer, sizeof(line_buffer), stdin) == NULL)
        {
            continue;
        }
        nl = strchr(line_buffer, '\n');
        if (nl)
        {
            *nl = '\0';
        }
        command = strtok(line_buffer, " ");
        if (!command)
        {
            continue;
        }
        argument = strtok(NULL, "");
        if (!argument)
        {
            nullchar[0] = '\0';
            argument = nullchar;
        }
        for (command_handler = handlers; command_handler->name != NULL; ++command_handler)
        {
            if (!strcmp(command_handler->name, command))
            {
                command_handler->function(argument);
                break;
            }
        }
    }
}
