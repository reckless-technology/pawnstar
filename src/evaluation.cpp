#include "position.h"
#include "debug_hashtable.h"
#include "transposition_table.h"
#include "function_prototypes.h"
#include "evaluation.h"

typedef struct EvalHash
{
    uint64_t    hash;
    int         score;
} EvalHash;

constexpr int EVAL_HASH_SIZE = 100003; /* Prime number */

static EvalHash eval_hash_table[EVAL_HASH_SIZE];

/**
 * @brief Evaluate the current position, assuming neither king is in check 
   and the position is quiet.
 * @param position position to evaluate
 * @param alpha current alpha value
 * @param beta current beta value
 * @return score from the perspective of the side to move
*/
int 
EvaluatePosition(const Position& position, 
                 int alpha, 
                 int beta)
{    
    INCREMENT("eval calls");
    EvalHash& eval_hash = eval_hash_table[position.hash_ % EVAL_HASH_SIZE];
    if (eval_hash.hash == position.hash_)
    {
        INCREMENT("eval hash table hits");
        return eval_hash.score;
    }
    if (position.IsDrawByMaterial())
    {
        eval_hash.hash = position.hash_;
        eval_hash.score = DRAW_SCORE;
        return DRAW_SCORE;
    }
    int scores[2];
    /* Phase 1: Evaluate material values alone. */
    scores[WHITE] = EvaluateMaterial<WHITE>(position);
    scores[BLACK] = EvaluateMaterial<BLACK>(position);
    /* 
    See if material score alone causes alpha or beta cutoff. 
    This saves time doing more expensive evaluation when clearly it's
    not a PV node so evaluation accuracy is not as important. 
    */
    int score = (position.flags_ & IS_BLACK_TO_MOVE) ?
        scores[BLACK] - scores[WHITE] :
        scores[WHITE] - scores[BLACK];
    if (score >= beta + 150)
    {
        INCREMENT("eval beta cutoff material");
        return beta;
    }
    if (score <= alpha - 150)
    {
        INCREMENT("eval alpha cutoff material");
        return alpha;
    }
    /* Piece square tables */
    scores[WHITE] += EvaluatePieceSquare<WHITE>(position);
    scores[BLACK] += EvaluatePieceSquare<BLACK>(position); 
    /* Pawn structure */   
    PawnStructure ps[2];
    DeterminePawnStructure<WHITE>(position, ps[WHITE]);
    DeterminePawnStructure<BLACK>(position, ps[BLACK]);
    scores[WHITE] += EvaluatePawnStructure<WHITE>(ps[WHITE]);
    scores[BLACK] += EvaluatePawnStructure<BLACK>(ps[BLACK]);
    /* Kings */
    scores[WHITE] += EvaluateKing<WHITE>(position);
    scores[BLACK] += EvaluateKing<BLACK>(position);
    score = (position.flags_ & IS_BLACK_TO_MOVE) ?
        scores[BLACK] - scores[WHITE] :
        scores[WHITE] - scores[BLACK];
    eval_hash.hash = position.hash_;
    eval_hash.score = score;
    return score;
}