#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"
#include "search.h"

/**
 * @brief Alpha beta quiescence (capture only) search.
 * @param src_position position to search
 * @param depth search depth (<= 0 for quiescence)
 * @param ply distance from root node
 * @param alpha parent floor value
 * @param beta parent ceiling value
 * @return score
*/
int SearchQuiescent(const Position& position, 
                    int             depth,
                    int             ply, 
                    int             alpha, 
                    int             beta,
                    volatile bool&  cancel)
{    
    INCREMENT("quiescent calls");
    if (ply == MAX_PLY)
    {
        INCREMENT("quiescent max ply");
        return EvaluatePosition(position, alpha, beta);
    }
    if (position.flags_ & IS_CHECK)
    {
        INCREMENT("quiescent checks");
        Variation dummy;
        dummy.moves[0] = 0;
        return Search(position, depth, ply, alpha, beta, cancel, dummy);
    }
    int score = EvaluatePosition(position, alpha, beta);
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
    MoveList move_list;
    position.GeneratePseudoLegalCaptures(move_list);
    ScoreAndSortMoves(position, move_list.moves, ply, depth);
    for (auto move : move_list)
    {
        if (MoveScore(move) < 0)
        {
            INCREMENT("negative SEE quiescent skips");
            continue;
        }
        Position child_position { position, move };
        if (child_position.flags_ & IS_MOVED_INTO_CHECK)
        {
            INCREMENT("quiescent moved into check");
            continue;
        }
        score = -SearchQuiescent(child_position, depth - 1, ply + 1, -beta, -alpha, cancel);
        if (score >= beta)
        {
            INCREMENT("quiescent beta cutoffs");
            RecordGoodMove(ply, move);
            return beta;
        }
        if (score > alpha)
        {
            alpha = score;
            INCREMENT("quiescent pv changed");
            RecordGoodMove(ply, move);
        }      
    }
    return alpha;
}