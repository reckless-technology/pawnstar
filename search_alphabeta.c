#include "pawnstar.h"
#pragma warning(disable:4221)

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
    Variation           pv;
    int                 pre_move[2];
    int                 captures[MAX_MOVES_PER_POSITION];
    int                 non_captures[MAX_MOVES_PER_POSITION]; 
    int                 num_legal_moves  = 0;
    int                 best_move        = 0;
    bool                has_raised_alpha = false;
    const int* const    move_phases[]    = 
    { 
        [PHASE_PRE_MOVES]        = pre_move,
        [PHASE_CAPTURES]         = captures,
        [PHASE_NON_CAPTURES]     = non_captures,
    };
    if (*cancel)
    {
        return SEARCH_CANCELLED_SCORE;
    }
    pv.num_moves = 0;
    if (!(++globals->node_count & 0xFFFF) &&
        globals->hard_stop_search_ms      &&
        GetMilliseconds() >= globals->hard_stop_search_ms)
    {
        *cancel = true;
        return SEARCH_CANCELLED_SCORE;
    }
    INCREMENT("alpha beta calls");
    if (IsDrawByRepetition(src_position, true) || 
        IsDrawByMaterial  (src_position)       || 
        IsDrawByFiftyMoves(src_position))
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
        ++depth;
        INCREMENT("checks");
    }
    else if (depth <= 0)
    {
        return SearchQuiescent(src_position, depth, ply, alpha, beta);
    } 
    /*
    Determine if there is an entry in the transposition table for this 
    position.
    */
    Transposition transposition;
    const bool is_transposition = FindTransposition(src_position->hash, &transposition, false);
    if (is_transposition && transposition.depth >= depth)
    {
        switch (transposition.node_type)
        {
        case NODE_CUT:
            /*
            We don't know the exact score of the best move from this
            position, but we do know it is at least transposition->score
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
            position, but we do know it is at most transposition->score
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
            break;
        }
    }
 
#if DO_NULL_MOVE_PRUNING
    /*
    Try null move pruning if ALL of the following are true:

    # the previous move was not a null move
    # we are not in check   
    # this is not a PV node
    # this is not the endgame
    # static eval is at least beta
    
    Hopefully this is sufficient to prevent most Zugzwang positions.
    */
    if ((src_position->move)                            &&
        !(src_position->state_flags & IS_CHECK)         &&
        beta == alpha + 1                               &&
        !IS_ENDGAME(src_position)                       &&
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
        if (score == beta)
        {
            return beta;
        }
        INCREMENT("null move fails");
    }
#endif
    
    /*
    Before we generate any moves, try any transposition table move first. 
    This might save us the cost of move generation, or provide better alpha 
    beta bounds for the main search.
    In any case, it's always a good idea to search the PV first.
    */
    if (is_transposition && transposition.move)
    {
        INCREMENT("table moves");
        pre_move[0] = transposition.move;
        pre_move[1] = 0;
    }   
    else
    {
        pre_move[0] = 0;
    }
    INCREMENT_IF(!is_transposition && depth > 1, "nodes without transposition");   
    /*
    Start of main loop - we go through the following move phases:
    
    1) pre move from the transposition table
    2) captures
    3) non captures
    
    Move generation is deferred until after the pre move has been searched,
    in the hope that in the case of a cutoff we don't actually have to bother
    with move generation at all.
    */
    for (int phase = PHASE_PRE_MOVES; phase <= PHASE_NON_CAPTURES; ++phase)
    {            
        switch (phase)
        {
        case PHASE_PRE_MOVES:
            break;

        case PHASE_CAPTURES:
            GeneratePseudoLegalMoves(src_position, captures, non_captures); 
            SortMoves(src_position, captures, ply, false);
            break;

        case PHASE_NON_CAPTURES:
            SortMoves(src_position, non_captures, ply, false);
            break;

        default:
            printf("ERROR: illegal move phase\n");
            break;
        }
        const int* moves_this_phase = move_phases[phase];
        while (*moves_this_phase)
        {
            int move = *moves_this_phase++;
            int score = SearchSingleMove(src_position, depth, ply, alpha, beta, cancel, move, &pv, num_legal_moves);
            if (*cancel)
            {
                return SEARCH_CANCELLED_SCORE;
            }
            if (score == MOVED_INTO_CHECK_SCORE)
            {
                continue;
            }
            ++num_legal_moves;
            if (score == beta)
            {
                INCREMENT("beta cutoffs");
                INCREMENT_IF(phase == PHASE_PRE_MOVES, "beta cutoffs from TT move");
                RecordTransposition(src_position->hash, depth, score, move, NODE_CUT, false);
                RecordGoodMove(ply, move);
                return beta;
            }
            if (score > alpha)
            {
                INCREMENT("pv changed");
                alpha = score;
                has_raised_alpha = true;
                /* Might turn out to be a PV node but record in the TT for now */
                RecordTransposition(src_position->hash, depth, score, move, NODE_CUT, false);
                RecordGoodMove(ply, move);
                best_move  = move;
            }
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
        We raised alpha but did not cutoff; this was a PV node
        */
        INCREMENT("pv nodes");
        RecordTransposition(src_position->hash, depth, alpha, best_move, NODE_PV, false);
        RecordGoodMove(ply, best_move);
        CopyVariation(parent_pv, &pv, best_move);
    }
    else
    {
        /*
        We tried every move but did not raise alpha; this was an all node
        */
        INCREMENT("all nodes");
        RecordTransposition(src_position->hash, depth, alpha, best_move, NODE_ALL, false);
    }
    return alpha;
}

