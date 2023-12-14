#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"
#include "game.h"
#include "search.h"

/**
 * @brief Alpha beta quiescence (capture only) search.
 * @param game Game we are searching
 * @param depth search depth (<= 0 for quiescence)
 * @param ply distance from root node
 * @param alpha parent floor value
 * @param beta parent ceiling value
 * @return score The score for this position
*/
int 
SearchQuiescent(Game& game, 
                int   depth,
                int   ply, 
                int   alpha, 
                int   beta)
{    
    INCREMENT("quiescent calls");
    if (ply == MAX_PLY)
    {
        INCREMENT("quiescent max ply");
        return EvaluatePosition(*game.position, alpha, beta);
    }
    if (game.position->flags_ & IS_CHECK)
    {
        INCREMENT("quiescent checks");
        Variation dummy {};
        return Search(game, depth, ply, alpha, beta, dummy);
    }
    int score = EvaluatePosition(*game.position, alpha, beta);
    if (score >= beta)
    {
        INCREMENT("quiescent eval beta cutoffs");
        return beta;
    }
    if (score > alpha)
    {
        INCREMENT("quiescent eval raises alpha");
        alpha = score;
    }
    MoveList move_list;
    game.position->GeneratePseudoLegalCaptures(move_list);
    ScoreAndSortMoves(*game.position, move_list.moves, ply, depth);
    for (auto move : move_list)
    {
        if (MoveScore(move) < 0)
        {
            INCREMENT("negative SEE quiescent skips");
            continue;
        }
        game.PlayMove(move);
        if (game.position->flags_ & IS_MOVED_INTO_CHECK)
        {
            INCREMENT("quiescent moved into check");
            game.UndoMove();
            continue;
        }
        score = -SearchQuiescent(game, depth - 1, ply + 1, -beta, -alpha);
        game.UndoMove();
        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            RecordGoodMove(ply, move);
            return beta;
        }
        if (score > alpha)
        {
            alpha = score;
            INCREMENT("quiescent pv changed");
            RecordGoodMove(ply, move);
        }      
    }
    return alpha;
}