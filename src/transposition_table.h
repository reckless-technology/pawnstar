#pragma once
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "move.h"

/// @brief A transposition table entry.
/// A transposition is the result of a previous search containing pertinent information about what was found when this
/// position was searched earlier. The score, depth, node type and age are packed into the move's spare bits so that an
/// entry is exactly 128 bits (a 64-bit hash and a 64-bit move).
struct Transposition
{
    using NodeType = ::NodeType; ///< Alpha-beta node type (kCut / kAll / kPv), defined in move.h.

    zobrist_t hash; ///< Zobrist hash of this position.
    Move      move; ///< Best move, with score/depth/node-type/age packed into its spare bits.

    constexpr Transposition() : hash(0), move(Move::None())
    {
    }

    constexpr Transposition(zobrist_t hash, const Move &move, int score, int depth, NodeType type)
        : hash(hash), move(move.WithTTData(score, depth, type))
    {
    }

    /// @brief Score computed from this position (lower/upper bound or exact, per node_type()).
    constexpr int score() const
    {
        return move.score();
    }
    /// @brief Depth to which this position was searched.
    constexpr int depth() const
    {
        return move.TTDepth();
    }
    /// @brief Node type of the stored result.
    constexpr NodeType node_type() const
    {
        return move.TTNodeType();
    }
    /// @brief Table generation when stored; stale when != the table's current generation.
    constexpr uint8_t age() const
    {
        return move.TTAge();
    }
};

static_assert(sizeof(Transposition) == 16);

/// @brief Class to hold the transposition table.
class TranspositionTable
{
  public:
    TranspositionTable(std::size_t megabytes);
    std::optional<Transposition> FindTransposition(zobrist_t hash) const;
    void                         RecordTransposition(const Transposition &transposition);
    void                         Age();
    std::pair<std::size_t, int>  UsageStats() const;

  private:
    std::vector<Transposition>                  table_;          ///< Indexed using Zobrist hash.
    mutable std::unique_ptr<std::atomic_flag[]> locks_;          ///< Per-entry spinlocks, parallel to table_.
    uint8_t                                     generation_ = 0; ///< Current generation; bumped by Age().
};