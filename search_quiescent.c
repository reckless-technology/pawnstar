#include "pawnstar.h"
/******************************************************************************
Fairly standard alpha-beta quiescence search
Refer to: http://chessprogramming.wikispaces.com/Quiescence+Search
*******************************************************************************/
int SearchQuiescent(const Position* src_position, 
                    int depth,
                    int ply, 
                    int alpha, 
                    int beta, 
                    volatile bool* cancel)
{
    int         moves[MAX_MOVES_PER_POSITION];
    const int*  pmove = moves;
    int         move;
    int         score;
    bool        is_first_move = true;
    
    INCREMENT("quiescent calls");
    if (*cancel)
    {
        return ILLEGAL_SCORE;
    }
    if (ply == MAX_PLY)
    {
        INCREMENT("quiescent max ply");
        return EvaluatePosition(src_position, alpha, beta);
    }
    if (src_position->state_flags & IS_CHECK)
    {
        INCREMENT("quiescent checks");
        /**********************************************************************
        Standing pat is not allowed in check - we must do a full search
        ***********************************************************************/
        return Search(src_position, depth, ply, alpha, beta, cancel);
    }
    score = EvaluatePosition(src_position, alpha, beta);
    if (score >= beta)
    {
        INCREMENT("quiescent eval beta cutoffs");
        return beta;
    }
    if (score > alpha)
    {
        INCREMENT("quiescent eval raises alpha");
        alpha = score;
    }
    else if (score + FUTILITY_CUTOFF_THRESHOLD < alpha)
    {
        INCREMENT("quiescent futility cutoffs");
        return alpha;
    }
    GeneratePseudoLegalMoves(src_position, moves, false);    
    SortMoves(moves, ply);
    /**************************************************************************
    Main loop
    ***************************************************************************/
    while ((move = *pmove++) != 0)
    {
        Position position[1];
#if DO_QUIESCENCE_STATIC_EXCHANGE_EVAL
        if (EvaluateStaticExchange(src_position, move) <= 0)
        {
            INCREMENT("quiescent SEE skips");
            continue;
        }
#endif
        MakeMove(position, src_position, move);
        if (position->state_flags & MOVED_INTO_CHECK)
        {
            continue;
        }
        if (is_first_move)
        {
            score = -SearchQuiescent(position, depth - 1, ply + 1, -beta, -alpha, cancel);
            is_first_move = false;
        }
        else
        {
            INCREMENT("quiescent pvs attempts");
            score = -SearchQuiescent(position, depth - 1, ply + 1, -alpha - 1, -alpha, cancel);
            if (score > alpha)
            {
                INCREMENT("quiescent pvs fails");
                score = -SearchQuiescent(position, depth - 1, ply + 1, -beta, -alpha, cancel);
            }
        }
        if (*cancel)
        {
            return ILLEGAL_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            return beta;
        }
        if (score > alpha)
        {
            alpha = score;            
            INCREMENT("quiescent pv changed");
        }
    }
    return alpha;
}