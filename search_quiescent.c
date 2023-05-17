#include "pawnstar.h"

/**
 * @brief Alpha beta quiescence (capture only) search.
 * @param src_position position to search
 * @param depth search depth (<= 0 for quiescence)
 * @param ply distance from root node
 * @param alpha parent floor value
 * @param beta parent ceiling value
 * @return score
*/
int SearchQuiescent(const Position* src_position, 
                    int depth,
                    int ply, 
                    int alpha, 
                    int beta)
{    
    INCREMENT("quiescent calls");
    if (ply == MAX_PLY)
    {
        INCREMENT("quiescent max ply");
        return EvaluatePosition(src_position, alpha, beta);
    }
    if (src_position->state_flags & IS_CHECK)
    {
        INCREMENT("quiescent checks");
        return alpha;
    }
    Transposition transposition;
    const bool is_transposition = FindTransposition(src_position->hash, &transposition, true);
    if (is_transposition)
    {
        INCREMENT("quiescent TT hits");
        if (transposition.score == beta)
        {
            INCREMENT("quiescent TT hit beta cutoffs");
            return beta;
        }
    }
    int score = EvaluatePosition(src_position, alpha, beta);
    if (score >= beta)
    {
        INCREMENT("quiescent eval beta cutoffs");
        RecordTransposition(src_position->hash, depth, beta, 0, NODE_CUT, true);
        return beta;
    }
    if (score > alpha)
    {
        INCREMENT("quiescent eval raises alpha");
        alpha = score;
    }
    int captures[MAX_MOVES_PER_POSITION];
    if (is_transposition && transposition.move)
    {
        INCREMENT("quiescent TT moves");
        captures[0] = transposition.move;
        GeneratePseudoLegalMoves(src_position, captures + 1, NULL);
        SortMoves(src_position, captures + 1, ply, false);
    }
    else
    {
        GeneratePseudoLegalMoves(src_position, captures, NULL);
        SortMoves(src_position, captures, ply, false);
    }
    for (const int* move = captures; *move; ++move)
    {
#if DO_QUIESCENCE_STATIC_EXCHANGE_EVAL
        if (EvaluateStaticExchange(src_position, *move) < 0)
        {
            INCREMENT("quiescent SEE skips");
            continue;
        }
#endif
        Position position;
        MakeMove(&position, src_position, *move);
        if (position.state_flags & IS_MOVED_INTO_CHECK)
        {
            continue;
        }
        score = -SearchQuiescent(&position, depth - 1, ply + 1, -beta, -alpha);
        if (score == beta)
        {
            INCREMENT("quiescent beta cutoffs");
            RecordGoodMove(ply, *move);
            RecordTransposition(src_position->hash, depth, score, *move, NODE_CUT, true);
            return beta;
        }
        if (score > alpha)
        {
            alpha = score;
            INCREMENT("quiescent pv changed");
            RecordTransposition(src_position->hash, depth, score, *move, NODE_CUT, true);
            RecordGoodMove(ply, *move);
        }      
    }
    return alpha;
}