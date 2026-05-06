#pragma once
/// @file Functions for managing history tables.

#include "constants.h"
#include "move.h"

#include <algorithm>
#include <cstdint>
#include <vector>

/// @brief Class for managing history of which moves raised alpha or caused a cutoff, at each ply of search.
/// Moves are indexed per ply, by from and to square.
class HistoryTable
{
  public:
    constexpr HistoryTable()
    {
        counts_.assign(kMaxPly * 4096, 0);
    }

    constexpr void Reset()
    {
        std::ranges::fill(counts_, 0);
    }

    constexpr uint32_t MaxCount(void) const
    {
        return std::ranges::max(counts_);
    }

    constexpr void RecordGoodMove(int ply, const Move &move)
    {
        ++counts_[ply * 4096 + move.from_to()];
    }

    constexpr uint32_t GetCount(int ply, const Move &move) const
    {
        return counts_[ply * 4096 + move.from_to()];
    }

  private:
    std::vector<uint32_t> counts_;
};