#pragma once
/// @file input_handling.h UCI command parsing + dispatch (header-only).

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
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
#include "version.h"

/// @brief Read a numeric UCI value strictly: the whole of @p text must be an optional sign and decimal digits that fit
/// a 64-bit integer. The same rule as the Go and Rust engines (strconv.Atoi, str::parse::<i64>).
/// @param text The token (or, for setoption, the joined value).
/// @return The value, or nothing for anything else ("abc", "12x", "", "+-3", an overflow): the caller then treats
/// the value, and the keyword or option it belongs to, as absent. Never throws, unlike std::stoi, which ended the
/// engine on `go depth abc`.
inline std::optional<long long> ParseInteger(std::string_view text)
{
    if (text.starts_with('+'))
    {
        text.remove_prefix(1); // from_chars takes a minus sign but not a plus
        if (text.starts_with('-'))
        {
            return std::nullopt;
        }
    }
    long long value                = 0;
    const auto [end, error_code]   = std::from_chars(text.data(), text.data() + text.size(), value);
    const bool is_whole_token_read = error_code == std::errc{} && end == text.data() + text.size();
    return is_whole_token_read ? std::optional<long long>{value} : std::nullopt;
}

/// @brief A parsed value narrowed to an int field (a depth, a move count), saturating rather than wrapping.
/// @param value The parsed value.
/// @return @p value clamped to the range of int.
inline int SaturateToInt(long long value)
{
    return static_cast<int>(
        std::clamp<long long>(value, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
}

/// @brief Pointer to a command handler taking the game and the command's argument list.
using HandlerFn = void (*)(Game &game, std::span<std::string> args);

/// @brief Structure to hold a hander for an input command.
struct Handler
{
    HandlerFn        function_;    ///< Function to be called
    std::string_view name_;        ///< Command name
    std::string_view description_; ///< Command description
    constexpr Handler(HandlerFn fn, std::string_view name, std::string_view desc);
};

/// @brief Construct a command handler entry.
/// @param fn Handler function. @param name Command name. @param desc Human-readable description.
constexpr Handler::Handler(HandlerFn fn, std::string_view name, std::string_view desc)
    : function_(fn), name_(name), description_(desc)
{
}

/// @brief Handle the "bookmoves" command: display book moves for the current position.
/// @param game Game to act on.
inline void handle_bookmoves(Game &game, std::span<std::string>)
{
    game.book_.DisplayAvailableMoves(game.CurrentPosition());
}

/// @brief Handle the "freebook" command: release the opening book from memory.
/// @param game Game to act on.
inline void handle_freebook(Game &game, std::span<std::string>)
{
    game.book_.Free();
}

/// @brief Handle the "eval" command: print the static evaluation of the current position.
/// @param game Game to act on.
inline void handle_eval(Game &game, std::span<std::string>)
{
    if (!game.nnue_network_.loaded_)
    {
        std::cout << "info string no NNUE net loaded (use setoption name EvalFile value <path>)\n";
        return;
    }
    SearchState tmp{game};
    std::cout << std::format("evaluation {} (nnue)\n", EvaluatePosition(tmp));
}

/// @brief Handle the "nnue" command: print the raw NNUE evaluation of the current position (diagnostic).
/// @param game Game to act on.
inline void handle_nnue(Game &game, std::span<std::string>)
{
    if (!game.nnue_network_.loaded_)
    {
        std::cout << "info string NNUE: no net loaded (use setoption name EvalFile value <path>)\n";
        return;
    }
    std::cout << std::format("nnue {}\n", game.nnue_network_.Evaluate(game.CurrentPosition()));
}

/// @brief Handle the "dbg" command: print diagnostic counters.
inline void handle_dbg(Game &, std::span<std::string>)
{
    DebugXWrite();
}

/// @brief Handle the "dbgclear" command: reset diagnostic counters.
inline void handle_dbgclear(Game &, std::span<std::string>)
{
    DebugXClear();
}

/// @brief Handle the "getboard" command: print the FEN of the current position.
/// @param game Game to act on.
inline void handle_getboard(Game &game, std::span<std::string>)
{
    auto fen = game.CurrentPosition().ToString();
    std::cout << std::format("{}\n", fen);
}

/// @brief Handle the "uci" command: identify the engine and acknowledge UCI mode.
inline void handle_uci(Game &game, std::span<std::string>)
{
    std::cout << std::format("id name Pawnstar {}\n", VersionString());
    std::cout << "id author Jonny Reckless\n";
    std::cout << "option name Hash type spin default " << kHashtableMegabytes << " min 1 max 4096\n";
    std::cout << "option name Threads type spin default " << game.thread_count_ << " min 1 max " << kMaxSearchThreads
              << "\n";
    std::cout << "option name Ponder type check default false\n";
    std::cout << "option name EvalFile type string default <empty>\n";
    std::cout << "option name Clear Hash type button\n";
    std::cout << "option name Move Overhead type spin default " << game.move_overhead_.count() << " min 0 max 5000\n";
    std::cout << "option name OwnBook type check default " << (game.use_own_book_ ? "true" : "false") << "\n";
    std::cout << "uciok\n";
}

/// @brief Handle the "setoption" command: "setoption name <Name> value <Value>".
/// Recognises EvalFile (load a different NNUE net file; NNUE is the only evaluator — a net the loader
/// refuses leaves the net already loaded playing, and says so on both streams, naming it), Hash (transposition
/// table size in MB, 1..4096), Threads (Lazy SMP search threads, 1..kMaxSearchThreads), Clear Hash (a
/// button that empties the transposition table), Move Overhead (ms reserved from each deadline, 0..5000) and
/// OwnBook (whether to consult the built-in opening book). A numeric option whose value is not an integer
/// (ParseInteger) is left as it is.
/// @param args Command arguments.
inline void handle_setoption(Game &game, std::span<std::string> args)
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
        if (game.nnue_network_.Load(value))
        {
            game.eval_cache_.Clear(); // cached evals belong to the previous net
        }
        else
        {
            // **A refusal must say what is still playing, not only what it would not take.** The loader has
            // already named the field and both values, on stdout and on stderr; what it cannot say is which
            // net the engine will go on using. Keeping the loaded net is right — every check happens before
            // a weight is written, so nothing was disturbed — but being *quiet* about it is the defect: a
            // match harness sets EvalFile, sees `readyok`, and attributes a thousand games to a net that
            // never played a move. Both streams again, because a harness keeps stderr and routinely drops
            // `info` lines, and the same wording in all four engines so one harness can grep for it.
            const std::string kept =
                game.nnue_network_.loaded_
                    ? std::format("NNUE: keeping the net already loaded ('{}'); '{}' was refused and is not "
                                  "in play",
                                  game.nnue_network_.loaded_net_origin_, value)
                    // No net has ever loaded on this Game, so there is nothing to keep. The engine binary
                    // cannot reach this (main exits rather than start without a net), but a Game driven in
                    // process can, and "keeping the net already loaded ('')" would be a lie.
                    : std::format("NNUE: no net was loaded, and '{}' was refused, so there is still no net in "
                                  "play",
                                  value);
            std::cout << std::format("info string {}\n", kept) << std::flush;
            std::cerr << kept << "\n" << std::flush;
            // The eval cache is deliberately NOT cleared. Its entries belong to the net that is still the
            // one playing, so clearing them would throw away warm evaluations for a net change that never
            // happened — the cache is only stale when the net actually changed, which is the branch above.
        }
    }
    else if (name == "Hash")
    {
        if (const std::optional<long long> mb = ParseInteger(value)) // a malformed value leaves the table as it is
        {
            game.transposition_table_.Resize(static_cast<std::size_t>(std::clamp<long long>(*mb, 1, 4096)));
        }
    }
    else if (name == "Threads")
    {
        if (const std::optional<long long> threads = ParseInteger(value))
        {
            game.thread_count_ = static_cast<int>(std::clamp<long long>(*threads, 1, kMaxSearchThreads));
        }
    }
    else if (name == "Clear Hash") // a button: no value, just empty the transposition table
    {
        game.transposition_table_.Clear();
    }
    else if (name == "Move Overhead")
    {
        if (const std::optional<long long> overhead = ParseInteger(value))
        {
            game.move_overhead_ = ChessClock::Duration{std::clamp<long long>(*overhead, 0, 5000)};
        }
    }
    else if (name == "OwnBook")
    {
        game.use_own_book_ = (value == "true");
    }
}

