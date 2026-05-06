#include "evaluation.h"
#include "debug_hashtable.h"
#include "game.h"
#include "position.h"
#include "transposition_table.h"

/// @brief Evaluate the current position, assuming neither king is in check and the position is quiet.
/// @param game game position to evaluate
/// @param alpha current alpha value
/// @param beta current beta value
/// @return score from the perspective of the side to move
int EvaluatePosition(const Game &game, int alpha, int beta)
{
    INCREMENT("eval calls");
    if (game.CurrentPosition().IsDrawByMaterial() || game.IsDrawByFiftyMoves() || game.IsDrawByRepetition())
    {
        return kDrawScore;
    }
    int             scores[2];
    const Position &position = game.CurrentPosition();
    // Phase 1: Evaluate material values alone.
    scores[kWhite] = EvaluateMaterial<kWhite>(position);
    scores[kBlack] = EvaluateMaterial<kBlack>(position);
    int score = position.ColorToMove() == kWhite ? scores[kWhite] - scores[kBlack] : scores[kBlack] - scores[kWhite];
    if (score > beta + 200 || score < alpha - 200)
    {
        INCREMENT("eval material cutoff");
        return score;
    }
    // Piece square tables
    scores[kWhite] += EvaluatePieceSquare<kWhite>(position);
    scores[kBlack] += EvaluatePieceSquare<kBlack>(position);
    // Pawn structure
    const std::array<PawnStructure, 2> ps{DeterminePawnStructure<kWhite>(position),
                                          DeterminePawnStructure<kBlack>(position)};
    scores[kWhite] += EvaluatePawnStructure<kWhite>(ps[kWhite]);
    scores[kBlack] += EvaluatePawnStructure<kBlack>(ps[kBlack]);
    // Mobility
    scores[kWhite] += EvaluateMobility<kWhite>(position, ps);
    scores[kBlack] += EvaluateMobility<kBlack>(position, ps);
    // Kings
    scores[kWhite] += EvaluateKing<kWhite>(position);
    scores[kBlack] += EvaluateKing<kBlack>(position);
    score = position.ColorToMove() == kWhite ? scores[kWhite] - scores[kBlack] : scores[kBlack] - scores[kWhite];
    // Round to nearest 5
    return (score / 5) * 5;
}