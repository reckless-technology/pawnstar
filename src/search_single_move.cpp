#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"
#include "search.h"
#include "game.h"

/*
Search a single move and return its score, or MOVED_INTO_CHECK_SCORE if the 
move was not legal.
*/
int 
SearchSingleMove(Game&      game, 
                 int        depth, 
                 int        ply, 
                 int        alpha, 
                 int        beta, 
                 Move       move,
                 Variation& pv,
                 int        move_index)
{      
    if (game.is_cancel_pending)
    {
        return SEARCH_CANCELLED_SCORE;
    }
    game.PlayMove(move);
    if (game.position->flags_ & IS_MOVED_INTO_CHECK)
    {
        game.UndoMove();
        return MOVED_INTO_CHECK_SCORE;
    }
    int score;
    if (beta > alpha + 1 && move_index > 0)
    {
        INCREMENT("pvs attempts");
        score = -Search(game, depth - 1, ply + 1, -alpha - 1, -alpha, pv);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            score = -Search(game, depth - 1, ply + 1, -beta, -alpha, pv);
        }
    }
    else
    {
        score = -Search(game, depth - 1, ply + 1, -beta, -alpha, pv);
    }
    game.UndoMove();
    return score;
}