#pragma once
/// @file history_table.h Functions for managing history tables.

#include "constants.h"
#include "move.h"

#include <atomic>
#include <cstdint>
#include <memory>

/// @brief Thread-safe history table tracking which moves raised alpha or caused a beta cutoff.
/// Moves are indexed per ply by from/to square. Counts are stored as atomics so that parallel
/// workers can increment them concurrently without data races.
class HistoryTable
{
  public:
    static constexpr int kTableSize = kMaxPly * 4096; ///< Total number of (ply, move) entries.

    /// @brief Construct the table, allocating and zeroing the count array.
    HistoryTable() : counts_{new std::atomic<uint32_t>[kTableSize] {}}
    {
    }

    /// @brief Reset all counts to zero (call before each root search, single-threaded).
    void Reset()
    {
        for (int i = 0; i < kTableSize; ++i)
            counts_[i].store(0, std::memory_order_relaxed);
    }

    /// @brief Return the maximum count across all entries.
    /// @return The largest history count currently stored.
    uint32_t MaxCount() const
    {
        uint32_t max_val = 0;
        for (int i = 0; i < kTableSize; ++i)
        {
            uint32_t v = counts_[i].load(std::memory_order_relaxed);
            if (v > max_val)
                max_val = v;
        }
        return max_val;
    }

    /// @brief Increment the count for a move that raised alpha or caused a cutoff.
    /// @param ply Current search ply.
    /// @param move Move to record.
    void RecordGoodMove(int ply, const Move &move)
    {
        counts_[ply * 4096 + move.from_to()].fetch_add(1, std::memory_order_relaxed);
    }

    /// @brief Retrieve the history count for a move at a given ply.
    /// @param ply Current search ply.
    /// @param move Move to look up.
    /// @return The recorded history count for this (ply, move).
    uint32_t GetCount(int ply, const Move &move) const
    {
        return counts_[ply * 4096 + move.from_to()].load(std::memory_order_relaxed);
    }

  private:
    std::unique_ptr<std::atomic<uint32_t>[]> counts_; ///< Heap-allocated atomic count array.
};
