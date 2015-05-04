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
        return Search(src_position, depth, ply, alpha, beta, cancel, 0);
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
    int legal_move_count = 0;
    while ((move = *pmove++) != 0)
    {
        Position position[1];

#if DO_QUIESCENCE_STATIC_EXCHANGE_EVAL
        if (EvaluateStaticExchange(src_position, move) < 0)
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
        if (beta > alpha + 1 && legal_move_count)
        {
            INCREMENT("quiescent pvs attempts");
            score = -SearchQuiescent(position, depth - 1, ply + 1, -alpha - 1, -alpha, cancel);
            if (score > alpha)
            {
                INCREMENT("quiescent pvs fails");
                score = -SearchQuiescent(position, depth - 1, ply + 1, -beta, -alpha, cancel);
            }
        }
        else
        {
            score = -SearchQuiescent(position, depth - 1, ply + 1, -beta, -alpha, cancel);
        }
        ++legal_move_count;
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