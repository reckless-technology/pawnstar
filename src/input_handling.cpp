#include <cstring>
#include <string>

#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"
#include "game.h"
#include "opening_book.h"

using std::string;

/**
 * @brief Handling for input commands (xboard support)
*/
typedef struct
{
    void (*function)(Game& game, int argc, char* argv[]);   /**< function to be called  */
    const char* name;                                       /**< command name           */
    const char* description;                                /**< command description    */
} InputHandler;

static void handle_quit(Game& game, int argc, char* argv[])
{
	game.StopThinking();
    exit(0);
    (void)argc;
    (void)argv;
}

static void handle_perft(Game& game, int argc, char* argv[])
{
    RunPerftTests();
    (void)argc;
    (void)argv;
    (void)game;
}

static void handle_postests(Game& game, int argc, char* argv[])
{
    int depth = 9;
    if (argc > 1)
    {
        int d = 0;
        if (sscanf(argv[1], "%d", &d) == 1)
        {
            depth = d;
        }
    }    
    RunPositionTests(depth);
    (void)game;
}

static void handle_ping(Game& game, int argc, char* argv[])
{
    printf("pong %s\n", argc > 1 ? argv[1] : "");
    (void)game;
}

static void handle_xboard(Game& game, int argc, char* argv[])
{
    printf("\n");
    (void)argc;
    (void)argv;
    (void)game;
}

static void handle_bookmoves(Game& game, int argc, char* argv[])
{
    DisplayAvailableBookMoves(*game.position_);
    (void)argc;
    (void)argv;
}

static void handle_freebook(Game& game, int argc, char* argv[])
{
    FreeOpeningBook();
    (void)argc;
    (void)argv;
    (void)game;
}

static void handle_protover(Game& game, int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("ERROR protocol not defined\n");
        return;
    }
    if (!strcmp(argv[1], "2"))
    {
        printf(
            "feature ping=1 setboard=1 playother=1 san=1 usermove=1 time=1 draw=0 "
            "sigint=0 sigterm=0 reuse=1 analyze=0 "
            "myname=\"Pawnstar " __DATE__ " " __TIME__ "\" variants=\"normal\" "
            "colors=0 ics=0 name=0 pause=0 nps=0 debug=0 memory=0 smp=0 done=1 \n");
    }
    else
    {
        printf("ERROR: unsupported XBoard protocol version %s\n", argv[1]);
    }
    (void)game;
}

static void handle_new(Game& game, int argc, char* argv[])
{
    game.StopThinking();
    game = Game();
    (void)argc;
    (void)argv;
}

static void handle_force(Game &game, int argc, char* argv[])
{
    game.engine_color_ = NEITHER_COLOR;
    (void)argc;
    (void)argv;
}

static void handle_go(Game& game, int argc, char* argv[])
{
    game.engine_color_ = game.position_->ColorToMove();
    game.StartThinking();
    (void)argc;
    (void)argv;
}

static void handle_playother(Game& game, int argc, char* argv[])
{
    game.engine_color_ = EnemyOf(game.position_->ColorToMove());
    (void)argc;
    (void)argv;
}

static void handle_usermove(Game& game, int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("ERROR: move not specified\n");
        return;
    }
    Move move = game.PlayMove(argv[1]);
    if (move == 0)
    {
        printf("Illegal move: %s\n", argv[1]);
    }
    if (!game.IsGameOver())
    {
        if (game.engine_color_ == game.position_->ColorToMove())
        {
            game.StartThinking();
        }
    }
}

static void handle_setboard(Game& game, int argc, char* argv[])
{
    char fen_string[256];
    char* p = fen_string;
    for (int i = 1; i < argc; ++i)
    {
        p += sprintf(p, "%s ", argv[i]);
    }
    *p = 0;
    game.position_ = game.stack_;
    *game.position_ = Position { fen_string };
}

static void handle_getboard(Game& game, int argc, char* argv[])
{
    std::string fen_string = game.position_->operator std::string();
    printf("%s\n", fen_string.c_str());
    (void)argc;
    (void)argv;
}

