#include "pawnstar.h"

/**
 * @brief Fail hard alpha beta main search algorithm.
 * @param src_position position to search
 * @param depth search depth
 * @param ply distance from root node
 * @param alpha score floor at parent node
 * @param beta score ceiling at parent node
 * @param cancel cancel thinking flag
 * @param parent_pv principal variation
 * @return score for this node
*/
int 
Search(const Position*  src_position, 
       int              depth, 
       int              ply, 
       int              alpha, 
       int              beta, 
       volatile bool*   cancel, 
       Variation*       parent_pv)
{    
    if (*cancel)
    {
        return SEARCH_CANCELLED_SCORE;
    }
    
    if (!(++the_game.node_count & 0xFFFF)         &&
        the_game.time_control.hard_stop_search_ms &&
        GetMilliseconds() >= the_game.time_control.hard_stop_search_ms)
    {
        *cancel = true;
        return SEARCH_CANCELLED_SCORE;
    }
    INCREMENT("alpha beta calls");
    if (IsDrawByMaterial  (src_position)    ||
        IsDrawByFiftyMoves(src_position)    ||
        IsDrawByRepetition(src_position, true))
    {
        return DRAW_SCORE;
    }    
    if (ply == MAX_PLY)
    {
        INCREMENT("max ply reached");
        return EvaluatePosition(src_position, alpha, beta);
    }
    if (src_position->state_flags & IS_CHECK)
    {
        INCREMENT("extensions check");
        ++depth;
    }
    else if (depth <= 0)
    {
        return SearchQuiescent(src_position, depth, ply, alpha, beta, cancel);
    } 
    /*
    Determine if there is an entry in the transposition table 
    for this position.
    */
    Transposition transposition;
    const bool is_transposition = FindTransposition(src_position->hash, &transposition);
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
    /*
    Try null move pruning if ALL of the following are true:

    # the previous move was not a null move
    # we are not in check   
    # this is not a PV node
    # we are not down to king and pawns
    # static eval is at least beta
    
    Hopefully this is sufficient to prevent most Zugzwang positions.
    */
    if (!(src_position->state_flags & IS_NULL_MOVE)         &&
        !(src_position->state_flags & IS_CHECK)             &&
        beta == alpha + 1                                   &&
        (src_position->knights | src_position->bishops | 
         src_position->rooks   | src_position->queens)      &&
        EvaluatePosition(src_position, alpha, beta) >= beta)
    {
        INCREMENT("null move attempts");
        Position position;       
        MakeNullMove(&position, src_position);
        int score = -Search(&position, depth - 3, ply + 1, -beta, -alpha, cancel, NULL);
        if (*cancel)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            return beta;
        }
        INCREMENT("null move fails");
    }
#endif /* DO_NULL_MOVE_PRUNING */

    /*
    Before we generate any moves, try the transposition table move first. 
    This might save us the cost of move generation altogether, or provide 
    better alpha beta bounds for the main search.
    */
    Variation   pv;
    int         num_legal_moves     = 0;
    Move        best_move           = 0;
    bool        has_raised_alpha    = false;
    pv.moves[0]                     = 0;
    
    if (is_transposition && transposition.move)
    {
        INCREMENT("table move");
        ++num_legal_moves;
        int score = SearchSingleMove(src_position, depth, ply, alpha, beta, cancel, transposition.move, &pv, 0);
        if (*cancel)
        {
            return SEARCH_CANCELLED_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("table move beta cutoffs");
            RecordTransposition(src_position->hash, depth, beta, transposition.move, NODE_CUT);
            RecordGoodMove(ply, transposition.move);
            return beta;
        }
        if (score > alpha)
        {
            INCREMENT("table move raised alpha");
            alpha = score;
            has_raised_alpha = true;
            best_move = transposition.move;
            /* 
            Provisionally store this move as a CUT node - it 
            may later turn out to be a PV but we don't know that yet.
            */
            RecordTransposition(src_position->hash, depth, alpha, transposition.move, NODE_CUT);
            RecordGoodMove(ply, transposition.move);
        }
    } 
    Move moves[MAX_MOVES_PER_POSITION];
    GeneratePseudoLegalMoves(src_position, moves);
    ScoreAndSortMoves(src_position, moves, ply);
    /* 
    This is the main move loop. 
    */
    for (const Move* move = moves; *move; ++move)
    {
        int score;
        /* Consider candidate move reductions. */
        if (!(src_position->state_flags & IS_CHECK)  && 
              num_legal_moves > 2                    &&
              MOVE_SCORE(*move) < 0                  &&
              beta == alpha + 1)
        {
            if (depth <= 1)
            {
                INCREMENT("negative SEE frontier skips");
                continue;
            }
            else
            {
                INCREMENT("negative SEE reduction attempts");
                score = SearchSingleMove(src_position, depth - 1, ply, alpha, beta, cancel, *move, &pv, num_legal_moves);
            }
        }
        else
        {
            score = SearchSingleMove(src_position, depth, ply, alpha, beta, cancel, *move, &pv, num_legal_moves);
        }
        if (*cancel)
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
            RecordTransposition(src_position->hash, depth, beta, *move, NODE_CUT);
            RecordGoodMove(ply, *move);
            return beta;
        }
        if (score > alpha)
        {
            INCREMENT("pv changed");
            alpha = score;
            has_raised_alpha = true;
            /* 
            Provisionally store this move as a CUT node - it 
            may later turn out to be a PV but we don't know that yet.
            */
            RecordTransposition(src_position->hash, depth, score, *move, NODE_CUT);
            RecordGoodMove(ply, *move);
            best_move = *move;
        }
    }
    /*
    End of main loop: we searched all moves and did not get a beta cutoff.
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
        if (src_position->state_flags & IS_CHECK)
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
        RecordTransposition(src_position->hash, depth, alpha, best_move, NODE_PV);
        RecordGoodMove(ply, best_move);
        CopyVariation(parent_pv, &pv, best_move);
    }
    else
    {
        /*
        We tried every move but did not raise alpha; this was an all node
        */
        INCREMENT("all nodes");
        RecordTransposition(src_position->hash, depth, alpha, best_move, NODE_ALL);
    }
    return alpha;
}