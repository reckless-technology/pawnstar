#include <cstring>
#include <format>
#include <iostream>
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

typedef void (*HandlerFn)(Game &game, std::span<std::string> args);

/// @brief Structure to hold a hander for an input command.
struct Handler
{
    HandlerFn        function;    ///< Function to be called
    std::string_view name;        ///< Command name
    std::string_view description; ///< Command description

    constexpr Handler(HandlerFn fn, std::string_view name, std::string_view desc)
        : function(fn), name(name), description(desc)
    {
    }
};

void handle_perft(Game &, std::span<std::string>)
{
    RunPerftTests();
}

void handle_postests(Game &, std::span<std::string> args)
{
    int depth = 9;
    if (args.size() > 1)
    {
        const int d = stoi(args[1]);
        depth       = d;
    }
    RunPositionTests(depth);
}

void handle_bookmoves(Game &game, std::span<std::string>)
{
    DisplayAvailableBookMoves(game.CurrentPosition());
}

void handle_freebook(Game &, std::span<std::string>)
{
    FreeOpeningBook();
}

void handle_eval(Game &game, std::span<std::string>)
{
    std::cout << std::format("evaluation {}\n", EvaluatePosition(game, ALPHA, BETA));
}

void handle_dbg(Game &, std::span<std::string>)
{
    DebugXWrite();
}

void handle_dbgclear(Game &, std::span<std::string>)
{
    DebugXClear();
}

void handle_seetests(Game &, std::span<std::string>)
{
    RunStaticExchangeTests();
}

void handle_getboard(Game &game, std::span<std::string>)
{
    auto fen = game.CurrentPosition().ToString();
    std::cout << std::format("{}\n", fen);
}

void handle_uci(Game &, std::span<std::string>)
{
    std::cout << "id name Pawnstar\n";
    std::cout << "id author Jonny Reckless\n";
    std::cout << "uciok\n";
}

void handle_ucinewgame(Game &game, std::span<std::string>)
{
    game.StopThinking();
    game.NewGame();
}

void handle_go(Game &game, std::span<std::string> args)
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

void handle_stop(Game &game, std::span<std::string>)
{
    game.StopThinking();
}

void handle_quit(Game &game, std::span<std::string>)
{
    game.StopThinking();
    exit(0);
}

void handle_isready(Game &, std::span<std::string>)
{
    std::cout << "readyok\n";
}

void handle_position(Game &game, std::span<std::string> args)
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

void handle_help(Game &, std::span<std::string>);

#define COMMAND(name) handle_##name, #name

// clang-format off
constexpr std::array handlers = 
{
    Handler { COMMAND(bookmoves),      "Display available book moves for current position"},
    Handler { COMMAND(dbg),            "Display diagnostic counts"},
    Handler { COMMAND(dbgclear),       "Reset diagnostic counts"},
    Handler { COMMAND(eval),           "Display the current static evaluation"},
    Handler { COMMAND(freebook),       "Delete the opening book from memory"},
    Handler { COMMAND(getboard),       "Display the FEN of the current position"},
    Handler { COMMAND(go),             "Search the current position"},
    Handler { COMMAND(help),           "Display a summary of commands"},
    Handler { COMMAND(isready),        "Respond with readyok"},
    Handler { COMMAND(perft),          "Run basic move generation tests"},
    Handler { COMMAND(position),       "Set the position and series of moves"},
    Handler { COMMAND(postests),       "Search the Bratko Kopec test positions"},
    Handler { COMMAND(quit),           "Exit the program"},
    Handler { COMMAND(seetests),       "Perform very simple static exchange tests"},
    Handler { COMMAND(stop),           "Stop searching and return best move found"},
    Handler { COMMAND(uci),            "Enter UCI protocol"},
    Handler { COMMAND(ucinewgame),     "UCI mode start new game"}
};
// clang-format on

void handle_help(Game &, std::span<std::string>)
{
    std::cout << "Available commands:\n";
    for (auto &i : handlers)
    {
        std::cout << std::format("{:<12} {}\n", i.name.data(), i.description.data());
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
    for (const auto &handler : handlers)
    {
        if (args[0] == handler.name)
        {
            handler.function(game, args);
            break;
        }
    }
}