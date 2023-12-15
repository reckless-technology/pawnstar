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
        return EvaluatePosition(*game.position_, alpha, beta);
    }
    if (game.position_->flags_ & IS_CHECK)
    {
        INCREMENT("quiescent checks");
        return alpha;
    }
    int score = EvaluatePosition(*game.position_, alpha, beta);
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
    Transposition transposition;
    bool is_transposition = FindTransposition(game.position_->hash_, transposition);
    if (is_transposition && transposition.move)
    {
        INCREMENT("quiescent table hit");
        if (transposition.node_type == NODE_CUT && transposition.score >= beta)
        {
            INCREMENT("quiescent table hit beta cutoff");
            return beta;
        }
        game.PlayMove(transposition.move);
        int score = -SearchQuiescent(game, depth - 1, ply + 1, -beta, -alpha);
        game.UndoMove();
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent table move caused beta cutoff");
            RecordGoodMove(ply, transposition.move);
            return beta;
        }
    }
    MoveList move_list;
    game.position_->GeneratePseudoLegalCaptures(move_list);
    ScoreAndSortMoves(*game.position_, move_list.moves, ply, depth);
    for (auto move : move_list)
    {
        if (MoveScore(move) < 0)
        {
            INCREMENT("negative SEE quiescent skips");
            continue;
        }
        game.PlayMove(move);
        if (game.position_->flags_ & IS_MOVED_INTO_CHECK)
        {
            INCREMENT("quiescent moved into check");
            game.UndoMove();
            continue;
        }
        score = -SearchQuiescent(game, depth - 1, ply + 1, -beta, -alpha);
        game.UndoMove();
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            RecordGoodMove(ply, move);
            RecordTranspositionMaybe(game.position_->hash_, depth, beta, move, NODE_CUT);
            return beta;
        }
        if (score > alpha)
        {
            alpha = score;
            INCREMENT("quiescent pv changed");
            RecordGoodMove(ply, move);
            RecordTranspositionMaybe(game.position_->hash_, depth, score, move, NODE_CUT);
        }      
    }
    return alpha;
}