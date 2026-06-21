#pragma once
/// @file history_table.h Functions for managing history tables.

#include "constants.h"
#include "move.h"

#include <cstdint>
#include <memory>

/// @brief History table tracking which moves raised alpha or caused a beta cutoff.
/// Moves are indexed per ply by from/to square. The table is per-thread (owned by SearchState) under
/// Lazy SMP — each search thread has its own — so the counts are plain (non-atomic): there is no
/// cross-thread access to synchronise.
class HistoryTable
{
  public:
    static constexpr int kTableSize = kMaxPly * 4096; ///< Total number of (ply, move) entries.

    /// @brief Construct the table, allocating and zeroing the count array.
    HistoryTable() : counts_{new uint32_t[kTableSize]{}}
    {
    }

    /// @brief Reset all counts to zero (call before each root search).
    void Reset()
    {
        for (int i = 0; i < kTableSize; ++i)
        {
            counts_[i] = 0;
        }
    }

    /// @brief Return the maximum count across all entries.
    /// @return The largest history count currently stored.
    uint32_t MaxCount() const
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
    void RecordGoodMove(int ply, const Move &move)
    {
        counts_[ply * 4096 + move.from_to()] += 1;
    }

    /// @brief Retrieve the history count for a move at a given ply.
    /// @param ply Current search ply.
    /// @param move Move to look up.
    /// @return The recorded history count for this (ply, move).
    uint32_t GetCount(int ply, const Move &move) const
    {
        return counts_[ply * 4096 + move.from_to()];
    }

  private:
    std::unique_ptr<uint32_t[]> counts_; ///< Heap-allocated count array.
};