/// @brief Handle the "ucinewgame" command: stop searching and reset to a new game.
/// Clears all shared, game-spanning state so the new game starts clean: the transposition table and the
/// NNUE eval cache. (`SetPosition` itself only resets per-position state and clears the eval cache — it is also
/// called per move by `position`, so it must NOT clear the TT, whose whole value is persisting across moves.
/// The per-thread history / killer / countermove / continuation tables live on `SearchState` and are rebuilt
/// fresh on every search, so they never carry across games and need no reset here.)
/// @param game Game to act on.
inline void handle_ucinewgame(Game &game, std::span<std::string>)
{
    game.StopThinking();
    game.transposition_table_.Clear();
    game.eval_cache_.Clear();
    game.SetPosition();
}

/// @brief Fixed, known-legal position set for `bench` (the Bratko-Kopec middlegame/endgame positions). Each
/// is searched to a fixed depth; the total node count is a determinism signature and the nps a speed check.
static const char *const kBenchFens[] = {
    "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1",
    "3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - - 0 1",
    "2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - - 0 1",
    "rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq - 0 1",
    "r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - - 0 1",
    "2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - - 0 1",
    "1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - - 0 1",
    "4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - - 0 1",
    "2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - - 0 1",
    "3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - - 0 1",
    "2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - - 0 1",
    "r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - - 0 1",
    "r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - - 0 1",
    "rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - - 0 1",
    "2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - - 0 1",
    "r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq - 0 1",
};

