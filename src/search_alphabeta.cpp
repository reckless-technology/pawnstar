#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"
#include "game.h"
#include "search.h"

/**
 * @brief Try null move pruning.
 * @param game Current game (position)
 * @param depth Current search depth
 * @param ply Distance from root node
 * @param alpha Score floor
 * @param beta Score ceiling (opponent's floor)
 * @return beta on success, alpha on failure
 */
#if DO_NULL_MOVE_PRUNING
static int 
AttemptNullMove(Game& game, 
                int   depth, 
                int   ply, 
                int   alpha, 
                int   beta)
{
    /*
    Try null move pruning if ALL of the following are true:

    # the previous move was not a null move
    # we are not in check   
    # this is not a PV node
    # we are not down to king and pawns
    # static eval is at least beta
    
    Hopefully this is sufficient to prevent most Zugzwang positions.
    */
    Position& position = *game.position_;
    if (   !(position.flags_ & IS_NULL_MOVE)
        && !(position.flags_ & IS_CHECK    )
        && beta == alpha + 1
        && (position.knights_ | position.bishops_ | position.rooks_ | position.queens_)
        && EvaluatePosition(position, alpha, beta) >= beta)
    {
        INCREMENT("null move attempts");
        game.MakeNullMove();
        Variation dummy {};
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
#endif

/**
 * @brief Fail hard alpha beta main search algorithm.
 * @param game position to search
 * @param depth search depth
 * @param ply distance from root node
 * @param alpha score floor at parent node
 * @param beta score ceiling at parent node
 * @param parent_pv principal variation at the parent node
 * @return score for this node
*/
int 
Search(Game&        game, 
       int          depth, 
       int          ply, 
       int          alpha, 
       int          beta, 
       Variation&   parent_pv)
{    
    if (game.is_cancel_pending_)
    {
        return SEARCH_CANCELLED_SCORE;
    }    
    if ((++game.node_count_ & 0xFFFF) == 0          &&
        game.time_control_.hard_stop_search_ms != 0 &&
        GetMilliseconds() >= game.time_control_.hard_stop_search_ms)
    {
        game.is_cancel_pending_ = true;
        return SEARCH_CANCELLED_SCORE;
    }
    INCREMENT("alpha beta calls");
    Position& position = *game.position_;
    if (position.IsDrawByMaterial  () ||
        position.IsDrawByFiftyMoves() ||
        position.IsDrawByRepetition(true))
    {
        return DRAW_SCORE;
    }    
    if (ply == MAX_PLY)
    {
        INCREMENT("max ply reached");
        return EvaluatePosition(position, alpha, beta);
    }
    if (position.flags_ & IS_CHECK)
    {
        INCREMENT("extensions check");
        ++depth;
    }
    else if (depth <= 0)
    {
        return SearchQuiescent(game, depth, ply, alpha, beta);
    } 
    /*
    Determine if there is an entry in the transposition table 
    for this position.
    */
    Variation pv {};
    Transposition transposition {};
    const bool is_transposition = FindTransposition(position.hash_, transposition);
    if (is_transposition && transposition.depth >= depth)
    {
        INCREMENT("table hit");
        switch (transposition.node_type)
        {
        case NODE_CUT:
            /*
            We don't know the exact score of the best move from this
            position, but we do know it is at least transposition.score
            */
            INCREMENT("table hit cut node");
            if (transposition.score >= beta)
            {
                INCREMENT("table hit beta cutoffs");
                return beta;
            }
            break;

        case NODE_ALL:
            /*
            We don't know the exact score of the best move from this
            position, but we do know it is at most transposition.score
            */
            INCREMENT("table hit all node");
            if (transposition.score <= alpha)
            {
                INCREMENT("table hit alpha cutoffs");
                return alpha;
            }
            break;

        case NODE_PV:
            /*
            We know the exact score and the best move from this position.
            However, do the full search to get the PV. The extra time 
            searching these few principal variation nodes is trivial.
            */
            INCREMENT("table hit pv node");
            ++depth;
            break;
        }
    }
 
#if DO_NULL_MOVE_PRUNING
    int null_move_score = AttemptNullMove(game, depth, ply, alpha, beta);
    if (game.is_cancel_pending_)
    {
        return SEARCH_CANCELLED_SCORE;
    }
    if (null_move_score >= beta)
    {
        return beta;
    }
#endif

#if DO_FUTILITY_PRUNING
    if (depth == 1                          &&
        !(position.flags_ & IS_CHECK)       &&
        !(position.flags_ & IS_NULL_MOVE)   &&
        beta == alpha + 1                   &&
        EvaluatePosition(position, alpha, beta) + 500 <= alpha)
    {
        INCREMENT("futile prunes");
        return alpha;
    }
#endif

    /*
    Before we generate any moves, try the transposition table move first. 
    This might save us the cost of move generation altogether, or provide 
    better alpha beta bounds for the main search.
    */
    
    int  num_legal_moves   = 0;
    Move best_move         = 0;
    bool has_raised_alpha  = false;
    if (is_transposition && transposition.move)
    {
        INCREMENT("table move");
        best_move = transposition.move;
        ++num_legal_moves;
        int score = SearchSingleMove(game, depth, ply, alpha, beta, transposition.move, pv, 0);
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("table move beta cutoffs");
            RecordTransposition(position.hash_, depth, beta, transposition.move, NODE_CUT);
            RecordKillerMove(ply, transposition.move);
            return beta;
        }
        if (score > alpha)
        {
            INCREMENT("table move raised alpha");
            alpha = score;
            has_raised_alpha = true;
            /* 
            Provisionally store this move as a CUT node - it 
            may later turn out to be a PV but we don't know that yet.
            */
            RecordTransposition(position.hash_, depth, alpha, transposition.move, NODE_CUT);
            RecordKillerMove(ply, transposition.move);
        }
    } 
    /*
    We didn't get a cutoff from the transposition table so proceed
    with generating and searching moves.
    */
    MoveList move_list { position.GeneratePseudoLegalMoves() };
    ScoreAndSortMoves(position, move_list, ply, depth);
    if (best_move == 0)
    {
        best_move = move_list[0];
    }
    /* 
    Start of the main loop. 
    */
    for (const auto& move : move_list)
    {
        if (is_transposition && MoveBits(move) == MoveBits(transposition.move))
        {
            continue; /* We already searched this move. */
        }
        int score;

#if DO_LATE_MOVE_REDUCTION
        if (!(position.flags_ & IS_CHECK)               &&
            !(position.flags_ & HAS_BEEN_REDUCED)       &&
            beta == alpha + 1                           &&
            num_legal_moves * 3 > (int)move_list.size() &&
            depth > 3                                   &&
            MoveCaptured(move) == NO_PIECE              &&
            MovePromoted(move) == NO_PIECE              &&
            MoveScore(move) < 0)
        {
            game.PlayMove(move);
            if (game.position_->flags_ & IS_MOVED_INTO_CHECK)
            {
                game.UndoMove();
                continue;
            }
            if (!(game.position_->flags_ & IS_CHECK))
            {
                INCREMENT("late move reductions");
                game.position_->flags_ |= HAS_BEEN_REDUCED;
                score = -Search(game, depth - 2, ply + 1, -beta, -alpha, pv);
            }
            else
            {
                score = -Search(game, depth - 1, ply + 1, -beta, -alpha, pv);
            }
            game.UndoMove();
        }
        else
#endif

        {
            score = SearchSingleMove(game, depth, ply, alpha, beta, move, pv, num_legal_moves);
        }
        if (game.is_cancel_pending_)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score == MOVED_INTO_CHECK_SCORE)
        {
            INCREMENT("moved into check");
            continue;
        }
        ++num_legal_moves;
        if (score >= beta)
        {
            INCREMENT("beta cutoffs");
            RecordTransposition(position.hash_, depth, beta, move, NODE_CUT);
            RecordKillerMove(ply, move);
            return beta;
        }
        if (score > alpha)
        {
            INCREMENT("pv changed");
            alpha = score;
            has_raised_alpha = true;
            best_move = move;
            /* 
            Provisionally store this move as a CUT node - it 
            may later turn out to be a PV but we don't know that 
            for sure yet until we have searched every move.
            */
            RecordTransposition(position.hash_, depth, score, move, NODE_CUT);
            RecordKillerMove(ply, transposition.move);
        }
    }
    /*
    End of the main loop.
    We searched all moves and did not get a beta cutoff.
    */
    if (num_legal_moves == 0)
    {
        /*
        We failed to find any strictly legal moves from this position.
        The position therefore represents either checkmate or stalemate 
        depending on whether we are presently in check.
        If we are in checkmate, add the ply (distance from the root node) to  
        the losing score, so that the search algorithm chooses the shortest  
        path to checkmate when multiple possible checkmates exist in the tree.
        */
        if (position.flags_ & IS_CHECK)
        {
            INCREMENT("checkmates");
            return CHECKMATED_SCORE + ply;
        }
        INCREMENT("stalemates");
        return DRAW_SCORE;
    }
    if (has_raised_alpha)
    {
        /*
        We raised alpha but did not cutoff; this was a 
        PV node (these are rare) so copy the PV up the 
        tree to our parent node.
        */
        INCREMENT("pv nodes");
        RecordTransposition(position.hash_, depth, alpha, best_move, NODE_PV);
        RecordKillerMove(ply, best_move);
        CopyVariation(parent_pv, pv, best_move);
    }
    else
    {
        /*
        We tried every move but did not raise alpha; this was an all node
        */
        INCREMENT("all nodes");
        RecordTransposition(position.hash_, depth, alpha, best_move, NODE_ALL);
    }
    return alpha;
}