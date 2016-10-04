#include "pawnstar.h"
#pragma warning(disable:4221)
/*
Main search routine: fail-hard alpha beta with PVS
Refer to:
http://chessprogramming.wikispaces.com/Alpha-Beta
http://chessprogramming.wikispaces.com/Principal+Variation+Search
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
    Variation   pv;
    int         move;   
    int         pre_move[2];
    int         captures[MAX_MOVES_PER_POSITION];
    int         non_captures[MAX_MOVES_PER_POSITION];
    int         deferred_moves[MAX_MOVES_PER_POSITION];    
    int         score;   
    int*        deferred_move    = deferred_moves;
    int         num_legal_moves  = 0;
    int         best_move        = 0;
    int         best_score       = ALPHA;
    bool        has_raised_alpha = false;
    int* const  move_phases[]    = 
    { 
        [PHASE_PRE_MOVES]        = pre_move,
        [PHASE_CAPTURES]         = captures,
        [PHASE_NON_CAPTURES]     = non_captures,
        [PHASE_DEFERRED_MOVES]   = deferred_moves,
    };
    if (*cancel)
    {
        return ILLEGAL_SCORE;
    }
    pv.num_moves = 0;
    ++globals->node_count;
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
    if (!(src_position->state_flags & IS_CHECK))
    {
        if (depth <= 0)
        {
            return SearchQuiescent(src_position, depth, ply, alpha, beta, cancel);
        }
    }
    else
    {
        INCREMENT("checks");
        ++depth;
    }    
    /*
    Determine if there is an entry in the transposition table for this 
    position.
    */
    Transposition transposition;
    const bool is_transposition = FindTransposition(src_position->hash, &transposition);
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
    # we have at least 5 pieces on the board
    # we are not down to king and pawns
    # static eval is at least beta
    
    Hopefully this is sufficient to prevent most Zugzwang positions.
    */
    const bitboard friendly_pieces = src_position->pieces_of_color[COLOR_TO_MOVE(src_position)];
    if ((src_position->move)                                               &&
        !(src_position->state_flags & IS_CHECK)                            &&
        beta == alpha + 1                                                  &&
        PopCount(friendly_pieces) > 4                                      &&
        !!(friendly_pieces & ~(src_position->pawns | src_position->kings)))
    {
        INCREMENT("null move attempts");
        Position position;       
        MakeNullMove(&position, src_position);
        score = -Search(&position, depth - 3, ply + 1, -beta, -alpha, cancel, NULL);
        if (*cancel)
        {
            return ILLEGAL_SCORE;
        }
        if (score >= beta)
        {
            return beta;
        }
        INCREMENT("null move fails");
    }
#endif
    
#if DO_FUTILITY_PRUNING
    /*
    Futility pruning doesn't really achieve much; the idea is to prune frontier
    nodes (depth 1) where the eval is so bad there is no way we can match alpha 
    even with a great winning tactical sequence.
    */
    if (depth == 1                              &&
        !(src_position->state_flags & IS_CHECK) &&
        EvaluatePosition(src_position, alpha, beta) + FUTILITY_CUTOFF_THRESHOLD < alpha)
    {
        INCREMENT("futility cutoffs");
        return alpha;
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
    if (!is_transposition && depth > 1)
    {
        INCREMENT("nodes without transposition");
    }    
    /*
    Start of main loop - we go through the following move phases:

    1) Pre move from the PV or TT
    2) Regular moves with a SEE >= 0 (sorted "best first")
    3) Deferred moves with a SEE < 0

    Move generation is deferred until after the pre move has been searched.
    */
    int search_depth = depth;
    for (int phase = PHASE_PRE_MOVES; phase <= PHASE_DEFERRED_MOVES; ++phase)
    {            
        switch (phase)
        {
        case PHASE_PRE_MOVES:
            break;

        case PHASE_CAPTURES:
            GeneratePseudoLegalMoves(src_position, captures, non_captures); 
            SortMoves(captures, ply);
            break;

        case PHASE_NON_CAPTURES:
            SortMoves(non_captures, ply);
            break;

        case PHASE_DEFERRED_MOVES:
            *deferred_move = 0; // Terminate the deferred moves list.

#if DO_LATE_MOVE_REDUCTION
            if (!(src_position->state_flags & IS_CHECK))
            {
                INCREMENT("lmr");
                --search_depth;
            }
#endif
            break;

        default:
            printf("ERROR: illegal move phase\n");
            break;
        }
        int* moves_this_phase = move_phases[phase];
        while (*moves_this_phase)
        {
            switch (phase)
            {
            case PHASE_CAPTURES:
            case PHASE_NON_CAPTURES:
                move = *moves_this_phase++;
                if (EvaluateStaticExchange(src_position, move) < 0)
                {
                    // Defer searching moves with a negative SEE.
                    *deferred_move++ = move;
                    INCREMENT("deferred moves");
                    continue;
                }
                break;

            default:
                move = *moves_this_phase++;
                break;
            }              
            score = SearchSingleMove(src_position, search_depth, ply, alpha, beta, cancel, move, &pv, num_legal_moves);
            if (*cancel)
            {
                return ILLEGAL_SCORE;
            }
            if (score == MOVED_INTO_CHECK_SCORE)
            {
                continue;
            }
            ++num_legal_moves;
            if (score >= beta)
            {
                INCREMENT("beta cutoffs");
                INCREMENT_IF(phase == PHASE_PRE_MOVES, "beta cutoffs from TT move");
                RecordTransposition(src_position->hash, depth, score, move, NODE_CUT);
                RecordGoodMove(ply, move);
                return score;
            }
            if (score > best_score)
            {
                best_score = score;
                best_move  = move;
                if (score > alpha)
                {
                    INCREMENT("pv changed");
                    has_raised_alpha = true;
                    RecordGoodMove(ply, move);
                    alpha = score;
                }
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
        RecordTransposition(src_position->hash, depth, best_score, best_move, NODE_ALL);       
    }
    return alpha;
}