/// @brief Default `bench` search depth (overridden by `bench <depth>`).
static constexpr int kBenchDepth = 13;

/// @brief One bench position's outcome: the move the search chose and the nodes it took.
struct BenchResult
{
    std::string best_move_; ///< The best move found, in UCI notation.
    uint64_t    nodes_;     ///< Nodes the main thread searched for this position.
};

/// @brief The bench recipe, shared by the `bench` command and the bench-golden test (test/bench_golden_test.cpp):
/// search each position in kBenchFens to @p depth, single-threaded with the opening book off (both restored
/// afterwards) and the hash and eval cache cleared up front. Leaves the board at the last bench position.
/// @param game Game to search with (a net must be loaded).
/// @param depth Fixed search depth for every position.
/// @param debug After each search, print the position's bestmove and nodes and dump the DEBUGX counters (cleared
///        before it), for the cross-engine transcript diff (PAWNSTAR_BENCH_DEBUG).
/// @return Each position's best move and node count, in kBenchFens order.
inline std::vector<BenchResult> RunBench(Game &game, int depth, bool debug)
{
    const int  saved_threads  = game.thread_count_;
    const bool saved_own_book = game.use_own_book_;
    game.thread_count_        = 1;     // deterministic, single-threaded
    game.use_own_book_        = false; // never short-circuit on a book move
    game.transposition_table_.Clear();
    game.eval_cache_.Clear();

    std::vector<BenchResult> results;
    for (const char *fen : kBenchFens)
    {
        game.SetPosition(fen); // also clears the cancel flag the previous search left set (SearchRootNode never does)
        game.time_control_.clock_type_ = ChessClock::kFixedDepth; // SetPosition reset the clock; set fixed depth after
        game.time_control_.depth_      = depth;
        if (debug)
        {
            DebugXClear();
        }
        const Move move = game.SearchRootNode();
        results.push_back({move.ToString(), game.last_search_node_count_});
        if (debug)
        {
            std::cout << std::format("### pos {:02} bm={} nodes={}\n", results.size(), results.back().best_move_,
                                     results.back().nodes_);
            DebugXWrite();
        }
    }
    game.thread_count_ = saved_threads;
    game.use_own_book_ = saved_own_book;
    return results;
}

