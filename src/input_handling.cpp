#include <cstring>
#include <iterator>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "opening_book.h"
#include "position.h"
#include "tests.h"
#include "transposition_table.h"

using std::span;
using std::string;
using std::string_view;
using std::stringstream;
using std::vector;

/// @brief Structure to hold a hander for an input command.
typedef struct
{
    void (*function)(Game &game, span<string> args); ///< Function to be called
    string name;                                     ///< Command name
    string description;                              ///< Command description
} InputHandler;

static void handle_quit(Game &game, span<string>)
{
    game.StopThinking();
    exit(0);
}

static void handle_perftx(Game &, span<string>)
{
    RunPerftTestsExtra();
}

static void handle_perft(Game &, span<string>)
{
    RunPerftTests();
}

static void handle_postests(Game &, span<string> args)
{
    int depth = 9;
    if (args.size() > 1)
    {
        const int d = stoi(args[1]);
        depth       = d;
    }
    RunPositionTests(depth);
}

static void handle_ping(Game &, span<string> args)
{
    if (args.size() > 1)
    {
        printf("pong %s\n", args[1].c_str());
    }
}

static void handle_xboard(Game &, span<string>)
{
    printf("\n");
}

static void handle_bookmoves(Game &game, span<string>)
{
    DisplayAvailableBookMoves(game.CurrentPosition());
}

static void handle_freebook(Game &, span<string>)
{
    FreeOpeningBook();
}

static void handle_protover(Game &, span<string> args)
{
    if (args.size() != 2)
    {
        printf("ERROR protocol not defined\n");
        return;
    }
    if (args[1] == "2")
    {
        printf("feature ping=1 setboard=1 playother=1 san=1 usermove=1 time=1 draw=0 "
               "sigint=0 sigterm=0 reuse=1 analyze=0 "
               "myname=\"Pawnstar " __DATE__ " " __TIME__ "\" variants=\"normal\" "
               "colors=0 ics=0 name=0 pause=0 nps=0 debug=0 memory=0 smp=0 done=1 \n");
    }
    else
    {
        printf("ERROR: unsupported XBoard protocol version %s\n", args[1].c_str());
    }
}

static void handle_new(Game &game, span<string>)
{
    game.StopThinking();
    game = Game();
}

static void handle_force(Game &game, span<string>)
{
    game.engine_color_ = NEITHER_COLOR;
}

static void handle_go(Game &game, span<string>)
{
    game.engine_color_ = game.CurrentPosition().ColorToMove();
    game.StartThinking();
}

static void handle_playother(Game &game, span<string>)
{
    game.engine_color_ = EnemyOf(game.CurrentPosition().ColorToMove());
}

static void handle_usermove(Game &game, span<string> args)
{
    if (args.size() != 2)
    {
        printf("ERROR: move not specified\n");
        return;
    }
    Move move = game.PlayMove(args[1]);
    if (!move)
    {
        printf("Illegal move: %s\n", args[1].c_str());
    }
    if (!game.IsGameOver())
    {
        if (game.engine_color_ == game.CurrentPosition().ColorToMove())
        {
            game.StartThinking();
        }
    }
}

static void handle_setboard(Game &game, span<string> args)
{
    stringstream ss;
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        ss << args[i] << ' ';
    }
    game = Game(ss.str());
}

static void handle_getboard(Game &game, span<string>)
{
    std::string fen_string = game.CurrentPosition().ToString();
    printf("%s\n", fen_string.c_str());
}

static void handle_nopost(Game &game, span<string>)
{
    game.do_show_thinking_ = false;
}

static void handle_post(Game &game, span<string>)
{
    game.do_show_thinking_ = true;
}

static void handle_time(Game &game, span<string> args)
{
    if (args.size() != 2)
    {
        printf("ERROR: time not specified\n");
        return;
    }
    int centiseconds = stoi(args[1]);
    if (centiseconds != 0)
    {
        game.time_control_.standard.milliseconds_remaining = centiseconds * 10;
    }
}

static void handle_cancel(Game &game, span<string>)
{
    game.StopThinking();
}

static void handle_level(Game &game, span<string> args)
{
    if (args.size() != 4)
    {
        printf("ERROR: time control specification invalid\n");
        return;
    }
    int moves = 40, minutes = 5, seconds = 0, increment = 5;
    sscanf(args[1].c_str(), "%d", &moves);
    if (strchr(args[2].c_str(), ':'))
    {
        sscanf(args[2].c_str(), "%d:%d", &minutes, &seconds);
    }
    else
    {
        sscanf(args[2].c_str(), "%d", &minutes);
    }
    sscanf(args[3].c_str(), "%d", &increment);
    if (moves)
    {
        game.time_control_.clock_type                       = CLOCK_STANDARD;
        game.time_control_.standard.moves_per_period        = moves;
        game.time_control_.standard.milliseconds_per_period = minutes * 60000 + seconds * 1000;
        game.time_control_.standard.milliseconds_remaining  = game.time_control_.standard.milliseconds_per_period;
    }
    else
    {
        game.time_control_.clock_type                         = CLOCK_INCREMENTAL;
        game.time_control_.incremental.base_milliseconds      = minutes * 60000 + seconds * 1000;
        game.time_control_.incremental.increment_milliseconds = increment * 1000;
        game.time_control_.incremental.milliseconds_remaining = game.time_control_.incremental.base_milliseconds;
    }
}

