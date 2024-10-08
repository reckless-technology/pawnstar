#include "chess_clock.h"
#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "sort_moves.h"
#include "transposition_table.h"

/// @brief Search a single move and return its alpha beta score.
/// @param game Game we are searching
/// @param depth Depth to search to
/// @param ply Distance from root node
/// @param alpha Floor value
/// @param beta Opponents floor value
/// @param move Move to search
/// @param pv Parent's principal variation
/// @param move_index Move number (0 is first move)
/// @return score for this move
int SearchSingleMove(Game &game, int depth, int ply, int alpha, int beta, Move move, Variation &pv, int move_index)
{
    game.PlayMove(move);
    int score;
    if (beta > alpha + 1 && move_index > 0 && !game.CurrentPosition().IsInCheck() && !move.IsChecking())
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

/// @brief Try null move pruning.
/// Try null move pruning if ALL of the following are true:
///
/// 1) the previous move was not a null move
/// 2) we are not in check
/// 3) this is not a PV node
/// 4) we are not down to king and pawns
/// 5) static eval is at least beta
///
/// Hopefully this is sufficient to prevent most Zugzwang positions.
///
/// @param game Current game
/// @param depth Current search depth
/// @param ply Distance from root node
/// @param alpha Score floor
/// @param beta Score ceiling (opponent's floor)
/// @return beta on success, alpha on failure
static inline int AttemptNullMove(Game &game, int depth, int ply, int alpha, int beta)
{
    if (!game.CurrentPosition().IsNullMove() && !game.CurrentPosition().IsInCheck() && beta == alpha + 1 &&
        (game.CurrentPosition().Knights() | game.CurrentPosition().Bishops() | game.CurrentPosition().Rooks() |
         game.CurrentPosition().Queens())
            .IsNotEmpty() &&
        EvaluatePosition(game.CurrentPosition(), alpha, beta) >= beta)
    {
        INCREMENT("null move attempts");
        Variation dummy{};
        game.MakeNullMove();
        int score = -Search(game, depth - 3, ply + 1, -beta, -alpha, dummy);
        game.UndoMove();
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            return beta;
        }
        INCREMENT("null move fails");
        return alpha;
    }
    return alpha;
}

