#include "pawnstar.h"
#pragma warning(disable:4221)
/******************************************************************************
Main search routine: fail-hard alpha beta with PVS
Refer to:
http://chessprogramming.wikispaces.com/Alpha-Beta
http://chessprogramming.wikispaces.com/Principal+Variation+Search
*******************************************************************************/


int 
Search(const Position*  src_position, 
       int              depth, 
       int              ply, 
       int              alpha, 
       int              beta, 
       volatile bool*   cancel, 
       int              search_flags,
       Variation*       parent_pv)
{
    Transposition       transposition;
    Variation           pv;
    int                 move;   
    int                 pre_moves[4];
    int                 regular_moves[MAX_MOVES_PER_POSITION];
    int                 deferred_moves[MAX_MOVES_PER_POSITION];    
    int                 score;   
    bool                is_transposition  = false;
    int*                deferred_move     = deferred_moves;
    int                 num_legal_moves   = 0;
    int                 best_move         = 0;
    bool                has_raised_alpha  = false;
    const int* const    move_phases[]     = 
    { 
        [PHASE_PRE_MOVES]       = pre_moves,
        [PHASE_REGULAR_MOVES]   = regular_moves,
        [PHASE_DEFERRED_MOVES]  = deferred_moves,
    };
    if (*cancel)
    {
        return ILLEGAL_SCORE;
    }
    InitVariation(&pv);
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
    /**************************************************************************
    Determine if there is an entry in the transposition table for this 
    position.
    ***************************************************************************/
    is_transposition = FindTransposition(src_position->hash, &transposition);
    if (is_transposition && transposition.depth >= depth)
    {
        switch (transposition.node_type)
        {
        case NODE_CUT:
            /**************************************************************
            We don't know the exact score of the best move from this
            position, but we do know it is at least transposition->score
            ***************************************************************/
            INCREMENT("table hit cut node");
            if (transposition.score >= beta)
            {
                INCREMENT("table hit beta cutoffs");
                return beta;
            }
            break;

        case NODE_ALL:
            /**************************************************************
            We don't know the exact score of the best move from this
            position, but we do know it is at most transposition->score
            ***************************************************************/
            INCREMENT("table hit all node");
            if (transposition.score <= alpha)
            {
                INCREMENT("table hit alpha cutoffs");
                return alpha;
            }
            break;

        case NODE_PV:
            /**************************************************************
            We know the exact score and the best move from this position.
            ***************************************************************/
            INCREMENT("table hit pv node");
            if (transposition.score > alpha && transposition.score < beta && transposition.move)
            {
                RecordPrincipalVariationMove(src_position->hash, transposition.move);
                CopyVariation(parent_pv, &pv, transposition.move);
            }
            //return transposition.score;
        }
    }
 
#if DO_NULL_MOVE_PRUNING
    /**************************************************************************
    Try null move pruning if ALL of the following are true:

    # the previous move was not a null move
    # we are not in check   
    # this is not a PV node
    # we are not down to king and pawns
    
    Hopefully this is sufficient to prevent most Zugzwang positions.
    ***************************************************************************/
    if ((search_flags & IS_NULL_MOVE_OK)        &&
        (src_position->move)                    &&
        !(src_position->state_flags & IS_CHECK) &&
        beta == alpha + 1                       &&
        (src_position->pieces_of_color[COLOR_TO_MOVE(src_position)] & ~(src_position->pawns | src_position->kings)) &&
        EvaluatePosition(src_position, alpha, beta) >= beta)
    {
        Position position;
        INCREMENT("null move attempts");
        MakeNullMove(&position, src_position);
        score = -Search(&position, depth - 4, ply + 1, -beta, -alpha, cancel, search_flags & ~IS_NULL_MOVE_OK, NULL);
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
    /**************************************************************************
    Futility pruning doesn't really achieve much; the idea is to prune frontier
    nodes where the eval is so bad there is no way we can match alpha even with
    a great winning tactical sequence.
    ***************************************************************************/
    if (depth == 1                              &&
        !(src_position->state_flags & IS_CHECK) &&
        EvaluatePosition(src_position, alpha, beta) + FUTILITY_CUTOFF_THRESHOLD < alpha)
    {
        INCREMENT("futility cutoffs");
        return alpha;
    }
#endif

    /**************************************************************************
    Before we generate any moves, try any principal variation table or 
    transposition table moves first. This might save us the cost of move 
    generation, or provide better alpha beta bounds for the main search.
    In any case, it's always a good idea to search the PV first.
    ***************************************************************************/
    int* m = pre_moves;
    if ((move = GetPrincipalVariationMove(src_position->hash)) != 0)
    {
        INCREMENT("pre moves - PV");
        *m++ = move;
    }
    else
    {
        search_flags &= ~IS_FOLLOWING_PV;
        if (is_transposition && transposition.move)
        {
            INCREMENT("pre moves - TT");
            *m++ = transposition.move;
        }
    }    
    *m = 0; /* terminate pre moves list */
    if (!is_transposition && depth > 1)
    {
        INCREMENT("nodes without transposition");
    }    
    /**************************************************************************
    Start of main loop - we go through the following move phases:

    1) Pre move from the PV or TT
    2) Regular moves with a SEE >= 0 (sorted best first)
    3) Deferred moves with a SEE < 0

    Move generation is deferred until after pre moves have been searched.
    ***************************************************************************/
    for (int phase = PHASE_PRE_MOVES; phase <= PHASE_DEFERRED_MOVES; ++phase)
    {            
        switch (phase)
        {
        case PHASE_PRE_MOVES:
            search_flags &= ~(IS_DEFERRED_MOVE | IS_PVS_OK);
            break;

        case PHASE_REGULAR_MOVES:
            GeneratePseudoLegalMoves(src_position, regular_moves, true);
            SortMoves(regular_moves, ply);
            break;

        case PHASE_DEFERRED_MOVES:
            *deferred_move = 0; /* terminate deferred moves list */
            search_flags |= IS_DEFERRED_MOVE;
            break;

        default:
            printf("ERROR: illegal move phase\n");
            break;
        }
        const int* moves_this_phase = move_phases[phase];
        while ((move = *moves_this_phase++) != 0)
        {
            if (phase == PHASE_REGULAR_MOVES && EvaluateStaticExchange(src_position, move) < 0)
            {
                /* defer moves with a negative static exchange evaluation for later consideration */
                *deferred_move++ = move;
                INCREMENT("deferred moves");
                continue;
            }
            score = SearchSingleMove(src_position, depth, ply, alpha, beta, cancel, search_flags, move, &pv);
            if (*cancel)
            {
                return ILLEGAL_SCORE;
            }
            if (score == MOVED_INTO_CHECK_SCORE)
            {
                continue;
            }
            ++num_legal_moves;
            search_flags |= IS_PVS_OK; /* allow PVS after first legal move */
            if (score >= beta)
            {
                INCREMENT("beta cutoffs");
                RecordTransposition(src_position->hash, depth, beta, move, NODE_CUT);
                RecordGoodMove(ply, move);
                return beta;
            }
            if (score > alpha)
            {
                INCREMENT("pv changed");
                has_raised_alpha = true;
                alpha = score;
                best_move = move;
                RecordGoodMove(ply, move);
            }
        }       
    }    
    /**************************************************************************
    End of main loop: we searched all moves and did not get a beta cutoff.
    ***************************************************************************/
    if (num_legal_moves == 0)
    {
        /**********************************************************************
        We failed to find any strictly legal moves from this position.
        The position therefore represents either checkmate or stalemate 
        depending on whether we are presently in check.
        If we are in checkmate, add the ply (distance from the root node) to  
        the losing score, so that the search algorithm chooses the shortest  
        path to checkmate when multiple possible checkmates exist in the tree.
        ***********************************************************************/
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
        /**********************************************************************
        We raised alpha but did not cutoff; this was a PV node
        ***********************************************************************/
        INCREMENT("pv nodes");
        RecordPrincipalVariationMove(src_position->hash, best_move);
        RecordTransposition(src_position->hash, depth, alpha, best_move, NODE_PV);
        RecordGoodMove(ply, best_move);
        CopyVariation(parent_pv, &pv, best_move);
    }
    else
    {
        /**********************************************************************
        We tried every move but did not raise alpha; this was an all node
        ***********************************************************************/
        INCREMENT("all nodes");
        RecordTransposition(src_position->hash, depth, alpha, best_move, NODE_ALL);       
    }
    return alpha;
}