/// @brief Handle the "bench" command: search each position in kBenchFens to a fixed depth (kBenchDepth, or
/// `bench <depth>`) through RunBench and print `<nodes> nodes <nps> nps`. The node total is a determinism signature
/// (a search change meant to be neutral that alters it shows up immediately; test/bench_golden_test.cpp pins it at
/// the default depth) and the nps a quick speed-regression check. Leaves the board at the last bench position (run
/// it standalone).
inline void handle_bench(Game &game, std::span<std::string> args)
{
    game.StopThinking(); // bench searches on this thread, over the position and tables a running search uses
    // `bench <depth>` overrides the default; a missing, malformed or non-positive depth keeps it.
    const std::optional<long long> requested = args.size() > 1 ? ParseInteger(args[1]) : std::nullopt;
    const int                      depth     = requested && *requested > 0 ? SaturateToInt(*requested) : kBenchDepth;
    // PAWNSTAR_BENCH_DEBUG dumps each position's bestmove and diagnostic counters after its search, for
    // cross-engine comparison against the Go and Rust benches. Off by default, so normal bench output is unchanged.
    const bool debug       = std::getenv("PAWNSTAR_BENCH_DEBUG") != nullptr;
    const auto start       = ChessClock::Clock::now();
    uint64_t   total_nodes = 0;
    for (const BenchResult &result : RunBench(game, depth, debug))
    {
        total_nodes += result.nodes_;
    }
    const long long elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(ChessClock::Clock::now() - start).count();
    const long long nps = static_cast<long long>(total_nodes) * 1000 / std::max<long long>(1, elapsed_ms);
    std::cout << std::format("{} nodes {} nps\n", total_nodes, nps);
}