/// @brief Alpha beta main search algorithm.
/// @param game game.CurrentPosition() to search
/// @param depth search depth
/// @param ply distance from root node
/// @param alpha score floor at parent node
/// @param beta score ceiling at parent node
/// @param parent_pv principal variation at the parent node
/// @return score for this node
int Search(Game &game, int depth, int ply, int alpha, int beta, Variation &parent_pv)
{
    INCREMENT("alpha beta calls");
    if ((++game.node_count_ & 0xFFFF) == 0 && game.time_control_.hard_stop_ms != 0 &&
        ElapsedMilliseconds() >= game.time_control_.hard_stop_ms)
    {
        game.is_cancel_pending_ = true;
        return SEARCH_CANCELLED_SCORE;
    }
    if (game.CurrentPosition().IsDrawByMaterial() || game.IsDrawByFiftyMoves() || game.IsDrawByRepetition(true))
    {
        return DRAW_SCORE;
    }
    if (ply == MAX_PLY)
    {
        INCREMENT("max ply reached");
        return EvaluatePosition(game.CurrentPosition(), alpha, beta);
    }
    if (depth <= 0 && !game.CurrentPosition().IsInCheck())
    {
        return SearchQuiescent(game, depth, ply, alpha, beta);
    }
    // Determine if there is an entry in the transposition table for this game.CurrentPosition(). If so, see if it is
    // sufficient for us to avoid the search entirely.
    Transposition transposition;
    const bool    is_transposition = FindTransposition(game.CurrentPosition().Hash(), transposition);
    if (is_transposition && transposition.depth >= depth)
    {
        switch (transposition.node_type)
        {
        case NODE_CUT:
            // We don't know the exact score of the best move from this game.CurrentPosition(), but we do know it is at
            // least transposition.score
            INCREMENT("table hit cut node");
            if (transposition.score >= beta)
            {
                INCREMENT("table hit cut node cutoffs");
                return transposition.score;
            }
            break;

        case NODE_ALL:
            // We don't know the exact score of the best move from this game.CurrentPosition(), but we do know it is at
            // most transposition.score
            INCREMENT("table hit all node");
            if (transposition.score <= alpha)
            {
                INCREMENT("table hit all node cutoffs");
                return transposition.score;
            }
            break;

        case NODE_PV:
            // We know the exact score and the best move from this game.CurrentPosition(). However, do the full search
            // to get the PV. The extra time searching these few principal variation nodes is trivial.
            INCREMENT("table hit pv node");
            break;
        }
    }

    // Try null move pruning.
    if (!is_transposition || transposition.node_type == NODE_CUT)
    {
        const int null_move_score = AttemptNullMove(game, depth, ply, alpha, beta);
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (null_move_score >= beta)
        {
            return null_move_score;
        }
    }
    // Before we generate any moves, try the transposition table move first. This might save us the cost of move
    // generation altogether, or provide better alpha beta bounds for the main search.
    Variation pv;
    Move      best_move        = Move::None();
    int       best_score       = ALPHA;
    bool      has_raised_alpha = false;
    if (is_transposition && transposition.move)
    {
        INCREMENT("table move");
        best_move = transposition.move;
        int score = SearchSingleMove(game, depth, ply, alpha, beta, transposition.move, pv, 0);
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("table move beta cutoffs");
            RecordTransposition(game.CurrentPosition().Hash(), depth, score, transposition.move, NODE_CUT);
            RecordKillerMove(ply, transposition.move);
            return score;
        }
        best_score = score;
        if (score > alpha)
        {
            INCREMENT("table move raised alpha");
            alpha            = score;
            has_raised_alpha = true;
            // Provisionally store this move as a CUT node - it may later turn out to be a PV but we don't know that yet
            // until we have searched every move.
            RecordTransposition(game.CurrentPosition().Hash(), depth, score, transposition.move, NODE_CUT);
            RecordKillerMove(ply, transposition.move);
        }
    }
    // We didn't get a cutoff from the transposition table so proceed with generating and searching moves.
    MoveList move_list{game.CurrentPosition().GenerateLegalMoves()};
    if (move_list.size() == 0)
    {
        // We failed to generate any strictly legal moves from this position. The position therefore represents either
        // checkmate or stalemate depending on whether we are presently in check. If we are in checkmate, add the ply
        // (distance from the root node) to the losing score, so that the search algorithm chooses the shortest path to
        // checkmate when multiple possible checkmates exist in the tree.
        if (game.CurrentPosition().IsInCheck())
        {
            INCREMENT("checkmates");
            return CHECKMATED_SCORE + ply;
        }
        INCREMENT("stalemates");
        return DRAW_SCORE;
    }
    // Start of the main loop.
    ScoreAndSortMoves(game.CurrentPosition(), move_list, ply, depth);
    for (int move_index = 0; move_index != (int)move_list.size(); ++move_index)
    {
        const Move move = move_list[move_index];
        if (is_transposition && move == transposition.move)
        {
            continue; // We already searched this move.
        }
        int score;
        // Try late move reduction.
        if (!game.CurrentPosition().IsInCheck() && !game.CurrentPosition().HasBeenReduced() && beta == alpha + 1 &&
            move_index > 1 && depth > 2 && !(move.captured() || move.piece() == PAWN || move.IsChecking()))
        {
            game.PlayMove(move);
            INCREMENT("late move reduction");
            game.CurrentPosition().Reduce();
            score = -Search(game, depth - 2, ply + 1, -beta, -alpha, pv);
            game.UndoMove();
            if (score > alpha)
            {
                INCREMENT("late move reduction fails");
                score = SearchSingleMove(game, depth, ply, alpha, beta, move, pv, move_index);
            }
        }
        else
        {
            score = SearchSingleMove(game, depth, ply, alpha, beta, move, pv, move_index);
        }
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("beta cutoffs");
            RecordTransposition(game.CurrentPosition().Hash(), depth, score, move, NODE_CUT);
            RecordKillerMove(ply, move);
            return score;
        }
        if (score > best_score)
        {
            INCREMENT("best score changed");
            best_score = score;
            best_move  = move;
            if (score > alpha)
            {
                INCREMENT("pv changed");
                alpha            = score;
                has_raised_alpha = true;
                // Provisionally store this move as a CUT node - it may later turn out to be a PV but we don't know that
                // for sure yet until we have searched every move.
                RecordTransposition(game.CurrentPosition().Hash(), depth, score, move, NODE_CUT);
                RecordKillerMove(ply, move);
            }
        }
    }
    // End of the main loop. We searched all moves and did not get a beta cutoff.
    if (has_raised_alpha)
    {
        // We raised alpha but did not cutoff; this was a PV node (these are rare) so copy the PV up the tree to our
        // parent node.
        INCREMENT("pv nodes");
        RecordTransposition(game.CurrentPosition().Hash(), depth, alpha, best_move, NODE_PV);
        RecordKillerMove(ply, best_move);
        CopyVariation(parent_pv, pv, game.CurrentPosition().MoveToString(best_move, &move_list));
    }
    else
    {
        // We tried every move but did not raise alpha; this was an all node.
        INCREMENT("all nodes");
        RecordTransposition(game.CurrentPosition().Hash(), depth, best_score, best_move, NODE_ALL);
    }
    return best_score;
}