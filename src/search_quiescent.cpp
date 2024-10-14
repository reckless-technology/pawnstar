#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "static_exchange_evaluation.h"
#include "transposition_table.h"

/// @brief Alpha beta quiescence search.
/// @param game Game to be searched.
/// @param depth Search depth (<= 0)
/// @param ply Distance from root node.
/// @param alpha Alpha value.
/// @param beta Beta value.
/// @return Quiescent score for this node.
int SearchQuiescent(Game &game, int depth, int ply, int alpha, int beta)
{
    INCREMENT("quiescent calls");
    if (ply == MAX_PLY)
    {
        INCREMENT("quiescent max ply");
        return EvaluatePosition(game, alpha, beta);
    }
    if (game.CurrentPosition().IsInCheck())
    {
        // We can't handle checks in quiescence.
        INCREMENT("quiescent checks");
        Variation dummy{};
        return Search(game, depth, ply, alpha, beta, dummy);
    }
    int score = EvaluatePosition(game, alpha, beta);
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
    game.ScoreAndSortMoves(move_list, ply);
    for (Move &move : move_list)
    {
        const auto [see_score, is_checking] = EvaluateStaticExchange(game.CurrentPosition(), move);
        if (see_score < 0 && !is_checking)
        {
            // Skip moves with a negative SEE which do not give check; they're most likely futile.
            INCREMENT("quiescent negative see");
            continue;
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
            game.history_table.RecordGoodMove(ply, move);
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                INCREMENT("quiescent pv changed");
                game.history_table.RecordGoodMove(ply, move);
            }
        }
    }
    return best_score;
}