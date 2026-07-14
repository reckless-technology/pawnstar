#pragma once
/// @file game.h Game: the shared engine state (transposition table, clock, NNUE net, book, options) and the
/// root search entry point, pulled in by all Lazy SMP threads.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "chess_clock.h"
#include "constants.h"
#include "eval_cache.h"
#include "nnue.h"
#include "opening_book.h"
#include "position.h"
#include "transposition_table.h"

/// @brief Class to represent a game of chess.
class Game
{
  public:
    // State variables
    // clang-format off
    TranspositionTable      transposition_table_;    ///< The transposition table (shared by all Lazy SMP threads).
    EvalCache               eval_cache_;             ///< Lockless NNUE evaluation cache (shared by all threads).
    ChessClock              time_control_;           ///< Clock controls for the current game.
    OpeningBook             book_;                   ///< The opening book.
    std::atomic<bool>       is_cancel_pending_;      ///< Shared abort flag: set on timeout, UCI stop, AND by the main thread at the end of every search (helpers wind up). Written by the search threads.
    std::atomic<bool>       is_stop_requested_;      ///< "The GUI asked us to stop" (`stop` / `quit`). Written ONLY by the UCI thread; never by a search thread. Releases the ponder-wait.
    int                     thread_count_;           ///< Lazy SMP search threads; default from PAWNSTAR_THREADS / hardware, set by UCI `Threads`.
    ChessClock::Duration    move_overhead_;          ///< Reserved from each hard deadline for GUI/network lag (UCI `Move Overhead`, ms).
    std::atomic<bool>       is_pondering_;           ///< True while a `go ponder` search runs (until `ponderhit` / `stop`); suppresses time stops. Written only by the UCI thread.
    Move                    ponder_move_;            ///< Predicted opponent reply (PV[1]); emitted as `bestmove <m> ponder <ponder_move>`.
    bool                    use_own_book_;           ///< Whether to consult the built-in opening book (UCI `OwnBook`).
    uint64_t                last_search_node_count_; ///< Node count of the most recent search's main thread (for `bench`).
    std::vector<std::string> search_moves_;          ///< UCI `go searchmoves`: restrict the root search to these (empty = all).
    std::vector<Position>   positions_;              ///< Game position history (grows with the game; no fixed cap).
    nnue::Network           nnue_network_;           ///< NNUE network instance (loaded via EvalFile; read-only in search).
    // clang-format on

    // Interface
    Game();
    static int      ComputeDefaultThreads();
    Position       &CurrentPosition();
    const Position &CurrentPosition() const;
    void            SetPosition(std::string_view fen_string);
    void            SetPosition();
    Move            PlayMove(std::string_view move_string);
    void            PlayMove(Move move);
    void            MakeNullMove();
    void            UndoMove();
    void            StartThinking();
    void            StopThinking();
    Move            SearchRootNode();

  private:
    void        SearchThreadEntry();
    std::thread worker_thread_; ///< Worker thread for searching moves.
};

/// @brief Construct a game, sizing the transposition tables and starting from the initial position.
inline Game::Game()
    : transposition_table_{kHashtableMegabytes}, eval_cache_{kEvalCacheMb}, is_cancel_pending_{false},
      is_stop_requested_{false}, thread_count_{ComputeDefaultThreads()}, move_overhead_{ChessClock::Duration{30}},
      is_pondering_{false}, ponder_move_{Move::None()}, use_own_book_{true}, last_search_node_count_{0}
{
    SetPosition();
}

/// @brief Current position.
/// @return Reference to the current position.
inline Position &Game::CurrentPosition()
{
    return positions_.back();
}

/// @brief Current position (const overload).
/// @return Const reference to the current position.
inline const Position &Game::CurrentPosition() const
{
    return positions_.back();
}

// ---- Out-of-class definitions (header-only build). The methods here need only class Game; Game::SearchRootNode
//      needs SearchState complete, so it is defined at the very bottom, after the search_state.h include. ----

/// @brief Set the board to a position from a FEN (Forsyth Edwards) string, resetting per-game transient
/// state (move stack, clock, eval cache, cancel flag). Called for a new game and by every `position` command.
/// @param fen_string Initial position.
inline void Game::SetPosition(std::string_view fen_string)
{
    time_control_.Reset();
    is_cancel_pending_.store(false, std::memory_order_relaxed);
    eval_cache_.Clear(); // evals are net-specific; start each game with a clean cache
    positions_.clear();
    positions_.reserve(1024); // typical games stay well under this; avoids reallocations during normal play
    positions_.push_back(Position::FromString(fen_string));
}

