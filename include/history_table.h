#pragma once
/// @file Functions for managing history tables.

#include "constants.h"
#include "move.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

/// @brief Class for managing history of which moves raised alpha or caused a cutoff, at each ply of search.
/// Moves are indexed per ply, by from and to square.
class HistoryTable
{
  private:
    typedef std::array<uint32_t, MAX_PLY * 4096> Table;
    std::unique_ptr<Table>                       data_;

  public:
    HistoryTable()
    {
        data_ = std::make_unique<Table>();
    }
    void Reset()
    {
        auto &table = *data_;
        std::ranges::fill(table, 0);
    }
    uint32_t MaxCount(void) const
    {
        const auto &table = *data_;
        return *std::ranges::max_element(table);
    }
    void RecordGoodMove(int ply, const Move &move)
    {
        auto &table = *data_;
        ++table[ply * 4096 + move.from_to()];
    }
    uint32_t GetCount(int ply, const Move &move) const
    {
        const auto &table = *data_;
        return table[ply * 4096 + move.from_to()];
    }
};