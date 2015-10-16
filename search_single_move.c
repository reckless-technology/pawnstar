#include "pawnstar.h"

/******************************************************************************
Search a single move and return its score, or MOVED_INTO_CHECK_SCORE if the 
move was not legal.
*******************************************************************************/
int 
SearchSingleMove(const Position*    src_position, 
                 int                depth, 
                 int                ply, 
                 int                alpha, 
                 int                beta, 
                 volatile bool*     cancel, 
                 int                search_flags, 
                 int                move,
                 Variation*         pv)
{      
    if (*cancel)
    {
        return ILLEGAL_SCORE;
    }
    Position position;
    MakeMove(&position, src_position, move);
    if (position.state_flags & MOVED_INTO_CHECK)
    {
        return MOVED_INTO_CHECK_SCORE;
    }
    int child_depth = depth - 1;
    /******************************************************************
    Extend the search depth if any of the following are true:    
    # We have been following the principal variation from the root node
    # This is a pawn promotion
    # This is a pawn push to the 7th rank 
    # Recapture of same value piece
    *******************************************************************/
    if ((search_flags & IS_FOLLOWING_PV) && depth == 1)
    {
        INCREMENT("extensions following PV");
        ++child_depth;
    }

    if (MOVE_PROMOTED(move))
    {
        INCREMENT("extensions promotion");
        ++child_depth;
    }

#if DO_PUSH_TO_SEVENTH_RANK_EXTENSION
    static const int SEVENTH_RANK[2] = { 6, 1 };
    if (MOVE_PIECE(move) == PAWN && 
        RANK_OF(MOVE_TO(move)) == SEVENTH_RANK[COLOR_TO_MOVE(src_position)])
    {
        INCREMENT("extensions push to 7th");
        ++child_depth;
    }
#endif

#if DO_RECAPTURE_EXTENSION
    static const int CLASSICAL_MATERIAL[7] = { 0, 1, 3, 3, 5, 9, 0 };
    if (MOVE_CAPTURED(move)                          && 
        MOVE_TO(move) == MOVE_TO(src_position->move) &&
        CLASSICAL_MATERIAL[MOVE_CAPTURED(move)] == CLASSICAL_MATERIAL[MOVE_CAPTURED(src_position->move)])
    {
        INCREMENT("extensions recapture");
        ++child_depth;
    }
#endif
 
    int score;
    if (beta > alpha + 1           &&
        (search_flags & IS_PVS_OK) && 
        !(src_position->state_flags & IS_CHECK))
    {
        INCREMENT("pvs attempts");
        score = -Search(&position, child_depth, ply + 1, -alpha - 1, -alpha, cancel, search_flags, pv);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            score = -Search(&position, child_depth, ply + 1, -beta, -alpha, cancel, search_flags, pv);
        }
    }
    else
    {
        score = -Search(&position, child_depth, ply + 1, -beta, -alpha, cancel, search_flags, pv);
    }
    return score;
}