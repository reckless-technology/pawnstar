#include "pawnstar.h"
/*
Fairly standard alpha-beta quiescence search
Refer to: http://chessprogramming.wikispaces.com/Quiescence+Search
*/
int SearchQuiescent(const Position* src_position, 
                    int depth,
                    int ply, 
                    int alpha, 
                    int beta, 
                    volatile bool* cancel)
{    
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
        return Search(src_position, depth, ply, alpha, beta, cancel, NULL);
    }
    int score = EvaluatePosition(src_position, alpha, beta);
    if (score >= beta)
    {
        INCREMENT("quiescent eval beta cutoffs");
        return score;
    }
    if (score > alpha)
    {
        INCREMENT("quiescent eval raises alpha");
        alpha = score;
    }

#if DO_FUTILITY_PRUNING
    else if (score + FUTILITY_CUTOFF_THRESHOLD < alpha)
    {
        INCREMENT("quiescent futility cutoffs");
        return alpha;
    }
#endif

    int best_score = score;

    int captures[MAX_MOVES_PER_POSITION];
    GeneratePseudoLegalMoves(src_position, captures, NULL);  
    SortMoves(src_position, captures, ply, false);
    /*
    Main loop
    */
    int* pmove = captures;
    while (*pmove)
    {
        const int move = *pmove++;
        Position position;

#if DO_QUIESCENCE_STATIC_EXCHANGE_EVAL
        if (EvaluateStaticExchange(src_position, move) < 0)
        {
            INCREMENT("quiescent SEE skips");
            continue;
        }
#endif

        MakeMove(&position, src_position, move);
        if (position.state_flags & MOVED_INTO_CHECK)
        {
            continue;
        }
        score = -SearchQuiescent(&position, depth - 1, ply + 1, -beta, -alpha, cancel);
        if (*cancel)
        {
            return ILLEGAL_SCORE;
        }
        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            return score;
        }
        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;            
                INCREMENT("quiescent pv changed");
            }
        }      
    }
    INCREMENT("quiescent all nodes");
    return best_score;
}