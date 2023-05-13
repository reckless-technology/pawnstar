#include "pawnstar.h"

/*
Search a single move and return its score, or MOVED_INTO_CHECK_SCORE if the 
move was not legal.
*/
int 
SearchSingleMove(const Position*    src_position, 
                 int                depth, 
                 int                ply, 
                 int                alpha, 
                 int                beta, 
                 volatile bool*     cancel, 
                 int                move,
                 Variation*         pv,
                 int                move_index)
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

    /*
    Extend the search depth in the following cases: 
        # This is a pawn promotion
        # This is a pawn push to the 7th rank 
        # Recapture of same value piece
    */

    if (MOVE_PROMOTED(move))
    {
        INCREMENT("extensions promotion");
        ++child_depth;
    }


    int score;
    if (beta > alpha + 1 &&
        move_index       &&
       !(src_position->state_flags & IS_CHECK))
    {
        INCREMENT("pvs attempts");
        score = -Search(&position, child_depth, ply + 1, -alpha - 1, -alpha, cancel, pv);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            score = -Search(&position, child_depth, ply + 1, -beta, -alpha, cancel, pv);
        }
    }
    else
    {
        score = -Search(&position, child_depth, ply + 1, -beta, -alpha, cancel, pv);
    }
    return score;
}