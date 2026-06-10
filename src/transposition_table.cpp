/// @file Functions to implement a transposition table.
/// The table is lockless: each cell holds key = hash ^ data and data (the packed move). A reader accepts
/// a cell only when key ^ data == probe hash, so a torn pair written by concurrent stores is detected and
/// read as a miss (Hyatt's XOR trick). Individual 64-bit words are std::atomic, accessed with relaxed
/// ordering; the XOR check supplies the cross-word consistency that relaxed ordering does not.

#include "debug_hashtable.h"
#include "position.h"
#include "transposition_table.h"

/// @brief Create the transposition table.
/// @param megabytes Approx max size of the table in megabytes.
TranspositionTable::TranspositionTable(std::size_t megabytes)
{
    size_  = (megabytes * kMegabyte) / sizeof(AtomicEntry);
    table_ = std::make_unique<AtomicEntry[]>(size_); // value-initialises every word to 0.
}

/// @brief Find an entry in the TT if one exists for this position.
/// @param hash Zobrist hash of position.
/// @return The matching transposition if found.
std::optional<Transposition> TranspositionTable::FindTransposition(zobrist_t hash) const
{
    const AtomicEntry &e    = table_[hash % size_];
    const uint64_t     data = e.data.load(std::memory_order_relaxed);
    const uint64_t     key  = e.key.load(std::memory_order_relaxed);
    if ((key ^ data) == hash)
        return Transposition{hash, Move::FromBits(data)};
    return std::nullopt;
}

/// @brief Insert a new entry into the table if it is more valuable than the existing one.
/// Lockless read-decide-write: a concurrent writer to the same cell results in benign last-writer-wins,
/// while the XOR-encoded key keeps each stored pair self-consistent.
/// @param transposition Transposition to be inserted.
void TranspositionTable::RecordTransposition(const Transposition &transposition)
{
    AtomicEntry   &e        = table_[transposition.hash % size_];
    const uint64_t cur_key  = e.key.load(std::memory_order_relaxed);
    const uint64_t cur_data = e.data.load(std::memory_order_relaxed);
    const Move     cur      = Move::FromBits(cur_data);
    const bool     is_empty = (cur_key == 0 && cur_data == 0);
    const bool     is_old   = cur.TTAge() != generation_;
    if (is_empty || is_old || cur.TTDepth() <= transposition.depth() ||
        transposition.node_type() == Transposition::NodeType::kPv)
    {
        Move m = transposition.move;
        m.SetTTAge(generation_);
        const uint64_t data = m.Bits();
        e.key.store(transposition.hash ^ data, std::memory_order_relaxed);
        e.data.store(data, std::memory_order_relaxed);
    }
}

/// @brief Return the number of occupied entries and percentage full of the table.
std::pair<std::size_t, int> TranspositionTable::UsageStats() const
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < size_; ++i)
        if (table_[i].data.load(std::memory_order_relaxed) != 0)
            ++count;
    return {count, (int)((count * 100) / size_)};
}

/// @brief Advance the table generation so that all existing entries become stale (replaceable).
/// O(1): entries are not touched; an entry is "old" when its age differs from the current
/// generation. The uint8_t generation wraps every 256 searches, which is harmless. Called
/// single-threaded before search starts; no locking needed.
void TranspositionTable::Age()
{
    ++generation_;
}
