#include "pawnstar.h"
/******************************************************************************
Moves are contained within the least significant 24 bits of an integer

  Bits      Interpretation
  
 0 -  5     to (destination square index)
 6 - 11     from (source square index)
12 - 14     moving piece type
15 - 17     captured piece type in the case of capture moves
18 - 20     promoted piece type in the case of pawn promotions
21 - 23     move type

A value of 0 terminates a move list

The good_move_counts array maintains a count, indexed for each ply of search, 
moving piece type, source and destination square (4 dimensions), how many  
times that move has raised alpha or caused a cutoff in all the various 
subtrees at that ply (depth from root node). 

This is based on the premise that moves which are good in one subtree might 
well be also good in another subtree, and thus moves which have caused more 
cutoffs should generally be preferred over moves which have caused fewer 
cutoffs. It's sort of my generalization of the history table and killer move 
concepts. Whilst this assumption is questionable in theory, it has been 
empirically confirmed that it does indeed measurably speed up the search on a 
wide variety of positions.
*******************************************************************************/
#define MIN(a,b)    ((a) < (b) ? (a) : (b))
#define MOVE_MASK   0x7FFF                  // piece, from, to fields only
#define MERIT(move) ((move) & 0x1F8000)     // promoted and captured material only

static int good_move_counts[MAX_PLY][8 * 64 * 64];
/******************************************************************************
This function gets called by the search when we find an interesting move, i.e.
one which raises alpha or causes a beta cutoff. 
*******************************************************************************/
void RecordGoodMove(int ply, int move)
{
    _InterlockedIncrement(&good_move_counts[ply][move & MOVE_MASK]);
}

bool HasMoveBeenGood(int ply, int move)
{
    return !!good_move_counts[ply][move & MOVE_MASK];
}
/******************************************************************************
The valuable move counts table is reset before the start of each search.
*******************************************************************************/
void InitializeGoodMoveCounts(void)
{
    memset(good_move_counts, 0, sizeof(good_move_counts));
}
/******************************************************************************
Sort moves into "best first" order. The heuristic for ranking moves is:
# Promoted material value, then
# Captured material value, then
# Total number of times this move has cutoff or raised alpha at this ply
*******************************************************************************/
void SortMoves(int moves[], int ply)
{   
    int i;
    ScoredMove scored_moves[MAX_MOVES_PER_POSITION];    
    const int* const counts = &good_move_counts[ply][0];
    int move;
    for (i = 0; (move = moves[i]) != 0; ++i)
    {
        scored_moves[i].move  = move;
        scored_moves[i].score = MERIT(move) + counts[move & MOVE_MASK];
    }
    if (i < 2)
    {
        return;
    }
    MergeSort(i, scored_moves);
    for ( --i; i >= 0; --i)
    {
        moves[i] = scored_moves[i].move;
    }
}
/******************************************************************************
Sort an array of scored moves into best first (descending score) order.
This uses a stable merge sort algorithm, and is preferred over the use of the 
built in qsort because it avoids the overhead of the comparison predicate 
function call, which is non trivial in C. A stable sort is required so that the
sorting of moves at the root node, where many moves share the same alpha, is
preserved through multiple iterations.
*******************************************************************************/
void MergeSort(int num_elements, ScoredMove values[])
{
    ScoredMove work[MAX_MOVES_PER_POSITION];
    int width;
    ScoredMove* merge_src = values;
    ScoredMove* merge_dst = work;
    ScoredMove* tmp;
    for (width = 1; width < num_elements; width *= 2)
    {
        const int twice_width = width * 2;
        int left;
        for (left = 0; left < num_elements; left += twice_width)
        {
            /******************************************************************
            Merge the 2 sub-lists:
            merge_src[left:left+width], merge_src[left+width:left+twice_width]
            to:
            merge_dst[left:left+twice_width]
            *******************************************************************/
            const int right = MIN(left + width,       num_elements);
            const int end   = MIN(left + twice_width, num_elements);
            int dst = left;
            int j   = left;
            int k   = right;                    
            while (j < right && k < end)
            {
                if (merge_src[j].score >= merge_src[k].score)
                {
                    merge_dst[dst++] = merge_src[j++];
                }
                else
                {
                    merge_dst[dst++] = merge_src[k++];
                }
            }
            while (j < right)
            {
                merge_dst[dst++] = merge_src[j++];
            }
            while (k < end)
            {
                merge_dst[dst++] = merge_src[k++];
            }
        }
        /**********************************************************************
        Toggle merge source and dest between successive runs to avoid copy
        overhead.
        ***********************************************************************/
        tmp       = merge_src;
        merge_src = merge_dst;
        merge_dst = tmp;
    }
    /**************************************************************************
    If that last merge was to the work array, copy it back to the values array
    ***************************************************************************/
    if (merge_dst == values)
    {
        memcpy(values, work, num_elements * sizeof(ScoredMove));
    }
}