#include "chess_clock.h"
#include "debug_hashtable.h"
#include "evaluation.h"
#include "game.h"
#include "position.h"
#include "search.h"
#include "sort_moves.h"
#include "static_exchange_evaluation.h"
#include "transposition_table.h"

#include <utility>

/// @brief Search a single move and return its alpha beta score and whether it gives check.
/// @param game Game we are searching.
/// @param depth Depth to search to.
/// @param ply Distance from root node.
/// @param alpha Alpha value.
/// @param beta Beta value.
/// @param move Move to search.
/// @param pv Parent's principal variation
/// @param move_index Move number (0 is first move).
/// @return Score for this move, and whether move is checking.
std::pair<int, bool> SearchSingleMove(Game &game, int depth, int ply, int alpha, int beta, Move move, Variation &pv,
                                      int move_index)
{
    const Position &position     = game.CurrentPosition();
    const bool      was_in_check = position.IsInCheck();

    int child_depth = depth - 1;
    switch (move.type())
    {
    case Move::Type::PROMOTION_KNIGHT:
    case Move::Type::PROMOTION_BISHOP:
    case Move::Type::PROMOTION_ROOK:
    case Move::Type::PROMOTION_QUEEN:
        ++child_depth;
        INCREMENT("extensions promotion");
        break;

    case Move::Type::EP_CAPTURE:
        ++child_depth;
        INCREMENT("extensions ep capture");
        break;

    default:
        break;
    }
    game.PlayMove(move);
    const bool is_checking = game.CurrentPosition().IsInCheck();
    int        score;
    if (beta > alpha + 1 && move_index > 0 && !was_in_check && !is_checking)
    {
        // Try principal variation search.
        INCREMENT("pvs attempts");
        score = -Search(game, child_depth, ply + 1, -alpha - 1, -alpha, pv);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            score = -Search(game, child_depth, ply + 1, -beta, -alpha, pv);
        }
    }
    else
    {
        score = -Search(game, child_depth, ply + 1, -beta, -alpha, pv);
    }
    game.UndoMove();
    return {score, is_checking};
}

