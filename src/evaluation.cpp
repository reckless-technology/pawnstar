#include "evaluation.h"
#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

/// @brief Evaluate the current position, assuming neither king is in check and the position is quiet.
/// @param position position to evaluate
/// @param alpha current alpha value
/// @param beta current beta value
/// @return score from the perspective of the side to move
int EvaluatePosition(const Position &position, int alpha, int beta)
{
    INCREMENT("eval calls");
    int scores[2];
    // Phase 1: Evaluate material values alone.
    scores[WHITE] = EvaluateMaterial<WHITE>(position);
    scores[BLACK] = EvaluateMaterial<BLACK>(position);
    // Piece square tables
    scores[WHITE] += EvaluatePieceSquare<WHITE>(position);
    scores[BLACK] += EvaluatePieceSquare<BLACK>(position);
    // Pawn structure
    const std::array<PawnStructure, 2> ps{DeterminePawnStructure<WHITE>(position),
                                          DeterminePawnStructure<BLACK>(position)};
    scores[WHITE] += EvaluatePawnStructure<WHITE>(ps[WHITE]);
    scores[BLACK] += EvaluatePawnStructure<BLACK>(ps[BLACK]);
    // Mobility
    scores[WHITE] += EvaluateMobility<WHITE>(position, ps);
    scores[BLACK] += EvaluateMobility<BLACK>(position, ps);
    // Kings
    scores[WHITE] += EvaluateKing<WHITE>(position);
    scores[BLACK] += EvaluateKing<BLACK>(position);
    const int score = position.ColorToMove() == WHITE ? scores[WHITE] - scores[BLACK] : scores[BLACK] - scores[WHITE];
    // Round to nearest 5
    return (score / 5) * 5;
    (void)alpha;
    (void)beta;
}