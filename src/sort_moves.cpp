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

/// @brief Assign provisional scores to each mover and then sort them best first.
/// @param game Game being searched
/// @param moves List of legal moves to evaluate.
/// @param depth Current search depth.
/// @param ply Current search ply.
/// @param alpha Alpha value
/// @param beta Beta value
void ScoreAndSortMoves(Game &game, MoveList &moves, int depth, int ply, int alpha, int beta)
{
    const uint32_t *const counts = &killer_move_counts[ply][0];
    if (depth > 3)
    {
        Variation dummy_pv{};
        for (std::size_t i = 0; i != moves.size(); ++i)
        {
            bool      is_checking;
            const int score = SearchSingleMove(game, depth - 3, ply, alpha, beta, moves[i], dummy_pv, i, is_checking);
            moves[i].AssignScore(score * 1000 + counts[moves[i].piece_from_to_mask()]);
            if (is_checking)
            {
                moves[i].GivesCheck();
            }
        }
    }
    else
    {
        for (Move &move : moves)
        {
            bool      is_checking;
            const int score = EvaluateStaticExchange(game.CurrentPosition(), move, is_checking);
            move.AssignScore(score * 1000 + counts[move.piece_from_to_mask()]);
            if (is_checking)
            {
                move.GivesCheck();
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