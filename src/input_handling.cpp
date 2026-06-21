#include <algorithm>
#include <cstdlib>
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
#include "nnue.h"
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
    if (!game.NnueNetwork().IsLoaded())
    {
        std::cout << "info string no NNUE net loaded (use setoption name EvalFile value <path>)\n";
        return;
    }
    SearchState tmp{game};
    std::cout << std::format("evaluation {} (nnue)\n", EvaluatePosition(tmp));
}

/// @brief Handle the "nnue" command: print the raw NNUE evaluation of the current position (diagnostic).
/// @param game Game to act on.
void handle_nnue(Game &game, std::span<std::string>)
{
    if (!game.NnueNetwork().IsLoaded())
    {
        std::cout << "info string NNUE: no net loaded (use setoption name EvalFile value <path>)\n";
        return;
    }
    std::cout << std::format("nnue {}\n", game.NnueNetwork().Evaluate(game.CurrentPosition()));
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
void handle_uci(Game &game, std::span<std::string>)
{
    std::cout << "id name Pawnstar\n";
    std::cout << "id author Jonny Reckless\n";
    std::cout << "option name Hash type spin default " << kHashtableMegabytes << " min 1 max 4096\n";
    std::cout << "option name Threads type spin default " << game.thread_count << " min 1 max " << kMaxSearchThreads
              << "\n";
    std::cout << "option name Ponder type check default false\n";
    std::cout << "option name EvalFile type string default <empty>\n";
    std::cout << "option name Clear Hash type button\n";
    std::cout << "option name Move Overhead type spin default " << game.move_overhead.count() << " min 0 max 5000\n";
    std::cout << "uciok\n";
}

/// @brief Handle the "setoption" command: "setoption name <Name> value <Value>".
/// Recognises EvalFile (load a different NNUE net file; NNUE is the only evaluator), Hash (transposition
/// table size in MB, 1..4096), Threads (Lazy SMP search threads, 1..kMaxSearchThreads), Clear Hash (a
/// button that empties the transposition table) and Move Overhead (ms reserved from each deadline, 0..5000).
/// @param args Command arguments.
void handle_setoption(Game &game, std::span<std::string> args)
{
    // Parse the UCI "name <words...> value <words...>" grammar.
    std::string  name, value;
    std::string *target = nullptr;
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "name")
        {
            target = &name;
        }
        else if (args[i] == "value")
        {
            target = &value;
        }
        else if (target)
        {
            if (!target->empty())
            {
                *target += ' ';
            }
            *target += args[i];
        }
    }
    if (name == "EvalFile")
    {
        game.NnueNetwork().Load(value);
        game.eval_cache.Clear(); // cached evals belong to the previous net
    }
    else if (name == "Hash")
    {
        const int mb = std::clamp(std::atoi(value.c_str()), 1, 4096);
        game.transposition_table.Resize(static_cast<std::size_t>(mb));
    }
    else if (name == "Threads")
    {
        game.thread_count = std::clamp(std::atoi(value.c_str()), 1, kMaxSearchThreads);
    }
    else if (name == "Clear Hash") // a button: no value, just empty the transposition table
    {
        game.transposition_table.Clear();
    }
    else if (name == "Move Overhead")
    {
        game.move_overhead = ChessClock::Duration{std::clamp(std::atoi(value.c_str()), 0, 5000)};
    }
}

/// @brief Handle the "ucinewgame" command: stop searching and reset to a new game.
/// @param game Game to act on.
void handle_ucinewgame(Game &game, std::span<std::string>)
{
    game.StopThinking();
    game.NewGame();
}