/// @brief Set the board to the standard initial position (overload of SetPosition).
inline void Game::SetPosition()
{
    SetPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

/// @brief Play a move and update state. Move must be legal for the current position.
/// @param move The move to be played.
inline void Game::PlayMove(Move move)
{
    positions_.push_back(CurrentPosition().MakeMove(move));
}

/// @brief Make a null move. Only used during null move heuristic.
inline void Game::MakeNullMove()
{
    positions_.push_back(CurrentPosition().MakeNullMove());
}

/// @brief Undo the last move played.
inline void Game::UndoMove()
{
    positions_.pop_back();
}

/// @brief Play a move from the given string and update game state accordingly.
/// @param move_str The string containing the move to be made, e.g. "e2e4", "e7e8q" (promotion), "e1g1" (castling).
/// @return The move which was made, or Move::None in the case the string did not contain a legal move for this
/// position, and no move was therefore played.
inline Move Game::PlayMove(std::string_view move_str)
{
    MoveList move_list = CurrentPosition().GenerateLegalMoves();
    for (const Move &move : move_list)
    {
        if (move.ToString() == move_str)
        {
            PlayMove(move);
            return move;
        }
    }
    return Move::None();
}

/// @brief Entry point for the search worker thread.
inline void Game::SearchThreadEntry()
{
    Move move = SearchRootNode();
    // UCI forbids sending `bestmove` while pondering: if the ponder search ended on its own (reached max
    // depth or a forced mate) before the GUI resolved it, wait here until `ponderhit` clears is_pondering or
    // `stop` sets is_stop_requested. For a normal (non-ponder) search is_pondering is false, so this is a no-op.
    //
    // Both flags are written ONLY by the UCI thread, which is what makes this wait race-free. It must not be
    // gated on is_cancel_pending: the search itself sets that flag at the end of every root search (to wind the
    // Lazy SMP helpers up), which made this guard permanently false — dead code — and the engine answered while
    // still pondering. Nor can the search restore it afterwards: a `stop` landing in that window would be
    // clobbered, nothing would ever clear is_pondering, and this loop would spin forever while StopThinking
    // blocked in join() — a permanent hang. Hence the dedicated flag no search thread ever touches.
    while (is_pondering_.load(std::memory_order_relaxed) && !is_stop_requested_.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (move != Move::None())
    {
        PlayMove(move);
        if (ponder_move_ != Move::None())
        {
            std::cout << std::format("bestmove {} ponder {}\n", move.ToString(), ponder_move_.ToString());
        }
        else
        {
            std::cout << std::format("bestmove {}\n", move.ToString());
        }
    }
}

/// @brief If currently thinking, stop immediately.
/// is_stop_requested is set FIRST and left set: it is the durable record that the GUI asked us to stop, and it
/// is what releases the ponder-wait in SearchThreadEntry (is_cancel_pending cannot, since the search sets that
/// itself at the end of every search). StartThinking clears it for the next search.
/// Note: this does NOT clear is_pondering — handle_go sets it just before calling StartThinking (which calls
/// StopThinking), so clearing it here would clobber the flag for the search about to start.
inline void Game::StopThinking()
{
    is_stop_requested_.store(true, std::memory_order_relaxed); // durable: releases the ponder-wait
    is_cancel_pending_.store(true, std::memory_order_relaxed); // aborts the running search
    if (worker_thread_.joinable())
    {
        worker_thread_.join();
    }
    is_cancel_pending_.store(false, std::memory_order_relaxed); // the search is over; leave no stale abort
}

/// @brief Start thinking on the worker thread.
inline void Game::StartThinking()
{
    StopThinking(); // joins any previous search (and leaves is_stop_requested set)
    // A fresh search: the previous stop is resolved, so clear both flags before the worker starts. Ordering
    // matters — these must land before the thread launches, which the std::thread constructor guarantees.
    is_stop_requested_.store(false, std::memory_order_relaxed);
    is_cancel_pending_.store(false, std::memory_order_relaxed);
    worker_thread_ = std::thread([this] { this->SearchThreadEntry(); });
}

/// @brief Default Lazy SMP thread count: PAWNSTAR_THREADS if set (>0), else hardware_concurrency, clamped to
/// [1, kMaxSearchThreads]. Read once at construction; the UCI `Threads` option overrides Game::thread_count.
inline int Game::ComputeDefaultThreads()
{
    const unsigned hw = std::thread::hardware_concurrency();
    int            n  = std::clamp<int>(hw == 0 ? 1 : (int)hw, 1, kMaxSearchThreads);
    if (const char *env = std::getenv("PAWNSTAR_THREADS"))
    {
        const int requested = std::atoi(env);
        if (requested > 0)
        {
            n = std::clamp(requested, 1, kMaxSearchThreads);
        }
    }
    return n;
}

// Header-only: pull in the search_state.h hub here, at the very bottom — after class Game is fully defined —
// so that simply including game.h yields a complete, searchable Game. search_state.h defines class SearchState
// (and its methods + EvaluatePosition); with SearchState complete, Game::SearchRootNode is defined just below.
// The include is guarded by pragma once and works in either order: a TU that includes search_state.h first has
// that header define the SearchState class and then include game.h, so this line is skipped yet SearchRootNode
// below still sees a complete SearchState; a TU that includes game.h first drives search_state.h from here,
// which sees the now-complete Game.
#include "search_state.h"

// ---- Game::SearchRootNode — needs SearchState complete, so defined here after the hub include above ----

/// @brief Search the current position and return the best move.
/// Returns a book move immediately on a book hit; otherwise runs the Lazy SMP iterative-deepening search:
/// one authoritative main thread plus (Game::thread_count - 1) helpers, all sharing the transposition table.
/// @return The best move, or Move::None if there are no legal moves.
inline Move Game::SearchRootNode()
{
    Game &game        = *this;        // local alias so the search body reads uniformly through `game`
    game.ponder_move_ = Move::None(); // set by the main thread's IterativeDeepen if the PV has a reply move
    // Clear the abort flag up front so the search does not start already cancelled. StartThinking clears it
    // too; this also covers the callers that invoke SearchRootNode directly on the UCI thread (`bench`).
    game.is_cancel_pending_.store(false, std::memory_order_relaxed);
    game.last_search_node_count_ = 0; // set below once a real search runs (stays 0 for book/forced returns)
    // If there is a book move for this position, do not bother with search (unless OwnBook is disabled).
    // Skip the book when `go searchmoves` restricts the root (the caller wants those moves searched, not a
    // book reply).
    if (game.use_own_book_ && game.search_moves_.empty())
    {
        Move book_move = game.book_.GetMove(game.CurrentPosition().hash_);
        if (book_move != Move::None())
        {
            return book_move;
        }
    }
    MoveList move_list = game.CurrentPosition().GenerateLegalMoves();
    // UCI `go searchmoves`: restrict the root move list to the requested moves (ignored if none of them are
    // legal here, so a malformed list can't wedge the search).
    if (!game.search_moves_.empty())
    {
        MoveList filtered;
        for (const Move &move : move_list)
        {
            if (std::ranges::find(game.search_moves_, move.ToString()) != game.search_moves_.end())
            {
                filtered.push_back(move);
            }
        }
        if (filtered.size() != 0)
        {
            move_list = filtered;
        }
    }
    if (move_list.size() == 0)
    {
        return Move::None();
    }
    // With only one legal move there is nothing to search — but when `searchmoves` is set we still search it
    // (the caller wants its evaluation), so only take this shortcut for a genuine forced move.
    if (move_list.size() == 1 && game.search_moves_.empty())
    {
        return move_list[0];
    }
    // Plan time usage for this search.
    using Duration           = ChessClock::Duration;
    Duration allocated_time  = Duration::zero(); // soft budget (drives between-iteration stopping)
    Duration max_search_time = Duration::zero(); // hard budget (deadline); zero means no hard stop
    switch (game.time_control_.clock_type_)
    {
    case ChessClock::kStandard:
    default: {
        const int moves_to_go = game.time_control_.moves_to_go_ != 0
                                    ? game.time_control_.moves_to_go_
                                    : std::max(40 - game.CurrentPosition().full_move_count_, 5);
        // Soft budget: an even slice of the remaining time. NOTE: folding the per-move increment in here
        // (`+ increment / 2`) was SPRT-tested and did NOT help — neutral-to-slightly-negative at both 8+0.08
        // (-14.5 +/- 21, 480 games) and 10+0.1 (-2.5 +/- 7.9, 3562 games), zero forfeits. In equal-clock
        // self-play at fast TCs spending the increment is ~symmetric; it may pay off at much slower increment
        // TCs (untested). `ChessClock::increment` is still parsed (winc/binc) but deliberately unused here.
        allocated_time = game.time_control_.remaining_time_ / moves_to_go;
        // Hard budget: up to 2x the soft budget, but never spend into the last second of our clock, and always
        // allow at least 100 ms.
        max_search_time =
            std::max(Duration{100}, std::min(allocated_time * 2, game.time_control_.remaining_time_ - Duration{1000}));
        break;
    }
    case ChessClock::kFixedDepth:
        break; // no time limit; the depth loop bounds the search
    case ChessClock::kFixedTime:
        max_search_time = game.time_control_.remaining_time_; // movetime: a hard budget, no soft management
        break;
    case ChessClock::kInfinite:
        break; // no time limit; only `stop` ends it
    }
    // Reserve the configured move overhead (GUI/network lag) from the hard deadline so we don't forfeit on
    // time. Only when there is a hard deadline (fixed-depth / infinite searches leave it zero = no stop).
    if (max_search_time > Duration::zero())
    {
        max_search_time = std::max(Duration{1}, max_search_time - game.move_overhead_);
    }

    // Arm the clock. A ponder search has no hard deadline until `ponderhit` (budget-from-ponderhit); the
    // budgets are recorded so the conversion can start the real clock from that moment.
    if (game.is_pondering_)
    {
        game.time_control_.StartPonderSearch(allocated_time, max_search_time);
    }
    else
    {
        game.time_control_.StartSearch(allocated_time, max_search_time);
    }
    DebugXClear();
    game.transposition_table_.Age();

    // Shallow pass for initial move ordering (on a temporary state shared as the launch ordering).
    SearchState state{game};
    Variation   shallow_pv{};
    for (auto &move : move_list)
    {
        const int move_index = (int)(&move - &move_list[0]);
        const int score = state.SearchSingleMove(kStartDepth, 0, kAlpha, kBeta, move, shallow_pv, move_index).score();
        move.AssignScore(score);
    }
    SortMoves(move_list);
    Move best_move = move_list[0];

    // Lazy SMP: each thread runs an independent iterative-deepening search of the same root, sharing only the
    // (lockless) transposition table (history/killers/etc. are per-thread on each SearchState). The main thread is
    // authoritative; helper threads deepen the shared TT to accelerate it. Each thread has its own SearchState and
    // move-list copy. Thread count is configured via the UCI `Threads` option (or the PAWNSTAR_THREADS default at
    // startup), stored in game.thread_count; re-clamped here defensively.
    const int                n_threads = std::clamp(game.thread_count_, 1, kMaxSearchThreads);
    std::vector<std::thread> helpers;
    helpers.reserve(n_threads - 1);
    for (int i = 1; i < n_threads; ++i)
    {
        // Each helper gets a distinct thread id, which selects its iteration-skip schedule (see IterativeDeepen).
        helpers.emplace_back([&game, &move_list, best_move, i] {
            SearchState helper_state{game};
            helper_state.IterativeDeepen(move_list, best_move, /*thread_id=*/i);
        });
    }

    best_move                    = state.IterativeDeepen(move_list, best_move, /*thread_id=*/0);
    game.last_search_node_count_ = state.node_count_; // main-thread node total (used by `bench`)

    // The main thread is done, so wind the helpers up. Note this makes is_cancel_pending "the search is over"
    // as much as "the GUI said stop" — which is precisely why the ponder-wait in SearchThreadEntry keys off
    // is_stop_requested instead, and why the search thread must never try to restore this flag (a `stop`
    // landing in that window would be clobbered and the engine would hang; see SearchThreadEntry).
    game.is_cancel_pending_.store(true, std::memory_order_relaxed); // signal helpers to stop
    for (auto &helper : helpers)
    {
        helper.join();
    }
    return best_move;
}
