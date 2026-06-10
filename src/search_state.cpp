/// @file search_state.cpp Per-thread search state implementation.

#include "search_state.h"
#include "game.h"
#include "static_exchange_evaluation.h"

#include <algorithm>

/// @brief Construct the main-thread search state from the current game.
/// hash_stack_ is populated with all ancestor positions (positions 0..N-1 where N is
/// the current position), so that IsDrawByRepetition can walk backwards through the full
/// game history.
SearchState::SearchState(Game &g) : game(g), batch_cutoff(nullptr)
{
    const StackList<Position, kMaxGameLength> &pos = g.Positions();
    // Push all positions except the last (current) one into the hash history.
    for (std::size_t i = 0; i + 1 < pos.size(); ++i)
    {
        hash_stack_.push_back({pos[i].Hash(), pos[i].ReversibleMoveCount()});
    }
    // Seed the per-thread position stack with only the current game position.
    positions_.push_back(g.CurrentPosition());
}

/// @brief Construct a worker search state forked from a parent state.
/// The worker gets a full copy of the parent's hash history (for repetition detection)
/// and starts its own position stack from the parent's current position.
SearchState::SearchState(const SearchState &parent, std::atomic<bool> *cutoff) : game(parent.game), batch_cutoff(cutoff)
{
    hash_stack_ = parent.hash_stack_;
    positions_.push_back(parent.CurrentPosition());
}

/// @brief Push the current position's hash then make the move on a copy of the position.
void SearchState::PlayMove(Move move)
{
    const Position &cur = CurrentPosition();
    hash_stack_.push_back({cur.Hash(), cur.ReversibleMoveCount()});
    positions_.push_back(cur.MakeMove(move));
}

/// @brief Pop the position and hash stacks to undo the last move.
void SearchState::UndoMove()
{
    positions_.pop_back();
    hash_stack_.pop_back();
}

/// @brief Push a null move (skip this side's turn) used for null-move pruning.
void SearchState::MakeNullMove()
{
    const Position &cur = CurrentPosition();
    hash_stack_.push_back({cur.Hash(), cur.ReversibleMoveCount()});
    positions_.push_back(cur.MakeNullMove());
}

/// @brief Score moves for ordering, then sort descending.
/// The score field is a signed 23-bit value shared with the stored TT score. Move ordering uses, from
/// best to worst:
///   - winning / equal captures and promotions: kWinningCaptureBase + SEE score,
///   - the two killer moves for this ply: just below winning captures, above all quiet moves,
///   - quiet moves: their history count, clamped below the killers,
///   - losing captures (negative SEE): scored by their (negative) SEE, so they sort below quiet moves.
void SearchState::ScoreAndSortMoves(MoveList &moves, int ply) const
{
    static constexpr int kWinningCaptureBase = 1 << 20;         // 1,048,576: winning captures / promotions
    static constexpr int kKillerBase         = 1 << 19;         // 524,288: killers sit just below winning captures
    static constexpr int kMaxQuiet           = kKillerBase - 1; // history scores clamp just below the killers
    const Position      &position            = CurrentPosition();
    const Move           killer0             = killers[ply][0];
    const Move           killer1             = killers[ply][1];
    for (Move &move : moves)
    {
        int sort;
        if (!move.IsQuiet()) // capture or promotion: order by static exchange evaluation
        {
            const int see = EvaluateStaticExchange(position, move).first;
            sort          = see >= 0 ? kWinningCaptureBase + see : see; // losing captures sort below quiet moves
        }
        else if (move == killer0) // first killer: just below winning captures
        {
            sort = kKillerBase + 1;
        }
        else if (move == killer1) // second killer
        {
            sort = kKillerBase;
        }
        else // quiet move: history count, clamped below the killers
        {
            sort = (int)std::min<uint32_t>(game.history_table.GetCount(ply, move), (uint32_t)kMaxQuiet);
        }
        move.AssignScore(sort);
    }
    SortMoves<false>(moves);
}

/// @brief Check for draw by the 50-move rule.
bool SearchState::IsDrawByFiftyMoves() const
{
    return CurrentPosition().ReversibleMoveCount() >= 100;
}

/// @brief Check for draw by three-fold repetition using the compact hash history.
/// Walks backwards through hash_stack_ two entries at a time (same side to move),
/// starting four half-moves before the current position, matching the logic in Game::IsDrawByRepetition.
bool SearchState::IsDrawByRepetition() const
{
    const zobrist_t hash        = CurrentPosition().Hash();
    int             repetitions = 2;
    for (int i = (int)hash_stack_.size() - 4; i >= 0; i -= 2)
    {
        if (hash_stack_[i].hash == hash && --repetitions == 0)
        {
            return true;
        }
        if (hash_stack_[i].reversible_move_count == 0)
        {
            break;
        }
    }
    return false;
}

/// @brief Check whether this thread should abort.
bool SearchState::IsCancelled() const
{
    return game.is_cancel_pending.load(std::memory_order_relaxed) ||
           (batch_cutoff != nullptr && batch_cutoff->load(std::memory_order_relaxed));
}
