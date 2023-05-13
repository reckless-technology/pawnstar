#include "pawnstar.h"
/*
Moves are contained within the least significant 22 bits of an integer

  Bits      Interpretation
  
 0 -  5     to (destination square index)
 6 - 11     from (source square index)
12 - 14     moving piece type
15 - 17     captured piece type in the case of capture moves
18 - 20     promoted piece type in the case of pawn promotions
21 - 21     special flag (en passant capture or castling)

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
*/

#define MOVE_MASK   0x7FFF                  // piece, from, to fields only
#define MERIT(move) ((move) & 0x1F8000)     // promoted and captured material only

static const int MATERIAL_VALUE[7] = {
    [PAWN]      = 100,
    [KNIGHT]    = 300,
    [BISHOP]    = 300,
    [ROOK]      = 500,
    [QUEEN]     = 900,
    [KING]      = 0,
};


static int good_move_counts[MAX_PLY][8 * 64 * 64];
/*
This function gets called by the search when we find an interesting move, i.e.
one which raises alpha or causes a beta cutoff. 
*/
void 
RecordGoodMove(int ply, 
               int move)
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
 * @param use_see true to use static exchange evaluation for scoring
 *                false to use the number of cutoffs / alpha raises in the tree
*/
void 
SortMoves(const Position* position,
          int moves[], 
          int ply,
          bool use_see)
{   
    int i;
    ScoredMove scored_moves[MAX_MOVES_PER_POSITION];    
    const int* const counts = &good_move_counts[ply][0];
    int move;
    if (use_see)
    {
        for (i = 0; (move = moves[i]) != 0; ++i)
        {
            scored_moves[i].move  = move;
            scored_moves[i].score = EvaluateStaticExchange(position, move);
        }
    }
    else
    {
        for (i = 0; (move = moves[i]) != 0; ++i)
        {
            scored_moves[i].move  = move;
            scored_moves[i].score = MERIT(move) + counts[move & MOVE_MASK];
        }
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
    (void)ply;
    (void)counts;
}

/**
 * @brief Stable merge sort of scored moves.
 * @param num_elements number of elements to sort
 * @param values pointer to the elements
*/
void 
MergeSort(int        num_elements, 
          ScoredMove values[])
{
    ScoredMove work[MAX_MOVES_PER_POSITION];
    ScoredMove* merge_src = values;
    ScoredMove* merge_dst = work;
    ScoredMove* tmp;
    for (int width = 1; width < num_elements; width *= 2)
    {
        for (int left = 0; left < num_elements; left += width * 2)
        {
            ScoredMove* dst         = &merge_dst[left];
            const ScoredMove* right = &merge_src[min(left + width,     num_elements)];
            const ScoredMove* end   = &merge_src[min(left + width * 2, num_elements)];           
            const ScoredMove* j     = &merge_src[left];
            const ScoredMove* k     = right;                    
            while (j < right && k < end)
            {
                if (j->score >= k->score)
                {
                    *dst++ = *j++;
                }
                else
                {
                    *dst++ = *k++;
                }
            }
            while (j < right)
            {
                *dst++ = *j++;
            }
            while (k < end)
            {
                *dst++ = *k++;
            }
        }
        tmp       = merge_src;
        merge_src = merge_dst;
        merge_dst = tmp;
    }
    /*
    If that last merge was to the work array, copy it back to the values array
    */
    if (merge_dst == values)
    {
        memcpy(values, work, num_elements * sizeof(ScoredMove));
    }
}