/// @brief Handle the "go" command: parse time controls and start searching.
/// Parsed as a small state machine alternating between a keyword and its integer argument, rather than
/// scanning with `args[++i]` look-ahead (which also read out of bounds when a value keyword ended the
/// input). Recognised keywords: the flags `infinite`/`ponder`, and the value-taking `wtime`/`btime`/
/// `winc`/`binc`/`movetime`/`depth`/`movestogo`/`nodes`, and `searchmoves` with its moves; any other token is
/// ignored. A value-taking keyword whose value is missing or not an integer (ParseInteger) is ignored with it.
/// @param game Game to act on.
/// @param args Command arguments (time controls, depth, etc.).
inline void handle_go(Game &game, std::span<std::string> args)
{
    /// @brief Parser state: whether the next token is a keyword or the pending keyword's integer value.
    enum class State
    {
        kAwaitKeyword, ///< Expecting a keyword (`infinite`, `wtime`, `depth`, …).
        kAwaitValue,   ///< Expecting the integer argument for @c pending.
        kCollectMoves, ///< Collecting the move list that follows `searchmoves`.
    };

    State            state = State::kAwaitKeyword;
    std::string_view pending; // the value-taking keyword whose argument is expected next

    // Stop any running search before touching the clock and the root-move list it reads: a `go` that arrives while
    // one runs would otherwise rewrite them under it (a data race; the Go engine's race detector caught `go infinite`
    // + `go depth 1`). The Rust engine stops first for the same reason; StartThinking's own stop is then a no-op.
    game.StopThinking();
    game.is_pondering_.store(false, std::memory_order_relaxed);   // set true below if this is a `go ponder`
    game.time_control_.increment_ = ChessClock::Duration::zero(); // cleared up front; set below if winc/binc present
    game.time_control_.max_nodes_ = 0;                            // cleared up front; set below if `nodes` present
    game.search_moves_.clear();                                   // set below if `searchmoves` present

    for (std::size_t i = 1; i < args.size(); ++i)
    {
        const std::string &token = args[i];
        if (state == State::kCollectMoves)
        {
            // Collect coordinate-notation moves until the next recognised keyword (searchmoves is usually
            // last, but tolerate a keyword following it).
            if (token == "ponder" || token == "infinite" || token == "wtime" || token == "btime" || token == "winc" ||
                token == "binc" || token == "movetime" || token == "depth" || token == "movestogo" ||
                token == "nodes" || token == "searchmoves")
            {
                state = State::kAwaitKeyword;
                --i; // reprocess this keyword on the next iteration
                continue;
            }
            game.search_moves_.push_back(token);
            continue;
        }
        if (state == State::kAwaitKeyword)
        {
            if (token == "ponder") // flag: search on the opponent's time until ponderhit / stop
            {
                game.is_pondering_.store(true, std::memory_order_relaxed);
            }
            else if (token == "searchmoves") // restrict the root search to the move list that follows
            {
                state = State::kCollectMoves;
            }
            else if (token == "infinite") // a flag, no argument
            {
                game.time_control_.clock_type_ = ChessClock::kInfinite; // no time limit; remaining_time is unused
            }
            else if (token == "wtime" || token == "btime" || token == "winc" || token == "binc" ||
                     token == "movetime" || token == "depth" || token == "movestogo" || token == "nodes")
            {
                pending = token; // a value-taking keyword: its argument is the next token
                state   = State::kAwaitValue;
            }
            // else: ignore unrecognised tokens (nodes, searchmoves and its move list, …)
        }
        else // kAwaitValue: token is the integer argument for `pending`
        {
            // A value that is not an integer (`go depth abc`) is consumed and ignored, and so is its keyword, exactly
            // as a keyword with no value at the end of the line is: the line reads as though neither were there.
            const std::optional<long long> parsed = ParseInteger(token);
            state                                 = State::kAwaitKeyword;
            if (!parsed)
            {
                continue;
            }
            const long long value = *parsed;
            if ((pending == "wtime" && game.CurrentPosition().color_to_move_ == kWhite) ||
                (pending == "btime" && game.CurrentPosition().color_to_move_ == kBlack))
            {
                game.time_control_.clock_type_     = ChessClock::kStandard;
                game.time_control_.remaining_time_ = ChessClock::Duration{value};
            }
            else if ((pending == "winc" && game.CurrentPosition().color_to_move_ == kWhite) ||
                     (pending == "binc" && game.CurrentPosition().color_to_move_ == kBlack))
            {
                game.time_control_.increment_ = ChessClock::Duration{value};
            }
            else if (pending == "movetime")
            {
                game.time_control_.clock_type_     = ChessClock::kFixedTime;
                game.time_control_.remaining_time_ = ChessClock::Duration{value};
            }
            else if (pending == "depth")
            {
                game.time_control_.clock_type_ = ChessClock::kFixedDepth;
                game.time_control_.depth_      = SaturateToInt(value);
            }
            else if (pending == "movestogo")
            {
                game.time_control_.moves_to_go_ = SaturateToInt(value);
            }
            else if (pending == "nodes")
            {
                // Node-limited search: run with no clock (kInfinite) and stop at the node budget (Search polls
                // max_nodes). Useful for reproducible, machine-speed-independent testing.
                game.time_control_.clock_type_ = ChessClock::kInfinite;
                game.time_control_.max_nodes_  = static_cast<uint64_t>(value);
            }
            // (wtime/btime for the side *not* to move falls through here and is intentionally ignored.)
        }
    }
    game.StartThinking();
}

/// @brief Handle the "stop" command: stop searching and report the best move found.
/// @param game Game to act on.
inline void handle_stop(Game &game, std::span<std::string>)
{
    game.StopThinking();
}

/// @brief Handle the "ponderhit" command: the predicted move was played, so convert the running ponder
/// search into a normally-timed search, keeping all work done so far (warm TT, current iteration depth).
/// `OnPonderhit` starts the budget from this moment (budget-from-ponderhit) by resetting the soft-time
/// origin and arming the hard deadline; is_pondering is cleared last so the search never sees the new flag
/// with a stale start time.
/// @param game Game to act on.
inline void handle_ponderhit(Game &game, std::span<std::string>)
{
    game.time_control_.OnPonderhit();
    game.is_pondering_.store(false, std::memory_order_relaxed);
}

/// @brief Handle the "quit" command: stop searching and exit the program.
/// @param game Game to act on.
inline void handle_quit(Game &game, std::span<std::string>)
{
    game.StopThinking();
    exit(0);
}

