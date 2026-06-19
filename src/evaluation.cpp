#include "evaluation.h"
#include "debug_hashtable.h"
#include "game.h"
#include "nnue.h"
#include "position.h"
#include "search_state.h"

namespace
{
/// @brief Plies the fifty-move clock may reach before the static eval is discounted. Below this the eval
/// is untouched, so normal play (where the clock is reset often by pawn moves/captures) is unaffected.
constexpr int kRule50ScaleThreshold = 20;

/// @brief Scale the static eval toward draw as the fifty-move clock climbs, so the search has a standing
/// incentive to make progress (reset the clock) rather than shuffle in a won-but-not-yet-converted
/// position. NNUE inputs are piece placements only, so without this a winning eval is identical at clock
/// 0 and clock 99 — the search sees no gradient distinguishing progress from shuffling until the
/// fifty-move draw enters the horizon, and leaks won endgames to the rule (notably KBN-vs-K).
/// Identity for clock <= kRule50ScaleThreshold; then a linear ramp to 0 as the clock approaches 100 (the
/// hard draw, handled by the IsDrawByFiftyMoves short-circuit). @param rule50 reversible-move count (plies).
int ScaleByRule50(int score, int rule50)
{
    if (rule50 <= kRule50ScaleThreshold)
    {
        return score;
    }
    // num falls 80..1 over rule50 21..99; den is the ramp length. A progress move (clock reset to 0) keeps
    // the full eval, a shuffle (clock +1) keeps less — the difference is the gradient that drives progress.
    const int num = 100 - rule50;
    const int den = 100 - kRule50ScaleThreshold;
    return static_cast<int>(static_cast<int64_t>(score) * num / den);
}
} // namespace

/// @brief Evaluate the current position, assuming neither king is in check and the position is quiet.
/// NNUE is the only evaluator: after the draw short-circuit this returns the network's score for the
/// (incrementally maintained) accumulator, memoised by Zobrist hash in the net-specific eval cache, then
/// scaled toward draw by the fifty-move clock (see ScaleByRule50).
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
    //
    // The cached value is the RAW, clock-independent NNUE eval: the Zobrist hash excludes the fifty-move
    // clock, so the same position can be probed at different clock values. The fifty-move scaling is
    // therefore applied AFTER the cache, on the way out — never stored.
    const zobrist_t hash = position.Hash();
    int             raw;
    if (state.game.eval_cache.Probe(hash, raw))
    {
        INCREMENT("eval hash hits");
        return ScaleByRule50(raw, position.ReversibleMoveCount());
    }
    raw = state.game.NnueNetwork().Evaluate(state.CurrentAccumulator(), position.color_to_move);
    state.game.eval_cache.Store(hash, raw);
    return ScaleByRule50(raw, position.ReversibleMoveCount());
}
