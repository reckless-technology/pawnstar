#pragma once
/// @file history_table.h Functions for managing history tables.

#include "constants.h"
#include "move.h"

#include <cstdint>
#include <memory>

/// @brief Bound on a gravity-managed history entry: an entry self-saturates toward +/-kHistoryMax.
/// 16384 fits an int16_t (the continuation-history storage) with margin.
inline constexpr int32_t kHistoryMax = 16384;
/// @brief Cap on the per-update history bonus, so a single very deep cutoff cannot dominate the table.
/// depth*depth only reaches it near depth 20.
inline constexpr int32_t kMaxHistoryBonus = 400;

/// @brief Depth-weighted magnitude credited to a good move (and debited from the quiets that failed
/// before it). Quadratic in depth, clamped to kMaxHistoryBonus. Depths below 1 are treated as 1.
/// @param depth Adjusted node depth. @return The bonus magnitude in [1, kMaxHistoryBonus].
inline int32_t HistoryBonus(int depth)
{
    if (depth < 1)
    {
        depth = 1;
    }
    int32_t bonus = static_cast<int32_t>(depth * depth);
    if (bonus > kMaxHistoryBonus)
    {
        bonus = kMaxHistoryBonus;
    }
    return bonus;
}

/// @brief Nudge @p entry by @p bonus with a gravity term that keeps it within +/-kHistoryMax, so recent
/// evidence overwrites stale evidence (the entry self-saturates toward the sign of the running bonus).
/// Integer arithmetic (truncating division), matching the Go engine bit-for-bit.
/// @param entry History entry to update in place. @param bonus Signed magnitude (negative debits).
inline void ApplyGravity(int32_t &entry, int32_t bonus)
{
    const int32_t abs_bonus = bonus < 0 ? -bonus : bonus;
    entry += bonus - entry * abs_bonus / kHistoryMax;
}

/// @brief Butterfly history table tracking which moves raised alpha or caused a beta cutoff.
/// Moves are indexed by side to move and from/to square, pooling statistics across all plies (which
/// concentrates the signal where positions repeat across branches and transpositions; the explicit
/// colour dimension replaces the accidental colour proxy that per-ply indexing provided). The table
/// is per-thread (owned by SearchState) under Lazy SMP — each search thread has its own — so the
/// counts are plain (non-atomic): there is no cross-thread access to synchronise.
///
/// Storage is signed (int32_t) because the main search updates entries with a depth-weighted
/// bonus/malus and a gravity term (ApplyBonus): a good move is credited, the quiet moves that failed
/// before a cutoff are debited (driving entries negative), and gravity self-saturates the entry toward
/// +/-kHistoryMax. Quiescence still uses the plain increment (RecordGoodMove); both coexist on the same
/// table, matching the Go engine.
class HistoryTable
{
  public:
    static constexpr int kTableSize = 2 * 4096; ///< Total number of (side, from/to) entries.
    HistoryTable();
    void    Reset();
    int32_t MaxCount() const;
    void    RecordGoodMove(Color side, const Move &move);
    void    ApplyBonus(Color side, const Move &move, int32_t bonus);
    int32_t GetCount(Color side, const Move &move) const;

  private:
    std::unique_ptr<int32_t[]> counts_; ///< Heap-allocated (signed) count array.
};

/// @brief Construct the table, allocating and zeroing the count array.
inline HistoryTable::HistoryTable() : counts_{new int32_t[kTableSize]{}}
{
}

/// @brief Reset all counts to zero (call before each root search).
inline void HistoryTable::Reset()
{
    for (int i = 0; i < kTableSize; ++i)
    {
        counts_[i] = 0;
    }
}

/// @brief Return the maximum count across all entries.
/// @return The largest history count currently stored.
inline int32_t HistoryTable::MaxCount() const
{
    int32_t max_val = 0;
    for (int i = 0; i < kTableSize; ++i)
    {
        if (counts_[i] > max_val)
        {
            max_val = counts_[i];
        }
    }
    return max_val;
}

/// @brief Increment the count for a move that raised alpha or caused a cutoff.
/// Used by quiescence, which carries no meaningful depth and stays on the simple increment; the main
/// search uses ApplyBonus with a depth-weighted gravity update instead.
/// @param side Side to move. @param move Move to record.
inline void HistoryTable::RecordGoodMove(Color side, const Move &move)
{
    counts_[static_cast<int>(side) * 4096 + move.from_to()] += 1;
}

/// @brief Credit (bonus > 0) or debit (bonus < 0) the butterfly-history entry for @p move by @p side,
/// with the gravity update (self-saturating toward +/-kHistoryMax).
/// @param side Side to move. @param move Move to update. @param bonus Signed bonus magnitude.
inline void HistoryTable::ApplyBonus(Color side, const Move &move, int32_t bonus)
{
    ApplyGravity(counts_[static_cast<int>(side) * 4096 + move.from_to()], bonus);
}

/// @brief Retrieve the history count for a move by the given side.
/// @param side Side to move.
/// @param move Move to look up.
/// @return The recorded (signed) history count for this (side, move).
inline int32_t HistoryTable::GetCount(Color side, const Move &move) const
{
    return counts_[static_cast<int>(side) * 4096 + move.from_to()];
}
