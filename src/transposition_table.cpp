/// @file Functions to implement a transposition table.

#include <algorithm>
#include <vector>

#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

constexpr static bool IsPrime(std::size_t x);

/// @brief Create the transposition table.
/// @param megabytes Approx max size of the table in megabytes. May be slightly smaller (table is prime size).
TranspositionTable::TranspositionTable(std::size_t megabytes)
{
    std::size_t num_entries = (megabytes * MEGABYTE) / sizeof(Transposition);
    // Find the next smallest prime number and make the table that size.
    if ((num_entries & 1) == 0)
    {
        --num_entries;
    }
    while (!IsPrime(num_entries))
    {
        num_entries -= 2;
    }
    table_.assign(num_entries, Transposition{0, Move::None(), 0, 0, Transposition::NodeType::CUT});
}

/// @brief Find an entry in the TT if one exists for this position.
/// @param hash Zobrist hash of position
/// @return If a transposition was found in the table, then return it
std::optional<Transposition> TranspositionTable::FindTransposition(uint64_t hash) const
{
    const Transposition &t = table_[hash % table_.size()];
    if (t.hash == hash)
    {
        return t;
    }
    return std::nullopt;
}

/// @brief Insert a new entry into the table.
/// @param hash Zobrist hash of position.
/// @param depth Search depth.
/// @param score Score for this position.
/// @param move Best move found.
/// @param node_type Node type.
void TranspositionTable::RecordTransposition(const Transposition &transposition)
{
    Transposition &t = table_[transposition.hash % table_.size()];
    // If the new entry is more valuable than any previous entry then replace it.
    if (t.hash == 0 || t.is_old || t.depth <= transposition.depth ||
        transposition.node_type == Transposition::NodeType::PV)
    {
        t = transposition;
    }
}

/// @brief Show table occupancy.
/// @return The number of transpositions and percentage full of the table.
std::pair<std::size_t, int> TranspositionTable::UsageStats() const
{
    const std::size_t count = std::ranges::count_if(table_, [](const Transposition &t) { return t.hash != 0; });
    return {count, (count * 100) / table_.size()};
}

/// @brief Age all the transposition entries in the table. An entry is old if it does not pertain to the current search
/// being performed.
void TranspositionTable::Age()
{
    std::ranges::for_each(table_, [](Transposition &t) { t.is_old = true; });
}

/// @brief determine if a number is a prime number. We get marginally better hash dispersion when the hashtable size is
/// a prime number.
/// @param x candidate
/// @return true if x is prime
constexpr static bool IsPrime(std::size_t x)
{
    if (x < 3)
    {
        return false;
    }
    for (std::size_t i = 2; i * i <= x; ++i)
    {
        if (x % i == 0)
        {
            return false;
        }
    }
    return true;
}