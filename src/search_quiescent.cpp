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
int SearchQuiescent(Game &game,
                    int depth,
                    int ply,
                    int alpha,
                    int beta)
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
        Variation dummy{};
        return Search(game, depth, ply, alpha, beta, dummy);
    }
    int score = EvaluatePosition(*game.position_, alpha, beta);
    if (score >= beta)
    {
        INCREMENT("quiescent eval beta cutoffs");
        return score;
    }
    if (score > alpha)
    {
        INCREMENT("quiescent eval raises alpha");
        alpha = score;
    }
    int best_score = score;
    MoveList move_list{game.position_->GeneratePseudoLegalCaptures()};
    ScoreAndSortMoves(*game.position_, move_list, ply, depth);
    for (const auto &move : move_list)
    {
        if (MoveScore(move) < 0)
        {
            INCREMENT("quiescent negative SEE skips");
            return best_score;
        }
        game.PlayMove(move);
        score = -SearchQuiescent(game, depth - 1, ply + 1, -beta, -alpha);
        game.UndoMove();
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                INCREMENT("quiescent pv changed");
            }
        }
    }
    return best_score;
}