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
#include "search_state.h"
#include "transposition_table.h"

/// @brief Pointer to a command handler taking the game and the command's argument list.
using HandlerFn = void (*)(Game &game, std::span<std::string> args);

/// @brief Structure to hold a hander for an input command.
struct Handler
{
    HandlerFn        function;    ///< Function to be called
    std::string_view name;        ///< Command name
    std::string_view description; ///< Command description

    /// @brief Construct a command handler entry.
    /// @param fn Handler function. @param name Command name. @param desc Human-readable description.
    constexpr Handler(HandlerFn fn, std::string_view name, std::string_view desc)
        : function(fn), name(name), description(desc)
    {
    }
};

/// @brief Handle the "bookmoves" command: display book moves for the current position.
/// @param game Game to act on.
void handle_bookmoves(Game &game, std::span<std::string>)
{
    game.book.DisplayAvailableMoves(game.CurrentPosition());
}

/// @brief Handle the "freebook" command: release the opening book from memory.
/// @param game Game to act on.
void handle_freebook(Game &game, std::span<std::string>)
{
    game.book.Free();
}

/// @brief Handle the "eval" command: print the static evaluation of the current position.
/// @param game Game to act on.
void handle_eval(Game &game, std::span<std::string>)
{
    SearchState tmp{game};
    std::cout << std::format("evaluation {}\n", EvaluatePosition(tmp, kAlpha, kBeta));
}

/// @brief Handle the "dbg" command: print diagnostic counters.
void handle_dbg(Game &, std::span<std::string>)
{
    DebugXWrite();
}

/// @brief Handle the "dbgclear" command: reset diagnostic counters.
void handle_dbgclear(Game &, std::span<std::string>)
{
    DebugXClear();
}

/// @brief Handle the "getboard" command: print the FEN of the current position.
/// @param game Game to act on.
void handle_getboard(Game &game, std::span<std::string>)
{
    auto fen = game.CurrentPosition().ToString();
    std::cout << std::format("{}\n", fen);
}

/// @brief Handle the "uci" command: identify the engine and acknowledge UCI mode.
void handle_uci(Game &, std::span<std::string>)
{
    std::cout << "id name Pawnstar\n";
    std::cout << "id author Jonny Reckless\n";
    std::cout << "uciok\n";
}

/// @brief Handle the "ucinewgame" command: stop searching and reset to a new game.
/// @param game Game to act on.
void handle_ucinewgame(Game &game, std::span<std::string>)
{
    game.StopThinking();
    game.NewGame();
}

/// @brief Handle the "go" command: parse time controls and start searching.
/// @param game Game to act on.
/// @param args Command arguments (time controls, depth, etc.).
void handle_go(Game &game, std::span<std::string> args)
{
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        if ((args[i] == "wtime" && game.CurrentPosition().ColorToMove() == kWhite) ||
            (args[i] == "btime" && game.CurrentPosition().ColorToMove() == kBlack))
        {
            game.time_control.clock_type   = ChessClock::kStandard;
            game.time_control.ms_remaining = stoi(args[++i]);
            continue;
        }
        if (args[i] == "depth")
        {
            game.time_control.clock_type = ChessClock::kFixedDepth;
            game.time_control.depth      = stoi(args[++i]);
            continue;
        }
        if (args[i] == "movetime")
        {
            game.time_control.clock_type   = ChessClock::kFixedTime;
            game.time_control.ms_remaining = stoi(args[++i]);
            continue;
        }
        if (args[i] == "infinite")
        {
            game.time_control.clock_type   = ChessClock::kInfinite;
            game.time_control.ms_remaining = 0;
        }
        if (args[i] == "movestogo")
        {
            game.time_control.num_moves_remaining = stoi(args[++i]);
        }
    }
    game.StartThinking();
}

/// @brief Handle the "stop" command: stop searching and report the best move found.
/// @param game Game to act on.
void handle_stop(Game &game, std::span<std::string>)
{
    game.StopThinking();
}

/// @brief Handle the "quit" command: stop searching and exit the program.
/// @param game Game to act on.
void handle_quit(Game &game, std::span<std::string>)
{
    game.StopThinking();
    exit(0);
}

/// @brief Handle the "isready" command: respond with readyok.
void handle_isready(Game &, std::span<std::string>)
{
    std::cout << "readyok\n";
}

/// @brief Handle the "position" command: set up the position and apply the listed moves.
/// @param game Game to act on.
/// @param args Command arguments (startpos / fen and the move list).
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

/// @brief Handle the "help" command: list all available commands.
void handle_help(Game &, std::span<std::string>);

/// @brief Expand to a handler function pointer and its command name string.
#define COMMAND(name) handle_##name, #name

// clang-format off
/// @brief Table of all supported input commands and their handlers.
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
    Handler { COMMAND(position),       "Set the position and series of moves"},
    Handler { COMMAND(quit),           "Exit the program"},
    Handler { COMMAND(stop),           "Stop searching and return best move found"},
    Handler { COMMAND(uci),            "Enter UCI protocol"},
    Handler { COMMAND(ucinewgame),     "UCI mode start new game"}
};
// clang-format on

/// @brief Handle the "help" command: list all available commands.
void handle_help(Game &, std::span<std::string>)
{
    std::cout << "Available commands:\n";
    for (auto &i : handlers)
    {
        std::cout << std::format("{:<12} {}\n", i.name.data(), i.description.data());
    }
}

/// @brief Tokenize a line of input and dispatch it to the matching command handler.
/// @param game Game to act on.
/// @param line Input line.
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