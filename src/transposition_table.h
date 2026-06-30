#pragma once
#include <atomic>
#include <memory>
#include <optional>
#include <utility>

#include "constants.h"
#include "generated_data.h"
#include "move.h"

/// @brief A transposition table entry.
/// A transposition is the result of a previous search containing pertinent information about what was found when this
/// position was searched earlier. The score, depth, node type and age are packed into the move's spare bits so that an
/// entry is exactly 128 bits (a 64-bit hash and a 64-bit move).
struct Transposition
{
    using NodeType = ::NodeType; ///< Alpha-beta node type (kCut / kAll / kPv), defined in move.h.
    zobrist_t hash_;             ///< Zobrist hash of this position.
    Move      move_;             ///< Best move, with score/depth/node-type/age packed into its spare bits.
    constexpr Transposition();
    constexpr Transposition(zobrist_t hash, Move move);
    constexpr Transposition(zobrist_t hash, const Move &move, int score, int depth, NodeType type);
    constexpr int      score() const;
    constexpr int      depth() const;
    constexpr NodeType node_type() const;
    constexpr uint8_t  age() const;
};

constexpr Transposition::Transposition() : hash_(0), move_(Move::None())
{
}

/// @brief Construct from a move that already has its TT metadata packed in (used when reconstructing
/// an entry read back from the table).
constexpr Transposition::Transposition(zobrist_t hash, Move move) : hash_(hash), move_(move)
{
}

constexpr Transposition::Transposition(zobrist_t hash, const Move &move, int score, int depth, NodeType type)
    : hash_(hash), move_(move.WithTTData(score, depth, type))
{
}

/// @brief Score computed from this position (lower/upper bound or exact, per node_type()).
constexpr int Transposition::score() const
{
    return move_.score();
}

/// @brief Depth to which this position was searched.
constexpr int Transposition::depth() const
{
    return move_.TTDepth();
}

/// @brief Node type of the stored result.
constexpr Transposition::NodeType Transposition::node_type() const
{
    return move_.TTNodeType();
}

/// @brief Table generation when stored; stale when != the table's current generation.
constexpr uint8_t Transposition::age() const
{
    return move_.TTAge();
}

static_assert(sizeof(Transposition) == 16);

/// @brief Class to hold the transposition table.
/// The table is lockless: each cell holds key = hash ^ data and data (the packed move). A reader accepts
/// a cell only when key ^ data == probe hash, so a torn pair written by concurrent stores is detected and
/// read as a miss (Hyatt's XOR trick). Individual 64-bit words are std::atomic, accessed with relaxed
/// ordering; the XOR check supplies the cross-word consistency that relaxed ordering does not.
class TranspositionTable
{
  public:
    TranspositionTable(std::size_t megabytes);
    /// @brief Reallocate the table to approximately @p megabytes (clamped to >= 1 MB), clearing every entry
    /// and resetting the generation. Backs the UCI `Hash` option. Not safe to call during a search.
    void Resize(std::size_t megabytes);
    /// @brief Zero every entry and reset the generation, keeping the table size. Backs the UCI `Clear Hash`
    /// button. Not safe to call during a search.
    void                         Clear();
    std::optional<Transposition> FindTransposition(zobrist_t hash) const;
    void                         RecordTransposition(const Transposition &transposition);
    void                         Age();
    std::pair<std::size_t, int>  UsageStats() const;
    /// @brief Approximate fill level for the UCI `info hashfull` field, in per mille (0..1000). Samples the
    /// first min(size, 1000) cells rather than scanning the whole table, so it is cheap to call per info line.
    int  HashfullPermille() const;
    void Prefetch(zobrist_t hash) const;

  private:
    /// @brief One lockless table cell. Stores @c key = hash ^ data alongside @c data (the packed move
    /// bits). A reader accepts the entry only if @c key ^ @c data equals the probe hash, so a torn pair
    /// (two 64-bit words written by different stores) is detected and treated as a miss (Hyatt's XOR trick).
    struct AtomicEntry
    {
        AtomicEntry() : key_(0), data_(0)
        {
        }

        std::atomic<uint64_t> key_;  ///< hash ^ data.
        std::atomic<uint64_t> data_; ///< Packed move bits (move + score/depth/node-type/age).
    };

    static_assert(sizeof(AtomicEntry) == 16);

    std::unique_ptr<AtomicEntry[]> table_;      ///< Indexed using Zobrist hash; value-initialised to 0.
    std::size_t                    size_;       ///< Number of cells in table_.
    uint8_t                        generation_; ///< Current generation; bumped by Age().
};