/// @brief Handle the "isready" command: respond with readyok.
inline void handle_isready(Game &, std::span<std::string>)
{
    std::cout << "readyok\n";
}

/// @brief Handle the "position" command: set up the position and apply the listed moves.
/// Parses the UCI grammar `position [startpos | fen <6 fields>] [moves <m1> <m2> ...]` as a small state
/// machine over the tokens, rather than scanning with look-ahead and index mutation.
/// @param game Game to act on.
/// @param args Command arguments (startpos / fen and the move list).
inline void handle_position(Game &game, std::span<std::string> args)
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

    game.StopThinking(); // a search still running reads the position stack this rewrites (as in the Go and Rust)
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        const std::string &token = args[i];
        switch (state)
        {
        case State::kAwaitSource:
            if (token == "startpos")
            {
                game.SetPosition();
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
                game.SetPosition(fen); // FEN ended early (fewer than six fields is tolerated)
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
                    game.SetPosition(fen);
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
        game.SetPosition(fen);
    }
}

/// @brief The text the "license" command prints: this engine's licence, then every third-party notice the
/// material it distributes obliges. Condensed from THIRD-PARTY-NOTICES.md, which is the full text.
///
/// **The same notice the sibling ports print**, line for line with pawnstar-rs's `LICENSE_NOTICE`
/// (its src/input_handling.rs), so a user reads the same words whichever engine they ran. The one
/// deliberate difference is the last paragraph: this repository redistributes the two bullet-adapted
/// trainer sources under tools/bullet/, where the Rust port carries none and says instead that it has no
/// third-party code dependencies. Keep the rest in step across the ports.
///
/// **The opening-book paragraph changed on 2026-10-30 and is owed to both ports.** It used to say the book
/// was derived from the public-domain TSCP `book.txt`, which was false - TSCP is Copyright 1997 Tom
/// Kerrigan, under an informal non-commercial permission GPLv3 cannot take. The book is now built from the
/// Lichess open database under CC0 1.0, and the paragraph says plainly that CC0 obliges no attribution, so
/// the credit is a courtesy. `test_uci_commands` pins the new text, the absence of the old claim, and that
/// sentence about the courtesy, along with the sentence distinguishing the 84 lines this engine
/// chose from the lines the database played.
inline constexpr std::array kLicenseNotice = {
    "Pawnstar is free software under the GNU General Public License, version 3,\n",
    "and comes with NO WARRANTY. See <https://www.gnu.org/licenses/gpl-3.0.html>.\n",
    "\n",
    "Third-party notices (the full text is THIRD-PARTY-NOTICES.md in the source):\n",
    "\n",
    "The NNUE network was trained entirely on Leela Chess Zero self-play training data,\n",
    "runs test91 (62.83% of the pool) and test60 (37.17%):\n",
    "\n",
    "  Position data from the Leela Chess Zero self-play training data\n",
    "  (https://storage.lczero.org/files/training_data/), run test91, used under the\n",
    "  Open Database License 1.0 (http://opendatacommons.org/licenses/odbl/1.0/).\n",
    "\n",
    "  Position data from the Leela Chess Zero self-play training data\n",
    "  (https://storage.lczero.org/files/training_data/), run test60, used under the\n",
    "  Open Database License 1.0 (http://opendatacommons.org/licenses/odbl/1.0/).\n",
    "\n",
    "The database is under the Open Database License 1.0\n",
    "(http://opendatacommons.org/licenses/odbl/1.0/) and its contents under the Database\n",
    "Contents License 1.0 (http://opendatacommons.org/licenses/dbcl/1.0/). A trained network\n",
    "is an ODbL \"Produced Work\": the obligation is this notice. ODbL does not require the\n",
    "network itself to be ODbL-licensed, and the network is not GPL merely because GPLv3 code\n",
    "trained it - a program's licence does not reach the data it processes. The network is\n",
    "distributed under this project's own licence, by choice.\n",
    "\n",
    "The opening book was built from the Lichess open database\n",
    "(https://database.lichess.org/), which is dedicated to the public domain under\n",
    "Creative Commons CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/).\n",
    "CC0 waives attribution, so this credit is owed to nobody; it is given because saying\n",
    "where the lines came from is right. The book carries no third party's restriction.\n",
    "\n",
    "Eighty-four of the book's 3,328 lines end in a move this engine chose rather than\n",
    "one the database played: screening left those positions no move this engine would\n",
    "play, so its own choice was added. They are this project's own work, and carry no\n",
    "condition either.\n",
    "\n",
    "Networks up to and including v12 were trained on self-play data generated by\n",
    "PlentyChess, by Yoshie2000. That data is no longer used: no network in this release was\n",
    "trained on any of it, and every network that was has been withdrawn.\n",
    "\n",
    "The NNUE trainer sources under tools/bullet/ are adapted from bullet\n",
    "(https://github.com/jw1912/bullet), MIT-licensed: see THIRD-PARTY-NOTICES.md for the\n",
    "copyright and permission notice that accompanies them.\n"};

