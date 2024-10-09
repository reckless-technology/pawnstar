#pragma once
#include "game.h"
#include "move.h"

#include <algorithm>

class Position;

void     RecordKillerMove(int ply, Move move);
void     ResetKillerCounts();
void     ScoreAndSortMoves(Game &game, MoveList &moves, int depth, int ply, int alpha, int beta);
uint32_t MaxKillerMoveCount(void);

/// @brief Sort moves best first i.e. in descending score order
/// @param moves Moves to be sorted
/// @tparam is_stable_sort true for slower stable sort (required at root node search only)
template <bool is_stable_sort> void SortMoves(MoveList &moves)
{
    if constexpr (is_stable_sort)
    {
        std::stable_sort(moves.begin(), moves.end(),
                         [](const Move &a, const Move &b) { return a.score() > b.score(); });
    }
    else
    {
        std::sort(moves.begin(), moves.end(), [](const Move &a, const Move &b) { return a.score() > b.score(); });
    }
}