/// @brief Handle the "go" command: parse time controls and start searching.
/// Parsed as a small state machine alternating between a keyword and its integer argument, rather than
/// scanning with `args[++i]` look-ahead (which also read out of bounds when a value keyword ended the
/// input). Recognised keywords: the flags `infinite`/`ponder`, and the value-taking `wtime`/`btime`/
/// `winc`/`binc`/`movetime`/`depth`/`movestogo`; any other token (e.g. `nodes`, `searchmoves` and its
/// moves) is ignored.
/// @param game Game to act on.
/// @param args Command arguments (time controls, depth, etc.).
void handle_go(Game &game, std::span<std::string> args)
{
    /// @brief Parser state: whether the next token is a keyword or the pending keyword's integer value.
    enum class State
    {
        kAwaitKeyword, ///< Expecting a keyword (`infinite`, `wtime`, `depth`, …).
        kAwaitValue,   ///< Expecting the integer argument for @c pending.
    };

    State            state = State::kAwaitKeyword;
    std::string_view pending; // the value-taking keyword whose argument is expected next

    game.is_pondering.store(false, std::memory_order_relaxed);  // set true below if this is a `go ponder`
    game.time_control.increment = ChessClock::Duration::zero(); // cleared up front; set below if winc/binc present

    for (std::size_t i = 1; i < args.size(); ++i)
    {
        const std::string &token = args[i];
        if (state == State::kAwaitKeyword)
        {
            if (token == "ponder") // flag: search on the opponent's time until ponderhit / stop
            {
                game.is_pondering.store(true, std::memory_order_relaxed);
            }
            else if (token == "infinite") // a flag, no argument
            {
                game.time_control.clock_type = ChessClock::kInfinite; // no time limit; remaining_time is unused
            }
            else if (token == "wtime" || token == "btime" || token == "winc" || token == "binc" ||
                     token == "movetime" || token == "depth" || token == "movestogo")
            {
                pending = token; // a value-taking keyword: its argument is the next token
                state   = State::kAwaitValue;
            }
            // else: ignore unrecognised tokens (nodes, searchmoves and its move list, …)
        }
        else // kAwaitValue: token is the integer argument for `pending`
        {
            const int value = stoi(token);
            if ((pending == "wtime" && game.CurrentPosition().color_to_move == kWhite) ||
                (pending == "btime" && game.CurrentPosition().color_to_move == kBlack))
            {
                game.time_control.clock_type     = ChessClock::kStandard;
                game.time_control.remaining_time = ChessClock::Duration{value};
            }
            else if ((pending == "winc" && game.CurrentPosition().color_to_move == kWhite) ||
                     (pending == "binc" && game.CurrentPosition().color_to_move == kBlack))
            {
                game.time_control.increment = ChessClock::Duration{value};
            }
            else if (pending == "movetime")
            {
                game.time_control.clock_type     = ChessClock::kFixedTime;
                game.time_control.remaining_time = ChessClock::Duration{value};
            }
            else if (pending == "depth")
            {
                game.time_control.clock_type = ChessClock::kFixedDepth;
                game.time_control.depth      = value;
            }
            else if (pending == "movestogo")
            {
                game.time_control.moves_to_go = value;
            }
            // (wtime/btime for the side *not* to move falls through here and is intentionally ignored.)
            state = State::kAwaitKeyword;
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

/// @brief Handle the "ponderhit" command: the predicted move was played, so convert the running ponder
/// search into a normally-timed search, keeping all work done so far (warm TT, current iteration depth).
/// `OnPonderhit` starts the budget from this moment (budget-from-ponderhit) by resetting the soft-time
/// origin and arming the hard deadline; is_pondering is cleared last so the search never sees the new flag
/// with a stale start time.
/// @param game Game to act on.
void handle_ponderhit(Game &game, std::span<std::string>)
{
    game.time_control.OnPonderhit();
    game.is_pondering.store(false, std::memory_order_relaxed);
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
/// Parses the UCI grammar `position [startpos | fen <6 fields>] [moves <m1> <m2> ...]` as a small state
/// machine over the tokens, rather than scanning with look-ahead and index mutation.
/// @param game Game to act on.
/// @param args Command arguments (startpos / fen and the move list).
void handle_position(Game &game, std::span<std::string> args)
{
    /// @brief Parser state: what the next token is expected to be.
    enum class State
    {
        kAwaitSource, ///< Expecting `startpos` or `fen` (the position source).
        kCollectFen,  ///< Accumulating the six space-separated FEN fields.
        kPlayMoves,   ///< Source is set; the `moves` keyword and the moves themselves follow.
    };

    State       state = State::kAwaitSource;
    std::string fen;
    int         fen_fields = 0;

    for (std::size_t i = 1; i < args.size(); ++i)
    {
        const std::string &token = args[i];
        switch (state)
        {
        case State::kAwaitSource:
            if (token == "startpos")
            {
                game.NewGame();
                state = State::kPlayMoves;
            }
            else if (token == "fen")
            {
                state = State::kCollectFen;
            }
            else if (token == "moves")
            {
                state = State::kPlayMoves; // tolerate a missing source: apply moves to the current position
            }
            break;

        case State::kCollectFen:
            if (token == "moves")
            {
                game.NewGame(fen); // FEN ended early (fewer than six fields is tolerated)
                state = State::kPlayMoves;
            }
            else
            {
                if (!fen.empty())
                {
                    fen += ' ';
                }
                fen += token;
                if (++fen_fields == 6) // a complete FEN; anything after must be the move list
                {
                    game.NewGame(fen);
                    state = State::kPlayMoves;
                }
            }
            break;

        case State::kPlayMoves:
            if (token != "moves") // skip the `moves` keyword itself; everything else is a move
            {
                game.PlayMove(token);
            }
            break;
        }
    }
    // A FEN that ran to the end of input without a `moves` keyword or a sixth field is still applied.
    if (state == State::kCollectFen && !fen.empty())
    {
        game.NewGame(fen);
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
    Handler { COMMAND(nnue),           "Display the raw NNUE evaluation of the current position"},
    Handler { COMMAND(ponderhit),      "Pondered move was played; convert ponder search to a timed search"},
    Handler { COMMAND(position),       "Set the position and series of moves"},
    Handler { COMMAND(quit),           "Exit the program"},
    Handler { COMMAND(setoption),      "Set a UCI option (EvalFile/Hash/Threads/Clear Hash/Move Overhead)"},
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