static void handle_st(Game &game, span<string> args)
{
    if (args.size() != 2)
    {
        printf("ERROR: time not specified\n");
        return;
    }
    int seconds = 0;
    if (sscanf(args[1].c_str(), "%d", &seconds) == 1)
    {
        game.time_control_.clock_type              = CLOCK_FIXED_TIME;
        game.time_control_.fixed_time.milliseconds = seconds * 1000;
    }
}

static void handle_sd(Game &game, span<string> args)
{
    if (args.size() != 2)
    {
        printf("ERROR: depth not specified\n");
        return;
    }
    int depth = 0;
    if (sscanf(args[1].c_str(), "%d", &depth) == 1)
    {
        game.time_control_.clock_type        = CLOCK_FIXED_DEPTH;
        game.time_control_.fixed_depth.depth = depth;
    }
}

#define PRINT_MIN_SEC(milliseconds) printf("%02d:%02d\n", (milliseconds) / 60000, ((milliseconds) / 1000) % 60)

static void handle_showtime(Game &game, span<string>)
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
}

static void handle_eval(Game &game, span<string>)
{
    printf("evaluation %5d\n", EvaluatePosition(game.CurrentPosition(), ALPHA, BETA));
}

static void handle_dbg(Game &, span<string>)
{
    DebugXWrite();
}

static void handle_dbgclear(Game &, span<string>)
{
    DebugXClear();
}

static void handle_undo(Game &game, span<string>)
{
    game.UndoMove();
}

static void handle_remove(Game &game, span<string>)
{
    game.UndoMove();
    game.UndoMove();
}

static void handle_seetests(Game &, span<string>)
{
    RunStaticExchangeTests();
}

static void handle_help(Game &, span<string>);

#define COMMAND(name) handle_##name, #name

// clang-format off
const InputHandler handlers[] = 
{
    {COMMAND(bookmoves),    "display available book moves for current position"},
    {COMMAND(dbg),          "display diagnostic counts"},
    {COMMAND(dbgclear),     "reset diagnostic counts"},
    {COMMAND(eval),         "display the current static evaluation"},
    {COMMAND(force),        "assign pawnstar to play neither color"},
    {COMMAND(freebook),     "delete the opening book from memory"},
    {COMMAND(getboard),     "get the Forsyth Edwards string for the position"},
    {COMMAND(go),           "assign pawnstar to play the color to move"},
    {COMMAND(help),         "display a summary of commands"},
    {COMMAND(level),        "set a chess clock: 'level moves min:sec increment'"},
    {COMMAND(new),          "start a new game (pawnstar will play black)"},
    {COMMAND(nopost),       "turns off analysis output while thinking"},
    {COMMAND(perft),        "run basic move generation tests"},
    {COMMAND(perftx),       "run extended move generation tests"},
    {COMMAND(ping),         "responds with pong <n> (check worker still alive)"},
    {COMMAND(playother),    "assign pawnstar to play the color not to move"},
    {COMMAND(post),         "turns on analysis output while thinking"},
    {COMMAND(postests),     "search the Bratko Kopec test positions"},
    {COMMAND(protover),     "specify xboard protocol revision (currently 2)"},
    {COMMAND(quit),         "exit the program"},
    {COMMAND(remove),       "undo the last move made by each side"},
    {COMMAND(sd),           "set a fixed search depth regardless of time spent"},
    {COMMAND(seetests),     "perform very simple static exchange tests"},
    {COMMAND(setboard),     "set the current position to a Forsyth Edwards string"},
    {COMMAND(showtime),     "show the current time controls"},
    {COMMAND(st),           "set a fixed search time per move in seconds"},
    {COMMAND(time),         "set the time on pawnstar's clock (centiseconds)"},
    {COMMAND(undo),         "undo the last (half) move made"},
    {COMMAND(usermove),     "enter the user move in algebraic xboard format"},
    {COMMAND(xboard),       "enter xboard protocol"},
    {handle_cancel, "?",    "if thinking, stop now and move immediately"},
};
// clang-format on

static void handle_help(Game &, span<string>)
{
    printf("Refer to 'engine_protocol.html' for details of the communication\n");
    printf("between an xboard protocol chess engine and a user interface\n\n");
    printf("available commands:\n");
    for (auto &i : handlers)
    {
        printf("%-12s %s\n", i.name.c_str(), i.description.c_str());
    }
}

void ProcessInput(Game &game, string_view line)
{
    stringstream ss;
    ss << line;
    vector<string> args(std::istream_iterator<string>(ss), {});
    if (args.size() == 0)
    {
        return;
    }
    for (auto &handler : handlers)
    {
        if (args[0] == handler.name)
        {
            handler.function(game, args);
            break;
        }
    }
}