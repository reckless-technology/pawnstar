#include "evaluation.h"
#include "debug_hashtable.h"
#include "game.h"
#include "nnue.h"
#include "position.h"
#include "search_state.h"

/// @brief Evaluate the current position, assuming neither king is in check and the position is quiet.
/// NNUE is the only evaluator: after the draw short-circuit this returns the network's score for the
/// (incrementally maintained) accumulator, memoised by Zobrist hash in the net-specific eval cache.
/// @param state Per-thread search state providing the current position, accumulator and draw-detection.
/// @return Score from the perspective of the side to move.
int EvaluatePosition(const SearchState &state)
{
    INCREMENT("eval calls");
    if (state.CurrentPosition().IsDrawByMaterial() || state.IsDrawByFiftyMoves() || state.IsDrawByRepetition())
    {
        return kDrawScore;
    }
    const Position &position = state.CurrentPosition();
    // Memoise the (net-specific) NNUE eval by Zobrist hash: a cache hit skips the forward pass. The
    // accumulator is maintained incrementally by SearchState make/undo; the cache only saves the
    // dot-product. The cache is cleared whenever the net changes (NewGame / EvalFile load).
    const zobrist_t hash = position.Hash();
    int             cached;
    if (state.game.eval_cache.Probe(hash, cached))
    {
        INCREMENT("eval hash hits");
        return cached;
    }
    const int score = state.game.NnueNetwork().Evaluate(state.CurrentAccumulator(), position.ColorToMove());
    state.game.eval_cache.Store(hash, score);
    return score;
}
