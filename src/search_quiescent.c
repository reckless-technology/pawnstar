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
                    int             depth,
                    int             ply, 
                    int             alpha, 
                    int             beta,
                    volatile bool*  cancel)
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
        return Search(src_position, depth, ply, alpha, beta, cancel, NULL);
    }
    int score = EvaluatePosition(src_position, alpha, beta);
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
    Move moves[MAX_MOVES_PER_POSITION];
    GeneratePseudoLegalCaptures(src_position, moves);
    ScoreAndSortMoves(src_position, moves, ply, depth);
    for (const Move* move = moves; *move; ++move)
    {
        if (MOVE_SCORE(*move) < 0)
        {
            INCREMENT("negative SEE quiescent skips");
            continue;
        }
        Position position;
        MakeMove(&position, src_position, *move);
        if (position.state_flags & IS_MOVED_INTO_CHECK)
        {
            INCREMENT("quiescent moved into check");
            continue;
        }
        score = -SearchQuiescent(&position, depth - 1, ply + 1, -beta, -alpha, cancel);
        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            RecordGoodMove(ply, *move);
            return beta;
        }
        if (score > alpha)
        {
            alpha = score;
            INCREMENT("quiescent pv changed");
            RecordGoodMove(ply, *move);
        }      
    }
    return alpha;
}