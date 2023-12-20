#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"
#include "search.h"
#include "game.h"

/**
 * @brief Search a single move and return its alpha beta score.
 * @param game Game we are searching
 * @param depth Depth to search to
 * @param ply Distance from root node
 * @param alpha Floor value
 * @param beta Opponents floor value
 * @param move Move to search
 * @param pv Parent's principal variation
 * @param move_index Move number (0 is first move)
 * @return score for this move
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
    game.PlayMove(move);
    int score;
    if (beta > alpha + 1    && 
        move_index > 0      && 
        !(game.position_->flags_ & IS_CHECK))
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