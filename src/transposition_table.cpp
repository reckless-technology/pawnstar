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

namespace
{
struct TTLock
{
    std::atomic_flag &flag;
    explicit TTLock(std::atomic_flag &f) : flag(f)
    {
        while (flag.test_and_set(std::memory_order_acquire))
        {
        }
    }
    ~TTLock() { flag.clear(std::memory_order_release); }
};
} // namespace

/// @brief Find an entry in the TT if one exists for this position.
/// Acquires a per-entry spinlock, copies the entry while locked, then releases.
/// @param hash Zobrist hash of position.
/// @return The matching transposition if found.
std::optional<Transposition> TranspositionTable::FindTransposition(zobrist_t hash) const
{
    const std::size_t    idx = hash % table_.size();
    TTLock               lock{locks_[idx]};
    const Transposition  t = table_[idx];
    if (t.hash == hash)
        return t;
    return std::nullopt;
}

/// @brief Insert a new entry into the table if it is more valuable than the existing one.
/// @param transposition Transposition to be inserted.
void TranspositionTable::RecordTransposition(const Transposition &transposition)
{
    const std::size_t idx = transposition.hash % table_.size();
    TTLock            lock{locks_[idx]};
    Transposition    &t      = table_[idx];
    const bool        is_old = t.age() != generation_;
    if (t.hash == 0 || is_old || t.depth() <= transposition.depth() ||
        transposition.node_type() == Transposition::NodeType::kPv)
    {
        t = transposition;
        t.move.SetTTAge(generation_);
    }
}

/// @brief Return the number of occupied entries and percentage full of the table.
std::pair<std::size_t, int> TranspositionTable::UsageStats() const
{
    std::size_t count = std::ranges::count_if(table_, [](const Transposition &t) { return t.hash != 0; });
    return {count, (count * 100) / table_.size()};
}

/// @brief Advance the table generation so that all existing entries become stale (replaceable).
/// O(1): entries are not touched; an entry is "old" when its age differs from the current
/// generation. The uint8_t generation wraps every 256 searches, which is harmless. Called
/// single-threaded before search starts; no locking needed.
void TranspositionTable::Age()
{
    ++generation_;
}
