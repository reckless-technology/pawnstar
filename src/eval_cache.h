#pragma once
/// @file eval_cache.h Small lockless cache memoising NNUE evaluations by Zobrist hash.

#include "generated_data.h" // zobrist_t

#include <atomic>
#include <cstdint>
#include <vector>

/// @brief Lockless cache of NNUE evaluation results, keyed by Zobrist hash.
///
/// An NNUE evaluation is a pure function of the position, so memoising it across the search tree (and
/// across transpositions) lets a cache hit skip the per-node forward pass. Each entry is a single 64-bit
/// atomic packing a 48-bit key check (the top 48 bits of the hash) and a 16-bit score, so loads/stores
/// are atomic with no torn values and no locks — shared freely by all Lazy SMP threads. A foreign or
/// evicted entry simply fails the key check and reads as a miss; writers race benignly (last writer
/// wins). Because the stored score depends on the active network, the cache MUST be cleared whenever the
/// loaded net changes (a different net produces a different eval for the same hash).
///
/// Static evaluations are clamped to +/-kMaxEvaluation (30000) in Dequant, well within int16 range (and
/// only static evals — never mate scores — are cached), so 16 bits suffice for the score and
/// the other 48 bits go to the key. A zeroed slot has key 0, so a position whose top 48 hash bits are
/// exactly 0 (probability ~2^-48) reads a spurious score of 0; this is a heuristic cache, so that
/// vanishingly rare event only perturbs one leaf evaluation and never affects correctness.
class EvalCache
{
  public:
    explicit EvalCache(int megabytes);
    bool Probe(zobrist_t hash, int &score) const;
    void Store(zobrist_t hash, int score);
    void Clear();

  private:
    std::vector<std::atomic<uint64_t>> table_; ///< Power-of-two table; each entry packs key|score.
    std::size_t                        mask_;  ///< Index mask (table size - 1).
};

/// @brief Construct a cache of about @p megabytes MB (entry count rounded down to a power of two).
/// @param megabytes Target size in megabytes.
inline EvalCache::EvalCache(int megabytes) : mask_(0)
{
    const std::size_t want = (static_cast<std::size_t>(megabytes) * 1024 * 1024) / sizeof(std::atomic<uint64_t>);
    std::size_t       pow2 = 1;
    while (pow2 * 2 <= want)
    {
        pow2 *= 2;
    }
    mask_  = pow2 - 1;
    table_ = std::vector<std::atomic<uint64_t>>(pow2); // value-initialised (all zero)
}

/// @brief Probe the cache for a position. @param hash Position Zobrist hash.
/// @param[out] score Cached evaluation (centipawns, side-to-move relative) on a hit. @return true on hit.
inline bool EvalCache::Probe(zobrist_t hash, int &score) const
{
    const uint64_t entry = table_[hash & mask_].load(std::memory_order_relaxed);
    if ((entry >> 16) != (hash >> 16))
    {
        return false;
    }
    score = static_cast<int16_t>(static_cast<uint16_t>(entry));
    return true;
}

/// @brief Store an evaluation for a position. @param hash Position Zobrist hash. @param score Eval in cp.
inline void EvalCache::Store(zobrist_t hash, int score)
{
    const uint64_t entry =
        (hash & 0xFFFFFFFFFFFF0000ULL) | static_cast<uint64_t>(static_cast<uint16_t>(static_cast<int16_t>(score)));
    table_[hash & mask_].store(entry, std::memory_order_relaxed);
}

/// @brief Reset every entry. Call when the active network changes (and at the start of a game).
inline void EvalCache::Clear()
{
    for (auto &entry : table_)
    {
        entry.store(0, std::memory_order_relaxed);
    }
}
