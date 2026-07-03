#pragma once
/// @file history_table.h Functions for managing history tables.

#include "constants.h"
#include "move.h"

#include <cstdint>
#include <memory>

/// @brief Butterfly history table tracking which moves raised alpha or caused a beta cutoff.
/// Moves are indexed by side to move and from/to square, pooling statistics across all plies (which
/// concentrates the signal where positions repeat across branches and transpositions; the explicit
/// colour dimension replaces the accidental colour proxy that per-ply indexing provided). The table
/// is per-thread (owned by SearchState) under Lazy SMP — each search thread has its own — so the
/// counts are plain (non-atomic): there is no cross-thread access to synchronise.
class HistoryTable
{
  public:
    static constexpr int kTableSize = 2 * 4096; ///< Total number of (side, from/to) entries.
    HistoryTable();
    void     Reset();
    uint32_t MaxCount() const;
    void     RecordGoodMove(Color side, const Move &move);
    uint32_t GetCount(Color side, const Move &move) const;

  private:
    std::unique_ptr<uint32_t[]> counts_; ///< Heap-allocated count array.
};

/// @brief Construct the table, allocating and zeroing the count array.
inline HistoryTable::HistoryTable() : counts_{new uint32_t[kTableSize]{}}
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
inline uint32_t HistoryTable::MaxCount() const
{
    uint32_t max_val = 0;
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
/// @param ply Current search ply.
/// @param move Move to record.
inline void HistoryTable::RecordGoodMove(Color side, const Move &move)
{
    counts_[static_cast<int>(side) * 4096 + move.from_to()] += 1;
}

/// @brief Retrieve the history count for a move by the given side.
/// @param side Side to move.
/// @param move Move to look up.
/// @return The recorded history count for this (side, move).
inline uint32_t HistoryTable::GetCount(Color side, const Move &move) const
{
    return counts_[static_cast<int>(side) * 4096 + move.from_to()];
}
