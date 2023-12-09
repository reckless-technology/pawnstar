#include "pawnstar.h"

/*
Search a single move and return its score, or MOVED_INTO_CHECK_SCORE if the 
move was not legal.
*/
int 
SearchSingleMove(const Position*    position, 
                 int                depth, 
                 int                ply, 
                 int                alpha, 
                 int                beta, 
                 volatile bool*     cancel, 
                 Move               move,
                 Variation*         pv,
                 int                move_index)
{      
    if (*cancel)
    {
        return SEARCH_CANCELLED_SCORE;
    }
    Position child_position;
    MakeMove(&child_position, position, move);
    if (child_position.flags & IS_MOVED_INTO_CHECK)
    {
        return MOVED_INTO_CHECK_SCORE;
    }
    int score;
    if (beta > alpha + 1 && move_index > 0)
    {
        INCREMENT("pvs attempts");
        score = -Search(&child_position, depth - 1, ply + 1, -alpha - 1, -alpha, cancel, pv);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            score = -Search(&child_position, depth - 1, ply + 1, -beta, -alpha, cancel, pv);
        }
    }
    else
    {
        score = -Search(&child_position, depth - 1, ply + 1, -beta, -alpha, cancel, pv);
    }
    return score;
}