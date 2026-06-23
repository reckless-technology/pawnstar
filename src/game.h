#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
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
    // clang-format off
    TranspositionTable      transposition_table_;        ///< The transposition table (shared by all Lazy SMP threads).
    EvalCache               eval_cache_;                 ///< Lockless NNUE evaluation cache (shared by all threads).
    ChessClock              time_control_;               ///< Clock controls for the current game.
    OpeningBook             book_;                       ///< The opening book.
    std::atomic<bool>       is_cancel_pending_;          ///< Shared stop flag: set on timeout, UCI stop, or search completion.
    int                     thread_count_;               ///< Lazy SMP search threads; default from PAWNSTAR_THREADS / hardware, set by UCI `Threads`.
    ChessClock::Duration    move_overhead_{ChessClock::Duration{30}}; ///< Reserved from each hard deadline for GUI/network lag (UCI `Move Overhead`, ms).
    std::atomic<bool>       is_pondering_{false};        ///< True while a `go ponder` search runs (until `ponderhit` / `stop`); suppresses time stops.
    Move                    ponder_move_{Move::None()};  ///< Predicted opponent reply (PV[1]); emitted as `bestmove <m> ponder <ponder_move>`.
    bool                    use_own_book_{true};         ///< Whether to consult the built-in opening book (UCI `OwnBook`).
    uint64_t                last_search_node_count_{0};  ///< Node count of the most recent search's main thread (for `bench`).
    // clang-format on

    /// @brief Construct a game, sizing the transposition tables and starting from the initial position.
    Game()
        : transposition_table_{kHashtableMegabytes}, eval_cache_{kEvalCacheMb}, is_cancel_pending_{false},
          thread_count_{ComputeDefaultThreads()}
    {
        SetPosition();
    }

    /// @brief Default Lazy SMP thread count: PAWNSTAR_THREADS if set (>0), else hardware_concurrency,
    /// clamped to [1, kMaxSearchThreads]. Read once at construction; the UCI `Threads` option overrides it.
    static int ComputeDefaultThreads();

    /// @brief Current position.
    /// @return Reference to the current position.
    Position &CurrentPosition()
    {
        return positions_.back();
    }

    /// @brief Current position (const overload).
    /// @return Const reference to the current position.
    const Position &CurrentPosition() const
    {
        return positions_.back();
    }

    /// @brief Set the board to the given position, resetting per-game transient state (see definition).
    void SetPosition(std::string_view fen_string);

    /// @brief Set the board to the standard initial position (overload of SetPosition).
    void SetPosition();

    /// @brief Play a move given in algebraic notation (see definition for details).
    Move PlayMove(std::string_view move_string);

    /// @brief Play a move (see definition for details).
    void PlayMove(Move move);

    /// @brief Play a null (pass) move.
    void MakeNullMove();

    /// @brief Undo the most recent move.
    void UndoMove();

    /// @brief Start searching for a move on the worker thread.
    void StartThinking();

    /// @brief Signal the search to stop and join the worker thread.
    void StopThinking();

    /// @brief Search the current position and return the best move (see definition in search_root_node.cpp).
    /// Returns a book move immediately on a book hit; otherwise runs the Lazy SMP iterative-deepening search.
    /// @return The best move, or Move::None if there are no legal moves.
    Move SearchRootNode();

  private:
    void        SearchThreadEntry(); ///< Entry point of the search worker thread.
    std::thread worker_thread_;      ///< Worker thread for searching moves.

  public:
    std::vector<Position> positions_;    ///< Game position history (grows with the game; no fixed cap).
    nnue::Network         nnue_network_; ///< NNUE network instance (loaded via EvalFile; read-only in search).
};

// ---- Definitions moved from game.cpp (header-only). SearchRootNode/SearchThreadEntry's search half
//      needs SearchState, so SearchRootNode lives in the search_state.h hub; the rest are inline here. ----

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
    // `stop` sets is_cancel_pending. For a normal (non-ponder) search is_pondering is false, so this is a no-op.
    while (is_pondering_.load(std::memory_order_relaxed) && !is_cancel_pending_.load(std::memory_order_relaxed))
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
/// Note: this does NOT clear is_pondering — handle_go sets it just before calling StartThinking (which calls
/// StopThinking), so clearing it here would clobber the flag for the search about to start. The ponder-wait
/// in SearchThreadEntry exits on is_cancel_pending (set below), so a `stop` during a ponder still works.
inline void Game::StopThinking()
{
    is_cancel_pending_.store(true, std::memory_order_relaxed);
    if (worker_thread_.joinable())
    {
        worker_thread_.join();
    }
}

/// @brief Start thinking on the worker thread.
inline void Game::StartThinking()
{
    StopThinking();
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

// Header-only: Game::SearchRootNode (and the rest of the search) needs SearchState complete, so it is
// defined in the search_state.h hub. Pull the hub in here, at the very bottom — after class Game is fully
// defined — so that simply including game.h yields a complete, searchable Game. The include is guarded by
// pragma once and works in either order: if a TU includes search_state.h first, search_state.h includes
// game.h (whose trailing include of search_state.h is then skipped, the body already in progress); if a TU
// includes game.h first, this line drives search_state.h, which sees the now-complete Game.
#include "search_state.h"
