#include "evaluation.h"
#include "debug_hashtable.h"
#include "game.h"
#include "nnue.h"
#include "position.h"
#include "search_state.h"
#include "transposition_table.h"

/// @brief Evaluate the current position, assuming neither king is in check and the position is quiet.
/// @param state Per-thread search state providing the current position and draw-detection.
/// @param alpha Current alpha value.
/// @param beta Current beta value.
/// @return Score from the perspective of the side to move.
int EvaluatePosition(const SearchState &state, int alpha, int beta)
{
    INCREMENT("eval calls");
    if (state.CurrentPosition().IsDrawByMaterial() || state.IsDrawByFiftyMoves() || state.IsDrawByRepetition())
    {
        return kDrawScore;
    }
    const Position &position = state.CurrentPosition();
    // NNUE evaluation (experimental): when selected and a net is loaded, use it instead of the handcrafted
    // evaluation. The NNUE path bypasses the material lazy cutoff and the round-to-nearest-5 below, both of
    // which are specific to the handcrafted scores.
    if (state.game.NnueActive())
    {
        // Use the incrementally-maintained accumulator (kept in sync by SearchState make/undo) rather than
        // rebuilding it from scratch here.
        return state.game.NnueNetwork().Evaluate(state.CurrentAccumulator(), position.ColorToMove());
    }
    std::array<int, 2> scores;
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
