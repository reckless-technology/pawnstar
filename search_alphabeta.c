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
           volatile bool* cancel)
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
    const int* const    move_phases[4]    = { pre_moves, regular_moves, deferred_moves, NULL };
    const int* const *  move_phase;

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
    if (is_transposition)
    {
        Position position[1];
        MakeMove(position, src_position, transposition->move);
        if (IsDrawByRepetition(position, true))
        {
            /******************************************************************
            We cannot trust this transposition table entry since it resulted
            in a draw by repetition when played here.
            *******************************************************************/
            INCREMENT("table hit caused draw by repetition");
            is_transposition = false;
        }
        else if (transposition->depth >= depth)
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
    }
 
#if DO_NULL_MOVE_PRUNING
    /**************************************************************************
    Try null move pruning if ALL of the following are true:

    # we are not in check 
    # this is not the root node
    # the previous move was not a null move    
    # we are not down to king and pawns
    # we have at least 4 pieces remaining
    # the eval score is high enough for a beta cutoff 

    Hopefully this is sufficient to prevent most Zugzwang positions.

    Null move pruning makes a huge improvement to search depth / speed. 
    ***************************************************************************/
    if (!(src_position->state_flags & IS_CHECK) &&
        ply != 0                                &&
        src_position->move)
    {
        const bitboard friendly_pieces = (src_position->occupied_squares ^ src_position->kings) & src_position->pieces_of_color[COLOR_TO_MOVE(src_position)];
        if ((friendly_pieces & ~src_position->pawns) && 
            PopCount(friendly_pieces) > 4            && 
            EvaluatePosition(src_position, alpha, beta) >= beta)
        {
            Position position[1];
            INCREMENT("null move attempts");
            MakeNullMove(position, src_position);
            score = -Search(position, depth - 3, ply + 1, -beta, -beta + 1, cancel);
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
    for (move_phase = move_phases; *move_phase; ++move_phase)
    {       
        const int* moves_this_phase = *move_phase;
        const bool is_deferred = (*move_phase == deferred_moves);
        if (*move_phase == regular_moves)
        {
            GeneratePseudoLegalMoves(src_position, regular_moves, true);
            SortMoves(regular_moves, ply);
        }
        while ((move = *moves_this_phase++) != 0)
        {
            if (*move_phase == regular_moves && EvaluateStaticExchange(src_position, move) < 0)
            {
                /* defer moves with a negative static exchange evaluation for later consideration */
                *deferred_move++ = move;
                INCREMENT("deferred moves");
                continue;
            }
            score = SearchSingleMove(src_position, depth, ply, alpha, beta, move, num_legal_moves, is_deferred, cancel);
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
        *deferred_move = 0; /* terminate deferred moves list */
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
                     int move, 
                     int move_index,
                     bool is_deferred_move,
                     volatile bool* cancel)
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
    Extend the search depth if ANY of the following are true:
    # We are in check
    # This is a pawn promotion
    # This is a pawn push to the 7th rank 
    *******************************************************************/
    if (src_position->state_flags & IS_CHECK)
    {
        INCREMENT("extensions check");
        child_depth = depth;
    }
    else if (MOVE_PROMOTED(move))
    {
        INCREMENT("extensions promotion");
        child_depth = depth;
    }
    else if (MOVE_PIECE(move) == PAWN && 
             RANK_OF(MOVE_TO(move)) == SEVENTH_RANK[COLOR_TO_MOVE(src_position)])
    {
        INCREMENT("extensions push to 7th");
        child_depth = depth;
    }

#if DO_RECAPTURE_EXTENSION
    else if (MOVE_CAPTURED(move)                          &&
             MOVE_TO(move) == MOVE_TO(src_position->move) &&
             CLASSICAL_MATERIAL[MOVE_CAPTURED(move)] == CLASSICAL_MATERIAL[MOVE_CAPTURED(src_position->move)])
    {
        INCREMENT("extensions recapture");
        child_depth = depth;
    }
#endif

#if DO_LATE_MOVE_REDUCTION
    /**************************************************************************
    Reduce the search depth if ALL of the following are true:
    # The move was deferred due to negative SEE
    # Search depth is greater than 2    
    # The move is not a capture
    # The move does not give check
    # The move has never raised alpha at this ply
    ***************************************************************************/
    else if (is_deferred_move            &&
             !MOVE_CAPTURED(move)        &&
             !HasMoveBeenGood(ply, move) &&
             !(position->state_flags & IS_CHECK))
    {
        INCREMENT("extensions reduce LMR");
        child_depth = depth - 2;
    }
#endif
   
    else
    {
        child_depth = depth - 1;
    }
    if (move_index != 0 && !(src_position->state_flags & IS_CHECK))
    {
        INCREMENT("pvs attempts");
        score = -Search(position, child_depth, ply + 1, -alpha - 1, -alpha, cancel);
        if (score > alpha)
        {
            INCREMENT("pvs fails");
            score = -Search(position, child_depth, ply + 1, -beta, -alpha, cancel);
        }
    }
    else
    {
        score = -Search(position, child_depth, ply + 1, -beta, -alpha, cancel);
    }
    return score;
    (void)is_deferred_move;
}