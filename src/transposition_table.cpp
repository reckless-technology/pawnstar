/// @file Functions to implement a transposition table.

#include <algorithm>
#include <memory>
#include <vector>

#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

/// @brief Create the transposition table.
/// @param megabytes Approx max size of the table in megabytes.
TranspositionTable::TranspositionTable(std::size_t megabytes)
{
    std::size_t num_entries = (megabytes * kMegabyte) / sizeof(Transposition);
    table_.assign(num_entries, Transposition{});
    locks_ = std::make_unique<std::atomic_flag[]>(num_entries);
}

/// @brief Find an entry in the TT if one exists for this position.
/// Acquires a per-entry spinlock, copies the entry while locked, then releases.
/// @param hash Zobrist hash of position.
/// @return The matching transposition if found.
std::optional<Transposition> TranspositionTable::FindTransposition(zobrist_t hash) const
{
    const std::size_t idx = hash % table_.size();
    // Acquire spinlock.
    while (locks_[idx].test_and_set(std::memory_order_acquire))
    {
    }
    const Transposition t = table_[idx]; // copy while locked
    locks_[idx].clear(std::memory_order_release);
    if (t.hash == hash)
        return t;
    return std::nullopt;
}

/// @brief Insert a new entry into the table if it is more valuable than the existing one.
/// @param transposition Transposition to be inserted.
void TranspositionTable::RecordTransposition(const Transposition &transposition)
{
    const std::size_t idx = transposition.hash % table_.size();
    while (locks_[idx].test_and_set(std::memory_order_acquire))
    {
    }
    Transposition &t = table_[idx];
    if (t.hash == 0 || t.is_old || t.depth <= transposition.depth ||
        transposition.node_type == Transposition::NodeType::kPv)
    {
        t = transposition;
    }
    locks_[idx].clear(std::memory_order_release);
}

/// @brief Show table occupancy.
/// @return The number of transpositions and percentage full of the table.
std::pair<std::size_t, int> TranspositionTable::UsageStats() const
{
    std::size_t count = std::ranges::count_if(table_, [](const Transposition &t) { return t.hash != 0; });
    return {count, (count * 100) / table_.size()};
}

/// @brief Mark all entries as old so they can be replaced by the upcoming search.
/// Called single-threaded before search starts; no locking needed.
void TranspositionTable::Age()
{
    std::ranges::for_each(table_, [](Transposition &t) { t.is_old = true; });
}
