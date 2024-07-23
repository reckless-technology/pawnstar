#include "evaluation.h"
#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

constexpr int EVAL_HASH_SIZE = 100003; // Prime number

struct EvalHash
{
    uint64_t hash;
    int      score;
};

static EvalHash eval_hash_table[EVAL_HASH_SIZE];

/// @brief Evaluate the current position, assuming neither king is in check and the position is quiet.
/// @param position position to evaluate
/// @param alpha current alpha value
/// @param beta current beta value
/// @return score from the perspective of the side to move
int EvaluatePosition(const Position &position, int alpha, int beta)
{
    INCREMENT("eval calls");
    EvalHash &eval_hash = eval_hash_table[position.Hash() % EVAL_HASH_SIZE];
    if (eval_hash.hash == position.Hash())
    {
        INCREMENT("eval hash table hits");
        return eval_hash.score;
    }
    if (position.IsDrawByMaterial())
    {
        eval_hash.hash  = position.Hash();
        eval_hash.score = DRAW_SCORE;
        return DRAW_SCORE;
    }
    int scores[2];
    // Phase 1: Evaluate material values alone.
    scores[WHITE] = EvaluateMaterial<WHITE>(position);
    scores[BLACK] = EvaluateMaterial<BLACK>(position);
    // Piece square tables
    scores[WHITE] += EvaluatePieceSquare<WHITE>(position);
    scores[BLACK] += EvaluatePieceSquare<BLACK>(position);
    // Pawn structure
    const PawnStructure ps[2]{DeterminePawnStructure<WHITE>(position), DeterminePawnStructure<BLACK>(position)};
    scores[WHITE] += EvaluatePawnStructure<WHITE>(ps[WHITE]);
    scores[BLACK] += EvaluatePawnStructure<BLACK>(ps[BLACK]);
    // Kings
    scores[WHITE] += EvaluateKing<WHITE>(position);
    scores[BLACK] += EvaluateKing<BLACK>(position);
    const int score = position.ColorToMove() == WHITE ? scores[WHITE] - scores[BLACK] : scores[BLACK] - scores[WHITE];
    eval_hash.hash  = position.Hash();
    eval_hash.score = score;
    return score;
    (void)alpha;
    (void)beta;
}