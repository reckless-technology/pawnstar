#include <algorithm>
#include <cstring>
#include <vector>

#include "move.h"
#include "debug_hashtable.h"
#include "function_prototypes.h"

#define MOVE_MASK 0xFFF /* from, to only */

static int good_move_counts[MAX_PLY][64 * 64];

/*
This function gets called by the search when we find an "interesting" move, 
i.e. one which raises alpha or causes a beta cutoff. 
*/
void 
RecordGoodMove(int  ply, 
               Move move)
{
    INCREMENT("good moves");
    ++good_move_counts[ply][move & MOVE_MASK];
}

/*
The valuable move counts table is reset before the start of each search.
*/
void InitializeGoodMoveCounts(void)
{
    memset(good_move_counts, 0, sizeof(good_move_counts));
}

/**
 * @brief Sort moves "best first".
 * Uses static exchange evaluation (SEE) to provide a provisional
 * score for each move for sorting.
 * @param position position for which the moves were generated
 * @param moves null terminated list of moves to be sorted
 * @param ply distance from root node
*/
void 
ScoreAndSortMoves(const Position&       position,
                  MoveList&    moves, 
                  int                   ply,
                  int                   depth)
{   
    const int* const counts = &good_move_counts[ply][0];
    for (auto& move : moves)
    {
        /* 
        Assign provisional scores based on static exchange evaluation
        and how many cutoffs this move has caused in the search history.
        */
        move = ScoredMove(move, EvaluateStaticExchange(position, move) * 1000 + counts[move & MOVE_MASK]);
    }
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
