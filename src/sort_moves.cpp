#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <vector>

#include "debug_hashtable.h"
#include "move.h"
#include "sort_moves.h"
#include "static_exchange_evaluation.h"

//                   indexed by      ply   pce from  to
static uint32_t killer_move_counts[MAX_PLY][8 * 64 * 64];

void RecordKillerMove(int ply, Move move)
{
    // Mask all except piece, from, to (lower 15 bits)
    ++killer_move_counts[ply][move.piece_from_to_mask()];
}

void ResetKillerCounts()
{
    memset(killer_move_counts, 0, sizeof(killer_move_counts));
}

/// @brief Sort moves "best first".
/// Uses static exchange evaluation (SEE) to provide a provisional score for each move for sorting.
/// @param position position for which the moves were generated
/// @param moves list of moves to be sorted
/// @param ply distance from root node
/// @param depth remaining search depth (unused)
void ScoreAndSortMoves(const Position &position, MoveList &moves, int ply, int depth)
{
    const uint32_t *const counts = &killer_move_counts[ply][0];
    for (Move &move : moves)
    {
        // Assign provisional scores based on static exchange evaluation and how many cutoffs this move has caused in
        // the search history.
        bool      is_checking;
        const int see_score = EvaluateStaticExchange(position, move, is_checking);
        move.AssignScore(see_score * 1000 + counts[move.piece_from_to_mask()]);
        if (is_checking)
        {
            move.GivesCheck();
        }
    }
    SortMoves<false>(moves);
    (void)depth;
}

/// @brief Get the maximum count in the killer move table
/// @return Count of the most popular killer move
uint32_t MaxKillerMoveCount()
{
    uint32_t result = 0;
    for (int i = 0; i != MAX_PLY; ++i)
    {
        for (int j = 0; j != 32768; ++j)
        {
            if (killer_move_counts[i][j] > result)
            {
                result = killer_move_counts[i][j];
            }
        }
    }
    return result;
}