#include "pawnstar.h"
#pragma warning(disable:4221)
/******************************************************************************
Main search routine: fail-hard alpha beta with PVS
Refer to:
http://chessprogramming.wikispaces.com/Alpha-Beta
http://chessprogramming.wikispaces.com/Principal+Variation+Search
*******************************************************************************/
static const int SEVENTH_RANK[2]       = { 6, 1 };
static const int CLASSICAL_MATERIAL[7] = { 0, 1, 3, 3, 5, 9, 0 };

int Search(const Position* src_position, 
           int depth, 
           int ply, 
           int alpha, 
           int beta, 
           volatile bool* cancel, 
           int search_flags)
{
    Transposition       transposition[1];
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
    ++globals->node_count;
    INCREMENT("alpha beta calls");
    INCREMENT_IF(src_position->state_flags & IS_CHECK, "checks");
    /**************************************************************************
    This is a leaf node which requires no further searching if either of the 
    following are true: 

    # it is a drawn position by repetition, material or the 50-move rule
    # we have reached the absolute maximum ply of search
    ***************************************************************************/
    if (IsDrawByRepetition(src_position, true) || 
        IsDrawByMaterial(src_position)         || 
        IsDrawByFiftyMoves(src_position))
    {
        return DRAW_SCORE;
    }
    if (ply == MAX_PLY)
    {
        INCREMENT("max ply reached");
        return EvaluatePosition(src_position, alpha, beta);
    }
    if (!(src_position->state_flags & IS_CHECK) && 
        depth <= 0)
    {
        return SearchQuiescent(src_position, depth, ply, alpha, beta, cancel);
    }
    /**************************************************************************
    Determine if there is an entry in the transposition table for this 
    position.
    ***************************************************************************/
    is_transposition = FindTransposition(src_position->hash, transposition);
    if (is_transposition && transposition->depth >= depth)
    {
        switch (transposition->node_type)
        {
        case NODE_CUT:
            /**************************************************************
            We don't know the exact score of the best move from this
            position, but we do know it is at least transposition->score
            ***************************************************************/
            INCREMENT("table hit cut node");
            if (transposition->score >= beta)
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
            if (transposition->score <= alpha)
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
            if (transposition->score > alpha && transposition->score < beta && transposition->move)
            {
                RecordPrincipalVariationMove(src_position->hash, transposition->move);
            }
            return transposition->score;
        }
    }
 
#if DO_NULL_MOVE_PRUNING
    /**************************************************************************
    Try null move pruning if ALL of the following are true:

    # we haven't already tried it further up this path in the tree
    # we are not in check   
    # this is not a PV node
    # we are not down to king and pawns
    # the eval score is high enough for a beta cutoff 
    
    Hopefully this is sufficient to prevent most Zugzwang positions.
    ***************************************************************************/
    if ((search_flags & IS_NULL_MOVE_OK)                                                                            &&
        !(src_position->state_flags & IS_CHECK)                                                                     &&
        alpha == beta - 1                                                                                           && 
        (src_position->pieces_of_color[COLOR_TO_MOVE(src_position)] & ~(src_position->pawns | src_position->kings)) &&
        EvaluatePosition(src_position, alpha, beta) >= beta)
    {
        Position position[1];
        INCREMENT("null move attempts");
        MakeNullMove(position, src_position);
        score = -Search(position, depth - 3, ply + 1, -beta, -beta + 1, cancel, search_flags & ~IS_NULL_MOVE_OK);
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
    Before we generate any moves, try any principal variation table and 
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
    }
    if (is_transposition           && 
        transposition->move        && 
        transposition->move != move)
    {
        INCREMENT("pre moves - TT");
        *m++ = transposition->move;
    }
    *m = 0; /* terminate pre moves list */
    if (!is_transposition && depth > 1)
    {
        INCREMENT("nodes without transposition");
    }    
    /**************************************************************************
    Start of main loop - we go through the following move phases:

    1) Pre moves from the PV and TT
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
            score = SearchSingleMove(src_position, depth, ply, alpha, beta, cancel, search_flags, move);
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

/******************************************************************************
Search a single move and return its score, or MOVED_INTO_CHECK_SCORE if the 
move was not legal.
*******************************************************************************/
int SearchSingleMove(const Position* src_position, 
                     int depth, 
                     int ply, 
                     int alpha, 
                     int beta, 
                     volatile bool* cancel, 
                     int search_flags, 
                     int move)
{  
    Position position[1];
    int child_depth;
    int score;
    if (*cancel)
    {
        return ILLEGAL_SCORE;
    }
    MakeMove(position, src_position, move);
    if (position->state_flags & MOVED_INTO_CHECK)
    {
        return MOVED_INTO_CHECK_SCORE;
    }
    /******************************************************************
    Extend the search depth if any of the following are true:    
    # We are in check
    # We have been following the principal variation from the root node
    # This is a pawn promotion
    # This is a pawn push to the 7th rank 
    # Recapture of same value piece
    *******************************************************************/
    if (src_position->state_flags & IS_CHECK)
    {
        INCREMENT("extensions check");
        child_depth = depth;
    }

    else if ((search_flags & IS_FOLLOWING_PV) && depth == 1)
    {
        INCREMENT("extensions following PV");
        child_depth = depth;
    }

    else if (MOVE_PROMOTED(move))
    {
        INCREMENT("extensions promotion");
        child_depth = depth;
    }

#if DO_PUSH_TO_SEVENTH_RANK_EXTENSION
    else if (MOVE_PIECE(move) == PAWN && 
             RANK_OF(MOVE_TO(move)) == SEVENTH_RANK[COLOR_TO_MOVE(src_position)])
    {
        INCREMENT("extensions push to 7th");
        child_depth = depth;
    }
#endif

#if DO_RECAPTURE_EXTENSION
    else if (MOVE_CAPTURED(move) && CLASSICAL_MATERIAL[MOVE_CAPTURED(move)] == CLASSICAL_MATERIAL[MOVE_CAPTURED(src_position->move)])
    {
        INCREMENT("extensions recapture");
        child_depth = depth;
    }
#endif

#if DO_LATE_MOVE_REDUCTION
    /**************************************************************************
    Reduce the search depth if ALL of the following are true:
    # We did not already do LMR further up the tree
    # The move was deferred due to negative SEE
    # The move is not a capture  
    # Depth is at least 3
    # The move does not give check
    ***************************************************************************/
    else if ((search_flags & IS_LMR_OK)        &&
             (search_flags & IS_DEFERRED_MOVE) &&
             !MOVE_CAPTURED(move)              &&
             depth > 2                         &&
             !(position->state_flags & IS_CHECK))
    {
        INCREMENT("extensions reduce LMR");
        child_depth = depth - 2;
        search_flags &= ~IS_LMR_OK;
    }
#endif
   
    else
    {
        child_depth = depth - 1;
    }

    if ((search_flags & IS_PVS_OK)              && 
        !(src_position->state_flags & IS_CHECK) &&
        beta > alpha + 1)
    {
        INCREMENT("pvs attempts");
        score = -Search(position, child_depth, ply + 1, -alpha - 1, -alpha, cancel, search_flags);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            score = -Search(position, child_depth, ply + 1, -beta, -alpha, cancel, search_flags);
        }
    }
    else
    {
        score = -Search(position, child_depth, ply + 1, -beta, -alpha, cancel, search_flags);
    }
    return score;
}