#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <vector>

#include "debug_hashtable.h"
#include "move.h"
#include "search.h"
#include "sort_moves.h"
#include "static_exchange_evaluation.h"

//                   indexed by      ply   from  to
static uint32_t killer_move_counts[MAX_PLY][64 * 64];

void RecordKillerMove(int ply, Move move)
{
    // Mask all except from to
    ++killer_move_counts[ply][move.from_to_mask()];
}

void ResetKillerCounts()
{
    memset(killer_move_counts, 0, sizeof(killer_move_counts));
}

/// @brief Assign provisional scores to each move and then sort them best first.
/// @param game Game being searched
/// @param moves List of legal moves to evaluate.
/// @param depth Current search depth.
/// @param ply Current search ply.
/// @param alpha Alpha value
/// @param beta Beta value
void ScoreAndSortMoves(Game &game, MoveList &moves, int depth, int ply, int alpha, int beta)
{
    const uint32_t *const counts = &killer_move_counts[ply][0];
    if (depth > 4)
    {
        Variation dummy_pv{};
        for (std::size_t i = 0; i != moves.size(); ++i)
        {
            bool      is_checking;
            const int score = SearchSingleMove(game, depth - 4, ply, alpha, beta, moves[i], dummy_pv, i, is_checking);
            if (is_checking)
            {
                moves[i].GivesCheck();
                // Give a bonus score to moves which give check.
                moves[i].AssignScore(score * 1000 + counts[moves[i].from_to_mask()] + 50000);
            }
            else
            {
                moves[i].AssignScore(score * 1000 + counts[moves[i].from_to_mask()]);
            }
        }
    }
    else
    {
        for (Move &move : moves)
        {
            bool      is_checking;
            const int score = EvaluateStaticExchange(game.CurrentPosition(), move, is_checking);
            if (is_checking)
            {
                move.GivesCheck();
                move.AssignScore(score * 1000 + counts[move.from_to_mask()] + 50000);
            }
            else
            {
                move.AssignScore(score * 1000 + counts[move.from_to_mask()]);
            }
        }
    }
    SortMoves<false>(moves);
}

/// @brief Get the maximum count in the killer move table
/// @return Count of the most popular killer move
uint32_t MaxKillerMoveCount()
{
    uint32_t result = 0;
    for (int i = 0; i != MAX_PLY; ++i)
    {
        for (int j = 0; j != 64 * 64; ++j)
        {
            if (killer_move_counts[i][j] > result)
            {
                result = killer_move_counts[i][j];
            }
        }
    }
    return result;
}