/// @brief Try null move pruning.
/// @param game Current game.
/// @param depth Current search depth.
/// @param ply Distance from root node.
/// @param alpha Alpha value.
/// @param beta Beta value.
/// @return beta on success, alpha on failure.
static inline int AttemptNullMove(Game &game, int depth, int ply, int alpha, int beta)
{
    const Position &position = game.CurrentPosition();
    const Color     color    = position.ColorToMove();
    // Only try null move pruning if all conditions are met.
    if (!position.IsNullMove() &&                       // previous move was not a null move
        !position.IsInCheck() &&                        // we are not in check
        beta == alpha + 1 &&                            // this is not a PV node
        position.PiecesOfColor(color).PopCount() > 4 && // we have at least 5 friendly pieces
        EvaluatePosition(game, alpha, beta) >= beta)    // static evaluation is at least beta
    {
        INCREMENT("null move");
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
/// This is the primary recursive search function used to find the best move.
/// @param game Game being searched.
/// @param depth Depth to search.
/// @param ply Distance from root node.
/// @param alpha Alpha value.
/// @param beta Beta value.
/// @param parent_pv Principal variation at the parent node.
/// @return The alpha beta score for this node.
int Search(Game &game, int depth, int ply, int alpha, int beta, Variation &parent_pv)
{
    INCREMENT("alpha beta calls");
    if ((++game.node_count_ & 0xFFFF) == 0 && game.time_control_.hard_stop_ms != 0 &&
        ElapsedMilliseconds() >= game.time_control_.hard_stop_ms)
    {
        // Out of time; stop searching immediately.
        game.is_cancel_pending_ = true;
        return SEARCH_CANCELLED_SCORE;
    }
    if (ply == MAX_PLY)
    {
        INCREMENT("max ply reached");
        return EvaluatePosition(game, alpha, beta);
    }
    // Determine if there is an entry in the transposition table for this position. If so, see if it is sufficient for
    // us to avoid the search entirely.
    const auto transposition = game.Table().FindTransposition(game.CurrentPosition().Hash());
    if (transposition && transposition->depth >= depth)
    {
        switch (transposition->node_type)
        {
        case Transposition::NodeType::CUT:
            // We don't know the exact score of the best move from this position, but we do know it is at least
            // transposition.score
            INCREMENT("table hit cut node");
            if (transposition->score >= beta)
            {
                INCREMENT("table hit cut node cutoffs");
                return transposition->score;
            }
            break;

        case Transposition::NodeType::ALL:
            // We don't know the exact score of the best move from this position, but we do know it is at most
            // transposition.score
            INCREMENT("table hit all node");
            if (transposition->score < alpha)
            {
                INCREMENT("table hit all node cutoffs");
                return transposition->score;
            }
            break;

        case Transposition::NodeType::PV:
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
    if (transposition && transposition->move)
    {
        INCREMENT("table move");
        best_move = transposition->move;
        int score = SearchSingleMove(game, depth, ply, alpha, beta, transposition->move, pv, 0).first;
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("table move beta cutoffs");
            game.Table().RecordTransposition(Transposition{game.CurrentPosition().Hash(), transposition->move, score,
                                                           depth, Transposition::NodeType::CUT});
            RecordGoodMove(ply, transposition->move);
            return score;
        }
        best_score = score;
        if (score > alpha)
        {
            INCREMENT("table move raised alpha");
            alpha            = score;
            has_raised_alpha = true;
            RecordGoodMove(ply, transposition->move);
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
    ScoreAndSortMoves(game, move_list, ply);

    // Start of the main loop.
    for (const Move &move : move_list)
    {
        const int move_index = &move - move_list.begin();
        if (transposition && move == transposition->move)
        {
            continue; // We already searched this move.
        }
        // Maybe try late move depth reduction, if all the conditions are met.
        int lmr_depth = depth;
        if (!game.CurrentPosition().IsInCheck() && // we are not in check
            depth > 2 &&                           // do not drop directly into quiescence
            beta == alpha + 1 &&                   // it is not a PV node
            move_index > 3)                        // we already tried a few moves without cutoff
        {
            auto [see_score, is_checking] = EvaluateStaticExchange(game.CurrentPosition(), move);
            if (!is_checking && see_score <= 0) // move does not give check and does not win material
            {
                INCREMENT("late move reduction");
                --lmr_depth;
                if (see_score < 0 && move_index > 6) // Move loses material and is very late; reduce further.
                {
                    INCREMENT("late move reduction extreme");
                    --lmr_depth;
                }
            }
        }
        int score = SearchSingleMove(game, lmr_depth, ply, alpha, beta, move, pv, move_index).first;
        if (score > alpha && lmr_depth < depth)
        {
            INCREMENT("late move reduction fails");
            score = SearchSingleMove(game, depth, ply, alpha, beta, move, pv, move_index).first;
        }
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("beta cutoffs");
            game.Table().RecordTransposition(
                Transposition{game.CurrentPosition().Hash(), move, score, depth, Transposition::NodeType::CUT});
            RecordGoodMove(ply, move);
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
                RecordGoodMove(ply, move);
            }
        }
    }
    // End of the main loop. We searched all moves and did not get a beta cutoff.
    if (has_raised_alpha)
    {
        // We raised alpha but did not cutoff; this was a PV node (these are rare) so copy the PV up the tree to our
        // parent node.
        INCREMENT("pv nodes");
        game.Table().RecordTransposition(
            Transposition{game.CurrentPosition().Hash(), best_move, alpha, depth, Transposition::NodeType::PV});
        RecordGoodMove(ply, best_move);
        CopyVariation(parent_pv, pv, best_move);
    }
    else
    {
        // We tried every move but did not raise alpha; this was an all node.
        INCREMENT("all nodes");
        game.Table().RecordTransposition(
            Transposition{game.CurrentPosition().Hash(), best_move, best_score, depth, Transposition::NodeType::ALL});
    }
    return best_score;
}