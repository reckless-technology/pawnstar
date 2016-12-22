#include "pawnstar.h"

#pragma warning(push)
#pragma warning(disable:4100)

static void handle_quit(char buffer[])
{
	StopThinkingMoveImmediately();
	StopWorker();
    exit(0);
}

static void handle_perft(char buffer[])
{
    RunPerftTests();
}

static void handle_postests(char buffer[])
{
    int depth = 9;
    sscanf(buffer, "%u", &depth);
    RunPositionTests(depth);
}

static void handle_ping(char buffer[])
{
    printf("pong %s\n", buffer);
}

static void handle_xboard(char buffer[])
{
    setbuf(stdout, NULL);
    printf("\n");
}

static void handle_book(char buffer[])
{
    if (!strlen(buffer))
    {
        printf("ERROR: book name not specified\n");
        return;
    }
    InitializeOpeningBookFromFile(buffer);
}

static void handle_bookmoves(char buffer[])
{
    DisplayAvailableBookMoves(globals->game->position);
}

static void handle_freebook(char buffer[])
{
    FreeOpeningBook();
}

static void handle_protover(char buffer[])
{
    if (!strcmp(buffer, "2"))
    {
        printf(
            "feature ping=1 setboard=1 playother=1 san=1 usermove=1 time=1 draw=0 "
            "sigint=0 sigterm=0 reuse=1 analyze=0 myname=\"Pawnstar " __DATE__ " " __TIME__ "\" variants=\"normal\" "
            "colors=0 ics=0 name=0 pause=0 nps=0 debug=0 memory=0 smp=0 done=1 \n");
    }
    else
    {
        printf("ERROR: unsupported XBoard protocol version %s\n", buffer);
    }
}

static void handle_new(char buffer[])
{
    StopThinkingMoveImmediately();
    InitializeGame(globals->game);
    globals->engine_color = BLACK;
}

static void handle_force(char buffer[])
{
    globals->engine_color = NEITHER_COLOR;
}

static void handle_go(char buffer[])
{
    globals->engine_color = COLOR_TO_MOVE(globals->game->position);
    if (!(globals->game->position->state_flags & IS_GAME_OVER))
    {
        StartThinking(globals->game);
    }
}

static void handle_playother(char buffer[])
{
    globals->engine_color = ENEMY(COLOR_TO_MOVE(globals->game->position));
}

static void handle_usermove(char buffer[])
{
    int move = PlayMoveString(globals->game, buffer, true);
    if (!move)
    {
        printf("Illegal move: %s\n", buffer);
    }
    else
    {
        if (!(globals->game->position->state_flags & IS_GAME_OVER) && globals->engine_color == (int)COLOR_TO_MOVE(globals->game->position))
        {
            StartThinking(globals->game);
        }
        else
        {
            DisplayResultIfGameOver(globals->game->position);
        }
    }
}

static void handle_setboard(char buffer[])
{
    globals->game->position = globals->game->stack;
    if (!PositionFromString(buffer, globals->game->position))
    {
        InitializeGame(globals->game);
    }
}

static void handle_getboard(char buffer[])
{
    char fen_string[256];
    PositionToString(globals->game->position, fen_string);
    printf("%s\n", fen_string);
}

static void handle_nopost(char buffer[])
{
    globals->do_show_thinking = false;
}

static void handle_post(char buffer[])
{
    globals->do_show_thinking = true;
}

static void handle_time(char buffer[])
{
    globals->time_control.milliseconds_remaining = atoi(buffer) * 10;
}

static void handle_cancel(char buffer[])
{
    StopThinkingMoveImmediately();
}

static void handle_level(char buffer[])
{
    int moves = 40, minutes = 5, seconds = 0, increment = 5;
    if (strchr(buffer, ':'))
    {
        sscanf(buffer, "%d %d:%d %d", &moves, &minutes, &seconds, &increment);
    }
    else
    {
        sscanf(buffer, "%d %d %d", &moves, &minutes, &increment);
    }
    if (moves)
    {
        globals->time_control.clock_type = CLOCK_STANDARD;
        globals->time_control.moves_per_period = moves;
        globals->time_control.milliseconds_per_period = minutes * 60000 + seconds * 1000;
        globals->time_control.milliseconds_remaining = globals->time_control.milliseconds_per_period;
    }
    else
    {
        globals->time_control.clock_type = CLOCK_INCREMENTAL;
        globals->time_control.base_milliseconds = minutes * 60000 + seconds * 1000;
        globals->time_control.increment_milliseconds = increment * 1000;
        globals->time_control.milliseconds_remaining = globals->time_control.base_milliseconds;
    }
}

static void handle_st(char buffer[])
{
    globals->time_control.clock_type = CLOCK_FIXED_TIME;
    globals->time_control.fixed_milliseconds = atoi(buffer) * 1000;
}

static void handle_sd(char buffer[])
{
    globals->time_control.clock_type = CLOCK_FIXED_DEPTH;
    globals->time_control.fixed_depth = atoi(buffer);
}

#define PRINT_MIN_SEC(milliseconds) printf("%02d:%02d\n", (milliseconds) / 60000, ((milliseconds) / 1000) % 60)

