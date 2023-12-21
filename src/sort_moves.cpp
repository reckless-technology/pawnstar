#include <algorithm>
#include <cstring>
#include <vector>
#include <cinttypes>

#include "move.h"
#include "debug_hashtable.h"
#include "function_prototypes.h"

/*                   indexed by      ply   pce from  to */
static uint32_t killer_move_counts[MAX_PLY][8 * 64 * 64];

void
RecordKillerMove(int ply, Move move)
{
    /* Mask all except piece, from, to (lower 15 bits) */
    ++killer_move_counts[ply][move & 0x7FFF];
}

void 
ResetKillerCounts()
{
    memset(killer_move_counts, 0, sizeof(killer_move_counts));
}

/**
 * @brief Sort moves "best first".
 * Uses static exchange evaluation (SEE) to provide a provisional
 * score for each move for sorting.
 * Removes moves found to be illegal during the scoring process.
 * @param position position for which the moves were generated
 * @param moves list of moves to be sorted
 * @param ply distance from root node
 * @param depth remaining search depth (unused)
*/
void 
ScoreAndSortMoves(const Position&   position,
                  MoveList&         moves, 
                  int               ply,
                  int               depth)
{   
    MoveList legal_moves {};
    legal_moves.reserve(64);
    const uint32_t* const counts = &killer_move_counts[ply][0];
    for (const auto& move : moves)
    {
        /* 
        Assign provisional scores based on static exchange evaluation
        and how many cutoffs this move has caused in the search history.
        */
        bool is_checking;
        const int see_score = EvaluateStaticExchange(position, move, is_checking);
        if (see_score == MOVED_INTO_CHECK_SCORE)
        {
            continue;
        }
        if (is_checking)
        {
            legal_moves.push_back(ScoredCheckingMove(move, see_score * 1000 + counts[move & 0x7FFF]));
        }
        else
        {
            legal_moves.push_back(ScoredMove(move, see_score * 1000 + counts[move & 0x7FFF]));
        }
    }
    moves.swap(legal_moves);
    SortMoves(moves, false);
    (void)depth;
}

/**
 * @brief Sort moves "best first" i.e. in descending score order
 * @param moves Moves to be sorted
 * @param is_stable_sort true for slower stable sort (required at root node search only)
 */
void
SortMoves(MoveList& moves, bool is_stable_sort)
{
    if (is_stable_sort)
    {
       std::stable_sort 
            (moves.rbegin(), moves.rend(), 
            [](const Move& a, const Move& b){ return MoveScore(a) < MoveScore(b); } );
    }
    else
    {
        std::sort 
            (moves.rbegin(), moves.rend(), 
            [](const Move& a, const Move& b){ return MoveScore(a) < MoveScore(b); } );
    }
}