/// @brief Handle the "license" command: print the engine's licence and the third-party notices a
/// distributed copy of it owes - above all the Open Database License attribution the shipped network
/// carries. ODbL wants the notice to accompany the Produced Work, and a user who downloads only a release
/// binary gets no file from this repository at all, so the engine has to be able to say it itself.
/// It writes to stdout and touches no engine state, so it cannot affect a search: the text is the whole
/// of it.
inline void handle_license(Game &, std::span<std::string>)
{
    for (const std::string_view line : kLicenseNotice)
    {
        std::cout << line;
    }
}

/// @brief Handle the "help" command: list all available commands.
inline void handle_help(Game &, std::span<std::string>);

/// @brief Expand to a handler function pointer and its command name string.
#define COMMAND(name) handle_##name, #name

// clang-format off
/// @brief Table of all supported input commands and their handlers.
inline constexpr std::array handlers =
{
    Handler { COMMAND(bench),          "Search a fixed position set to fixed depth; print nodes + nps"},
    Handler { COMMAND(bookmoves),      "Display available book moves for current position"},
    Handler { COMMAND(dbg),            "Display diagnostic counts"},
    Handler { COMMAND(dbgclear),       "Reset diagnostic counts"},
    Handler { COMMAND(eval),           "Display the current static evaluation"},
    Handler { COMMAND(freebook),       "Delete the opening book from memory"},
    Handler { COMMAND(getboard),       "Display the FEN of the current position"},
    Handler { COMMAND(go),             "Search the current position"},
    Handler { COMMAND(help),           "Display a summary of commands"},
    Handler { COMMAND(isready),        "Respond with readyok"},
    Handler { COMMAND(license),        "Display the engine's licence and its third-party notices"},
    Handler { COMMAND(nnue),           "Display the raw NNUE evaluation of the current position"},
    Handler { COMMAND(ponderhit),      "Pondered move was played; convert ponder search to a timed search"},
    Handler { COMMAND(position),       "Set the position and series of moves"},
    Handler { COMMAND(quit),           "Exit the program"},
    Handler { COMMAND(setoption),      "Set a UCI option (EvalFile/Hash/Threads/Clear Hash/Move Overhead/OwnBook)"},
    Handler { COMMAND(stop),           "Stop searching and return best move found"},
    Handler { COMMAND(uci),            "Enter UCI protocol"},
    Handler { COMMAND(ucinewgame),     "UCI mode start new game"}
};
// clang-format on

/// @brief Handle the "help" command: list all available commands.
inline void handle_help(Game &, std::span<std::string>)
{
    std::cout << "Available commands:\n";
    for (auto &i : handlers)
    {
        std::cout << std::format("{:<12} {}\n", i.name_.data(), i.description_.data());
    }
}

/// @brief Tokenize a line of input and dispatch it to the matching command handler.
/// @param game Game to act on.
/// @param line Input line.
inline void ProcessInput(Game &game, std::string_view line)
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
        if (args[0] == handler.name_)
        {
            handler.function_(game, args);
            break;
        }
    }
}