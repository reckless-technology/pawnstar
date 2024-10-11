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
int SearchSingleMove(Game &game, int depth, int ply, int alpha, int beta, Move move, Variation &pv, int move_index,
                     bool &is_checking)
{
    const bool was_in_check = game.CurrentPosition().IsInCheck();
    game.PlayMove(move);
    is_checking = game.CurrentPosition().IsInCheck();
    int score;
    if (beta > alpha + 1 && move_index > 0 && !was_in_check && !is_checking)
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
/// @param game Current game
/// @param depth Current search depth
/// @param ply Distance from root node
/// @param alpha Alpha value
/// @param beta Beta value
/// @return beta on success, alpha on failure
static inline int AttemptNullMove(Game &game, int depth, int ply, int alpha, int beta)
{
    // clang-format off
    if (depth > 2 &&                            // do not drop directly into quiescence search
        !game.CurrentPosition().IsNullMove() && // previous move was not a null move
        !game.CurrentPosition().IsInCheck() &&  // we are not in check
        beta == alpha + 1 &&                    // this is not a PV node
        game.CurrentPosition().PiecesOfColor(game.CurrentPosition().ColorToMove()).PopCount() > 4 &&    // we have at least 4 friendly pieces
        EvaluatePosition(game.CurrentPosition(), alpha, beta) >= beta)  // static evaluation is at least beta
    // clang-format on
    {
        INCREMENT("null move");
        Variation dummy{};
        game.MakeNullMove();
        int score = -Search(game, depth - 2, ply + 1, -beta, -alpha, dummy);
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

/// @brief Alpha beta main search algorithm. This is the primary recursive search function used to find the best move.
/// @param game game position to search
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
    if (game.CurrentPosition().IsDrawByMaterial() || game.IsDrawByFiftyMoves() || game.IsDrawByRepetition())
    {
        return DRAW_SCORE;
    }
    if (ply == MAX_PLY)
    {
        INCREMENT("max ply reached");
        return EvaluatePosition(game.CurrentPosition(), alpha, beta);
    }

    // Determine if there is an entry in the transposition table for this position. If so, see if it is sufficient for
    // us to avoid the search entirely.
    Transposition transposition;
    const bool    is_transposition = FindTransposition(game.CurrentPosition().Hash(), transposition);
    if (is_transposition && transposition.depth >= depth)
    {
        switch (transposition.node_type)
        {
        case NODE_CUT:
            // We don't know the exact score of the best move from this position, but we do know it is at least
            // transposition.score
            INCREMENT("table hit cut node");
            if (transposition.score >= beta)
            {
                INCREMENT("table hit cut node cutoffs");
                return transposition.score;
            }
            break;

        case NODE_ALL:
            // We don't know the exact score of the best move from this position, but we do know it is at most
            // transposition.score
            INCREMENT("table hit all node");
            if (transposition.score <= alpha)
            {
                INCREMENT("table hit all node cutoffs");
                return transposition.score;
            }
            break;

        case NODE_PV:
            // We know the exact score and the best move from this position. However, do the full search
            // to get the PV. The extra time searching these few principal variation nodes is trivial.
            INCREMENT("table hit pv node");
            ++depth;
            break;
        }
    }

    // Can we go directly to the quiescence search?
    if (depth <= 0 && !game.CurrentPosition().IsInCheck())
    {
        return SearchQuiescent(game, depth, ply, alpha, beta);
    }

    // Try null move pruning.
    const int null_move_score = AttemptNullMove(game, depth, ply, alpha, beta);
    if (game.is_cancel_pending_)
    {
        return SEARCH_CANCELLED_SCORE;
    }
    if (null_move_score >= beta)
    {
        return null_move_score;
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
        bool is_checking;
        int  score = SearchSingleMove(game, depth, ply, alpha, beta, transposition.move, pv, 0, is_checking);
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("table move beta cutoffs");
            RecordTransposition(game.CurrentPosition().Hash(), depth, score, transposition.move, NODE_CUT);
            RecordGoodMove(depth, ply, transposition.move);
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
            RecordGoodMove(depth, ply, transposition.move);
        }
    }
    // We didn't get a cutoff from the transposition table so proceed with generating and searching moves.
    MoveList move_list{game.CurrentPosition().GenerateLegalMoves()};
    if (move_list.size() == 0)
    {
        // We failed to generate any strictly legal moves from this position. The position therefore represents either
        // checkmate or stalemate depending on whether we are presently in check. If we are in checkmate, add the ply
        // (distance from the root node) to the losing score, so that the search algorithm chooses the shortest path to
        // checkmate when multiple possible checkmate paths exist.
        if (game.CurrentPosition().IsInCheck())
        {
            INCREMENT("checkmates");
            return CHECKMATED_SCORE + ply;
        }
        INCREMENT("stalemates");
        return DRAW_SCORE;
    }
    // Assign provisional scores to each move and sort them best first.
    ScoreAndSortMoves(game, move_list, depth, ply, alpha, beta);
    // Start of the main loop.
    for (int move_index = 0; move_index != (int)move_list.size(); ++move_index)
    {
        const Move move = move_list[move_index];
        if (is_transposition && move == transposition.move)
        {
            continue; // We already searched this move.
        }
        int score;
        // Try late move reduction, if all the conditions are met.
        // clang-format off
        if (!game.CurrentPosition().IsInCheck() &&  // we are not in check
            beta == alpha + 1 &&                    // it is not a PV node
            move_index > 0 &&                       // it's not the first move
            depth > 2 &&                            // we don't drop directly into quiescence search
            !move.IsChecking() &&                   // move does not give check
            move.score() < 0)                       // move has negative SEE
        // clang-format on
        {
            INCREMENT("late move reduction");
            bool is_checking;
            score = SearchSingleMove(game, depth - 1, ply, alpha, beta, move, pv, move_index, is_checking);
            if (score > best_score)
            {
                INCREMENT("late move reduction fails");
                score = SearchSingleMove(game, depth, ply, alpha, beta, move, pv, move_index, is_checking);
            }
        }
        else
        {
            bool is_checking;
            score = SearchSingleMove(game, depth, ply, alpha, beta, move, pv, move_index, is_checking);
        }
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("beta cutoffs");
            RecordTransposition(game.CurrentPosition().Hash(), depth, score, move, NODE_CUT);
            RecordGoodMove(depth, ply, move);
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
                RecordGoodMove(depth, ply, move);
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
        RecordGoodMove(depth, ply, best_move);
        CopyVariation(parent_pv, pv, best_move);
    }
    else
    {
        // We tried every move but did not raise alpha; this was an all node.
        INCREMENT("all nodes");
        RecordTransposition(game.CurrentPosition().Hash(), depth, best_score, best_move, NODE_ALL);
    }
    return best_score;
}