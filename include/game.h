#pragma once

#include <array>
#include <string_view>
#include <thread>
#include <vector>

#include "chess_clock.h"
#include "constants.h"
#include "history_table.h"
#include "position.h"
#include "transposition_table.h"

/// @brief Class to represent a game of chess.
class Game
{
  public:
    TranspositionTable transposition_table; ///< The transposition table.
    HistoryTable       history_table;       ///< The history table.
    TimeControl        time_control;        ///< Clock controls for the current game.
    int                node_count;          ///< Number of nodes (positions) during search.
    bool               is_cancel_pending;   ///< Set to true when time for this search is expired.

    Game() : transposition_table{HASHTABLE_MEGABYTES}
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

    /// @brief Assign provisional scores to each move and then sort them best first.
    /// Use MVV/LVA plus history table counts to score moves. This is primitive, but way faster than static exchange
    /// evaluation, and seems to work pretty well in practice.
    /// @param game Game being searched
    /// @param moves List of legal moves to evaluate.
    /// @param ply Current search ply.
    constexpr void ScoreAndSortMoves(MoveList &moves, int ply)
    {

        const Position &position = CurrentPosition();
        for (Move &move : moves)
        {
            move.AssignScore(piece_values[position.PieceAt(move.to())] * 10000 -
                             piece_values[position.PieceAt(move.from())] * 1000 + history_table.GetCount(ply, move));
        }
        SortMoves<false>(moves);
    }

  private:
    void                     SearchThreadEntry(); ///< Entry point of worker thread.
    std::thread              worker_thread_;      ///< Worker thread for searching moves.
    StackList<Position, 256> positions_;          ///< Position stack.

    /// Raw material values for MVV/LVA provisional scoring of moves.
    static constexpr std::array<int, 7> piece_values{0, 100, 300, 300, 500, 900, 10000};
};