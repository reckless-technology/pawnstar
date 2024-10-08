#include <cstring>
#include <iterator>
#include <ranges>
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

/// @brief Structure to hold a hander for an input command.
typedef struct
{
    void (*function)(Game &game, std::span<std::string> args); ///< Function to be called
    std::string name;                                          ///< Command name
    std::string description;                                   ///< Command description
} InputHandler;

static void handle_perftx(Game &, std::span<std::string>)
{
    RunPerftTestsExtra();
}

static void handle_perft(Game &, std::span<std::string>)
{
    RunPerftTests();
}

static void handle_postests(Game &, std::span<std::string> args)
{
    int depth = 9;
    if (args.size() > 1)
    {
        const int d = stoi(args[1]);
        depth       = d;
    }
    RunPositionTests(depth);
}

static void handle_bookmoves(Game &game, std::span<std::string>)
{
    DisplayAvailableBookMoves(game.CurrentPosition());
}

static void handle_freebook(Game &, std::span<std::string>)
{
    FreeOpeningBook();
}

static void handle_eval(Game &game, std::span<std::string>)
{
    printf("evaluation %5d\n", EvaluatePosition(game.CurrentPosition(), ALPHA, BETA));
}

static void handle_dbg(Game &, std::span<std::string>)
{
    DebugXWrite();
}

static void handle_dbgclear(Game &, std::span<std::string>)
{
    DebugXClear();
}

static void handle_seetests(Game &, std::span<std::string>)
{
    RunStaticExchangeTests();
}

static void handle_getboard(Game &game, std::span<std::string>)
{
    auto fen = game.CurrentPosition().ToString();
    printf("%s\n", fen.c_str());
}

static void handle_uci(Game &, std::span<std::string>)
{
    printf("id name Pawnstar\n");
    printf("id author Jonny Reckless\n");
    printf("uciok\n");
}

static void handle_ucinewgame(Game &game, std::span<std::string>)
{
    game.StopThinking();
    game.NewGame();
}

static void handle_go(Game &game, std::span<std::string> args)
{
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        if ((args[i] == "wtime" && game.CurrentPosition().ColorToMove() == WHITE) ||
            (args[i] == "btime" && game.CurrentPosition().ColorToMove() == BLACK))
        {
            game.time_control_.clock_type   = CHESS_CLOCK_STANDARD;
            game.time_control_.ms_remaining = stoi(args[++i]);
            continue;
        }
        if (args[i] == "depth")
        {
            game.time_control_.clock_type = CHESS_CLOCK_FIXED_DEPTH;
            game.time_control_.depth      = stoi(args[++i]);
            continue;
        }
        if (args[i] == "movetime")
        {
            game.time_control_.clock_type   = CHESS_CLOCK_FIXED_TIME;
            game.time_control_.ms_remaining = stoi(args[++i]);
            continue;
        }
        if (args[i] == "infinite")
        {
            game.time_control_.clock_type   = CHESS_CLOCK_INFINITE;
            game.time_control_.ms_remaining = 0;
        }
        if (args[i] == "movestogo")
        {
            game.time_control_.num_moves_remaining = stoi(args[++i]);
        }
    }
    game.StartThinking();
}

static void handle_stop(Game &game, std::span<std::string>)
{
    game.StopThinking();
}

static void handle_quit(Game &game, std::span<std::string>)
{
    game.StopThinking();
    exit(0);
}

static void handle_isready(Game &, std::span<std::string>)
{
    printf("readyok\n");
}

static void handle_position(Game &game, std::span<std::string> args)
{
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "startpos")
        {
            game.NewGame();
            continue;
        }
        if (args[i] == "fen")
        {
            game.NewGame(args[++i]);
            continue;
        }
        if (args[i] == "moves")
        {
            continue;
        }
        game.PlayMove(args[i]);
    }
}

static void handle_help(Game &, std::span<std::string>);

#define COMMAND(name) handle_##name, #name

// clang-format off
const InputHandler handlers[] = 
{
    {COMMAND(bookmoves),    "display available book moves for current position"},
    {COMMAND(dbg),          "display diagnostic counts"},
    {COMMAND(dbgclear),     "reset diagnostic counts"},
    {COMMAND(eval),         "display the current static evaluation"},
    {COMMAND(freebook),     "delete the opening book from memory"},
    {COMMAND(getboard),     "display the FEN of the current position"},
    {COMMAND(go),           "search the current position"},
    {COMMAND(help),         "display a summary of commands"},
    {COMMAND(isready),      "respond with readyok"},
    {COMMAND(perft),        "run basic move generation tests"},
    {COMMAND(perftx),       "run extended move generation tests"},
    {COMMAND(position),     "set the position and series of moves"},
    {COMMAND(postests),     "search the Bratko Kopec test positions"},
    {COMMAND(quit),         "exit the program"},
    {COMMAND(seetests),     "perform very simple static exchange tests"},
    {COMMAND(stop),         "stop searching and return best move found"},
    {COMMAND(uci),          "enter UCI protocol"},
    {COMMAND(ucinewgame),   "UCI mode start new game"}
};
// clang-format on

static void handle_help(Game &, std::span<std::string>)
{
    printf("Refer to 'engine_protocol.html' for details of the communication\n");
    printf("between an xboard protocol chess engine and a user interface\n\n");
    printf("available commands:\n");
    for (auto &i : handlers)
    {
        printf("%-12s %s\n", i.name.c_str(), i.description.c_str());
    }
}

void ProcessInput(Game &game, std::string_view line)
{
    std::stringstream ss;
    ss << line;
    std::vector<std::string> args(std::istream_iterator<std::string>(ss), {});
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