/// @brief Prefetch the cell a future FindTransposition(@p hash) will read, hiding the cache-miss
/// latency of the random table access. Result-neutral. @param hash Zobrist hash to be probed soon.
inline void TranspositionTable::Prefetch(zobrist_t hash) const
{
    // Portable prefetch (clang/gcc on x86 and ARM alike): read access (rw = 0), low temporal locality
    // (locality = 1, the equivalent of x86 _MM_HINT_T2). Avoids the x86-only <immintrin.h> _mm_prefetch.
    __builtin_prefetch(reinterpret_cast<const char *>(&table_[hash % size_]), 0, 1);
}

/// @brief Create the transposition table.
/// @param megabytes Approx max size of the table in megabytes.
inline TranspositionTable::TranspositionTable(std::size_t megabytes) : size_(0), generation_(0)
{
    Resize(megabytes);
}

/// @brief Reallocate the table to approximately @p megabytes (clamped to >= 1 MB), clearing it and resetting
/// the generation. Backs the UCI `Hash` option; not safe to call during a search.
inline void TranspositionTable::Resize(std::size_t megabytes)
{
    if (megabytes < 1)
    {
        megabytes = 1;
    }
    size_       = (megabytes * kMegabyte) / sizeof(AtomicEntry);
    table_      = std::make_unique<AtomicEntry[]>(size_); // value-initialises every word to 0.
    generation_ = 0;
}

/// @brief Zero every entry and reset the generation, keeping the current table size (UCI `Clear Hash`).
/// Not safe to call during a search.
inline void TranspositionTable::Clear()
{
    for (std::size_t i = 0; i < size_; ++i)
    {
        table_[i].key_.store(0, std::memory_order_relaxed);
        table_[i].data_.store(0, std::memory_order_relaxed);
    }
    generation_ = 0;
}

/// @brief Find an entry in the TT if one exists for this position.
/// @param hash Zobrist hash of position.
/// @return The matching transposition if found.
inline std::optional<Transposition> TranspositionTable::FindTransposition(zobrist_t hash) const
{
    const AtomicEntry &e    = table_[hash % size_];
    const uint64_t     data = e.data_.load(std::memory_order_relaxed);
    const uint64_t     key  = e.key_.load(std::memory_order_relaxed);
    if ((key ^ data) == hash)
    {
        return Transposition{hash, Move::FromBits(data)};
    }
    return std::nullopt;
}

/// @brief Insert a new entry into the table if it is more valuable than the existing one.
/// Lockless read-decide-write: a concurrent writer to the same cell results in benign last-writer-wins,
/// while the XOR-encoded key keeps each stored pair self-consistent.
/// @param transposition Transposition to be inserted.
inline void TranspositionTable::RecordTransposition(const Transposition &transposition)
{
    AtomicEntry   &e        = table_[transposition.hash_ % size_];
    const uint64_t cur_key  = e.key_.load(std::memory_order_relaxed);
    const uint64_t cur_data = e.data_.load(std::memory_order_relaxed);
    const Move     cur      = Move::FromBits(cur_data);
    const bool     is_empty = (cur_key == 0 && cur_data == 0);
    const bool     is_old   = cur.TTAge() != generation_;
    if (is_empty || is_old || cur.TTDepth() <= transposition.depth() ||
        transposition.node_type() == Transposition::NodeType::kPv)
    {
        Move m = transposition.move_;
        m.SetTTAge(generation_);
        const uint64_t data = m.Bits();
        e.key_.store(transposition.hash_ ^ data, std::memory_order_relaxed);
        e.data_.store(data, std::memory_order_relaxed);
    }
}

/// @brief Return the number of occupied entries and percentage full of the table.
inline std::pair<std::size_t, int> TranspositionTable::UsageStats() const
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < size_; ++i)
    {
        if (table_[i].data_.load(std::memory_order_relaxed) != 0)
        {
            ++count;
        }
    }
    return {count, (int)((count * 100) / size_)};
}

/// @brief Approximate fill level for UCI `info hashfull`, in per mille. Samples the first min(size, 1000)
/// cells (positions hash uniformly, so the prefix is a representative sample) rather than scanning the whole
/// table, so it is cheap enough to call on every info line.
inline int TranspositionTable::HashfullPermille() const
{
    const std::size_t sample = size_ < 1000 ? size_ : 1000;
    if (sample == 0)
    {
        return 0;
    }
    std::size_t count = 0;
    for (std::size_t i = 0; i < sample; ++i)
    {
        if (table_[i].data_.load(std::memory_order_relaxed) != 0)
        {
            ++count;
        }
    }
    return static_cast<int>((count * 1000) / sample);
}

/// @brief Advance the table generation so that all existing entries become stale (replaceable).
/// O(1): entries are not touched; an entry is "old" when its age differs from the current
/// generation. The uint8_t generation wraps every 256 searches, which is harmless. Called
/// single-threaded before search starts; no locking needed.
inline void TranspositionTable::Age()
{
    ++generation_;
}