static void handle_nopost(Game& game, int argc, char* argv[])
{
    game.do_show_thinking_ = false;
    (void)argc;
    (void)argv;
}

static void handle_post(Game& game, int argc, char* argv[])
{
    game.do_show_thinking_ = true;
    (void)argc;
    (void)argv;
}

static void handle_time(Game& game, int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("ERROR: time not specified\n");
        return;
    }
    int time;
    if (sscanf(argv[1], "%d", &time) == 1)
    {
        game.time_control_.standard.milliseconds_remaining = time * 10;
    }
}

static void handle_cancel(Game& game, int argc, char* argv[])
{
    game.StopThinking();
    (void)argc;
    (void)argv;
}

static void handle_level(Game& game, int argc, char* argv[])
{
    if (argc != 4)
    {
        printf("ERROR: time control specification invalid\n");
        return;
    }
    int moves = 40, minutes = 5, seconds = 0, increment = 5;
    sscanf(argv[1], "%d", &moves);
    if (strchr(argv[2], ':'))
    {
        sscanf(argv[2], "%d:%d", &minutes, &seconds);
    }
    else
    {
        sscanf(argv[2], "%d", &minutes);
    }
    sscanf(argv[3], "%d", &increment);
    if (moves)
    {
        game.time_control_.clock_type = CLOCK_STANDARD;
        game.time_control_.standard.moves_per_period = moves;
        game.time_control_.standard.milliseconds_per_period = minutes * 60000 + seconds * 1000;
        game.time_control_.standard.milliseconds_remaining = game.time_control_.standard.milliseconds_per_period;
    }
    else
    {
        game.time_control_.clock_type = CLOCK_INCREMENTAL;
        game.time_control_.incremental.base_milliseconds = minutes * 60000 + seconds * 1000;
        game.time_control_.incremental.increment_milliseconds = increment * 1000;
        game.time_control_.incremental.milliseconds_remaining = game.time_control_.incremental.base_milliseconds;
    }
}

static void handle_st(Game& game, int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("ERROR: time not specified\n");
        return;
    }
    int seconds = 0;
    if (sscanf(argv[1], "%d", &seconds) == 1) 
    {
        game.time_control_.clock_type = CLOCK_FIXED_TIME;
        game.time_control_.fixed_time.milliseconds = seconds * 1000;
    }
}

static void handle_sd(Game& game, int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("ERROR: depth not specified\n");
        return;
    }
    int depth = 0;
    if (sscanf(argv[1], "%d", &depth) == 1)
    {
        game.time_control_.clock_type = CLOCK_FIXED_DEPTH;
        game.time_control_.fixed_depth.depth = depth;
    }
}

#define PRINT_MIN_SEC(milliseconds) printf("%02d:%02d\n", (milliseconds) / 60000, ((milliseconds) / 1000) % 60)

static void handle_showtime(Game& game, int argc, char* argv[])
{
    switch (game.time_control_.clock_type)
    {
    case CLOCK_STANDARD:
        printf("standard clock mode\n");
        printf("moves per period              %5d\n", game.time_control_.standard.moves_per_period);
        printf("time period                   ");
        PRINT_MIN_SEC(game.time_control_.standard.milliseconds_per_period);
        printf("time remaining                ");
        PRINT_MIN_SEC(game.time_control_.standard.milliseconds_remaining);
        break;
    case CLOCK_INCREMENTAL:
        printf("incremental clock mode\n");
        printf("base time                     ");
        PRINT_MIN_SEC(game.time_control_.incremental.base_milliseconds);
        printf("increment time                ");
        PRINT_MIN_SEC(game.time_control_.incremental.increment_milliseconds);
        printf("time remaining                ");
        PRINT_MIN_SEC(game.time_control_.incremental.milliseconds_remaining);
        break;
    case CLOCK_FIXED_DEPTH:
        printf("fixed depth mode\n");
        printf("search depth                  %5d\n", game.time_control_.fixed_depth.depth);
        break;
    case CLOCK_FIXED_TIME:
        printf("fixed time mode\n");
        printf("search time                   ");
        PRINT_MIN_SEC(game.time_control_.fixed_time.milliseconds);
        break;
    }
    (void)argc;
    (void)argv;
}

