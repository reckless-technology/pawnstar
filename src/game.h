#pragma once

#include <array>
#include <atomic>
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
    TranspositionTable transposition_table; ///< The transposition table (shared by all Lazy SMP threads).
    EvalCache          eval_cache;          ///< Lockless NNUE evaluation cache (shared by all threads).
    ChessClock         time_control;        ///< Clock controls for the current game.
    OpeningBook        book;                ///< The opening book.
    std::atomic<bool>  is_cancel_pending;   ///< Shared stop flag: set on timeout, UCI stop, or search completion.
    int thread_count; ///< Lazy SMP search threads; default from PAWNSTAR_THREADS / hardware, set by UCI `Threads`.
    ChessClock::Duration move_overhead{
        ChessClock::Duration{10}}; ///< Reserved from each hard deadline for GUI/network lag (UCI `Move Overhead`, ms).
                                   ///< Persists across games.
    std::atomic<bool> is_pondering{
        false}; ///< True while a `go ponder` search runs (until `ponderhit` / `stop`); suppresses time stops.
    Move ponder_move{
        Move::None()}; ///< Predicted opponent reply (PV[1]); emitted as `bestmove <m> ponder <ponder_move>`.

    /// @brief Construct a game, sizing the transposition tables and starting from the initial position.
    Game()
        : transposition_table{kHashtableMegabytes}, eval_cache{kEvalCacheMb}, is_cancel_pending{false},
          thread_count{ComputeDefaultThreads()}
    {
        NewGame();
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

    /// @brief Read-only view of the full game position stack, used to seed SearchState.
    /// @return The position history stack.
    const std::vector<Position> &Positions() const
    {
        return positions_;
    }

    /// @brief The NNUE network owned by this game (shared read-only by all search threads after loading).
    /// @return Reference to the network.
    nnue::Network &NnueNetwork()
    {
        return nnue_network_;
    }

    /// @brief The NNUE network (const overload).
    /// @return Const reference to the network.
    const nnue::Network &NnueNetwork() const
    {
        return nnue_network_;
    }

    /// @brief Start a new game from the given position (see definition for details).
    void NewGame(std::string_view fen_string);

    /// @brief Start a new game from the standard initial position.
    void NewGame();

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
    void                  SearchThreadEntry(); ///< Entry point of the search worker thread.
    std::thread           worker_thread_;      ///< Worker thread for searching moves.
    std::vector<Position> positions_;          ///< Game position history (grows with the game; no fixed cap).
    nnue::Network         nnue_network_;       ///< NNUE network instance (loaded via EvalFile; read-only in search).
};