static void handle_showtime(char buffer[])
{
    switch (globals->time_control.clock_type)
    {
    case CLOCK_STANDARD:
        printf("standard clock mode\n");
        printf("moves per period              %5d\n", globals->time_control.moves_per_period);
        printf("time period                   ");
        PRINT_MIN_SEC(globals->time_control.milliseconds_per_period);
        printf("time remaining                ");
        PRINT_MIN_SEC(globals->time_control.milliseconds_remaining);
        break;
    case CLOCK_INCREMENTAL:
        printf("incremental clock mode\n");
        printf("base time                     ");
        PRINT_MIN_SEC(globals->time_control.base_milliseconds);
        printf("increment time                ");
        PRINT_MIN_SEC(globals->time_control.increment_milliseconds);
        printf("time remaining                ");
        PRINT_MIN_SEC(globals->time_control.milliseconds_remaining);
        break;
    case CLOCK_FIXED_DEPTH:
        printf("fixed depth mode\n");
        printf("search depth                  %5d\n", globals->time_control.fixed_depth);
        break;
    case CLOCK_FIXED_TIME:
        printf("fixed time mode\n");
        printf("search time                   ");
        PRINT_MIN_SEC(globals->time_control.fixed_milliseconds);
        break;
    }
}

static void handle_eval(char buffer[])
{
    printf("evaluation %5d\n", EvaluatePosition(globals->game->position, ALPHA, BETA));
}

static void handle_pawntests(char buffer[])
{
    RunPawnStructureTests();
}

static void handle_pawnstruct(char buffer[])
{
    DisplayPawnStructure(globals->game->position);
}

#if DEBUGX
static void handle_dbg(char buffer[])
{
    DebugXWrite(stdout);
}

static void handle_dbgclear(char buffer[])
{
    DebugXClear();
}
#endif

static void handle_undo(char buffer[])
{
    if (globals->game->position != globals->game->stack)
    {
        --globals->game->position;
    }
}

static void handle_remove(char buffer[])
{
    if (globals->game->position - globals->game->stack >= 2)
    {
        globals->game->position -= 2;
    }
}

static void handle_seetests(char buffer[])
{
    RunStaticExchangeTests();
}

static void handle_help(char buffer[]);

#define COMMAND(name) handle_ ## name, #name

const CommandHandler handlers[] = {
    { COMMAND(book),        "initialize opening book from text file"                    },
    { COMMAND(bookmoves),   "display available book moves for current position"         },
#if DEBUGX
    { COMMAND(dbg),         "display diagnostic counts"                                 },
    { COMMAND(dbgclear)     "reset diagnostic counts"                                   },
#endif
    { COMMAND(eval),        "display the current static evaluation"                     },
    { COMMAND(force),       "assign pawnstar to play neither color"                     },
    { COMMAND(freebook),    "delete the opening book from memory"                       },
    { COMMAND(getboard),    "get the Forsyth Edwards string for the position"           },
    { COMMAND(go),          "assign pawnstar to play the color to move"                 },
    { COMMAND(help),        "display a summary of commands"                             },
    { COMMAND(level),       "set a chess clock: 'level moves min:sec increment'"        },
    { COMMAND(new),         "start a new game (pawnstar will play black)"               },
    { COMMAND(nopost),      "turns off analysis output while thinking"                  },
    { COMMAND(pawnstruct),  "display pawn structure for current position"               },
    { COMMAND(pawntests),   "perform a series of pawn structure tests"                  },
    { COMMAND(perft),       "run standard move generation tests"                        },
    { COMMAND(ping),        "responds with pong <n> (check worker still alive)"         },
    { COMMAND(playother),   "assign pawnstar to play the color not to move"             },
    { COMMAND(post),        "turns on analysis output while thinking"                   },
    { COMMAND(postests),    "search the Bratko Kopec test positions"                    },
    { COMMAND(protover),    "specify xboard protocol revision (currently 2)"            },
    { COMMAND(quit),        "exit the program"                                          },
    { COMMAND(remove),      "undo the last move made by each side"                      },
    { COMMAND(sd),          "set a fixed search depth regardless of time spent"         },
    { COMMAND(seetests),    "perform very simple static exchange tests"                 },
    { COMMAND(setboard),    "set the current position to a Forsyth Edwards string"      },
    { COMMAND(showtime),    "show the current time controls"                            },
    { COMMAND(st),          "set a fixed search time per move in seconds"               },
    { COMMAND(time),        "set the time on pawnstar's clock (centiseconds)"           },
    { COMMAND(undo),        "undo the last (half) move made"                            },
    { COMMAND(usermove),    "enter the user move in algebraic xboard format"            },
    { COMMAND(xboard),      "enter xboard protocol"                                     },
    { handle_cancel, "?",   "if thinking, stop now and move immediately"                },
    { NULL, NULL,           NULL                                                        },
};

static void handle_help(char buffer[])
{
    const CommandHandler* i;
    printf("refer to 'engine_protocol.html' for details of the communication\n");
    printf("between an xboard protocol chess engine and a user interface\n\n");
    printf("available commands:\n");
    for (i = handlers; i->name; ++i)
    {
        printf("%-12s %s\n", i->name, i->description);
    }
}
#pragma warning(pop)