static void handle_eval(Game& game, int argc, char* argv[])
{
    printf("evaluation %5d\n", EvaluatePosition(*game.position_, ALPHA, BETA));
    (void)argc;
    (void)argv;
}

#if DEBUGX
static void handle_dbg(Game& game, int argc, char* argv[])
{
    DebugXWrite();
    (void)argc;
    (void)argv;
    (void)game;
}

static void handle_dbgclear(Game& game, int argc, char* argv[])
{
    DebugXClear();
    (void)argc;
    (void)argv;
    (void)game;
}
#endif

static void handle_undo(Game& game, int argc, char* argv[])
{
    if (game.position_ != game.stack_)
    {
        --game.position_;
    }
    (void)argc;
    (void)argv;
}

static void handle_remove(Game& game, int argc, char* argv[])
{
    if (game.position_ - game.stack_ >= 2)
    {
        game.position_ -= 2;
    }
    (void)argc;
    (void)argv;
}

static void handle_seetests(Game& game, int argc, char* argv[])
{
    RunStaticExchangeTests();
    (void)argc;
    (void)argv;
    (void)game;
}

static void handle_mergetest(Game& game, int argc, char* argv[])
{
    const bool is_pass = RunMergeSortTests();
    printf("Merge sort tests: %s\n", is_pass ? "PASS" : "FAIL");
    (void)argc;
    (void)argv;
    (void)game;
}

static void handle_help(Game& game, int argc, char* argv[]);

#define COMMAND(name) handle_ ## name, #name

const InputHandler handlers[] = {
    { COMMAND(bookmoves),   "display available book moves for current position"         },
#if DEBUGX
    { COMMAND(dbg),         "display diagnostic counts"                                 },
    { COMMAND(dbgclear),    "reset diagnostic counts"                                   },
#endif
    { COMMAND(eval),        "display the current static evaluation"                     },
    { COMMAND(force),       "assign pawnstar to play neither color"                     },
    { COMMAND(freebook),    "delete the opening book from memory"                       },
    { COMMAND(getboard),    "get the Forsyth Edwards string for the position"           },
    { COMMAND(go),          "assign pawnstar to play the color to move"                 },
    { COMMAND(help),        "display a summary of commands"                             },
    { COMMAND(level),       "set a chess clock: 'level moves min:sec increment'"        },
    { COMMAND(mergetest),   "run merge sort tests"                                      },
    { COMMAND(new),         "start a new game (pawnstar will play black)"               },
    { COMMAND(nopost),      "turns off analysis output while thinking"                  },
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

static void handle_help(Game& game, int argc, char* argv[])
{
    const InputHandler* i;
    printf("refer to 'engine_protocol.html' for details of the communication\n");
    printf("between an xboard protocol chess engine and a user interface\n\n");
    printf("available commands:\n");
    for (i = handlers; i->name; ++i)
    {
        printf("%-12s %s\n", i->name, i->description);
    }
    (void)argc;
    (void)argv;
    (void)game;
}

#define MAX_NUM_ARGS 8

void ProcessInput(Game& game, char* line)
{
    int argc                    = 0;
    char* argv[MAX_NUM_ARGS]    = { 0 };
    char* save_ptr              = NULL;
    char* newline               = strchr(line, '\n');
    if (newline)
    {
        *newline = '\0';
    }
    for (int i = 0; i != MAX_NUM_ARGS; ++i)
    {
        argv[i] = strtok_r(i == 0 ? line : NULL, " ", &save_ptr);
        if (!argv[i])
        {
            break;
        }
        ++argc;
    }
    if (argc == 0)
    {
        return;
    }
    for (const InputHandler* handler = handlers; handler->name; ++handler)
    {
        if (!strcmp(handler->name, argv[0]))
        {
            handler->function(game, argc, argv);
            break;
        }
    }
}