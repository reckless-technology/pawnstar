#pragma once

#include <array>
#include <string_view>
#include <thread>
#include <vector>

#include "chess_clock.h"
#include "constants.h"
#include "history_table.h"
#include "opening_book.h"
#include "position.h"
#include "transposition_table.h"

/// @brief Class to represent a game of chess.
class Game
{
  public:
    TranspositionTable transposition_table; ///< The transposition table.
    TranspositionTable quiescent_table;     ///< Special TT for quiescence search.
    HistoryTable       history_table;       ///< The history table.
    TimeControl        time_control;        ///< Clock controls for the current game.
    OpeningBook        book;                ///< The opening book.
    int                node_count;          ///< Number of nodes (positions) during search.
    bool               is_cancel_pending;   ///< Set to true when time for this search is expired.

    Game() : transposition_table{HASHTABLE_MEGABYTES}, quiescent_table{Q_HASHTABLE_MB}
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
    void ScoreAndSortMoves(MoveList &moves, int ply);

  private:
    void                     SearchThreadEntry(); ///< Entry point of worker thread.
    std::thread              worker_thread_;      ///< Worker thread for searching moves.
    StackList<Position, 256> positions_;          ///< Position stack.

    /// Raw material values for MVV/LVA provisional scoring of moves.
    static constexpr std::array<int, 7> piece_values{0, 100, 300, 300, 500, 900, 10000};
};