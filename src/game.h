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

    Game() : transposition_table{kHashtableMegabytes}, quiescent_table{kQHashtableMb}, is_cancel_pending{false}
    {
        NewGame();
    }

    Position &CurrentPosition()
    {
        return positions_.back();
    }
    const Position &CurrentPosition() const
    {
        return positions_.back();
    }
    /// @brief Read-only view of the full game position stack, used to seed SearchState.
    const StackList<Position, 256> &Positions() const
    {
        return positions_;
    }

    void NewGame(std::string_view fen_string);
    void NewGame();
    Move PlayMove(std::string_view move_string);
    void PlayMove(Move move);
    void MakeNullMove();
    void UndoMove();
    void StartThinking();
    void StopThinking();
    bool IsGameOver() const;
    bool IsDrawByRepetition() const;
    bool IsDrawByFiftyMoves() const;

  private:
    void                     SearchThreadEntry(); ///< Entry point of the search worker thread.
    std::thread              worker_thread_;      ///< Worker thread for searching moves.
    StackList<Position, 256> positions_;          ///< Game position history stack.
};
