#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "search_state.h"
#include "static_exchange_evaluation.h"
#include "transposition_table.h"

/// @brief Alpha-beta quiescence search.
/// @param state Per-thread search state.
/// @param depth Search depth (<= 0).
/// @param ply Distance from root node.
/// @param alpha Alpha value.
/// @param beta Beta value.
/// @return Quiescent score for this node.
int SearchQuiescent(SearchState &state, int depth, int ply, int alpha, int beta)
{
    INCREMENT("quiescent calls");
    if (ply == kMaxPly)
    {
        INCREMENT("quiescent max ply");
        return EvaluatePosition(state, alpha, beta);
    }
    if (state.CurrentPosition().IsInCheck())
    {
        // We can't handle checks in quiescence.
        INCREMENT("quiescent checks");
        Variation dummy{};
        return Search(state, depth, ply, alpha, beta, dummy);
    }
    auto transposition = state.game.quiescent_table.FindTransposition(state.CurrentPosition().Hash());
    if (transposition && transposition->score >= beta)
    {
        INCREMENT("quiescent table beta cutoffs");
        return transposition->score;
    }
    int score = EvaluatePosition(state, alpha, beta);
    if (score >= beta)
    {
        INCREMENT("quiescent eval beta cutoffs");
        state.game.quiescent_table.RecordTransposition(
            Transposition{state.CurrentPosition().Hash(), Move::None(), score, depth, Transposition::NodeType::kCut});
        return score;
    }
    if (score > alpha)
    {
        INCREMENT("quiescent eval raises alpha");
        alpha = score;
    }
    int best_score = score;
    if (transposition && transposition->move != Move::None())
    {
        INCREMENT("quiescent table move");
        state.PlayMove(transposition->move);
        score = -SearchQuiescent(state, depth - 1, ply + 1, -beta, -alpha);
        state.UndoMove();
        if (state.IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent table move beta cutoffs");
            state.game.history_table.RecordGoodMove(ply, transposition->move);
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                INCREMENT("quiescent table move raise alpha");
                state.game.history_table.RecordGoodMove(ply, transposition->move);
            }
        }
    }
    MoveList move_list{state.CurrentPosition().GenerateLegalCaptures()};
    state.ScoreAndSortMoves(move_list, ply);
    for (Move &move : move_list)
    {
        if (transposition && transposition->move == move)
        {
            continue;
        }
#if 0
        const auto [see_score, is_checking] = EvaluateStaticExchange(state.CurrentPosition(), move);
        if (see_score < 0 && !is_checking)
        {
            INCREMENT("quiescent negative see");
            continue;
        }
#endif
        state.PlayMove(move);
        score = -SearchQuiescent(state, depth - 1, ply + 1, -beta, -alpha);
        state.UndoMove();
        if (state.IsCancelled())
        {
            return kSearchCancelledScore;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            state.game.history_table.RecordGoodMove(ply, move);
            state.game.quiescent_table.RecordTransposition(
                Transposition{state.CurrentPosition().Hash(), move, score, depth, Transposition::NodeType::kCut});
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                INCREMENT("quiescent pv changed");
                state.game.history_table.RecordGoodMove(ply, move);
                state.game.quiescent_table.RecordTransposition(
                    Transposition{state.CurrentPosition().Hash(), move, score, depth, Transposition::NodeType::kCut});
            }
        }
    }
    return best_score;
}
