#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "sort_moves.h"
#include "transposition_table.h"

/// @brief Alpha beta quiescence (capture only) search.
/// @param game Game we are searching
/// @param depth search depth (<= 0 for quiescence)
/// @param ply distance from root node
/// @param alpha parent floor value
/// @param beta parent ceiling value
/// @return score The score for this position
int SearchQuiescent(Game &game, int depth, int ply, int alpha, int beta)
{
    INCREMENT("quiescent calls");
    if (ply == MAX_PLY)
    {
        INCREMENT("quiescent max ply");
        return EvaluatePosition(game.CurrentPosition(), alpha, beta);
    }
    if (game.CurrentPosition().IsInCheck())
    {
        INCREMENT("quiescent checks");
        Variation dummy{};
        return Search(game, depth, ply, alpha, beta, dummy);
    }
    int score = EvaluatePosition(game.CurrentPosition(), alpha, beta);
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
    int      best_score = score;
    MoveList move_list{game.CurrentPosition().GenerateLegalCaptures()};
    ScoreAndSortMoves(game.CurrentPosition(), move_list, ply, depth);
    for (Move move : move_list)
    {
        if (move.score() < 0)
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