#include "pawnstar.h"

#define MOVE_MASK 0x7FFF /* piece, from, to only */

/*
The good_move_counts array maintains a count, indexed for each ply of search, 
moving piece type, source and destination square (4 dimensions), how many  
times that move has raised alpha or caused a cutoff in all the various 
subtrees at that ply (depth from root node). 

This is based on the premise that moves which are good in one subtree might 
well be also good in another subtree, and thus moves which have caused more 
cutoffs should generally be preferred over moves which have caused fewer 
cutoffs. It's sort of a generalization of the history table and killer move 
concepts. Whilst this assumption is questionable in theory, it has been 
experimentally confirmed that it does indeed measurably speed up the search 
on a wide variety of positions.
*/
static int good_move_counts[MAX_PLY][8 * 64 * 64];

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
 * @brief Sort moves "best first" using a stable merge sort.
 * A stable sort is required so that root node moves with equal alpha score
 * have their relative sequence preserved.
 * @param position position for which the moves were generated
 * @param moves null terminated list of moves to be sorted
 * @param ply distance from root node
*/
void 
SortMoves(const Position* position,
          Move            moves[], 
          int             ply)
{   
    Move* move;
    const int* const counts = &good_move_counts[ply][0];
    for (move = moves; *move; ++move)
    {
        /* 
        Assign provisional scores based on static exchange evaluation
        and how many cutoffs this move has caused in the search history.
        */
        *move = SCORED_MOVE(*move, EvaluateStaticExchange(position, *move) * 1000 + counts[*move & MOVE_MASK]);
    }
    MergeSort((int)(move - moves), moves);
}

#define min(a,b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})

/**
 * @brief Sort moves best first.
 * Uses iterative, stable merge sort algorithm.
 * Sort must be stable to preserve move ordering at the root node.
 * @param num_elements number of elements to sort
 * @param values pointer to the elements
*/
void 
MergeSort(int   num_elements, 
          Move  values[])
{
    Move work[MAX_MOVES_PER_POSITION];
    Move* merge_src = values;
    Move* merge_dst = work;
    for (int width = 1; width < num_elements; width *= 2)
    {
        for (int begin = 0; begin < num_elements; begin += width * 2)
        {
            const Move* const mid = &merge_src[min(begin + width,     num_elements)];
            const Move* const end = &merge_src[min(begin + width * 2, num_elements)];
            const Move* i         = &merge_src[begin];
            const Move* j         = mid;
            Move* dst             = &merge_dst[begin];
            while (i < mid && j < end)
            {
                *dst++ = (MOVE_SCORE(*i) >= MOVE_SCORE(*j)) ? *i++ : *j++;
            }
            while (i < mid)
            {
                *dst++ = *i++;
            }
            while (j < end)
            {
                *dst++ = *j++;
            }
        }
        Move* tmp = merge_src;
        merge_src = merge_dst;
        merge_dst = tmp;
    }
    if (merge_dst == values)
    {
        memcpy(values, work, num_elements * sizeof(Move));
    }
}
