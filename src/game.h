#pragma once

#include <array>
#include <atomic>
#include <string_view>
#include <thread>
#include <vector>

#include "chess_clock.h"
#include "constants.h"
#include "history_table.h"
#include "opening_book.h"
#include "position.h"
#include "search_state_pool.h"
#include "thread_pool.h"
#include "transposition_table.h"

/// @brief Class to represent a game of chess.
class Game
{
  public:
    TranspositionTable transposition_table; ///< The transposition table.
    TranspositionTable quiescent_table;     ///< Special TT for quiescence search.
    HistoryTable       history_table;       ///< The history table.
    ChessClock         time_control;        ///< Clock controls for the current game.
    OpeningBook        book;                ///< The opening book.
    std::atomic<bool>  is_cancel_pending;   ///< Set to true when search time is expired.
    ThreadPool         thread_pool;         ///< Worker thread pool for parallel search.
    SearchStatePool    state_pool;          ///< Slab allocator for per-worker search states.

    /// @brief Construct a game, sizing the transposition tables and starting from the initial position.
    Game() : transposition_table{kHashtableMegabytes}, quiescent_table{kQHashtableMb}, is_cancel_pending{false}
    {
        NewGame();
    }

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
    const StackList<Position, kMaxGameLength> &Positions() const
    {
        return positions_;
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
    /// @brief Whether the game has ended (checkmate, stalemate or draw).
    /// @return true if the game is over.
    bool IsGameOver() const;
    /// @brief Whether the current position is a draw by threefold repetition.
    /// @return true if drawn by repetition.
    bool IsDrawByRepetition() const;
    /// @brief Whether the current position is a draw by the fifty-move rule.
    /// @return true if drawn by the fifty-move rule.
    bool IsDrawByFiftyMoves() const;

  private:
    void                     SearchThreadEntry(); ///< Entry point of the search worker thread.
    std::thread              worker_thread_;      ///< Worker thread for searching moves.
    StackList<Position, kMaxGameLength> positions_; ///< Game position history stack.
};
