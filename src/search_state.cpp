/// @file Per-thread search state implementation.

#include "search_state.h"
#include "game.h"

/// @brief Construct the main-thread search state from the current game.
/// hash_stack_ is populated with all ancestor positions (positions 0..N-1 where N is
/// the current position), so that IsDrawByRepetition can walk backwards through the full
/// game history.
SearchState::SearchState(Game &g) : game(g), batch_cutoff(nullptr)
{
    const StackList<Position, 256> &pos = g.Positions();
    // Reserve enough for the full game history plus the maximum search depth.
    hash_stack_.reserve(pos.size() + kMaxPly + 4);
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
SearchState::SearchState(const SearchState &parent, std::atomic<bool> *cutoff)
    : game(parent.game), batch_cutoff(cutoff)
{
    hash_stack_ = parent.hash_stack_;
    hash_stack_.reserve(hash_stack_.size() + kMaxPly + 4);
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

/// @brief Score moves using MVV/LVA combined with history table counts, then sort descending.
void SearchState::ScoreAndSortMoves(MoveList &moves, int ply) const
{
    const Position &position = CurrentPosition();
    for (Move &move : moves)
    {
        move.AssignScore(kPieceValues[position.PieceAt(move.to())] * 10000 -
                         kPieceValues[position.PieceAt(move.from())] * 1000 +
                         game.history_table.GetCount(ply, move));
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
